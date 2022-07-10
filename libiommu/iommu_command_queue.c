// Copyright (c) 2022 by Rivos Inc.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
// Author: ved@rivosinc.com
#include "iommu.h"
uint8_t g_command_queue_stall_for_itag = 0;
uint8_t g_ats_inv_req_timeout = 0;
uint8_t g_iofence_wait_pending_inv = 0;
uint8_t g_iofence_pending_PR, g_iofence_pending_PW, g_iofence_pending_AV, g_iofence_pending_WIS_BIT; 
uint64_t g_iofence_pending_ADDR; 
uint32_t g_iofence_pending_DATA;

uint8_t g_pending_inval_req_DSV; 
uint8_t g_pending_inval_req_DSEG; 
uint16_t g_pending_inval_req_RID;
uint8_t g_pending_inval_req_PV;
uint32_t g_pending_inval_req_PID;
uint64_t g_pending_inval_req_PAYLOAD;

void
process_commands(
    void) {
    uint8_t status, opcode, func3, GV, AV, PSCV, DV, DSV, PV, DSEG, PR, PW, WIS_BIT, itag;
    uint16_t RID;
    uint32_t GSCID, PSCID, PID, DID, DATA;
    uint64_t a, ADDR, PAYLOAD, reserved;
    command_t command;

    // Command queue is used by software to queue commands to be processed by 
    // the IOMMU. Each command is 16 bytes.
    // The PPN of the base of this in-memory queue and the size of the queue 
    // is configured into a memorymapped register called command-queue base (cqb).
    // The tail of the command-queue resides in a software controlled read/write
    // memory-mapped register called command-queue tail (cqt). The cqt is an index
    // into the next command queue entry that software will write. Subsequent to 
    // writing the command(s), software advances the cqt by the count of the number
    // of commands written. The head of the command-queue resides in a read-only 
    // memory-mapped IOMMU controlled register called command-queue head (cqh). The
    // cqh is an index into the command queue that IOMMU should process next. 
    // 

    // If command-queue access leads to a memory fault then the 
    // command-queue-memory-fault cqmf bit is set to 1 
    // If the execution of a command leads to a timeout (e.g. a command to invalidate
    // device ATC may timeout waiting for a completion), then the command-queue 
    // sets the cmd_to bit.
    // If an illegal or unsupported command is fetched and decoded by the 
    // command-queue then the command-queue sets the cmd_ill bit
    // If any of these bits are set then CQ stops processing from the 
    // command-queue. 
    // The command-queue is active if cqon is 1.
    // Sometimes the command queue may stall due to unavailability of internal
    // resources - e.g. ITAG trackers
    if ( (g_reg_file.cqcsr.cqon == 0) ||
         (g_reg_file.cqcsr.cqmf != 0) ||
         (g_reg_file.cqcsr.cmd_ill != 0) ||
         (g_reg_file.cqcsr.cmd_to != 0) ||
         (g_command_queue_stall_for_itag != 0) ||
         (g_iofence_wait_pending_inv != 0) )
        return;

    // If cqh == cqt, the command-queue is empty. 
    // If cqt == (cqh - 1) the command-queue is full.
    if ( g_reg_file.cqh.index == g_reg_file.cqt.index )
        return;

    a = g_reg_file.cqb.ppn * PAGESIZE | (g_reg_file.cqh.index * 16);
    status = read_memory(a, 16, (char *)&command);
    if ( status != 0 ) {
        // If command-queue access leads to a memory fault then the
        // command-queue-memory-fault bit is set to 1 and the command
        // queue stalls until this bit is cleared. When cqmf is set to 1, an
        // interrupt is generated if an interrupt is not already pending (i.e.,
        // ipsr.cip == 1) and not masked (i.e. cqsr.cie == 0). To reenable 
        // command processing, software should clear this bit by writing 1
        if ( g_reg_file.cqcsr.cqmf == 0 ) {
            g_reg_file.cqcsr.cqmf = 1;
            generate_interrupt(COMMAND_QUEUE);
        }

        return;
    }

    // IOMMU commands are grouped into a major command group determined by the 
    // opcode and within each group the func3 field specifies the function invoked 
    // by that command. The opcode defines the format of the operand fields. One 
    // or more of those fields may be used by the specific function invoked.
    opcode = get_bits(6, 0, command.low);
    func3  = get_bits(9, 7, command.low);

    switch ( opcode ) {
        case IOTINVAL:
            PSCV     = get_bits(10, 10, command.low);
            AV       = get_bits(11, 11, command.low);
            GV       = get_bits(12, 12, command.low);
            PSCID    = get_bits(35, 16, command.low);
            GSCID    = get_bits(55, 40, command.low);
            ADDR     = get_bits(52,  0, command.high);
            reserved = get_bits(15, 13, command.low);
            reserved|= get_bits(39, 36, command.low);
            reserved|= get_bits(63, 56, command.low);
            reserved|= get_bits(63, 52, command.high);
            if ( reserved ) 
                goto command_illegal;
            switch ( func3 ) {
                case VMA:
                    do_iotinval_vma(GV, AV, PSCV, GSCID, PSCID, ADDR);
                    break;
                case GVMA:
                    // Setting PSCV to 1 with IOTINVAL.GVMA is illegal.
                    if ( PSCV ) 
                        goto command_illegal;
                    do_iotinval_gvma(GV, AV, GSCID, ADDR);
                    break;
                default: goto command_illegal;
            }
            break;
        case IODIR:
            DV       = get_bits(10, 10, command.low);
            PID      = get_bits(35, 16, command.low);
            DID      = get_bits(55, 40, command.low);
            reserved = get_bits(15, 11, command.low);
            reserved|= get_bits(39, 36, command.low);
            reserved|= get_bits(63, 56, command.low);
            reserved|= command.high;
            if ( reserved ) 
                goto command_illegal;
            switch ( func3 ) {
                case INVAL_DDT:
                    if ( PID != 0 ) goto command_illegal;
                    do_inval_ddt(DV, DID);
                    break;
                case INVAL_PDT:
                    if ( DV != 1 ) goto command_illegal;
                    do_inval_pdt(DID, PID);
                    break;
                default: goto command_illegal;
            }
            break;
        case IOFENCE:
            PR       = get_bits(10, 10, command.low);
            PW       = get_bits(11, 11, command.low);
            AV       = get_bits(12, 12, command.low);
            WIS_BIT  = get_bits(13, 13, command.low);
            DATA     = get_bits(63, 32, command.low);
            ADDR     = command.high;
            reserved = get_bits(31, 14, command.low);
            reserved|= get_bits(1,   0, command.high);
            if ( reserved ) goto command_illegal;
            // The wired-interrupt-signaling (WIS) bit when set to 1 
            // causes a wired-interrupt from the command
            // queue to be generated on completion of IOFENCE.C. This
            // bit is reserved if the IOMMU supports MSI.
            if ( g_reg_file.fctrl.wis == 0 && WIS_BIT == 1) 
                goto command_illegal;
            switch ( func3 ) {
                case IOFENCE_C:
                    do_iofence_c(PR, PW, AV, WIS_BIT, ADDR, DATA);
                    // If IOFENCE is waiting for invalidation requests
                    // to complete then do not advance the CQ head
                    if ( g_iofence_wait_pending_inv != 0 ) {
                        return;
                    }
                    break;
                default: goto command_illegal;
            }
            break;
        case ATS:
            func3    = get_bits(9,   7, command.low);
            DSV      = get_bits(10, 10, command.low);
            PV       = get_bits(11, 11, command.low);
            PID      = get_bits(35, 16, command.low);
            DSEG     = get_bits(47, 40, command.low);
            RID      = get_bits(63, 48, command.low);
            PAYLOAD  = command.high;
            reserved = get_bits(39, 36, command.low);
            reserved|= get_bits(15, 12, command.low);
            if ( reserved ) goto command_illegal;
            switch ( func3 ) {
                case INVAL:
                    // Allocate a ITAG for the request
                    if ( allocate_itag(DSV, DSEG, RID, &itag) ) { 
                        // No ITAG available, This command stays pending
                        // but since the reference implementation only
                        // has one deep pending command buffer the CQ
                        // is now stall till a completion or a timeout 
                        // frees up pending ITAGs.
                        g_pending_inval_req_DSV = DSV;
                        g_pending_inval_req_DSEG = DSEG;
                        g_pending_inval_req_RID = RID;
                        g_pending_inval_req_PV = PV;
                        g_pending_inval_req_PID = PID;
                        g_pending_inval_req_PAYLOAD = PAYLOAD;
                        g_command_queue_stall_for_itag = 1;
                    } else {
                        // ITAG allocated successfully, send invalidate request
                        do_ats_msg(INVAL_REQ_MSG_CODE, itag, DSV, DSEG, RID, PV, PID, PAYLOAD);
                    }
                    break;
                case PRGR:
                    do_ats_msg(PRGR_MSG_CODE, 0, DSV, DSEG, RID, PV, PID, PAYLOAD);
                    break;
                default: goto command_illegal;
            }
        default: goto command_illegal;
    }
    // The head of the command-queue resides in a read-only memory-mapped IOMMU
    // controlled register called command-queue head (`cqh`). The `cqh` is an index
    // into the command queue that IOMMU should process next. Subsequent to reading
    // each command the IOMMU may advance the `cqh` by 1.
    g_reg_file.cqh.index =  
        (g_reg_file.cqh.index + 1) & ((1UL << (g_reg_file.cqb.log2szm1 + 1)) - 1);
    return;

command_illegal:
    // If an illegal or unsupported command is fetched and decoded by
    // the command-queue then the command-queue sets the cmd_ill
    // bit and stops processing from the command-queue. When cmd_ill
    // is set to 1, an interrupt is generated if not already pending (i.e.
    // ipsr.cip == 1) and not masked (i.e. cqsr.cie == 0). To reenable 
    // command processing software should clear this bit by writing 1
    if ( g_reg_file.cqcsr.cmd_ill == 0 ) {
        g_reg_file.cqcsr.cmd_ill = 1;
        generate_interrupt(COMMAND_QUEUE);
    }
    return;
}
void
do_inval_ddt(
    uint8_t DV, uint32_t DID) {
    uint8_t i;
    // IOMMU operations cause implicit reads to DDT and/or PDT. 
    // To reduce latency of such reads, the IOMMU may cache entries from 
    // the DDT and/or PDT in IOMMU directory caches. These caches may not 
    // observe modifications performed by software to these data structures
    // in memory.
    // The IOMMU DDT cache invalidation command, `IODIR.INVAL_DDT` 
    // synchronize updates to DDT with the operation of the IOMMU and 
    // flushes the matching cached entries.
    // The `DV` operand indicates if the device ID (`DID`) operand is valid.
    // `IODIR.INVAL_DDT` guarantees that any previous stores made by a RISC-V hart to
    // the DDT are observed before all subsequent implicit reads from IOMMU to DDT.
    // If `DV` is 0, then the command invalidates all  DDT and PDT entries cached for
    // all devices. If `DV` is 1, then the command invalidates cached leaf level DDT
    // entry for the device identified by `DID` operand and all associated PDT entries.
    // The `PID` operand is reserved for `IODIR.INVAL_DDT`.
    for ( i = 0; i < DDT_CACHE_SIZE; i++ ) {
        if ( DV == 0 ) ddt_cache[i].valid = 0;
        if ( DV == 1 && (ddt_cache[i].DID == DID) ) 
            ddt_cache[i].valid = 0;
    }
    return;
}
void
do_inval_pdt(
    uint32_t DID, uint32_t PID) {
    int i;

    // IOMMU operations cause implicit reads to DDT and/or PDT. 
    // To reduce latency of such reads, the IOMMU may cache entries from 
    // the DDT and/or PDT in IOMMU directory caches. These caches may not 
    // observe modifications performed by software to these data structures
    // in memory.
    // The IOMMU PDT cache invalidation command, `IODIR.INVAL_PDT` synchronize
    // updates to PDT with the operation of the IOMMU and flushes the matching
    // cached entries.
    // The `DV` operand must be 1 for `IODIR.INVAL_PDT`.
    // `IODIR.INVAL_PDT` guarantees that any previous stores made by a RISC-V hart to
    // the PDT are observed before all subsequent implicit reads from IOMMU to PDT.
    // The command invalidates cached leaf PDT entry for the specified `PID` and `DID`.

    for ( i = 0; i < PDT_CACHE_SIZE; i++ )
        if ( pdt_cache[i].DID == DID && pdt_cache[i].PID == PID ) 
            pdt_cache[i].valid = 0;
    return;
}

void 
do_iotinval_vma(
    uint8_t GV, uint8_t AV, uint8_t PSCV, uint32_t GSCID, uint32_t PSCID, uint64_t ADDR) {

    // IOMMU operations cause implicit reads to PDT, first-stage and second-stage 
    // page tables. To reduce latency of such reads, the IOMMU may cache entries 
    // from the first and/or second-stage page tables in the 
    // IOMMU-address-translation-cache (IOATC). These caches may not observe 
    // modifications performed by software to these data structures in memory.
    // The IOMMU translation-table cache invalidation commands, IOTINVAL.VMA 
    // and IOTINVAL.GVMA synchronize updates to in-memory S/VS-stage and G-stage 
    // page table data structures with the operation of the IOMMU and invalidate
    // the matching IOATC entries.
    // The GV operand indicates if the Guest-Soft-Context ID (GSCID) operand is 
    // valid. The PSCV operand indicates if the Process Soft-Context ID (PSCID)
    // operand is valid. Setting PSCV to 1 is allowed only for IOTINVAL.VMA. The
    // AV operand indicates if the address (ADDR) operand is valid. When GV is 0,
    // the translations associated with the host (i.e. those where the 
    // second-stage translation is not active) are operated on.
    // IOTINVAL.VMA ensures that previous stores made to the first-stage page 
    // tables by the harts are observed by the IOMMU before all subsequent 
    // implicit reads from IOMMU to the corresponding firststage page tables.
    // 
    // .`IOTINVAL.VMA` operands and operations
    // |`GV`|`AV`|`PSCV`| Operation
    // |0   |0   |0     | Invalidates all address-translation cache entries, including
    //                    those that contain global mappings, for all host address
    //                    spaces.
    // |0   |0   |1     | Invalidates all address-translation cache entries for the
    //                    host address space identified by `PSCID` operand, except for
    //                    entries containing global mappings.
    // |0   |1   |0     | Invalidates all address-translation cache entries that
    //                    contain leaf page table entries, including those that contain 
    //                    global mappings, corresponding to the IOVA in `ADDR` operand, 
    //                    for all host address spaces.
    // |0   |1   |1     | Invalidates all address-translation cache entries that
    //                    contain leaf page table entries corresponding to the IOVA in
    //                    `ADDR` operand and that match the host address space
    //                    identified by `PSCID` operand, except for entries containing
    //                    global mappings.
    // |1   |0   |0     | Invalidates all address-translation cache entries, including
    //                    those that contain global mappings, for all VM address spaces
    //                    associated with `GSCID` operand.
    // |1   |0   |1     | Invalidates all address-translation cache entries for the
    //                    for the VM address space identified by `PSCID` and `GSCID`
    //                    operands, except for entries containing global mappings.
    // |1   |1   |0     | Invalidates all address-translation cache entries that
    //                    contain leaf page table entries, including those that contain 
    //                    global mappings, corresponding to the IOVA in `ADDR` operand, 
    //                    for all VM address spaces associated with the `GSCID` operand.
    // |1   |1   |1     | Invalidates all address-translation cache entries that
    //                    contain leaf page table entries corresponding to the IOVA in
    //                    `ADDR` operand, for the VM address space identified by `PSCID`
    //                    and `GSCID` operands, except for entries containing global
    //                    mappings.

    uint8_t i, gscid_match, pscid_match, addr_match, global_match;

    for ( i = 0; i < TLB_SIZE; i++ ) {
        gscid_match = pscid_match = addr_match = global_match = 0;
        if ( (GV == 0 && tlb[i].GV == 0 ) ||
             (GV == 1 && tlb[i].GV == 1 && tlb[i].GSCID == GSCID) )
            gscid_match = 1;
        if ( (PSCV == 0 && tlb[i].PSCV == 0) ||
             (PSCV == 1 && tlb[i].PSCV == 1 && tlb[i].PSCID == PSCID) )
            pscid_match = 1;
        if ( (AV == 0) ||
             (AV == 1 && match_address_range(ADDR, tlb[i].PPN, tlb[i].S)) )
            addr_match = 1;
        if ( (PSCV == 0) || 
             (PSCV == 1 && tlb[i].G == 0) )
            global_match = 1;
        if ( gscid_match && pscid_match && addr_match && global_match )
            tlb[i].valid = 0;
    }
    return;
}
void
do_iotinval_gvma(
    uint8_t GV, uint8_t AV, uint32_t GSCID, uint64_t ADDR) {

    uint8_t i, gscid_match, addr_match;
    // Conceptually, an implementation might contain two address-translation
    // caches: one that maps guest virtual addresses to guest physical addresses, 
    // and another that maps guest physical addresses to supervisor physical 
    // addresses. IOTINVAL.GVMA need not flush the former cache, but it must 
    // flush entries from the latter cache that match the IOTINVAL.GVMA’s 
    // address and GSCID arguments.
    // More commonly, implementations contain address-translation caches 
    // that map guest virtual addresses directly to supervisor physical 
    // addresses, removing a level of indirection. For such implementations, 
    // any entry whose guest virtual address maps to a guest physical address that
    // matches the IOTINVAL.GVMA’s address and GSCID arguments must be flushed. 
    // Selectively flushing entries in this fashion requires tagging them with 
    // the guest physical address, which is costly, and so a common technique 
    // is to flush all entries that match the IOTINVAL.GVMA’s GSCID argument, 
    // regardless of the address argument.
    // IOTINVAL.GVMA ensures that previous stores made to the G-stage page 
    // tables are observed before all subsequent implicit reads from IOMMU 
    // to the corresponding G-stage page tables. Setting PSCV to 1 with 
    // IOTINVAL.GVMA is illegal.
    // .`IOTINVAL.GVMA` operands and operations
    // | `GV` | `AV`   | Operation
    // | 0    | n/a    | Invalidates information cached from any level of the
    //                   G-stage page table, for all VM address spaces.
    // | 1    | 0      | Invalidates information cached from any level of the
    //                   G-stage page tables, but only for VM address spaces
    //                   identified by the `GSCID` operand.
    // | 1    | 1      | Invalidates information cached from leaf G-stage page
    //                   table entries corresponding to the guest-physical-address in
    //                   `ADDR` operand, for only for VM address spaces identified
    //                   `GSCID` operand.
    for ( i = 0; i < TLB_SIZE; i++ ) {
        if ( (GV == 0 && tlb[i].GV == 1) ||
             (GV == 1 && tlb[i].GV == 1 && tlb[i].GSCID == GSCID) )
            gscid_match = 1;
        // If the cache holds a VA -> SPA translation i.e. PSCV == 1 then invalidate
        // it. If PSCV is 0 then it holds a GPA. If AV is 0 then all entries are 
        // eligible else match the address
        if ( (tlb[i].PSCV == 1) || (AV == 0) ||
             (tlb[i].PSCV == 0 && AV == 1 && match_address_range(ADDR, tlb[i].PPN, tlb[i].S)) ) 
            addr_match = 1;
        if ( gscid_match && addr_match )
            tlb[i].valid = 0;
    }
    return;
}
void
do_ats_msg(
    uint8_t MSGCODE, uint8_t TAG, uint8_t DSV, uint8_t DSEG, uint16_t RID, 
    uint8_t PV, uint32_t PID, uint64_t PAYLOAD) {
    ats_msg_t msg;
    // The ATS.INVAL command instructs the IOMMU to send a “Invalidation Request” message 
    // to the PCIe device function identified by RID. An “Invalidation Request” message 
    // is used to clear a specific subset of the address range from the address translation 
    // cache in a device function. The ATS.INVAL command completes when an “Invalidation 
    // Completion” response message is received from the device or a protocol defined 
    // timeout occurs while waiting for a response. The IOMMU may advance the cqh and fetch 
    // more commands from CQ while a response is awaited.
    // The ATS.PRGR command instructs the IOMMU to send a “Page Request Group Response” 
    // message to the PCIe device function identified by the RID. The “Page Request Group 
    // Response” message is used by system hardware and/or software to communicate with the 
    // device functions page-request interface to signal completion of a “Page Request”, or
    // the catastrophic failure of the interface.If the PV operand is set to 1, the message
    // is generated with a PASID with the PASID field set to the PID operand. The PAYLOAD 
    // operand of the command is used to form the message body.
    // If the DSV operand is 1, then a valid destination segment number is specified by 
    // the DSEG operand.
    msg.MSGCODE = MSGCODE;
    msg.TAG     = TAG;
    msg.RID     = RID;
    msg.DSV     = DSV;
    msg.DSEG    = DSEG;
    msg.PV      = PV;
    msg.PID     = PID;
    msg.PAYLOAD = PAYLOAD;
    send_msg_iommu_to_hb(&msg);
    return;
}
void
do_iofence_c(
    uint8_t PR, uint8_t PW, uint8_t AV, uint8_t WIS_BIT, uint64_t ADDR, uint32_t DATA) {

    uint8_t status;
    // The IOMMU fetches commands from the CQ in order but the IOMMU may execute the fetched
    // commands out of order. The IOMMU advancing cqh is not a guarantee that the commands 
    // fetched by the IOMMU have been executed or committed. A IOFENCE.C command guarantees 
    // that all previous commands fetched from the CQ have been completed and committed.
    g_iofence_wait_pending_inv = 1;
    if ( any_ats_invalidation_requests_pending() ) {
        // if all previous ATS invalidation requests
        // have not completed then IOFENCE waits for
        // them to complete - or timeout
        g_iofence_pending_PR = PR;
        g_iofence_pending_PW = PW;
        g_iofence_pending_AV = AV;
        g_iofence_pending_WIS_BIT = WIS_BIT; 
        g_iofence_pending_ADDR = ADDR; 
        g_iofence_pending_DATA = DATA;
        return;
    }
    // All previous pending invalidation requests completed or timed out
    g_iofence_wait_pending_inv = 0;
    // If any ATC invalidation requests timed out then set command timeout
    if ( g_ats_inv_req_timeout == 1 ) {
        if ( g_reg_file.cqcsr.cmd_to == 0 ) {
            g_reg_file.cqcsr.cmd_to = 1;
            generate_interrupt(COMMAND_QUEUE);
        }
        g_ats_inv_req_timeout = 0;
    }
    // The commands may be used to order memory accesses from I/O devices connected to the IOMMU
    // as viewed by the IOMMU, other RISC-V harts, and external devices or co-processors. The 
    // PR and PW bits can be used to request that the IOMMU ensure that all previous requests 
    // from devices that have already been processed by the IOMMU be committed to a global 
    // ordering point such that they can be observed by all RISC-V harts and IOMMUs in the machine.
    if ( PR == 1 || PW == 1 )
        iommu_to_hb_do_global_observability_sync(PR, PW);

    // The wired-interrupt-signaling (WIS) bit when set to 1 causes a wired-interrupt from the command
    // queue to be generated on completion of IOFENCE.C. This bit is reserved if the IOMMU supports MSI
    if ( g_reg_file.cqcsr.fence_w_ip == 0 && WIS_BIT == 1 ) {
        g_reg_file.cqcsr.fence_w_ip = 1;
        generate_interrupt(COMMAND_QUEUE);
    }
    // The AV command operand indicates if ADDR[63:2] operand and DATA operands are valid. 
    // If AV=1, the IOMMU writes DATA to memory at a 4-byte aligned address ADDR[63:2] * 4 as 
    // a 4-byte store.
    if ( AV == 1 ) {
        status = write_memory((char *)&DATA, ADDR, 4);
        if ( status != 0 ) {
            if ( g_reg_file.cqcsr.cqmf == 0 ) {
                g_reg_file.cqcsr.cqmf = 1;
                generate_interrupt(COMMAND_QUEUE);
            }
        }
    }
    return;
}
// Retry a pending IOFENCE if all invalidations received
void
do_pending_iofence() {
    if ( g_iofence_wait_pending_inv == 1 ) {
        do_iofence_c(g_iofence_pending_PR, g_iofence_pending_PW, g_iofence_pending_AV, 
                     g_iofence_pending_WIS_BIT, g_iofence_pending_ADDR, g_iofence_pending_DATA);
    }
    // If not still pending then advance the CQH
    if ( g_iofence_wait_pending_inv == 1 ) {
        g_reg_file.cqh.index =  
            (g_reg_file.cqh.index + 1) & ((1UL << (g_reg_file.cqb.log2szm1 + 1)) - 1);
    }
    return;
}
void 
queue_any_blocked_ats_inval_req() {
    uint8_t itag;
    if ( g_command_queue_stall_for_itag == 0 ) {
        // Allocate a ITAG for the request
        if ( allocate_itag(g_pending_inval_req_DSV, g_pending_inval_req_DSEG, 
                           g_pending_inval_req_RID, &itag) )
            return;
        // ITAG allocated successfully, send invalidate request
        do_ats_msg(INVAL_REQ_MSG_CODE, itag, g_pending_inval_req_DSV, 
                   g_pending_inval_req_DSEG, g_pending_inval_req_RID, 
                   g_pending_inval_req_PV, g_pending_inval_req_PID, 
                   g_pending_inval_req_PAYLOAD);
        // Remove the command queue stall
        g_command_queue_stall_for_itag = 0;
    }
    return;
}

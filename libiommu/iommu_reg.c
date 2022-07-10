// Copyright (c) 2022 by Rivos Inc.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
// Author: ved@rivosinc.com

#include "iommu.h"

// IOMMU register file
iommu_regs_t g_reg_file;
// Register offset to size mapping
uint8_t g_offset_to_size[4096];
// Global parameters of the design
uint8_t g_num_hpm;
uint8_t g_hpmctr_bits;
uint8_t g_eventID_mask;
uint8_t g_num_vec_bits;

uint8_t 
is_access_valid(
    uint16_t offset, uint8_t num_bytes) {
    // The IOMMU behavior for register accesses where the 
    // address is not aligned to the size of the access or
    // if the access spans multiple registers is undefined
    if ( (num_bytes != 4 && num_bytes != 8) ||       // only 4B & 8B registers in IOMMU
         (offset >= 4096) ||                         // Offset must be <= 4095
         ((offset & (num_bytes - 1)) != 0) ||        // Offset must be aligned to size
         (g_offset_to_size[offset] < num_bytes) ) { // Acesss cannot span two registers
        return 0;
    }
    return 1;
}
uint32_t 
get_iocountovf() {
    iocountovf_t iocountovf_temp;
    uint8_t i;
    iocountovf_temp.raw = 0;
    iocountovf_temp.cy = g_reg_file.iohpmcycles.of;
    for ( i = 0; i < (g_num_hpm - 1); i++ )
        iocountovf_temp.hpm |= (g_reg_file.iohpmevt[i].of << i);
    return iocountovf_temp.raw;
}

uint64_t 
read_register(
    uint16_t offset, uint8_t num_bytes) {

    // If access is not valid then return -1
    if ( !is_access_valid(offset, num_bytes) )
        return 0xFFFFFFFFFFFFFFFF;

    // Counter overflows are to be gathered from all counters
    if ( offset == IOCNTOVF_OFFSET )
        return get_iocountovf();

    // If access is valid then return data from the register file
    return ( num_bytes == 4 ) ? g_reg_file.regs4[offset/4] :
                                g_reg_file.regs8[offset/8];
}
void 
write_register(
    uint16_t offset, uint8_t num_bytes, uint64_t data) {

    uint32_t data4 = data & 0xFFFFFFFF;
    uint64_t data8 = data;
    uint16_t ctr_num, x;
    cqcsr_t cqcsr_temp;
    fqcsr_t fqcsr_temp;
    pqcsr_t pqcsr_temp;
    ipsr_t  ipsr_temp;
    icvec_t icvec_temp;
    fctrl_t fctrl_temp;
    ddtp_t  ddtp_temp;
    cqb_t   cqb_temp;
    cqt_t   cqt_temp;
    fqb_t   fqb_temp;
    fqh_t   fqh_temp;
    pqb_t   pqb_temp;
    pqh_t   pqh_temp;
    iohpmcycles_t iohpmcycles_temp;
    iohpmevt_t iohpmevt_temp;
    msi_addr_t msi_addr_temp;
    msi_vec_ctrl_t msi_vec_ctrl_temp;
    uint64_t pa_mask  = ((1UL << (g_reg_file.capabilities.pas)) - 1);
    uint64_t ppn_mask = pa_mask >> 12;

    // If access is not valid then discard the write
    if ( !is_access_valid(offset, num_bytes) ) {
        return;
    }

    // If its a 4B write to a 8B register then merge the new 
    // write data with current data in register file
    if ( (g_offset_to_size[offset] == 8) && (num_bytes == 4) ) {
        // read the old 8B  
        data8 = g_reg_file.regs8[(offset & ~0x7)/8];
        if ( (offset & 0x7) != 0 ) {
            // write to high half - replace high half
            data8 = ((data8) & 0x00000000FFFFFFFF) | ((uint64_t)data4 << 32);
        } else {
            // write to low half - replace low half
            data8 = ((data8) & 0xFFFFFFFF00000000) | (data4);
        }
        // Align offset to 8B boundary
        offset = offset / 8;
    }

    fctrl_temp.raw = data4;
    ddtp_temp.raw = data8;
    cqcsr_temp.raw = data4;
    pqcsr_temp.raw = data4;
    fqcsr_temp.raw = data4;
    cqb_temp.raw = data8;
    cqt_temp.raw = data4;
    fqb_temp.raw = data8;
    fqh_temp.raw = data4;
    pqb_temp.raw = data8;
    pqh_temp.raw = data4;
    cqcsr_temp.raw = data4;
    fqcsr_temp.raw = data4;
    pqcsr_temp.raw = data4;
    ipsr_temp.raw = data4;
    iohpmcycles_temp.raw = data8;
    iohpmevt_temp.raw = data8;
    icvec_temp.raw = data4;
    msi_addr_temp.raw = data8;
    msi_vec_ctrl_temp.raw = data4;
     
    switch (offset) {
        case CAPABILITIES_OFFSET:
            // This register is read only
            return;
        case FCTRL_OFFSET:
            // This register must be readable in any 
            // implementation. An implementation may allow one or more
            // fields in the register to be writable to support enabling 
            // or disabling the feature controlled by that field.
            // If software enables or disables a feature when 
            // the IOMMU is not OFF (i.e. ddtp.iommu_mode == Off)
            // then the IOMMU behavior is UNSPECIFIED.
            // If software enables or disables a feature when the 
            // IOMMU in-memory queues are enabled (i.e.
            // cqcsr.cqon/cqen == 1, fqcsr.fqon/cqen == 1, or 
            // pqcsr.pqon/pqen == 1) then the IOMMU
            // behavior is UNSPECIFIED.
            // FCTRL is writeable if IOMMU is bi-endian
            // or supports both wired and MSI interrupts
            // retain default values for the field if not
            // writeable
            if ( (g_reg_file.capabilities.end != BOTH_END) &&
                 (g_reg_file.capabilities.igs != IGS_BOTH) ) {
                // Register is not writeable
                break;
            }
            // Register is writeable
            if ( (g_reg_file.ddtp.iommu_mode != Off) ||
                 (g_reg_file.cqcsr.cqen == 1) ||
                 (g_reg_file.cqcsr.cqon == 1) ||
                 (g_reg_file.fqcsr.fqen == 1) ||
                 (g_reg_file.fqcsr.fqon == 1) ||
                 (g_reg_file.pqcsr.pqen == 1) ||
                 (g_reg_file.pqcsr.pqon == 1) ) {
                // The UNSPECIFIED behavior in reference model
                // is to drop the write
                break;
            }
            if ( (g_reg_file.capabilities.end == BOTH_END) )
                g_reg_file.fctrl.end = fctrl_temp.end;
            if ( (g_reg_file.capabilities.igs == IGS_BOTH) )
                g_reg_file.fctrl.wis = fctrl_temp.wis;
            break;
        case DDTP_OFFSET:
            // If DDTP is busy the discard the write
            // A write to ddtp may require the IOMMU to perform
            // many operations that may not occur synchronously to 
            // the write. When a write is observed by the ddtp, the
            // busy bit is set to 1. When the busy bit is 1, behavior of
            // additional writes to the ddtp is implementation
            // defined. Some implementations may ignore the second
            // write and others may perform the actions determined
            // by the second write. Software must verify that the busy
            // bit is 0 before writing to the ddtp.
            // If the busy bit reads 0 then the IOMMU has completed
            // the operations associated with the previous write to
            // ddtp.
            // An IOMMU that can complete these operations
            // synchronously may hard-wire this bit to 0
            if ( g_reg_file.ddtp.busy )
                return;
            // If a illegal value written to ddtp.iommu_mode then 
            // retain the current legal value
            if ( (ddtp_temp.iommu_mode == Off) ||
                 (ddtp_temp.iommu_mode == DDT_Bare) ||
                 (ddtp_temp.iommu_mode == DDT_1LVL) ||
                 (ddtp_temp.iommu_mode == DDT_2LVL) ||
                 (ddtp_temp.iommu_mode == DDT_3LVL) )
                g_reg_file.ddtp.iommu_mode = ddtp_temp.iommu_mode;
            printf("DDTP mode = %d-%d\n", ddtp_temp.iommu_mode, DDT_3LVL);
            g_reg_file.ddtp.ppn = ddtp_temp.ppn & ppn_mask;
            break;
        case CQB_OFFSET:
            // The command-queue is active if cqon is 1. IOMMU behavior on
            // changing cqb when busy is 1 or cqon is 1 is implementation
            // defined. The software recommended sequence to change cqb is to
            // first disable the command-queue by clearing cqen and waiting for
            // both busy and cqon to be 0 before changing the cqb.
            // The reference model discards the write
            if ( g_reg_file.cqcsr.busy || g_reg_file.cqcsr.cqon )
                return;
            g_reg_file.cqb.ppn = cqb_temp.ppn & ppn_mask;
            break;
        case CQH_OFFSET:
            // This register is read only
            break;
        case CQT_OFFSET:
            // The command-queue is active if cqon is 1. IOMMU behavior on
            // changing cqb when busy is 1 or cqon is 1 is implementation
            // defined. The software recommended sequence to change cqb is to
            // first disable the command-queue by clearing cqen and waiting for
            // both busy and cqon to be 0 before changing the cqb.
            // The reference model discards the write
            if ( g_reg_file.cqcsr.busy || g_reg_file.cqcsr.cqon )
                return;
            g_reg_file.cqt.index = cqt_temp.index & 
                ((1UL << (g_reg_file.cqb.log2szm1 + 1)) - 1);
            break;
        case FQB_OFFSET:
            // The fault-queue is active if `fqon` reads 1.
            // IOMMU behavior on changing `fqb` when `busy` is 1
            // or `fqon` is 1 implementation defined. The
            // recommended sequence to change `fqb` is to first
            // disable the fault-queue by clearing `fqen` and
            // waiting for both `busy` and `fqon` to be 0 before
            // changing `fqb`.
            // The reference model discards the write
            if ( g_reg_file.fqcsr.busy || g_reg_file.fqcsr.fqon )
                return;
            g_reg_file.fqb.ppn = fqb_temp.ppn & ppn_mask;
            break;
        case FQH_OFFSET:
            // The fault-queue is active if `fqon` reads 1.
            // IOMMU behavior on changing `fqb` when `busy` is 1
            // or `fqon` is 1 implementation defined. The
            // recommended sequence to change `fqb` is to first
            // disable the fault-queue by clearing `fqen` and
            // waiting for both `busy` and `fqon` to be 0 before
            // changing `fqb`.
            // The reference model discards the write
            if ( g_reg_file.fqcsr.busy || g_reg_file.fqcsr.fqon )
                return;
            g_reg_file.fqh.index = fqh_temp.index & 
                ((1UL << (g_reg_file.fqb.log2szm1 + 1)) - 1);
            break;
        case FQT_OFFSET:
            // This register is read only
            break;
        case PQB_OFFSET:
            // This register is read-only 0 if capabilities.ATS is 0.
            if ( g_reg_file.capabilities.ats == 0 )
                break;
            // The page-request is active when `pqon` reads 1.
            // IOMMU behavior on changing `pqb` when `busy` is 1
            // or `pqon` is 1 implementation defined. The
            // recommended sequence to change `pqb` is to first
            // disable the page-request queue by clearing `pqen`
            // and waiting for both `busy` and `pqon` to be 0
            // before changing `pqb`.
            // The reference model discards the write
            if ( g_reg_file.pqcsr.busy || g_reg_file.pqcsr.pqon )
                return;
            g_reg_file.pqb.ppn = pqb_temp.ppn & ppn_mask;
            break;
        case PQH_OFFSET:
            // This register is read-only 0 if capabilities.ATS is 0.
            if ( g_reg_file.capabilities.ats == 0 )
                break;
            // The page-request is active when `pqon` reads 1.
            // IOMMU behavior on changing `pqb` when `busy` is 1
            // or `pqon` is 1 implementation defined. The
            // recommended sequence to change `pqb` is to first
            // disable the page-request queue by clearing `pqen`
            // and waiting for both `busy` and `pqon` to be 0
            // before changing `pqb`.
            // The reference model discards the write
            if ( g_reg_file.pqcsr.busy || g_reg_file.pqcsr.pqon )
                break;
            g_reg_file.pqh.index = pqh_temp.index & 
                ((1UL << (g_reg_file.pqb.log2szm1 + 1)) - 1);
            break;
        case PQT_OFFSET:
            // This register is read only
            break;
        case CQCSR_OFFSET:
            // A write to `cqcsr` may require the IOMMU to perform
            // many operations that may not occur synchronously 
            // to the write. When a write is observed by the 
            // `cqcsr`, the `busy` bit is set to 1.

            // When the `busy` bit is 1, behavior of additional 
            // writes to the `cqcsr` is implementation defined. 
            // Some implementations may ignore the second write and
            // others may perform the actions determined by the 
            // second write.

            // Software must verify that the busy bit is 0 before 
            // writing to the `cqcsr`. An IOMMU that can complete 
            // controls synchronously may hard-wire this bit to 0.

            // An IOMMU that can complete these operations 
            // synchronously may hard-wire this bit to 0.
            // The reference model discards the write
            if ( g_reg_file.cqcsr.busy )
                return;
            // First set the busy bit
            g_reg_file.cqcsr.busy = 1;
            // The command-queue-enable bit enables the command-
            // queue when set to 1. Changing `cqen` from 0 to 1
            // sets the `cqh` and `cqt` to 0. The command-queue
            // may take some time to be active following setting
            // the `cqen` to 1. When the command queue is active,
            // the `cqon` bit reads 1.
            // When `cqen` is changed from 1 to 0, the command 
            // queue may stay active till the commands already 
            // fetched from the command-queue are being processed 
            // and/or there are outstanding implicit loads from 
            // the command-queue.  When the command-queue turns 
            // off, the `cqon` bit reads 0, `cqh` is set to 0, 
            // `cqt` is set to 0 and the `cqcsr` bits `cmd_ill`, 
            // `cmd_to`, `cqmf`, `fence_w_ip` are set to 0.
            // When the `cqon` bit reads 0, the IOMMU guarantees 
            // that no implicit memory accesses to the command 
            // queue are in-flight and the command-queue will not 
            // generate new implicit loads to the queue memory. 
            if ( g_reg_file.cqcsr.cqen != cqcsr_temp.cqen ) {
                // cqen going from 0->1 or 1->0
                if ( cqcsr_temp.cqen == 1 ) {
                    g_reg_file.cqh.index = 0;
                    g_reg_file.cqt.index = 0;
                    // mark queue as being on
                    g_reg_file.cqcsr.cqon = 1;
                }
                if ( cqcsr_temp.cqen == 0 ) {
                    g_reg_file.cqh.index = 0;
                    g_reg_file.cqt.index = 0;
                    g_reg_file.cqcsr.cmd_ill = 0;
                    g_reg_file.cqcsr.cmd_to = 0;
                    g_reg_file.cqcsr.cqmf = 0;
                    g_reg_file.cqcsr.fence_w_ip = 0;
                    // mark queue as being off
                    g_reg_file.cqcsr.cqon = 0;
                }
            }
            // Command-queue-interrupt-enable bit enables 
            // generation of interrupts from command-queue when 
            // set to 1.
            g_reg_file.cqcsr.cie = cqcsr_temp.cie;

            // Update the RW1C bits - clear if written to 1
            if ( cqcsr_temp.cqmf == 1 )       g_reg_file.cqcsr.cqmf = 0;
            if ( cqcsr_temp.cmd_to == 1 )     g_reg_file.cqcsr.cmd_to = 0;
            if ( cqcsr_temp.cmd_ill == 1 )    g_reg_file.cqcsr.cmd_ill = 0;
            if ( cqcsr_temp.fence_w_ip == 1 ) g_reg_file.cqcsr.fence_w_ip = 0;

            // Clear the busy bit
            g_reg_file.cqcsr.busy = 0;
            return;
        case FQCSR_OFFSET:
            // Write to `fqcsr` may require the IOMMU to perform 
            // many operations that may not occur synchronously to 
            // the write.
            // When a write is observed by the fqcsr, the `busy` 
            // bit is set to 1. When the `busy` bit is 1, behavior 
            // of additional writes to the `fqcsr` are 
            // implementation defined. Some implementations may 
            // ignore the second write and others may perform the 
            // actions determined by the second write.
            // Software should ensure that the `busy` bit is 0 
            // before writing to the `fqcsr`. 
            // An IOMMU that can complete controls synchronously 
            // may hard-wire this bit to 0. 
            if ( g_reg_file.fqcsr.busy ) {
                return;
            }
            // First set the busy bit
            g_reg_file.fqcsr.busy = 1;
            // The fault-queue enable bit enables the fault-queue
            // when set to 1.
            // Changing `fqen`  from 0 to 1, resets the `fqh` and
            // `fqt` to 0 and clears `fqcsr` bits `fqmf` and `fqof`.
            // The fault-queue may take some time to be active
            // following setting the `fqen` to 1. When the fault
            // queue is active, the `fqon` bit reads 1.

            // When `fqen` is changed from 1 to 0, the fault-queue
            // may stay active till in-flight fault-recording is
            // completed. When the fault-queue is off, the `fqon`
            // bit reads 0. The IOMMU guarantees that there are no
            // in-flight implicit writes to the fault-queue in
            // progress when `fqon` reads 0 and no new fault
            // records will be written to the fault-queue.
            if ( g_reg_file.fqcsr.fqen != fqcsr_temp.fqen ) {
                // fqen going from 0->1 or 1->0
                if ( fqcsr_temp.fqen == 1 ) {
                    g_reg_file.fqh.index = 0;
                    g_reg_file.fqt.index = 0;
                    // mark queue as being on
                    g_reg_file.fqcsr.fqon = 1;
                }
                if ( fqcsr_temp.fqen == 0 ) {
                    g_reg_file.fqh.index = 0;
                    g_reg_file.fqt.index = 0;
                    g_reg_file.fqcsr.fqof = 0;
                    g_reg_file.fqcsr.fqmf = 0;
                    // mark queue as being off
                    g_reg_file.fqcsr.fqon = 0;
                }
            }
            // Fault-queue-interrupt-enable bit enables 
            // generation of interrupts from command-queue when 
            // set to 1.
            g_reg_file.fqcsr.fie = fqcsr_temp.fie;
            // Update the RW1C bits - clear if written to 1
            if ( fqcsr_temp.fqmf == 1)
                g_reg_file.fqcsr.fqmf = 0;
            if ( fqcsr_temp.fqof == 1)
                g_reg_file.fqcsr.fqof = 0;
            // Clear the busy bit
            g_reg_file.fqcsr.busy = 0;
            break;
        case PQCSR_OFFSET:
            // Write to `pqcsr` may require the IOMMU to perform 
            // many operations that may not occur synchronously to 
            // the write.
            // When a write is observed by the `pqcsr`, the `busy` 
            // bit is set to 1. When the `busy` bit is 1, behavior 
            // of additional writes to the `fqcsr` are 
            // implementation defined. Some implementations may 
            // ignore the second write and others may perform the 
            // actions determined by the second write.
            // Software should ensure that the `busy` bit is 0 
            // before writing to the `fqcsr`. 
            // An IOMMU that can complete controls synchronously 
            // may hard-wire this bit to 0. 
            if ( g_reg_file.pqcsr.busy ) {
                return;
            }
            // First set the busy bit
            g_reg_file.pqcsr.busy = 1;
            // The page-request-enable bit enables the
            // page-request-queue when set to 1.
            // Changing `pqen` from 0 to 1, resets the `pqh`
            // and `pqt` to 0 and clears `pqcsr` bits `pqmf` and
            // `pqof` to 0. The page-request-queue may take
            // some time to be active following setting the
            // `pqen` to 1. When the page-request-queue is
            // active, the `pqon` bit reads 1.
            // When `pqen` is changed from 1 to 0, the
            // page-request-queue may stay active till in-flight
            // page-request writes are completed. When the
            // page-request-queue turns off, the `pqon` bit
            // reads 0, `pqh` is set to 0, `pqt` is set to 0 and
            // the `pqcsr` bits `pqof`, and `pqmf` are set to 0.
            // When `pqon` reads 0, the IOMMU guarantees that
            // there are no older in-flight implicit writes to
            // the queue memory and no further implicit writes
            // will be generated to the queue memory.
            // The IOMMU may respond to “Page Request” messages
            // received when page-request-queue is off or in
            // the process of being turned off, as having
            // encountered a catastrophic error as defined by
            // the PCIe ATS specifications
            if ( g_reg_file.pqcsr.pqen != pqcsr_temp.pqen ) {
                // fqen going from 0->1 or 1->0
                if ( pqcsr_temp.pqen == 1 ) {
                    g_reg_file.pqh.index = 0;
                    g_reg_file.pqt.index = 0;
                    // mark queue as being on
                    g_reg_file.pqcsr.pqon = 1;
                }
                if ( pqcsr_temp.pqen == 0 ) {
                    g_reg_file.pqh.index = 0;
                    g_reg_file.pqt.index = 0;
                    g_reg_file.pqcsr.pqof = 0;
                    g_reg_file.pqcsr.pqmf = 0;
                    // mark queue as being off
                    g_reg_file.pqcsr.pqon = 0;
                }
            }
            // page-request-queue-interrupt-enable bit enables 
            // generation of interrupts from page-request-queue when 
            // set to 1.
            g_reg_file.pqcsr.pie = pqcsr_temp.pie;
            // Update the RW1C bits - clear if written to 1
            if ( pqcsr_temp.pqmf == 1 )
                g_reg_file.pqcsr.pqmf = 0;
            if ( pqcsr_temp.pqof == 1 )
                g_reg_file.pqcsr.pqof = 0;
            // Clear the busy bit
            g_reg_file.pqcsr.busy = 0;
            break;
        case IPSR_OFFSET:
            // This 32-bits register (RW1C) reports the pending 
            // interrupts which require software service. Each 
            // interrupt-pending bit in the register corresponds to
            // a interrupt source in the IOMMU. When an 
            // interrupt-pending bit in the register is set to 1 the 
            // IOMMU will not signal another interrupt from that source till
            // software clears that interrupt-pending bit by writing 1 to clear it.
            // Update the RW1C bits - clear if written to 1
            if ( ipsr_temp.cip == 1 )
                g_reg_file.ipsr.cip = 0;
            // Pend interrupt If there are unacknowledge interrupts from CQ
            // If interrupts are enabled
            if ( ipsr_temp.cip == 1 ) {
                if ( (g_reg_file.cqcsr.cmd_to ||
                      g_reg_file.cqcsr.cmd_ill ||
                      g_reg_file.cqcsr.cqmf ||
                      g_reg_file.cqcsr.fence_w_ip) && 
                     (g_reg_file.cqcsr.cie == 1) ) {
                    generate_interrupt(COMMAND_QUEUE);
                }
            }
            if ( ipsr_temp.fip == 1 )
                g_reg_file.ipsr.fip = 0;
            // Pend interrupt If there are unacknowledge interrupts from FQ
            // If interrupts are enabled
            if ( ipsr_temp.fip == 1 ) {
                if ( (g_reg_file.fqcsr.fqof ||
                      g_reg_file.fqcsr.fqmf) &&
                     (g_reg_file.fqcsr.fie == 1) ) {
                    generate_interrupt(FAULT_QUEUE);
                }
            }
            if ( ipsr_temp.pmip == 1 )
                g_reg_file.ipsr.pmip = 0;
            // Pend interrupt If there are unacknowledge interrupts from PMU
            // NOTE THERE IS A BUG HERE - TBD to solve
            if ( ipsr_temp.pmip == 1 ) {
                if ( get_iocountovf() != 0 )
                    generate_interrupt(PMU);
            }
            if ( ipsr_temp.pip == 1 )
                g_reg_file.ipsr.pip = 0;
            // Pend interrupt If there are unacknowledge interrupts from FQ
            // If interrupts are enabled
            if ( ipsr_temp.pip == 1 ) {
                if ( (g_reg_file.pqcsr.pqof ||
                      g_reg_file.pqcsr.pqmf) &&
                     (g_reg_file.pqcsr.pie == 1) ) {
                    generate_interrupt(PAGE_QUEUE);
                }
            }
            break;
        case IOCNTOVF_OFFSET:
            // This register is read only
            return;
        case IOCNTINH_OFFSET:
            // This register is read-only 0 if capabilities.PMON is 0
            if ( g_reg_file.capabilities.pmon == 1 )
                g_reg_file.iocountinh.raw = data4 & ((1UL << g_num_hpm) - 1);
            break;
        case IOHPMCYCLES_OFFSET:
            // This register is read-only 0 if capabilities.PMON is 0
            if ( g_reg_file.capabilities.pmon == 1 ) {
                g_reg_file.iohpmcycles.counter = 
                    iohpmcycles_temp.counter & ((1UL << g_hpmctr_bits) - 1);
                g_reg_file.iohpmcycles.of = iohpmcycles_temp.of;
            }
            break;
        case IOHPMCTR1_OFFSET:
        case IOHPMCTR2_OFFSET:
        case IOHPMCTR3_OFFSET:
        case IOHPMCTR4_OFFSET:
        case IOHPMCTR5_OFFSET:
        case IOHPMCTR6_OFFSET:
        case IOHPMCTR7_OFFSET:
        case IOHPMCTR8_OFFSET:
        case IOHPMCTR9_OFFSET:
        case IOHPMCTR10_OFFSET:
        case IOHPMCTR11_OFFSET:
        case IOHPMCTR12_OFFSET:
        case IOHPMCTR13_OFFSET:
        case IOHPMCTR14_OFFSET:
        case IOHPMCTR15_OFFSET:
        case IOHPMCTR16_OFFSET:
        case IOHPMCTR17_OFFSET:
        case IOHPMCTR18_OFFSET:
        case IOHPMCTR19_OFFSET:
        case IOHPMCTR20_OFFSET:
        case IOHPMCTR21_OFFSET:
        case IOHPMCTR22_OFFSET:
        case IOHPMCTR23_OFFSET:
        case IOHPMCTR24_OFFSET:
        case IOHPMCTR25_OFFSET:
        case IOHPMCTR26_OFFSET:
        case IOHPMCTR27_OFFSET:
        case IOHPMCTR28_OFFSET:
        case IOHPMCTR29_OFFSET:
        case IOHPMCTR30_OFFSET:
        case IOHPMCTR31_OFFSET:
            // These register are read-only 0 if capabilities.PMON is 0
            if ( g_reg_file.capabilities.pmon == 1 ) { 
                ctr_num = ((offset - IOHPMCTR1_OFFSET)/8) + 1;
                // Writes discarded to non implemented HPM counters
                if ( ctr_num <= (g_num_hpm - 1) )  {
                    // These registers are 64-bit WARL counter registers
                    g_reg_file.iohpmctr[ctr_num - 1].counter = data8 & ((1UL << g_hpmctr_bits) - 1);
                }
            }
            break;
        case IOHPMEVT1_OFFSET:
        case IOHPMEVT2_OFFSET:
        case IOHPMEVT3_OFFSET:
        case IOHPMEVT4_OFFSET:
        case IOHPMEVT5_OFFSET:
        case IOHPMEVT6_OFFSET:
        case IOHPMEVT7_OFFSET:
        case IOHPMEVT8_OFFSET:
        case IOHPMEVT9_OFFSET:
        case IOHPMEVT10_OFFSET:
        case IOHPMEVT11_OFFSET:
        case IOHPMEVT12_OFFSET:
        case IOHPMEVT13_OFFSET:
        case IOHPMEVT14_OFFSET:
        case IOHPMEVT15_OFFSET:
        case IOHPMEVT16_OFFSET:
        case IOHPMEVT17_OFFSET:
        case IOHPMEVT18_OFFSET:
        case IOHPMEVT19_OFFSET:
        case IOHPMEVT20_OFFSET:
        case IOHPMEVT21_OFFSET:
        case IOHPMEVT22_OFFSET:
        case IOHPMEVT23_OFFSET:
        case IOHPMEVT24_OFFSET:
        case IOHPMEVT25_OFFSET:
        case IOHPMEVT26_OFFSET:
        case IOHPMEVT27_OFFSET:
        case IOHPMEVT28_OFFSET:
        case IOHPMEVT29_OFFSET:
        case IOHPMEVT30_OFFSET:
        case IOHPMEVT31_OFFSET:
            // These register are read-only 0 if capabilities.PMON is 0
            if ( g_reg_file.capabilities.pmon == 1 ) { 
                ctr_num = ((offset - IOHPMCTR1_OFFSET)/8);
                iohpmevt_temp.eventID &= g_eventID_mask;
                // Writes discarded to non implemented HPM counters
                if ( ctr_num < (g_num_hpm - 1) )  {
                    // These registers are 64-bit WARL counter registers
                    g_reg_file.iohpmevt[ctr_num].raw = iohpmevt_temp.raw;
                }
            }
            break;
        case ICVEC_OFFSET:
            // The performance-monitoring-interrupt-vector
            // (`pmiv`) is the vector number assigned to the
            // performance-monitoring-interrupt. This field is
            // read-only 0 if `capabilities.PMON` is 0.
            if ( g_reg_file.capabilities.pmon == 0 ) { 
                icvec_temp.pmiv = 0;
            }
            // The page-request-queue-interrupt-vector (`piv`)
            // is the vector number assigned to the
            // page-request-queue-interrupt. This field is
            // read-only 0 if `capabilities.ATS` is 0.
            if ( g_reg_file.capabilities.ats == 0 ) { 
                icvec_temp.piv = 0;
            }
            // If an implementation only supports a single vector then all 
            // bits of this register may be hardwired to 0 (WARL). Likewise 
            // if only two vectors are supported then only bit 0 for each 
            // cause could be writable.
            g_reg_file.icvec.pmiv = icvec_temp.pmiv & ((1UL << g_num_vec_bits) - 1);
            g_reg_file.icvec.piv  = icvec_temp.piv & ((1UL << g_num_vec_bits) - 1);
            g_reg_file.icvec.fiv  = icvec_temp.fiv & ((1UL << g_num_vec_bits) - 1);
            g_reg_file.icvec.civ  = icvec_temp.civ & ((1UL << g_num_vec_bits) - 1);
            break;
        case MSI_ADDR_0_OFFSET:
        case MSI_ADDR_1_OFFSET:
        case MSI_ADDR_2_OFFSET:
        case MSI_ADDR_3_OFFSET:
        case MSI_ADDR_4_OFFSET:
        case MSI_ADDR_5_OFFSET:
        case MSI_ADDR_6_OFFSET:
        case MSI_ADDR_7_OFFSET:
        case MSI_ADDR_8_OFFSET:
        case MSI_ADDR_9_OFFSET:
        case MSI_ADDR_10_OFFSET:
        case MSI_ADDR_11_OFFSET:
        case MSI_ADDR_12_OFFSET:
        case MSI_ADDR_13_OFFSET:
        case MSI_ADDR_14_OFFSET:
        case MSI_ADDR_15_OFFSET:
            // IOMMU that supports MSI implements a MSI configuration table 
            // that is indexed by the vector from icvec to determine a MSI table entry. 
            // Each MSI table entry for interrupt vector x has three registers msi_addr_x, 
            // msi_data_x, and msi_vec_ctrl_x.  If number of writable bits in each field 
            // of icvec is V, then x is a number between 0 and 2V - 1. If V is less than 4 
            // then MSI configuration table entries 2^V to 15 are read-only 0. These registers 
            // are read-only 0 if the IOMMU does not support MSI 
            // (i.e., if capabilities.IGS == WIS).
            if ( g_reg_file.capabilities.igs == WIS )
                break;
            x = (offset - MSI_ADDR_0_OFFSET) / 16;
            if ( x >= (1UL << g_num_vec_bits) ) 
                break;
            msi_addr_temp.addr = msi_addr_temp.addr & (pa_mask >> 2);
            g_reg_file.msi_cfg_tbl[x].msi_addr.addr = msi_addr_temp.addr;
            break;
        case MSI_DATA_0_OFFSET:
        case MSI_DATA_1_OFFSET:
        case MSI_DATA_2_OFFSET:
        case MSI_DATA_3_OFFSET:
        case MSI_DATA_4_OFFSET:
        case MSI_DATA_5_OFFSET:
        case MSI_DATA_6_OFFSET:
        case MSI_DATA_7_OFFSET:
        case MSI_DATA_8_OFFSET:
        case MSI_DATA_9_OFFSET:
        case MSI_DATA_10_OFFSET:
        case MSI_DATA_11_OFFSET:
        case MSI_DATA_12_OFFSET:
        case MSI_DATA_13_OFFSET:
        case MSI_DATA_14_OFFSET:
        case MSI_DATA_15_OFFSET:
            // IOMMU that supports MSI implements a MSI configuration table 
            // that is indexed by the vector from icvec to determine a MSI table entry. 
            // Each MSI table entry for interrupt vector x has three registers msi_addr_x, 
            // msi_data_x, and msi_vec_ctrl_x.  If number of writable bits in each field 
            // of icvec is V, then x is a number between 0 and 2V - 1. If V is less than 4 
            // then MSI configuration table entries 2^V to 15 are read-only 0. These registers 
            // are read-only 0 if the IOMMU does not support MSI 
            // (i.e., if capabilities.IGS == WIS).
            if ( g_reg_file.capabilities.igs == WIS )
                break;
            x = (offset - MSI_ADDR_0_OFFSET) / 16;
            if ( x >= (1UL << g_num_vec_bits) ) 
                break;
            g_reg_file.msi_cfg_tbl[x].msi_data = data4;
            break;
        case MSI_VEC_CTRL_0_OFFSET:
        case MSI_VEC_CTRL_1_OFFSET:
        case MSI_VEC_CTRL_2_OFFSET:
        case MSI_VEC_CTRL_3_OFFSET:
        case MSI_VEC_CTRL_4_OFFSET:
        case MSI_VEC_CTRL_5_OFFSET:
        case MSI_VEC_CTRL_6_OFFSET:
        case MSI_VEC_CTRL_7_OFFSET:
        case MSI_VEC_CTRL_8_OFFSET:
        case MSI_VEC_CTRL_9_OFFSET:
        case MSI_VEC_CTRL_10_OFFSET:
        case MSI_VEC_CTRL_11_OFFSET:
        case MSI_VEC_CTRL_12_OFFSET:
        case MSI_VEC_CTRL_13_OFFSET:
        case MSI_VEC_CTRL_14_OFFSET:
        case MSI_VEC_CTRL_15_OFFSET:
            // IOMMU that supports MSI implements a MSI configuration table 
            // that is indexed by the vector from icvec to determine a MSI table entry. 
            // Each MSI table entry for interrupt vector x has three registers msi_addr_x, 
            // msi_data_x, and msi_vec_ctrl_x.  If number of writable bits in each field 
            // of icvec is V, then x is a number between 0 and 2V - 1. If V is less than 4 
            // then MSI configuration table entries 2^V to 15 are read-only 0. These registers 
            // are read-only 0 if the IOMMU does not support MSI 
            // (i.e., if capabilities.IGS == WIS).
            if ( g_reg_file.capabilities.igs == WIS )
                break;
            x = (offset - MSI_ADDR_0_OFFSET) / 16;
            if ( x >= (1UL << g_num_vec_bits) ) 
                break;
            g_reg_file.msi_cfg_tbl[x].msi_vec_ctrl.m = msi_vec_ctrl_temp.m;
            break;
    }
    return;
}
int 
reset_iommu(uint8_t num_hpm, uint8_t hpmctr_bits, uint16_t eventID_mask, 
                uint8_t num_vec_bits, uint8_t reset_iommu_mode, 
                capabilities_t capabilities, fctrl_t fctrl) {
    int i;

    // Only PA upto 56 bits supported in RISC-V
    if ( capabilities.pas > 56 )
        return -1;
    // Only one of MSI, WIS, or BOTH supported
    if ( capabilities.igs != MSI && 
         capabilities.igs != WIS && 
         capabilities.igs != IGS_BOTH )
        return -1;
    // If IGS_BOTH is not supported then WIS must be 0
    // if MSI is only supported mode else it must be 1
    if ( capabilities.igs != IGS_BOTH && 
         ((capabilities.igs == MSI && fctrl.wis != 0) ||
          (capabilities.igs == WIS && fctrl.wis == 0)) )
        return -1;
    // Only 15-bit event ID supported
    // Mask must be 0 when pmon not supported
    if ( g_eventID_mask != 0 && capabilities.pmon == 0 )
        return -1; 
    // vectors is a number between 1 and 15
    if ( num_vec_bits > 4 )
        return -1;
    // Number of HPM counters must be between 0 and 31
    // If perfmon is not supported then should be 0
    if ( num_hpm > 31 ||
         (num_hpm != 0 && capabilities.pmon == 0) )
        return -1;
    // HPM counters must be between 1 and 63 bits
    if ( (hpmctr_bits < 1 || hpmctr_bits > 63)  && 
         (capabilities.pmon == 1) )
        return -1;
    if ( hpmctr_bits != 0 && capabilities.pmon == 0 ) 
        return -1;
    // Reset value for ddtp.iommu_mode field must be either Off or Bare
    if ( reset_iommu_mode != Off && reset_iommu_mode != DDT_Bare )
        return -1;

    g_eventID_mask = eventID_mask;
    g_num_vec_bits = num_vec_bits;
    g_num_hpm = num_hpm;
    g_hpmctr_bits = hpmctr_bits;


    // Initialize registers that have resets to 0
    // The reset default value is 0 for the following registers. 
    // Section 4.2 - Reset value is implementation-defined for all
    // other registers and/or fields.
    // - fctrl
    // - cqcsr
    // - fqcsr
    // - pqcsr
    // If test needs random values then use the register read/write
    // interface to setup random values. By default all registers are
    // cleared to 0
    memset(&g_reg_file, 0, sizeof(g_reg_file));

    // Initialize the reset default capabilities and feature
    // control.
    g_reg_file.capabilities = capabilities;
    g_reg_file.fctrl = fctrl;

    // Reset value for ddtp.iommu_mode field must be either Off or Bare. 
    // The reset value for ddtp.busy field must be 0.
    g_reg_file.ddtp.iommu_mode = reset_iommu_mode;

    // Initialize the offset to register size mapping array

    // Initialize offsets as invalid by default
    for ( i = 0; i < 4096; i++ )
        g_offset_to_size[i] = 0xFF;

    g_offset_to_size[CAPABILITIES_OFFSET] = 8;
    g_offset_to_size[FCTRL_OFFSET] = 4;
    g_offset_to_size[DDTP_OFFSET] = 8;
    g_offset_to_size[CQB_OFFSET] = 8;
    g_offset_to_size[CQH_OFFSET] = 4;
    g_offset_to_size[CQT_OFFSET] = 4;
    g_offset_to_size[FQB_OFFSET] = 8;
    g_offset_to_size[FQH_OFFSET] = 4;
    g_offset_to_size[FQT_OFFSET] = 4;
    g_offset_to_size[PQB_OFFSET] = 8;
    g_offset_to_size[PQH_OFFSET] = 4;
    g_offset_to_size[PQT_OFFSET] = 4;
    g_offset_to_size[CQCSR_OFFSET] = 4;
    g_offset_to_size[FQCSR_OFFSET] = 4;
    g_offset_to_size[PQCSR_OFFSET] = 4;
    g_offset_to_size[IPSR_OFFSET] = 4;
    g_offset_to_size[IOCNTOVF_OFFSET] = 4;
    g_offset_to_size[IOCNTINH_OFFSET] = 4;
    g_offset_to_size[IOHPMCYCLES_OFFSET] = 4;
    for ( i = IOHPMCTR1_OFFSET; i < IOHPMCTR1_OFFSET + (8 * 31); i += 8 ) {
        g_offset_to_size[i] = 8;
    }
    for ( i = IOHPMEVT1_OFFSET; i < IOHPMEVT1_OFFSET + (8 * 31); i += 8 ) {
        g_offset_to_size[i] = 8;
    }
    for ( i = 600; i < ICVEC_OFFSET; i++ ) {
        g_offset_to_size[i] = 1;
    }
    g_offset_to_size[ICVEC_OFFSET] = 4;
    for ( i = 0; i < 256; i += 16) {
        g_offset_to_size[i + MSI_ADDR_0_OFFSET] = 8;
        g_offset_to_size[i + MSI_DATA_0_OFFSET] = 4;
        g_offset_to_size[i + MSI_VEC_CTRL_0_OFFSET] = 4;
    }
    return 0;
}

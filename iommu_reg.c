#include "iommu_registers.h"
#include "iommu_data_structures.h"
int is_access_valid(
    uint16_t offset, uint8_t num_bytes) {
    // The IOMMU behavior for register accesses where the 
    // address is not aligned to the size of the access or
    // if the access spans multiple registers is undefined
    if ( (num_bytes !=4 && num_bytes != 8) ||        // only 4B & 8B registers in IOMMU
         (offset >= 4096) ||                         // Offset must be <= 4095
         ((offset & (num_bytes - 1)) != 0) ||        // Offset must be aligned to size
         (offset_to_size[offset] != num_bytes) ) {   // Acesss cannot span two registers
        return 0;
    }
    return 1;
}
uint64_t 
read_register(
    uint16_t offset, uint8_t num_bytes) {

    // If access is not valid then return -1
    if ( !is_access_valid(offset, num_bytes) ) {
        return -1;
    }

    // If access is valid then return data from the register file
    return ( num_bytes == 4 ) ? *((uint32_t *)&g_reg_file.regs[offset]) :
                                *((uint64_t *)&g_reg_file.regs[offset]);
}
void 
write_register(
    uint16_t offset, uint8_t num_bytes, uint64_t data) {

    uint32_t data4 = data & 0xFFFFFFFF;
    uint32_t data8 = data;
    cqcsr_t cqcsr_temp;
    fqcsr_t fqcsr_temp;
    pqcsr_t pqcsr_temp;
    ipsr_t  ipsr_temp;
    fctrl_t fctrl_temp;
    ddtp_t  ddtp_temp;
     
    // If access is not valid then discard the write
    if ( !is_access_valid(offset, num_bytes) ) {
        return;
    }

    // Clean the reserved bits
    if ( num_bytes == 4 ) {
        data4 &= ~*((uint32_t *)&g_reg_types.regs[offset]);
    } else {
        data8 &= ~*((uint64_t *)&g_reg_types.regs[offset]);
    }
    switch (offset) {
        case CAPABILITIES_OFFSET:
        case CQH_OFFSET:
        case FQT_OFFSET:
        case PQT_OFFSET:
        case IOCNTOVF_OFFSET:
            // These register are read only
            return;
        case FCTRL_OFFSET:
            // FCTRL is writeable if IOMMU is bi-endian
            // or supports both wired and MSI interrupts
            // retain default values for the field if not
            // writeable
            fctrl_temp.raw = data4;
            if ( (g_reg_file.capabilities.end == ONE_END) ) {
                fctrl_temp.end = g_reg_file.fctrl.end;
            }
            if ( (g_reg_file.capabilities.igs != IGS_BOTH) ) {
                fctrl_temp.wis = g_reg_file.fctrl.wis;
            }
            data = fctrl_temp.raw;
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
            if ( g_reg_file.ddtp.busy ) {
                return;
            }
            ddtp_temp.raw = data8;
            // If a illegal value written to ddtp.iommu_mode then 
            // retain the current legal value
            if ( ddtp_temp.iommu_mode != Off &&
                 ddtp_temp.iommu_mode != DDT_Bare &&
                 ddtp_temp.iommu_mode != DDT_1LVL &&
                 ddtp_temp.iommu_mode != DDT_2LVL &&
                 ddtp_temp.iommu_mode != DDT_3LVL ) {
                ddtp_temp.iommu_mode = g_reg_file.ddtp.iommu_mode;
            }
            data8 = ddtp_temp.raw;
            break;
        case CQB_OFFSET:
        case CQT_OFFSET:
            // The command-queue is active if cqon is 1. IOMMU behavior on
            // changing cqb when busy is 1 or cqon is 1 is implementation
            // defined. The software recommended sequence to change cqb is to
            // first disable the command-queue by clearing cqen and waiting for
            // both busy and cqon to be 0 before changing the cqb.
            // The reference model discards the write
            if ( g_reg_file.cqcsr.busy || g_reg_file.cqcsr.cqon ) {
                return;
            }
            break;
        case FQB_OFFSET:
        case FQH_OFFSET:
            // The fault-queue is active if `fqon` reads 1.
            // IOMMU behavior on changing `fqb` when `busy` is 1
            // or `fqon` is 1 implementation defined. The
            // recommended sequence to change `fqb` is to first
            // disable the fault-queue by clearing `fqen` and
            // waiting for both `busy` and `fqon` to be 0 before
            // changing `fqb`.
            // The reference model discards the write
            if ( g_reg_file.fqcsr.busy || g_reg_file.fqcsr.fqon ) {
                return;
            }
            break;
        case PQB_OFFSET:
        case PQH_OFFSET:
            // The page-request is active when `pqon` reads 1.
            // IOMMU behavior on changing `pqb` when `busy` is 1
            // or `pqon` is 1 implementation defined. The
            // recommended sequence to change `pqb` is to first
            // disable the page-request queue by clearing `pqen`
            // and waiting for both `busy` and `pqon` to be 0
            // before changing `pqb`.
            // The reference model discards the write
            if ( g_reg_file.pqcsr.busy || g_reg_file.pqcsr.pqon ) {
                return;
            }
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
            if ( g_reg_file.cqcsr.busy ) {
                return;
            }
            cqcsr_temp.raw = data4;
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
                    // Holds the index into the command-queue where software queues
                    // the next command for IOMMU. Only LOG2SZ:0 bits are writable
                    // when the queue is in enabled state (i.e., cqsr.cqon == 1).
                    g_reg_types.cqt.index = (1 << (g_reg_file.cqb.log2szm1 + 1)) - 1;
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
            g_reg_file.cqcsr.cqmf &= ~cqcsr_temp.cqmf;
            g_reg_file.cqcsr.cmd_to &= ~cqcsr_temp.cmd_to;
            g_reg_file.cqcsr.cmd_ill &= ~cqcsr_temp.cmd_ill;
            g_reg_file.cqcsr.fence_w_ip &= ~cqcsr_temp.fence_w_ip;

            // Clear the busy bit
            g_reg_file.cqcsr.busy = 0;
            data4 = g_reg_file.cqcsr.raw;
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
            fqcsr_temp.raw = data4;
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
                    // Holds the index into the command-queue where software queues
                    // the next command for IOMMU. Only LOG2SZ:0 bits are writable
                    // when the queue is in enabled state (i.e., cqsr.cqon == 1).
                    g_reg_types.fqh.index = (1 << (g_reg_file.fqb.log2szm1 + 1)) - 1;
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
            g_reg_file.fqcsr.fqmf &= ~fqcsr_temp.fqmf;
            g_reg_file.fqcsr.fqof &= ~fqcsr_temp.fqof;
            // Clear the busy bit
            g_reg_file.fqcsr.busy = 0;
            data4 = g_reg_file.fqcsr.raw;
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
            pqcsr_temp.raw = data4;
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
                    // Holds the index into the command-queue where software queues
                    // the next command for IOMMU. Only LOG2SZ:0 bits are writable
                    // when the queue is in enabled state (i.e., cqsr.cqon == 1).
                    g_reg_types.pqh.index = (1 << (g_reg_file.pqb.log2szm1 + 1)) - 1;
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
            g_reg_file.pqcsr.pqmf &= ~pqcsr_temp.pqmf;
            g_reg_file.pqcsr.pqof &= ~pqcsr_temp.pqof;
            // Clear the busy bit
            g_reg_file.pqcsr.busy = 0;
            data4 = g_reg_file.pqcsr.raw;
            break;
        case IPSR_OFFSET:
            // This 32-bits register (RW1C) reports the pending 
            // interrupts which require software service. Each 
            // interrupt-pending bit in the register corresponds to
            // a interrupt source in the IOMMU. When an 
            // interrupt-pending bit in the register is set to 1 the 
            // IOMMU will not signal another interrupt from that source till
            // software clears that interrupt-pending bit by writing 1 to clear it.
            ipsr_temp.raw = data4;
            // Update the RW1C bits - clear if written to 1
            g_reg_file.ipsr.cip &= ~ipsr_temp.cip;
            g_reg_file.ipsr.fip &= ~ipsr_temp.fip;
            g_reg_file.ipsr.pmip &= ~ipsr_temp.pmip;
            g_reg_file.ipsr.pip &= ~ipsr_temp.pip;
            data4 = g_reg_file.ipsr.raw;
            break;
    }
    // Write the data
    if ( num_bytes == 4 ) {
        *((uint32_t *)&g_reg_file.regs[offset]) = data4;
    } else {
        *((uint64_t *)&g_reg_file.regs[offset]) = data8;
    }
    return;
}

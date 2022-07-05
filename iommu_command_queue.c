#include "iommu.h"
void
process_commands(void) {

    if ( (g_reg_file.cqcsr.cqon == 0) ||
         (g_reg_file.cqcsr.cqmf != 0) ||
         (g_reg_file.cqcsr.cmd_ill != 0) ||
         (g_reg_file.cqcsr.cmd_to != 0) )
        return;
    if ( g_reg_file.cqh.index == g_reg_file.cqt.index )
        return;
    a = g_reg_file.cqb.ppn * PAGESIZE | (g_reg_file.cqh.index * 16);
    status = read_memory(a, 16, (char *)&command.raw);
    if ( status != 0 ) {
        g_reg_file.cqcsr.cqmf = 1;
        generate_interrupt(COMMAND_QUEUE);
        return;
    }
    switch ( command.opcode ) {
        case IOTINVAL:
            func3    = get_bits(9,   7, command.low);
            PSCV     = get_bits(10, 10, command.low);
            AV       = get_bits(11, 11, command.low);
            GV       = get_bits(12, 12, command.low);
            reserved = get_bits(15, 13, command.low);
            PSCID    = get_bits(35, 16, command.low);
            reserved|= get_bits(39, 36, command.low);
            GSCID    = get_bits(55, 40, command.low);
            reserved|= get_bits(63, 56, command.low);
            ADDR     = get_bits(52,  0, command.high);
            if ( reserved ) goto command_illegal;
            switch ( func3 ) {
                case VMA:
                    do_iotinval_vma(GV, AV, PSCV, GSCID, PSCID, ADDR);
                    break;
                case GVMA:
                    if ( PSCV ) goto command_illegal;
                    do_iotinval_vma(GV, AV, GSCID, ADDR);
                    break;
                case MSI:
                    if ( PSCV ) goto command_illegal;
                    INT_FILE_NUM = ADDR;
                    do_iotinval_msi(GV, AV, GSCID, INT_FILE_NUM);
                    break;
                default: goto command_illegal;
            }
            break;
        case IODIR:
            func3    = get_bits(9,   7, command.low);
            DV       = get_bits(10, 10, command.low);
            reserved = get_bits(15, 11, command.low);
            PID      = get_bits(35, 16, command.low);
            reserved|= get_bits(39, 36, command.low);
            DID      = get_bits(55, 40, command.low);
            reserved|= get_bits(63, 56, command.low);
            reserved|= get_bits(63,  0, command.high);
            if ( reserved ) goto command_illegal;
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
            func3    = get_bits(9,   7, command.low);
            PR       = get_bits(10, 10, command.low);
            PW       = get_bits(11, 11, command.low);
            AV       = get_bits(12, 12, command.low);
            WIS      = get_bits(13, 13, command.low);
            reserved = get_bits(31, 14, command.low);
            DATA     = get_bits(63, 32, command.low);
            reserved|= get_bits(1,   0, command.high);
            ADDR    |= get_bits(63,  0, command.high);
            if ( reserved ) goto command_illegal;
            switch ( func3 ) {
                case IOFENCE_C:
                    do_iofence_c(PR, PW, AV, WIS, ADDR, DATA);
                    break;
                default: goto command_illegal;
            }
            break;
        case ATS:
            func3    = get_bits(9,   7, command.low);
            DSV      = get_bits(10, 10, command.low);
            PV       = get_bits(11, 11, command.low);
            reserved = get_bits(15, 12, command.low);
            PID      = get_bits(35, 16, command.low);
            reserved|= get_bits(39, 36, command.low);
            DSEG     = get_bits(47, 40, command.low);
            RID      = get_bits(63, 48, command.low);
            PAYLOAD  = get_bits(63,  0, command.high);
            if ( reserved ) goto command_illegal;
            switch ( func3 ) {
                case INVAL:
                case PRGR:
                    send_message(DV, DSEG, RID, PV, PID, PAYLOAD);
                    break;
                default: goto command_illegal;
            }
        default: goto command_illegal;
    }

}
void
do_inval_ddt(uint8_t DV, uint32_t DID) {
    int i;
    for ( i = 0; i < DDT_CACHE_SIZE; i++ ) {
        if ( DV == 0 ) 
            ddt_cache_tag[i].valid = 0
        if ( DV == 1 && (ddt_cache_tag.DID == DID) ) 
            ddt_cache_tag[i].valid = 0
    }
    return;
}
void
do_inval_pdt(uint32_t DID, uint32_t PID) {
    int i;
    for ( i = 0; i < PDT_CACHE_SIZE; i++ ) {
        if ( pdt_cache_tag[i].DID == DID && 
             pdt_cache_tag[i].PID == PID ) 
            ddt_cache_tag[i].valid = 0
    }
    return;
}
void 
do_iotinval_vma(uint8_t GV, uint8_t AV, uint8_t PSCV, 
                uint32_t GSCID, uint32_t PSCID, uint64_t ADDR) {
    int i;
    for ( i = 0; i < ATC_SIZE; i++ ) {
        gscid_match = pscid_match = addr_match = global_match = 0;
        if ( (atc_tag[i].GV == GV) && (atc_tag[i].GSCID == GSCID) )
            gscid_match = 1;
        if ( (atc_tag[i].PSCV == PSCV) && (atc_tag[i].PSCID == PSCID) )
            pscid_match = 1;
        if ( (atc_tag[i].AV == 1) && (atc_tag[i].ADDR == ADDR) )
            addr_match = 1;
        if ( (PSCV == 1 && AV == 1) || (atc_tag[i].G == 0) )
            global_match = 1;
        if ( gscid_match && pscid_match && addr_match && global_match ) {
            atc_tag[i].valid = 0;
        }
    }
}

#include "iommu_registers.h"
#include "iommu_data_structures.h"
#include "iommu_req_rsp.h"
#include "iommu_fault.h"
void 
generate_interrupt(
    uint8_t unit) {
    uint64_t msi_addr;
    uint32_t msi_data;
    uint32_t msi_vec_ctrl;

    switch ( unit ) {
        case FAULT_QUEUE;
            if ( g_reg_file.ipsr.fip == 1) 
                return;
            if ( g_reg_file.ipsr.fie == 0) 
                return;
            vec = g_reg_file.icvec.fiv;
            g_reg_file.ipsr.fip = 1;
            break;
        case PAGE_QUEUE;
            if ( g_reg_file.ipsr.pip == 1) 
                return;
            if ( g_reg_file.ipsr.pie == 0) 
                return;
            vec = g_reg_file.icvec.piv;
            g_reg_file.ipsr.pip = 1;
            break;
        case COMMAND_QUEUE;
            if ( g_reg_file.ipsr.cip == 1) 
                return;
            if ( g_reg_file.ipsr.cie == 0) 
                return;
            vec = g_reg_file.icvec.civ;
            g_reg_file.ipsr.cip = 1;
            break;
        case PMU;
            if ( g_reg_file.ipsr.pmip == 1) 
                return;
            if ( g_reg_file.ipsr.pmie == 0) 
                return;
            vec = g_reg_file.icvec.pmiv;
            g_reg_file.ipsr.pmip = 1;
            break;
    }
    if ( g_reg_file.fctrl.wis == 1 ) {
        msi_addr = g_reg_file.msi_cfg_tbl[vec].msi_addr;
        msi_data = g_reg_file.msi_cfg_tbl[vec].msi_data;
        msi_vec_ctrl = g_reg_file.msi_cfg_tbl[vec].msi_vec_ctrl;
        if ( msi_vec_ctrl & MASK_BIT ) {
            return;
        }
        write_mem(msi_data, msi_addr, 4, &access_fault, &data_corruption);
    }
    return;
}

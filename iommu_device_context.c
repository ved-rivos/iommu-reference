// Copyright (c) 2022 by Rivos Inc.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
// Author: ved@rivosinc.com

#include "iommu.h"

uint8_t
locate_device_context(
    device_context_t *DC, uint8_t *DDI, uint32_t *cause) {
    uint64_t a;
    uint8_t i, LEVELS, status, DC_SIZE;
    ddte_t ddte;

    // The DDT used to locate the DC may be configured to be a 1, 2, or 3 level 
    // radix-table depending on the maximum width of the device_id supported. 
    // The partitioning of the device_id to obtain the device directory indexes
    // (DDI) to traverse the DDT radix-tree table are as follows:

    // The process to locate the Device-context for transaction 
    // using its `device_id` is as follows:
    // 1. Let `a` be `ddtp.PPN x 2^12^` and let `i = LEVELS - 1`. When
    //    `ddtp.iommu_mode` is `3LVL`, `LEVELS` is three. When `ddtp.iommu_mode` is
    //    `2LVL`, `LEVELS` is two. When `ddtp.iommu_mode` is `1LVL`, `LEVELS` is one.
    a = g_reg_file.ddtp.ppn * PAGESIZE;
    if ( g_reg_file.ddtp.iommu_mode == DDT_3LVL ) LEVELS = 3;
    if ( g_reg_file.ddtp.iommu_mode == DDT_2LVL ) LEVELS = 2;
    if ( g_reg_file.ddtp.iommu_mode == DDT_1LVL ) LEVELS = 1;
    i = LEVELS - 1;

step_2:
    // 2. If `i == 0` go to step 8.
    if ( i == 0 ) goto step_8;

    // 3. Let `ddte` be value of eight bytes at address `a + DDI[i] x 8`. If accessing
    //    `ddte` violates a PMA or PMP check, then stop and report "DDT entry load
    //     access fault" (cause = 257).
    status = read_memory((a + (DDI[i] * 8)), 8, (char *)&ddte.raw);
    if ( status & ACCESS_FAULT ) {
        *cause = 257;     // DDT entry load access fault
        return 1;
    }

    // 4. If `ddte` access detects a data corruption (a.k.a. poisoned data), then
    //    stop and report "DDT data corruption" (cause = 268). This fault is detected
    //    if the IOMMU supports the RAS capability (`capabilities.RAS == 1`).
    if ( (status & DATA_CORRUPTION) && (g_reg_file.capabilities.ras == 1) ) {
        *cause = 268;     // DDT data corruption
        return 1;
    }

    // 5. If `ddte.V == 0`, stop and report "DDT entry not valid" (cause = 258).
    if ( ddte.V == 0 ) {
        *cause = 258;     // DDT entry not valid
        return 1;
    }

    // 6. If if any bits or encoding that are reserved for future standard use are
    //    set within `ddte`, stop and report "DDT entry misconfigured"
    //    (cause = 259).
    if ( ddte.reserved0 != 0 || ddte.reserved1 != 0 ) {
        *cause = 259;     // DDT entry misconfigured
        return 1;
    }

    // 7. Let `i = i - 1` and let `a = ddte.PPN x 2^12^`. Go to step 4.
    i = i - 1;
    a = ddte.PPN * PAGESIZE;
    goto step_2;

step_8:
    // 8. Let `DC` be value of `DC_SIZE` bytes at address `a + DDI[0] * DC_SIZE`. If
    //    `capabilities.MSI_FLAT` is 1 then `DC_SIZE` is 64-bytes else it is 32-bytes.
    //    If accessing `DC` violates a PMA or PMP check, then stop and report
    //    "DDT entry load access fault" (cause = 257). If `DC` access detects a data
    //    corruption (a.k.a. poisoned data), then stop and report "DDT data corruption"
    //    (cause = 268). This fault is detected if the IOMMU supports the RAS capability
    //    (`capabilities.RAS == 1`).
    DC_SIZE = ( g_reg_file.capabilities.msi_flat == 1 ) ? EXT_FORMAT_DC_SIZE : BASE_FORMAT_DC_SIZE;
    status = read_memory((a + (DDI[i] * DC_SIZE)), DC_SIZE, (char *)DC);
    if ( status & ACCESS_FAULT ) {
        *cause = 257;     // DDT entry load access fault
        return 1;
    }
    if ( (status & DATA_CORRUPTION) && (g_reg_file.capabilities.ras == 1) ) {
        *cause = 268;     // DDT data corruption
        return 1;
    }
    // 9. If `DC.tc.V == 0`, stop and report "DDT entry not valid" (cause = 258).
    if ( DC->tc.V == 0 ) {
        *cause = 258;     // DDT entry not valid
        return 1;
    }
    //10. If any bits or encoding that are reserved for future standard use are set
    //    within `DC`, stop and report "DDT entry misconfigured" (cause = 259).
    if ( ((g_reg_file.capabilities.msi_flat == 1) && (DC->reserved != 0)) ||
         ((g_reg_file.capabilities.msi_flat == 1) && (DC->msiptp.reserved != 0)) ||
         (DC->tc.reserved != 0) ||
         (DC->fsc.pdtp.reserved != 0 && DC->tc.PDTV == 1) ||
         (DC->fsc.iosatp.reserved != 0 && DC->tc.PDTV == 0) ||
         (DC->ta.reserved != 0) ) {
        *cause = 259;     // DDT entry misconfigured
        return 1;
    }
    if ( ((DC->tc.EN_ATS || DC->tc.EN_PRI || DC->tc.PRPR) &&
          (g_reg_file.capabilities.ats == 0)) ||
         (DC->tc.T2GPA && (g_reg_file.capabilities.t2gpa == 0)) ) {
        *cause = 259;     // DDT entry misconfigured
        return 1;
    }
    if ( (DC->iohgatp.MODE != IOHGATP_Bare) &&
         (DC->iohgatp.MODE != IOHGATP_Sv32x4) &&
         (DC->iohgatp.MODE != IOHGATP_Sv39x4) &&
         (DC->iohgatp.MODE != IOHGATP_Sv48x4) &&
         (DC->iohgatp.MODE != IOHGATP_Sv57x4) ) {
        *cause = 259;     // DDT entry misconfigured
        return 1;
    }
    if ( ((DC->iohgatp.MODE == IOHGATP_Sv32x4) && (g_reg_file.capabilities.Sv32x4 == 0)) &&
         ((DC->iohgatp.MODE == IOHGATP_Sv39x4) && (g_reg_file.capabilities.Sv39x4 == 0)) &&
         ((DC->iohgatp.MODE == IOHGATP_Sv48x4) && (g_reg_file.capabilities.Sv48x4 == 0)) &&
         ((DC->iohgatp.MODE == IOHGATP_Sv57x4) && (g_reg_file.capabilities.Sv57x4 == 0)) ) {
        *cause = 259;     // DDT entry misconfigured
        return 1;
    }
    if ( (DC->tc.PDTV == 1) && 
         ((DC->fsc.pdtp.MODE != PDTP_Bare) &&
          (DC->fsc.pdtp.MODE != PD20) &&
          (DC->fsc.pdtp.MODE != PD17) &&
          (DC->fsc.pdtp.MODE != PD8)) ) {
        *cause = 259;     // DDT entry misconfigured
        return 1;
    }
    if ( (DC->tc.PDTV == 0) && 
         ((DC->fsc.iosatp.MODE != IOSATP_Bare) &&
          (DC->fsc.iosatp.MODE != IOSATP_Sv32) &&
          (DC->fsc.iosatp.MODE != IOSATP_Sv39) &&
          (DC->fsc.iosatp.MODE != IOSATP_Sv48) &&
          (DC->fsc.iosatp.MODE != IOSATP_Sv57)) ) {
        *cause = 259;     // DDT entry misconfigured
        return 1;
    }
    if ( (DC->tc.PDTV == 0) && 
         ((DC->fsc.iosatp.MODE == IOSATP_Sv32) && (g_reg_file.capabilities.Sv32 == 0)) &&
         ((DC->fsc.iosatp.MODE == IOSATP_Sv39) && (g_reg_file.capabilities.Sv39 == 0)) &&
         ((DC->fsc.iosatp.MODE == IOSATP_Sv48) && (g_reg_file.capabilities.Sv48 == 0)) &&
         ((DC->fsc.iosatp.MODE == IOSATP_Sv57) && (g_reg_file.capabilities.Sv57 == 0)) ) {
        *cause = 259;     // DDT entry misconfigured
        return 1;
    }
    if ( (g_reg_file.capabilities.msi_flat == 0) && 
         ((DC->msiptp.MODE != MSIPTP_Bare) &&
          (DC->msiptp.MODE != MSIPTP_Flat)) ) {
        *cause = 259;     // DDT entry misconfigured
        return 1;
    }
    //11. The device-context has been successfully located and may be cached.
    return 0;
}

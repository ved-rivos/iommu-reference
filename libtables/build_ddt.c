// Copyright (c) 2022 by Rivos Inc.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
// Author: ved@rivosinc.com
#include "iommu.h"

uint8_t
add_dev_context(
    device_context_t *DC, uint32_t device_id) {

    uint64_t a;
    uint8_t i, LEVELS, DC_SIZE;
    ddte_t ddte;
    uint8_t DDI[3];
    // The DDT used to locate the DC may be configured to be a 1, 2, or 3 level 
    // radix-table depending on the maximum width of the device_id supported. 
    // The partitioning of the device_id to obtain the device directory indexes
    // (DDI) to traverse the DDT radix-tree table are as follows:
    // If `capabilities.MSI_FLAT` is 0 then the IOMMU uses base-format device
    // context. Let `DDI[0]` be `device_id[6:0]`, `DDI[1]` be `device_id[15:7]`, and
    // `DDI[2]` be `device_id[23:16]`.
    if ( g_reg_file.capabilities.msi_flat == 0 ) {
        DDI[0] = get_bits(6,   0, device_id);
        DDI[1] = get_bits(15,  7, device_id);
        DDI[2] = get_bits(23, 16, device_id);
    }
    // If `capabilities.MSI_FLAT` is 1 then the IOMMU uses extended-format device
    // context. Let `DDI[0]` be `device_id[5:0]`, `DDI[1]` be `device_id[14:6]`, and
    // `DDI[2]` be `device_id[23:15]`.
    if ( g_reg_file.capabilities.msi_flat == 1 ) {
        DDI[0] = get_bits(5,   0, device_id);
        DDI[1] = get_bits(14,  6, device_id);
        DDI[2] = get_bits(23, 15, device_id);
    }
    a = g_reg_file.ddtp.ppn * PAGESIZE;
    if ( g_reg_file.ddtp.iommu_mode == DDT_3LVL ) LEVELS = 3;
    if ( g_reg_file.ddtp.iommu_mode == DDT_2LVL ) LEVELS = 2;
    if ( g_reg_file.ddtp.iommu_mode == DDT_1LVL ) LEVELS = 1;
    i = LEVELS - 1;
    while ( i > 0 ) {
        read_memory((a + (DDI[i] * 8)), 8, (char *)&ddte.raw);
        if ( ddte.V == 0 ) {
            ddte.V = 1;
            ddte.PPN = get_free_ppn(1);
            write_memory((char *)&ddte.raw, (a + (DDI[i] * 8)), 8);
        }
        i = i - 1;
        a = ddte.PPN * PAGESIZE;
    }
    DC_SIZE = ( g_reg_file.capabilities.msi_flat == 1 ) ? EXT_FORMAT_DC_SIZE : BASE_FORMAT_DC_SIZE;
    write_memory((char *)DC, (a + (DDI[0] * DC_SIZE)), DC_SIZE);
    return 0;
}

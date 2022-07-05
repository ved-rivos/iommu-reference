// Copyright (c) 2022 by Rivos Inc.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
// Author: ved@rivosinc.com
#ifndef __IOMMU_H__
#define __IOMMU_H__
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "iommu_registers.h"
#include "iommu_data_structures.h"
#include "iommu_req_rsp.h"
#include "iommu_fault.h"
#include "iommu_translate.h"
#include "iommu_utils.h"
#include "iommu_interrupt.h"
#include "iommu_ref_api.h"
extern uint8_t 
locate_device_context(device_context_t *DC, uint8_t *DDI, uint32_t *cause);
extern int locate_process_context(process_context_t *PC, uint32_t device_id, uint32_t process_id);
extern uint8_t
s_vs_stage_address_translation(
    uint64_t iova,
    uint8_t priv, uint8_t is_read, uint8_t is_write, uint8_t is_exec,
    uint8_t SUM, iosatp_t iosatp, uint32_t PSCID, iohgatp_t iohgatp, 
    uint32_t *cause, uint64_t *iotval2, uint64_t *resp_pa, uint64_t *page_sz,
    uint8_t *R, uint8_t *W, uint8_t *X, uint8_t *G, uint8_t *PBMT, uint8_t *UNTRANSLATED_ONLY );
extern uint8_t
g_stage_address_translation(
    uint64_t gpa, uint8_t is_read, uint8_t is_write, uint8_t is_exec, uint8_t implicit,
    iohgatp_t iohgatp, uint32_t *cause, uint64_t *iotval2, 
    uint64_t *resp_pa, uint64_t *gst_page_sz,
    uint8_t *GR, uint8_t *GW, uint8_t *GX, uint8_t *GPBMT);
extern uint8_t
msi_address_translation(
    uint64_t iova, uint32_t msi_write_data, addr_type_t at, device_context_t *DC,
    uint32_t *cause, uint64_t *resp_pa, uint8_t *R, uint8_t *W, uint8_t *U, 
    uint8_t *is_msi, uint8_t *is_unsup, uint8_t *is_mrif_wr, uint32_t *mrif_nid);
extern void 
generate_interrupt(
    uint8_t unit);

#endif

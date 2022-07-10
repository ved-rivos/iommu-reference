// Copyright (c) 2022 by Rivos Inc.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
// Author: ved@rivosinc.com

#include <stdio.h>
#include <inttypes.h>
#include "iommu.h"

int
main(void) {
    capabilities_t cap = {0};
    fctrl_t fctrl;

    cap.Sv39 = cap.Sv48 = 1;
    cap.Sv39x4 = cap.Sv48x4 = 1;
    cap.amo = cap.ats = cap.t2gpa = cap.pmon = 1;
    cap.pas = 46;
    cap.version = 0x10;
    cap.Sv32 = 0;
    cap.Sv39 = 1;
    cap.Sv48 = 1;
    cap.Sv57 = 0;
    cap.rsvd0 = 0;
    cap.Svnapot = 0;
    cap.Svpbmt = 0;
    cap.Sv32x4 = 0;
    cap.Sv39x4 = 1;
    cap.Sv48x4 = 1;
    cap.Sv57x4 = 0;
    cap.rsvd1 = 0;
    cap.msi_flat = 1;
    cap.msi_mrif = 1;
    cap.amo = 1;
    cap.ats = 1;
    cap.t2gpa = 1;
    cap.end = 0;
    cap.igs = 0;
    cap.pmon = 1;
    cap.pas = 46;
    cap.rsvd3 = 0;
    cap.custom = 0;

    fctrl.raw = 0;
    fctrl.end = 0;
    fctrl.wis = 0;

    if ( reset_iommu(8, 40, 0xff, 4, Off, cap, fctrl) < 0 ) {
        printf("IOMMU reset failed\n");
    }
    cap.raw = read_register(CAPABILITIES_OFFSET, 8);
    printf("CAPABILITIES = %"PRIx64"\n", cap.raw);

    return 0;
}
uint8_t read_memory(uint64_t addr, uint8_t size, char *data){return 0;}
uint8_t read_memory_for_AMO(uint64_t address, uint8_t size, char *data) {return 0;}
uint8_t write_memory(char *data, uint64_t address, uint8_t size) {return 0;}
void iommu_to_hb_do_global_observability_sync(uint8_t PR, uint8_t PW){}
void send_msg_iommu_to_hb(ats_msg_t *prgr){}

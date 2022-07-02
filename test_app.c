#include <stdio.h>
#include <inttypes.h>
#include "iommu_registers.h"
#include "iommu_data_structures.h"

int
main(void) {
    capabilities_t cap = {0};
    fctrl_t fctrl;

    cap.Sv39 = cap.Sv48 = 1;
    cap.Sv39x4 = cap.Sv48x4 = 1;
    cap.amo = cap.ats = cap.t2gpa = cap.pmon = cap.ras = 1;
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
    cap.ras = 1;
    cap.pas = 46;
    cap.rsvd3 = 0;
    cap.custom = 0;

    fctrl.raw = 0;
    fctrl.end = 0;
    fctrl.wis = 0;

    if ( reset_iommu(8, 40, 16, 4, cap, fctrl) < 0 ) {
        printf("IOMMU reset failed\n");
    }
    cap.raw = read_register(CAPABILITIES_OFFSET, 8);
    printf("CAPABILITIES = %"PRIx64"\n", cap.raw);

    return 0;
}
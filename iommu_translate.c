#include "iommu_registers.h"
#include "iommu_data_structures.h"
#include "iommu_req_rsp.h"
#include "iommu_fault.h"

#define get_bits(__MS_BIT, __LS_BIT, __FIELD) ((__FIELD >> __LS_BIT) & ((1 << (((__MS_BIT - __LS_BIT) + 1))) - 1))
extern void stop_and_report_fault(char *cause_string, uint16_t cause, uint64_t iotval2, uint8_t dtf, 
                                  hb_to_iommu_req_t req, iommu_to_hb_rsp_msg_t *rsp_msg);
extern int locate_device_context(device_context_t *DC, uint32_t device_id);
extern int locate_process_context(process_context_t *PC, uint32_t device_id, uint32_t process_id);
extern int determine_msi_address_and_translate(hb_to_iommu_req_t *req, iommu_to_hb_rsp_msg_t *rsp);
extern int one_stage_address_translation(hb_to_iommu_req_t req, iosatp_t iosatp, uint32_t pscid, iohgatp_t iohgatp);
extern int two_stage_address_translation(hb_to_iommu_req_t req, iosatp_t iosatp, uint32_t pscid, iohgatp_t iohgatp);

void 
iommu_translate(
    hb_to_iommu_req_t req, iommu_to_hb_rsp_msg_t *rsp_msg) {
    uint8_t DDI[3];
    device_context_t DC;
    process_context_t PC;
    iosatp_t iosatp;
    uint32_t pscid;
    iohgatp_t iohgatp;

    // The process to translate an `IOVA` is as follows:
    // 1. If `ddtp.iommu_mode == Off` then stop and report "All inbound transactions
    //    disallowed" (cause = 256).
    if ( g_reg_file.ddtp.iommu_mode == Off )
        return stop_and_report_fault("All inbound transactions disallowed", 256, 0, 0, req, rsp_msg);

    // 2. If `ddtp.iommu_mode == Bare` and any of the following conditions hold then
    //    stop and report "Transaction type disallowed" (cause = 260).
    //    a. Transaction type is a Translated request (read, write/AMO, read-for-execute)
    //       or is a PCIe ATS Translation request.
    //    b. Transaction type is a PCIe "Page Request" Message.
    //    c. Transaction has a valid `process_id`
    //    d. Transaction type is not supported by the IOMMU in `Bare` mode.
    if ( g_reg_file.ddtp.iommu_mode == DDT_Bare ) {
        if ( req.tr.at == ADDR_TYPE_TRANSLATED || 
             req.tr.at == ADDR_TYPE_PCIE_ATS_TRANSLATION_REQUEST)
            return stop_and_report_fault("Transaction type disallowed", 260, 0, 0, req, rsp_msg);

        if ( req.pid_valid )
            return stop_and_report_fault("Transaction type disallowed", 260, 0, 0, req, rsp_msg);
    }
    // 3. If `capabilities.MSI_FLAT` is 0 then the IOMMU uses base-format device
    //    context. Let `DDI[0]` be `device_id[6:0]`, `DDI[1]` be `device_id[15:7]`, and
    //    `DDI[2]` be `device_id[23:16]`.
    if ( g_reg_file.capabilities.msi_flat == 0 ) {
        DDI[0] = get_bits(6,  0, req.device_id);
        DDI[1] = get_bits(15, 7, req.device_id);
        DDI[2] = get_bits(23, 16, req.device_id);
    }
    // 4. If `capabilities.MSI_FLAT` is 0 then the IOMMU uses extended-format device
    //    context. Let `DDI[0]` be `device_id[5:0]`, `DDI[1]` be `device_id[14:6]`, and
    //    `DDI[2]` be `device_id[23:15]`.
    if ( g_reg_file.capabilities.msi_flat == 0 ) {
        DDI[0] = get_bits(5,  0, req.device_id);
        DDI[1] = get_bits(14, 6, req.device_id);
        DDI[2] = get_bits(23, 15, req.device_id);
    }
    // 5. The `device_id` is wider than that supported by the IOMMU mode if any of the
    //    following conditions hold. If the following conditions hold then stop and
    //    report "Transaction type disallowed" (cause = 260).
    //    a. `ddtp.iommu_mode` is `2LVL` and `DDI[2]` is not 0
    //    b. `ddtp.iommu_mode` is `1LVL` and either `DDI[2]` is not 0 or `DDI[1]` is not 0
    if ( g_reg_file.ddtp.iommu_mode == DDT_2LVL && DDI[2] != 0 )
        return stop_and_report_fault("Transaction type disallowed", 260, 0, 0, req, rsp_msg);
        
    if ( g_reg_file.ddtp.iommu_mode == DDT_1LVL && (DDI[2] != 0 || DDI[1] != 0) )
        return stop_and_report_fault("Transaction type disallowed", 260, 0, 0, req, rsp_msg);


    // 6. Use `device_id` to then locate the device-context (`DC`) as specified in
    //    section 2.4.1 of IOMMU specification.
    if ( locate_device_context(&DC, req.device_id) )
        return;

    // 7. if any of the following conditions hold then stop and report
    //    "Transaction type disallowed" (cause = 260).
    //   * Transaction type is a Translated request (read, write/AMO, read-for-execute)
    //     or is a PCIe ATS Translation request and `DC.tc.EN_ATS` is 0.
    //   * Transaction type is a PCIe "Page Request" Message and `DC.tc.EN_PRI` is 0.
    //   * Transaction has a valid `process_id` and `DC.tc.PDTV` is 0.
    //   * Transaction has a valid `process_id` and `DC.tc.PDTV` is 1 and the
    //     `process_id` is wider than supported by `pdtp.MODE`.
    //   * Transaction type is not supported by the IOMMU.
    if ( DC.tc.EN_ATS == 0 && ( req.tr.at == ADDR_TYPE_TRANSLATED || 
                                req.tr.at == ADDR_TYPE_PCIE_ATS_TRANSLATION_REQUEST) )
        return stop_and_report_fault("Transaction type disallowed", 260, 0, 0, req, rsp_msg);
    if ( req.pid_valid && DC.tc.PDTV == 0 )
        return stop_and_report_fault("Transaction type disallowed", 260, 0, 0, req, rsp_msg);
    if ( req.pid_valid && DC.tc.PDTV == 1 ) {
        if ( DC.fsc.pdtp.MODE == PD17 && req.process_id > ((1 << 17) - 1) )
            return stop_and_report_fault("Transaction type disallowed", 260, 0, 0, req, rsp_msg);
        if ( DC.fsc.pdtp.MODE == PD8 && req.process_id > ((1 << 8) - 1) )
            return stop_and_report_fault("Transaction type disallowed", 260, 0, 0, req, rsp_msg);
    }

    // 8. If all of the following conditions hold then MSI address translations using
    //    MSI page tables is enabled and the transaction is eligible for MSI address
    //    translation and the MSI address translation process specified in section 2.4.3
    //    is invoked to determine if the `IOVA` is a MSI address and if so translate it.
    //    * `capabilities.MSI_FLAT` (Section 4.3) is 1, i.e., IOMMU support MSI address
    //       translation.
    //    * `IOVA` is a 32-bit aligned address.
    //    * Transaction is a Translated 32-bit write, Untranslated 32-bit write, or is
    //      an ATS translation request.
    //    * Transaction does not have a `process_id` (e.g., PASID present). Transactions
    //       with a `process_id` use a virtual address as IOVA and are not MSI.
    //    * `DC.msiptp.MODE != Bare` i.e., MSI address translation using MSI page tables
    //       is enabled.
    if ( (g_reg_file.capabilities.msi_flat == 1) &&
         ((req.tr.iova & 0x3) == 0) &&
         ((req.tr.at == ADDR_TYPE_PCIE_ATS_TRANSLATION_REQUEST) ||
          (req.tr.at == ADDR_TYPE_TRANSLATED && req.tr.length == 4) ||
          (req.tr.at == ADDR_TYPE_UNTRANSLATED && req.tr.length == 4)) &&
         (req.pid_valid == 0) &&
         (DC.msiptp.MODE != MSIPTP_Bare) ) { 
        if ( determine_msi_address_and_translate(&req, rsp_msg) )
            return;
        //    If the `IOVA` is determined to be not an MSI then the process continues at
        //    step 9.
    }

    // 9. If `DC.tc.pdtv` is set to 0 or if a `process_id` is not associated with the
    //    transaction then go to step 12 with the following page table information:
    //    * Let `iosatp.MODE` be value in `DC.fsc.MODE` field
    //    * Let `iosatp.PPN` be value in `DC.fsc.PPN` field
    //    * Let `PSCID` be value in `DC.ta.PSCID` field
    //    * Let `iohgatp` be value in `DC.iohgatp` field 
    //    * If a G-stage page table is not active in the device-context
    //      (`DC.iohgatp.mode` is `Bare`) then `iosatp` is a a S-stage page-table else
    //      it is a VS-stage page table.

    if ( (DC.tc.PDTV == 0) || (req.pid_valid == 0 ) ) {
        iosatp.MODE = DC.fsc.iosatp.MODE;
        iosatp.PPN = DC.fsc.iosatp.PPN;
        pscid = DC.ta.PSCID;
        iohgatp = DC.iohgatp;
        goto step_12;
    }

    // 10. Locate the process-context (`PC`) as specified in Section 2.4.2
    if ( locate_process_context(&PC, req.device_id, req.process_id) )
        return;

    // 11. Go to step 12 with the following page table information:
    //     * Let `iosatp.MODE` be value in `PC.fsc.MODE` field
    //     * Let `iosatp.PPN` be value in `PC.fsc.PPN` field
    //     * Let `PSCID` be value in `PC.ta.PSCID` field
    //     * Let `iohgatp` be value in `DC.iohgatp` field 
    //     * If a G-stage page table is not active in the device-context
    //       (`DC.iohgatp.mode` is `Bare`) then `iosatp` is a a S-stage page-table else
    //       it is a VS-stage page table.
    iosatp.MODE = PC.fsc.MODE;
    iosatp.PPN = PC.fsc.PPN;
    pscid = PC.ta.PSCID;
    iohgatp = DC.iohgatp;
    goto step_12;

step_12:
    // 12. If a G-stage page table is not active in the device-context then use the
    //     single stage address translation process specified in Section 4.3.2 of the
    //     RISC-V privileged specification. If a fault is detecting by the single stage
    //     address translation process then stop and report the fault.
    if ( iohgatp.MODE == HGATP_Bare ) {
        one_stage_address_translation(req, iosatp, pscid, iohgatp);
    } else {
    // 13. If a G-stage page table is active in the device-context then use the
    //     two-stage address translation process specified in Section 8.5 of the RISC-V
    //     privileged specification. If a fault is detecting by the single stage address
    //     translation process then stop and report the fault.
        two_stage_address_translation(req, iosatp, pscid, iohgatp);
    }
    return;
}

 

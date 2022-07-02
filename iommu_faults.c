#include "iommu_registers.h"
#include "iommu_data_structures.h"
#include "iommu_req_rsp.h"
#include "iommu_fault.h"

#define FAULT_QUEUE 0

extern void write_frec(fault_rec_t *frec, uint64_t frec_addr, uint8_t size, uint8_t *access_fault, uint8_t *data_corruption);
extern void generate_interrupt(uint8_t unit);
void 
stop_and_report_fault(
    char *cause_string, uint16_t cause, uint64_t iotval2, uint8_t dtf,
    hb_to_iommu_req_t req, iommu_to_hb_rsp_msg_t *rsp_msg) {
    fault_rec_t frec;
    uint32_t fqh;
    uint32_t fqt;
    uint64_t fqb;
    uint64_t frec_addr;
    uint8_t access_fault, data_corruption;

    if ( req.cmd == TRANS_REQUEST ) {
        if ( req.pid_valid ) {
            rsp_msg->pid_valid = 1;
            rsp_msg->process_id = req.process_id;
            rsp_msg->trsp.Priv = req.priv_req;
        } else {
            rsp_msg->pid_valid = 0;
            rsp_msg->trsp.Priv = 0;
        }
        // ATS translation requests that encounter a configuration error results in a
        // Completer Abort (CA) response to the requester. The following cause codes
        // belong to this category:
        // * Instruction access fault (cause = 1)
        // * Read access fault (cause = 5)
        // * Write/AMO access fault (cause = 7)
        // * MSI PTE load access fault (cause = 261)
        // * MSI PTE misconfigured (cause = 263)
        // * PDT entry load access fault (cause = 265)
        // * PDT entry misconfigured (cause = 267)
        if ( (cause == 1) || (cause == 5) || (cause == 7) || (cause == 261) || 
             (cause == 263) || (cause == 265) || (cause == 267) )
            rsp_msg->status = COMPLETER_ABORT;
        

        // If there is a permanent error or if ATS transactions are disabled then a
        // Unsupported Request (UR) response is generated. The following cause codes
        // belong to this category:
        // * All inbound transactions disallowed (cause = 256)
        // * DDT entry load access fault (cause = 257)
        // * DDT entry not valid (cause = 258)
        // * DDT entry misconfigured (cause = 259)
        // * Transaction type disallowed (cause = 260)
        if ( (cause == 256) || (cause == 257) || (cause == 258) || (cause == 259) || (cause == 260) ) 
            rsp_msg->status = UNSUPPORTED_REQUEST;

        // When translation could not be completed due to PDT entry being not present, MSI
        // PTE being not present, or first and/or second stage PTE being not present or
        // misconfigured then a Success Response with R and W bits set to 0 is generated.
        // The translated address returned with such completions is undefined. The
        // following cause codes belong to this category:
        // * Instruction page fault (cause = 12)
        // * Read page fault (cause = 13)
        // * Write/AMO page fault (cause = 15)
        // * Instruction guest page fault (cause = 20)
        // * Read guest-page fault (cause = 21)
        // * Write/AMO guest-page fault (cause = 23)
        // * PDT entry not valid (cause = 266)
        // * MSI PTE not valid (cause = 262)
        if ( req.tr.at == ADDR_TYPE_PCIE_ATS_TRANSLATION_REQUEST ) {
            rsp_msg->status = SUCCESS;
            rsp_msg->trsp.R = 0;
            rsp_msg->trsp.W = 0;

            // When a Success response is generated for a ATS translation request, the setting
            // of the Priv, N, CXL.io, and AMA fields is as follows:
            // * Priv field of the ATS translation completion is always set to 0 if the request
            //   does not have a PASID. When a PASID is present then the Priv field is set to
            //   the value in "Privilege Mode Requested" field as the permissions provided
            //   correspond to those the privilege mode indicate in the request.
            // * N field of the ATS translation completion is always set to 0. The device may
            //   use other means to determine if the No-snoop flag should be set in the
            //   translated requests.
            // * If requesting device is not a CXL device then CXL.io is set to 0.
            // * If requesting device is a CXL type 1 or type 2 device and the memory
            //   type, as determined by the Svpbmt extension, is NC or IO then the CXL.io
            //   bit is set to 1. If the memory type is PMA then the determination of the
            //   setting of this bit is `UNSPECIFIED`. If the Svpbmt extension is not
            //    supported then the setting of this bit is `UNSPECIFIED`.
            // * The AMA field is by default set to 000b. The IOMMU may support an
            //   implementation specific method to provide other encodings.
            rsp_msg->trsp.PBMT = 0;
            rsp_msg->trsp.CXL_IO = 0;
            rsp_msg->trsp.AMA = 0;
            rsp_msg->trsp.N = 0;
            rsp_msg->trsp.Exe = 0;
            rsp_msg->trsp.Global = 0;

            // No faults are logged in the fault queue for PCIe ATS Translation Requests.
            return;
        } else {
            // Translated and Untranslated requests
            rsp_msg->status = UNSUPPORTED_REQUEST;
        }
    }

    // The fault-queue enable bit enables the fault-queue when set to 1. 
    // The fault-queue is active if fqon reads 1. IOMMU behavior on
    // changing fqb when busy is 1 or fqon is 1 UNSPECIFIED. The
    // recommended sequence to change fqb is to first disable the fault
    // queue by clearing fqen and waiting for both busy and fqon to be 0
    // before changing fqb
    if ( g_reg_file.fqcsr.fqon == 0 || g_reg_file.fqcsr.fqen == 0 )
        return;

    // The fqmf bit is set to 1 if the IOMMU encounters an access fault
    // when storing a fault record to the fault queue. The fault-record that
    // was attempted to be written is discarded and no more fault records
    // are generated until software clears fqmf bit by writing 1 to the bit.
    // An interrupt is generated if enabled and not already pending (i.e.
    // ipsr.fip == 1) and not masked (i.e. fqsr.fie == 0).
    if ( g_reg_file.fqcsr.fqmf == 1 )
        return;

    // The fault-queue-overflow bit is set to 1 if the IOMMU needs to
    // queue a fault record but the fault-queue is full (i.e., fqh == fqt - 1)
    // The fault-record is discarded and no more fault records are
    // generated till software clears fqof by writing 1 to the bit. An
    // interrupt is generated if not already pending (i.e. ipsr.fip == 1)
    // and not masked (i.e. fqsr.fie == 0)
    if ( g_reg_file.fqcsr.fqof == 1 )
        return;

    // Setting the disable-translation-fault - DTF - bit to 1 disables reporting of 
    // faults encountered in the address translation process. Setting DTF to 1 does not 
    // disable error responses from being generated to the device in response to faulting 
    // transactions. Setting DTF to 1 does not disable reporting of faults from the IOMMU 
    // that are not related to the address translation process. The faults that are not
    // reported when DTF is 1 are listed in Table 8
    // |CAUSE | Description                         | Reported if `DTF` is 1?
    // |0     | Instruction address misaligned      | No
    // |1     | Instruction access fault            | No
    // |4     | Read address misaligned             | No
    // |5     | Read access fault                   | No
    // |6     | Write/AMO address misaligned        | No
    // |7     | Write/AMO access fault              | No
    // |12    | Instruction page fault              | No
    // |13    | Read page fault                     | No
    // |15    | Write/AMO page fault                | No
    // |20    | Instruction guest page fault        | No
    // |21    | Read guest-page fault               | No
    // |23    | Write/AMO guest-page fault          | No
    // |256   | All inbound transactions disallowed | Yes
    // |257   | DDT entry load access fault         | Yes
    // |258   | DDT entry not valid                 | Yes
    // |259   | DDT entry misconfigured             | Yes
    // |260   | Transaction type disallowed         | No
    // |261   | MSI PTE load access fault           | No
    // |262   | MSI PTE not valid                   | No
    // |263   | MSI PTE misconfigured               | No
    // |264   | MRIF access fault                   | No
    // |265   | PDT entry load access fault         | No
    // |266   | PDT entry not valid                 | No
    // |267   | PDT entry misconfigured             | No
    // |268   | DDT data corruption                 | No
    // |269   | PDT data corruption                 | No
    // |270   | MSI PT data corruption              | No
    // |271   | MSI MRIF data corruption            | No
    // |272   | Internal datapath error             | No
    // |273   | IOMMU MSI write access fault        | Yes
    if ( (dtf == 1) && (cause != 256) &&  (cause != 257) && 
         (cause != 258) && (cause != 259) && (cause != 273) ) {
        return;
    }
    if ( req.cmd == INVAL_COMPLETION ) {
        // unexpected - terminate
        *((char *)0) = 0;
    }
    // DID holds the device_id of the transaction. 
    // If PV is 0, then PID and PRIV are 0. If PV is 1, the PID
    // holds a process_id of the transaction and if the privilege 
    // of the transaction was Supervisor then PRIV bit is 1 else its 0. 
    frec.DID = req.device_id;
    if ( req.pid_valid ) {
        frec.PID = req.process_id;
        frec.PV = req.pid_valid;
        frec.PRIV = req.priv_req;
    } else {
        frec.PID = 0;
        frec.PV = 0;
        frec.PRIV = 0;
    }
    frec.reserved = 0;
    frec.custom = 0;

    if ( req.cmd == TRANS_REQUEST ) {
        // If the TTYP is a transaction with an IOVA then its reported in iotval. 
        frec.iotval = req.tr.iova;
        frec.iotval2 = iotval2;

        if ( req.tr.at == ADDR_TYPE_UNTRANSLATED && req.tr.read_writeAMO == READ ) {
            if ( req.pid_valid && req.exec_req ) {
                frec.TTYP = UNTRANSLATED_READ_FOR_EXECUTE_TRANSACTION;
            } else {
                frec.TTYP = UNTRANSLATED_READ_TRANSACTION;
            }
        }
        if ( req.tr.at == ADDR_TYPE_UNTRANSLATED && req.tr.read_writeAMO == WRITE_AMO )
            frec.TTYP = UNTRANSLATED_WRITE_AMO_TRANSACTION;

        if ( req.tr.at == ADDR_TYPE_TRANSLATED && req.tr.read_writeAMO == READ ) {
            if ( req.pid_valid && req.exec_req ) {
                frec.TTYP = TRANSLATED_READ_FOR_EXECUTE_TRANSACTION;
            } else {
                frec.TTYP = TRANSLATED_READ_TRANSACTION;
            }
        }

        if ( req.tr.at == ADDR_TYPE_TRANSLATED && req.tr.read_writeAMO == WRITE_AMO )
            frec.TTYP = TRANSLATED_WRITE_AMO_TRANSACTION;

        if ( req.tr.at == ADDR_TYPE_PCIE_ATS_TRANSLATION_REQUEST ) {
            frec.TTYP = PCIE_ATS_TRANSLATION_REQUEST;
        }
    }
    if ( req.cmd == PAGE_REQUEST ) {
        // If the TTYP is a message request then the message
        // code is reported in iotval.
        frec.iotval = PAGE_REQ_MSG_CODE;
        frec.iotval2 = 0;
    }
    // Fault/Event queue is an in-memory queue data structure used to report events
    // and faults raised when processing transactions. Each fault record is 32 bytes.
    // The PPN of the base of this in-memory queue and the size of the queue is 
    // configured into a memorymapped register called fault-queue base (fqb).
    // The tail of the fault-queue resides in a IOMMU controlled read-only 
    // memory-mapped register called fqt. The fqt is an index into the next fault 
    // record that IOMMU will write in the fault-queue.
    // Subsequent to writing the record, the IOMMU advances the fqt by 1. The head of
    // the fault-queue resides in a read/write memory-mapped software controlled 
    // register called fqh. The fqh is an index into the next fault record that SW 
    // should process next. Subsequent to processing fault record(s) software advances
    // the fqh by the count of the number of fault records processed. If fqh == fqt, the
    // fault-queue is empty. If fqt == (fqh - 1) the fault-queue is full.
    fqh = g_reg_file.fqh.index;
    fqt = g_reg_file.fqt.index;
    fqb = g_reg_file.fqb.ppn;
    if ( fqt == (fqh - 1) ) {
        g_reg_file.fqcsr.fqof = 1;
        generate_interrupt(FAULT_QUEUE);
        return;
    }
    // The IOMMU may be unable to report faults through the fault-queue due to error 
    // conditions such as the fault-queue being full or the IOMMU encountering access 
    // faults when attempting to access the queue memory. A memory-mapped fault control 
    // and status register (fqcsr) holds information about such faults. If the fault-queue
    // full condition is detected the IOMMU sets a fault-queue overflow (fqof)
    // bit in fqcsr. If the IOMMU encounters a fault in accessing the fault-queue memory, 
    // the IOMMU sets a fault-queue memory access fault (fqmf) bit in fqcsr. While either
    // error bits are set in fqcsr, the IOMMU discards the record that led to the fault
    // and all further fault records. When an error bit is in the fqcsr changes state 
    // from 0 to 1 or when a new fault record is produced in the fault-queue, fault
    // interrupt pending (fip) bit is set in the fqcsr.
    frec_addr = ((fqb * 4096) | (fqh * 32));
    write_frec(&frec, frec_addr, 32, &access_fault, &data_corruption);
    if ( access_fault || data_corruption ) {
        g_reg_file.fqcsr.fqmf = 1;
        generate_interrupt(FAULT_QUEUE);
        return;
    }
    fqh = (fqh + 1) & ((1 << (g_reg_file.cqb.log2szm1 + 1)) - 1);
    g_reg_file.fqh.index = fqh;
    generate_interrupt(FAULT_QUEUE);
    return;
}

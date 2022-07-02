typedef enum {
    // 00 - Untranslated  - IOMMU may treat the address as either virtual or physical.
    // 01 - Trans. Req.   - The IOMMU will return the translation of the address
    //                      contained in the address field of the request as a read
    //                      completion. 
    // 10 - Translated    - The address in the transaction has been translated by an IOMMU. 
    //                      If the Function associated with the device_id is allowed to 
    //                      present physical addresses to the system memory, then the IOMMU
    //                      might not translate this address. If the Function is not allowed
    //                      to present physical addresses, then the TA may treat this as an UR.
    ADDR_TYPE_UNTRANSLATED = 0,
    ADDR_TYPE_PCIE_ATS_TRANSLATION_REQUEST = 1,
    ADDR_TYPE_TRANSLATED = 2
} addr_type_t;
#define READ      0
#define WRITE_AMO 1
typedef struct {
    addr_type_t at;
    uint64_t    iova;
    uint32_t    length;
    uint32_t    read_writeAMO;
} iommu_trans_req_t;
typedef struct {
    uint32_t device_id;
    uint32_t pid_valid;
    uint32_t process_id;
    uint64_t payload;
} iommu_page_req_t;
typedef struct {
    uint64_t payload;
} iommu_inv_compl_t;
typedef enum {
    TRANS_REQUEST    = 0,
    INVAL_COMPLETION = 1,
    PAGE_REQUEST     = 2
} hb_to_iommu_cmd_t;
// Message Code Routing r[2:0] Type Description
// 00000100     000            Msg  Page Request Message, see § Section 10.4.1
#define PAGE_REQ_MSG_CODE 0x04

// Request to IOMMU from the host bridge
typedef struct {
    hb_to_iommu_cmd_t cmd;
    // Device ID input
    uint32_t  device_id;
    // Process ID input (e.g. PASID present)
    uint32_t  pid_valid;
    uint32_t  process_id;
    uint8_t   no_write;
    uint8_t   exec_req;
    uint8_t   priv_req;
    // Pick based on the command
    union {
        iommu_trans_req_t  tr;
        iommu_page_req_t   pr;
        iommu_inv_compl_t ic;
    };
} hb_to_iommu_req_t;

// Translation response from iommu to host bridge
typedef struct {
    uint64_t pa;
    uint8_t S;
    uint8_t N;
    uint8_t CXL_IO;
    uint8_t Global;
    uint8_t Priv;
    uint8_t U;
    uint8_t R;
    uint8_t W;
    uint8_t Exe;
    uint8_t AMA;
    uint8_t PBMT;
} iommu_trans_rsp_t;

typedef struct {
    uint32_t  device_id;
    uint32_t  pid_valid;
    uint32_t  process_id;
    uint8_t   no_write;
    uint8_t   exec_req;
    uint8_t   priv_req;
    uint64_t  payload;
} iommu_msg_req_t;

typedef enum {
    // Completion to a translation request
    TRANS_COMPLETION = 0,
    
    // PCIe Invalidation request
    INVAL_REQUEST = 1,

    // PCIe Page Group response
    PAGE_GROUP_RESPONSE = 2,
} iommu_to_hb_cmd_t;

typedef enum {
    // This Completion Status has a nominal meaning of “success”.
    SUCCESS = 0,

    // A status that applies to a posted or non-posted Request 
    // that specifies some action or access to some space that 
    // is not supported by the Completer. 
    // OR
    // A status indication returned with a Completion for a 
    // non-posted Request that suffered an Unsupported Request
    // at the Completer.
    UNSUPPORTED_REQUEST = 1,

    // A status that applies to a posted or non-posted Request 
    // that the Completer is permanently unable to complete 
    // successfully, due to a violation of the Completer’s 
    // programming model or to an unrecoverable error associated
    // with the Completer.
    // OR
    // A status indication returned with a Completion for a 
    // non-posted Request that suffered a Completer Abort at the
    // Completer.
    COMPLETER_ABORT = 4
} status_t;

// IOMMU response to requests from the IO bridge
// IOMMU generated notifications (invalidation requests and
// page group responses)
typedef struct {
    iommu_to_hb_cmd_t cmd;
    status_t  status;
    // Device ID input
    uint32_t  device_id;
    // Process ID input (e.g. PASID present)
    uint32_t  pid_valid;
    uint32_t  process_id;
    uint8_t   no_write;
    uint8_t   exec_req;
    uint8_t   priv_req;
    uint64_t  payload;
    // Pick based on the command
    union {
        iommu_trans_rsp_t trsp;
        uint64_t          prg_payload;
        uint64_t          inv_req_payload;
    };
} iommu_to_hb_rsp_msg_t;


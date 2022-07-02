// The TTYP field reports inbound transaction type
// Fault record `TTYP` field encodings
// |TTYP   | Description 
// |0      | None. Fault not caused by an inbound transaction.
// |1      | Untranslated read for execute transaction
// |2      | Untranslated read transaction
// |3      | Untranslated write/AMO transaction
// |4      | Translated read for execute transaction
// |5      | Translated read transaction
// |6      | Translated write/AMO transaction
// |7      | PCIe ATS Translation Request
// |8      | Message Request 
// |9 - 15 | Reserved
// |16 - 31| Reserved for custom use
#define TTYPE_NONE                                0
#define UNTRANSLATED_READ_FOR_EXECUTE_TRANSACTION 1
#define UNTRANSLATED_READ_TRANSACTION             2
#define UNTRANSLATED_WRITE_AMO_TRANSACTION        3
#define TRANSLATED_READ_FOR_EXECUTE_TRANSACTION   4
#define TRANSLATED_READ_TRANSACTION               5
#define TRANSLATED_WRITE_AMO_TRANSACTION          6
#define PCIE_ATS_TRANSLATION_REQUEST              7
#define MESSAGE_REQUEST                           8


// Fault-queue record
// bits:    23:0: 'DID'
// bits:   43:24: 'PID'
// bits:      44: 'PV'
// bits:      45: 'PRIV'
// bits:   51:46: 'TTYP'
// bits:   63:52: 'CAUSE'
// bits:   95:64: 'for custom use'
// bits:  127:96: 'reserved'
// bits: 191:128: 'iotval'
// bits: 255:192: 'iotval2'
typedef union {
    struct {
        uint64_t DID:24;
        uint64_t PID:20;
        uint64_t PV:1;
        uint64_t PRIV:1;
        uint64_t TTYP:5;
        uint64_t CAUSE:12;
        uint32_t custom;
        uint32_t reserved;
        uint64_t iotval;
        uint64_t iotval2;
    };
    uint64_t raw[4];
} fault_rec_t;

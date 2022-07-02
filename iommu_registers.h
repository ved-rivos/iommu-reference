#ifndef _IOMMU_REGS_H_
#define _IOMMU_REGS_H_
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
// The `capabilities` register is a read-only register reporting features supported
// by the IOMMU. Each field if not clear indicates presence of that feature in
// the IOMMU. At reset, the register shall contain the IOMMU supported features.
// Hypervisor may provide an SW emulated IOMMU to allow the guest to manage
// the VS-stage page tables for fine grained control on memory accessed by guest
// controlled devices. 
// A hypervisor that provides such an emulated IOMMU to the guest may retain
// control of the G-stage page tables and clear the `SvNx4` fields of the 
// emulated `capabilities` register.
// A hypervisor that provides such an emulated IOMMU to the guest may retain
// control of the MSI page tables used to direct MSI to guest interrupt files in
// an IMSIC or to a memory-resident-interrupt-file and clear the `MSI_FLAT` and 
// `MSI_MRIF` fields of the emulated `capabilities` register.
typedef union {
    struct {
        uint64_t version : 8;      // The `version` field holds the version of the
                                   // specification implemented by the IOMMU. The low
                                   // nibble is used to hold the minor version of the
                                   // specification and the upper nibble is used to
                                   // hold the major version of the specification.
                                   // For example, an implementation that supports
                                   // version 1.0 of the specification reports 0x10.
        uint64_t Sv32    : 1;      // Page-based 32-bit virtual addressing is supported
        uint64_t Sv39    : 1;      // Page-based 39-bit virtual addressing is supported
        uint64_t Sv48    : 1;      // Page-based 48-bit virtual addressing is supported
        uint64_t Sv57    : 1;      // Page-based 57-bit virtual addressing is supported
        uint64_t rsvd0   : 2;      // Reserved for standard use.
        uint64_t Svnapot : 1;      // NAPOT translation contiguity.
        uint64_t Svpbmt  : 1;      // Page-based memory types.
        uint64_t Sv32x4  : 1;      // Page-based 34-bit virtual addressing for G-stage
                                   // translation is supported
        uint64_t Sv39x4  : 1;      // Page-based 41-bit virtual addressing for G-stage
                                   // translation is supported
        uint64_t Sv48x4  : 1;      // Page-based 50-bit virtual addressing for G-stage
                                   // translation is supported
        uint64_t Sv57x4  : 1;      // Page-based 59-bit virtual addressing for G-stage
                                   // translation is supported
        uint64_t rsvd1   : 2;      // Reserved for standard use.
        uint64_t msi_flat: 1;      // MSI address translation using Write-through
                                   // mode MSI PTE is supported.
        uint64_t msi_mrif: 1;      // MSI address translation using MRIF mode MSI PTE
                                   // is supported.
        uint64_t amo     : 1;      // Atomic updates to MRIF and PTE accessed (A)
                                   // and dirty (D) bit is supported.
        uint64_t ats     : 1;      // PCIe Address Translation Services (ATS) and
                                   // page-request interface (PRI) is supported.
        uint64_t t2gpa   : 1;      // Returning guest-physical-address in ATS
                                   //  translation completions is supported.
        uint64_t end     : 1;      // When 0, IOMMU supports one endianness (either little
                                   // or big). When 1, IOMMU supports both endianness.
                                   // The endianness is defined in `fctrl` register.
        uint64_t igs     : 1;      // IOMMU interrupt generation support.
                                   // !Value  !Name      ! Description
                                   // !0      ! `MSI`    ! IOMMU supports only MSI
                                   //                      generation.
                                   // !1      ! `WIS`    ! IOMMU supports only wire
                                   //                      interrupt generation.
                                   // !2      ! `BOTH`   ! IOMMU supports both MSI
                                   //                      and wire interrupt generation.
                                   //                      The interrupt generation method
                                   //                      must be defined in `fctrl`
                                   //                      register.
                                   // !3      ! 0        ! Reserved for standard use
        uint64_t pmon    : 1;      // IOMMU implements a performance-monitoring unit
        uint64_t ras     : 1;      // IOMMU implements the RISC-V RAS regsiters
        uint64_t pas     : 6;      // Physical Address Size (value between 32 and 56)
        uint64_t rsvd3   : 19;     // Reserved for standard use
        uint64_t custom  : 16;     // _Reserved for custom use_
    };
    uint64_t raw;
} capabilities_t;
// This register must be readable in any implementation. An implementation may
// allow one or more fields in the register to be writable to support enabling
// or disabling the feature controlled by that field.
// If software enables or disables a feature when the IOMMU is not OFF
// (i.e. `ddtp.iommu_mode == Off`) then the IOMMU behavior is `UNSPECIFIED`.
typedef union {
    struct {
        uint32_t end     : 1;      // When 0, IOMMU accesses to memory resident data
                                   // structures (e.g. DDT, PDT, in-memory queues,
                                   // S/VS and G stage page tables) are performed as
                                   // little-endian accesses and when 1 as
                                   // big-endian accesses.
        uint32_t wis     : 1;      // When 1, IOMMU interrupts are signaled as
                                   // wired-interrupts.
        uint32_t reserved: 14;     // reserved for standard use.
        uint32_t custom  : 16;     //  _Reserved for custom use._
    };
    uint32_t raw;
} fctrl_t;
// The device-context is 64-bytes in size if `capabilities.MSI_FLAT` is 1 else it is
// 32-bytes.
// When the `iommu_mode` is `Bare` or `Off`, the `PPN` field is don't-care. When
// in `Bare` mode only Untranslated requests are allowed. Translated requests,
// Translation request, and message transactions are unsupported.
// All IOMMU must support `Off` and `Bare` mode. An IOMMU is allowed to support a
// subset of directory-table levels and device-context widths. At a minimum one
// of the modes must be supported.
// When the `iommu_mode` field value is changed the IOMMU guarantees that
// in-flight transactions from devices connected to the IOMMU will be processed
// with the configurations applicable to the old value of the `iommu_mode` field
// and that all transactions and previous requests from devices that have already
// been processed by the IOMMU be committed to a global ordering point such that
// they can be observed by all RISC-V hart, devices, and IOMMUs in the platform.
typedef union {
    struct {
        uint64_t ppn     : 44;     // Holds the `PPN` of the root page of the
                                   // device-directory-table.
        uint64_t reserved: 15;     // reserved for standard use.
        uint64_t busy    : 1;      // A write to `ddtp` may require the IOMMU to 
                                   // perform many operations that may not occur 
                                   // synchronously to the write. When a write is
                                   // observed by the `ddtp`, the `busy` bit is set
                                   // to 1. When the `busy` bit is 1, behavior of
                                   // additional writes to the `ddtp` is 
                                   // implementation defined. Some implementations
                                   // may ignore the second write and others may
                                   // perform the actions determined by the second
                                   // write. Software must verify that the `busy`
                                   // bit is 0 before writing to the `ddtp`.
    
                                   // If the `busy` bit reads 0 then the IOMMU has 
                                   // completed the operations associated with the 
                                   // previous write to `ddtp`.
    
                                   // An IOMMU that can complete these operations 
                                   // synchronously may hard-wire this bit to 0.
        uint64_t iommu_mode: 4;    // The IOMMU may be configured to be in following
                                   // modes:
                                   // !Value  !Name      ! Description
                                   // !0      ! `Off`    ! No inbound memory 
                                   //                      transactions are allowed
                                   //                      by the IOMMU.
                                   // !1      ! `Bare`   ! No translation or 
                                   //                      protection. All inbound
                                   //                      memory accesses are passed
                                   //                      through.
                                   // !2      ! `1LVL`   ! One-level
                                   //                      device-directory-table
                                   // !3      ! `2LVL`   ! Two-level
                                   //                      device-directory-table
                                   // !4      ! `3LVL`   ! Three-level
                                   //                      device-directory-table
    };
    uint64_t raw;
} ddtp_t;
// This 64-bits register (RW) holds the PPN of the root page of the command-queue
// and number of entries in the queue. Each command is 16 bytes.
typedef struct {
    uint64_t log2szm1: 5;      // The `LOG2SZ-1` field holds the number of
                               // entries in command-queue as a log to base 2
                               // minus 1. 
                               // A value of 0 indicates a queue of 2 entries.
                               // Each IOMMU command is 16-bytes. 
                               // If the command-queue has 256 or fewer entries
                               // then the base address of the queue is always 
                               // aligned to 4-KiB. If the command-queue has more
                               // than 256 entries then the command-queue 
                               // base address must be naturally aligned to
                               // `2^LOG2SZ^ x 16`. 
    uint64_t ppn     : 44;     // Holds the `PPN` of the root page of the
                               // in-memory command-queue used by software to
                               // queue commands to the IOMMU. 
    uint64_t reserved: 15;     // Reserved for standard use
} cqb_t;
// This 32-bits register (RO) holds the index into the command-queue where
// the IOMMU will fetch the next command.
typedef struct {
    uint32_t index;            // Holds the `index` into the command-queue from where
                               // the next command will be fetched next by the IOMMU.
} cqh_t;
// This 32-bits register (RO) holds the index into the command-queue where
// the IOMMU will fetch the next command.
typedef struct {
    uint32_t index;            // Holds the `index` into the command-queue where
                               // software queues the next command for IOMMU.  Only
                               // `LOG2SZ:0` bits are writable when the queue is 
                               // in enabled state (i.e., `cqsr.cqon == 1`). 
} cqt_t;
// This 64-bits register (RW) holds the PPN of the root page of the fault-queue
// and number of entries in the queue. Each fault record is 32 bytes.
typedef struct {
    uint64_t log2szm1: 5;      // The `LOG2SZ-1` field holds the number of
                               // entries in fault-queue as a log-to-base-2
                               // minus 1. A value of 0 indicates a queue of 2
                               // entries. Each fault record is 32-bytes. 
                               // If the fault-queue has 128 or fewer entries then
                               // the base address of the queue is always aligned 
                               // to 4-KiB. If the fault-queue has more than 128  
                               // entries then the fault-queue base address must  
                               // be naturally aligned to `2^LOG2SZ^ x 32`. 
    uint64_t ppn     : 44;     // Holds the `PPN` of the root page of the
                               // in-memory fault-queue used by IOMMU to queue
                               // fault record.
    uint64_t reserved: 15;     // Reserved for standard use
} fqb_t;

// This 32-bits register (RW) holds the index into fault-queue where the
// software will fetch the next fault record.
typedef struct {
    uint32_t index;            // Holds the `index` into the fault-queue from which
                               // software reads the next fault record.  Only
                               // `LOG2SZ:0` bits are writable when the queue is
                               // in enabled state (i.e., `fqsr.fqon == 1`).
} fqh_t;
// This 32-bits register (RO) holds the index into the fault-queue where the 
// IOMMU queues the next fault record.
typedef struct {
    uint32_t index;            // Holds the `index` into the fault-queue where IOMMU
                               // writes the next fault record.
} fqt_t;
// This 64-bits register (RW) holds the PPN of the root page of the
// page-request-queue and number of entries in the queue. Each page-request
// message is 16 bytes.
typedef struct {
    uint64_t log2szm1: 5;      // The `LOG2SZ-1` field holds the number of entries
                               // in page-request-queue as a log-to-base-2 minus 1.
                               // A value of 0 indicates a queue of 2 entries. 
                               // Each page-request is 16-bytes. If the 
                               // page-request-queue has 256 or fewer entries
                               // then the base address of the queue is always
                               // aligned to 4-KiB.
                               // If the page-request-queue has more than 256
                               // entries then the page-request-queue base address
                               // must be naturally aligned to `2^LOG2SZ^ x 16`.
    uint64_t ppn     : 44;     // Holds the `PPN` of the root page of the
                               // in-memory page-request-queue used by IOMMU to
                               // queue "Page Request" messages.
    uint64_t reserved: 15;     // Reserved for standard use
} pqb_t;
// This 32-bits register (RW) holds the index into the page-request-queue where
// software will fetch the next page-request.
typedef struct {
    uint32_t index;
} pqh_t;
// This 32-bits register (RW) holds the index into the page-request-queue where
// software will fetch the next page-request.
typedef struct {
    uint32_t index;
} pqt_t;
// This 32-bits register (RW) is used to control the operations and report the
// status of the command-queue.
typedef union {
    struct { 
        uint32_t cqen    :1;       // The command-queue-enable bit enables the command-
                                   // queue when set to 1. Changing `cqen` from 0 to 1
                                   // sets the `cqh` and `cqt` to 0. The command-queue
                                   // may take some time to be active following setting
                                   // the `cqen` to 1. When the command queue is active,
                                   // the `cqon` bit reads 1.
                                   // 
                                   // When `cqen` is changed from 1 to 0, the command 
                                   // queue may stay active till the commands already 
                                   // fetched from the command-queue are being processed 
                                   // and/or there are outstanding implicit loads from 
                                   // the command-queue.  When the command-queue turns 
                                   // off, the `cqon` bit reads 0, `cqh` is set to 0, 
                                   // `cqt` is set to 0 and the `cqcsr` bits `cmd_ill`, 
                                   // `cmd_to`, `cqmf`, `fence_w_ip` are set to 0.
                                   // 
                                   // When the `cqon` bit reads 0, the IOMMU guarantees 
                                   // that no implicit memory accesses to the command 
                                   // queue are in-flight and the command-queue will not 
                                   // generate new implicit loads to the queue memory.
        uint32_t cie     :1;       // Command-queue-interrupt-enable bit enables 
                                   // generation of interrupts from command-queue when 
                                   // set to 1.
        uint32_t rsvd0   :6;       // Reserved for standard use
        uint32_t cqmf    :1;       // If command-queue access leads to a memory fault then
                                   // the command-queue-memory-fault bit is set to 1 and 
                                   // the command-queue stalls until this bit is cleared. 
                                   // When `cqmf` is set to 1, an interrupt is generated 
                                   // if an interrupt is not already pending 
                                   // (i.e., `ipsr.cip == 1`) and not masked 
                                   // (i.e. `cqsr.cie == 0`). To re-enable command 
                                   // processing, software should clear this bit by 
                                   // writing 1. 
        uint32_t cmd_to  :1;       // If the execution of a command leads to a 
                                   // timeout (e.g. a command to invalidate device ATC 
                                   // may timeout waiting for a completion), then the 
                                   // command-queue sets the `cmd_to` bit and stops 
                                   // processing from the command-queue. When `cmd_to` is
                                   // set to 1 an interrupt is generated if an interrupt 
                                   // is not already pending (i.e., `ipsr.cip == 1`) and 
                                   // not masked (i.e. `cqsr.cie == 0`). To re-enable 
                                   // command processing software should clear this bit 
                                   // by writing 1. 
        uint32_t cmd_ill :1;       // If an illegal or unsupported command is fetched and
                                   // decoded by the command-queue then the command-queue 
                                   // sets the `cmd_ill` bit and stops processing from the
                                   // command-queue. When `cmd_ill` is set to 1, 
                                   // an interrupt is generated if not already pending 
                                   // (i.e. `ipsr.cip == 1`) and not masked 
                                   // (i.e.  `cqsr.cie == 0`). To re-enable command 
                                   // processing software should clear this bit by 
                                   // writing 1. 
        uint32_t fence_w_ip :1;    // An IOMMU that supports only wired interrupts sets 
                                   // `fence_w_ip` bit is set to indicate completion of a 
                                   // `IOFENCE.C` command. An interrupt on setting 
                                   // `fence_w_ip` if not already pending 
                                   // (i.e. `ipsr.cip == 1`) and not masked 
                                   // (i.e. `cqsr.cie == 0`) and `fence_w_ip` is 0. 
                                   // To re-enable interrupts on `IOFENCE.C` completion
                                   // software should clear this bit by writing 1.
                                   // This bit is reserved if the IOMMU uses MSI. 
        uint32_t rsvd1   :4;       // Reserved for standard use
        uint32_t cqon    :1;       // The command-queue is active if `cqon` is 1.
                                   // IOMMU behavior on changing cqb when busy is 1 or 
                                   // `cqon` is 1 is implementation defined. The software 
                                   // recommended sequence to change `cqb` is to first 
                                   // disable the command-queue by clearing cqen and 
                                   // waiting for both `busy` and `cqon` to be 0 before 
                                   // changing the `cqb`.
        uint32_t busy    :1;       // A write to `cqcsr` may require the IOMMU to perform
                                   // many operations that may not occur synchronously 
                                   // to the write. When a write is observed by the 
                                   // `cqcsr`, the `busy` bit is set to 1.
                                   //
                                   // When the `busy` bit is 1, behavior of additional 
                                   // writes to the `cqcsr` is implementation defined. 
                                   // Some implementations may ignore the second write and
                                   // others may perform the actions determined by the 
                                   // second write.
                                   //
                                   // Software must verify that the busy bit is 0 before 
                                   // writing to the `cqcsr`. An IOMMU that can complete 
                                   // controls synchronously may hard-wire this bit to 0.
                                   //
                                   // An IOMMU that can complete these operations 
                                   // synchronously may hard-wire this bit to 0.
        uint32_t rsvd2   :10;      // Reserved for standard use
        uint32_t custom  :4;       // _Reserved for custom use._
    };
    uint32_t raw;
} cqcsr_t;
typedef union {
    struct {
        uint32_t fqen    :1;
        uint32_t fie     :1;
        uint32_t rsvd0   :6;
        uint32_t fqmf    :1;
        uint32_t fqof    :1;
        uint32_t rsvd1   :6;
        uint32_t fqon    :1;
        uint32_t busy    :1;
        uint32_t rsvd2   :10;
        uint32_t custom  :4;
    };
    uint32_t raw;
} fqcsr_t;
typedef union {
    struct {
        uint32_t pqen    :1;
        uint32_t pie     :1;
        uint32_t rsvd0   :6;
        uint32_t pqmf    :1;
        uint32_t pqof    :1;
        uint32_t rsvd1   :6;
        uint32_t pqon    :1;
        uint32_t busy    :1;
        uint32_t rsvd2   :10;
        uint32_t custom  :4;
    };
    uint32_t raw;
} pqcsr_t;
typedef union {
    struct {
        uint32_t cip     :1;
        uint32_t fip     :1;
        uint32_t pmip    :1;
        uint32_t pip     :1;
        uint32_t rsvd0   :4;
        uint32_t custom  :8;
        uint32_t rsvd1   :16;
    };
    uint32_t raw;
} ipsr_t;
typedef struct {
    uint32_t cy      :1;
    uint32_t hpm     :31;
} iocountovf_t;
typedef struct {
    uint32_t cy      :1;
    uint32_t hpm     :31;
} iocountinh_t;
typedef struct {
    uint64_t counter :63;
    uint64_t of      :1;
} iohpmcycles_t;
typedef struct {
    uint64_t counter :64;
} iohpmctr_t;
typedef struct {
    uint64_t eventID :15;
    uint64_t dmask   :1;
    uint64_t pid_pscid :20;
    uint64_t did_gscid :24;
    uint64_t pv_pscv :1;
    uint64_t dv_gscv :1;
    uint64_t idt     :1;
    uint64_t of      :1;
} iohpmevt_t;
typedef struct {
    uint64_t civ     :4;
    uint64_t fiv     :4;
    uint64_t pmiv    :4;
    uint64_t piv     :4;
    uint64_t reserved:16;
    uint64_t custom  :32;
} icvec_t;
typedef struct {
    uint64_t msi_addr;
    uint32_t msi_data;
    uint32_t msi_vec_ctrl;
} msi_cfg_tbl_t;
// The IOMMU provides a memory-mapped programming interface. The memory-mapped
// registers of each IOMMU are located within a naturally aligned 4-KiB region
// (a page) of physical address space.
// The IOMMU behavior for register accesses where the address is not aligned to
// the size of the access or if the access spans multiple registers is undefined.
// IOMMU Memory-mapped register layout
typedef union {                        // |Ofst|Name            |Size|Description
    struct {
        capabilities_t capabilities;   // |0   |`capabilities`  |8   |Capabilities supported by the IOMMU
        fctrl_t        fctrl;          // |8   |`fctrl`         |4   |Features control>>
        uint32_t       custom0;        // |12  |_custom_        |4   |For custom use_
        ddtp_t         ddtp;           // |16  |`ddtp`          |8   |Device directory table pointer
        cqb_t          cqb;            // |24  |`cqb`           |8   |Command-queue base
        cqh_t          cqh;            // |32  |`cqh`           |4   |Command-queue head
        cqt_t          cqt;            // |36  |`cqt`           |4   |Command-queue tail
        fqb_t          fqb;            // |40  |`fqb`           |8   |Fault-queue base
        fqh_t          fqh;            // |48  |`fqh`           |4   |Fault-queue head
        fqt_t          fqt;            // |52  |`fqt`           |4   |Fault-queue tail
        pqb_t          pqb;            // |56  |`pqb`           |8   |Page-request-queue base
        pqh_t          pqh;            // |64  |`pqh`           |4   |Page-request-queue head
        pqt_t          pqt;            // |68  |`pqt`           |4   |Page-request-queue tail
        cqcsr_t        cqcsr;          // |72  |`cqcsr`         |4   |Command-queue control and status register
        fqcsr_t        fqcsr;          // |76  |`fqcsr`         |4   |Fault-queue control and status register
        pqcsr_t        pqcsr;          // |80  |`pqcsr`         |4   |Page-req-queue control and status register
        ipsr_t         ipsr;           // |84  |`ipsr`          |4   |Interrupt pending status register
        iocountovf_t   iocountovf;     // |88  |`iocntovf`      |4   |Perf-monitoring counter overflow status
        iocountinh_t   iocountinh;     // |92  |`iocntinh`      |4   |Performance-monitoring counter inhibits
        iohpmcycles_t  iohpmcycles;    // |96  |`iohpmcycles`   |8   |Performance-monitoring cycles counter
        iohpmctr_t     iohpmctr[31];   // |104 |`iohpmctr1 - 31`|248 |Performance-monitoring event counters
        iohpmevt_t     iohpmevt[31];   // |352 |`iohpmevt1 - 31`|248 |Performance-monitoring event selector
        uint8_t        reserved0[82];  // |600 |Reserved        |82  |Reserved for future use (`WPRI`)
        uint8_t        custom1[78];    // |682 |_custom_        |78  |Reserved for custom use (`WARL`)_
        icvec_t        icvec;          // |760 |`icvec`         |4   |Interrupt cause to vector register
        msi_cfg_tbl_t  msi_cfg_tbl[16];// |768 |`msi_cfg_tbl`   |256 |MSI Configuration Table
        uint8_t        reserved1[3072];// |1024|Reserved        |3072|Reserved for future use (`WPRI`)
    };
    uint8_t        regs[4096];
} iommu_regs_t;

// Offset to field
#define CAPABILITIES_OFFSET  0
#define FCTRL_OFFSET         8
#define DDTP_OFFSET          16
#define CQB_OFFSET           24
#define CQH_OFFSET           32
#define CQT_OFFSET           36
#define FQB_OFFSET           40
#define FQH_OFFSET           48
#define FQT_OFFSET           52
#define PQB_OFFSET           56
#define PQH_OFFSET           64
#define PQT_OFFSET           68
#define CQCSR_OFFSET         72
#define FQCSR_OFFSET         76
#define PQCSR_OFFSET         80
#define IPSR_OFFSET          84
#define IOCNTOVF_OFFSET      88
#define IOCNTINH_OFFSET      92
#define IOHPMCYCLES_OFFSET   96
#define IOHPMCTR1_OFFSET     104
#define IOHPMEVT1_OFFSET     352
#define ICVEC_OFFSET         760
#define MSI_CFG_TBL_OFFSET   768

// capabilities fields
#define MSI      0
#define WIS      1
#define IGS_BOTH 2
#define ONE_END   0
#define BOTH_END  1

// ddtp defines
#define Off      0
#define DDT_Bare 1
#define DDT_1LVL 2
#define DDT_2LVL 3
#define DDT_3LVL 4

extern iommu_regs_t g_reg_types;
extern iommu_regs_t g_reg_file;
extern uint8_t offset_to_size[4096];
extern int reset_iommu(uint8_t num_hpm, uint8_t hpmctr_bits, uint16_t num_evts, 
                uint8_t num_vec, capabilities_t capabilities, fctrl_t fctrl);
extern uint64_t read_register(uint16_t offset, uint8_t num_bytes);
extern void write_register(uint16_t offset, uint8_t num_bytes, uint64_t data);

#endif //_IOMMU_REGS_H_

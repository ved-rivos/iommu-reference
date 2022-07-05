// Copyright (c) 2022 by Rivos Inc.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
// Author: ved@rivosinc.com
#ifndef __IOMMU_TRANSLATE_H__
#define __IOMMU_TRANSLATE_H__

#define PAGESIZE 4096
#define U_MODE   0
#define S_MODE   1
#define PMA      0
#define NC       1
#define IO       2
typedef union {
    struct {
        uint64_t V:1;
        uint64_t R:1;
        uint64_t W:1;
        uint64_t X:1;
        uint64_t U:1;
        uint64_t G:1;
        uint64_t A:1;
        uint64_t D:1;
        uint64_t RSW:2;
        uint64_t PPN:44;
        uint64_t reserved:7;
        uint64_t PBMT:2;
        uint64_t N:1;
    };
    uint64_t raw;
} pte_t;
typedef union {
    struct {
        uint64_t V:1;
        uint64_t R:1;
        uint64_t W:1;
        uint64_t X:1;
        uint64_t U:1;
        uint64_t G:1;
        uint64_t A:1;
        uint64_t D:1;
        uint64_t RSW:2;
        uint64_t PPN:44;
        uint64_t reserved0:7;
        uint64_t PBMT:2;
        uint64_t reserved1:1;
    };
    uint64_t raw;
} gpte_t;
typedef union {
    struct {
        uint64_t V:1;
        uint64_t reserved0:1;
        uint64_t W:1;
        uint64_t other:60;
        uint64_t C:1;
        uint64_t upperQW:64;
    };
    struct {
        uint64_t V:1;
        uint64_t reserved0:1;
        uint64_t W:1;
        uint64_t PPN:44;
        uint64_t reserved1:9;
        uint64_t C:1;
        uint64_t ignored;
    } write_through;
    struct {
        uint64_t V:1;
        uint64_t reserved0:1;
        uint64_t W:1;
        uint64_t reserved1:4;
        uint64_t MRIF_ADDR:47;
        uint64_t reserved2:9;
        uint64_t C:1;
        uint64_t N90:10;
        uint64_t NPPN:44;
        uint64_t reserved3:6;
        uint64_t N10:1;
        uint64_t reserved4:3;
    } mrif;
    uint64_t raw[2];
} msipte_t;

#endif // __IOMMU_TRANSLATE_H__

// Copyright (c) 2022 by Rivos Inc.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
// Author: ved@rivosinc.com
#ifndef __IOMMU_UTILS_H__
#define __IOMMU_UTILS_H__
#define get_bits(__MS_BIT, __LS_BIT, __FIELD)\
    ((__FIELD >> __LS_BIT) & ((1 << (((__MS_BIT - __LS_BIT) + 1))) - 1))
#endif // __IOMMU_UTILS_H__

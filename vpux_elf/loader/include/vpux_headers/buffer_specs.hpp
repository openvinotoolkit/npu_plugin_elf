//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <vpux_elf/types/data_types.hpp>

namespace elf {

struct BufferSpecs {
public:
    uint64_t alignment;
    uint64_t size;
    Elf_Xword procFlags;
    BufferSpecs(): alignment(0), size(0), procFlags(0) {
    }
    BufferSpecs(uint64_t alignment, uint64_t size, uint64_t procFlags)
            : alignment(alignment), size(size), procFlags(procFlags) {
    }
};

}  // namespace elf

//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once
#include <stddef.h>

namespace elf {
namespace utils {

bool checkELFMagic(const unsigned char *elfIdent);

size_t alignUp(size_t size, size_t alignment);

} // namespace utils
} // namespace elf

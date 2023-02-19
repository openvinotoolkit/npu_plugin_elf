//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <vpux_elf/types/data_types.hpp>

namespace elf {

///
/// Refer to https://docs.oracle.com/cd/E19683-01/816-1386/chapter6-83432/index.html
/// for the detailed description of the values and structures below
///

//! Segment types
constexpr Elf_Word PT_NULL      = 0;
constexpr Elf_Word PT_LOAD      = 1;
constexpr Elf_Word PT_DYNAMIC   = 2;
constexpr Elf_Word PT_INTERP    = 3;
constexpr Elf_Word PT_NOTE      = 4;
constexpr Elf_Word PT_SHLIB     = 5;
constexpr Elf_Word PT_PHDR      = 6;
constexpr Elf_Word PT_LOSUNW    = 0x6ffffffa;
constexpr Elf_Word PT_SUNWBSS   = 0x6ffffffb;
constexpr Elf_Word PT_SUNWSTACK = 0x6ffffffa;
constexpr Elf_Word PT_HISUNW    = 0x6fffffff;
constexpr Elf_Word PT_LOPROC    = 0x70000000;
constexpr Elf_Word PT_HIPROC    = 0x7fffffff;

//! Segment permission flags
constexpr Elf_Word PF_X        = 0x1;
constexpr Elf_Word PF_W        = 0x2;
constexpr Elf_Word PF_R        = 0x4;
constexpr Elf_Word PF_MASKPROC = 0xf0000000;

struct Elf64_Phdr {
    Elf_Word   p_type;
    Elf_Word   p_flags;
    Elf64_Off  p_offset;
    Elf64_Addr p_vaddr;
    Elf64_Addr p_paddr;
    Elf_Xword  p_filesz;
    Elf_Xword  p_memsz;
    Elf_Xword  p_align;
};

struct Elf32_Phdr {
    Elf_Word   p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf_Word   p_filesz;
    Elf_Word   p_memsz;
    Elf_Word   p_flags;
    Elf_Word   p_align;
};

using ProgramHeader = Elf64_Phdr;

} // namespace elf

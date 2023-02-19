//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <vpux_elf/types/data_types.hpp>

namespace elf {

///
/// Refer to https://docs.oracle.com/cd/E19455-01/806-3773/elf-2/index.html
/// for the detailed description of the values and structures below
///

//! Section types
constexpr Elf_Word SHT_NULL          = 0;
constexpr Elf_Word SHT_PROGBITS      = 1;
constexpr Elf_Word SHT_SYMTAB        = 2;
constexpr Elf_Word SHT_STRTAB        = 3;
constexpr Elf_Word SHT_RELA          = 4;
constexpr Elf_Word SHT_HASH          = 5;
constexpr Elf_Word SHT_DYNAMIC       = 6;
constexpr Elf_Word SHT_NOTE          = 7;
constexpr Elf_Word SHT_NOBITS        = 8;
constexpr Elf_Word SHT_REL           = 9;
constexpr Elf_Word SHT_SHLIB         = 10;
constexpr Elf_Word SHT_DYNSYM        = 11;
constexpr Elf_Word SHT_LOPROC        = 0x70000000;
constexpr Elf_Word SHT_HIPROC        = 0x7fffffff;
constexpr Elf_Word SHT_LOUSER        = 0x80000000;
constexpr Elf_Word SHT_HIUSER        = 0xffffffff;

//! Section flags
constexpr Elf_Xword SHF_NONE            = 0x0;
constexpr Elf_Xword SHF_WRITE           = 0x1;
constexpr Elf_Xword SHF_ALLOC           = 0x2;
constexpr Elf_Xword SHF_EXECINSTR       = 0x4;
constexpr Elf_Xword SHF_INFO_LINK       = 0x40;
constexpr Elf_Xword SHF_MASKOS          = 0xff00000;
constexpr Elf_Xword SHF_MASKPROC        = 0xf0000000;

//! Special section indexes
constexpr Elf_Word SHN_UNDEF     = 0;
constexpr Elf_Word SHN_LORESERVE = 0xff00;
constexpr Elf_Word SHN_LOPROC    = 0xff00;
constexpr Elf_Word SHN_HIPROC    = 0xff1f;
constexpr Elf_Word SHN_LOOS      = 0xff20;
constexpr Elf_Word SHN_HIOS      = 0xff3f;
constexpr Elf_Word SHN_ABS       = 0xfff1;
constexpr Elf_Word SHN_COMMON    = 0xfff2;
constexpr Elf_Word SHN_XINDEX    = 0xffff;
constexpr Elf_Word SHN_HIRESERVE = 0xffff;

struct Elf64_Shdr {
    Elf_Word   sh_name;
    Elf_Word   sh_type;
    Elf_Xword  sh_flags;
    Elf64_Addr sh_addr;
    Elf64_Off  sh_offset;
    Elf_Xword  sh_size;
    Elf_Word   sh_link;
    Elf_Word   sh_info;
    Elf_Xword  sh_addralign;
    Elf_Xword  sh_entsize;
};

struct Elf32_Shdr {
    Elf_Word   sh_name;
    Elf_Word   sh_type;
    Elf_Word   sh_flags;
    Elf32_Addr sh_addr;
    Elf32_Off  sh_offset;
    Elf_Word   sh_size;
    Elf_Word   sh_link;
    Elf_Word   sh_info;
    Elf_Word   sh_addralign;
    Elf_Word   sh_entsize;
};

using SectionHeader = Elf64_Shdr;

} // namespace elf

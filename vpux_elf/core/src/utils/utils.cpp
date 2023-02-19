//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#include <vpux_elf/utils/utils.hpp>

#include <vpux_elf/types/elf_header.hpp>

#include <vpux_elf/utils/error.hpp>

using namespace elf;
//
// @breif returning true if the magic is correct
// @params pointer to the elf blob memory
//
bool utils::checkELFMagic(const unsigned char *elfIdent) {
    VPUX_ELF_THROW_UNLESS(elfIdent, ArgsError, "nullptr passed for elf buffer");


    if (elfIdent[elf::EI_MAG0] != elf::ELFMAG0 ||
        elfIdent[elf::EI_MAG1] != elf::ELFMAG1 ||
        elfIdent[elf::EI_MAG2] != elf::ELFMAG2 ||
        elfIdent[elf::EI_MAG3] != elf::ELFMAG3) {
        // if the elf magic is not correct return false
        return false;
    }
    return true;
}

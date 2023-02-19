//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#include <vpux_elf/writer/section.hpp>

using namespace elf;
using namespace elf::writer;

Section::Section(const std::string& name) : m_name(name) {
    m_header.sh_name = 0;
    m_header.sh_type = SHT_NULL;
    m_header.sh_flags = 0;
    m_header.sh_addr = 0;
    m_header.sh_offset = 0;
    m_header.sh_size = 0;
    m_header.sh_link = SHN_UNDEF;
    m_header.sh_info = 0;
    m_header.sh_addralign = 4;
    m_header.sh_entsize = 0;
}

std::string Section::getName() const {
    return m_name;
}

void Section::setName(const std::string& name) {
    m_name = name;
}

Elf_Xword Section::getAddrAlign() const {
    return m_header.sh_addralign;
}

void Section::setAddrAlign(Elf_Xword addrAlign) {
    m_header.sh_addralign = addrAlign;
}

Elf64_Addr Section::getAddr() const {
    return m_header.sh_addr;
}

void Section::setAddr(Elf64_Addr addr) {
    m_header.sh_addr = addr;
}

Elf_Xword Section::getFlags() const {
    return m_header.sh_flags;
}

void Section::setFlags(Elf_Xword flags) {
    m_header.sh_flags = flags;
}

void Section::maskFlags(Elf_Xword flags) {
    m_header.sh_flags |= flags;
}

Elf_Word Section::getType() const {
    return m_header.sh_type;
}

void Section::setType(Elf_Word type) {
    m_header.sh_type = type;
}

size_t Section::getFileAlignRequirement() const {
    return m_fileAlignRequirement;
}

void Section::finalize() {}

void Section::setIndex(size_t index) {
    m_index = index;
}

void Section::setNameOffset(size_t offset) {
    m_header.sh_name = static_cast<Elf_Word>(offset);
}

size_t Section::getIndex() const {
    return m_index;
}

size_t Section::getDataSize() const {
    return m_data.size();
}

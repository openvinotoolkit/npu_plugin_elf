//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#include <vpux_elf/writer/segment.hpp>
#include <vpux_elf/utils/error.hpp>

using namespace elf;
using namespace elf::writer;

Segment::Segment() {
    m_header.p_type = PT_NULL;
    m_header.p_flags = 0;
    m_header.p_offset = 0;
    m_header.p_vaddr = 0;
    m_header.p_paddr = 0;
    m_header.p_filesz = 0;
    m_header.p_memsz = 0;
    m_header.p_align = 0;
}

void Segment::addSection(Section* section) {
    VPUX_ELF_THROW_UNLESS(section->getFileAlignRequirement() == 1, SectionError,
                          "Adding section with file align requirement different then 1 is not supported");
    m_sections.push_back(section);
}

void Segment::setType(Elf_Word type) {
    m_header.p_type = type;
}

void Segment::setAlign(Elf_Xword align) {
    m_header.p_align = align;
}

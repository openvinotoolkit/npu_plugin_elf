//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <vpux_elf/writer/section.hpp>
#include <vpux_elf/writer/segment.hpp>

#include <vpux_elf/writer/relocation_section.hpp>
#include <vpux_elf/writer/symbol_section.hpp>
#include <vpux_elf/writer/binary_data_section.hpp>
#include <vpux_elf/writer/string_section.hpp>
#include <vpux_elf/writer/empty_section.hpp>

#include <vpux_elf/types/data_types.hpp>
#include <vpux_elf/types/section_header.hpp>
#include <vpux_elf/types/program_header.hpp>
#include <vpux_elf/types/elf_header.hpp>

#include <string>
#include <vector>

namespace elf {

class Writer {
public:
    Writer();

    std::vector<uint8_t> generateELF();

    writer::Segment* addSegment();

    writer::RelocationSection* addRelocationSection(const std::string& name = {});
    writer::SymbolSection* addSymbolSection(const std::string& name = {});
    writer::EmptySection* addEmptySection(const std::string& name = {});

    template <typename T>
    writer::BinaryDataSection<T>* addBinaryDataSection(const std::string& name = {}, const Elf_Word section_type = SHT_PROGBITS) {
        m_sections.push_back(std::unique_ptr<writer::BinaryDataSection<T>>(new writer::BinaryDataSection<T>(name, section_type)));
        m_sections.back()->setIndex(m_sections.size() - 1);
        return dynamic_cast<writer::BinaryDataSection<T>*>(m_sections.back().get());
    }

private:
    writer::Section* addSection(const std::string& name = {});
    writer::StringSection* addStringSection(const std::string& name = {});

    elf::ELFHeader generateELFHeader() const;

private:
    writer::StringSection* m_sectionHeaderNames;
    writer::StringSection* m_symbolNames;
    std::vector<std::unique_ptr<writer::Section>> m_sections;
    std::vector<std::unique_ptr<writer::Segment>> m_segments;
};

} // namespace elf

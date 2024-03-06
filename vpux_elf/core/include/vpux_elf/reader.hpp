//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <vpux_elf/types/data_types.hpp>
#include <vpux_elf/types/section_header.hpp>
#include <vpux_elf/types/program_header.hpp>
#include <vpux_elf/types/elf_header.hpp>
#include <vpux_elf/types/elf_structs.hpp>
#include <vpux_elf/utils/error.hpp>
#include <vpux_elf/utils/utils.hpp>
#include <vpux_elf/types/vpu_extensions.hpp>

#include <vpux_elf/accessor.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>

namespace elf {

template<ELF_Bitness B>
class Reader {
public:
    class Section {
    public:
        Section() = default;
        Section(AccessManager* accessor, const typename ElfTypes<B>::SectionHeader* sectionHeader, const char* name, const uint8_t* data = nullptr)
                : m_accessor(accessor), m_header(sectionHeader), m_name(name), m_data(data) {}

        const typename ElfTypes<B>::SectionHeader* getHeader() const {
            return m_header;
        }

        size_t getEntriesNum() const {
            VPUX_ELF_THROW_UNLESS(m_header->sh_entsize, SectionError,
                                    "sh_entsize=0 represents a section that does not hold a table of fixed-size entries. This feature is not suported.")
            return static_cast<size_t>(m_header->sh_size / m_header->sh_entsize);
        }

        const char* getName() const {
            return m_name;
        }

        template<typename T>
        const T* getData() const {
            if (m_data == nullptr) {
                m_data = m_accessor->read(AccessorDescriptor{m_header->sh_offset, m_header->sh_size, m_header->sh_flags, m_header->sh_addralign});
            }
            return reinterpret_cast<const T*>(m_data);
        }

    private:
        AccessManager* m_accessor = nullptr;
        const typename ElfTypes<B>::SectionHeader* m_header = nullptr;
        const char* m_name = nullptr;
        mutable const uint8_t* m_data = nullptr;
    };

    class Segment {
    public:
        Segment(const typename ElfTypes<B>::ProgramHeader* programHeader, const uint8_t* data)
                : m_programHeader(programHeader), m_data(data) {}

        const typename ElfTypes<B>::ProgramHeader* getHeader() const {
            return m_programHeader;
        }

        const uint8_t* getData() const {
            return m_data;
        }

    private:
        const typename ElfTypes<B>::ProgramHeader* m_programHeader = nullptr;
        const uint8_t* m_data = nullptr;
    };

public:
    Reader(AccessManager* accessor)
            : m_accessor(accessor) {
        VPUX_ELF_THROW_UNLESS(m_accessor, ArgsError, "Accessor pointer is null");

        m_elfHeader = reinterpret_cast<const typename ElfTypes<B>::ELFHeader*>(
            m_accessor->read(AccessorDescriptor{0, sizeof(typename ElfTypes<B>::ELFHeader)}));

        VPUX_ELF_THROW_UNLESS(utils::checkELFMagic(reinterpret_cast<const uint8_t*>(m_elfHeader)), HeaderError, "Incorrect ELF magic");

        m_sectionHeadersStart = reinterpret_cast<const typename ElfTypes<B>::SectionHeader*>(
            m_accessor->read(AccessorDescriptor{m_elfHeader->e_shoff, (uint64_t)(m_elfHeader->e_shnum*m_elfHeader->e_shentsize)}));
        m_programHeadersStart = reinterpret_cast<const typename ElfTypes<B>::ProgramHeader*>(
            m_accessor->read(AccessorDescriptor{m_elfHeader->e_phoff, sizeof(typename ElfTypes<B>::ProgramHeader)}));

        const auto secNames = reinterpret_cast<const typename ElfTypes<B>::SectionHeader*>(
                                m_sectionHeadersStart + m_elfHeader->e_shstrndx);
        m_sectionHeadersNames = reinterpret_cast<const char*>(
            m_accessor->read(AccessorDescriptor{secNames->sh_offset, secNames->sh_size}));
    }

    const typename ElfTypes<B>::ELFHeader* getHeader() const {
        return m_elfHeader;
    }

    size_t getSectionsNum() const {
        VPUX_ELF_THROW_UNLESS(m_elfHeader && (m_elfHeader->e_shnum <= 1000), ArgsError, "Invalid e_shnum");
        return m_elfHeader->e_shnum;
    }

    size_t getSegmentsNum() const {
        VPUX_ELF_THROW_UNLESS(m_elfHeader && (m_elfHeader->e_phnum <= 1000), ArgsError, "Invalid e_phnum");
        return m_elfHeader->e_phnum;
    }

    const Section& getSection(size_t index) const {
        if (m_sectionsCache.find(index) != m_sectionsCache.end()) {
            return m_sectionsCache[index];
        }

        const auto secHeader = m_sectionHeadersStart + index;
        const auto name = m_sectionHeadersNames + secHeader->sh_name;
        const auto data = m_accessor->read(
            AccessorDescriptor{secHeader->sh_offset,
                               // SHT_NOBITS - sections can have a size greater than the file
                               // which will cause offset out of bounds.
                               (secHeader->sh_type == SHT_NOBITS) ? 0 : secHeader->sh_size,
                               secHeader->sh_flags, secHeader->sh_addralign});

        auto section = Section(m_accessor, secHeader, name, data);
        m_sectionsCache[index] = section;

        return m_sectionsCache[index];
    }

    const Section& getSectionNoData(size_t index) const {
        if (m_sectionsCache.find(index) != m_sectionsCache.end()) {
            return m_sectionsCache[index];
        }

        const auto sectionHeader = m_sectionHeadersStart + index;
        const auto name = m_sectionHeadersNames + sectionHeader->sh_name;
        auto section = Section(m_accessor, sectionHeader, name);
        m_sectionsCache[index] = section;

        return m_sectionsCache[index];
    }

private:
    AccessManager* m_accessor;

    const typename ElfTypes<B>::ELFHeader* m_elfHeader = nullptr;
    const typename ElfTypes<B>::SectionHeader* m_sectionHeadersStart = nullptr;
    const typename ElfTypes<B>::ProgramHeader* m_programHeadersStart = nullptr;
    const char* m_sectionHeadersNames = nullptr;

    mutable std::unordered_map<size_t, Section> m_sectionsCache;
};

} // namespace elf

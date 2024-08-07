//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#include <vpux_elf/utils/error.hpp>
#include <vpux_elf/utils/utils.hpp>
#include <vpux_elf/writer.hpp>

#include <algorithm>
#include <unordered_set>

using namespace elf;
using namespace elf::writer;

//
// Writer
//

Writer::Writer() {
    // Creating NULL section
    m_sections.push_back(std::unique_ptr<Section>(new Section));

    m_sectionHeaderNames = addStringSection(".strtab");

    m_symbolNames = addStringSection(".symstrtab");
};

std::vector<uint8_t> Writer::generateELF() {
    auto elfHeader = generateELFHeader();

    std::vector<elf::SectionHeader> sectionHeaders;
    sectionHeaders.reserve(elfHeader.e_shnum);
    std::vector<elf::ProgramHeader> programHeaders;
    programHeaders.reserve(elfHeader.e_phnum);

    std::vector<Section*> sectionsFromSegments;
    for (const auto& segment : m_segments) {
        for (const auto& section : segment->m_sections) {
            sectionsFromSegments.push_back(section);
        }
    }

    elfHeader.e_shstrndx = static_cast<Elf_Half>(m_sectionHeaderNames->getIndex());

    for (auto& section : m_sections) {
        section->finalize();
        section->setNameOffset(m_sectionHeaderNames->addString(section->getName()));
    }

    auto curOffset = elfHeader.e_ehsize;
    if (elfHeader.e_shnum) {
        elfHeader.e_shoff = utils::alignUp(curOffset, elfHeader.e_shentsize);
        curOffset = static_cast<Elf_Half>(elfHeader.e_shoff);
    }
    if (elfHeader.e_phnum) {
        elfHeader.e_phoff =
                utils::alignUp(curOffset + elfHeader.e_shnum * elfHeader.e_shentsize, elfHeader.e_phentsize);
        curOffset = static_cast<Elf_Half>(elfHeader.e_phoff);
    } else {
        curOffset += elfHeader.e_shnum * elfHeader.e_shentsize;
    }

    auto dataOffset = static_cast<size_t>(curOffset + elfHeader.e_phnum * elfHeader.e_phentsize);
    auto totalBinarySize = dataOffset;

    for (auto& section : m_sections) {
        totalBinarySize = utils::alignUp(totalBinarySize, section->m_header.sh_addralign) + section->m_data.size();
    }
    for (auto& segment : m_segments) {
        for (auto& section : segment->m_sections) {
            totalBinarySize = utils::alignUp(totalBinarySize, section->m_header.sh_addralign) + section->m_data.size();
        }
        totalBinarySize = utils::alignUp(totalBinarySize, segment->m_header.p_align) + segment->m_data.size();
    }

    std::vector<uint8_t> data(totalBinarySize);

    const auto serializeSection = [&data, &sectionHeaders, &dataOffset](Section* section) {
        const auto sectionData = section->m_data;
        auto sectionHeader = section->m_header;

        dataOffset = utils::alignUp(dataOffset, section->getAddrAlign());

        if (!sectionData.empty()) {
            sectionHeader.sh_offset = dataOffset;
            sectionHeader.sh_size = sectionData.size();
        }
        sectionHeaders.push_back(sectionHeader);
        dataOffset = Writer::writeContainerToStorageVector(data, dataOffset, sectionData, 0, sectionData.size());
    };

    for (auto& section : m_sections) {
        if (std::find(sectionsFromSegments.begin(), sectionsFromSegments.end(), section.get()) !=
            sectionsFromSegments.end()) {
            continue;
        }

        serializeSection(section.get());
    }

    for (auto& segment : m_segments) {
        if (segment->m_data.empty() && segment->m_sections.empty()) {
            continue;
        }

        auto programHeader = segment->m_header;
        programHeader.p_offset = dataOffset;

        for (auto& section : segment->m_sections) {
            programHeader.p_filesz += section->m_data.size();
            serializeSection(section);
        }

        if (!segment->m_data.empty()) {
            programHeader.p_filesz += segment->m_data.size();
            data.insert(data.end(), segment->m_data.data(), segment->m_data.data() + segment->m_data.size());
            dataOffset =
                    Writer::writeContainerToStorageVector(data, dataOffset, segment->m_data, 0, segment->m_data.size());
        }

        programHeader.p_memsz = programHeader.p_filesz;

        programHeaders.push_back(programHeader);
    }

    dataOffset = writeObjectToStorageVector(data, 0, elfHeader);

    if (elfHeader.e_shoff) {
        dataOffset = writeContainerToStorageVector(data, dataOffset, sectionHeaders, 0, sectionHeaders.size());
    }
    if (elfHeader.e_phoff) {
        dataOffset = writeContainerToStorageVector(data, dataOffset, programHeaders, 0, programHeaders.size());
    }

    return data;
}

Segment* Writer::addSegment() {
    m_segments.push_back(std::unique_ptr<Segment>(new Segment));
    return m_segments.back().get();
}

Section* Writer::addSection(const std::string& name) {
    m_sections.push_back(std::unique_ptr<Section>(new Section(name)));
    m_sections.back()->setIndex(m_sections.size() - 1);
    return m_sections.back().get();
}

RelocationSection* Writer::addRelocationSection(const std::string& name) {
    m_sections.push_back(std::unique_ptr<RelocationSection>(new RelocationSection(name)));
    m_sections.back()->setIndex(m_sections.size() - 1);
    return dynamic_cast<RelocationSection*>(m_sections.back().get());
}

SymbolSection* Writer::addSymbolSection(const std::string& name) {
    m_sections.push_back(std::unique_ptr<SymbolSection>(new SymbolSection(name, m_symbolNames)));
    m_sections.back()->setIndex(m_sections.size() - 1);
    return dynamic_cast<SymbolSection*>(m_sections.back().get());
}

EmptySection* Writer::addEmptySection(const std::string& name) {
    m_sections.push_back(std::unique_ptr<EmptySection>(new EmptySection(name)));
    m_sections.back()->setIndex(m_sections.size() - 1);
    return dynamic_cast<EmptySection*>(m_sections.back().get());
}

StringSection* Writer::addStringSection(const std::string& name) {
    m_sections.push_back(std::unique_ptr<StringSection>(new StringSection(name)));
    m_sections.back()->setIndex(m_sections.size() - 1);
    return dynamic_cast<StringSection*>(m_sections.back().get());
}

elf::ELFHeader Writer::generateELFHeader() const {
    ELFHeader fileHeader{};

    fileHeader.e_ident[EI_MAG0] = ELFMAG0;
    fileHeader.e_ident[EI_MAG1] = ELFMAG1;
    fileHeader.e_ident[EI_MAG2] = ELFMAG2;
    fileHeader.e_ident[EI_MAG3] = ELFMAG3;
    fileHeader.e_ident[EI_CLASS] = ELFCLASS64;
    fileHeader.e_ident[EI_DATA] = ELFDATA2LSB;
    fileHeader.e_ident[EI_VERSION] = EV_NONE;
    fileHeader.e_ident[EI_OSABI] = 0;
    fileHeader.e_ident[EI_ABIVERSION] = 0;

    fileHeader.e_type = ET_REL;
    fileHeader.e_machine = EM_NONE;
    fileHeader.e_version = EV_NONE;

    fileHeader.e_entry = 0;
    fileHeader.e_flags = 0;
    fileHeader.e_shstrndx = 0;

    fileHeader.e_shnum = static_cast<Elf_Half>(m_sections.size());
    fileHeader.e_phnum = static_cast<Elf_Half>(m_segments.size());

    fileHeader.e_shoff = fileHeader.e_phoff = 0;

    fileHeader.e_ehsize = sizeof(ELFHeader);
    fileHeader.e_phentsize = sizeof(ProgramHeader);
    fileHeader.e_shentsize = sizeof(SectionHeader);

    return fileHeader;
}

size_t Writer::writeRawBytesToStorageVector(std::vector<uint8_t>& storageVector, size_t storageOffset,
                                            const uint8_t* sourceData, size_t sourceByteCount) {
    auto storagePos = storageVector.begin() + storageOffset;

    // Check storage offset and size bounds
    VPUX_ELF_THROW_WHEN(storagePos >= storageVector.end(), RuntimeError, "Write offset out of bounds");
    VPUX_ELF_THROW_WHEN(storagePos + sourceByteCount > storageVector.end(), RuntimeError, "Write size exceeds bounds");

    std::copy_n(sourceData, sourceByteCount, storagePos);

    return storageOffset + sourceByteCount;
}

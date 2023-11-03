//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#include <vpux_elf/accessor.hpp>
#include <vpux_elf/utils/error.hpp>
#include <vpux_elf/utils/log.hpp>

namespace elf {

size_t AccessManager::getSize() const {
    return m_size;
}

ElfDDRAccessManager::ElfDDRAccessManager(const uint8_t* blob, size_t size) {
    VPUX_ELF_THROW_WHEN(blob == nullptr, ArgsError, "invalid binary file arg");

    m_blob = blob;
    m_size = size;
}

const uint8_t* ElfDDRAccessManager::read(const AccessorDescriptor& descriptor) {
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t offset: %llu, size: %llu, procFlags: %llu, alignment: %llu",
                 descriptor.offset, descriptor.size, descriptor.procFlags, descriptor.alignment);

    VPUX_ELF_THROW_WHEN(descriptor.offset + descriptor.size > m_size, AccessError,
                        "Offset out of bounds!");

    return m_blob + descriptor.offset;
}

const uint8_t* ElfDDRAccessManager::getBlob() const {
    return m_blob;
}

ElfFSAccessManager::ElfFSAccessManager(const std::string& elfFileName)
        : m_elfStream(elfFileName, std::ifstream::binary) {
    VPUX_ELF_THROW_WHEN(!m_elfStream.good(), AccessError,
                        std::string("unable to access binary file " + elfFileName).c_str());

    m_elfStream.seekg(0, m_elfStream.end);
    m_size = m_elfStream.tellg();
    m_readBuffer.reserve(m_size);
    VPUX_ELF_THROW_UNLESS(m_readBuffer.capacity() == m_size, AllocError, "Could not allocate memory for ELF file");
}

const uint8_t* ElfFSAccessManager::read(const AccessorDescriptor& descriptor) {
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t offset: %llu, size: %llu, procFlags: %llu, alignment: %llu",
                 descriptor.offset, descriptor.size, descriptor.procFlags, descriptor.alignment);

    VPUX_ELF_THROW_WHEN(descriptor.offset + descriptor.size > m_size, AccessError,
                        "Offset out of bounds!");

    m_elfStream.seekg(descriptor.offset, m_elfStream.beg);
    m_elfStream.read(&m_readBuffer[descriptor.offset], descriptor.size);

    return reinterpret_cast<const uint8_t*>(&m_readBuffer[descriptor.offset]);
}

ElfFSAccessManager::~ElfFSAccessManager() {
    m_elfStream.close();
}

}  // namespace elf

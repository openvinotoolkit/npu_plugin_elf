//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#ifndef VPUX_ELF_LOG_UNIT_NAME
#define VPUX_ELF_LOG_UNIT_NAME "Accessor"
#endif

#include <cstring>
#include <vpux_elf/accessor.hpp>
#include <vpux_elf/utils/error.hpp>
#include <vpux_elf/utils/log.hpp>

namespace elf {

AccessManager::AccessManager(): mSize(0) {
}

AccessManager::AccessManager(size_t binarySize): mSize(binarySize) {
}

size_t AccessManager::getSize() const {
    return mSize;
}

ElfDDRAccessManager::Config::InPlaceConfig::InPlaceConfig(): mIsInPlaceEnabled(true), mIsAlignmentCheckEnabled(false) {
}

ElfDDRAccessManager::Config::Config(): mBufferManager(nullptr), mInPlaceConfig() {
}

ElfDDRAccessManager::ElfDDRAccessManager(const uint8_t* blob, size_t size, Config config)
        : AccessManager(size), mConfig(config) {
    VPUX_ELF_THROW_WHEN(blob == nullptr, ArgsError, "invalid binary file arg");

    mBlob = blob;
}

std::unique_ptr<ManagedBuffer> ElfDDRAccessManager::readInternal(size_t offset, const BufferSpecs& specs) {
    std::unique_ptr<ManagedBuffer> buffer = nullptr;

    VPUX_ELF_THROW_WHEN((offset + specs.size) > mSize, AccessError, "Read request out of bounds");

    if (mConfig.mBufferManager) {
        buffer = std::make_unique<AllocatedDeviceBuffer>(mConfig.mBufferManager, specs);
        auto devBuffer = buffer->getBuffer();
        mConfig.mBufferManager->copy(devBuffer, mBlob + offset, devBuffer.size());
    } else {
        if (mConfig.mInPlaceConfig.mIsInPlaceEnabled) {
            if (mConfig.mInPlaceConfig.mIsAlignmentCheckEnabled) {
                if ((!specs.alignment) || ((reinterpret_cast<size_t>(mBlob + offset)) % specs.alignment == 0)) {
                    buffer = std::make_unique<StaticBuffer>(const_cast<uint8_t*>(mBlob + offset), specs);
                } else {
                    buffer = std::make_unique<DynamicBuffer>(specs);
                    std::memcpy(buffer->getBuffer().cpu_addr(), mBlob + offset, buffer->getBuffer().size());
                }
            } else {
                buffer = std::make_unique<StaticBuffer>(const_cast<uint8_t*>(mBlob + offset), specs);
            }
        } else {
            buffer = std::make_unique<DynamicBuffer>(specs);
            std::memcpy(buffer->getBuffer().cpu_addr(), mBlob + offset, buffer->getBuffer().size());
        }
    }

    return buffer;
}

void ElfDDRAccessManager::readExternal(size_t offset, ManagedBuffer& buffer) {
    VPUX_ELF_THROW_WHEN((offset + buffer.getBufferSpecs().size) > mSize, AccessError, "Read request out of bounds");

    auto devBuffer = buffer.getBuffer();
    std::memcpy(devBuffer.cpu_addr(), mBlob + offset, devBuffer.size());
}

ElfFSAccessManager::ElfFSAccessManager(const std::string& elfFileName)
        : m_elfStream(elfFileName, std::ifstream::binary) {
    VPUX_ELF_THROW_WHEN(!m_elfStream.good(), AccessError,
                        std::string("unable to access binary file " + elfFileName).c_str());

    m_elfStream.seekg(0, m_elfStream.end);
    mSize = m_elfStream.tellg();
}

std::unique_ptr<ManagedBuffer> ElfFSAccessManager::readInternal(size_t offset, const BufferSpecs& specs) {
    VPUX_ELF_THROW_WHEN((offset + specs.size) > mSize, AccessError, "Read request out of bounds");
    auto buffer = std::make_unique<DynamicBuffer>(specs);
    m_elfStream.seekg(offset, m_elfStream.beg);
    m_elfStream.read(reinterpret_cast<char*>(buffer->getBuffer().cpu_addr()), buffer->getBuffer().size());

    return buffer;
}

void ElfFSAccessManager::readExternal(size_t offset, ManagedBuffer& buffer) {
    VPUX_ELF_THROW_WHEN((offset + buffer.getBufferSpecs().size) > mSize, AccessError, "Read request out of bounds");
    m_elfStream.seekg(offset, m_elfStream.beg);
    m_elfStream.read(reinterpret_cast<char*>(buffer.getBuffer().cpu_addr()), buffer.getBuffer().size());
}

ElfFSAccessManager::~ElfFSAccessManager() {
    m_elfStream.close();
}

}  // namespace elf

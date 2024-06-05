//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <vpux_headers/buffer_manager.hpp>
#include <vpux_headers/managed_buffer.hpp>

namespace elf {

/*
Abstraction class to encapsulate access to ELF binary file from DDR memory.
*/

class AccessManager {
public:
    AccessManager();
    explicit AccessManager(size_t binarySize);
    virtual ~AccessManager() = default;

    virtual std::unique_ptr<ManagedBuffer> readInternal(size_t offset, const BufferSpecs& specs) = 0;
    virtual void readExternal(size_t offset, ManagedBuffer& buffer) = 0;
    size_t getSize() const;

protected:
    size_t mSize = 0;
};

/*
Abstraction class to encapsulate access to ELF binary file from DDR memory.
*/

class ElfDDRAccessManager final : public AccessManager {
public:
    struct Config {
        struct InPlaceConfig {
            bool mIsInPlaceEnabled;
            bool mIsAlignmentCheckEnabled;

            InPlaceConfig();
        };

        BufferManager* mBufferManager;
        InPlaceConfig mInPlaceConfig;

        Config();
    };

    ElfDDRAccessManager(const uint8_t* blob, size_t size, Config = {});
    ElfDDRAccessManager(const ElfDDRAccessManager&) = delete;
    ElfDDRAccessManager(ElfDDRAccessManager&&) = delete;

    ElfDDRAccessManager& operator=(const ElfDDRAccessManager&) = delete;
    ElfDDRAccessManager& operator=(ElfDDRAccessManager&&) = delete;

    ~ElfDDRAccessManager() = default;

    std::unique_ptr<ManagedBuffer> readInternal(size_t offset, const BufferSpecs& specs) override;
    void readExternal(size_t offset, ManagedBuffer& buffer) override;

private:
    const uint8_t* mBlob = nullptr;
    Config mConfig;
};

class ElfFSAccessManager final : public AccessManager {
public:
    explicit ElfFSAccessManager(const std::string& elfFileName);
    ElfFSAccessManager(const ElfFSAccessManager&) = delete;
    ElfFSAccessManager(ElfFSAccessManager&&) = delete;

    ElfFSAccessManager& operator=(const ElfFSAccessManager&) = delete;
    ElfFSAccessManager& operator=(ElfFSAccessManager&&) = delete;

    ~ElfFSAccessManager();

    std::unique_ptr<ManagedBuffer> readInternal(size_t offset, const BufferSpecs& specs) override;
    void readExternal(size_t offset, ManagedBuffer& buffer) override;

private:
    std::ifstream m_elfStream;
};

}  // namespace elf

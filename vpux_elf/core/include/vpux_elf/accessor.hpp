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

namespace elf {

/*
Abstraction class to encapsulate access to ELF binary file from DDR memory.
*/

struct AccessorDescriptor {
public:
    uint64_t offset;
    uint64_t size;
    uint64_t procFlags;
    uint64_t alignment;

    AccessorDescriptor(uint64_t offset, uint64_t size, uint64_t procFlags = 0, uint64_t alignment = 0);
};

class AccessManager {
public:
    virtual const uint8_t* read(const AccessorDescriptor& descriptor) = 0;

    virtual ~AccessManager() = default;

    size_t getSize() const;

protected:
    size_t m_size = 0;
};

/*
Abstraction class to encapsulate access to ELF binary file from DDR memory.
*/

class ElfDDRAccessManager : public AccessManager {
public:
    ElfDDRAccessManager(const uint8_t* blob, size_t size);

    const uint8_t* read(const AccessorDescriptor& descriptor) override;
    const uint8_t* getBlob() const;

private:
    const uint8_t* m_blob = nullptr;
};

class ElfFSAccessManager : public AccessManager {
public:
    ElfFSAccessManager(const std::string& elfFileName);
    ElfFSAccessManager(const ElfFSAccessManager&) = delete;
    ElfFSAccessManager(const ElfFSAccessManager&&) = delete;
    ElfFSAccessManager& operator=(const ElfFSAccessManager&) = delete;
    ElfFSAccessManager& operator=(const ElfFSAccessManager&&) = delete;

    const uint8_t* read(const AccessorDescriptor& descriptor) override;

    ~ElfFSAccessManager();

private:
    std::ifstream m_elfStream;
    std::vector<char> m_readBuffer;
};

}  // namespace elf

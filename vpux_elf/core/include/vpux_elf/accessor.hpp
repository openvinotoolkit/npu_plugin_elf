//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <string>
#include <iostream>
#include <fstream>

#include <vpux_loader/vpux_loader.hpp>

namespace elf {

/*
Abstraction class to encapsulate access to ELF binary file from DDR memory.
*/

class ElfDDRAccessManager : public AccessManager {
public:
    ElfDDRAccessManager(const uint8_t* blob, size_t size, BufferManager* bufferMgr = nullptr);

    const uint8_t* read(const AccessorDescriptor& descriptor) override;
    const uint8_t* getBlob() const;

private:
    const uint8_t* m_blob = nullptr;
};

class ElfFSAccessManager : public AccessManager {
public:
    ElfFSAccessManager(const std::string& elfFileName, BufferManager* bufferMgr);
 
    const uint8_t* read(const AccessorDescriptor& descriptor) override;

    ~ElfFSAccessManager();

private:
    std::ifstream m_elfStream;
};

} // namespace elf

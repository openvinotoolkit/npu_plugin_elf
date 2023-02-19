//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <vpux_elf/types/data_types.hpp>
#include <vpux_elf/types/section_header.hpp>

#include <vector>
#include <string>
#include <memory>

namespace elf {

class Writer;

namespace writer {

class Section {
public:
    std::string getName() const;
    void setName(const std::string& name);

    Elf_Xword getAddrAlign() const;
    void setAddrAlign(Elf_Xword addrAlign);

    Elf64_Addr getAddr() const;
    void setAddr(Elf64_Addr addr);

    Elf_Xword getFlags() const;
    void setFlags(Elf_Xword flags);
    void maskFlags(Elf_Xword flags);

    Elf_Word getType() const;
    void setType(Elf_Word type);

    size_t getFileAlignRequirement() const;

    size_t getIndex() const;
    size_t getDataSize() const;
    
    virtual ~Section() = default;

protected:
    explicit Section(const std::string& name = {});

    virtual void finalize();

    void setIndex(size_t index);
    void setNameOffset(size_t offset);

protected:
    std::string m_name;
    size_t m_index = 0;
    size_t m_fileAlignRequirement = 1;

    SectionHeader m_header{};
    std::vector<uint8_t> m_data;

    friend Writer;
};

} // namespace writer
} // namespace elf

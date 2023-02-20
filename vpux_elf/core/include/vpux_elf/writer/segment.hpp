//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <vpux_elf/types/data_types.hpp>
#include <vpux_elf/types/program_header.hpp>

#include <vpux_elf/writer/section.hpp>

#include <vector>
#include <memory>

namespace elf {

class Writer;

namespace writer {

class Segment {
public:
    template <typename T>
    void appendData(const T* data, size_t sizeInElements) {
        m_data.insert(m_data.end(), reinterpret_cast<const uint8_t*>(data), reinterpret_cast<const uint8_t*>(data) + sizeInElements  * sizeof(T));
    }
    void addSection(Section* section);

    void setType(Elf_Word type);
    void setAlign(Elf_Xword align);

private:
    Segment();

    ProgramHeader m_header{};

    std::vector<uint8_t> m_data;
    std::vector<Section*> m_sections;

    friend Writer;
};

} // namespace writer
} // namespace elf

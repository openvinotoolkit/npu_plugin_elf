//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <cstddef>
#include <vpux_elf/utils/error.hpp>


namespace elf {

namespace details {

template <typename T>
class FixedVector {
public:
    inline size_t size() const { return m_size; }
    inline T *data() { return m_data; }
    inline const T *data() const { return m_data; }
    inline T &operator[](size_t index) { return m_data[index]; }
    inline const T &operator[](size_t index) const { return m_data[index]; }

    FixedVector()
        : m_size(0)
        , m_data(nullptr) {}

    FixedVector(size_t size)
        : m_size(size)
        , m_data(new T[m_size]) {
        VPUX_ELF_THROW_UNLESS(m_data != nullptr, AllocError, "Failed to allocate memory for internal FixedVector");
    }

    void resize(size_t size) {
        if (m_data) {
            delete[] m_data;
        }
        m_size = size;
        m_data = new T[m_size];

        VPUX_ELF_THROW_UNLESS(m_data != nullptr, AllocError, "Failed to allocate memory for internal FixedVector");
        return;
    }

    ~FixedVector() {
        if (m_data) {
            delete[] m_data;
        }
    }

private:
    size_t m_size;
    T *m_data;
};

} // namespace details

} // namespace elf

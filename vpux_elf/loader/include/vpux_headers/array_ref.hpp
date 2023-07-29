//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <cstddef>
#include <vector>
#include <array>

namespace elf {
// quick immutable array ref
template <typename T>
class ArrayRef {
public:
    using value_type = T;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using reference = value_type &;
    using const_reference = const value_type &;
    using iterator = const_pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;

public:
    inline size_t size() const { return m_size; }
    inline const T *data() const { return m_data; }
    inline const T &operator[](size_t index) const { return m_data[index]; }

    ArrayRef()
        : m_data(nullptr)
        , m_size(0){};
    ArrayRef(const T *data, size_t size)
        : m_data(data)
        , m_size(size){};

    template <typename A>
    ArrayRef(const std::vector<T, A> &vector)
        : m_data(vector.data())
        , m_size(vector.size()) {}

    template <size_t N>
    constexpr ArrayRef(const std::array<T, N> &array)
        : m_data(array.data())
        , m_size(N) {}

    iterator begin() const { return m_data; }
    iterator end() const { return m_data + m_size; }

    reverse_iterator rbegin() const { return reverse_iterator(end()); }
    reverse_iterator rend() const { return reverse_iterator(begin()); }

    bool empty() const { return m_size == 0; }

private:
    const T *m_data;
    const size_t m_size;
};
} // namespace elf

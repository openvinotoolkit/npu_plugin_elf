//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <vpux_elf/types/symbol_entry.hpp>
#include <vpux_headers/buffer_manager.hpp>
#include <vpux_headers/device_buffer.hpp>
#include <vpux_headers/array_ref.hpp>
#include <vpux_headers/metadata.hpp>

namespace elf {

constexpr auto DEFAULT_ALIGN = 64;

class HostParsedInferenceCommon {
public:
    virtual ~HostParsedInferenceCommon() = default;
    virtual ArrayRef<SymbolEntry> getSymbolTable(uint8_t index) const = 0;

    /**
     * Allocate specific architecture host parsed inference
     *
     * @param bufferManager the buffer manager used by the caller to allocate
     * loader and sections. To be deallocated by the caller of this method.
     *
     * @return DeviceBuffer to the HostParsedInference allocated using
     * the structures used for the current architecture.
     *
     */
    virtual DeviceBuffer allocateHostParsedInference(BufferManager *bufferManager) = 0;
    /**
     * Set the entry (mapped inference) and the resource requirements
     * for the pre-allocated DeviceBuffer that contains the current architecture
     * structure in memory
     *
     * @param devBuffer reference to the device buffer in order to reinterpret
     * it to the current architecture specific HPI strucutre
     *
     * @param mapped_entry uint64_t that holds the address of the mapped entry, returned by the loader
     * getEntry method
     *
     * @param resReq resource requirements to be added to the host parsed inference
     */
    virtual void setHostParsedInference(DeviceBuffer &devBuffer, uint64_t mapped_entry,
                                        ResourceRequirements resReq) = 0;
};
} // namespace elf

//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//


#pragma once

#include <vpux_headers/buffer_manager.hpp>
#include <vpux_headers/device_buffer.hpp>
#include <hpi_common_interface.hpp>

namespace elf {
class HostParsedInference_3720 : public HostParsedInferenceCommon {
public:
    ArrayRef<SymbolEntry> getSymbolTable(uint8_t index) const override;
    DeviceBuffer allocateHostParsedInference(BufferManager *bufferManager) override;
    void setHostParsedInference(DeviceBuffer &devBuffer, uint64_t mapped_entry, ResourceRequirements resReq) override;
};
} // namespace elf

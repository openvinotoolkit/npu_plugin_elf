//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <memory>
#include <vpux_headers/buffer_manager.hpp>
#include <vpux_headers/device_buffer.hpp>
#include <vpux_headers/array_ref.hpp>
#include <vpux_headers/metadata.hpp>
#include <vpux_elf/accessor.hpp>


namespace elf {

class VPUXLoader;

class HostParsedInference final {
public:
    HostParsedInference(BufferManager *bufferMgr, AccessManager *accessMgr);
    HostParsedInference(const HostParsedInference& other) = delete;
    HostParsedInference(HostParsedInference&& other) = default;
    ~HostParsedInference();

    HostParsedInference operator=(const HostParsedInference& rhs) = delete;

    DeviceBuffer getParsedInference() const;
    ArrayRef<DeviceBuffer> getAllocatedBuffers() const;
    ArrayRef<DeviceBuffer> getInputBuffers() const;
    ArrayRef<DeviceBuffer> getOutputBuffers() const;
    ArrayRef<DeviceBuffer> getProfBuffers() const;
    NetworkMetadata getMetadata();
    void applyInputOutput(std::vector<DeviceBuffer> &inputs, std::vector<DeviceBuffer> &outputs,
                          std::vector<DeviceBuffer> &profiling);
    HostParsedInference clone();

private:
    DeviceBuffer parsedInference;
    BufferManager *bufferManager;
    AccessManager *accessManager;
    ResourceRequirements resRequirements;
    std::unique_ptr<VPUXLoader> loader;
};

} // namespace elf

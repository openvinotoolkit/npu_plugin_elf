//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <vpux_elf/accessor.hpp>
#include <vpux_headers/buffer_manager.hpp>
#include <vpux_headers/device_buffer.hpp>
#include <vpux_headers/metadata.hpp>
#include <vpux_headers/platform.hpp>
#include <vpux_elf/utils/version.hpp>

namespace elf {

class VPUXLoader;

// Structure that gathers configuration options for HPI instances.
// Subject to various additions/modifications in the future
struct HPIConfigs {
    elf::Version nnVersion;
    elf::platform::ArchKind archKind = elf::platform::ArchKind::UNKNOWN;
};

class HostParsedInference final {
public:
    HostParsedInference(BufferManager* bufferMgr, AccessManager* accessMgr, elf::HPIConfigs hpiConfigs);
    HostParsedInference(const HostParsedInference& other);
    HostParsedInference(HostParsedInference&& other);
    ~HostParsedInference();

    HostParsedInference& operator=(const HostParsedInference& rhs);
    HostParsedInference& operator=(HostParsedInference&& rhs);

    DeviceBuffer getParsedInference() const;
    std::vector<DeviceBuffer> getAllocatedBuffers() const;
    std::vector<DeviceBuffer> getInputBuffers() const;
    std::vector<DeviceBuffer> getOutputBuffers() const;
    std::vector<DeviceBuffer> getProfBuffers() const;
    std::shared_ptr<const NetworkMetadata> getMetadata();
    std::shared_ptr<const elf::platform::PlatformInfo> getPlatformInfo();
    elf::Version getElfABIVersion() const;
    elf::Version getMIVersion() const;

    void applyInputOutput(std::vector<DeviceBuffer>& inputs, std::vector<DeviceBuffer>& outputs,
                          std::vector<DeviceBuffer>& profiling);
    void load();

private:
    BufferManager* bufferManager;
    AccessManager* accessManager;
    std::shared_ptr<NetworkMetadata> metadata;
    std::shared_ptr<elf::platform::PlatformInfo> platformInfo;
    std::unique_ptr<VPUXLoader> loader;
    std::shared_ptr<AllocatedDeviceBuffer> parsedInference;
    elf::HPIConfigs hpiCfg;

    // helpers
    void readMetadata();
    void readPlatformInfo();
    const uint64_t* readPerfMetrics();
    elf::Version readVersioningInfo(uint32_t versionType) const;
};

}  // namespace elf

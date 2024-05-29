//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <tuple>
#include <unordered_map>
#include <vector>

#include <vpux_elf/utils/log.hpp>
#include <vpux_headers/buffer_details.hpp>
#include <vpux_headers/buffer_manager.hpp>
#include <vpux_headers/buffer_specs.hpp>
#include <vpux_headers/device_buffer.hpp>
#include <vpux_headers/managed_buffer.hpp>

namespace elf {

class DeviceBufferContainer {
public:
    using BufferType = ManagedBuffer;
    using BufferPtr = std::shared_ptr<BufferType>;

    struct BufferInfo {
        BufferPtr mBuffer;
        BufferDetails mBufferDetails;
    };

    using BufferMap = std::unordered_map<size_t, BufferInfo>;

    DeviceBufferContainer(BufferManager* bManager);
    DeviceBufferContainer(const DeviceBufferContainer& other);
    DeviceBufferContainer(DeviceBufferContainer&&) = default;
    ~DeviceBufferContainer() = default;
    DeviceBufferContainer& operator=(const DeviceBufferContainer& rhs);
    DeviceBufferContainer& operator=(DeviceBufferContainer&&) = default;

    BufferPtr buildAllocatedDeviceBuffer(BufferSpecs bSpecs);
    bool hasBufferInfoAtIndex(size_t index);
    BufferInfo& getBufferInfoFromIndex(size_t index);
    void replaceBufferInfoAtIndex(size_t index, BufferInfo bufferInfo);
    size_t getBufferInfoCount();
    std::vector<DeviceBuffer> getBuffersAsVector() const;

private:
    BufferMap mBufferMap;
    BufferManager* mBufferManager;

    void copyBufferMap(const BufferMap& otherBufferMap);
};

}  // namespace elf

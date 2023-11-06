//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <memory>
#include <vpux_elf/utils/error.hpp>
#include <vpux_elf/utils/log.hpp>
#include <vpux_headers/array_ref.hpp>
#include <vpux_headers/buffer_specs.hpp>
#include <vpux_headers/device_buffer.hpp>

namespace elf {

class BufferManager {
public:
    virtual DeviceBuffer allocate(const BufferSpecs& buffSpecs) = 0;
    virtual void deallocate(DeviceBuffer& devAddress) = 0;
    /// @brief
    /// This API is used by the loader to mark that it actively uses the DeviceBuffer.
    /// A successful lock call should ensure that the DeviceBuffer:
    ///  - has a valid cpu_addr
    ///  - remains in DDR while in use by the loader
    /// The driver-loader contract is not frozen yet, so below are a few cautious assumptions:
    ///  - the DeviceBuffer does not have a valid cpu_addr until lock is performed
    ///  - the sys calls behind the lock and unlock APIs might be state sensitive (i.e. an arbitrary call order is not
    ///    supported). Therefore, for every DeviceBuffer:
    ///    -> consider a lock counter initialized to 0 when the DeviceBuffer is created
    ///    -> every lock call increments the lock counter by 1
    ///    -> every unlock call decrements the lock counter by 1
    ///    -> incrementing from 1 is forbidden
    ///    -> decrementing from 0 is forbidden
    /// @param devAddress - DeviceBuffer reference
    virtual void lock(DeviceBuffer& devAddress) = 0;
    /// @brief This API releases a DeviceBuffer that was previously locked by a lock call.
    /// @param devAddress - DeviceBuffer reference
    virtual void unlock(DeviceBuffer& devAddress) = 0;
    virtual size_t copy(DeviceBuffer& to, const uint8_t* from, size_t count) = 0;
    virtual ~BufferManager() = default;
};

class AllocatedDeviceBuffer {
public:
    enum class SharedInfo {
        IS_SHARED = 0,
        NOT_SHARED,
    };

    enum class DataInfo {
        ELF_HAS_DATA = 0,
        ELF_NO_DATA,
    };

private:
    BufferManager* bufferManager;
    DeviceBuffer devBuffer;
    BufferSpecs bufferSpecs;
    DataInfo data;
    SharedInfo shared;

public:
    AllocatedDeviceBuffer(BufferManager* bManager, BufferSpecs bSpecs, DataInfo dataInfo = DataInfo::ELF_NO_DATA,
                          SharedInfo sharedInfo = SharedInfo::NOT_SHARED)
            : bufferSpecs(bSpecs) {
        VPUX_ELF_THROW_UNLESS(bManager, ArgsError, "Invalid BufferManager pointer");
        bufferManager = bManager;
        devBuffer = bufferManager->allocate(bufferSpecs);
        VPUX_ELF_THROW_WHEN((devBuffer.cpu_addr() == nullptr) || (devBuffer.size() < bSpecs.size), AllocError,
                            "Failed to allocate DeviceBuffer");
        data = dataInfo;
        shared = sharedInfo;
    }
    AllocatedDeviceBuffer(const AllocatedDeviceBuffer& other) = delete;
    AllocatedDeviceBuffer(const AllocatedDeviceBuffer&& other) = delete;
    AllocatedDeviceBuffer& operator=(const AllocatedDeviceBuffer& rhs) = delete;
    AllocatedDeviceBuffer& operator=(const AllocatedDeviceBuffer&& rhs) = delete;
    ~AllocatedDeviceBuffer() {
        bufferManager->deallocate(devBuffer);
        bufferManager = nullptr;
    }

    DeviceBuffer getBuffer() const {
        return devBuffer;
    }
    BufferSpecs getBufferSpecs() const {
        return bufferSpecs;
    }
    auto getSharedInfo() {
        return shared;
    }
    auto getDataInfo() {
        return data;
    }
    bool isShared() const {
        return (shared == SharedInfo::IS_SHARED);
    }
    bool hasData() const {
        return (data == DataInfo::ELF_HAS_DATA);
    }
    void lock() {
        bufferManager->lock(devBuffer);
    }
    void unlock() {
        bufferManager->unlock(devBuffer);
    }
    size_t load(const uint8_t* from, size_t count) {
        return bufferManager->copy(devBuffer, from, count);
    }
    void loadWithLock(const uint8_t* from, size_t count) {
        VPUX_ELF_LOG(LogLevel::LOG_INFO, "Loading %lu with lock from %p to %p", count, from, getBuffer().cpu_addr());
        lock();
        load(from, count);
        unlock();
    }
};

}  // namespace elf

//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <vpux_headers/array_ref.hpp>
#include <vpux_headers/buffer_manager.hpp>
#include <vpux_headers/buffer_specs.hpp>
#include <vpux_headers/device_buffer.hpp>

#include <vpux_elf/accessor.hpp>
#include <vpux_elf/reader.hpp>

#include <vpux_elf/types/elf_structs.hpp>
#include <vpux_elf/types/relocation_entry.hpp>
#include <vpux_elf/types/symbol_entry.hpp>
#include <vpux_elf/types/vpu_extensions.hpp>
#include <vpux_elf/utils/error.hpp>
#include <vpux_headers/metadata.hpp>

namespace elf {

class DeviceBufferContainer {
private:
    using BufferT = AllocatedDeviceBuffer;
    using SharedPtrT = std::shared_ptr<BufferT>;

    std::unordered_map<uint64_t, SharedPtrT> devBuffers;
    mutable std::vector<DeviceBuffer> devBuffersVector;
    BufferManager* bufferManager;

public:
    DeviceBufferContainer(BufferManager* bManager): bufferManager(bManager) {
    }
    DeviceBufferContainer(const DeviceBufferContainer& other) {
        VPUX_ELF_LOG(LogLevel::LOG_TRACE, "Copying DeviceBuffer container");
        bufferManager = other.bufferManager;
        for (auto& it : other.devBuffers) {
            auto index = it.first;
            auto buffer = it.second;
            if (buffer->isShared()) {
                devBuffers[index] = buffer;
            } else {
                devBuffers[index] = std::make_shared<BufferT>(bufferManager, buffer->getBufferSpecs(),
                                                              buffer->getDataInfo(), buffer->getSharedInfo());
            }
        }
    }
    DeviceBufferContainer(const DeviceBufferContainer&&);
    DeviceBufferContainer& operator=(const DeviceBufferContainer&) = delete;
    DeviceBufferContainer& operator=(const DeviceBufferContainer&&) = delete;

    ~DeviceBufferContainer() = default;

    template <typename... BufferArgs>
    SharedPtrT createSharedDeviceBuffer(uint64_t index, BufferArgs... args) {
        devBuffers[index] = std::make_shared<BufferT>(bufferManager, std::forward<BufferArgs>(args)...);
        return devBuffers[index];
    }
    bool hasBufferAtIndex(uint64_t index) {
        return (devBuffers.find(index) != devBuffers.end());
    }
    SharedPtrT getFromIndex(uint64_t index) {
        VPUX_ELF_THROW_UNLESS(hasBufferAtIndex(index), ArgsError, "Unknown buffer index requested");
        return devBuffers[index];
    }
    size_t getCount() {
        return devBuffers.size();
    }
    ArrayRef<DeviceBuffer> getBuffersAsArrayRef() const {
        devBuffersVector.clear();
        for (const auto& buffer : devBuffers) {
            devBuffersVector.push_back(buffer.second->getBuffer());
        }
        return ArrayRef<DeviceBuffer>(devBuffersVector);
    }
};

class VPUXLoader {
private:
    using RelocationFunc = std::function<void(void*, const elf::SymbolEntry&, const Elf_Sxword)>;
    using RelocationType = Elf_Word;

    enum class Action { None, AllocateAndLoad, Allocate, Relocate, RegisterUserIO, RegisterNetworkMetadata, Error };

    static const std::map<Elf_Word, Action> actionMap;
    static const std::map<RelocationType, RelocationFunc> relocationMap;

public:
    VPUXLoader(AccessManager* accessor, BufferManager* bufferManager, ArrayRef<SymbolEntry> runtimeSymTabs,
               bool symTabOverrideMode = false, ArrayRef<std::string> symbolnames = ArrayRef<std::string>());
    VPUXLoader(const VPUXLoader& other);
    VPUXLoader(VPUXLoader&& other) = delete;
    VPUXLoader& operator=(const VPUXLoader&) = delete;
    VPUXLoader& operator=(const VPUXLoader&&) = delete;
    ~VPUXLoader();

    uint64_t getEntry();

    void applyJitRelocations(std::vector<DeviceBuffer>& inputs, std::vector<DeviceBuffer>& outputs,
                             std::vector<DeviceBuffer>& profiling);

    ArrayRef<DeviceBuffer> getAllocatedBuffers() const;
    ArrayRef<DeviceBuffer> getInputBuffers() const;
    ArrayRef<DeviceBuffer> getOutputBuffers() const;
    ArrayRef<DeviceBuffer> getProfBuffers() const;
    const elf::ResourceRequirements getResourceRequirements() const;
    const elf::NetworkMetadata getNetworkMetadata() const;

private:
    bool checkSectionType(const elf::SectionHeader* section, Elf_Word secType) const;
    void registerUserIO(std::vector<DeviceBuffer>& io, const elf::SymbolEntry* symbols, size_t symbolCount) const;
    void load();

    void applyRelocations(ArrayRef<int> relocationSectionIndexes);

    BufferManager* m_bufferManager;
    std::shared_ptr<Reader<ELF_Bitness::Elf64>> m_reader;
    DeviceBufferContainer m_bufferContainer;
    ArrayRef<SymbolEntry> m_runtimeSymTabs;

    std::shared_ptr<std::vector<int>> m_relocationSectionIndexes;
    std::shared_ptr<std::vector<int>> m_jitRelocations;

    std::shared_ptr<std::vector<DeviceBuffer>> m_userInputsDescriptors;
    std::shared_ptr<std::vector<DeviceBuffer>> m_userOutputsDescriptors;
    std::shared_ptr<std::vector<DeviceBuffer>> m_profOutputsDescriptors;

    std::shared_ptr<elf::NetworkMetadata> m_networkMetadata;

    const bool m_symTabOverrideMode;
    const bool m_explicitAllocations;
    ArrayRef<std::string> m_symbolNames;
};

}  // namespace elf

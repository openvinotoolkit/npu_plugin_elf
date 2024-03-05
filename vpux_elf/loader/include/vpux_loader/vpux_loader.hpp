//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#ifndef VPUX_ELF_LOG_UNIT_NAME
#define VPUX_ELF_LOG_UNIT_NAME "VpuxLoader"
#endif

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <vpux_elf/accessor.hpp>
#include <vpux_elf/reader.hpp>
#include <vpux_elf/utils/log.hpp>
#include <vpux_headers/buffer_manager.hpp>
#include <vpux_headers/buffer_specs.hpp>
#include <vpux_headers/device_buffer.hpp>

#include <vpux_elf/types/elf_structs.hpp>
#include <vpux_elf/types/relocation_entry.hpp>
#include <vpux_elf/types/symbol_entry.hpp>
#include <vpux_elf/types/vpu_extensions.hpp>
#include <vpux_elf/utils/error.hpp>
#include <vpux_headers/metadata.hpp>
#include <vpux_headers/serial_metadata.hpp>

namespace elf {

class DeviceBufferContainer {
private:
    using BufferT = AllocatedDeviceBuffer;
    using SharedPtrT = std::shared_ptr<BufferT>;

    std::unordered_map<uint64_t, SharedPtrT> devBuffers;
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
    DeviceBufferContainer(DeviceBufferContainer&&) = default;
    DeviceBufferContainer& operator=(const DeviceBufferContainer& other) {
        if (this == &other) {
            return *this;
        }

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
        return *this;
    }
    DeviceBufferContainer& operator=(DeviceBufferContainer&&) = default;

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
    std::vector<DeviceBuffer> getBuffersAsVector() const {
        std::vector<DeviceBuffer> devBuffersVector;
        devBuffersVector.reserve(devBuffers.size());
        for (const auto& buffer : devBuffers) {
            devBuffersVector.push_back(buffer.second->getBuffer());
        }
        return devBuffersVector;
    }
};

class VPUXLoader {
private:
    using RelocationFunc = std::function<void(void*, const elf::SymbolEntry&, const Elf_Sxword)>;
    using RelocationType = Elf_Word;

    enum class Action {
        None,
        AllocateAndLoad,
        Allocate,
        Relocate,
        RegisterUserIO,
        RegisterNetworkMetadata,
        RegisterVersionInfo,
        Error
    };

    static const std::map<Elf_Word, Action> actionMap;
    static const std::map<RelocationType, RelocationFunc> relocationMap;

public:
    VPUXLoader(AccessManager* accessor, BufferManager* bufferManager);
    VPUXLoader(const VPUXLoader& other);
    VPUXLoader(VPUXLoader&& other) = delete;
    VPUXLoader& operator=(const VPUXLoader&);
    VPUXLoader& operator=(const VPUXLoader&&) = delete;
    ~VPUXLoader();

    void load(const std::vector<SymbolEntry>& runtimeSymTabs, bool symTabOverrideMode = false,
              const std::vector<elf::Elf_Word>& symbolSectionTypes = {});
    uint64_t getEntry();

    void applyJitRelocations(std::vector<DeviceBuffer>& inputs, std::vector<DeviceBuffer>& outputs,
                             std::vector<DeviceBuffer>& profiling);

    std::vector<DeviceBuffer> getAllocatedBuffers() const;
    std::vector<DeviceBuffer> getInputBuffers() const;
    std::vector<DeviceBuffer> getOutputBuffers() const;
    std::vector<DeviceBuffer> getProfBuffers() const;
    std::vector<DeviceBuffer>& getSectionsOfType(elf::Elf_Word type);

private:
    bool checkSectionType(const elf::SectionHeader* section, Elf_Word secType) const;
    void registerUserIO(std::vector<DeviceBuffer>& io, const elf::SymbolEntry* symbols, size_t symbolCount) const;

    void updateSharedBuffers(const std::vector<std::size_t>& relocationSectionIndexes);
    void applyRelocations(const std::vector<std::size_t>& relocationSectionIndexes);

    BufferManager* m_bufferManager;
    std::shared_ptr<Reader<ELF_Bitness::Elf64>> m_reader;
    DeviceBufferContainer m_bufferContainer;
    std::vector<SymbolEntry> m_runtimeSymTabs;

    std::shared_ptr<std::vector<std::size_t>> m_relocationSectionIndexes;
    std::shared_ptr<std::vector<std::size_t>> m_jitRelocations;

    std::shared_ptr<std::vector<DeviceBuffer>> m_userInputsDescriptors;
    std::shared_ptr<std::vector<DeviceBuffer>> m_userOutputsDescriptors;
    std::shared_ptr<std::vector<DeviceBuffer>> m_profOutputsDescriptors;

    std::shared_ptr<std::map<elf::Elf_Word /*section type*/, std::vector<DeviceBuffer>>> /*section data*/
            m_sectionMap;

    bool m_symTabOverrideMode;
    bool m_explicitAllocations;
    bool m_loaded;
    std::vector<elf::Elf_Word> m_symbolSectionTypes;
};

}  // namespace elf

//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <vpux_headers/buffer_specs.hpp>
#include <vpux_headers/buffer_manager.hpp>
#include <vpux_headers/device_buffer.hpp>
#include <vpux_headers/fixed_vector.hpp>
#include <vpux_headers/array_ref.hpp>

#include <vpux_elf/accessor.hpp>
#include <vpux_elf/reader.hpp>

#include <vpux_elf/types/elf_structs.hpp>
#include <vpux_elf/types/relocation_entry.hpp>
#include <vpux_elf/types/symbol_entry.hpp>
#include <vpux_elf/types/vpu_extensions.hpp>
#include <vpux_elf/utils/error.hpp>
#include <vpux_headers/metadata.hpp>

namespace elf {
class VPUXLoader {
private:
    using RelocationFunc = std::function<void(void*, const elf::SymbolEntry&, const Elf_Sxword)>;
    using RelocationType = Elf_Word;

    enum class Action { None, AllocateAndLoad, Allocate, Relocate, RegisterUserIO, RegisterNetworkMetadata, Error };

    static const std::map<Elf_Word, Action> actionMap;
    static const std::map<RelocationType, RelocationFunc> relocationMap;

public:
    explicit VPUXLoader(AccessManager* accessor, BufferManager* bufferManager, ArrayRef<SymbolEntry> runtimeSymTabs);
    ~VPUXLoader();

    uint64_t getEntry();

    void applyJitRelocations(std::vector<DeviceBuffer>& inputs, std::vector<DeviceBuffer>& outputs, std::vector<DeviceBuffer>& profiling);

    ArrayRef<DeviceBuffer> getAllocatedBuffers() const;
    ArrayRef<DeviceBuffer> getInputBuffers() const;
    ArrayRef<DeviceBuffer> getOutputBuffers() const;
    ArrayRef<DeviceBuffer> getProfBuffers() const;
    const elf::ResourceRequirements getResourceRequirements() const;
    const elf::NetworkMetadata getNetworkMetadata() const;

private:
    bool checkSectionType(const elf::SectionHeader* section, Elf_Word secType) const;
    void registerUserIO(details::FixedVector<DeviceBuffer>& io, const elf::SymbolEntry* symbols,
                        size_t symbolCount) const;
    void load();
    void clean();

    void applyRelocations(ArrayRef<int> relocationSectionIndexes);

    Reader<ELF_Bitness::Elf64>* m_reader = nullptr;
    BufferManager* m_bufferManager = nullptr;
    ArrayRef<SymbolEntry> m_runtimeSymTabs;

    std::vector<DeviceBuffer> m_allocatedZones;
    std::vector<DeviceBuffer> m_sectionToAddr;
    std::vector<int> m_jitRelocations;

    details::FixedVector<DeviceBuffer> m_userInputs;
    details::FixedVector<DeviceBuffer> m_userOutputs;
    details::FixedVector<DeviceBuffer> m_profOutputs;

    elf::NetworkMetadata m_networkMetadata;
};

}  // namespace elf

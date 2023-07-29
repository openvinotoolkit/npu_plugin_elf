//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#ifndef VPUX_ELF_LOG_UNIT_NAME
#define VPUX_ELF_LOG_UNIT_NAME "VpuxHpi"
#endif
// clang-format off
#include <vpux_loader/vpux_loader.hpp>
#include <vpux_elf/utils/log.hpp>
#include <vpux_elf/reader.hpp>
#include <vpux_hpi.hpp>

#include <hpi_3720.hpp>

#include <string.h>
// clang-format on

namespace elf {

enum ArchKind {
    UNKNOWN = 0,
    VPUX37XX
};

namespace {

static ResourceRequirements readResourcesFromElf(AccessManager *elfAccess) {
    Reader<ELF_Bitness::Elf64> reader(elfAccess);

    auto nSections = reader.getSectionsNum();

    for (size_t i = 0; i < nSections; i++) {
        const auto &section = reader.getSection(i);

        const auto sectionHeader = section.getHeader();
        auto sectionType = sectionHeader->sh_type;

        if (sectionType == elf::VPU_SHT_NETDESC) {
            ResourceRequirements res{};
            memcpy(&res, &section.getData<NetworkMetadata>()->resource_requirements, sizeof(res));
            return res;
        }
    }

    VPUX_ELF_THROW(HeaderError, "Failed to find a resource");
}

const static std::unordered_map<std::string, ArchKind> knownArch = {{"VPUX37XX", ArchKind::VPUX37XX}};

static ArchKind readArchKind(AccessManager *elfAccess) {
    Reader<ELF_Bitness::Elf64> reader(elfAccess);

    auto nSections = reader.getSectionsNum();

    for (size_t i = 0; i < nSections; i++) {
        const auto &section = reader.getSection(i);

        const auto sectionHeader = section.getHeader();
        auto sectionType = sectionHeader->sh_type;

        if (sectionType == elf::VPU_SHT_NETDESC) {
            char archName[MAX_STRING_LEN] = {};
            strncpy(archName, section.getData<NetworkMetadata>()->arch_name, MAX_STRING_LEN);
            return knownArch.find(archName)->second;
        }
    }

    return ArchKind::UNKNOWN;
}

} // namespace

HostParsedInference::HostParsedInference(BufferManager *bufferMgr, AccessManager *accessMgr)
    : bufferManager(bufferMgr), accessManager(accessMgr) {
    ArchKind arch = readArchKind(accessMgr);
    resRequirements = readResourcesFromElf(accessMgr);

    VPUX_ELF_LOG(LogLevel::DEBUG, "Creating specialized HPI for arch %u", arch);

    std::unique_ptr<HostParsedInferenceCommon> obj;
    switch (arch) {
        case ArchKind::VPUX37XX:
            obj = std::make_unique<HostParsedInference_3720>();
            break;
        default:
            VPUX_ELF_THROW(RangeError, "Arch not in range");
            break;
    }

    loader =
        std::make_unique<VPUXLoader>(accessMgr, bufferManager, obj->getSymbolTable(resRequirements.nn_slice_count_));

    // DeviceBuffer getting a pointer to arch specific host parsed inference
    parsedInference = obj->allocateHostParsedInference(bufferManager);

    obj->setHostParsedInference(parsedInference, loader->getEntry(), resRequirements);
}

HostParsedInference::~HostParsedInference() {
    bufferManager->deallocate(parsedInference);
}

DeviceBuffer HostParsedInference::getParsedInference() const {
    return parsedInference;
}

ArrayRef<DeviceBuffer> HostParsedInference::getAllocatedBuffers() const {
    return loader->getAllocatedBuffers();
}

ArrayRef<DeviceBuffer> HostParsedInference::getInputBuffers() const {
    return loader->getInputBuffers();
}

ArrayRef<DeviceBuffer> HostParsedInference::getOutputBuffers() const {
    return loader->getOutputBuffers();
}

ArrayRef<DeviceBuffer> HostParsedInference::getProfBuffers() const {
    return loader->getProfBuffers();
}

NetworkMetadata HostParsedInference::getMetadata() {
    return loader->getNetworkMetadata();
}

void HostParsedInference::applyInputOutput(std::vector<DeviceBuffer> &inputs, std::vector<DeviceBuffer> &outputs,
                                           std::vector<DeviceBuffer> &profiling) {
    return loader->applyJitRelocations(inputs, outputs, profiling);
}

// Buffer management of HostParsedInference and loader needs to be reworked to allow shared ownership of certain device
// buffers between multple HostParsedInference objects
HostParsedInference HostParsedInference::clone() {
    return HostParsedInference(bufferManager, accessManager);
}

} // namespace elf

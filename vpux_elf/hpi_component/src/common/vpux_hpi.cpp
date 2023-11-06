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

enum ArchKind { UNKNOWN = 0, VPUX37XX };

namespace {

static ResourceRequirements readResourcesFromElf(AccessManager* elfAccess) {

    Reader<ELF_Bitness::Elf64> reader(elfAccess);

    auto nSections = reader.getSectionsNum();

    for (size_t i = 0; i < nSections; i++) {
        const auto& section = reader.getSection(i);

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

static std::string readArchKind(AccessManager* elfAccess) {
    Reader<ELF_Bitness::Elf64> reader(elfAccess);

    auto nSections = reader.getSectionsNum();

    for (size_t i = 0; i < nSections; i++) {
        const auto& section = reader.getSection(i);

        const auto sectionHeader = section.getHeader();
        auto sectionType = sectionHeader->sh_type;

        if (sectionType == elf::VPU_SHT_NETDESC) {
            char archName[MAX_STRING_LEN] = {};
            strncpy(archName, section.getData<NetworkMetadata>()->arch_name, MAX_STRING_LEN);
            return std::string(archName);
        }
    }

    VPUX_ELF_THROW(RuntimeError, "Could not locate arch name");
    return std::string("UNKNOWN");
}

static ArchKind mapArchStringToArchKind(const std::string& archName) {
    auto retArch = knownArch.find(archName);
    if (retArch != knownArch.end()) {
        return retArch->second;
    } else {
        return ArchKind::UNKNOWN;
    }
}

static std::unique_ptr<HostParsedInferenceCommon> getArchSpecificHPI(const std::string& archName) {
    auto arch = mapArchStringToArchKind(archName);

    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "Creating specialized HPI for arch %u", arch);

    std::unique_ptr<HostParsedInferenceCommon> archSpecificHPI;
    switch (arch) {
    case ArchKind::VPUX37XX:
        archSpecificHPI = std::make_unique<HostParsedInference_3720>();
        break;
    default:
        VPUX_ELF_THROW(RangeError, (archName + " arch is not supported").c_str());
        break;
    }

    return archSpecificHPI;
}

static std::unique_ptr<VPUXLoader> getLoader(BufferManager* bufferMgr, AccessManager* accessMgr,
                                             HostParsedInferenceCommon& hpiCommon,
                                             const ResourceRequirements& resRequirements) {
    const auto symbolTable = hpiCommon.getSymbolTable(resRequirements.nn_slice_count_);
    const auto symbolNames = hpiCommon.getSymbolNames();
    auto symTabOverrideMode = hpiCommon.getSymbolNames().size() == 0 ? false : true;
    auto loader =
            std::make_unique<VPUXLoader>(accessMgr, bufferMgr, symbolTable, symTabOverrideMode, symbolNames);
    return loader;
}

}  // namespace

HostParsedInference::HostParsedInference(BufferManager* bufferMgr, AccessManager* accessMgr)
        : bufferManager(bufferMgr), accessManager(accessMgr) {
    auto archName = readArchKind(accessManager);
    resRequirements = readResourcesFromElf(accessManager);

    auto archSpecificHpi = getArchSpecificHPI(archName);
    loader = getLoader(bufferManager, accessManager, *archSpecificHpi, resRequirements);
    parsedInference =
            std::make_shared<AllocatedDeviceBuffer>(bufferManager, archSpecificHpi->getParsedInferenceBufferSpecs());
    auto parsedInferenceBuffer = parsedInference->getBuffer();
    archSpecificHpi->setHostParsedInference(parsedInferenceBuffer, loader->getEntry(), resRequirements);
}

HostParsedInference::HostParsedInference(const HostParsedInference& other)
        : bufferManager(other.bufferManager),
          accessManager(other.accessManager),
          resRequirements(other.resRequirements) {
    auto archName = readArchKind(accessManager);

    auto archSpecificHpi = getArchSpecificHPI(archName);
    loader = std::make_unique<VPUXLoader>(*other.loader);
    parsedInference =
            std::make_shared<AllocatedDeviceBuffer>(bufferManager, archSpecificHpi->getParsedInferenceBufferSpecs());
    auto parsedInferenceBuffer = parsedInference->getBuffer();
    archSpecificHpi->setHostParsedInference(parsedInferenceBuffer, loader->getEntry(), resRequirements);
};

HostParsedInference::HostParsedInference(HostParsedInference&& other)
        : bufferManager(other.bufferManager),
          accessManager(other.accessManager),
          resRequirements(other.resRequirements),
          loader(std::move(other.loader)),
          parsedInference(other.parsedInference) {
}

HostParsedInference::~HostParsedInference() {
}

HostParsedInference& HostParsedInference::operator=(const HostParsedInference& rhs) {
    if (this == &rhs) {
        return *this;
    }

    bufferManager = rhs.bufferManager;
    accessManager = rhs.accessManager;
    resRequirements = rhs.resRequirements;

    auto archName = readArchKind(accessManager);

    auto archSpecificHpi = getArchSpecificHPI(archName);
    loader = std::make_unique<VPUXLoader>(*rhs.loader);
    parsedInference =
            std::make_shared<AllocatedDeviceBuffer>(bufferManager, archSpecificHpi->getParsedInferenceBufferSpecs());
    auto parsedInferenceBuffer = parsedInference->getBuffer();
    archSpecificHpi->setHostParsedInference(parsedInferenceBuffer, loader->getEntry(), resRequirements);

    return *this;
}

HostParsedInference& HostParsedInference::operator=(HostParsedInference&& rhs) {
    if (this == &rhs) {
        return *this;
    }
    bufferManager = rhs.bufferManager;
    accessManager = rhs.accessManager;
    resRequirements = rhs.resRequirements;
    loader = std::move(rhs.loader);
    parsedInference = rhs.parsedInference;

    return *this;
}

DeviceBuffer HostParsedInference::getParsedInference() const {
    return parsedInference->getBuffer();
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

void HostParsedInference::applyInputOutput(std::vector<DeviceBuffer>& inputs, std::vector<DeviceBuffer>& outputs,
                                           std::vector<DeviceBuffer>& profiling) {
    return loader->applyJitRelocations(inputs, outputs, profiling);
}

}  // namespace elf

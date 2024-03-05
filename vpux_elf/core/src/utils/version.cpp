//
// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#include <sstream>
#include <vpux_elf/utils/log.hpp>
#include <vpux_elf/utils/version.hpp>

namespace elf {

namespace {
    std::string stringifyVersionTypeEnum(VersionType val) {
    switch (val) {
        case VersionType::UNKNOWN_VERSION: return "UNKNOWN_VERSION";
        case VersionType::ELF_ABI_VERSION: return "ELF_ABI_VERSION";
        case VersionType::MAPPED_INFERENCE_VERSION: return "MAPPED_INFERENCE_VERSION";
        default: return "";
    }
}
};

bool Version::checkValidity() const {
    return isValid && major > 0;
}

void Version::checkVersionCompatibility(const Version& expectedVersion, const Version& recievedVersion, const VersionType versionType) {
    auto versionTypeString = elf::stringifyVersionTypeEnum(versionType);

    VPUX_ELF_THROW_UNLESS(expectedVersion.checkValidity() && recievedVersion.checkValidity(), VersioningError, "Version major 0 does not constitute a valid version!", recievedVersion, expectedVersion);

    std::ostringstream logBuffer;
    if (expectedVersion.major != recievedVersion.major || expectedVersion.minor < recievedVersion.minor) {
        logBuffer << "ERROR! " << versionTypeString << " is NOT compatible with the ELF";
        logBuffer << " Expected: " << expectedVersion << " vs received: " << recievedVersion;
        VPUX_ELF_LOG(LogLevel::LOG_ERROR, logBuffer.str().c_str());
        VPUX_ELF_THROW(VersioningError, logBuffer.str().c_str(), recievedVersion, expectedVersion);
    } else if (expectedVersion.minor > recievedVersion.minor) {
        logBuffer << "Warning! " << versionTypeString << " are compatible but do not fully match.";
        logBuffer << " Expected: " << expectedVersion << " vs received: " << recievedVersion;
        VPUX_ELF_LOG(LogLevel::LOG_WARN, logBuffer.str().c_str());
    } else {
        logBuffer << versionTypeString << " are perfectly compatible. Version: " << expectedVersion;
        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, logBuffer.str().c_str());
    }
}

}  // namespace elf

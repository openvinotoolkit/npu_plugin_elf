//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <vpux_elf/types/data_types.hpp>
#include <vpux_elf/types/elf_header.hpp>
#include <vpux_elf/types/elf_structs.hpp>
#include <vpux_elf/types/program_header.hpp>
#include <vpux_elf/types/section_header.hpp>
#include <vpux_elf/types/vpu_extensions.hpp>
#include <vpux_elf/utils/error.hpp>
#include <vpux_elf/utils/utils.hpp>

#include <vpux_elf/accessor.hpp>

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace elf {

template <ELF_Bitness B>
class Reader {
public:
    class Section {
    public:
        Section() = default;
        Section(AccessManager* accessor, const typename ElfTypes<B>::SectionHeader* sectionHeader, const char* name)
                : mAccessManager(accessor), mHeader(sectionHeader), mName(name), mDataBuffer(nullptr) {
            VPUX_ELF_THROW_WHEN(!mAccessManager, ArgsError, "nullptr AccessManager");
            VPUX_ELF_THROW_WHEN(!mHeader, ArgsError, "nullptr section header");
        }

        const typename ElfTypes<B>::SectionHeader* getHeader() const {
            return mHeader;
        }

        size_t getEntriesNum() const {
            VPUX_ELF_THROW_UNLESS(mHeader->sh_entsize, SectionError,
                                  "sh_entsize=0 represents a section that does not hold a table of fixed-size entries. "
                                  "This feature is not suported.")
            return static_cast<size_t>(mHeader->sh_size / mHeader->sh_entsize);
        }

        const char* getName() const {
            return mName;
        }

        template <typename T>
        const T* getData() const {
            return reinterpret_cast<const T*>(getDataBuffer()->getBuffer().cpu_addr());
        }

        std::shared_ptr<ManagedBuffer> getDataBuffer() const {
            if (!mDataBuffer) {
                // SHT_NOBITS - sections can have a size greater than the file
                // which will cause offset out of bounds.
                // VPU_SHT_CMX_METADATA - does not contain data in the binary file, so avoid reading
                // VPU_SHT_CMX_WORKSPACE - does not contain data in the binary file, so avoid reading
                if (!((mHeader->sh_type == SHT_NOBITS) || (mHeader->sh_type == VPU_SHT_CMX_METADATA) ||
                      mHeader->sh_type == VPU_SHT_CMX_WORKSPACE)) {
                    mDataBuffer = mAccessManager->readInternal(
                            mHeader->sh_offset,
                            BufferSpecs(mHeader->sh_addralign, mHeader->sh_size, mHeader->sh_flags));
                }
            }

            return mDataBuffer;
        }

    private:
        AccessManager* mAccessManager = nullptr;
        const typename ElfTypes<B>::SectionHeader* mHeader = nullptr;
        const char* mName = nullptr;
        mutable std::shared_ptr<ManagedBuffer> mDataBuffer;
    };

    class Segment {
    public:
        Segment(const typename ElfTypes<B>::ProgramHeader* programHeader, const uint8_t* data)
                : mProgramHeader(programHeader), mData(data) {
        }

        const typename ElfTypes<B>::ProgramHeader* getHeader() const {
            return mProgramHeader;
        }

        const uint8_t* getData() const {
            return mData;
        }

    private:
        const typename ElfTypes<B>::ProgramHeader* mProgramHeader = nullptr;
        const uint8_t* mData = nullptr;
    };

public:
    explicit Reader(AccessManager* accessor): Reader(nullptr, accessor) {
    }
    Reader(BufferManager* bufferManager, AccessManager* accessor)
            : mBufferManager(bufferManager), mAccessManager(accessor) {
        VPUX_ELF_THROW_UNLESS(mAccessManager, ArgsError, "Accessor pointer is null");

        auto readBuffer(buildBufferFromMember(&mElfHeader));
        mAccessManager->readExternal(0, readBuffer);

        VPUX_ELF_THROW_UNLESS(utils::checkELFMagic(reinterpret_cast<const uint8_t*>(&mElfHeader)), HeaderError,
                              "Incorrect ELF magic");
        VPUX_ELF_THROW_UNLESS(sizeof(typename ElfTypes<B>::SectionHeader) == mElfHeader.e_shentsize, HeaderError,
                              "Mismatch between expected and received section header size");
        VPUX_ELF_THROW_UNLESS(mElfHeader.e_shoff >= sizeof(mElfHeader), HeaderError,
                              "Section table overlaps ELF header");
        VPUX_ELF_THROW_UNLESS(mElfHeader.e_shnum, HeaderError,
                              "No sections detected, ELF blob without sections is unsupported!");
        VPUX_ELF_THROW_UNLESS(mElfHeader.e_shstrndx < mElfHeader.e_shnum, HeaderError,
                              "Section name index exceeds section table");

        mSectionHeaders.resize(mElfHeader.e_shnum);
        readBuffer = buildBufferFromMember(&mSectionHeaders[0], mSectionHeaders.size() * sizeof(mSectionHeaders[0]));
        mAccessManager->readExternal(mElfHeader.e_shoff, readBuffer);

        if (mElfHeader.e_shstrndx) {
            const auto secNamesSection = mSectionHeaders[mElfHeader.e_shstrndx];

            VPUX_ELF_THROW_UNLESS(secNamesSection.sh_offset + secNamesSection.sh_size <= mAccessManager->getSize(),
                                  HeaderError, "Section name size exceeds buffer size");

            mSectionNames.resize(secNamesSection.sh_size);
            readBuffer = buildBufferFromMember(&mSectionNames[0], mSectionNames.size() * sizeof(mSectionNames[0]));
            mAccessManager->readExternal(secNamesSection.sh_offset, readBuffer);
        }
    }

    const typename ElfTypes<B>::ELFHeader* getHeader() const {
        return &mElfHeader;
    }

    size_t getSectionsNum() const {
        VPUX_ELF_THROW_UNLESS(mElfHeader.e_shnum <= 1000, ArgsError, "Invalid e_shnum");
        return mElfHeader.e_shnum;
    }

    size_t getSegmentsNum() const {
        VPUX_ELF_THROW_UNLESS(mElfHeader.e_phnum <= 1000, ArgsError, "Invalid e_phnum");
        return mElfHeader.e_phnum;
    }

    const Section& getSection(size_t index) const {
        VPUX_ELF_THROW_WHEN(index >= mElfHeader.e_shnum, RangeError, "Section index out of bounds");

        if (mSectionsCache.find(index) != mSectionsCache.end()) {
            return mSectionsCache[index];
        }

        const auto& secHeader = mSectionHeaders[index];
        const auto name = &mSectionNames[secHeader.sh_name];

        return mSectionsCache[index] = Section(mAccessManager, &secHeader, name);
    }

private:
    BufferManager* mBufferManager;
    AccessManager* mAccessManager;

    typename ElfTypes<B>::ELFHeader mElfHeader;
    std::vector<typename ElfTypes<B>::SectionHeader> mSectionHeaders;
    std::vector<typename ElfTypes<B>::ProgramHeader> mProgramHeaders;
    std::vector<char> mSectionNames;

    mutable std::unordered_map<size_t, Section> mSectionsCache;

    template <typename T>
    StaticBuffer buildBufferFromMember(T* member, size_t byteSize = sizeof(T)) {
        return StaticBuffer(reinterpret_cast<uint8_t*>(member), BufferSpecs(0, byteSize, 0));
    }
};

}  // namespace elf

//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#include <cstring>

#include <memory>
#include <vpux_loader/vpux_loader.hpp>
#include "vpux_elf/types/section_header.hpp"
#include "vpux_headers/buffer_specs.hpp"
#include "vpux_headers/device_buffer_container.hpp"
#include "vpux_headers/managed_buffer.hpp"

#ifndef VPUX_ELF_LOG_UNIT_NAME
#define VPUX_ELF_LOG_UNIT_NAME "VpuxLoader"
#endif
#include <vpux_elf/reader.hpp>

namespace elf {

namespace {

const uint32_t LO_21_BIT_MASK = 0x001F'FFFF;
const uint32_t B21_B26_MASK = 0x07E0'0000;

template <typename T>
void safeGet(T* dst, const T* src) {
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "copying to %p from %p amount %u", dst, src, sizeof(T));
    memcpy(reinterpret_cast<void*>(dst), reinterpret_cast<const void*>(src), sizeof(T));
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "copy done");
    return;
}

const uint32_t ADDRESS_MASK = ~0x00C0'0000u;
const uint64_t SLICE_LENGTH = 2 * 1024 * 1024;

uint32_t to_dpu_multicast(uint32_t addr, unsigned int& offset1, unsigned int& offset2, unsigned int& offset3) {
    const uint32_t bare_ptr = addr & ADDRESS_MASK;
    const uint32_t broadcast_mask = (addr & ~ADDRESS_MASK) >> 20;

    static const unsigned short multicast_masks[16] = {
            0x0000, 0x0001, 0x0002, 0x0003, 0x0012, 0x0011, 0x0010, 0x0030,
            0x0211, 0x0210, 0x0310, 0x0320, 0x3210, 0x3210, 0x3210, 0x3210,
    };

    VPUX_ELF_THROW_UNLESS(broadcast_mask < 16, RangeError, "Broadcast mask out of range");
    const unsigned short multicast_mask = multicast_masks[broadcast_mask];

    VPUX_ELF_THROW_UNLESS(multicast_mask != 0xffff, RangeError, "Got an invalid multicast mask");

    unsigned int base_mask = (static_cast<unsigned int>(multicast_mask) & 0xf) << 20;
    offset1 *= (multicast_mask >> 4) & 0xf;
    offset2 *= (multicast_mask >> 8) & 0xf;
    offset3 *= (multicast_mask >> 12) & 0xf;

    return bare_ptr | base_mask;
}

uint32_t to_dpu_multicast_base(uint32_t addr) {
    unsigned int offset1 = 0;
    unsigned int offset2 = 0;
    unsigned int offset3 = 0;
    return to_dpu_multicast(addr, offset1, offset2, offset3);
}

const auto VPU_16_BIT_SUM_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                          const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint16_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t16Bit SUM reloc, addr %p addrVal 0x%x symVal 0x%llx addend %llu", addr,
                 *addr, symVal, addend);

    *addr += static_cast<uint16_t>(symVal + addend);
};

const auto VPU_64_BIT_MULT_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                           const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint64_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t64Bit MULT reloc, addr %p addrVal 0x%x symVal 0x%llx addend %llu", addr,
                 *addr, symVal, addend);

    *addr *= static_cast<uint64_t>(symVal);
};

const auto VPU_64_BIT_MULT_SUB_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                               const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint64_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t64Bit MULT after SUB reloc, addr %p addrVal 0x%x symVal 0x%llx addend %llu",
                 addr, *addr, symVal, addend);

    *addr *= static_cast<int64_t>(addend) - static_cast<int64_t>(symVal);
};

const auto VPU_64_BIT_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                      const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint64_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t64Bit Reloc addr %p symval 0x%llx addnd %llu", addr, symVal, addend);

    *addr = symVal + addend;
};

const auto VPU_64_BIT_OR_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                         const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint64_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t64Bit OR reloc, addr %p addrVal 0x%llx symVal 0x%llx addend %llu", addr,
                 *addr, symVal, addend);

    *addr |= symVal + addend;
};

const auto VPU_64_BIT_LSHIFT_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                             const Elf_Sxword addend) -> void {
    (void)addend;  // hush compiler warning;
    auto addr = reinterpret_cast<uint64_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t64Bit LSHIFT reloc, addr %p addrVal 0x%llx symVal 0x%llx", addr, *addr,
                 symVal);

    *addr <<= symVal;
};

const auto VPU_DISP40_RTM_RELOCATION = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                          const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint64_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    auto symSize = targetSym.st_size;
    uint64_t mask = 0xffffffffff;
    uint64_t maskedAddr = *addr & mask;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\tDSIP40 reloc, addr %p symVal 0x%llx symSize %llu addend %llu", addr, symVal,
                 symSize, addend);

    *addr |= (symVal + (addend * (maskedAddr & (symSize - 1)))) & mask;
};

const auto VPU_32_BIT_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                      const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t32Bit reloc, addr %p symVal 0x%llx addend %llu", addr, symVal, addend);

    *addr = static_cast<uint32_t>(symVal + addend);
};

const auto VPU_32_BIT_RTM_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                          const Elf_Sxword addend) -> void {
    const auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    auto symSize = targetSym.st_size;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t32Bit RTM reloc, addr %p addrVal 0x%x symVal 0x%llx addend %llu", addr,
                 *addr, symVal, addend);

    *addr = static_cast<uint32_t>(symVal + (addend * (*addr & (symSize - 1))));
};

const auto VPU_32_BIT_SUM_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                          const Elf_Sxword addend) -> void {
    const auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t32Bit SUM reloc, addr %p addrVal 0x%x symVal 0x%llx addend %llu", addr,
                 *addr, symVal, addend);

    *addr += static_cast<uint32_t>(symVal + addend);
};

const auto VPU_32_MULTICAST_BASE_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                                 const Elf_Sxword addend) -> void {
    const auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t32Bit SUM reloc, addr %p addrVal 0x%x symVal 0x%llx addend %llu", addr,
                 *addr, symVal, addend);

    *addr = to_dpu_multicast_base(static_cast<uint32_t>(symVal + addend));
};

const auto VPU_32_MULTICAST_BASE_SUB_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                                     const Elf_Sxword addend) -> void {
    const auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t32Bit SUM reloc, addr %p addrVal 0x%x symVal 0x%llx addend %llu", addr,
                 *addr, symVal, addend);

    *addr = to_dpu_multicast_base(static_cast<uint32_t>(symVal + addend)) - *addr;
};

const auto VPU_DISP28_MULTICAST_OFFSET_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                                       const Elf_Sxword addend) -> void {
    const auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t32Bit SUM reloc, addr %p addrVal 0x%x symVal 0x%llx addend %llu", addr,
                 *addr, symVal, addend);

    unsigned int offs[3] = {SLICE_LENGTH >> 4, SLICE_LENGTH >> 4,
                            SLICE_LENGTH >> 4};  // 1024 * 1024 >> 4 as HW requirement
    to_dpu_multicast(static_cast<uint32_t>(symVal + addend), offs[0], offs[1], offs[2]);

    const auto index = *addr >> 4;
    *addr &= 0xf;
    *addr |= offs[index] << 4;
};

const auto VPU_DISP4_MULTICAST_OFFSET_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                                      const Elf_Sxword addend) -> void {
    const auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t32Bit SUM reloc, addr %p addrVal 0x%x symVal 0x%llx addend %llu", addr,
                 *addr, symVal, addend);

    unsigned int offs[3] = {SLICE_LENGTH >> 4, SLICE_LENGTH >> 4,
                            SLICE_LENGTH >> 4};  // 1024 * 1024 >> 4 as HW requirement
    to_dpu_multicast(static_cast<uint32_t>(symVal + addend), offs[0], offs[1], offs[2]);

    const auto index = *addr & 0xf;
    *addr &= 0xfffffff0;
    *addr |= offs[index] != 0;
};

const auto VPU_LO_21_BIT_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                         const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\tLow 21 bits reloc, addr %p symVal 0x%llx addend %llu", addr, symVal, addend);

    auto patchAddr = static_cast<uint32_t>(symVal + addend) & LO_21_BIT_MASK;
    *addr &= ~LO_21_BIT_MASK;
    *addr |= patchAddr;
};

const auto VPU_LO_21_BIT_SUM_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                             const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t32Bit Masked SUM reloc, addr %p symVal 0x%llx addend %llu", addr, symVal,
                 addend);

    auto patchAddr = static_cast<uint32_t>(symVal + addend) & LO_21_BIT_MASK;
    *addr += patchAddr;
};

const auto VPU_LO_21_BIT_MULTICAST_BASE_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                                        const Elf_Sxword addend) -> void {
    const auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t32Bit SUM reloc, addr %p addrVal 0x%x symVal 0x%llx addend %llu", addr,
                 *addr, symVal, addend);

    auto patchAddr = static_cast<uint32_t>(symVal + addend) & LO_21_BIT_MASK;
    *addr = to_dpu_multicast_base(patchAddr);
};

const auto VPU_16_BIT_LSB_21_RSHIFT_5_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                                      const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG,
                 "\t\t16Bit Reloc: Low 21 bits, rshift by 5 reloc, addr %p symVal 0x%llx addend %llu", addr, symVal,
                 addend);

    const uint32_t mask = 0x001F'FFFF;  // mask used to only keep last 21 bits
    const uint32_t lsb_16_mask = 0xFFFF;

    *addr &= ~lsb_16_mask;
    *addr |= (static_cast<uint32_t>(symVal + addend) & mask) >> 5;
};

const auto VPU_LO_21_BIT_RSHIFT_4_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                                  const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\tLow 21 bits, rshift 4 reloc, addr %p symVal 0x%llx addend %llu", addr,
                 symVal, addend);

    auto patchAddr = (static_cast<uint32_t>(symVal + addend) & LO_21_BIT_MASK) >> 4;
    *addr &= ~LO_21_BIT_MASK;
    *addr |= patchAddr;
};

const auto VPU_CMX_LOCAL_RSHIFT_5_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                                  const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\tCMX local rshift 5 reloc, addr %p addrVal 0x%x symVal 0x%llx addend %llu",
                 addr, *addr, symVal, addend);

    uint32_t CMX_TILE_SELECT_MASK = ~B21_B26_MASK;
    auto patchAddr = (static_cast<uint32_t>(symVal + addend) & CMX_TILE_SELECT_MASK) >> 5;
    *addr = patchAddr;
};

const auto VPU_32_BIT_OR_B21_B26_UNSET_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                                       const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG,
                 "\t\t32 bits OR reloc with b21-26 unset, addr %p, before value: 0x%x symVal 0x%llx addend %llu", addr,
                 *addr, symVal, addend);

    uint32_t B21_B26_UNSET_MASK = ~B21_B26_MASK;
    auto patchAddr = static_cast<uint32_t>(symVal + addend) & B21_B26_UNSET_MASK;
    *addr |= patchAddr;
};

const auto VPU_64_BIT_OR_B21_B26_UNSET_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                                       const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint64_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG,
                 "\t\t64 bits OR reloc with b21-26 unset, addr %p, before value: 0x%llx symVal 0x%llx addend %llu",
                 addr, *addr, symVal, addend);

    uint64_t B21_B26_UNSET_MASK = ~B21_B26_MASK;
    auto patchAddr = static_cast<uint64_t>(symVal + addend) & B21_B26_UNSET_MASK;
    *addr |= patchAddr;
};

const auto VPU_16_BIT_LSB_21_RSHIFT_5_LSHIFT_16_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                                                const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG,
                 "\t\t16Bit Reloc: Low 21 bits, rshift by 5 reloc, addr %p symVal 0x%llx addend %llu", addr, symVal,
                 addend);

    const uint32_t mask = 0x001F'FFFF;  // mask used to only keep last 21 bits
    const uint32_t msb_16_mask = 0xFFFF0000;

    *addr &= ~msb_16_mask;
    *addr |= ((static_cast<uint32_t>(symVal + addend) & mask) >> 5) << 16;
};

const auto VPU_16_BIT_LSB_21_RSHIFT_5_LSHIFT_CUSTOM_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                                                    const Elf_Sxword addend) -> void {
    // more details in ticket #E-97614
    auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(
            LogLevel::LOG_DEBUG,
            "\t\t16Bit Reloc preemtion workaround: Low 21 bits, rshift by 5 reloc, addr %p symVal 0x%llx addend %llu",
            addr, symVal, addend);

    const uint32_t mask = 0x001F'FFFF;                          // mask used to only keep last 21 bits
    const uint32_t preemtion_work_around_16_mask = 0xFFFE4000;  // 1111 1111 1111 1110 0100 0000 0000 0000

    *addr &= ~preemtion_work_around_16_mask;

    auto src_value = (static_cast<uint32_t>(symVal + addend) & mask) >> 5;
    // need to convert value from this view: 0000 0000 0000 0000 1111 1111 1111 1111
    // to                                    1111 1111 1111 1110 0100 0000 0000 0000

    // set [17:31] bits
    auto converted_value = (src_value & ~1) << 16;
    // format                                1111 1111 1111 1110 0000 0000 0000 0000

    // set [14] bit
    converted_value |= (src_value & 1) << 14;
    // format                                1111 1111 1111 1110 0100 0000 0000 0000

    *addr |= converted_value;
};

const auto VPU_32_BIT_OR_B21_B26_UNSET_HIGH_16_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                                               const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint16_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(
            LogLevel::LOG_DEBUG,
            "\t\t32 bits OR reloc with b21-26 unset and high 16, addr %p, before value: 0x%llx symVal 0x%x addend %llu",
            addr, *addr, symVal, addend);

    uint64_t B21_B26_UNSET_MASK = ~B21_B26_MASK;
    auto patchAddr = static_cast<uint32_t>(symVal + addend) & B21_B26_UNSET_MASK;
    *addr |= patchAddr >> 16;
};

const auto VPU_32_BIT_OR_B21_B26_UNSET_LOW_16_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                                              const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint16_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(
            LogLevel::LOG_DEBUG,
            "\t\t32 bits OR reloc with b21-26 unset and low 16, addr %p, before value: 0x%llx symVal 0x%x addend %llu",
            addr, *addr, symVal, addend);

    uint64_t B21_B26_UNSET_MASK = ~B21_B26_MASK;
    auto patchAddr = static_cast<uint16_t>(symVal + addend) & B21_B26_UNSET_MASK;
    *addr |= patchAddr & 0xFFFF;
};

const auto VPU_HIGH_27_BIT_OR_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym,
                                              const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint64_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\tHigh 27 bits reloc, addr %p addrVal 0x%llx  symVal 0x%llx addend %llu", addr,
                 *addr, symVal, addend);

    auto patchAddrUnsetTile = static_cast<uint32_t>(symVal + addend) &
                              ~0xE0'0000;
    auto patchAddr = (patchAddrUnsetTile >> 4) & (0x7FFF'FFFF >> 4);
    *addr |= (static_cast<uint64_t>(patchAddr) << 37);
};

}  // namespace

const std::map<Elf_Word, VPUXLoader::Action> VPUXLoader::actionMap = {
        {SHT_NULL, Action::None},
        {SHT_PROGBITS, Action::AllocateAndLoad},
        {SHT_SYMTAB, Action::RegisterUserIO},
        {SHT_STRTAB, Action::None},
        {SHT_RELA, Action::Relocate},
        {SHT_HASH, Action::Error},
        {SHT_DYNAMIC, Action::Error},
        {SHT_NOTE, Action::None},
        {SHT_NOBITS, Action::Allocate},
        {SHT_REL, Action::Error},
        {SHT_SHLIB, Action::Error},
        {SHT_DYNSYM, Action::Error},
        {VPU_SHT_NETDESC, Action::None},
        {VPU_SHT_PROF, Action::None},
        {VPU_SHT_CMX_METADATA, Action::None},
        {VPU_SHT_CMX_WORKSPACE, Action::None},
        {VPU_SHT_PLATFORM_INFO, Action::None},
        {VPU_SHT_PERF_METRICS, Action::None},
};

const std::map<VPUXLoader::RelocationType, VPUXLoader::RelocationFunc> VPUXLoader::relocationMap = {
        {R_VPU_64, VPU_64_BIT_Relocation},
        {R_VPU_16_SUM, VPU_16_BIT_SUM_Relocation},
        {R_VPU_64_MULT, VPU_64_BIT_MULT_Relocation},
        {R_VPU_64_MULT_SUB, VPU_64_BIT_MULT_SUB_Relocation},
        {R_VPU_64_OR, VPU_64_BIT_OR_Relocation},
        {R_VPU_DISP40_RTM, VPU_DISP40_RTM_RELOCATION},
        {R_VPU_64_LSHIFT, VPU_64_BIT_LSHIFT_Relocation},
        {R_VPU_32, VPU_32_BIT_Relocation},
        {R_VPU_32_RTM, VPU_32_BIT_RTM_Relocation},
        {R_VPU_32_SUM, VPU_32_BIT_SUM_Relocation},
        {R_VPU_32_MULTICAST_BASE, VPU_32_MULTICAST_BASE_Relocation},
        {R_VPU_32_MULTICAST_BASE_SUB, VPU_32_MULTICAST_BASE_SUB_Relocation},
        {R_VPU_DISP28_MULTICAST_OFFSET, VPU_DISP28_MULTICAST_OFFSET_Relocation},
        {R_VPU_DISP4_MULTICAST_OFFSET_CMP, VPU_DISP4_MULTICAST_OFFSET_Relocation},
        {R_VPU_LO_21, VPU_LO_21_BIT_Relocation},
        {R_VPU_LO_21_SUM, VPU_LO_21_BIT_SUM_Relocation},
        {R_VPU_LO_21_MULTICAST_BASE, VPU_LO_21_BIT_MULTICAST_BASE_Relocation},
        {R_VPU_16_LSB_21_RSHIFT_5, VPU_16_BIT_LSB_21_RSHIFT_5_Relocation},
        {R_VPU_LO_21_RSHIFT_4, VPU_LO_21_BIT_RSHIFT_4_Relocation},
        {R_VPU_CMX_LOCAL_RSHIFT_5, VPU_CMX_LOCAL_RSHIFT_5_Relocation},
        {R_VPU_32_BIT_OR_B21_B26_UNSET, VPU_32_BIT_OR_B21_B26_UNSET_Relocation},
        {R_VPU_64_BIT_OR_B21_B26_UNSET, VPU_64_BIT_OR_B21_B26_UNSET_Relocation},
        {R_VPU_16_LSB_21_RSHIFT_5_LSHIFT_16, VPU_16_BIT_LSB_21_RSHIFT_5_LSHIFT_16_Relocation},
        {R_VPU_16_LSB_21_RSHIFT_5_LSHIFT_CUSTOM, VPU_16_BIT_LSB_21_RSHIFT_5_LSHIFT_CUSTOM_Relocation},
        {R_VPU_32_BIT_OR_B21_B26_UNSET_HIGH_16, VPU_32_BIT_OR_B21_B26_UNSET_HIGH_16_Relocation},
        {R_VPU_32_BIT_OR_B21_B26_UNSET_LOW_16, VPU_32_BIT_OR_B21_B26_UNSET_LOW_16_Relocation},
        {R_VPU_HIGH_27_BIT_OR, VPU_HIGH_27_BIT_OR_Relocation},
};

VPUXLoader::VPUXLoader(AccessManager* accessor, BufferManager* bufferManager)
        : m_inferBufferContainer(bufferManager),
          m_backupBufferContainer(bufferManager),
          m_relocationSectionIndexes(std::make_shared<std::vector<std::size_t>>()),
          m_jitRelocations(std::make_shared<std::vector<std::size_t>>()),
          m_userInputsDescriptors(std::make_shared<std::vector<DeviceBuffer>>()),
          m_userOutputsDescriptors(std::make_shared<std::vector<DeviceBuffer>>()),
          m_profOutputsDescriptors(std::make_shared<std::vector<DeviceBuffer>>()),
          m_loaded(false) {
    VPUX_ELF_THROW_UNLESS(bufferManager, ArgsError, "Invalid BufferManager pointer");
    m_bufferManager = bufferManager;
    m_reader = std::make_shared<Reader<ELF_Bitness::Elf64>>(m_bufferManager, accessor);
    m_sectionMap = std::make_shared<std::map<elf::Elf_Word /*section type*/, std::vector<size_t>>>();

    VPUX_ELF_LOG(LogLevel::LOG_TRACE, "Initializing... Register sections");
    auto numSections = m_reader->getSectionsNum();
    for (size_t sectionCtr = 0; sectionCtr < numSections; ++sectionCtr) {
        auto section = m_reader->getSection(sectionCtr);
        auto sectionType = section.getHeader()->sh_type;

        (*m_sectionMap)[sectionType].emplace_back(sectionCtr);
        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "[%lu] Section name: %s", sectionCtr, section.getName());

        // Early fetch of IO buffer specs
        const auto action = actionMap.find(sectionType);
        if (action->second == Action::RegisterUserIO) {
            earlyFetchIO(section);
        }
    }

    // accomodate missing section due to compatibility with older ELFs
    if (m_sectionMap->find(elf::VPU_SHT_PERF_METRICS) == m_sectionMap->end()) {
        (*m_sectionMap)[elf::VPU_SHT_PERF_METRICS] = {};
    }
};

VPUXLoader::VPUXLoader(const VPUXLoader& other)
        : m_bufferManager(other.m_bufferManager),
          m_reader(other.m_reader),
          m_inferBufferContainer(other.m_inferBufferContainer),
          m_backupBufferContainer(other.m_backupBufferContainer),
          m_runtimeSymTabs(other.m_runtimeSymTabs),
          m_relocationSectionIndexes(other.m_relocationSectionIndexes),
          m_jitRelocations(other.m_jitRelocations),
          m_userInputsDescriptors(other.m_userInputsDescriptors),
          m_userOutputsDescriptors(other.m_userOutputsDescriptors),
          m_profOutputsDescriptors(other.m_profOutputsDescriptors),
          m_sectionMap(other.m_sectionMap),
          m_symTabOverrideMode(other.m_symTabOverrideMode),
          m_explicitAllocations(other.m_explicitAllocations),
          m_loaded(other.m_loaded),
          m_symbolSectionTypes(other.m_symbolSectionTypes) {
    reloadNewBuffers();
    applyRelocations(*m_relocationSectionIndexes);
}

// override the symbol table for the newly created loader
VPUXLoader::VPUXLoader(const VPUXLoader& other, const std::vector<SymbolEntry>& runtimeSymTabs)
        : m_bufferManager(other.m_bufferManager),
          m_reader(other.m_reader),
          m_inferBufferContainer(other.m_inferBufferContainer),
          m_backupBufferContainer(other.m_backupBufferContainer),
          m_runtimeSymTabs(runtimeSymTabs),
          m_relocationSectionIndexes(other.m_relocationSectionIndexes),
          m_jitRelocations(other.m_jitRelocations),
          m_userInputsDescriptors(other.m_userInputsDescriptors),
          m_userOutputsDescriptors(other.m_userOutputsDescriptors),
          m_profOutputsDescriptors(other.m_profOutputsDescriptors),
          m_sectionMap(other.m_sectionMap),
          m_symTabOverrideMode(other.m_symTabOverrideMode),
          m_explicitAllocations(other.m_explicitAllocations),
          m_loaded(other.m_loaded),
          m_symbolSectionTypes(other.m_symbolSectionTypes) {
    reloadNewBuffers();
    applyRelocations(*m_relocationSectionIndexes);
}

VPUXLoader& VPUXLoader::operator=(const VPUXLoader& other) {
    if (this == &other) {
        return *this;
    }

    m_bufferManager = other.m_bufferManager;
    m_reader = other.m_reader;
    m_inferBufferContainer = other.m_inferBufferContainer;
    m_runtimeSymTabs = other.m_runtimeSymTabs;
    m_relocationSectionIndexes = other.m_relocationSectionIndexes;
    m_jitRelocations = other.m_jitRelocations;
    m_userInputsDescriptors = other.m_userInputsDescriptors;
    m_userOutputsDescriptors = other.m_userOutputsDescriptors;
    m_profOutputsDescriptors = other.m_profOutputsDescriptors;
    m_symTabOverrideMode = other.m_symTabOverrideMode;
    m_explicitAllocations = other.m_explicitAllocations;
    m_symbolSectionTypes = other.m_symbolSectionTypes;
    m_sectionMap = other.m_sectionMap;
    m_loaded = other.m_loaded;

    reloadNewBuffers();
    applyRelocations(*m_relocationSectionIndexes);

    return *this;
}

VPUXLoader::~VPUXLoader() {
}

elf::DeviceBufferContainer::BufferPtr VPUXLoader::getEntry() {
    auto numSections = m_reader->getSectionsNum();

    for (size_t sectionCtr = 0; sectionCtr < numSections; ++sectionCtr) {
        const auto& section = m_reader->getSection(sectionCtr);

        auto hdr = section.getHeader();
        if (hdr->sh_type == elf::SHT_SYMTAB) {
            auto symTabsSize = section.getEntriesNum();
            auto symTabs = section.getData<elf::SymbolEntry>();

            for (size_t symTabIdx = 0; symTabIdx < symTabsSize; ++symTabIdx) {
                auto& symTab = symTabs[symTabIdx];
                auto symType = elf64STType(symTab.st_info);
                if (symType == VPU_STT_ENTRY) {
                    auto secIndx = symTab.st_shndx;
                    return m_inferBufferContainer.getBufferInfoFromIndex(secIndx).mBuffer;
                }
            }
        }
    }

    VPUX_ELF_THROW(ImplausibleState, "Can not continue without entry!");
    return {};
}

void VPUXLoader::load(const std::vector<SymbolEntry>& runtimeSymTabs, bool symTabOverrideMode,
                      const std::vector<elf::Elf_Word>& symbolSectionTypes) {
    VPUX_ELF_THROW_WHEN(m_loaded, SequenceError, "Sections were previously loaded.");

    m_runtimeSymTabs = runtimeSymTabs;
    m_symTabOverrideMode = symTabOverrideMode;
    m_explicitAllocations = symTabOverrideMode;
    m_symbolSectionTypes = symbolSectionTypes;

    VPUX_ELF_LOG(LogLevel::LOG_TRACE, "Starting LOAD process");
    auto numSections = m_reader->getSectionsNum();

    m_relocationSectionIndexes->reserve(numSections);
    m_jitRelocations->reserve(2);

    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "Got elf with %zu sections", numSections);
    for (size_t sectionCtr = 0; sectionCtr < numSections; ++sectionCtr) {
        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "Solving section %zu", sectionCtr);

        const auto& section = m_reader->getSection(sectionCtr);

        const auto sectionHeader = section.getHeader();
        auto sectionType = sectionHeader->sh_type;
        auto searchAction = actionMap.find(sectionType);
        auto action = Action::None;

        if (searchAction == actionMap.end()) {
            if (sectionType >= elf::SHT_LOUSER && sectionType <= elf::SHT_HIUSER) {
                VPUX_ELF_LOG(LogLevel::LOG_WARN, "Unrecognized Section Type in User range %x", sectionType);
            } else {
                VPUX_ELF_THROW(ImplausibleState, "Unrecognized Section Type outside of User range");
            }
        } else {
            action = searchAction->second;
        }

        auto sectionFlags = sectionHeader->sh_flags;

        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "    name  : %s", section.getName());
        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "    type  : %u", sectionType);
        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "    flags : 0x%llx", sectionFlags);
        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "    action: %u", (uint32_t)action);

        switch (action) {
        case Action::AllocateAndLoad: {
            bool isAllocateable = sectionFlags & SHF_ALLOC;
            if (m_explicitAllocations && !isAllocateable) {
                break;
            }

            VPUX_ELF_LOG(LogLevel::LOG_TRACE, "Allocate and loading %zu", sectionCtr);

            // Shared condition:
            //  1. has data
            //  2. is read only
            //  3. not target of a relocation section
            //
            // Condition 1 is fulfilled by entering this case
            // Condition 2 can be checked here
            // Condition 3 needs to be checked after all relocation sections have been registered in order to be
            // independent from the sections order inside the ELF binary

            auto& inferBufferInfo = m_inferBufferContainer.safeInitBufferInfoAtIndex(sectionCtr);
            inferBufferInfo.mBufferDetails.mHasData = true;
            inferBufferInfo.mBufferDetails.mIsShared = sectionFlags & SHF_WRITE ? false : true;
            inferBufferInfo.mBufferDetails.mIsProcessed = false;

            VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\tLoaded section %s (address: %p, size: %llu)", section.getName(),
                         inferBufferInfo.mBuffer->getBuffer().cpu_addr(), inferBufferInfo.mBuffer->getBuffer().size());
            break;
        }

        case Action::Allocate: {
            bool isAllocateable = sectionFlags & SHF_ALLOC;
            if ((m_explicitAllocations && !isAllocateable) || utils::isNetworkIO(sectionFlags)) {
                break;
            }

            VPUX_ELF_LOG(LogLevel::LOG_TRACE, "Allocating %zu", sectionCtr);

            auto sectionSize = sectionHeader->sh_size;
            auto sectionAlignment = sectionHeader->sh_addralign;

            auto& inferBufferInfo = m_inferBufferContainer.safeInitBufferInfoAtIndex(sectionCtr);
            inferBufferInfo.mBuffer = m_inferBufferContainer.buildAllocatedDeviceBuffer(
                    BufferSpecs(sectionAlignment, sectionSize, sectionFlags));
            inferBufferInfo.mBufferDetails.mHasData = false;
            inferBufferInfo.mBufferDetails.mIsShared = false;
            inferBufferInfo.mBufferDetails.mIsProcessed = true;

            VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\tFor section %s Allocated %p of size %llu", section.getName(),
                         inferBufferInfo.mBuffer->getBuffer().cpu_addr(), sectionSize);
            break;
        }

        case Action::Relocate: {
            if (sectionFlags & VPU_SHF_JIT) {
                // Trigger read of section data so that after load completes the AccessManager object can
                // be safely deleted
                section.getData<void>();

                VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "Registering JIT Relocation %zu", sectionCtr);
                m_jitRelocations->push_back(static_cast<int>(sectionCtr));
            } else {
                m_relocationSectionIndexes->push_back(static_cast<int>(sectionCtr));
                VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "Registering Relocation %zu", sectionCtr);
            }
            break;
        }

        case Action::Error: {
            VPUX_ELF_THROW(SectionError, "Unexpected section type");
            return;
        }

        case Action::RegisterUserIO: {
            // Trigger read of section data so that after load completes the AccessManager object can
            // be safely deleted
            section.getData<void>();
            break;
        }
        case Action::None: {
            break;
        }

        default: {
            VPUX_ELF_THROW(ImplausibleState, "Unrecognized Section Type outside of User range");
            return;
        }
        }
    }

    // Now that all relocation sections are known, check shared condition 3
    updateSharedBuffers(*m_relocationSectionIndexes);
    updateSharedBuffers(*m_jitRelocations);

    // Load actual buffers for the first time
    loadBuffers();

    applyRelocations(*m_relocationSectionIndexes);

    VPUX_ELF_LOG(LogLevel::LOG_INFO, "Allocated %zu sections", m_inferBufferContainer.getBufferInfoCount());

    // sections were loaded. other calls to this method will throw an error
    m_loaded = true;

    return;
}

void VPUXLoader::updateSharedBuffers(const std::vector<std::size_t>& relocationSectionIndexes) {
    VPUX_ELF_LOG(LogLevel::LOG_TRACE, "Update shared buffers");

    // First, exclude all relocation targets from shared pool
    for (const auto& relocationSectionIdx : relocationSectionIndexes) {
        const auto& relocSection = m_reader->getSection(relocationSectionIdx);
        const auto relocSecHdr = relocSection.getHeader();
        const auto relocSecFlags = relocSecHdr->sh_flags;
        Elf_Word targetSectionIdx;
        if (relocSecFlags & SHF_INFO_LINK) {
            targetSectionIdx = relocSecHdr->sh_info;
        } else {
            VPUX_ELF_THROW(RelocError, "Rela section with no target section");
            return;
        }
        VPUX_ELF_LOG(LogLevel::LOG_TRACE, "Processing buffer for section %zu", targetSectionIdx);
        VPUX_ELF_THROW_WHEN(targetSectionIdx == 0 || targetSectionIdx > m_reader->getSectionsNum(), RelocError,
                            "invalid target section from rela section");

        // Line below should throw if buffer container does not have by this point a valid buffer associated with the
        // target relocation section
        auto& inferBufferInfo = m_inferBufferContainer.getBufferInfoFromIndex(targetSectionIdx);

        if (!inferBufferInfo.mBufferDetails.mIsProcessed) {
            inferBufferInfo.mBufferDetails.mIsShared = false;
        } else {
            VPUX_ELF_LOG(LogLevel::LOG_TRACE, "Buffer for section %zu is already processed", targetSectionIdx);
        }
    }
}

void VPUXLoader::loadBuffers() {
    // Now actually create and load buffers
    for (auto& elem : m_inferBufferContainer) {
        auto bufferIndex = elem.first;
        auto& bufferInfo = elem.second;

        if (!bufferInfo.mBufferDetails.mIsProcessed) {
            auto& section = m_reader->getSection(bufferIndex);

            if (bufferInfo.mBufferDetails.mIsShared) {
                bufferInfo.mBuffer = m_reader->getSection(bufferIndex).getDataBuffer();
            } else {
                // Initialize backup buffer info
                auto& backupBufferInfo = m_backupBufferContainer.safeInitBufferInfoAtIndex(bufferIndex);

                // Get actual backup buffer with CPU-only access
                backupBufferInfo.mBuffer = m_reader->getSection(bufferIndex).getDataBuffer(true);
                auto backupBufferLock = ElfBufferLockGuard(backupBufferInfo.mBuffer.get());
                // Explicitly allocate a new NPU-access buffer
                auto bufferSpecs = backupBufferInfo.mBuffer->getBufferSpecs();
                bufferSpecs.procFlags = section.getHeader()->sh_flags;
                bufferInfo.mBuffer = m_inferBufferContainer.buildAllocatedDeviceBuffer(bufferSpecs);

                // Copy data from backup to infer buffer
                bufferInfo.mBuffer->loadWithLock(backupBufferInfo.mBuffer->getBuffer().cpu_addr(),
                                                 backupBufferInfo.mBuffer->getBuffer().size());

                backupBufferInfo.mBufferDetails.mHasData = true;
                backupBufferInfo.mBufferDetails.mIsShared = true;
                backupBufferInfo.mBufferDetails.mIsProcessed = true;
            }

            bufferInfo.mBufferDetails.mIsProcessed = true;
        }
    }
}

void VPUXLoader::reloadNewBuffers() {
    for (const auto& buffer : m_inferBufferContainer) {
        auto& sectionIndex = buffer.first;
        auto& inferBufferInfo = buffer.second;
        if (inferBufferInfo.mBufferDetails.mHasData && !inferBufferInfo.mBufferDetails.mIsShared) {
            auto& backupBufferInfo = m_backupBufferContainer.getBufferInfoFromIndex(sectionIndex);
            auto backupBufferLock = ElfBufferLockGuard(backupBufferInfo.mBuffer.get());

            VPUX_ELF_THROW_UNLESS(
                    backupBufferInfo.mBuffer->getBuffer().size() == inferBufferInfo.mBuffer->getBuffer().size(),
                    RuntimeError, "Mismatch between section backup size and allocated device buffer size");
            inferBufferInfo.mBuffer->loadWithLock(backupBufferInfo.mBuffer->getBuffer().cpu_addr(),
                                                  inferBufferInfo.mBuffer->getBuffer().size());
            VPUX_ELF_LOG(LogLevel::LOG_TRACE, "Loading with lock %llu bytes from %p to %p",
                         inferBufferInfo.mBuffer->getBuffer().size(), backupBufferInfo.mBuffer->getBuffer().cpu_addr(),
                         inferBufferInfo.mBuffer->getBuffer().cpu_addr());
        }
    }
}

void VPUXLoader::applyRelocations(const std::vector<std::size_t>& relocationSectionIndexes) {
    VPUX_ELF_LOG(LogLevel::LOG_TRACE, "apply relocations");
    for (const auto& relocationSectionIdx : relocationSectionIndexes) {
        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "applying relocation section %u", relocationSectionIdx);

        const auto& relocSection = m_reader->getSection(relocationSectionIdx);
        auto relocations = relocSection.getData<elf::RelocationAEntry>();
        auto relocSecHdr = relocSection.getHeader();
        auto numRelocs = relocSection.getEntriesNum();

        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\tRelA section with %zu elements at addr %p", numRelocs, relocations);
        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\tRelA section info, link flags 0x%x %u 0x%llx", relocSecHdr->sh_info,
                     relocSecHdr->sh_link, relocSecHdr->sh_flags);

        // At this point we assume that all the section indexes passed to this method
        // are containing a section of sh_type == SHT_RELA. So, the sh_link
        // must point only to a section header index of the associated symbol table or to the reserved
        // symbol range of sections.
        auto symTabIdx = relocSecHdr->sh_link;
        VPUX_ELF_THROW_UNLESS((symTabIdx < m_reader->getSectionsNum() || (symTabIdx == VPU_RT_SYMTAB)), RangeError,
                              "sh_link exceeds the number of entries.")

        // by convention, we will assume symTabIdx==VPU_RT_SYMTAB to be the "built-in" symtab
        auto getSymTab = [&](size_t& symTabEntries) -> const SymbolEntry* {
            if (symTabIdx == VPU_RT_SYMTAB) {
                return m_runtimeSymTabs.data();
            }

            const auto& symTabSection = m_reader->getSection(symTabIdx);
            auto symTabSectionHdr = symTabSection.getHeader();
            symTabEntries = symTabSection.getEntriesNum();

            VPUX_ELF_THROW_UNLESS(checkSectionType(symTabSectionHdr, elf::SHT_SYMTAB), RelocError,
                                  "Reloc section pointing to snon-symtab");

            return symTabSection.getData<elf::SymbolEntry>();
        };

        size_t symTabEntries = 0;
        auto symTabs = getSymTab(symTabEntries);

        auto relocSecFlags = relocSecHdr->sh_flags;
        Elf_Word targetSectionIdx = 0;
        if (relocSecFlags & SHF_INFO_LINK) {
            targetSectionIdx = relocSecHdr->sh_info;
        } else {
            VPUX_ELF_THROW(RelocError,
                           "Rela section with no target section");
            return;
        }

        VPUX_ELF_THROW_WHEN(targetSectionIdx == 0 || targetSectionIdx > m_reader->getSectionsNum(), RelocError,
                            "invalid target section from rela section");

        auto targetSection = m_reader->getSection(targetSectionIdx);

        // at this point we assume that all sections have an address, to which we can apply a simple lookup
        auto& targetSectionBuf = m_inferBufferContainer.getBufferInfoFromIndex(targetSectionIdx).mBuffer;
        auto targetSectionLock = ElfBufferLockGuard(targetSectionBuf.get());

        auto targetSectionAddr = targetSectionBuf->getBuffer().cpu_addr();
        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "Relocations are targeting section at addr %p named %s", targetSectionAddr,
                     targetSection.getName());

        // apply the actual relocations
        for (size_t relocIdx = 0; relocIdx < numRelocs; ++relocIdx) {
            const elf::RelocationAEntry& relocation = relocations[relocIdx];

            auto relOffset = relocation.r_offset;

            VPUX_ELF_THROW_UNLESS(relOffset < targetSectionBuf->getBuffer().size(), RelocError,
                                  "RelocOffset outside of the section size");

            auto relSymIdx = elf64RSym(relocation.r_info);

            // there are two types of relocation that can be suported at this point
            //    - special relocation that would use the runtime symbols
            // received from the user
            //    - relocations on the symbols defined in the symbol table inside the ELF file.
            // In this case the section has a specific number of entries (need to use the getEntriesNum method of
            // this section)
            VPUX_ELF_THROW_WHEN((relSymIdx > symTabEntries && symTabIdx != VPU_RT_SYMTAB) ||
                                        (relSymIdx > m_runtimeSymTabs.size() && symTabIdx == VPU_RT_SYMTAB),
                                RelocError, "SymTab index out of bounds!");

            auto relType = elf64RType(relocation.r_info);
            auto addend = relocation.r_addend;

            auto reloc = relocationMap.find(static_cast<RelocationType>(relType));
            VPUX_ELF_THROW_WHEN(reloc == relocationMap.end() || reloc->second == nullptr, RelocError,
                                "Invalid relocation type detected");

            auto relocFunc = reloc->second;

            // the actual data that we need to modify
            auto relocationTargetAddr = targetSectionAddr + relOffset;

            // deliberate copy so we don't modify the contents of the original elf.
            elf::SymbolEntry targetSymbol = symTabs[relSymIdx];
            auto symbolTargetSectionIdx = targetSymbol.st_shndx;

            uint64_t symValue = 0;
            if (m_inferBufferContainer.hasBufferInfoAtIndex(symbolTargetSectionIdx)) {
                symValue = m_inferBufferContainer.getBufferInfoFromIndex(symbolTargetSectionIdx)
                                   .mBuffer->getBuffer()
                                   .vpu_addr();
            }
            if (symValue || symTabIdx == VPU_RT_SYMTAB) {
                targetSymbol.st_value += symValue;
            } else {
                auto sectionType = m_reader->getSection(symbolTargetSectionIdx).getHeader()->sh_type;

                size_t index = -1;
                for (index = 0; index < m_symbolSectionTypes.size(); ++index) {
                    if (m_symbolSectionTypes[index] == sectionType) {
                        break;
                    }
                }

                // error if index still -1

                targetSymbol = m_runtimeSymTabs[index];
            }

            VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\tApplying Relocation at offset %llu symidx %u reltype %u addend %llu",
                         relOffset, relSymIdx, relType, addend);

            relocFunc((void*)relocationTargetAddr, targetSymbol, addend);
        }
    }

    return;
};

void VPUXLoader::applyJitRelocations(std::vector<DeviceBuffer>& inputs, std::vector<DeviceBuffer>& outputs,
                                     std::vector<DeviceBuffer>& profiling) {
    VPUX_ELF_LOG(LogLevel::LOG_TRACE, "apply JITrelocations");
    for (const auto& relocationSectionIdx : *m_jitRelocations) {
        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\tapplying JITrelocation section %u", relocationSectionIdx);

        const auto& relocSection = m_reader->getSection(relocationSectionIdx);
        auto relocations = relocSection.getData<elf::RelocationAEntry>();
        auto relocSecHdr = relocSection.getHeader();
        auto numRelocs = relocSection.getEntriesNum();

        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\tJitRelA section with %zu elements at addr %p", numRelocs, relocations);
        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\tJitRelA section info, link flags 0x%x %u 0x%llx", relocSecHdr->sh_info,
                     relocSecHdr->sh_link, relocSecHdr->sh_flags);

        // At this point we assume that all the section indexes passed to this method
        // are containing a section of sh_type == SHT_RELA. So, the sh_link
        // must point only to a section header index of the associated symbol table.
        auto symTabIdx = relocSecHdr->sh_link;
        VPUX_ELF_THROW_UNLESS(symTabIdx < m_reader->getSectionsNum(), RangeError,
                              "sh_link exceeds the number of entries.");

        // in JitRelocations case, we will expect to point to either "VPUX_USER_INPUT" or "VPUX_USER_INPUT" symtabs
        VPUX_ELF_THROW_WHEN(symTabIdx == VPU_RT_SYMTAB, RelocError, "JitReloc pointing to runtime symtab idx");

        const auto& symTabSection = m_reader->getSection(symTabIdx);
        auto symTabSectionHdr = symTabSection.getHeader();

        VPUX_ELF_THROW_UNLESS(checkSectionType(symTabSectionHdr, elf::SHT_SYMTAB), RelocError,
                              "Reloc section pointing to non-symtab");

        auto symTabSize = symTabSection.getEntriesNum();
        auto symTabs = symTabSection.getData<elf::SymbolEntry>();

        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\tSymTabIdx %u symTabSize %zu at %p", symTabIdx, symTabSize, symTabs);

        auto relocSecFlags = relocSecHdr->sh_flags;
        auto getUserAddrs = [&]() -> std::vector<DeviceBuffer> {
            if (relocSecFlags & VPU_SHF_USERINPUT) {
                return std::vector<DeviceBuffer>(inputs);
            } else if (relocSecFlags & VPU_SHF_USEROUTPUT) {
                return std::vector<DeviceBuffer>(outputs);
            } else if (relocSecFlags & VPU_SHF_PROFOUTPUT) {
                return std::vector<DeviceBuffer>(profiling);
            } else {
                VPUX_ELF_THROW(RelocError, "Jit reloc section pointing neither to userInput nor userOutput");
                return std::vector<DeviceBuffer>(outputs);
            }
        };

        auto userAddrs = getUserAddrs();

        Elf_Word targetSectionIdx = 0;
        if (relocSecFlags & SHF_INFO_LINK) {
            targetSectionIdx = relocSecHdr->sh_info;
        } else {
            VPUX_ELF_THROW(
                    RelocError,
                    "Rela section with no target section");
            return;
        }

        // at this point we assume that all sections have an address, to which we can apply a simple lookup
        auto targetSectionBuf = m_inferBufferContainer.getBufferInfoFromIndex(targetSectionIdx).mBuffer;
        auto targetSectionLock = ElfBufferLockGuard(targetSectionBuf.get());

        auto targetSectionAddr = targetSectionBuf->getBuffer().cpu_addr();

        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t targetSectionAddr %p", targetSectionAddr);

        // apply the actual relocations
        for (size_t relocIdx = 0; relocIdx < numRelocs; ++relocIdx) {
            VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t Solving Reloc at %p %zu", relocations, relocIdx);

            const elf::RelocationAEntry& relocation = relocations[relocIdx];

            auto relOffset = relocation.r_offset;

            VPUX_ELF_THROW_UNLESS(relOffset < targetSectionBuf->getBuffer().size(), RelocError,
                                  "RelocOffset outside of the section size");

            auto symIdx = elf64RSym(relocation.r_info);

            VPUX_ELF_THROW_WHEN(symIdx > symTabSize, RelocError, "SymTab index out of bounds!");
            VPUX_ELF_THROW_WHEN(symIdx > userAddrs.size(), RelocError,
                                "Invalid symbol index. It exceeds the number of relevant device buffers");

            auto relType = elf64RType(relocation.r_info);
            auto addend = relocation.r_addend;

            VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t\t applying Reloc offset symidx reltype addend %llu %u %u %llu",
                         relOffset, symIdx, relType, addend);
            auto reloc = relocationMap.find(static_cast<RelocationType>(relType));
            VPUX_ELF_THROW_WHEN(reloc == relocationMap.end() || reloc->second == nullptr, RelocError,
                                "Invalid relocation type detected");

            auto relocFunc = reloc->second;
            auto targetAddr = targetSectionAddr + relOffset;

            VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t targetsectionAddr %p offs %llu result %p userAddr 0x%x symIdx %u",
                         targetSectionAddr, relOffset, targetAddr, (uint32_t)userAddrs[symIdx - 1].vpu_addr(),
                         symIdx - 1);

            elf::SymbolEntry origSymbol = symTabs[symIdx];

            elf::SymbolEntry targetSymbol;
            targetSymbol.st_info = 0;
            targetSymbol.st_name = 0;
            targetSymbol.st_other = 0;
            targetSymbol.st_shndx = 0;
            targetSymbol.st_value = userAddrs[symIdx - 1].vpu_addr();
            targetSymbol.st_size = origSymbol.st_size;

            relocFunc((void*)targetAddr, targetSymbol, addend);
        }
    }
}

std::vector<DeviceBuffer> VPUXLoader::getAllocatedBuffers() const {
    return m_inferBufferContainer.getBuffersAsVector();
}

void VPUXLoader::registerUserIO(std::vector<DeviceBuffer>& userIO, const elf::SymbolEntry* symbols,
                                size_t symbolCount) const {
    if (symbolCount <= 1) {
        VPUX_ELF_LOG(LogLevel::LOG_WARN, "Have a USER_IO symbols section with no symbols");
        return;
    }

    userIO.resize(symbolCount - 1);

    // symbol sections always start with an UNDEFINED symbol by standard
    for (size_t symbolCtr = 1; symbolCtr < symbolCount; ++symbolCtr) {
        const elf::SymbolEntry& sym = symbols[symbolCtr];
        userIO[symbolCtr - 1] = DeviceBuffer(nullptr, 0, sym.st_size);
        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\t index %zu : size %zu\n", symbolCtr - 1, sym.st_size);
    }
}

void VPUXLoader::earlyFetchIO(const elf::Reader<Elf64>::Section& section) {
    const auto sectionFlags = section.getHeader()->sh_flags;

    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "Parsed symtab section with flags %llx", sectionFlags);

    if (sectionFlags & VPU_SHF_USERINPUT) {
        VPUX_ELF_THROW_WHEN(m_userInputsDescriptors->size(), SequenceError,
                            "User inputs already read.... potential more than one input section?");

        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\tRegistering %zu inputs", section.getEntriesNum() - 1);
        registerUserIO(*m_userInputsDescriptors, section.getData<elf::SymbolEntry>(), section.getEntriesNum());
    } else if (sectionFlags & VPU_SHF_USEROUTPUT) {
        VPUX_ELF_THROW_WHEN(m_userOutputsDescriptors->size(), SequenceError,
                            "User outputs already read.... potential more than one output section?");

        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\tRegistering %zu outputs", section.getEntriesNum() - 1);
        registerUserIO(*m_userOutputsDescriptors, section.getData<elf::SymbolEntry>(), section.getEntriesNum());
    } else if (sectionFlags & VPU_SHF_PROFOUTPUT) {
        VPUX_ELF_THROW_WHEN(m_profOutputsDescriptors->size(), SequenceError,
                            "Profiling outputs already read.... potential more than one output section?");

        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "\tRegistering %zu prof outputs", section.getEntriesNum() - 1);
        registerUserIO(*m_profOutputsDescriptors, section.getData<elf::SymbolEntry>(), section.getEntriesNum());
    }
}

std::vector<DeviceBuffer> VPUXLoader::getInputBuffers() const {
    return *m_userInputsDescriptors.get();
};

std::vector<DeviceBuffer> VPUXLoader::getOutputBuffers() const {
    return *m_userOutputsDescriptors.get();
};

std::vector<DeviceBuffer> VPUXLoader::getProfBuffers() const {
    return *m_profOutputsDescriptors.get();
};

bool VPUXLoader::checkSectionType(const elf::SectionHeader* section, Elf_Word secType) const {
    return section->sh_type == secType;
}

std::vector<std::shared_ptr<ManagedBuffer>> VPUXLoader::getSectionsOfType(elf::Elf_Word type) {
    VPUX_ELF_THROW_WHEN(!utils::hasMemoryFootprint(type), elf::RuntimeError,
                        "Can't access data of NOBITS-like section");
    VPUX_ELF_THROW_UNLESS(m_sectionMap->find(type) != m_sectionMap->end(), RangeError, "Section type not registered!");
    std::vector<std::shared_ptr<ManagedBuffer>> retVector;
    for (auto sectionIndex : (*m_sectionMap)[type]) {
        auto sectionBuffer = m_reader->getSection(sectionIndex).getDataBuffer();
        retVector.push_back(sectionBuffer);
    }

    return retVector;
};

}  // namespace elf

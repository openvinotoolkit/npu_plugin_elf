//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include "api/vpu_cmx_info_mtl.h"
#include <vpux_loader/vpux_loader.hpp>
#include "elf_utils.h"

namespace elf {

class SymTabGen {
    static constexpr uint8_t N_TABS = 2;

    static constexpr size_t SPECIAL_SYMTAB_SIZE = 7;
    static SymbolEntry symTab_[N_TABS][SPECIAL_SYMTAB_SIZE];

public:
    SymTabGen() = delete;

    static void initSymTab() {
        uint32_t inv_addr[] = {nn_public::CONFIG_1_TILE_INV_METADATA_ADDR_0,
                               nn_public::CONFIG_2_TILES_INV_METADATA_ADDR};

        uint32_t akr_addr[] = {nn_public::CONFIG_1_TILE_AKR_METADATA_ADDR_0,
                               nn_public::CONFIG_2_TILES_AKR_METADATA_ADDR};

        uint32_t dma0_addr[] = {nn_public::DMA0_STORAGE_ADDR, nn_public::DMA0_STORAGE_ADDR};

        uint32_t dma1_addr[] = {nn_public::DMA0_STORAGE_ADDR, nn_public::DMA1_STORAGE_ADDR};

        for (int j = 0; j < N_TABS; ++j) {
            for (size_t i = 0; i < SPECIAL_SYMTAB_SIZE; ++i) {
                symTab_[j][i].st_info = static_cast<unsigned char>(elf64STInfo(elf::STB_GLOBAL, elf::STT_OBJECT));
                symTab_[j][i].st_other = STV_DEFAULT;
                symTab_[j][i].st_shndx = 0;
                symTab_[j][i].st_name = 0;
            }

            symTab_[j][VPU_NNRD_SYM_NNCXM_SLICE_BASE_ADDR].st_value = nn_public::WORKSPACE_ADDR_0;
            symTab_[j][VPU_NNRD_SYM_NNCXM_SLICE_BASE_ADDR].st_size = nn_public::VPU_WORKSPACE_SIZE;

            symTab_[j][VPU_NNRD_SYM_RTM_IVAR].st_value = inv_addr[j];
            symTab_[j][VPU_NNRD_SYM_RTM_IVAR].st_size = nn_public::VPU_INVARIANT_COUNT;

            symTab_[j][VPU_NNRD_SYM_RTM_ACT].st_value = akr_addr[j];
            symTab_[j][VPU_NNRD_SYM_RTM_ACT].st_size = nn_public::VPU_KERNEL_RANGE_COUNT;

            symTab_[j][VPU_NNRD_SYM_RTM_DMA0].st_value = dma0_addr[j];
            symTab_[j][VPU_NNRD_SYM_RTM_DMA0].st_size = nn_public::VPU_DMA_TASK_COUNT;

            symTab_[j][VPU_NNRD_SYM_RTM_DMA1].st_value = dma1_addr[j];
            symTab_[j][VPU_NNRD_SYM_RTM_DMA1].st_size = nn_public::VPU_DMA_TASK_COUNT;

            symTab_[j][VPU_NNRD_SYM_FIFO_BASE].st_value = 0x0;
            symTab_[j][VPU_NNRD_SYM_FIFO_BASE].st_size = 0;

            symTab_[j][VPU_NNRD_SYM_BARRIERS_START].st_value = 0;
            symTab_[j][VPU_NNRD_SYM_BARRIERS_START].st_size = 0;
        }
    }

    static details::ArrayRef<SymbolEntry> symTab(uint8_t index) {
        check_cond_and_exit((index != 0 && index <= N_TABS), "The sym tab configuration %hhu is not supported!", index);

        // Return configuration of index -1, because the configuration list begins at 0
        // 0 - single tile
        // 1 - multi tile
        return details::ArrayRef<SymbolEntry>(symTab_[index - 1], SPECIAL_SYMTAB_SIZE);
    }
};

} // namespace elf

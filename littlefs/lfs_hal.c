/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2021, Northern Mechatronics, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <am_mcu_apollo.h>

#include "lfs.h"

// Read a region in a block. Negative error codes are propogated
// to the user.
int littlefs_hal_read(const struct lfs_config *c, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size)
{
    uint32_t starting_block = (uint32_t)c->context;
    uint32_t page = starting_block + block;
    uint32_t address = (page << 13) + off;

    memcpy(buffer, (void*)address, size);

    return LFS_ERR_OK;
}

// Program a region in a block. The block must have previously
// been erased. Negative error codes are propogated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
int littlefs_hal_prog(const struct lfs_config *c, lfs_block_t block,
        lfs_off_t off, const void *buffer, lfs_size_t size)
{
    uint32_t starting_block = (uint32_t)c->context;
    uint32_t page = starting_block + block;

    uint32_t address = (page << 13) + off;

    am_hal_interrupt_master_disable();
    int err = am_hal_flash_program_main(AM_HAL_FLASH_PROGRAM_KEY, (uint32_t *)buffer, (uint32_t *)address, size >> 2);
    am_hal_interrupt_master_enable();

    return LFS_ERR_OK;
}

// Erase a block. A block must be erased before being programmed.
// The state of an erased block is undefined. Negative error codes
// are propogated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
int littlefs_hal_erase(const struct lfs_config *c, lfs_block_t block)
{
    uint32_t starting_block = (uint32_t)c->context;
    uint32_t page = starting_block + block;
    uint32_t address = (page << 13);

    uint32_t instance = AM_HAL_FLASH_ADDR2INST(address);
    uint32_t rel_page = AM_HAL_FLASH_ADDR2PAGE(address);

    am_hal_interrupt_master_disable();
    int err = am_hal_flash_page_erase(AM_HAL_FLASH_PROGRAM_KEY,
            instance, rel_page);
    am_hal_interrupt_master_enable();

    return LFS_ERR_OK;
}

// Sync the state of the underlying block device. Negative error codes
// are propogated to the user.
int littlefs_hal_sync(const struct lfs_config *c)
{
    return LFS_ERR_OK;
}


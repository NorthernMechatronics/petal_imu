/*
 *  BSD 3-Clause License
 *
 * Copyright (c) 2024, Northern Mechatronics, Inc.
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
#include <am_util.h>

#include "am_bsp.h"

#include "lfs.h"
#include "lfs_hal.h"

#include "mag.h"

#include "application_task.h"

#define CACHE_SIZE      16
#define LOOKAHEAD_SIZE  16

// LoRaWAN and BLE each use 2 pages
#define LFS_NUM_PAGES   (4)
#define LFS_START_PAGE  ((AM_HAL_FLASH_INSTANCE_PAGES - 1) - 2 - 2 - LFS_NUM_PAGES)

static lfs_t lfs;
static uint8_t lfs_read_buffer[CACHE_SIZE];
static uint8_t lfs_prog_buffer[CACHE_SIZE];
static uint8_t lfs_lookahead_buffer[LOOKAHEAD_SIZE];

const struct lfs_config cfg = {
    .context = (void *)LFS_START_PAGE,                // starting page number
    .read = littlefs_hal_read,
    .prog = littlefs_hal_prog,
    .erase = littlefs_hal_erase,
    .sync = littlefs_hal_sync,
    .read_size = 4,               // minimum number of bytes per read
    .prog_size = 4,               // minimum number of bytes per write
    .block_size = AM_HAL_FLASH_PAGE_SIZE,           // size per page
    .block_count = LFS_NUM_PAGES,             // number of pages used for the file system
    .cache_size = CACHE_SIZE,
    .lookahead_size = LOOKAHEAD_SIZE,
    .block_cycles = 500,
    .read_buffer = lfs_read_buffer,
    .prog_buffer = lfs_prog_buffer,
    .lookahead_buffer = lfs_lookahead_buffer,
};

void application_lfs_init(void)
{
    int err = lfs_mount(&lfs, &cfg);
    if (err) {
        lfs_format(&lfs, &cfg);
        lfs_mount(&lfs, &cfg);
    }
}

void application_lfs_deinit(void)
{
    lfs_unmount(&lfs);
}

void application_lfs_load_cal(mag_cal_t *cal_data)
{
    lfs_file_t file;

    lfs_file_open(&lfs, &file, "cal_data", LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_read(&lfs, &file, cal_data, sizeof(mag_cal_t));
    lfs_file_close(&lfs, &file);
}

void application_lfs_write_cal(mag_cal_t *cal_data)
{
    lfs_file_t file;

    lfs_file_open(&lfs, &file, "cal_data", LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_write(&lfs, &file, cal_data, sizeof(mag_cal_t));
    lfs_file_close(&lfs, &file);
}
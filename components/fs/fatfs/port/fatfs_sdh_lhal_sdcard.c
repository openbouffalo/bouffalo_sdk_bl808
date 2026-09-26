/**
 * @file sdh_sdcard.c
 * @brief
 *
 * Copyright (c) 2021 Bouffalolab team
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 */

#include "ff.h"     /* Obtains integer types */
#include "diskio.h" /* Declarations of disk functions */
#include "sdh_sd.h"
#include <string.h>

struct sd_card_s sd_card;
struct sdh_host_s sdh_host;

/*
 * The SDH ADMA2 engine rejects buffers that are not USDHC_ADMA2_ADDRESS_ALIGN
 * aligned. FatFs passes the caller's f_read()/f_write() buffer straight to the
 * disk layer for whole-sector transfers, at an offset that depends on the file
 * position, so it can be arbitrarily aligned. Such transfers are staged through
 * this buffer. It is cache-line aligned and sized so D-cache maintenance on it
 * cannot touch neighbouring data.
 */
#define MMC_BOUNCE_SECTORS 4
#define MMC_BOUNCE_ALIGN   64
__attribute__((aligned(MMC_BOUNCE_ALIGN))) static BYTE mmc_bounce_buf[SD_DEFAULT_BLOCK_SIZE * MMC_BOUNCE_SECTORS];

static inline bool mmc_buf_is_aligned(const void *buff)
{
    return ((uintptr_t)buff % USDHC_ADMA2_ADDRESS_ALIGN) == 0;
}

int MMC_disk_status()
{
    return 0;
}

int MMC_disk_initialize()
{
    static bool inited = false;

    if (inited) {
        sdh_sd_deinit(&sd_card);
    }

    if (sdh_sd_card_init(&sd_card, &sdh_host) < 0) {
        sdh_sd_deinit(&sd_card);
        return -1;
    }
    inited = true;

    return 0;
}

int MMC_disk_read(BYTE *buff, LBA_t sector, UINT count)
{
    if (mmc_buf_is_aligned(buff)) {
        if (sdh_sd_read_blocks(&sd_card, (void *)buff, sector, count) < 0) {
            return -1;
        }
        return 0;
    }

    while (count > 0) {
        UINT chunk = (count > MMC_BOUNCE_SECTORS) ? MMC_BOUNCE_SECTORS : count;
        if (sdh_sd_read_blocks(&sd_card, (void *)mmc_bounce_buf, sector, chunk) < 0) {
            return -1;
        }
        memcpy(buff, mmc_bounce_buf, chunk * SD_DEFAULT_BLOCK_SIZE);
        buff += chunk * SD_DEFAULT_BLOCK_SIZE;
        sector += chunk;
        count -= chunk;
    }

    return 0;
}

int MMC_disk_write(const BYTE *buff, LBA_t sector, UINT count)
{
    if (mmc_buf_is_aligned(buff)) {
        if (sdh_sd_write_blocks(&sd_card, (void *)buff, sector, count) < 0) {
            return -1;
        }
        return 0;
    }

    while (count > 0) {
        UINT chunk = (count > MMC_BOUNCE_SECTORS) ? MMC_BOUNCE_SECTORS : count;
        memcpy(mmc_bounce_buf, buff, chunk * SD_DEFAULT_BLOCK_SIZE);
        if (sdh_sd_write_blocks(&sd_card, (void *)mmc_bounce_buf, sector, chunk) < 0) {
            return -1;
        }
        buff += chunk * SD_DEFAULT_BLOCK_SIZE;
        sector += chunk;
        count -= chunk;
    }

    return 0;
}

int MMC_disk_ioctl(BYTE cmd, void *buff)
{
    switch (cmd) {
        // Get R/W sector size (WORD)
        case GET_SECTOR_SIZE:
            *(WORD *)buff = sd_card.block_size;
            break;

        // Get erase block size in unit of sector (DWORD)
        case GET_BLOCK_SIZE:
            *(DWORD *)buff = 1;
            break;

        case GET_SECTOR_COUNT:
            *(DWORD *)buff = sd_card.block_count;
            break;

        case CTRL_SYNC:
            break;

        default:
            break;
    }

    return 0;
}

DSTATUS Translate_Result_Code(int result)
{
    return result;
}

void fatfs_sdh_driver_register(void)
{
    FATFS_DiskioDriverTypeDef SDH_DiskioDriver = { NULL };

    SDH_DiskioDriver.disk_status = MMC_disk_status;
    SDH_DiskioDriver.disk_initialize = MMC_disk_initialize;
    SDH_DiskioDriver.disk_write = MMC_disk_write;
    SDH_DiskioDriver.disk_read = MMC_disk_read;
    SDH_DiskioDriver.disk_ioctl = MMC_disk_ioctl;
    SDH_DiskioDriver.error_code_parsing = Translate_Result_Code;

    disk_driver_callback_init(DEV_SD, &SDH_DiskioDriver);
}

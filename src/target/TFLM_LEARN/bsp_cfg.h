#pragma once

#include "bsp/bsp_display.h"
#include "bsp/bsp_storage.h"
#include "bsp/bsp_touch.h"

/***********************************************************************************************************************/
/**
 * @brief bsp_display
 */

#define BSP_DISPLAY_MAP_TABLE {\
        { .type = BSP_DISPLAY_TYPE_USER, .idx = 0, .x = 0,   .y = 0,   .width = 320, .height = 240 },\
        { .type = BSP_DISPLAY_TYPE_USER, .idx = 1, .x = 330, .y = 0,   .width = 320, .height = 240 },\
        { .type = BSP_DISPLAY_TYPE_USER, .idx = 2, .x = 330, .y = 250, .width = 224, .height = 224 },\
        { .type = BSP_DISPLAY_TYPE_USER, .idx = 3, .x = 564, .y = 250, .width = 224, .height = 224 },\
    }

/***********************************************************************************************************************/
/**
 * @brief bsp_touch
 */
#define BSP_TOUCH_MAP_TABLE {\
        { .type = BSP_TOUCH_TYPE_USER, .idx = 0, .x = 0, .y = 0, .width = 320, .height = 240 },\
    }

/***********************************************************************************************************************/
/**
 * @brief bsp_storage 配置（SDL 文件后端）
 */

#define BSP_STORAGE_BASE_DIR    "flash_data"

#define BSP_STORAGE_MAP_TABLE {\
        { .type = BSP_STORAGE_TYPE_USER,   .index = 0, .medium = BSP_STORAGE_MEDIUM_FLASH,\
          .filename = "flash_user", .total_size = 1 * 1024 * 1024,\
          .erase_unit = 4096, .program_unit = 256, .erase_value = 0xFF, .write_direct = 0 },\
        { .type = BSP_STORAGE_TYPE_SYSTEM, .index = 0, .medium = BSP_STORAGE_MEDIUM_FLASH,\
          .filename = "flash_sys",  .total_size = 256 * 1024,\
          .erase_unit = 4096, .program_unit = 256, .erase_value = 0xFF, .write_direct = 0 },\
    }

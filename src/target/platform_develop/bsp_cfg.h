#pragma once

#include "bsp/bsp_display.h"
#include "bsp/bsp_encoder.h"
#include "bsp/bsp_storage.h"
#include "bsp/bsp_touch.h"

/***********************************************************************************************************************/
/**
 * @brief bsp_display
 *
 * 平台开发目标只保留一块主屏（320x240），触摸与之重合；右侧放编码器。
 *
 *   [0,0]                 [336,0]
 *   +------------------+  +--------+
 *   | display0 / touch |  |  VOL   |
 *   |     320 x 240    |  +--------+
 *   |                  |  |  BPM   |
 *   +------------------+  +--------+
 */

#define BSP_DISPLAY_MAP_TABLE {\
        { .type = BSP_DISPLAY_TYPE_USER, .idx = 0, .x = 0, .y = 0, .width = 320, .height = 240 },\
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
 * @brief bsp_encoder 配置（SDL 鼠标滚轮模拟）
 * @note bsp_encoder_map_t 定义在 bsp_encoder_sdl.c 内部，这里只放映射表宏。
 */

/* 所有编码器共用一个轮询任务，统一频率 */
#define BSP_ENCODER_POLL_FREQ_HZ    50

/* 编码器映射表：type + idx -> 主窗口内的旋钮区域
 *   名字显示在旋钮上方；鼠标位于区域内时滚轮模拟旋转，左键模拟按下。 */
#define BSP_ENCODER_MAP_TABLE {\
        { .type = BSP_ENCODER_TYPE_SYSTEM_CONTROL, .idx = 0, .name = "VOL",\
          .x = 336, .y = 8,   .width = 120, .height = 110 },\
        { .type = BSP_ENCODER_TYPE_BPM_CONTROL,    .idx = 0, .name = "BPM",\
          .x = 336, .y = 126, .width = 120, .height = 110 },\
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

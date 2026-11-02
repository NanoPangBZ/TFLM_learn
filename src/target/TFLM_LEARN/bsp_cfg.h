#pragma once

#include "bsp/bsp_display.h"
#include "bsp/bsp_storage.h"
#include "bsp/bsp_touch.h"

/***********************************************************************************************************************/
/**
 * @brief bsp_display
 * @note bsp_display_map_t 定义在 bsp_display_sdl.c 内部。
 *       rotation：呈现顺时针角度（度），常用 0/90/180/270。
 */

#define BSP_DISPLAY_MAP_TABLE {\
        { .type = BSP_DISPLAY_TYPE_USER, .idx = 0,\
          .x = 0,   .y = 0,   .width = 320, .height = 240, .rotation = 0 },\
        { .type = BSP_DISPLAY_TYPE_USER, .idx = 1,\
          .x = 330, .y = 0,   .width = 320, .height = 240, .rotation = 0 },\
        { .type = BSP_DISPLAY_TYPE_USER, .idx = 2,\
          .x = 330, .y = 250, .width = 224, .height = 224, .rotation = 0 },\
        { .type = BSP_DISPLAY_TYPE_USER, .idx = 3,\
          .x = 564, .y = 250, .width = 224, .height = 224, .rotation = 0 },\
    }

/***********************************************************************************************************************/
/**
 * @brief bsp_touch
 * @note bsp_touch_map_t 定义在 bsp_touch_sdl.c 内部。
 *       rotation：与配对 display 同值；命中旋转后 AABB，回报逻辑坐标。
 */
#define BSP_TOUCH_MAP_TABLE {\
        { .type = BSP_TOUCH_TYPE_USER, .idx = 0,\
          .x = 0, .y = 0, .width = 320, .height = 240, .rotation = 0 },\
    }

/***********************************************************************************************************************/
/**
 * @brief bsp_audio_system 配置
 * @note 采样率 / 帧长 / 通道数；PANEL_W 为频率表与配置页共用宽度。
 */
#define BSP_AUDIO_SAMPLE_RATE_HZ    44100u
#define BSP_AUDIO_FRAME_SAMPLES     128u
#define BSP_AUDIO_RX_CHANNELS       1u
#define BSP_AUDIO_TX_CHANNELS       1u
#define BSP_AUDIO_POLL_FREQ_HZ      500u

/* 主显示区右缘约 788，音频列紧挨其右 */
#define BSP_AUDIO_UI_X              800
#define BSP_AUDIO_UI_Y              8
#define BSP_AUDIO_PANEL_W           280
#define BSP_AUDIO_METER_H           244
#define BSP_AUDIO_METER_GAP         8

/***********************************************************************************************************************/
/**
 * @brief bsp_storage 配置（SDL 文件后端）
 * @note bsp_storage_map_t 定义在 bsp_storage_sdl.c 内部。
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

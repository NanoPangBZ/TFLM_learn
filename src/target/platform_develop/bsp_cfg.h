#pragma once

#include "bsp/bsp_display.h"
#include "bsp/bsp_keyboard.h"
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
 *
 * @note bsp_display_map_t 定义在 bsp_display_sdl.c 内部。
 */

#define BSP_DISPLAY_MAP_TABLE {\
        { .type = BSP_DISPLAY_TYPE_USER, .idx = 0, .x = 0, .y = 0, .width = 320, .height = 240 },\
    }

/***********************************************************************************************************************/
/**
 * @brief bsp_touch
 * @note bsp_touch_map_t 定义在 bsp_touch_sdl.c 内部。
 */
#define BSP_TOUCH_MAP_TABLE {\
        { .type = BSP_TOUCH_TYPE_USER, .idx = 0, .x = 0, .y = 0, .width = 320, .height = 240 },\
    }

/***********************************************************************************************************************/
/**
 * @brief bsp_encoder 配置（SDL 鼠标滚轮模拟）
 * @note 与 YouWei 一致按 name 打开；bsp_encoder_map_t 在 bsp_encoder_sdl.c 内定义。
 */

/* 所有编码器共用一个轮询任务，统一频率 */
#define BSP_ENCODER_POLL_FREQ_HZ    50

/* 编码器映射表：name -> 主窗口内的旋钮区域
 *   名字显示在旋钮上方；鼠标位于区域内时滚轮模拟旋转，左键模拟按下。 */
#define BSP_ENCODER_MAP_TABLE {\
        { .name = "VOL", .x = 336, .y = 8,   .width = 120, .height = 110, .dir = 1 },\
        { .name = "BPM", .x = 336, .y = 126, .width = 120, .height = 110, .dir = 1 },\
    }

/***********************************************************************************************************************/
/**
 * @brief bsp_audio_system 配置
 * @note 映射表项 / RX_IMPL_TYPE_* / TX_IMPL_TYPE_* 定义在 bsp_audio_system_sdl.c。
 */
#define BSP_AUDIO_SAMPLE_RATE_HZ    44100u
#define BSP_AUDIO_FRAME_SAMPLES     128u
#define BSP_AUDIO_POLL_FREQ_HZ      500u

#define BSP_AUDIO_SYSTEM_RX_MAPTABLE {\
        {\
          .rx_impl_type = RX_IMPL_TYPE_SYSTEM_OUTPUT_LOOPBACK, .name = "SYS",\
          .config.system_output_loopback = { .gain = 1.0f }\
        },\
        {\
          .rx_impl_type = RX_IMPL_TYPE_SINE_WAVE, .name = "SINE",\
          .config.sine_wave = { .frequency = 1000.0f, .amplitude = 0.2f }\
        },\
    }

#define BSP_AUDIO_SYSTEM_TX_MAPTABLE {\
        {\
          .tx_impl_type = TX_IMPL_TYPE_FILE, .name = "OUT",\
          .config.file = { .file_path = "audio_tx.wav", .gain = 1.0f }\
        },\
    }

#define BSP_AUDIO_ENERGY_X          8
#define BSP_AUDIO_ENERGY_Y          252
#define BSP_AUDIO_ENERGY_W          448
#define BSP_AUDIO_ENERGY_H          80

/***********************************************************************************************************************/
/**
 * @brief bsp_keyboard 配置（SDL 鼠标点击模拟）
 * @note 映射表项结构体在 bsp_keyboard_sdl.c；键位固定 8 列 × 4 行，
 *       key_count 决定实际显示/有效键数（≤32）。
 */
#define BSP_KEYBOARD_POLL_FREQ_HZ   50

#define BSP_KEYBOARD_MAP_TABLE {\
        {\
          .type = BSP_KEYBOARD_TYPE_VIRTUAL_TEST, .idx = 0, .name = "PAD",\
          .x = 8, .y = 340, .width = 448, .height = 168,\
          .key_count = 16\
        },\
    }

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

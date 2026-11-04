#pragma once

#include "bsp/bsp_display.h"
#include "bsp/bsp_keyboard.h"
#include "bsp/bsp_storage.h"
#include "bsp/bsp_touch.h"

/***********************************************************************************************************************/
/**
 * @brief bsp_display
 *
 * 平台开发目标三列布局：
 *
 *   [0,0]           [328,0]      [464,0]
 *   +------------+  +--------+   +--------------+
 *   | display0   |  |  VOL   |   | AUDIO meters |
 *   | 320 x 240  |  +--------+   | PANEL_W x N  |
 *   |            |  |  BPM   |   |              |
 *   +------------+  +--------+   +--------------+
 *   [LED 0..6]      [RV100/101]
 *   [keyboard]
 *
 * @note bsp_display_map_t 定义在 bsp_display_sdl.c 内部。
 *       rotation：呈现顺时针角度（度），常用 0/90/180/270。
 */

#define BSP_DISPLAY_MAP_TABLE {\
        { .type = BSP_DISPLAY_TYPE_USER, .idx = 0,\
          .x = 0, .y = 0, .width = 240, .height = 320, .rotation = 90 },\
    }

/***********************************************************************************************************************/
/**
 * @brief bsp_touch
 * @note bsp_touch_map_t 定义在 bsp_touch_sdl.c 内部。
 *       rotation：与配对 display 同值；命中旋转后 AABB，回报逻辑坐标。
 */
#define BSP_TOUCH_MAP_TABLE {\
        { .type = BSP_TOUCH_TYPE_USER, .idx = 0,\
          .x = 0, .y = 0, .width = 240, .height = 320, .rotation = 90 },\
    }

/***********************************************************************************************************************/
/**
 * @brief bsp_led 配置（SDL 圆点模拟）
 * @note bsp_led_map_t 在 bsp_led_sdl.c：圆心 (x,y)，color=0xRRGGBB 点亮色，
 *       直径固定 16px；灭时同色暗化。索引即 bsp_led_idx_t；流水灯测 0..6。
 *       行位于 display 底边与 keyboard 顶边之间的缝隙。
 */
#define BSP_LED_MAP_TABLE {\
        { .x =  24, .y = 330, .color = 0x28C85A },\
        { .x =  52, .y = 330, .color = 0x28C85A },\
        { .x =  80, .y = 330, .color = 0x28C85A },\
        { .x = 108, .y = 330, .color = 0xF0C030 },\
        { .x = 136, .y = 330, .color = 0xF0C030 },\
        { .x = 164, .y = 330, .color = 0xE04040 },\
        { .x = 192, .y = 330, .color = 0xE04040 },\
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
        { .name = "VOL", .x = 328, .y = 8,   .width = 120, .height = 110, .dir = 1 },\
        { .name = "BPM", .x = 328, .y = 126, .width = 120, .height = 110, .dir = 1 },\
    }

/***********************************************************************************************************************/
/**
 * @brief bsp_audio_system 配置
 * @note 采样率 / 帧长 / 通道数编译期固定。
 *       PANEL_W：频率表与配置页共用宽度；METER_H/GAP 控制每通道表高与间距。
 *       RX/TX_NAMES：可选通道显示名（配置页左侧列表与能量表标题）。
 */
#define BSP_AUDIO_SAMPLE_RATE_HZ    44100u
#define BSP_AUDIO_FRAME_SAMPLES     128u
#define BSP_AUDIO_RX_CHANNELS       2u
#define BSP_AUDIO_TX_CHANNELS       1u
#define BSP_AUDIO_POLL_FREQ_HZ      500u
#define BSP_AUDIO_TX_NAMES          { "OUT" }
#define BSP_AUDIO_RX_NAMES          { "IN0", "IN1" }

#define BSP_AUDIO_UI_X              464
#define BSP_AUDIO_UI_Y              8
#define BSP_AUDIO_PANEL_W           320
#define BSP_AUDIO_METER_H           148
#define BSP_AUDIO_METER_GAP         8

/***********************************************************************************************************************/
/**
 * @brief bsp_keyboard 配置（SDL 鼠标点击模拟）
 * @note 映射表项在 bsp_keyboard_sdl.c。
 *       width/height：面板外框像素尺寸；键格铺满可用区。
 *       key_count：有效键数（1..32）；cols=0 按长宽比自适应列数，>0 强制列数。
 */
#define BSP_KEYBOARD_POLL_FREQ_HZ   50

#define BSP_KEYBOARD_MAP_TABLE {\
        {\
          .type = BSP_KEYBOARD_TYPE_VIRTUAL_TEST, .idx = 0, .name = "PAD",\
          .x = 8, .y = 340, .width = 312, .height = 168,\
          .key_count = 16, .cols = 0\
        },\
    }

/***********************************************************************************************************************/
/**
 * @brief bsp_knob 配置（SDL 电位器模拟，有限行程 + 红点标位）
 * @note bsp_knob_map_t 在 bsp_knob_sdl.c；按 name 取句柄（与 YouWei 一致）。
 *       initial：初始值 0..1；dir：1 顺时针增大，-1 逆时针增大。
 */
#define BSP_KNOB_POLL_FREQ_HZ   50

#define BSP_KNOB_MAP_TABLE {\
        { .name = "RV100", .x = 328, .y = 340, .width = 120, .height = 84,\
          .initial = 0.5f, .dir = 1 },\
        { .name = "RV101", .x = 328, .y = 424, .width = 120, .height = 84,\
          .initial = 0.0f, .dir = 1 },\
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

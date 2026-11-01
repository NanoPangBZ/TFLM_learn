#pragma once

#include "bsp/bsp_display.h"
#include "bsp/bsp_touch.h"

/***********************************************************************************************************************/
/**
 * @brief bsp_display / bsp_touch 配置（SDL 后端）
 * @note 参考 N32H762 工程的 bsp_cfg.h：设备通过 type + idx 在映射表里查自己的一块区域。
 *       SDL 后端把这些区域映射到 arch 主窗口内的矩形（窗口客户区像素坐标，1:1 不缩放）。
 *       窗口小于区域时超出部分被裁剪；窗口更大时其余区域留给后续调试面板。
 * @file src: src/bsp/private/sdl/bsp_display_sdl.c
 * @file src: src/bsp/private/sdl/bsp_touch_sdl.c
 * @file api: src/bsp/bsp_display.h / src/bsp/bsp_touch.h
 */

/* 画布显示区（idx 0）的位置与大小；app 的静态墨迹缓冲仍按这个尺寸开 */
#define BSP_DISPLAY_X       0
#define BSP_DISPLAY_Y       0
#define BSP_DISPLAY_WIDTH   320
#define BSP_DISPLAY_HEIGHT  240

/* 显示区域映射表：type + idx -> 主窗口内矩形
 *   idx 0: 手写画布（canvas_service 使用）
 *   idx 1: 墨迹 + 包围盒预览（app 调试输出）
 *   idx 2: 缩放后的数字预览
 *   idx 3: 模型实际输入预览
 * 布局按 ARCH_WINDOW_WIDTH x ARCH_WINDOW_HEIGHT（800x490）排：
 *   [0,0 320x240] [330,0 320x240]
 *                 [330,250 224x224] [564,250 224x224]
 */
#define BSP_DISPLAY_MAP_TABLE {\
        { .type = BSP_DISPLAY_TYPE_USER, .idx = 0, .x = 0,   .y = 0,   .width = 320, .height = 240 },\
        { .type = BSP_DISPLAY_TYPE_USER, .idx = 1, .x = 330, .y = 0,   .width = 320, .height = 240 },\
        { .type = BSP_DISPLAY_TYPE_USER, .idx = 2, .x = 330, .y = 250, .width = 224, .height = 224 },\
        { .type = BSP_DISPLAY_TYPE_USER, .idx = 3, .x = 564, .y = 250, .width = 224, .height = 224 },\
    }

/* 触摸区域映射表：type + idx -> 主窗口内矩形
 *   当前把 idx 0 映射到画布区域；需要多个触摸实例时按同样格式追加，例如：
 *   { .type = BSP_TOUCH_TYPE_USER, .idx = 1, .x = 330, .y = 0, .width = 320, .height = 240 },
 *   注意：触摸区域独立配置，改了显示区位置要同步改这里。 */
#define BSP_TOUCH_MAP_TABLE {\
        { .type = BSP_TOUCH_TYPE_USER, .idx = 0, .x = 0, .y = 0, .width = 320, .height = 240 },\
    }

#pragma once

/**************************arch 层窗口配置（SDL3 后端）***************************/
/* arch 层代表"单片机板卡"：统一创建主窗口与渲染器。
 * bsp_display 等外设只是窗口里的一块区域（见 bsp_cfg.h），不再各自开窗。
 * 窗口允许拖动缩放；各区域按像素 1:1 布局，窗口小于区域时超出部分被裁剪。 */

#define ARCH_WINDOW_TITLE           "TFLM_LEARN (SDL3 board)"
/* 主显示区(~788) + 间距 + 右侧音频面板 280 */
#define ARCH_WINDOW_WIDTH           1128
#define ARCH_WINDOW_HEIGHT          520

/* 1: 窗口可拖动缩放；0: 固定大小 */
#define ARCH_WINDOW_RESIZABLE       1

/* 窗口背景色，0xRRGGBB */
#define ARCH_WINDOW_BG_COLOR        0x1E2228

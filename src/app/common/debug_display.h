#pragma once

#include <stdint.h>
#include "bsp/bsp_display.h"

/* app 调试显示：把中间图像写到 bsp_cfg.h 里配置的某个 display 区域。
 * 例如 display idx 1/2/3 分别显示墨迹、缩放后的数字、模型输入。
 * 只做 8-bit 灰度/纯色矩形，够调试用；不依赖 SDL。 */

typedef struct app_debug_display_t {
    bsp_display_handle_t handle;
    uint16_t width;
    uint16_t height;
    uint8_t* draw_buffer;      /* RGB565 */
    uint32_t draw_buffer_size;
} app_debug_display_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 打开一个调试显示区域
 * @param d 调试显示对象
 * @param idx bsp_cfg.h 中 BSP_DISPLAY_MAP_TABLE 的 idx
 * @return 0:成功 其它:失败
 */
int app_debug_display_open(app_debug_display_t* d, uint8_t idx);

/**
 * @brief 关闭调试显示区域
 */
void app_debug_display_close(app_debug_display_t* d);

/**
 * @brief 用纯色填满整个区域并刷新
 * @param color 0xRRGGBB
 */
void app_debug_display_clear(app_debug_display_t* d, uint32_t color);

/**
 * @brief 把 w*h 灰度图按整数倍放大写入绘制缓冲（不刷新）
 * @param scale 放大倍数
 * @param center 1: 在区域内居中；0: 左上角对齐
 */
void app_debug_display_show_gray(app_debug_display_t* d, const uint8_t* gray,
                                 int src_w, int src_h, int scale, int center);

/**
 * @brief 在绘制缓冲上画一个矩形边框（不刷新）
 * @param color 0xRRGGBB
 */
void app_debug_display_draw_rect(app_debug_display_t* d, int x, int y, int w, int h, uint32_t color);

/**
 * @brief 刷新指定区域
 */
void app_debug_display_flush(app_debug_display_t* d, int x, int y, int w, int h);

/**
 * @brief 刷新整个区域
 */
void app_debug_display_flush_all(app_debug_display_t* d);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdint.h>

/* arch 显示层：主窗口、渲染器与帧循环由 arch 统一持有。
 * bsp_display 只负责把外设内容画到窗口里的某个矩形，不再创建独立窗口。 */

struct SDL_Window;
struct SDL_Renderer;

#ifdef __cplusplus
extern "C"
{
#endif  //__cplusplus

/**
 * @brief 获取 arch 主窗口（主线程使用）
 * @return SDL 窗口指针；arch_init 之前为 NULL
 */
struct SDL_Window* arch_display_window(void);

/**
 * @brief 获取 arch 渲染器（主线程使用）
 * @return SDL 渲染器指针；arch_init 之前为 NULL
 */
struct SDL_Renderer* arch_display_renderer(void);

/**
 * @brief 窗口坐标 -> 渲染像素坐标（处理 DPI 缩放与窗口大小变化）
 * @param window_x 窗口客户区坐标 X
 * @param window_y 窗口客户区坐标 Y
 * @param pixel_x 输出渲染像素坐标 X
 * @param pixel_y 输出渲染像素坐标 Y
 * @return 0:成功 其它:失败
 */
int arch_display_window_to_pixel(float window_x, float window_y, int* pixel_x, int* pixel_y);

/**
 * @brief 每帧绘制回调
 * @note 由 arch 主线程调用，调用前 arch 已清屏；回调只负责画自己的区域。
 *       不要在回调里再次清屏或 Present。
 */
typedef void (*arch_display_render_cb_t)(struct SDL_Renderer* renderer, void* ctx);

/**
 * @brief 注册每帧绘制回调（主线程，初始化阶段调用）
 * @return 0:成功 其它:失败（回调表已满）
 */
int arch_display_add_render_cb(arch_display_render_cb_t cb, void* ctx);

/**
 * @brief 注销每帧绘制回调（主线程）
 */
void arch_display_remove_render_cb(arch_display_render_cb_t cb, void* ctx);

/**
 * @brief 请求重绘一帧
 * @note 线程安全：任意线程更新完自己的缓冲后调用。arch 主循环会合并请求。
 */
void arch_display_request_frame(void);

#ifdef __cplusplus
}
#endif  //__cplusplus

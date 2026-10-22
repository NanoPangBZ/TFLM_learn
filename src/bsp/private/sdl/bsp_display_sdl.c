/* bsp_display_sdl.c — bsp_display 的 SDL3 后端实现。
 *
 * 参考硬件的 RGB565 帧缓冲模型：
 *   - 绘制缓存为 RGB565（16bpp，每像素 2 字节）；
 *   - fill 传入 0xRRGGBB 颜色，内部转 RGB565 填充后整屏刷新（阻塞式）；
 *   - flush 把指定区域从绘制缓存搬到屏幕（SDL 下同步完成，故 is_busy 恒 0，
 *     wait_flush_finish / wait_te_signal 为空操作）；
 *   - 屏幕尺寸由 bsp_cfg.h 的 BSP_DISPLAY_WIDTH / BSP_DISPLAY_HEIGHT 配置。
 *
 * 每个 display 句柄对应一个独立 SDL 窗口；渲染统一走 RGBX8888 纹理，
 * flush 时做一次 RGB565 -> RGBX8888 转换，规避 SDL 像素字节序的坑。 */
#include "bsp/bsp_display.h"
#include "bsp_cfg.h"
#include "framework/e_heap.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

#define MALLOC(size)  e_heap_malloc((size), E_HEAP_ATTR_DEFAULT)

typedef struct bsp_display_t {
    uint16_t width;
    uint16_t height;

    SDL_Window*   window;
    SDL_Renderer* renderer;
    SDL_Texture*  texture;       /* RGBX8888 */
    SDL_Mutex*    lock;          /* 保护渲染器（flush 可能从其它线程调用） */

    uint8_t*  draw_buffer;       /* RGB565 绘制缓存 */
    uint32_t  draw_buffer_size;

    uint8_t is_on;
    uint8_t brightness;
    uint8_t busy;

    void (*flush_finish_cb)(bsp_display_handle_t, void*);
    void* flush_finish_ctx;
} bsp_display_t;

/* RGB565 -> RGBX8888 */
static uint32_t rgb565_to_rgbx(uint16_t c)
{
    uint32_t r = (c >> 11) & 0x1Fu;
    uint32_t g = (c >> 5) & 0x3Fu;
    uint32_t b = c & 0x1Fu;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    return (r << 16) | (g << 8) | b | 0xFF000000u;
}

bsp_display_handle_t bsp_display_open(bsp_display_desc_t* desc)
{
    (void)desc;

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        return NULL;
    }

    bsp_display_t* d = (bsp_display_t*)MALLOC(sizeof(bsp_display_t));
    if (!d) {
        return NULL;
    }
    memset(d, 0, sizeof(*d));

    d->width = BSP_DISPLAY_WIDTH;
    d->height = BSP_DISPLAY_HEIGHT;
    d->is_on = 0;
    d->brightness = 100;

    d->window = SDL_CreateWindow("bsp_display (SDL3)", d->width, d->height, 0);
    if (!d->window) {
        return NULL;
    }
    d->renderer = SDL_CreateRenderer(d->window, NULL);
    if (!d->renderer) {
        SDL_DestroyWindow(d->window);
        return NULL;
    }
    d->texture = SDL_CreateTexture(d->renderer, SDL_PIXELFORMAT_RGBX8888,
                                   SDL_TEXTUREACCESS_STREAMING, d->width, d->height);
    if (!d->texture) {
        SDL_DestroyRenderer(d->renderer);
        SDL_DestroyWindow(d->window);
        return NULL;
    }
    d->lock = SDL_CreateMutex();
    if (!d->lock) {
        SDL_DestroyTexture(d->texture);
        SDL_DestroyRenderer(d->renderer);
        SDL_DestroyWindow(d->window);
        return NULL;
    }

    /* 内部分配 RGB565 绘制缓存 */
    d->draw_buffer_size = (uint32_t)d->width * d->height * 2;
    d->draw_buffer = (uint8_t*)MALLOC(d->draw_buffer_size);
    if (!d->draw_buffer) {
        SDL_DestroyMutex(d->lock);
        SDL_DestroyTexture(d->texture);
        SDL_DestroyRenderer(d->renderer);
        SDL_DestroyWindow(d->window);
        return NULL;
    }
    memset(d->draw_buffer, 0, d->draw_buffer_size);

    return (bsp_display_handle_t)d;
}

void bsp_display_close(bsp_display_handle_t handle)
{
    bsp_display_t* d = (bsp_display_t*)handle;
    if (!d) {
        return;
    }
    if (d->texture) SDL_DestroyTexture(d->texture);
    if (d->renderer) SDL_DestroyRenderer(d->renderer);
    if (d->window) SDL_DestroyWindow(d->window);
    if (d->lock) SDL_DestroyMutex(d->lock);
    /* 释放内部绘制缓存 */
    if (d->draw_buffer) e_heap_free(d->draw_buffer);
    e_heap_free(d);
}

bsp_display_status_t bsp_display_on(bsp_display_handle_t handle, uint8_t on_off)
{
    bsp_display_t* d = (bsp_display_t*)handle;
    if (!d) {
        return BSP_DISPLAY_STATUS_ERROR;
    }
    d->is_on = on_off ? 1 : 0;
    if (d->window) {
        if (d->is_on) SDL_ShowWindow(d->window);
        else          SDL_HideWindow(d->window);
    }
    return BSP_DISPLAY_STATUS_SUCCESS;
}

uint8_t bsp_display_is_on(bsp_display_handle_t handle)
{
    bsp_display_t* d = (bsp_display_t*)handle;
    return (d && d->is_on) ? 1 : 0;
}

bsp_display_status_t bsp_display_get_size(bsp_display_handle_t handle, uint16_t* width, uint16_t* height)
{
    bsp_display_t* d = (bsp_display_t*)handle;
    if (!d) {
        return BSP_DISPLAY_STATUS_ERROR;
    }
    if (width) *width = d->width;
    if (height) *height = d->height;
    return BSP_DISPLAY_STATUS_SUCCESS;
}

bsp_display_status_t bsp_display_fill(bsp_display_handle_t handle, uint32_t color)
{
    bsp_display_t* d = (bsp_display_t*)handle;
    if (!d || !d->draw_buffer) {
        return BSP_DISPLAY_STATUS_ERROR;
    }

    /* 0xRRGGBB -> RGB565 */
    uint32_t r = (color >> 16) & 0xFFu;
    uint32_t g = (color >> 8) & 0xFFu;
    uint32_t b = color & 0xFFu;
    uint16_t c565 = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));

    uint32_t pixels = (uint32_t)d->width * d->height;
    uint16_t* buf = (uint16_t*)d->draw_buffer;
    for (uint32_t i = 0; i < pixels; i++) {
        buf[i] = c565;
    }

    /* 阻塞式：整屏刷新 */
    return bsp_display_flush(handle, 0, 0, d->width, d->height);
}

uint8_t bsp_display_is_busy(bsp_display_handle_t handle)
{
    bsp_display_t* d = (bsp_display_t*)handle;
    return (d && d->busy) ? 1 : 0;
}

bsp_display_status_t bsp_display_set_brightness(bsp_display_handle_t handle, uint8_t brightness)
{
    bsp_display_t* d = (bsp_display_t*)handle;
    if (!d) {
        return BSP_DISPLAY_STATUS_ERROR;
    }
    d->brightness = (brightness > 100) ? 100 : brightness;
    return BSP_DISPLAY_STATUS_SUCCESS;
}

uint8_t bsp_display_get_brightness(bsp_display_handle_t handle)
{
    bsp_display_t* d = (bsp_display_t*)handle;
    return d ? d->brightness : 0;
}

uint8_t* bsp_display_get_draw_buffer(bsp_display_handle_t handle, uint32_t* buf_size)
{
    bsp_display_t* d = (bsp_display_t*)handle;
    if (!d) {
        if (buf_size) *buf_size = 0;
        return NULL;
    }
    if (buf_size) *buf_size = d->draw_buffer_size;
    return d->draw_buffer;
}

bsp_display_status_t bsp_display_set_draw_buffer(bsp_display_handle_t handle, uint8_t* draw_buffer, uint32_t buf_size)
{
    bsp_display_t* d = (bsp_display_t*)handle;
    if (!d || !draw_buffer) {
        return BSP_DISPLAY_STATUS_ERROR;
    }
    d->draw_buffer = draw_buffer;
    d->draw_buffer_size = buf_size;
    return BSP_DISPLAY_STATUS_SUCCESS;
}

bsp_display_status_t bsp_display_flush(bsp_display_handle_t handle, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    bsp_display_t* d = (bsp_display_t*)handle;
    if (!d || !d->draw_buffer || !d->texture || !d->renderer) {
        return BSP_DISPLAY_STATUS_ERROR;
    }

    /* 区域裁剪 */
    if (x >= d->width || y >= d->height) {
        return BSP_DISPLAY_STATUS_SUCCESS;
    }
    if (x + width > d->width) width = (uint16_t)(d->width - x);
    if (y + height > d->height) height = (uint16_t)(d->height - y);

    SDL_LockMutex(d->lock);
    d->busy = 1;

    /* 逐行 RGB565 -> RGBX8888 转换到临时缓冲，再更新纹理 */
    uint32_t* tmp = (uint32_t*)malloc((size_t)width * height * 4);
    if (!tmp) {
        d->busy = 0;
        SDL_UnlockMutex(d->lock);
        return BSP_DISPLAY_STATUS_ERROR;
    }

    for (uint32_t row = 0; row < height; row++) {
        const uint16_t* src = (const uint16_t*)(d->draw_buffer + ((size_t)(y + row) * d->width + x) * 2);
        uint32_t* dst = tmp + (size_t)row * width;
        for (uint32_t col = 0; col < width; col++) {
            dst[col] = rgb565_to_rgbx(src[col]);
        }
    }

    SDL_Rect rect = { (int)x, (int)y, (int)width, (int)height };
    SDL_UpdateTexture(d->texture, &rect, tmp, (int)width * 4);
    free(tmp);

    SDL_RenderClear(d->renderer);
    SDL_RenderTexture(d->renderer, d->texture, NULL, NULL);
    SDL_RenderPresent(d->renderer);

    d->busy = 0;
    SDL_UnlockMutex(d->lock);

    if (d->flush_finish_cb) {
        d->flush_finish_cb(handle, d->flush_finish_ctx);
    }

    return BSP_DISPLAY_STATUS_SUCCESS;
}

bsp_display_status_t bsp_display_register_flush_finnish_trigger(bsp_display_handle_t handle, void (*callback)(bsp_display_handle_t, void*), void* ctx)
{
    bsp_display_t* d = (bsp_display_t*)handle;
    if (!d) {
        return BSP_DISPLAY_STATUS_ERROR;
    }
    d->flush_finish_cb = callback;
    d->flush_finish_ctx = ctx;
    return BSP_DISPLAY_STATUS_SUCCESS;
}

void bsp_display_wait_flush_finish(bsp_display_handle_t handle)
{
    /* SDL 后端 flush 为同步操作，无需等待 */
    (void)handle;
}

void bsp_display_wait_te_signal(bsp_display_handle_t handle)
{
    /* SDL 后端无 TE 信号 */
    (void)handle;
}

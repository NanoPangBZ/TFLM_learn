/* bsp_display_sdl.c — bsp_display 的 SDL3 后端实现（主窗口内多区域显示）。
 *
 * 参考 N32H762 工程的 bsp_cfg.h：显示器由 type + idx 在映射表里查到自己的
 * 硬件/区域配置。SDL 后端把映射表项解释成 arch 主窗口内的一块矩形：
 *   - 每个 (type, idx) 对应一个独立的 bsp_display 实例（可同时打开多个）；
 *   - 绘制缓存仍为 RGB565；flush 转换到 RGBX8888 staging 并标记脏区；
 *   - 纹理上传 / RenderTexture / Present 全部由 arch 主线程完成。
 *
 * 与旧版的区别：不再创建独立 SDL 窗口，也不再假设全窗口只有一个显示器。
 */
#include "bsp/bsp_display.h"
#include "bsp_cfg.h"
#include "arch/arch_display.h"
#include "framework/e_heap.h"

#include <SDL3/SDL.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "framework/e_log.h"
#define TAG "bsp_display"
#define INFO_LOG(...)   E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

#define MALLOC(size)  e_heap_malloc((size), E_HEAP_ATTR_DEFAULT)

/* 显示器映射表：由 bsp_cfg.h 提供，type + idx 唯一确定一块区域 */
static const bsp_display_map_t s_display_map[] = BSP_DISPLAY_MAP_TABLE;

typedef struct bsp_display_t {
    const bsp_display_map_t* map;

    SDL_Texture* texture;        /* RGBX8888，只在主线程访问 */
    SDL_Mutex*   lock;           /* 保护 stage / dirty 区 */
    uint32_t*    stage;          /* RGBX8888 转换缓冲，整块 width*height */
    uint32_t     stage_size;

    uint8_t* draw_buffer;        /* RGB565 绘制缓存 */
    uint32_t draw_buffer_size;
    uint8_t  draw_buffer_owned;  /* 1: 内部 e_heap 分配；0: 外部 set 进来 */

    uint8_t is_on;
    uint8_t brightness;
    uint8_t busy;
    uint8_t render_registered;

    uint8_t  dirty;
    SDL_Rect dirty_rect;         /* 显示区局部坐标 */

    void (*flush_finish_cb)(bsp_display_handle_t, void*);
    void* flush_finish_ctx;
} bsp_display_t;

/* RGB565 -> RGBX8888（SDL 的 RGBX8888 是 R 在高字节：0xRRGGBBXX） */
static uint32_t rgb565_to_rgbx(uint16_t c)
{
    uint32_t r = (c >> 11) & 0x1Fu;
    uint32_t g = (c >> 5) & 0x3Fu;
    uint32_t b = c & 0x1Fu;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    return (r << 24) | (g << 16) | (b << 8) | 0xFFu;
}

static const bsp_display_map_t* display_map_find(bsp_display_type_t type, uint8_t idx)
{
    for (size_t i = 0; i < sizeof(s_display_map) / sizeof(s_display_map[0]); i++) {
        if (s_display_map[i].type == type && s_display_map[i].idx == idx) {
            return &s_display_map[i];
        }
    }
    return NULL;
}

static void bsp_display_render(SDL_Renderer* renderer, void* ctx);

/* 释放一个 display 实例（主线程） */
static void bsp_display_destroy(bsp_display_t* d)
{
    if (!d) {
        return;
    }

    if (d->render_registered) {
        arch_display_remove_render_cb(bsp_display_render, d);
    }
    if (d->texture) SDL_DestroyTexture(d->texture);
    if (d->lock) SDL_DestroyMutex(d->lock);
    if (d->stage) free(d->stage);
    if (d->draw_buffer && d->draw_buffer_owned) {
        e_heap_free(d->draw_buffer);
    }
    e_heap_free(d);
}

/* arch 主线程帧回调：上传脏区到纹理，再把纹理画到窗口的映射矩形 */
static void bsp_display_render(SDL_Renderer* renderer, void* ctx)
{
    bsp_display_t* d = (bsp_display_t*)ctx;
    if (!d || !d->map || !d->texture || !d->stage) {
        return;
    }

    SDL_LockMutex(d->lock);
    if (d->dirty) {
        SDL_Rect r = d->dirty_rect;
        const uint32_t* src = d->stage + (size_t)r.y * d->map->width + (size_t)r.x;
        SDL_UpdateTexture(d->texture, &r, src, (int)d->map->width * 4);
        d->dirty = 0;
    }
    SDL_UnlockMutex(d->lock);

    if (!d->is_on) {
        return;
    }

    SDL_FRect dst = {
        (float)d->map->x,
        (float)d->map->y,
        (float)d->map->width,
        (float)d->map->height,
    };
    SDL_RenderTexture(renderer, d->texture, NULL, &dst);
}

bsp_display_handle_t bsp_display_open(bsp_display_desc_t* desc)
{
    if (!desc) {
        return NULL;
    }

    const bsp_display_map_t* map = display_map_find(desc->type, desc->idx);
    if (!map) {
        WARN_LOG("unsupported display type=%u idx=%u", (unsigned)desc->type, (unsigned)desc->idx);
        return NULL;
    }

    SDL_Renderer* renderer = arch_display_renderer();
    if (!renderer) {
        ERROR_LOG("arch renderer not ready");
        return NULL;
    }

    bsp_display_t* d = (bsp_display_t*)MALLOC(sizeof(bsp_display_t));
    if (!d) {
        return NULL;
    }
    memset(d, 0, sizeof(*d));
    d->map = map;
    d->is_on = 0;
    d->brightness = 100;

    d->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBX8888,
                                   SDL_TEXTUREACCESS_STREAMING, map->width, map->height);
    if (!d->texture) {
        goto fail;
    }

    d->lock = SDL_CreateMutex();
    if (!d->lock) {
        goto fail;
    }

    d->stage_size = (uint32_t)map->width * map->height * 4U;
    d->stage = (uint32_t*)malloc(d->stage_size);
    if (!d->stage) {
        goto fail;
    }
    memset(d->stage, 0, d->stage_size);
    SDL_UpdateTexture(d->texture, NULL, d->stage, (int)map->width * 4);

    d->draw_buffer_size = (uint32_t)map->width * map->height * 2U;
    d->draw_buffer = (uint8_t*)MALLOC(d->draw_buffer_size);
    if (!d->draw_buffer) {
        goto fail;
    }
    memset(d->draw_buffer, 0, d->draw_buffer_size);
    d->draw_buffer_owned = 1;

    if (arch_display_add_render_cb(bsp_display_render, d) != 0) {
        goto fail;
    }
    d->render_registered = 1;

    INFO_LOG("display idx=%u opened: %ux%u at (%d,%d)",
             (unsigned)map->idx, (unsigned)map->width, (unsigned)map->height,
             (int)map->x, (int)map->y);

    return (bsp_display_handle_t)d;

fail:
    bsp_display_destroy(d);
    return NULL;
}

void bsp_display_close(bsp_display_handle_t handle)
{
    bsp_display_destroy((bsp_display_t*)handle);
}

bsp_display_status_t bsp_display_on(bsp_display_handle_t handle, uint8_t on_off)
{
    bsp_display_t* d = (bsp_display_t*)handle;
    if (!d) {
        return BSP_DISPLAY_STATUS_ERROR;
    }
    d->is_on = on_off ? 1 : 0;
    arch_display_request_frame();
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
    if (width) *width = d->map->width;
    if (height) *height = d->map->height;
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

    uint32_t pixels = (uint32_t)d->map->width * d->map->height;
    uint16_t* buf = (uint16_t*)d->draw_buffer;
    for (uint32_t i = 0; i < pixels; i++) {
        buf[i] = c565;
    }

    /* 阻塞式：整屏刷新 */
    return bsp_display_flush(handle, 0, 0, d->map->width, d->map->height);
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
    if (buf_size < (uint32_t)d->map->width * d->map->height * 2U) {
        return BSP_DISPLAY_STATUS_ERROR;
    }
    d->draw_buffer = draw_buffer;
    d->draw_buffer_size = buf_size;
    d->draw_buffer_owned = 0;
    return BSP_DISPLAY_STATUS_SUCCESS;
}

bsp_display_status_t bsp_display_flush(bsp_display_handle_t handle, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    bsp_display_t* d = (bsp_display_t*)handle;
    if (!d || !d->draw_buffer || !d->stage || !d->texture) {
        return BSP_DISPLAY_STATUS_ERROR;
    }

    const uint16_t disp_w = d->map->width;
    const uint16_t disp_h = d->map->height;

    /* 区域裁剪 */
    if (x >= disp_w || y >= disp_h) {
        return BSP_DISPLAY_STATUS_SUCCESS;
    }
    if (x + width > disp_w) width = (uint16_t)(disp_w - x);
    if (y + height > disp_h) height = (uint16_t)(disp_h - y);
    if (width == 0 || height == 0) {
        return BSP_DISPLAY_STATUS_SUCCESS;
    }

    SDL_LockMutex(d->lock);
    d->busy = 1;

    /* RGB565 -> RGBX8888，直接写进 staging 对应位置 */
    for (uint32_t row = 0; row < height; row++) {
        const uint16_t* src = (const uint16_t*)(d->draw_buffer + ((size_t)(y + row) * disp_w + x) * 2);
        uint32_t* dst = d->stage + (size_t)(y + row) * disp_w + x;
        for (uint32_t col = 0; col < width; col++) {
            dst[col] = rgb565_to_rgbx(src[col]);
        }
    }

    /* 合并脏区（显示区局部坐标） */
    if (d->dirty) {
        int x0 = d->dirty_rect.x < (int)x ? d->dirty_rect.x : (int)x;
        int y0 = d->dirty_rect.y < (int)y ? d->dirty_rect.y : (int)y;
        int x1 = (d->dirty_rect.x + d->dirty_rect.w) > (int)(x + width)
                     ? (d->dirty_rect.x + d->dirty_rect.w) : (int)(x + width);
        int y1 = (d->dirty_rect.y + d->dirty_rect.h) > (int)(y + height)
                     ? (d->dirty_rect.y + d->dirty_rect.h) : (int)(y + height);
        d->dirty_rect.x = x0;
        d->dirty_rect.y = y0;
        d->dirty_rect.w = x1 - x0;
        d->dirty_rect.h = y1 - y0;
    } else {
        d->dirty_rect.x = x;
        d->dirty_rect.y = y;
        d->dirty_rect.w = width;
        d->dirty_rect.h = height;
        d->dirty = 1;
    }

    d->busy = 0;
    SDL_UnlockMutex(d->lock);

    /* 只请求重绘；真正的纹理上传/Present 由 arch 主线程完成 */
    arch_display_request_frame();

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
    /* SDL 后端 flush 为同步操作（只更新内存），无需等待 */
    (void)handle;
}

void bsp_display_wait_te_signal(bsp_display_handle_t handle)
{
    /* SDL 后端无 TE 信号 */
    (void)handle;
}

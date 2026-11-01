/* canvas_service 配套测试：
 * 注册 trigger 回调；首次触发时用 SDL 建预览窗口并绘制，之后每次 trigger 刷新画面。 */
#include "framework/c_function_regedit.h"
#include "service/canvas_service.h"

#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "framework/e_log.h"
#define TAG "canvas_service_test"
#define INFO_LOG(...)   E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

typedef struct preview_ctx_t {
    SDL_Window*   window;
    SDL_Renderer* renderer;
    SDL_Texture*  texture;
    uint32_t*     pixels;   /* RGBX8888 转换缓冲 */
    uint32_t      width;
    uint32_t      height;
    uint32_t      trigger_count;
} preview_ctx_t;

static preview_ctx_t g_preview;

/* 与 bsp_display_sdl 一致：uint32 = 0xRRGGBBAA 语义下的 RGBX8888 */
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

static int preview_ensure_window(preview_ctx_t* p, uint32_t width, uint32_t height)
{
    if (p->window) {
        return 0;
    }

    p->window = SDL_CreateWindow("canvas_service preview", (int)width, (int)height, 0);
    if (!p->window) {
        ERROR_LOG("SDL_CreateWindow failed: %s", SDL_GetError());
        return -1;
    }

    p->renderer = SDL_CreateRenderer(p->window, NULL);
    if (!p->renderer) {
        ERROR_LOG("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(p->window);
        p->window = NULL;
        return -1;
    }

    p->texture = SDL_CreateTexture(p->renderer, SDL_PIXELFORMAT_RGBX8888,
                                   SDL_TEXTUREACCESS_STREAMING, (int)width, (int)height);
    if (!p->texture) {
        ERROR_LOG("SDL_CreateTexture failed: %s", SDL_GetError());
        SDL_DestroyRenderer(p->renderer);
        SDL_DestroyWindow(p->window);
        p->renderer = NULL;
        p->window = NULL;
        return -1;
    }

    p->pixels = (uint32_t*)malloc((size_t)width * height * sizeof(uint32_t));
    if (!p->pixels) {
        ERROR_LOG("malloc preview pixels failed");
        SDL_DestroyTexture(p->texture);
        SDL_DestroyRenderer(p->renderer);
        SDL_DestroyWindow(p->window);
        p->texture = NULL;
        p->renderer = NULL;
        p->window = NULL;
        return -1;
    }

    p->width = width;
    p->height = height;
    INFO_LOG("preview window created %ux%u", (unsigned)width, (unsigned)height);
    return 0;
}

static void preview_draw(preview_ctx_t* p, const canvas_service_trigger_t* trigger)
{
    if (!trigger || !trigger->img_data || trigger->width == 0 || trigger->height == 0) {
        WARN_LOG("invalid trigger payload");
        return;
    }

    if (trigger->img_format != CANVAS_SERVICE_IMG_FORMAT_RGB565) {
        WARN_LOG("unsupported img_format=%u", (unsigned)trigger->img_format);
        return;
    }

    if (preview_ensure_window(p, trigger->width, trigger->height) != 0) {
        return;
    }

    /* 尺寸变化时重建纹理与缓冲 */
    if (p->width != trigger->width || p->height != trigger->height) {
        SDL_DestroyTexture(p->texture);
        free(p->pixels);
        p->texture = SDL_CreateTexture(p->renderer, SDL_PIXELFORMAT_RGBX8888,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       (int)trigger->width, (int)trigger->height);
        p->pixels = (uint32_t*)malloc((size_t)trigger->width * trigger->height * sizeof(uint32_t));
        if (!p->texture || !p->pixels) {
            ERROR_LOG("recreate texture/pixels failed");
            return;
        }
        p->width = trigger->width;
        p->height = trigger->height;
        SDL_SetWindowSize(p->window, (int)p->width, (int)p->height);
    }

    const uint16_t* src = (const uint16_t*)trigger->img_data;
    uint32_t pixels = p->width * p->height;
    for (uint32_t i = 0; i < pixels; i++) {
        p->pixels[i] = rgb565_to_rgbx(src[i]);
    }

    SDL_UpdateTexture(p->texture, NULL, p->pixels, (int)p->width * (int)sizeof(uint32_t));
    SDL_RenderClear(p->renderer);
    SDL_RenderTexture(p->renderer, p->texture, NULL, NULL);
    SDL_RenderPresent(p->renderer);

    p->trigger_count++;
    INFO_LOG("preview updated, trigger_count=%u", (unsigned)p->trigger_count);
}

static void canvas_trigger_cb(const canvas_service_trigger_t* trigger, void* ctx)
{
    (void)ctx;
    preview_draw(&g_preview, trigger);
}

static int canvas_service_test_init(void)
{
    memset(&g_preview, 0, sizeof(g_preview));
    canvas_service_register_trigger_callback(canvas_trigger_cb, NULL);
    INFO_LOG("registered trigger callback (preview window on first trigger)");
    return 0;
}

REGEDIT_AFTER_SERVICE_INIT_FUNCTION(canvas_service_test_init, 0);

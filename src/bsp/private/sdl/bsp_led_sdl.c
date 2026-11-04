/* bsp_led_sdl.c — bsp_led 的 SDL3 后端（固定直径圆点模拟）。
 *
 * 公共 API 与 YouWei 一致：bsp_led_set/get_value(idx)。
 * 映射表 BSP_LED_MAP_TABLE 配每颗灯圆心 (x,y) 与点亮色 color(0xRRGGBB)；
 * 直径固定 16px。value：0=灭，非 0=亮（灭时用同色暗化）。
 */
#include "bsp/bsp_led.h"
#include "bsp_cfg.h"

#include "arch/arch_display.h"

#include "framework/c_function_regedit.h"
#include "framework/e_log.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#define TAG "bsp_led"
#define INFO_LOG(...)  E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)  E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...) E_ERROR_LOG(TAG, __VA_ARGS__)

#ifndef BSP_LED_MAP_TABLE
#error "bsp_cfg.h must define BSP_LED_MAP_TABLE"
#endif

#define BSP_LED_DIAMETER_PX     16
#define BSP_LED_RADIUS_F        8.0f
#define BSP_LED_PI              3.14159265358979323846f

typedef struct bsp_led_map_t {
    int16_t  x;     /* 圆心窗口像素 X */
    int16_t  y;     /* 圆心窗口像素 Y */
    uint32_t color; /* 点亮色 0xRRGGBB */
} bsp_led_map_t;

static const bsp_led_map_t s_led_map[] = BSP_LED_MAP_TABLE;
#define BSP_LED_CNT (sizeof(s_led_map) / sizeof(s_led_map[0]))

static uint32_t s_led_value[BSP_LED_CNT];
static SDL_Mutex* s_led_lock = NULL;
static uint8_t s_render_on = 0;

static void led_draw_filled_disc(SDL_Renderer* r, float cx, float cy, float radius)
{
    int ring;
    int rings = (int)(radius + 0.5f);
    if (rings < 2) {
        rings = 2;
    }
    for (ring = rings; ring >= 1; ring--) {
        float rr = (float)ring;
        int seg = 28;
        float prev_x = cx + rr;
        float prev_y = cy;
        int i;
        for (i = 1; i <= seg; i++) {
            float a = (2.0f * BSP_LED_PI * (float)i) / (float)seg;
            float x = cx + cosf(a) * rr;
            float y = cy + sinf(a) * rr;
            SDL_RenderLine(r, prev_x, prev_y, x, y);
            prev_x = x;
            prev_y = y;
        }
    }
}

static void led_render_all(SDL_Renderer* renderer, void* ctx)
{
    uint32_t snapshot[BSP_LED_CNT];
    size_t i;
    (void)ctx;

    if (!renderer || !s_led_lock) {
        return;
    }

    SDL_LockMutex(s_led_lock);
    memcpy(snapshot, s_led_value, sizeof(snapshot));
    SDL_UnlockMutex(s_led_lock);

    for (i = 0; i < BSP_LED_CNT; i++) {
        const uint32_t c = s_led_map[i].color;
        const uint8_t r = (uint8_t)((c >> 16) & 0xFFu);
        const uint8_t g = (uint8_t)((c >> 8) & 0xFFu);
        const uint8_t b = (uint8_t)(c & 0xFFu);
        float cx = (float)s_led_map[i].x;
        float cy = (float)s_led_map[i].y;
        int on = (snapshot[i] != 0u);

        /* 外环底座 */
        SDL_SetRenderDrawColor(renderer, 48, 54, 62, 255);
        led_draw_filled_disc(renderer, cx, cy, BSP_LED_RADIUS_F);

        if (on) {
            SDL_SetRenderDrawColor(renderer, r, g, b, 255);
            led_draw_filled_disc(renderer, cx, cy, BSP_LED_RADIUS_F - 1.5f);
            /* 高光：向白混合 */
            SDL_SetRenderDrawColor(renderer,
                                   (uint8_t)(r + (255u - r) / 2u),
                                   (uint8_t)(g + (255u - g) / 2u),
                                   (uint8_t)(b + (255u - b) / 2u),
                                   255);
            led_draw_filled_disc(renderer, cx, cy, 3.0f);
        } else {
            /* 灭：同色压暗 */
            SDL_SetRenderDrawColor(renderer,
                                   (uint8_t)(r / 5u),
                                   (uint8_t)(g / 5u),
                                   (uint8_t)(b / 5u),
                                   255);
            led_draw_filled_disc(renderer, cx, cy, BSP_LED_RADIUS_F - 2.0f);
        }
    }
}

void bsp_led_set_value(bsp_led_idx_t led_idx, uint32_t value)
{
    if ((size_t)led_idx >= BSP_LED_CNT) {
        WARN_LOG("led_idx %u out of range (cnt=%u)",
                 (unsigned)led_idx, (unsigned)BSP_LED_CNT);
        return;
    }
    if (!s_led_lock) {
        return;
    }

    SDL_LockMutex(s_led_lock);
    if (s_led_value[led_idx] != value) {
        s_led_value[led_idx] = value;
        SDL_UnlockMutex(s_led_lock);
        arch_display_request_frame();
        return;
    }
    SDL_UnlockMutex(s_led_lock);
}

uint32_t bsp_led_get_value(bsp_led_idx_t led_idx)
{
    uint32_t v;
    if ((size_t)led_idx >= BSP_LED_CNT) {
        WARN_LOG("led_idx %u out of range (cnt=%u)",
                 (unsigned)led_idx, (unsigned)BSP_LED_CNT);
        return 0;
    }
    if (!s_led_lock) {
        return 0;
    }

    SDL_LockMutex(s_led_lock);
    v = s_led_value[led_idx];
    SDL_UnlockMutex(s_led_lock);
    return v;
}

static int bsp_led_init(void)
{
    size_t i;

    s_led_lock = SDL_CreateMutex();
    if (!s_led_lock) {
        ERROR_LOG("create led mutex failed: %s", SDL_GetError());
        return -1;
    }

    memset(s_led_value, 0, sizeof(s_led_value));

    if (arch_display_add_render_cb(led_render_all, NULL) != 0) {
        ERROR_LOG("register led render cb failed");
        SDL_DestroyMutex(s_led_lock);
        s_led_lock = NULL;
        return -1;
    }
    s_render_on = 1;

    for (i = 0; i < BSP_LED_CNT; i++) {
        INFO_LOG("LED[%u] at (%d,%d) d=%d color=#%06X",
                 (unsigned)i, (int)s_led_map[i].x, (int)s_led_map[i].y,
                 BSP_LED_DIAMETER_PX, (unsigned)(s_led_map[i].color & 0xFFFFFFu));
    }
    arch_display_request_frame();
    return 0;
}

static int bsp_led_deinit(void)
{
    if (s_render_on) {
        arch_display_remove_render_cb(led_render_all, NULL);
        s_render_on = 0;
    }
    if (s_led_lock) {
        SDL_DestroyMutex(s_led_lock);
        s_led_lock = NULL;
    }
    return 0;
}

REGEDIT_BSP_INIT_FUNCTION(bsp_led_init, 1);
REGEDIT_BSP_DEINIT_FUNCTION(bsp_led_deinit, 1);

/* bsp_knob_sdl.c — bsp_knob 的 SDL3 后端（电位器旋钮模拟）。
 *
 * 与 YouWei 公共 API 一致：bsp_knob_get(name) / bsp_knob_get_value -> [0,1]。
 *
 * 外观刻意区别于 encoder：
 *   - encoder：全周连续、刻度齿、中心黄指针线；
 *   - knob：有限行程（默认 270°）、两端限位挡块、盘面实心、红点标当前位置。
 *
 * 交互：鼠标在区域内滚轮调节；左键拖动按角度定位（均钳位到限位内）。
 */
#include "bsp/bsp_knob.h"
#include "bsp_cfg.h"
#include "arch/arch_display.h"
#include "arch/arch_input.h"

#include "framework/c_function_regedit.h"
#include "framework/e_log.h"
#include "framework/e_polling_pool.h"
#include "framework/embedded_font.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <math.h>
#include <string.h>

#define TAG "bsp_knob"
#define INFO_LOG(...)   E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

#ifndef BSP_KNOB_MAP_TABLE
#error "bsp_cfg.h must define BSP_KNOB_MAP_TABLE"
#endif

#ifndef BSP_KNOB_POLL_FREQ_HZ
#define BSP_KNOB_POLL_FREQ_HZ   50
#endif

#define BSP_KNOB_PI             3.14159265358979323846f
#define BSP_KNOB_FONT_SIZE      14.0f
/* 有限行程：value=0 在 -TRAVEL/2（相对正上方），value=1 在 +TRAVEL/2，顺时针增大 */
#define BSP_KNOB_TRAVEL_DEG     270.0f
#define BSP_KNOB_WHEEL_STEP     0.02f

typedef struct bsp_knob_map_t {
    const char* name;
    int16_t     x;
    int16_t     y;
    uint16_t    width;
    uint16_t    height;
    float       initial; /* 初始值 0..1；越界时钳位 */
    int8_t      dir;     /* 1: 顺时针增大；-1: 逆时针增大 */
} bsp_knob_map_t;

typedef struct bsp_knob_t {
    const bsp_knob_map_t* map;
    float value;         /* 钳位后的 [0,1] */
    uint8_t dragging;
    float last_angle_deg; /* 拖动时上一帧角度，用于跨过限位时不跳变 */

    SDL_Texture* name_tex;
    int name_w;
    int name_h;
} bsp_knob_t;

static const bsp_knob_map_t s_knob_map[] = BSP_KNOB_MAP_TABLE;
#define BSP_KNOB_CNT (sizeof(s_knob_map) / sizeof(s_knob_map[0]))

static bsp_knob_t s_knobs[BSP_KNOB_CNT];
static e_polling_task_handle_t s_polling_task = NULL;
static SDL_Mutex* s_knob_lock = NULL;
static TTF_Font* s_font = NULL;
static uint8_t s_ttf_ready = 0;

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float knob_value_to_angle_deg(const bsp_knob_t* k)
{
    float t = k->value;
    float half = BSP_KNOB_TRAVEL_DEG * 0.5f;
    float a = -half + t * BSP_KNOB_TRAVEL_DEG;
    if (k->map->dir == -1) {
        a = -a;
    }
    return a; /* 相对正上方，顺时针为正 */
}

static float knob_angle_deg_from_mouse(float cx, float cy, float mx, float my)
{
    /* 正上方为 0，顺时针为正（屏幕 Y 向下） */
    float dx = mx - cx;
    float dy = my - cy;
    float rad = atan2f(dx, -dy);
    return rad * 180.0f / BSP_KNOB_PI;
}

static float knob_angle_to_value(const bsp_knob_t* k, float angle_deg)
{
    float half = BSP_KNOB_TRAVEL_DEG * 0.5f;
    float a = angle_deg;
    if (k->map->dir == -1) {
        a = -a;
    }
    a = clampf(a, -half, half);
    return (a + half) / BSP_KNOB_TRAVEL_DEG;
}

static void knob_geometry(const bsp_knob_t* k, float* out_cx, float* out_cy, float* out_r,
                          float* out_name_area)
{
    const bsp_knob_map_t* m = k->map;
    float name_area = k->name_tex ? (float)k->name_h + 10.0f : 8.0f;
    float body_h = (float)m->height - name_area - 8.0f;
    float radius;

    if (body_h < 20.0f) {
        body_h = 20.0f;
    }
    radius = ((float)m->width < body_h ? (float)m->width : body_h) * 0.5f - 6.0f;
    if (radius < 10.0f) {
        radius = 10.0f;
    }

    *out_cx = (float)m->x + (float)m->width * 0.5f;
    *out_cy = (float)m->y + name_area + body_h * 0.5f;
    *out_r = radius;
    if (out_name_area) {
        *out_name_area = name_area;
    }
}

static int knob_point_inside(const bsp_knob_t* k, int32_t px, int32_t py)
{
    const bsp_knob_map_t* m = k->map;
    return (px >= m->x && px < m->x + (int32_t)m->width &&
            py >= m->y && py < m->y + (int32_t)m->height);
}

/* -------------------------------------------------------------------------- */
/* 字体                                                                       */
/* -------------------------------------------------------------------------- */

static void knob_font_init(void)
{
    if (s_font || s_ttf_ready) {
        return;
    }
    if (!TTF_Init()) {
        WARN_LOG("TTF_Init failed: %s", SDL_GetError());
        return;
    }
    s_ttf_ready = 1;
    s_font = embedded_font_open_dejavu(BSP_KNOB_FONT_SIZE);
    if (!s_font) {
        WARN_LOG("open embedded DejaVuSans failed: %s", SDL_GetError());
    }
}

static void knob_name_texture_create(bsp_knob_t* k, SDL_Renderer* renderer)
{
    SDL_Color color = { 200, 210, 220, 255 };
    SDL_Surface* surface;

    if (!k || !renderer || !s_font || !k->map->name) {
        return;
    }
    surface = TTF_RenderText_Blended(s_font, k->map->name, strlen(k->map->name), color);
    if (!surface) {
        return;
    }
    k->name_tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (k->name_tex) {
        k->name_w = surface->w;
        k->name_h = surface->h;
    }
    SDL_DestroySurface(surface);
}

/* -------------------------------------------------------------------------- */
/* 绘制                                                                       */
/* -------------------------------------------------------------------------- */

static void knob_draw_arc(SDL_Renderer* r, float cx, float cy, float radius,
                          float start_deg, float end_deg, int segments)
{
    float span = end_deg - start_deg;
    float prev_x;
    float prev_y;
    int i;

    if (segments < 8) {
        segments = 8;
    }
    {
        float a0 = (start_deg - 90.0f) * BSP_KNOB_PI / 180.0f;
        prev_x = cx + cosf(a0) * radius;
        prev_y = cy + sinf(a0) * radius;
    }
    for (i = 1; i <= segments; i++) {
        float t = (float)i / (float)segments;
        float deg = start_deg + span * t;
        float a = (deg - 90.0f) * BSP_KNOB_PI / 180.0f;
        float x = cx + cosf(a) * radius;
        float y = cy + sinf(a) * radius;
        SDL_RenderLine(r, prev_x, prev_y, x, y);
        prev_x = x;
        prev_y = y;
    }
}

static void knob_draw_stop(SDL_Renderer* r, float cx, float cy, float radius, float angle_deg)
{
    float a = (angle_deg - 90.0f) * BSP_KNOB_PI / 180.0f;
    float c = cosf(a);
    float s = sinf(a);
    float x1 = cx + c * (radius - 2.0f);
    float y1 = cy + s * (radius - 2.0f);
    float x2 = cx + c * (radius + 7.0f);
    float y2 = cy + s * (radius + 7.0f);
    /* 限位挡块：粗短线 */
    SDL_RenderLine(r, x1, y1, x2, y2);
    SDL_RenderLine(r, x1 + 1.0f, y1, x2 + 1.0f, y2);
}

static void knob_draw_filled_disc(SDL_Renderer* r, float cx, float cy, float radius)
{
    /* 用同心圆近似实心盘（区别于 encoder 的空心刻度盘） */
    int ring;
    int rings = (int)radius;
    if (rings < 4) {
        rings = 4;
    }
    for (ring = rings; ring >= 1; ring -= 2) {
        float rr = (float)ring;
        int seg = 36;
        float prev_x = cx + rr;
        float prev_y = cy;
        int i;
        for (i = 1; i <= seg; i++) {
            float a = (2.0f * BSP_KNOB_PI * (float)i) / (float)seg;
            float x = cx + cosf(a) * rr;
            float y = cy + sinf(a) * rr;
            SDL_RenderLine(r, prev_x, prev_y, x, y);
            prev_x = x;
            prev_y = y;
        }
    }
}

static void knob_draw_dot(SDL_Renderer* r, float cx, float cy, float radius)
{
    SDL_FRect d = { cx - radius, cy - radius, radius * 2.0f, radius * 2.0f };
    SDL_RenderFillRect(r, &d);
}

static void knob_render(SDL_Renderer* renderer, void* ctx)
{
    bsp_knob_t* k = (bsp_knob_t*)ctx;
    const bsp_knob_map_t* m;
    float cx;
    float cy;
    float radius;
    float half = BSP_KNOB_TRAVEL_DEG * 0.5f;
    float stop0;
    float stop1;
    float angle;
    float a;
    float dot_x;
    float dot_y;
    SDL_FRect bg;

    if (!k || !k->map || !renderer) {
        return;
    }
    m = k->map;

    bg.x = (float)m->x;
    bg.y = (float)m->y;
    bg.w = (float)m->width;
    bg.h = (float)m->height;
    /* 略偏暖的底板，区别 encoder 冷灰 */
    SDL_SetRenderDrawColor(renderer, 42, 36, 34, 255);
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, 110, 78, 70, 255);
    SDL_RenderRect(renderer, &bg);

    if (k->name_tex) {
        SDL_FRect dst = {
            (float)m->x + ((float)m->width - (float)k->name_w) * 0.5f,
            (float)m->y + 5.0f,
            (float)k->name_w,
            (float)k->name_h,
        };
        SDL_RenderTexture(renderer, k->name_tex, NULL, &dst);
    }

    knob_geometry(k, &cx, &cy, &radius, NULL);

    /* 实心盘面 */
    if (k->dragging) {
        SDL_SetRenderDrawColor(renderer, 90, 72, 64, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 72, 58, 52, 255);
    }
    knob_draw_filled_disc(renderer, cx, cy, radius - 4.0f);

    /* 行程弧轨（非全周） */
    SDL_SetRenderDrawColor(renderer, 160, 120, 100, 255);
    if (m->dir == -1) {
        knob_draw_arc(renderer, cx, cy, radius, half, -half, 40);
        stop0 = half;
        stop1 = -half;
    } else {
        knob_draw_arc(renderer, cx, cy, radius, -half, half, 40);
        stop0 = -half;
        stop1 = half;
    }

    /* 两端限位挡块 */
    SDL_SetRenderDrawColor(renderer, 220, 200, 180, 255);
    knob_draw_stop(renderer, cx, cy, radius, stop0);
    knob_draw_stop(renderer, cx, cy, radius, stop1);

    /* 外圈描边 */
    SDL_SetRenderDrawColor(renderer, 140, 100, 88, 255);
    knob_draw_arc(renderer, cx, cy, radius, 0.0f, 360.0f, 48);

    /* 红点标记当前位置 */
    angle = knob_value_to_angle_deg(k);
    a = (angle - 90.0f) * BSP_KNOB_PI / 180.0f;
    dot_x = cx + cosf(a) * (radius - 10.0f);
    dot_y = cy + sinf(a) * (radius - 10.0f);
    SDL_SetRenderDrawColor(renderer, 220, 40, 40, 255);
    knob_draw_dot(renderer, dot_x, dot_y, 4.5f);
    SDL_SetRenderDrawColor(renderer, 255, 120, 100, 255);
    knob_draw_dot(renderer, dot_x, dot_y, 2.0f);

    /* 中心暗点（电位器轴感） */
    SDL_SetRenderDrawColor(renderer, 30, 24, 22, 255);
    knob_draw_dot(renderer, cx, cy, 3.0f);
}

/* -------------------------------------------------------------------------- */
/* 轮询                                                                       */
/* -------------------------------------------------------------------------- */

static void knob_polling_task(void* arg)
{
    arch_input_state_t input;
    int32_t wheel;
    uint32_t i;
    int need_redraw = 0;

    (void)arg;
    arch_input_get(&input);
    wheel = arch_input_peek_wheel();

    SDL_LockMutex(s_knob_lock);

    for (i = 0; i < BSP_KNOB_CNT; i++) {
        bsp_knob_t* k = &s_knobs[i];
        float cx;
        float cy;
        float radius;
        int inside = knob_point_inside(k, input.mouse_x, input.mouse_y);
        float old = k->value;

        knob_geometry(k, &cx, &cy, &radius, NULL);

        if (inside && wheel != 0) {
            float step = (float)arch_input_take_wheel() * BSP_KNOB_WHEEL_STEP;
            if (k->map->dir == -1) {
                step = -step;
            }
            k->value = clampf(k->value + step, 0.0f, 1.0f);
            wheel = 0;
        }

        if (input.mouse_left && inside) {
            float ang = knob_angle_deg_from_mouse(cx, cy, (float)input.mouse_x, (float)input.mouse_y);
            float half = BSP_KNOB_TRAVEL_DEG * 0.5f;
            float a = ang;
            if (k->map->dir == -1) {
                a = -a;
            }
            /* 在限位外的死区：保持拖动但不越过限位 */
            if (a < -half) {
                a = -half;
            } else if (a > half) {
                a = half;
            }
            if (k->map->dir == -1) {
                a = -a;
            }
            k->value = knob_angle_to_value(k, a);
            k->dragging = 1;
            k->last_angle_deg = ang;
        } else {
            k->dragging = 0;
        }

        if (k->value != old) {
            need_redraw = 1;
        }
    }

    if (need_redraw) {
        arch_display_request_frame();
    }

    SDL_UnlockMutex(s_knob_lock);
}

/* -------------------------------------------------------------------------- */
/* 公共 API                                                                   */
/* -------------------------------------------------------------------------- */

bsp_knob_handle_t bsp_knob_get(const char* name)
{
    uint32_t i;
    if (!name) {
        return NULL;
    }
    for (i = 0; i < BSP_KNOB_CNT; i++) {
        if (s_knobs[i].map && s_knobs[i].map->name &&
            strcmp(name, s_knobs[i].map->name) == 0) {
            return &s_knobs[i];
        }
    }
    WARN_LOG("bsp_knob_get: name: %s not found", name);
    return NULL;
}

float bsp_knob_get_value(bsp_knob_handle_t handle)
{
    float v;
    if (!handle) {
        return 0.0f;
    }
    SDL_LockMutex(s_knob_lock);
    v = handle->value;
    SDL_UnlockMutex(s_knob_lock);
    return v;
}

/* -------------------------------------------------------------------------- */
/* BSP init                                                                   */
/* -------------------------------------------------------------------------- */

static int bsp_knob_init(void)
{
    SDL_Renderer* renderer;
    uint32_t i;

    s_knob_lock = SDL_CreateMutex();
    if (!s_knob_lock) {
        ERROR_LOG("create mutex failed");
        return -1;
    }

    renderer = arch_display_renderer();
    if (!renderer) {
        ERROR_LOG("arch renderer not ready");
        SDL_DestroyMutex(s_knob_lock);
        s_knob_lock = NULL;
        return -1;
    }

    knob_font_init();
    memset(s_knobs, 0, sizeof(s_knobs));

    for (i = 0; i < BSP_KNOB_CNT; i++) {
        bsp_knob_t* k = &s_knobs[i];
        k->map = &s_knob_map[i];
        k->value = clampf(s_knob_map[i].initial, 0.0f, 1.0f);
        knob_name_texture_create(k, renderer);
        if (arch_display_add_render_cb(knob_render, k) != 0) {
            WARN_LOG("register render cb failed for '%s'",
                     s_knob_map[i].name ? s_knob_map[i].name : "?");
        }
        INFO_LOG("knob '%s' ready: value=%.2f at (%d,%d) %ux%u",
                 s_knob_map[i].name ? s_knob_map[i].name : "?",
                 k->value, (int)s_knob_map[i].x, (int)s_knob_map[i].y,
                 s_knob_map[i].width, s_knob_map[i].height);
    }

    {
        e_polling_task_desc_t poll = {
            .attr = E_POLLING_TASK_ATTR_DEFAULT,
            .freq_hz = BSP_KNOB_POLL_FREQ_HZ,
            .name = "bsp_knob_poll",
            .polling_func = knob_polling_task,
            .start_before_create = 1,
            .user_ctx = NULL,
        };
        s_polling_task = e_polling_task_create(&poll);
        if (!s_polling_task) {
            ERROR_LOG("create polling task failed");
            return -1;
        }
    }

    arch_display_request_frame();
    INFO_LOG("knob polling %u Hz, count=%u", (unsigned)BSP_KNOB_POLL_FREQ_HZ, (unsigned)BSP_KNOB_CNT);
    return 0;
}

static int bsp_knob_deinit(void)
{
    uint32_t i;

    if (s_polling_task) {
        e_polling_task_destroy(s_polling_task);
        s_polling_task = NULL;
    }

    for (i = 0; i < BSP_KNOB_CNT; i++) {
        arch_display_remove_render_cb(knob_render, &s_knobs[i]);
        if (s_knobs[i].name_tex) {
            SDL_DestroyTexture(s_knobs[i].name_tex);
            s_knobs[i].name_tex = NULL;
        }
    }

    if (s_knob_lock) {
        SDL_DestroyMutex(s_knob_lock);
        s_knob_lock = NULL;
    }
    if (s_font) {
        TTF_CloseFont(s_font);
        s_font = NULL;
    }
    /* 不在此 TTF_Quit：encoder 等可能仍占用 SDL_ttf */
    s_ttf_ready = 0;
    return 0;
}

REGEDIT_BSP_INIT_FUNCTION(bsp_knob_init, 2);
REGEDIT_BSP_DEINIT_FUNCTION(bsp_knob_deinit, 2);

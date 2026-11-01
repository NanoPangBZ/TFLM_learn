/* bsp_encoder_sdl.c — bsp_encoder 的 SDL3 后端（鼠标滚轮模拟旋转编码器）。
 *
 * 设计：
 *   - 映射表放在 bsp_cfg.h 的 BSP_ENCODER_MAP_TABLE（名字/位置/尺寸）；
 *   - bsp_encoder_map_t 只在本文件内定义，不污染 bsp_encoder.h 标准接口；
 *   - 每个编码器是 arch 主窗口内的一块旋钮区域，由 arch 帧回调绘制；
 *   - 本文件通过 REGEDIT_BSP_INIT_FUNCTION 主动打开映射表里的编码器，
 *     启动后直接显示到屏幕，不依赖 app/test；
 *   - 不注册 SDL 事件回调：arch 主循环把鼠标位置/滚轮/左键更新到 arch_input，
 *     编码器轮询任务按统一频率读取快照并处理旋转/按键；
 *   - 所有编码器共用一个 e_polling_pool 任务，频率统一为 BSP_ENCODER_POLL_FREQ_HZ。
 */
#include "bsp/bsp_encoder.h"
#include "bsp_cfg.h"
#include "arch/arch_display.h"
#include "arch/arch_input.h"

#include "framework/c_function_regedit.h"
#include "framework/c_list.h"
#include "framework/e_log.h"
#include "framework/e_polling_pool.h"
#include "framework/e_sched.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "bsp_encoder"
#define INFO_LOG(...)   E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

#define BSP_ENCODER_MAX             4
#ifndef BSP_ENCODER_POLL_FREQ_HZ
#define BSP_ENCODER_POLL_FREQ_HZ    50
#endif
#define BSP_ENCODER_PI              3.14159265358979323846f
#define BSP_ENCODER_FONT_SIZE       14.0f
#define BSP_ENCODER_DEG_PER_STEP    20.0f

/* 编码器映射表项：只属于本实现，不污染 bsp_encoder.h 标准接口 */
typedef struct bsp_encoder_map_t {
    bsp_encoder_type_t type;
    uint8_t            idx;
    const char*        name;
    int16_t            x;
    int16_t            y;
    uint16_t           width;
    uint16_t           height;
} bsp_encoder_map_t;

typedef struct trig_reg_info_t {
    bsp_encoder_trigger_cb_t callback;
    void* ctx;
} trig_reg_info_t;

typedef struct bsp_encoder_t {
    bsp_encoder_desc_t desc;
    const bsp_encoder_map_t* map;
    uint8_t open_cnt;

    int32_t position;
    float angle_deg;
    uint8_t btn_pressed;
    uint8_t btn_last;

    list_handle_t trigger_list;
    SDL_Texture* name_tex;
    int name_w;
    int name_h;
} bsp_encoder_t;

static const bsp_encoder_map_t s_encoder_map[] = BSP_ENCODER_MAP_TABLE;

static bsp_encoder_t* s_encoders[BSP_ENCODER_MAX];
static uint32_t s_encoder_count = 0;
static e_polling_task_handle_t s_polling_task = NULL;
static SDL_Mutex* s_encoder_lock = NULL;

static TTF_Font* s_font = NULL;
static uint8_t s_ttf_ready = 0;

static const bsp_encoder_map_t* encoder_map_find(bsp_encoder_type_t type, uint8_t idx)
{
    for (size_t i = 0; i < sizeof(s_encoder_map) / sizeof(s_encoder_map[0]); i++) {
        if (s_encoder_map[i].type == type && s_encoder_map[i].idx == idx) {
            return &s_encoder_map[i];
        }
    }
    return NULL;
}

/* -------------------------------------------------------------------------- */
/* 字体 / 名字                                                                */
/* -------------------------------------------------------------------------- */

static void encoder_font_init(void)
{
    if (s_font || s_ttf_ready) {
        return;
    }
    if (!TTF_Init()) {
        WARN_LOG("TTF_Init failed: %s", SDL_GetError());
        return;
    }
    s_ttf_ready = 1;

    char path[512];
    char* base = SDL_GetBasePath();
    if (base) {
        snprintf(path, sizeof(path), "%sDejaVuSans.ttf", base);
        SDL_free(base);
    } else {
        snprintf(path, sizeof(path), "DejaVuSans.ttf");
    }

    s_font = TTF_OpenFont(path, BSP_ENCODER_FONT_SIZE);
    if (!s_font) {
        WARN_LOG("open font '%s' failed: %s", path, SDL_GetError());
    }
}

static void encoder_name_texture_create(bsp_encoder_t* e, SDL_Renderer* renderer)
{
    if (!e || !renderer || !s_font || !e->map->name) {
        return;
    }

    SDL_Color color = { 225, 230, 235, 255 };
    SDL_Surface* surface = TTF_RenderText_Blended(s_font, e->map->name,
                                                  strlen(e->map->name), color);
    if (!surface) {
        return;
    }

    e->name_tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (e->name_tex) {
        e->name_w = surface->w;
        e->name_h = surface->h;
    }
    SDL_DestroySurface(surface);
}

/* -------------------------------------------------------------------------- */
/* 绘制                                                                       */
/* -------------------------------------------------------------------------- */

static void encoder_draw_circle(SDL_Renderer* r, float cx, float cy, float radius)
{
    const int segments = 48;
    float prev_x = cx + radius;
    float prev_y = cy;

    for (int i = 1; i <= segments; i++) {
        float a = (2.0f * BSP_ENCODER_PI * (float)i) / (float)segments;
        float x = cx + cosf(a) * radius;
        float y = cy + sinf(a) * radius;
        SDL_RenderLine(r, prev_x, prev_y, x, y);
        prev_x = x;
        prev_y = y;
    }
}

static void encoder_render(SDL_Renderer* renderer, void* ctx)
{
    bsp_encoder_t* e = (bsp_encoder_t*)ctx;
    if (!e || !e->map) {
        return;
    }

    const bsp_encoder_map_t* m = e->map;
    SDL_FRect bg = { (float)m->x, (float)m->y, (float)m->width, (float)m->height };
    SDL_SetRenderDrawColor(renderer, 36, 41, 48, 255);
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, 70, 80, 92, 255);
    SDL_RenderRect(renderer, &bg);

    if (e->name_tex) {
        SDL_FRect dst = {
            (float)m->x + ((float)m->width - (float)e->name_w) * 0.5f,
            (float)m->y + 5.0f,
            (float)e->name_w,
            (float)e->name_h,
        };
        SDL_RenderTexture(renderer, e->name_tex, NULL, &dst);
    }

    float name_area = e->name_tex ? (float)e->name_h + 10.0f : 8.0f;
    float knob_h = (float)m->height - name_area - 8.0f;
    if (knob_h < 20.0f) {
        knob_h = 20.0f;
    }
    float radius = ((float)m->width < knob_h ? (float)m->width : knob_h) * 0.5f - 6.0f;
    if (radius < 10.0f) {
        radius = 10.0f;
    }

    const float cx = (float)m->x + (float)m->width * 0.5f;
    const float cy = (float)m->y + name_area + knob_h * 0.5f;

    /* 外圈 + 刻度 */
    if (e->btn_pressed) {
        SDL_SetRenderDrawColor(renderer, 240, 170, 60, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 120, 140, 160, 255);
    }
    encoder_draw_circle(renderer, cx, cy, radius);

    SDL_SetRenderDrawColor(renderer, 80, 92, 105, 255);
    for (int i = 0; i < 12; i++) {
        float a = (2.0f * BSP_ENCODER_PI * (float)i) / 12.0f;
        float x1 = cx + cosf(a) * (radius - 8.0f);
        float y1 = cy + sinf(a) * (radius - 8.0f);
        float x2 = cx + cosf(a) * (radius - 3.0f);
        float y2 = cy + sinf(a) * (radius - 3.0f);
        SDL_RenderLine(renderer, x1, y1, x2, y2);
    }

    /* 指针：0 度朝上 */
    float rad = (e->angle_deg - 90.0f) * BSP_ENCODER_PI / 180.0f;
    float tip_x = cx + cosf(rad) * (radius - 10.0f);
    float tip_y = cy + sinf(rad) * (radius - 10.0f);
    SDL_SetRenderDrawColor(renderer, 240, 210, 120, 255);
    SDL_RenderLine(renderer, cx, cy, tip_x, tip_y);

    /* 中心点 */
    SDL_FRect center = { cx - 2.0f, cy - 2.0f, 4.0f, 4.0f };
    SDL_SetRenderDrawColor(renderer, 210, 220, 230, 255);
    SDL_RenderFillRect(renderer, &center);
}

/* -------------------------------------------------------------------------- */
/* 轮询任务                                                                   */
/* -------------------------------------------------------------------------- */

static void encoder_trigger_fire(bsp_encoder_t* e, int32_t inc, uint32_t now)
{
    if (!e->trigger_list) {
        return;
    }

    bsp_encoder_trigger_t trigger;
    memset(&trigger, 0, sizeof(trigger));

    if (inc > 32767) inc = 32767;
    if (inc < -32768) inc = -32768;

    trigger.inc_value = (int16_t)inc;
    trigger.position_value = (uint32_t)e->position;
    trigger.speed_rps_pulse = (float)inc * (float)BSP_ENCODER_POLL_FREQ_HZ;
    trigger.btn_status = e->btn_pressed;
    trigger.timestamp = now;

    foreach_list(e->trigger_list, slot, trig_reg_info_t)
    {
        if (slot->callback) {
            slot->callback((bsp_encoder_handle_t)e, slot->ctx, &trigger);
        }
    }
}

static void encoder_polling_task(void* arg)
{
    (void)arg;

    arch_input_state_t input;
    arch_input_get(&input);
    int32_t wheel = arch_input_take_wheel();
    uint32_t now = e_sched_time_ms();
    int need_redraw = 0;

    SDL_LockMutex(s_encoder_lock);

    for (uint32_t i = 0; i < s_encoder_count; i++) {
        bsp_encoder_t* e = s_encoders[i];
        const bsp_encoder_map_t* m = e->map;

        int inside = (input.mouse_x >= (int32_t)m->x) &&
                     (input.mouse_x < (int32_t)m->x + (int32_t)m->width) &&
                     (input.mouse_y >= (int32_t)m->y) &&
                     (input.mouse_y < (int32_t)m->y + (int32_t)m->height);

        int32_t inc = 0;
        if (inside && wheel != 0) {
            int dir = (e->desc.dir == -1) ? -1 : 1;
            inc = wheel * dir;
            wheel = 0;

            e->position += inc;
            e->angle_deg += (float)inc * BSP_ENCODER_DEG_PER_STEP;
            while (e->angle_deg >= 360.0f) e->angle_deg -= 360.0f;
            while (e->angle_deg < 0.0f) e->angle_deg += 360.0f;
        }

        uint8_t btn = (inside && input.mouse_left) ? 1u : 0u;
        e->btn_pressed = btn;

        if (inc != 0 || btn != e->btn_last) {
            encoder_trigger_fire(e, inc, now);
            e->btn_last = btn;
            need_redraw = 1;
        }
    }

    if (need_redraw) {
        arch_display_request_frame();
    }

    SDL_UnlockMutex(s_encoder_lock);
}

/* -------------------------------------------------------------------------- */
/* 对外接口                                                                   */
/* -------------------------------------------------------------------------- */

bsp_encoder_handle_t bsp_encoder_open(bsp_encoder_desc_t* desc)
{
    if (!desc) {
        return NULL;
    }

    SDL_LockMutex(s_encoder_lock);

    for (uint32_t i = 0; i < s_encoder_count; i++) {
        bsp_encoder_t* e = s_encoders[i];
        if (e->desc.type == desc->type && e->desc.idx == desc->idx) {
            e->open_cnt++;
            SDL_UnlockMutex(s_encoder_lock);
            return (bsp_encoder_handle_t)e;
        }
    }

    if (s_encoder_count >= BSP_ENCODER_MAX) {
        SDL_UnlockMutex(s_encoder_lock);
        ERROR_LOG("too many encoders");
        return NULL;
    }

    SDL_UnlockMutex(s_encoder_lock);

    const bsp_encoder_map_t* map = encoder_map_find(desc->type, desc->idx);
    if (!map) {
        WARN_LOG("unsupported encoder type=%u idx=%u", (unsigned)desc->type, (unsigned)desc->idx);
        return NULL;
    }

    SDL_Renderer* renderer = arch_display_renderer();
    if (!renderer) {
        return NULL;
    }

    bsp_encoder_t* e = (bsp_encoder_t*)calloc(1, sizeof(*e));
    if (!e) {
        return NULL;
    }

    e->desc = *desc;
    e->map = map;
    e->open_cnt = 1;
    e->position = 0;
    e->angle_deg = 0.0f;
    e->trigger_list = list_create(sizeof(trig_reg_info_t));
    if (!e->trigger_list) {
        free(e);
        return NULL;
    }

    encoder_font_init();
    encoder_name_texture_create(e, renderer);

    if (arch_display_add_render_cb(encoder_render, e) != 0) {
        list_destroyed(e->trigger_list);
        if (e->name_tex) SDL_DestroyTexture(e->name_tex);
        free(e);
        return NULL;
    }

    SDL_LockMutex(s_encoder_lock);
    s_encoders[s_encoder_count++] = e;
    SDL_UnlockMutex(s_encoder_lock);

    INFO_LOG("encoder '%s' open: type=%u idx=%u at (%d,%d) %ux%u",
             map->name ? map->name : "?",
             (unsigned)desc->type, (unsigned)desc->idx,
             (int)map->x, (int)map->y, (unsigned)map->width, (unsigned)map->height);

    return (bsp_encoder_handle_t)e;
}

void bsp_encoder_close(bsp_encoder_handle_t handle)
{
    bsp_encoder_t* e = (bsp_encoder_t*)handle;
    if (!e) {
        return;
    }

    SDL_LockMutex(s_encoder_lock);

    if (e->open_cnt > 1) {
        e->open_cnt--;
        SDL_UnlockMutex(s_encoder_lock);
        return;
    }

    for (uint32_t i = 0; i < s_encoder_count; i++) {
        if (s_encoders[i] == e) {
            for (uint32_t j = i + 1; j < s_encoder_count; j++) {
                s_encoders[j - 1] = s_encoders[j];
            }
            s_encoder_count--;
            break;
        }
    }

    arch_display_remove_render_cb(encoder_render, e);
    if (e->name_tex) {
        SDL_DestroyTexture(e->name_tex);
    }
    if (e->trigger_list) {
        list_destroyed(e->trigger_list);
    }
    free(e);

    SDL_UnlockMutex(s_encoder_lock);
}

bsp_encoder_status_t bsp_encoder_register_trigger_callback(bsp_encoder_handle_t handle,
                                                           bsp_encoder_trigger_cb_t callback,
                                                           void* ctx)
{
    bsp_encoder_t* e = (bsp_encoder_t*)handle;
    if (!e || !callback || !e->trigger_list) {
        return BSP_ENCODER_STATUS_ERROR;
    }

    trig_reg_info_t reg = {
        .callback = callback,
        .ctx = ctx,
    };
    if (!list_add_element(e->trigger_list, &reg)) {
        return BSP_ENCODER_STATUS_ERROR;
    }
    return BSP_ENCODER_STATUS_SUCCESS;
}

uint32_t bsp_encoder_position(bsp_encoder_handle_t handle)
{
    bsp_encoder_t* e = (bsp_encoder_t*)handle;
    return e ? (uint32_t)e->position : 0;
}

bsp_encoder_status_t bsp_encoder_reset_position(bsp_encoder_handle_t handle)
{
    bsp_encoder_t* e = (bsp_encoder_t*)handle;
    if (!e) {
        return BSP_ENCODER_STATUS_ERROR;
    }
    e->position = 0;
    e->angle_deg = 0.0f;
    arch_display_request_frame();
    return BSP_ENCODER_STATUS_SUCCESS;
}

uint8_t bsp_encoder_is_btn_pressed(bsp_encoder_handle_t handle)
{
    bsp_encoder_t* e = (bsp_encoder_t*)handle;
    return e ? e->btn_pressed : 0;
}

/* -------------------------------------------------------------------------- */
/* BSP 初始化 / 反初始化                                                      */
/* -------------------------------------------------------------------------- */

static int bsp_encoder_init(void)
{
    s_encoder_lock = SDL_CreateMutex();
    if (!s_encoder_lock) {
        ERROR_LOG("create encoder mutex failed");
        return -1;
    }

    /* 主动打开映射表里的编码器：BSP 初始化完就显示到 arch 窗口 */
    for (size_t i = 0; i < sizeof(s_encoder_map) / sizeof(s_encoder_map[0]); i++) {
        bsp_encoder_desc_t desc = {
            .type = s_encoder_map[i].type,
            .idx = s_encoder_map[i].idx,
            .attr = BSP_ENCODER_ATTR_DEFAULT,
            .dir = 0,
        };
        if (!bsp_encoder_open(&desc)) {
            WARN_LOG("auto open encoder '%s' failed",
                     s_encoder_map[i].name ? s_encoder_map[i].name : "?");
        }
    }

    e_polling_task_desc_t desc = {
        .attr = E_POLLING_TASK_ATTR_DEFAULT,
        .freq_hz = BSP_ENCODER_POLL_FREQ_HZ,
        .name = "bsp_encoder_poll",
        .polling_func = encoder_polling_task,
        .start_before_create = 1,
        .user_ctx = NULL,
    };

    s_polling_task = e_polling_task_create(&desc);
    if (!s_polling_task) {
        ERROR_LOG("create polling task failed");
        while (s_encoder_count > 0) {
            bsp_encoder_close((bsp_encoder_handle_t)s_encoders[s_encoder_count - 1]);
        }
        SDL_DestroyMutex(s_encoder_lock);
        s_encoder_lock = NULL;
        return -1;
    }

    INFO_LOG("encoder polling %u Hz", (unsigned)BSP_ENCODER_POLL_FREQ_HZ);
    return 0;
}

static int bsp_encoder_deinit(void)
{
    if (s_polling_task) {
        e_polling_task_destroy(s_polling_task);
        s_polling_task = NULL;
    }

    while (s_encoder_count > 0) {
        bsp_encoder_close((bsp_encoder_handle_t)s_encoders[s_encoder_count - 1]);
    }

    if (s_encoder_lock) {
        SDL_DestroyMutex(s_encoder_lock);
        s_encoder_lock = NULL;
    }

    if (s_font) {
        TTF_CloseFont(s_font);
        s_font = NULL;
    }
    if (s_ttf_ready) {
        TTF_Quit();
        s_ttf_ready = 0;
    }
    return 0;
}

REGEDIT_BSP_INIT_FUNCTION(bsp_encoder_init, 1);
REGEDIT_BSP_DEINIT_FUNCTION(bsp_encoder_deinit, 1);

/* bsp_keyboard_sdl.c — bsp_keyboard 的 SDL3 后端（鼠标点击模拟按键）。
 *
 * 设计：
 *   - 映射表在 bsp_cfg.h 的 BSP_KEYBOARD_MAP_TABLE；
 *   - bsp_keyboard_map_t 只在本文件内定义，不污染 bsp_keyboard.h；
 *   - 面板外框由 map.x/y/width/height 指定；键格铺满可用区；
 *   - cols=0 时按面板长宽比自动选列数，使单元格接近正方形；
 *     cols>0 时强制列数，行数 = ceil(key_count / cols)；
 *   - key_count 为有效键数（1..32，对应 uint32_t key_mask），只画有效键；
 *   - REGEDIT_BSP_INIT_FUNCTION 主动打开映射表里的键盘；
 *   - 轮询读取 arch_input：鼠标左键按住且落在某键上则该位为 1。
 */
#include "bsp/bsp_keyboard.h"
#include "bsp_cfg.h"
#include "arch/arch_display.h"
#include "arch/arch_input.h"

#include "framework/c_function_regedit.h"
#include "framework/c_list.h"
#include "framework/e_log.h"
#include "framework/e_polling_pool.h"
#include "framework/e_sched.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "bsp_keyboard"
#define INFO_LOG(...)   E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

#define BSP_KEYBOARD_MAX                2
#define BSP_KEYBOARD_KEY_MAX            32
#ifndef BSP_KEYBOARD_POLL_FREQ_HZ
#define BSP_KEYBOARD_POLL_FREQ_HZ       50
#endif
#define BSP_KEYBOARD_TITLE_H            16
#define BSP_KEYBOARD_PAD                4
#define BSP_KEYBOARD_GAP                3

typedef struct bsp_keyboard_map_t {
    bsp_keyboard_type_t type;
    uint8_t             idx;
    const char*         name;
    int16_t             x;
    int16_t             y;
    uint16_t            width;     /* 面板宽（像素），由 bsp_cfg 指定 */
    uint16_t            height;    /* 面板高（像素），由 bsp_cfg 指定 */
    uint8_t             key_count; /* 有效键数 1..32；0 视为 1 */
    uint8_t             cols;      /* 列数；0=按面板长宽比自适应 */
} bsp_keyboard_map_t;

typedef struct trig_reg_info_t {
    bsp_keyboard_trigger_cb_t callback;
    void* ctx;
} trig_reg_info_t;

typedef struct bsp_keyboard_t {
    bsp_keyboard_desc_t desc;
    const bsp_keyboard_map_t* map;
    uint8_t open_cnt;

    uint32_t key_mask;
    uint8_t  key_count;
    uint8_t  cols;
    uint8_t  rows;
    uint8_t  title_h;

    list_handle_t trigger_list;
} bsp_keyboard_t;

#ifndef BSP_KEYBOARD_MAP_TABLE
#error "bsp_cfg.h must define BSP_KEYBOARD_MAP_TABLE"
#endif

static const bsp_keyboard_map_t s_keyboard_map[] = BSP_KEYBOARD_MAP_TABLE;

static bsp_keyboard_t* s_keyboards[BSP_KEYBOARD_MAX];
static uint32_t s_keyboard_count = 0;
static e_polling_task_handle_t s_polling_task = NULL;
static SDL_Mutex* s_keyboard_lock = NULL;

static const bsp_keyboard_map_t* keyboard_map_find(bsp_keyboard_type_t type, uint8_t idx)
{
    size_t i;
    for (i = 0; i < sizeof(s_keyboard_map) / sizeof(s_keyboard_map[0]); i++) {
        if (s_keyboard_map[i].type == type && s_keyboard_map[i].idx == idx) {
            return &s_keyboard_map[i];
        }
    }
    return NULL;
}

static uint8_t keyboard_effective_count(const bsp_keyboard_map_t* map)
{
    uint8_t n = map->key_count;
    if (n == 0u) {
        n = 1u;
    }
    if (n > BSP_KEYBOARD_KEY_MAX) {
        n = BSP_KEYBOARD_KEY_MAX;
    }
    return n;
}

static uint8_t keyboard_title_h(const bsp_keyboard_map_t* map)
{
    if (map->height < 40u) {
        return 0u;
    }
    if (map->height < (uint16_t)(BSP_KEYBOARD_TITLE_H * 3)) {
        return (uint8_t)(map->height / 6u);
    }
    return (uint8_t)BSP_KEYBOARD_TITLE_H;
}

/* 按面板可用区长宽比选列数，使单元格接近正方形 */
static void keyboard_grid_dims(const bsp_keyboard_map_t* map, uint8_t key_count,
                               uint8_t title_h, uint8_t* out_cols, uint8_t* out_rows)
{
    float grid_w = (float)map->width - 2.0f * (float)BSP_KEYBOARD_PAD;
    float grid_h = (float)map->height - (float)title_h - 2.0f * (float)BSP_KEYBOARD_PAD;
    uint8_t cols;
    uint8_t rows;

    if (grid_w < 1.0f) {
        grid_w = 1.0f;
    }
    if (grid_h < 1.0f) {
        grid_h = 1.0f;
    }

    if (map->cols > 0u) {
        cols = map->cols;
        if (cols > key_count) {
            cols = key_count;
        }
        if (cols > BSP_KEYBOARD_KEY_MAX) {
            cols = BSP_KEYBOARD_KEY_MAX;
        }
    } else {
        float aspect = grid_w / grid_h;
        int c = (int)ceilf(sqrtf((float)key_count * aspect));
        if (c < 1) {
            c = 1;
        }
        if (c > (int)key_count) {
            c = (int)key_count;
        }
        if (c > BSP_KEYBOARD_KEY_MAX) {
            c = BSP_KEYBOARD_KEY_MAX;
        }
        cols = (uint8_t)c;
    }

    rows = (uint8_t)((key_count + cols - 1u) / cols);
    if (rows < 1u) {
        rows = 1u;
    }

    *out_cols = cols;
    *out_rows = rows;
}

/* 键 i 在面板内的矩形（铺满可用网格） */
static void keyboard_key_rect(const bsp_keyboard_t* k, uint8_t key_idx, SDL_FRect* out)
{
    const bsp_keyboard_map_t* map = k->map;
    float grid_x = (float)map->x + (float)BSP_KEYBOARD_PAD;
    float grid_y = (float)map->y + (float)k->title_h + (float)BSP_KEYBOARD_PAD;
    float grid_w = (float)map->width - 2.0f * (float)BSP_KEYBOARD_PAD;
    float grid_h = (float)map->height - (float)k->title_h - 2.0f * (float)BSP_KEYBOARD_PAD;
    float gap = (float)BSP_KEYBOARD_GAP;
    float cell_w;
    float cell_h;
    uint8_t col = (uint8_t)(key_idx % k->cols);
    uint8_t row = (uint8_t)(key_idx / k->cols);

    if (grid_w < 1.0f) {
        grid_w = 1.0f;
    }
    if (grid_h < 1.0f) {
        grid_h = 1.0f;
    }
    if (k->cols > 1u) {
        cell_w = (grid_w - (float)(k->cols - 1u) * gap) / (float)k->cols;
    } else {
        cell_w = grid_w;
    }
    if (k->rows > 1u) {
        cell_h = (grid_h - (float)(k->rows - 1u) * gap) / (float)k->rows;
    } else {
        cell_h = grid_h;
    }
    if (cell_w < 2.0f) {
        cell_w = 2.0f;
    }
    if (cell_h < 2.0f) {
        cell_h = 2.0f;
    }

    out->x = grid_x + (float)col * (cell_w + gap);
    out->y = grid_y + (float)row * (cell_h + gap);
    out->w = cell_w;
    out->h = cell_h;
}

static int keyboard_hit_key(const bsp_keyboard_t* k, int32_t px, int32_t py)
{
    uint8_t i;
    if (px < 0 || py < 0) {
        return -1;
    }
    for (i = 0; i < k->key_count; i++) {
        SDL_FRect r;
        keyboard_key_rect(k, i, &r);
        if ((float)px >= r.x && (float)px < r.x + r.w &&
            (float)py >= r.y && (float)py < r.y + r.h) {
            return (int)i;
        }
    }
    return -1;
}

/* -------------------------------------------------------------------------- */
/* 渲染                                                                       */
/* -------------------------------------------------------------------------- */

static void keyboard_render(struct SDL_Renderer* renderer, void* ctx)
{
    bsp_keyboard_t* k = (bsp_keyboard_t*)ctx;
    const bsp_keyboard_map_t* m;
    SDL_FRect bg;
    const char* title;
    uint8_t i;
    char label[8];

    if (!k || !k->map || !renderer) {
        return;
    }
    m = k->map;

    bg.x = (float)m->x;
    bg.y = (float)m->y;
    bg.w = (float)m->width;
    bg.h = (float)m->height;
    SDL_SetRenderDrawColor(renderer, 36, 41, 48, 255);
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, 70, 80, 92, 255);
    SDL_RenderRect(renderer, &bg);

    if (k->title_h > 0u) {
        title = m->name ? m->name : "KEY";
        SDL_SetRenderDrawColor(renderer, 200, 210, 220, 255);
        SDL_RenderDebugText(renderer, (float)m->x + 6.0f, (float)m->y + 3.0f, title);
    }

    for (i = 0; i < k->key_count; i++) {
        SDL_FRect key;
        int pressed = (k->key_mask & (1u << i)) ? 1 : 0;
        float label_x;
        float label_y;
        keyboard_key_rect(k, i, &key);

        if (pressed) {
            SDL_SetRenderDrawColor(renderer, 240, 170, 60, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 55, 64, 74, 255);
        }
        SDL_RenderFillRect(renderer, &key);
        SDL_SetRenderDrawColor(renderer, 90, 105, 120, 255);
        SDL_RenderRect(renderer, &key);

        if (key.w >= (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE &&
            key.h >= (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) {
            snprintf(label, sizeof(label), "%u", (unsigned)i);
            label_x = key.x + (key.w - (float)(strlen(label) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE)) * 0.5f;
            label_y = key.y + (key.h - (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) * 0.5f;
            SDL_SetRenderDrawColor(renderer,
                                   pressed ? 30 : 190,
                                   pressed ? 30 : 200,
                                   pressed ? 30 : 210,
                                   255);
            SDL_RenderDebugText(renderer, label_x, label_y, label);
        }
    }
}

/* -------------------------------------------------------------------------- */
/* 轮询                                                                       */
/* -------------------------------------------------------------------------- */

static void keyboard_trigger_fire(bsp_keyboard_t* k, uint32_t now)
{
    if (!k->trigger_list) {
        return;
    }
    foreach_list(k->trigger_list, slot, trig_reg_info_t)
    {
        if (slot->callback) {
            slot->callback((bsp_keyboard_handle_t)k, slot->ctx, k->key_mask, now);
        }
    }
}

static void keyboard_polling_task(void* arg)
{
    arch_input_state_t input;
    uint32_t now;
    uint32_t i;
    int need_redraw = 0;

    (void)arg;
    arch_input_get(&input);
    now = e_sched_time_ms();

    SDL_LockMutex(s_keyboard_lock);

    for (i = 0; i < s_keyboard_count; i++) {
        bsp_keyboard_t* k = s_keyboards[i];
        uint32_t mask = 0;
        int hit;

        if (input.mouse_left) {
            hit = keyboard_hit_key(k, input.mouse_x, input.mouse_y);
            if (hit >= 0) {
                mask = (1u << (uint32_t)hit);
            }
        }

        if (mask != k->key_mask) {
            k->key_mask = mask;
            keyboard_trigger_fire(k, now);
            need_redraw = 1;
        }
    }

    if (need_redraw) {
        arch_display_request_frame();
    }

    SDL_UnlockMutex(s_keyboard_lock);
}

/* -------------------------------------------------------------------------- */
/* 对外接口                                                                   */
/* -------------------------------------------------------------------------- */

bsp_keyboard_handle_t bsp_keyboard_open(bsp_keyboard_desc_t* desc)
{
    const bsp_keyboard_map_t* map;
    bsp_keyboard_t* k;
    uint32_t i;

    if (!desc) {
        return NULL;
    }

    map = keyboard_map_find(desc->type, desc->idx);
    if (!map) {
        ERROR_LOG("no map for type=%u idx=%u", desc->type, desc->idx);
        return NULL;
    }

    SDL_LockMutex(s_keyboard_lock);
    for (i = 0; i < s_keyboard_count; i++) {
        if (s_keyboards[i]->desc.type == desc->type &&
            s_keyboards[i]->desc.idx == desc->idx) {
            s_keyboards[i]->open_cnt++;
            SDL_UnlockMutex(s_keyboard_lock);
            return (bsp_keyboard_handle_t)s_keyboards[i];
        }
    }

    if (s_keyboard_count >= BSP_KEYBOARD_MAX) {
        ERROR_LOG("keyboard table full");
        SDL_UnlockMutex(s_keyboard_lock);
        return NULL;
    }

    k = (bsp_keyboard_t*)calloc(1, sizeof(bsp_keyboard_t));
    if (!k) {
        SDL_UnlockMutex(s_keyboard_lock);
        return NULL;
    }
    k->desc = *desc;
    k->map = map;
    k->open_cnt = 1;
    k->key_count = keyboard_effective_count(map);
    k->title_h = keyboard_title_h(map);
    keyboard_grid_dims(map, k->key_count, k->title_h, &k->cols, &k->rows);
    k->key_mask = 0;
    k->trigger_list = list_create(sizeof(trig_reg_info_t));
    if (!k->trigger_list) {
        free(k);
        SDL_UnlockMutex(s_keyboard_lock);
        return NULL;
    }

    if (arch_display_add_render_cb(keyboard_render, k) != 0) {
        WARN_LOG("register render cb failed");
    }

    s_keyboards[s_keyboard_count++] = k;
    SDL_UnlockMutex(s_keyboard_lock);

    INFO_LOG("keyboard '%s' open: type=%u idx=%u keys=%u grid=%ux%u at (%d,%d) %ux%u",
             map->name ? map->name : "?",
             desc->type, desc->idx, k->key_count, k->cols, k->rows,
             (int)map->x, (int)map->y, map->width, map->height);
    arch_display_request_frame();
    return (bsp_keyboard_handle_t)k;
}

void bsp_keyboard_close(bsp_keyboard_handle_t handle)
{
    bsp_keyboard_t* k = (bsp_keyboard_t*)handle;
    uint32_t i;

    if (!k) {
        return;
    }

    SDL_LockMutex(s_keyboard_lock);
    if (k->open_cnt > 1) {
        k->open_cnt--;
        SDL_UnlockMutex(s_keyboard_lock);
        return;
    }

    arch_display_remove_render_cb(keyboard_render, k);
    if (k->trigger_list) {
        list_destroyed(k->trigger_list);
        k->trigger_list = NULL;
    }

    for (i = 0; i < s_keyboard_count; i++) {
        if (s_keyboards[i] == k) {
            for (; i + 1 < s_keyboard_count; i++) {
                s_keyboards[i] = s_keyboards[i + 1];
            }
            s_keyboard_count--;
            break;
        }
    }
    free(k);
    SDL_UnlockMutex(s_keyboard_lock);
    arch_display_request_frame();
}

int bsp_keyboard_register_trigger_callback(bsp_keyboard_handle_t handle,
                                           bsp_keyboard_trigger_cb_t cb,
                                           void* ctx)
{
    bsp_keyboard_t* k = (bsp_keyboard_t*)handle;
    trig_reg_info_t reg;

    if (!k || !cb || !k->trigger_list) {
        return -1;
    }
    reg.callback = cb;
    reg.ctx = ctx;
    if (list_add_element(k->trigger_list, &reg) == NULL) {
        return -1;
    }
    return 0;
}

uint32_t bsp_keyboard_get_key_mask(bsp_keyboard_handle_t handle)
{
    bsp_keyboard_t* k = (bsp_keyboard_t*)handle;
    uint32_t mask;

    if (!k) {
        return 0;
    }
    SDL_LockMutex(s_keyboard_lock);
    mask = k->key_mask;
    SDL_UnlockMutex(s_keyboard_lock);
    return mask;
}

/* -------------------------------------------------------------------------- */
/* BSP init                                                                   */
/* -------------------------------------------------------------------------- */

static int bsp_keyboard_init(void)
{
    size_t i;

    s_keyboard_lock = SDL_CreateMutex();
    if (!s_keyboard_lock) {
        ERROR_LOG("create mutex failed");
        return -1;
    }

    for (i = 0; i < sizeof(s_keyboard_map) / sizeof(s_keyboard_map[0]); i++) {
        bsp_keyboard_desc_t desc = {
            .type = s_keyboard_map[i].type,
            .idx = s_keyboard_map[i].idx,
            .attr = BSP_KEYBOARD_ATTR_DEFAULT,
        };
        if (!bsp_keyboard_open(&desc)) {
            WARN_LOG("auto open keyboard '%s' failed",
                     s_keyboard_map[i].name ? s_keyboard_map[i].name : "?");
        }
    }

    {
        e_polling_task_desc_t poll = {
            .attr = E_POLLING_TASK_ATTR_DEFAULT,
            .freq_hz = BSP_KEYBOARD_POLL_FREQ_HZ,
            .name = "bsp_keyboard_poll",
            .polling_func = keyboard_polling_task,
            .start_before_create = 1,
            .user_ctx = NULL,
        };
        s_polling_task = e_polling_task_create(&poll);
        if (!s_polling_task) {
            ERROR_LOG("create polling task failed");
            while (s_keyboard_count > 0) {
                bsp_keyboard_close((bsp_keyboard_handle_t)s_keyboards[s_keyboard_count - 1]);
            }
            SDL_DestroyMutex(s_keyboard_lock);
            s_keyboard_lock = NULL;
            return -1;
        }
    }

    INFO_LOG("keyboard polling %u Hz", (unsigned)BSP_KEYBOARD_POLL_FREQ_HZ);
    return 0;
}

static int bsp_keyboard_deinit(void)
{
    if (s_polling_task) {
        e_polling_task_destroy(s_polling_task);
        s_polling_task = NULL;
    }
    while (s_keyboard_count > 0) {
        bsp_keyboard_close((bsp_keyboard_handle_t)s_keyboards[s_keyboard_count - 1]);
    }
    if (s_keyboard_lock) {
        SDL_DestroyMutex(s_keyboard_lock);
        s_keyboard_lock = NULL;
    }
    return 0;
}

REGEDIT_BSP_INIT_FUNCTION(bsp_keyboard_init, 1);
REGEDIT_BSP_DEINIT_FUNCTION(bsp_keyboard_deinit, 1);

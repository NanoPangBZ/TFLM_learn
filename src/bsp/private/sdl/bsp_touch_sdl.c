/* bsp_touch_sdl.c — bsp_touch 的 SDL3 后端实现（主窗口内多区域触摸）。
 *
 * 参考 N32H762 工程的 bsp_cfg.h：触摸屏由 type + idx 在映射表里查到自己的
 * 硬件/区域配置。SDL 后端把映射表项解释成 arch 主窗口内的一块矩形：
 *   - 每个 (type, idx) 对应一个独立的触摸实例，可以同时打开多个；
 *   - 鼠标左键只有落在该实例的矩形内才产生触摸点；
 *   - 坐标从窗口坐标经 arch 的 DPI 映射换算，再减去该实例区域原点；
 *   - count/get_touch_point 可能从轮询线程调用，故用 SDL_Mutex 保护状态。
 */
#include "bsp/bsp_touch.h"
#include "bsp_cfg.h"
#include "arch/arch_display.h"
#include "framework/e_heap.h"

#include <SDL3/SDL.h>
#include <stddef.h>
#include <string.h>

#include "framework/e_log.h"
#define TAG "bsp_touch"
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)

#define MALLOC(size)  e_heap_malloc((size), E_HEAP_ATTR_DEFAULT)

/* 触摸区域映射表项：只属于本实现，不污染 bsp_touch.h 标准接口。
 * width/height 为逻辑分辨率（与配对 display 的 get_size 一致）；
 * rotation 为呈现顺时针角度（度），命中用旋转后 AABB，回报坐标做逆变换。 */
typedef struct bsp_touch_map_t {
    bsp_touch_type_t type;
    uint8_t          idx;
    int16_t          x;
    int16_t          y;
    uint16_t         width;
    uint16_t         height;
    uint16_t         rotation;
} bsp_touch_map_t;

/* 触摸区域映射表：由 bsp_cfg.h 提供，type + idx 唯一确定一块区域 */
static const bsp_touch_map_t s_touch_map[] = BSP_TOUCH_MAP_TABLE;

typedef struct bsp_touch_t {
    const bsp_touch_map_t* map;
    SDL_Mutex*        lock;
    uint8_t           button_down;   /* 鼠标左键是否按下 */
    uint8_t           inside;        /* 当前点是否落在触摸区域内 */
    bsp_touch_point_t point;         /* 触摸区局部坐标 */
} bsp_touch_t;

static const bsp_touch_map_t* touch_map_find(bsp_touch_type_t type, uint8_t idx)
{
    for (size_t i = 0; i < sizeof(s_touch_map) / sizeof(s_touch_map[0]); i++) {
        if (s_touch_map[i].type == type && s_touch_map[i].idx == idx) {
            return &s_touch_map[i];
        }
    }
    return NULL;
}

/* 窗口坐标 -> 逻辑触摸坐标；命中旋转后 AABB，再逆旋转到逻辑系 */
static void touch_update_point(bsp_touch_t* t, float window_x, float window_y)
{
    int px = (int)window_x;
    int py = (int)window_y;

    if (arch_display_window_to_pixel(window_x, window_y, &px, &py) != 0) {
        t->inside = 0;
        return;
    }

    const bsp_touch_map_t* m = t->map;
    const uint16_t rot = m->rotation % 360u;
    const int swap_wh = (rot == 90u || rot == 270u);
    const int out_w = swap_wh ? (int)m->height : (int)m->width;
    const int out_h = swap_wh ? (int)m->width : (int)m->height;
    const int w = (int)m->width;
    const int h = (int)m->height;

    const int sx = px - m->x;
    const int sy = py - m->y;
    if (sx < 0 || sy < 0 || sx >= out_w || sy >= out_h) {
        t->inside = 0;
        return;
    }

    int lx;
    int ly;
    switch (rot) {
        case 90:
            lx = sy;
            ly = h - 1 - sx;
            break;
        case 180:
            lx = w - 1 - sx;
            ly = h - 1 - sy;
            break;
        case 270:
            lx = w - 1 - sy;
            ly = sx;
            break;
        default:
            lx = sx;
            ly = sy;
            break;
    }

    t->point.x = lx;
    t->point.y = ly;
    t->inside = 1;
}

/* SDL 事件过滤器：鼠标事件 -> 触摸状态 */
static bool SDLCALL touch_event_filter(void* userdata, SDL_Event* event)
{
    bsp_touch_t* t = (bsp_touch_t*)userdata;

    SDL_LockMutex(t->lock);
    switch (event->type) {
        case SDL_EVENT_MOUSE_MOTION:
            touch_update_point(t, event->motion.x, event->motion.y);
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event->button.button == SDL_BUTTON_LEFT) {
                t->button_down = 1;
                touch_update_point(t, event->button.x, event->button.y);
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event->button.button == SDL_BUTTON_LEFT) {
                t->button_down = 0;
                t->inside = 0;
            }
            break;

        default:
            break;
    }
    SDL_UnlockMutex(t->lock);

    return true; /* 继续传播事件 */
}

bsp_touch_handle_t bsp_touch_open(bsp_touch_desc_t* desc)
{
    if (!desc) {
        return NULL;
    }

    const bsp_touch_map_t* map = touch_map_find(desc->type, desc->idx);
    if (!map) {
        WARN_LOG("unsupported touch type=%u idx=%u", (unsigned)desc->type, (unsigned)desc->idx);
        return NULL;
    }

    if (!arch_display_renderer()) {
        return NULL;
    }

    bsp_touch_t* t = (bsp_touch_t*)MALLOC(sizeof(bsp_touch_t));
    if (!t) {
        return NULL;
    }
    memset(t, 0, sizeof(*t));
    t->map = map;

    t->lock = SDL_CreateMutex();
    if (!t->lock) {
        e_heap_free(t);
        return NULL;
    }

    if (!SDL_AddEventWatch(touch_event_filter, t)) {
        SDL_DestroyMutex(t->lock);
        e_heap_free(t);
        return NULL;
    }

    return (bsp_touch_handle_t)t;
}

void bsp_touch_close(bsp_touch_handle_t handle)
{
    bsp_touch_t* t = (bsp_touch_t*)handle;
    if (!t) {
        return;
    }
    SDL_RemoveEventWatch(touch_event_filter, t);
    SDL_DestroyMutex(t->lock);
    e_heap_free(t);
}

uint8_t bsp_touch_count(bsp_touch_handle_t handle)
{
    bsp_touch_t* t = (bsp_touch_t*)handle;
    if (!t) {
        return 0U;
    }

    uint8_t count;
    SDL_LockMutex(t->lock);
    count = (t->button_down && t->inside) ? 1U : 0U;
    SDL_UnlockMutex(t->lock);
    return count;
}

uint8_t bsp_touch_get_touch_point(bsp_touch_handle_t handle, uint8_t index, bsp_touch_point_t* point)
{
    bsp_touch_t* t = (bsp_touch_t*)handle;
    if (!t || !point || index != 0U) {
        return 1U;
    }

    uint8_t ret;
    SDL_LockMutex(t->lock);
    if (!t->button_down || !t->inside) {
        ret = 1U;
    } else {
        point->x = t->point.x;
        point->y = t->point.y;
        ret = 0U;
    }
    SDL_UnlockMutex(t->lock);
    return ret;
}

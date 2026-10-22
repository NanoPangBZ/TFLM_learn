/* bsp_touch_sdl.c — bsp_touch 的 SDL3 后端实现。
 *
 * 参考硬件的触摸屏（SPD2010 I2C 触摸）模型：SDK 下把鼠标左键映射为触摸点。
 *   - 通过 SDL_AddEventWatch 注册全局事件过滤器，捕获鼠标移动/按下/抬起；
 *   - 左键按下时视为 1 个触摸点（坐标 = 鼠标在窗口内的位置），抬起后无触摸点；
 *   - 事件过滤器运行在调用 SDL_PollEvent 的线程（arch_run 主线程），
 *     而 count / get_touch_point 可能从轮询线程调用，故用 SDL_Mutex 保护状态。 */
#include "bsp/bsp_touch.h"
#include "framework/e_heap.h"

#include <SDL3/SDL.h>
#include <string.h>

#define MALLOC(size)  e_heap_malloc((size), E_HEAP_ATTR_DEFAULT)

typedef struct bsp_touch_t {
    SDL_Mutex*        lock;
    uint8_t           pressed;   /* 左键是否按下 */
    bsp_touch_point_t point;     /* 当前触摸点坐标 */
} bsp_touch_t;

/* SDL 事件过滤器：鼠标事件 -> 触摸状态 */
static bool SDLCALL touch_event_filter(void* userdata, SDL_Event* event)
{
    bsp_touch_t* t = (bsp_touch_t*)userdata;

    SDL_LockMutex(t->lock);
    switch (event->type) {
        case SDL_EVENT_MOUSE_MOTION:
            t->point.x = (int)event->motion.x;
            t->point.y = (int)event->motion.y;
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event->button.button == SDL_BUTTON_LEFT) {
                t->pressed = 1;
                t->point.x = (int)event->button.x;
                t->point.y = (int)event->button.y;
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event->button.button == SDL_BUTTON_LEFT) {
                t->pressed = 0;
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
    (void)desc;

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        return NULL;
    }

    bsp_touch_t* t = (bsp_touch_t*)MALLOC(sizeof(bsp_touch_t));
    if (!t) {
        return NULL;
    }
    memset(t, 0, sizeof(*t));

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
    count = t->pressed ? 1U : 0U;
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
    if (!t->pressed) {
        ret = 1U;
    } else {
        point->x = t->point.x;
        point->y = t->point.y;
        ret = 0U;
    }
    SDL_UnlockMutex(t->lock);
    return ret;
}

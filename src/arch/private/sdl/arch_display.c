/* arch_display.c — arch 层主窗口 / 渲染器 / 帧循环（SDL3 后端）。
 *
 * 职责划分：
 *   - 本文件是"板卡"：创建唯一的主窗口与渲染器，负责事件泵、清屏、Present；
 *   - bsp_display 是"屏幕外设"：只在窗口里画自己的一块矩形（见 bsp_cfg.h）。
 *
 * 线程模型：
 *   - SDL 窗口/渲染器的创建、纹理上传、RenderPresent 全部在主线程（arch_run）；
 *   - 外设线程（如 canvas 轮询线程）只更新自己的内存缓冲，然后调用
 *     arch_display_request_frame() 请求重绘；请求通过原子标志 + SDL 用户事件
 *     通知主线程，避免在非主线程触碰渲染器。
 */
#include "arch/arch_display.h"
#include "arch/arch_input.h"
#include "arch/private/sdl/arch_display_internal.h"
#include "arch_cfg.h"

#include <SDL3/SDL.h>
#include <stdio.h>

#define ARCH_RENDER_CB_MAX   8

typedef struct arch_render_slot_t {
    arch_display_render_cb_t cb;
    void* ctx;
} arch_render_slot_t;

static SDL_Window*   s_window = NULL;
static SDL_Renderer* s_renderer = NULL;
static Uint32        s_frame_event_type = 0;
static SDL_AtomicInt s_frame_requested;

/* 输入状态：主线程更新，轮询任务读取 */
static SDL_AtomicInt s_mouse_x;
static SDL_AtomicInt s_mouse_y;
static SDL_AtomicInt s_mouse_left;
static SDL_AtomicInt s_wheel_delta;

static arch_render_slot_t s_render_slots[ARCH_RENDER_CB_MAX];
static uint32_t s_render_slot_count = 0;

static void arch_input_update_mouse(float window_x, float window_y)
{
    int px = -1;
    int py = -1;

    if (arch_display_window_to_pixel(window_x, window_y, &px, &py) != 0) {
        px = -1;
        py = -1;
    }
    SDL_SetAtomicInt(&s_mouse_x, px);
    SDL_SetAtomicInt(&s_mouse_y, py);
}

static void arch_render_frame(void)
{
    if (!s_renderer) {
        return;
    }

    SDL_SetRenderDrawColor(s_renderer,
                           (Uint8)((ARCH_WINDOW_BG_COLOR >> 16) & 0xFFu),
                           (Uint8)((ARCH_WINDOW_BG_COLOR >> 8) & 0xFFu),
                           (Uint8)(ARCH_WINDOW_BG_COLOR & 0xFFu),
                           0xFF);
    SDL_RenderClear(s_renderer);

    for (uint32_t i = 0; i < s_render_slot_count; i++) {
        if (s_render_slots[i].cb) {
            s_render_slots[i].cb(s_renderer, s_render_slots[i].ctx);
        }
    }

    SDL_RenderPresent(s_renderer);
}

int arch_display_init(void)
{
    Uint64 window_flags = 0;

#if ARCH_WINDOW_RESIZABLE
    window_flags |= SDL_WINDOW_RESIZABLE;
#endif

    s_window = SDL_CreateWindow(ARCH_WINDOW_TITLE,
                                ARCH_WINDOW_WIDTH, ARCH_WINDOW_HEIGHT,
                                window_flags);
    if (!s_window) {
        fprintf(stderr, "[arch] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return -1;
    }

    s_renderer = SDL_CreateRenderer(s_window, NULL);
    if (!s_renderer) {
        fprintf(stderr, "[arch] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(s_window);
        s_window = NULL;
        return -1;
    }

    /* 垂直同步：没有也不影响功能，失败可忽略 */
    SDL_SetRenderVSync(s_renderer, 1);

    /* 自定义"请求重绘"事件；注册失败时仍可靠 16ms 超时兜底 */
    s_frame_event_type = SDL_RegisterEvents(1);

    SDL_SetAtomicInt(&s_frame_requested, 1);
    SDL_SetAtomicInt(&s_mouse_x, -1);
    SDL_SetAtomicInt(&s_mouse_y, -1);
    SDL_SetAtomicInt(&s_mouse_left, 0);
    SDL_SetAtomicInt(&s_wheel_delta, 0);

    return 0;
}

int arch_display_pump(void)
{
    SDL_Event ev;
    int quit = 0;
    int redraw = SDL_GetAtomicInt(&s_frame_requested) ? 1 : 0;

    if (SDL_WaitEventTimeout(&ev, 16)) {
        do {
            switch (ev.type) {
            case SDL_EVENT_QUIT:
                quit = 1;
                break;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                /* 只认主窗口：app 自己的预览窗关闭不应结束进程 */
                if (ev.window.windowID == SDL_GetWindowID(s_window)) {
                    quit = 1;
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                arch_input_update_mouse(ev.motion.x, ev.motion.y);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                arch_input_update_mouse(ev.button.x, ev.button.y);
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    SDL_SetAtomicInt(&s_mouse_left, 1);
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                arch_input_update_mouse(ev.button.x, ev.button.y);
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    SDL_SetAtomicInt(&s_mouse_left, 0);
                }
                break;

            case SDL_EVENT_MOUSE_WHEEL: {
                Sint32 delta = ev.wheel.integer_y;
                arch_input_update_mouse(ev.wheel.mouse_x, ev.wheel.mouse_y);
                if (delta == 0) {
                    if (ev.wheel.y > 0.0f) {
                        delta = 1;
                    } else if (ev.wheel.y < 0.0f) {
                        delta = -1;
                    }
                }
                if (delta != 0) {
                    SDL_AddAtomicInt(&s_wheel_delta, (int)delta);
                }
                break;
            }

            case SDL_EVENT_WINDOW_EXPOSED:
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WINDOW_SHOWN:
                redraw = 1;
                break;

            default:
                if (s_frame_event_type && ev.type == s_frame_event_type) {
                    redraw = 1;
                }
                break;
            }
        } while (SDL_PollEvent(&ev));
    }

    if (SDL_GetAtomicInt(&s_frame_requested)) {
        redraw = 1;
    }

    if (redraw) {
        SDL_SetAtomicInt(&s_frame_requested, 0);
        arch_render_frame();
    }

    return quit;
}

struct SDL_Window* arch_display_window(void)
{
    return s_window;
}

struct SDL_Renderer* arch_display_renderer(void)
{
    return s_renderer;
}

int arch_display_window_to_pixel(float window_x, float window_y, int* pixel_x, int* pixel_y)
{
    float x = 0.0f;
    float y = 0.0f;

    if (!s_renderer) {
        return -1;
    }
    if (!SDL_RenderCoordinatesFromWindow(s_renderer, window_x, window_y, &x, &y)) {
        return -1;
    }

    if (pixel_x) {
        *pixel_x = (int)(x >= 0.0f ? x + 0.5f : x - 0.5f);
    }
    if (pixel_y) {
        *pixel_y = (int)(y >= 0.0f ? y + 0.5f : y - 0.5f);
    }
    return 0;
}

int arch_display_add_render_cb(arch_display_render_cb_t cb, void* ctx)
{
    if (!cb || s_render_slot_count >= ARCH_RENDER_CB_MAX) {
        return -1;
    }

    s_render_slots[s_render_slot_count].cb = cb;
    s_render_slots[s_render_slot_count].ctx = ctx;
    s_render_slot_count++;

    arch_display_request_frame();
    return 0;
}

void arch_display_remove_render_cb(arch_display_render_cb_t cb, void* ctx)
{
    for (uint32_t i = 0; i < s_render_slot_count; i++) {
        if (s_render_slots[i].cb == cb && s_render_slots[i].ctx == ctx) {
            for (uint32_t j = i + 1; j < s_render_slot_count; j++) {
                s_render_slots[j - 1] = s_render_slots[j];
            }
            s_render_slot_count--;
            return;
        }
    }
}

void arch_display_request_frame(void)
{
    SDL_SetAtomicInt(&s_frame_requested, 1);

    /* 唤醒阻塞在 SDL_WaitEventTimeout 的主线程；注册失败时靠 16ms 超时兜底 */
    if (s_frame_event_type) {
        SDL_Event ev;
        SDL_zero(ev);
        ev.type = s_frame_event_type;
        SDL_PushEvent(&ev);
    }
}

void arch_input_get(arch_input_state_t* state)
{
    if (!state) {
        return;
    }
    state->mouse_x = SDL_GetAtomicInt(&s_mouse_x);
    state->mouse_y = SDL_GetAtomicInt(&s_mouse_y);
    state->mouse_left = (uint8_t)(SDL_GetAtomicInt(&s_mouse_left) ? 1u : 0u);
}

int32_t arch_input_peek_wheel(void)
{
    return (int32_t)SDL_GetAtomicInt(&s_wheel_delta);
}

int32_t arch_input_take_wheel(void)
{
    return (int32_t)SDL_SetAtomicInt(&s_wheel_delta, 0);
}

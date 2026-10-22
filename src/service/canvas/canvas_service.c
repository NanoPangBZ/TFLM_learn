#include "service_cfg.h"
#include "service/canvas_service.h"

#if defined(CANVAS_SERVICE_ENABLE) && (CANVAS_SERVICE_ENABLE == 1)

#include "bsp/bsp_display.h"
#include "bsp/bsp_touch.h"
#include "framework/c_function_regedit.h"
#include "framework/e_heap.h"
#include "framework/e_polling_pool.h"
#include "framework/e_sched.h"

#include <stdlib.h>
#include <string.h>

#include "framework/e_log.h"
#define TAG "canvas_service"
#define DEBUG_LOG(...)  //E_DEBUG_LOG(TAG, __VA_ARGS__)
#define INFO_LOG(...)   E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

typedef struct canvas_service_ctx_t {
    bsp_display_handle_t display;
    bsp_touch_handle_t   touch;
    e_polling_task_handle_t polling_task;

    uint8_t* draw_buf;       /* display 当前绘制缓存（RGB565） */
    uint8_t* saved_buf;      /* 触发时保存的画布快照 */
    uint32_t buf_size;
    uint16_t width;
    uint16_t height;

    uint16_t pen_color565;
    uint8_t  pen_radius;

    int16_t  last_x;
    int16_t  last_y;
    uint32_t last_touch_ms;

    canvas_service_trigger_callback_t trigger_cb;
    void* trigger_ctx;

    uint8_t enable      : 1;
    uint8_t has_content : 1;  /* 本笔划会话是否画过内容 */
    uint8_t was_pressed : 1;
    uint8_t reserved    : 5;
} canvas_service_ctx_t;

static canvas_service_ctx_t g_canvas;

static uint16_t rgb888_to_rgb565(uint32_t color)
{
    uint32_t r = (color >> 16) & 0xFFu;
    uint32_t g = (color >> 8) & 0xFFu;
    uint32_t b = color & 0xFFu;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void canvas_put_pixel(canvas_service_ctx_t* ctx, int x, int y)
{
    if (x < 0 || y < 0 || x >= (int)ctx->width || y >= (int)ctx->height) {
        return;
    }
    uint16_t* buf = (uint16_t*)ctx->draw_buf;
    buf[(uint32_t)y * ctx->width + (uint32_t)x] = ctx->pen_color565;
}

/* 实心圆笔刷 */
static void canvas_stamp(canvas_service_ctx_t* ctx, int cx, int cy)
{
    int r = (int)ctx->pen_radius;
    int r2 = r * r;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r2) {
                canvas_put_pixel(ctx, cx + dx, cy + dy);
            }
        }
    }
}

/* 相邻采样点之间插值盖章，避免快速拖动断触 */
static void canvas_stroke(canvas_service_ctx_t* ctx, int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    int x = x0;
    int y = y0;

    for (;;) {
        canvas_stamp(ctx, x, y);
        if (x == x1 && y == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

static void canvas_flush_dirty(canvas_service_ctx_t* ctx, int x0, int y0, int x1, int y1)
{
    int r = (int)ctx->pen_radius + 1;
    int min_x = (x0 < x1 ? x0 : x1) - r;
    int min_y = (y0 < y1 ? y0 : y1) - r;
    int max_x = (x0 > x1 ? x0 : x1) + r;
    int max_y = (y0 > y1 ? y0 : y1) + r;

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= (int)ctx->width) max_x = (int)ctx->width - 1;
    if (max_y >= (int)ctx->height) max_y = (int)ctx->height - 1;
    if (min_x > max_x || min_y > max_y) {
        return;
    }

    bsp_display_flush(ctx->display,
                      (uint16_t)min_x, (uint16_t)min_y,
                      (uint16_t)(max_x - min_x + 1),
                      (uint16_t)(max_y - min_y + 1));
}

static void canvas_clear_screen(canvas_service_ctx_t* ctx)
{
    bsp_display_fill(ctx->display, CANVAS_SERVICE_BG_COLOR);
    ctx->has_content = 0;
    ctx->was_pressed = 0;
}

static void canvas_fire_trigger(canvas_service_ctx_t* ctx)
{
    if (!ctx->has_content || !ctx->draw_buf || !ctx->saved_buf) {
        return;
    }

    /* 保存当前画布到内部 ctx */
    memcpy(ctx->saved_buf, ctx->draw_buf, ctx->buf_size);

    canvas_service_trigger_t trigger = {
        .width = ctx->width,
        .height = ctx->height,
        .img_format = CANVAS_SERVICE_IMG_FORMAT_RGB565,
        .img_data = ctx->saved_buf,
    };

    INFO_LOG("idle %ums, trigger canvas %ux%u",
             (unsigned)CANVAS_SERVICE_IDLE_TIMEOUT_MS,
             (unsigned)ctx->width, (unsigned)ctx->height);

    if (ctx->trigger_cb) {
        ctx->trigger_cb(&trigger, ctx->trigger_ctx);
    }

    canvas_clear_screen(ctx);
}

static void canvas_service_polling_func(void* args)
{
    canvas_service_ctx_t* ctx = (canvas_service_ctx_t*)args;
    if (!ctx || !ctx->enable || !ctx->display || !ctx->touch) {
        return;
    }

    uint32_t now = e_sched_time_ms();
    bsp_touch_point_t point;
    uint8_t pressed = 0;

    if (bsp_touch_count(ctx->touch) > 0U &&
        bsp_touch_get_touch_point(ctx->touch, 0U, &point) == 0U) {
        pressed = 1;
    }

    if (pressed) {
        int x = point.x;
        int y = point.y;
        if (ctx->was_pressed) {
            canvas_stroke(ctx, ctx->last_x, ctx->last_y, x, y);
            canvas_flush_dirty(ctx, ctx->last_x, ctx->last_y, x, y);
        } else {
            canvas_stamp(ctx, x, y);
            canvas_flush_dirty(ctx, x, y, x, y);
        }
        ctx->last_x = (int16_t)x;
        ctx->last_y = (int16_t)y;
        ctx->last_touch_ms = now;
        ctx->was_pressed = 1;
        ctx->has_content = 1;
        return;
    }

    /* 抬起：记录抬起时刻，等待空闲超时 */
    if (ctx->was_pressed) {
        ctx->was_pressed = 0;
        ctx->last_touch_ms = now;
        return;
    }

    /* 连续 idle 超时且有内容：保存、trigger、清屏 */
    if (ctx->has_content &&
        (now - ctx->last_touch_ms) >= (uint32_t)CANVAS_SERVICE_IDLE_TIMEOUT_MS) {
        canvas_fire_trigger(ctx);
    }
}

void canvas_service_enable(uint8_t enable)
{
    g_canvas.enable = enable ? 1U : 0U;
    if (g_canvas.enable && g_canvas.display) {
        canvas_clear_screen(&g_canvas);
    }
    INFO_LOG("canvas service %s", g_canvas.enable ? "enabled" : "disabled");
}

uint8_t canvas_service_is_enabled(void)
{
    return g_canvas.enable ? 1U : 0U;
}

void canvas_service_register_trigger_callback(canvas_service_trigger_callback_t callback, void* ctx)
{
    g_canvas.trigger_cb = callback;
    g_canvas.trigger_ctx = ctx;
}

static int canvas_service_init(void)
{
    canvas_service_ctx_t* ctx = &g_canvas;
    memset(ctx, 0, sizeof(*ctx));

    bsp_display_desc_t display_desc = CANVAS_SERVICE_BSP_DISPLAY_DESC;
    bsp_touch_desc_t touch_desc = CANVAS_SERVICE_BSP_TOUCH_DESC;

    ctx->display = bsp_display_open(&display_desc);
    if (!ctx->display) {
        ERROR_LOG("open display failed");
        return -1;
    }

    ctx->touch = bsp_touch_open(&touch_desc);
    if (!ctx->touch) {
        ERROR_LOG("open touch failed");
        bsp_display_close(ctx->display);
        ctx->display = NULL;
        return -1;
    }

    if (bsp_display_get_size(ctx->display, &ctx->width, &ctx->height) != BSP_DISPLAY_STATUS_SUCCESS ||
        ctx->width == 0 || ctx->height == 0) {
        ERROR_LOG("get display size failed");
        goto fail;
    }

    ctx->draw_buf = bsp_display_get_draw_buffer(ctx->display, &ctx->buf_size);
    if (!ctx->draw_buf || ctx->buf_size < (uint32_t)ctx->width * (uint32_t)ctx->height * 2U) {
        ERROR_LOG("get draw buffer failed");
        goto fail;
    }

    ctx->saved_buf = (uint8_t*)e_heap_malloc(ctx->buf_size, E_HEAP_ATTR_BIG_BLOCKS);
    if (!ctx->saved_buf) {
        ERROR_LOG("malloc saved buffer failed (%u bytes)", (unsigned)ctx->buf_size);
        goto fail;
    }

    ctx->pen_color565 = rgb888_to_rgb565(CANVAS_SERVICE_PEN_COLOR);
    ctx->pen_radius = (uint8_t)CANVAS_SERVICE_PEN_RADIUS;
    ctx->enable = 1;

    bsp_display_on(ctx->display, 1);
    bsp_display_set_brightness(ctx->display, 100);
    canvas_clear_screen(ctx);

    e_polling_task_desc_t polling_desc = {
        .attr = E_POLLING_TASK_ATTR_DEFAULT,
        .freq_hz = CANVAS_SERVICE_TASK_FREQ_HZ,
        .name = "canvas_service",
        .polling_func = canvas_service_polling_func,
        .start_before_create = 1,
        .user_ctx = ctx,
    };
    ctx->polling_task = e_polling_task_create(&polling_desc);
    if (!ctx->polling_task) {
        ERROR_LOG("create polling task failed");
        goto fail;
    }

    INFO_LOG("init ok, %ux%u idle=%ums",
             (unsigned)ctx->width, (unsigned)ctx->height,
             (unsigned)CANVAS_SERVICE_IDLE_TIMEOUT_MS);
    return 0;

fail:
    if (ctx->touch) {
        bsp_touch_close(ctx->touch);
        ctx->touch = NULL;
    }
    if (ctx->display) {
        bsp_display_close(ctx->display);
        ctx->display = NULL;
    }
    if (ctx->saved_buf) {
        e_heap_free(ctx->saved_buf);
        ctx->saved_buf = NULL;
    }
    ctx->draw_buf = NULL;
    return -1;
}

static int canvas_service_deinit(void)
{
    canvas_service_ctx_t* ctx = &g_canvas;

    if (ctx->polling_task) {
        e_polling_task_destroy(ctx->polling_task);
        ctx->polling_task = NULL;
    }
    if (ctx->touch) {
        bsp_touch_close(ctx->touch);
        ctx->touch = NULL;
    }
    if (ctx->display) {
        bsp_display_close(ctx->display);
        ctx->display = NULL;
    }
    /* draw_buf 归 display 所有，随 close 释放 */
    ctx->draw_buf = NULL;
    if (ctx->saved_buf) {
        e_heap_free(ctx->saved_buf);
        ctx->saved_buf = NULL;
    }
    memset(ctx, 0, sizeof(*ctx));
    return 0;
}

REGEDIT_SERVICE_INIT_FUNCTION(canvas_service_init, 1);
REGEDIT_SERVICE_DEINIT_FUNCTION(canvas_service_deinit, 1);

#else /* !CANVAS_SERVICE_ENABLE */

void canvas_service_enable(uint8_t enable)
{
    (void)enable;
}

uint8_t canvas_service_is_enabled(void)
{
    return 0;
}

void canvas_service_register_trigger_callback(canvas_service_trigger_callback_t callback, void* ctx)
{
    (void)callback;
    (void)ctx;
}

#endif

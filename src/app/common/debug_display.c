/* debug_display.c — app 调试图像输出到 bsp_display 区域。
 *
 * 这些 display 实例由显示配置表定义（idx 1/2/3），
 * 不再是独立 SDL 窗口；绘制只写 RGB565 缓冲，刷新由 bsp_display 统一处理。
 */
#include "app/common/debug_display.h"

#include <string.h>

/* 0xRRGGBB -> RGB565 */
static uint16_t rgb888_to_rgb565(uint32_t color)
{
    uint32_t r = (color >> 16) & 0xFFu;
    uint32_t g = (color >> 8) & 0xFFu;
    uint32_t b = color & 0xFFu;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static int clip_rect(app_debug_display_t* d, int* x, int* y, int* w, int* h)
{
    if (!d || !d->handle) {
        return -1;
    }
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x >= (int)d->width || *y >= (int)d->height) {
        return -1;
    }
    if (*x + *w > (int)d->width) *w = (int)d->width - *x;
    if (*y + *h > (int)d->height) *h = (int)d->height - *y;
    if (*w <= 0 || *h <= 0) {
        return -1;
    }
    return 0;
}

int app_debug_display_open(app_debug_display_t* d, uint8_t idx)
{
    if (!d) {
        return -1;
    }
    memset(d, 0, sizeof(*d));

    bsp_display_desc_t desc = {
        .type = BSP_DISPLAY_TYPE_USER,
        .idx = idx,
        .attr = BSP_DISPLAY_ATTR_DEFAULT,
    };

    d->handle = bsp_display_open(&desc);
    if (!d->handle) {
        return -1;
    }

    if (bsp_display_get_size(d->handle, &d->width, &d->height) != BSP_DISPLAY_STATUS_SUCCESS) {
        bsp_display_close(d->handle);
        d->handle = NULL;
        return -1;
    }

    d->draw_buffer = bsp_display_get_draw_buffer(d->handle, &d->draw_buffer_size);
    if (!d->draw_buffer || d->draw_buffer_size < (uint32_t)d->width * d->height * 2U) {
        bsp_display_close(d->handle);
        d->handle = NULL;
        d->draw_buffer = NULL;
        return -1;
    }

    bsp_display_on(d->handle, 1);
    app_debug_display_clear(d, 0x000000);
    return 0;
}

void app_debug_display_close(app_debug_display_t* d)
{
    if (!d) {
        return;
    }
    if (d->handle) {
        bsp_display_close(d->handle);
    }
    memset(d, 0, sizeof(*d));
}

void app_debug_display_clear(app_debug_display_t* d, uint32_t color)
{
    if (!d || !d->handle) {
        return;
    }
    bsp_display_fill(d->handle, color);
}

void app_debug_display_show_gray(app_debug_display_t* d, const uint8_t* gray,
                                 int src_w, int src_h, int scale, int center)
{
    if (!d || !d->handle || !d->draw_buffer || !gray || src_w <= 0 || src_h <= 0 || scale <= 0) {
        return;
    }

    int dw = src_w * scale;
    int dh = src_h * scale;
    int ox = 0;
    int oy = 0;

    if (center) {
        ox = ((int)d->width - dw) / 2;
        oy = ((int)d->height - dh) / 2;
        if (ox < 0) ox = 0;
        if (oy < 0) oy = 0;
    }
    if (ox >= (int)d->width || oy >= (int)d->height) {
        return;
    }
    if (ox + dw > (int)d->width) dw = (int)d->width - ox;
    if (oy + dh > (int)d->height) dh = (int)d->height - oy;

    uint16_t* buf = (uint16_t*)d->draw_buffer;
    for (int y = 0; y < dh; y++) {
        const uint8_t* src = gray + (size_t)(y / scale) * (size_t)src_w;
        uint16_t* dst = buf + (size_t)(oy + y) * d->width + ox;
        for (int x = 0; x < dw; x++) {
            uint32_t v = src[x / scale];
            dst[x] = (uint16_t)(((v >> 3) << 11) | ((v >> 2) << 5) | (v >> 3));
        }
    }
}

void app_debug_display_draw_rect(app_debug_display_t* d, int x, int y, int w, int h, uint32_t color)
{
    if (!d || !d->handle || !d->draw_buffer || w <= 0 || h <= 0) {
        return;
    }
    if (clip_rect(d, &x, &y, &w, &h) != 0) {
        return;
    }

    uint16_t c = rgb888_to_rgb565(color);
    uint16_t* buf = (uint16_t*)d->draw_buffer;
    int thickness = 2;

    for (int t = 0; t < thickness; t++) {
        int yy = y + t;
        if (yy < y + h && yy < (int)d->height) {
            for (int xx = x; xx < x + w; xx++) {
                buf[(size_t)yy * d->width + xx] = c;
            }
        }
        int yb = y + h - 1 - t;
        if (yb >= y && yb < (int)d->height) {
            for (int xx = x; xx < x + w; xx++) {
                buf[(size_t)yb * d->width + xx] = c;
            }
        }
    }

    for (int t = 0; t < thickness; t++) {
        int xx = x + t;
        if (xx < x + w && xx < (int)d->width) {
            for (int yy = y; yy < y + h; yy++) {
                buf[(size_t)yy * d->width + xx] = c;
            }
        }
        int xr = x + w - 1 - t;
        if (xr >= x && xr < (int)d->width) {
            for (int yy = y; yy < y + h; yy++) {
                buf[(size_t)yy * d->width + xr] = c;
            }
        }
    }
}

void app_debug_display_flush(app_debug_display_t* d, int x, int y, int w, int h)
{
    if (!d || !d->handle) {
        return;
    }
    if (clip_rect(d, &x, &y, &w, &h) != 0) {
        return;
    }
    bsp_display_flush(d->handle, (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h);
}

void app_debug_display_flush_all(app_debug_display_t* d)
{
    if (!d || !d->handle) {
        return;
    }
    bsp_display_flush(d->handle, 0, 0, d->width, d->height);
}

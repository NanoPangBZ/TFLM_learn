/* num_discern_uint8 — 纯 uint8 量化版手写数字识别 app。
 *
 * 与 float 版 num_discern 的区别只在模型和输入类型：
 *   - 预处理仍然输出 0..255 的墨迹灰度；
 *   - 不再做 /255，直接把 0..255 作为量化输入（S=1/255, Z=0）；
 *   - 推理走 num_discern_uint8_predict()，内部是 int32 累加 + uint8 再量化。
 *
 * 预处理代码暂时从 float 版复制，等实验稳定后再考虑抽公共模块。
 */
#include <SDL3/SDL.h>
#include "framework/c_function_regedit.h"
#include "framework/e_log.h"
#include "service/canvas_service.h"
#include "bsp_cfg.h"
#include "num_discern_uint8_model.h"

#include <stdint.h>
#include <stdlib.h>

#define TAG "num_discern_uint8"
#define INFO_LOG(...)   E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

#define ND_GRID_SIZE     28   /* MNIST 输入尺寸 */
#define ND_DIGIT_SIZE    20   /* 数字在 28x28 内占 20x20 */
#define ND_INK_THRESHOLD 16   /* 墨迹阈值（去噪） */
#define ND_PREVIEW_SCALE 8    /* 预览窗口放大倍数 */

/* 静态缓冲：e_heap 是 bump 分配器、free 为空操作，热路径不要反复 malloc */
static uint8_t s_ink[BSP_DISPLAY_WIDTH * BSP_DISPLAY_HEIGHT];
static uint8_t s_gray[Q8_INPUT_SIZE];

typedef struct nd_preview_t {
    SDL_Window*   win;
    SDL_Renderer* ren;
    SDL_Texture*  tex;
    uint32_t*     buf;
} nd_preview_t;

static nd_preview_t g_pv_ink;
static nd_preview_t g_pv_digit;
static nd_preview_t g_pv_input;

static int preview_ensure(nd_preview_t* p, const char* title, int w, int h)
{
    if (p->win) {
        return 0;
    }
    p->win = SDL_CreateWindow(title, w, h, 0);
    p->ren = p->win ? SDL_CreateRenderer(p->win, NULL) : NULL;
    p->tex = p->ren ? SDL_CreateTexture(p->ren, SDL_PIXELFORMAT_RGBX8888,
                                        SDL_TEXTUREACCESS_STREAMING, w, h) : NULL;
    p->buf = p->tex ? (uint32_t*)malloc((size_t)w * h * 4) : NULL;
    return (p->win && p->ren && p->tex && p->buf) ? 0 : -1;
}

static void preview_render(nd_preview_t* p, const char* title,
                           const uint8_t* gray, int w, int h, int scale)
{
    int dw = w * scale, dh = h * scale;
    if (preview_ensure(p, title, dw, dh) != 0) {
        return;
    }
    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            uint8_t v = gray[(y / scale) * w + (x / scale)];
            p->buf[y * dw + x] = 0xFF000000u | ((uint32_t)v << 16) | ((uint32_t)v << 8) | v;
        }
    }
    SDL_UpdateTexture(p->tex, NULL, p->buf, dw * 4);
    SDL_RenderClear(p->ren);
    SDL_RenderTexture(p->ren, p->tex, NULL, NULL);
}

static void preview_present(nd_preview_t* p)
{
    if (p->ren) {
        SDL_RenderPresent(p->ren);
    }
}

static void float_to_gray(const float* src, uint8_t* dst, int n, float scale255)
{
    for (int i = 0; i < n; i++) {
        float v = src[i] * scale255;
        if (v < 0.0f) v = 0.0f;
        if (v > 255.0f) v = 255.0f;
        dst[i] = (uint8_t)(v + 0.5f);
    }
}

/* RGB565 -> 亮度 -> 反相墨迹（黑笔迹->高值，白底->0） */
static uint8_t rgb565_to_ink(uint16_t c)
{
    uint32_t r = (c >> 11) & 0x1Fu;
    uint32_t g = (c >> 5) & 0x3Fu;
    uint32_t b = c & 0x1Fu;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    uint32_t lum = (r * 299 + g * 587 + b * 114) / 1000;
    return (uint8_t)(255 - lum);
}

static void num_discern_uint8_trigger_cb(const canvas_service_trigger_t* trigger, void* ctx)
{
    (void)ctx;
    if (!trigger || !trigger->img_data ||
        trigger->img_format != CANVAS_SERVICE_IMG_FORMAT_RGB565 ||
        trigger->width == 0 || trigger->height == 0) {
        WARN_LOG("invalid trigger payload");
        return;
    }

    uint32_t w = trigger->width;
    uint32_t h = trigger->height;
    uint32_t pixels = w * h;
    if (pixels > (uint32_t)(BSP_DISPLAY_WIDTH * BSP_DISPLAY_HEIGHT)) {
        ERROR_LOG("canvas too large: %u pixels", (unsigned)pixels);
        return;
    }

    /* 1. RGB565 -> 墨迹，并求包围盒 */
    const uint16_t* src = (const uint16_t*)trigger->img_data;
    int min_x = (int)w, min_y = (int)h, max_x = -1, max_y = -1;
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint8_t v = rgb565_to_ink(src[y * w + x]);
            s_ink[y * w + x] = v;
            if (v > ND_INK_THRESHOLD) {
                if ((int)x < min_x) min_x = (int)x;
                if ((int)x > max_x) max_x = (int)x;
                if ((int)y < min_y) min_y = (int)y;
                if ((int)y > max_y) max_y = (int)y;
            }
        }
    }

    if (max_x < 0) {
        WARN_LOG("no ink detected, skip");
        return;
    }

    int crop_w = max_x - min_x + 1;
    int crop_h = max_y - min_y + 1;

    /* 预览 1：墨迹图 + 包围盒（红框） */
    preview_render(&g_pv_ink, "1. ink + bbox (u8)", s_ink, (int)w, (int)h, 1);
    if (g_pv_ink.ren) {
        SDL_SetRenderDrawColor(g_pv_ink.ren, 255, 0, 0, 255);
        SDL_FRect r = { (float)min_x, (float)min_y, (float)crop_w, (float)crop_h };
        SDL_RenderRect(g_pv_ink.ren, &r);
    }
    preview_present(&g_pv_ink);

    /* 2. 包围盒 -> 20x20（等比缩放保留宽高比，区域平均降采样） */
    float scale = (crop_w > crop_h) ? (20.0f / (float)crop_w) : (20.0f / (float)crop_h);
    int sw = (int)(crop_w * scale + 0.5f);
    int sh = (int)(crop_h * scale + 0.5f);
    if (sw < 1) sw = 1;
    if (sh < 1) sh = 1;
    int pad_x = (ND_DIGIT_SIZE - sw) / 2;
    int pad_y = (ND_DIGIT_SIZE - sh) / 2;

    float digit[ND_DIGIT_SIZE * ND_DIGIT_SIZE];
    for (int oy = 0; oy < ND_DIGIT_SIZE; oy++) {
        for (int ox = 0; ox < ND_DIGIT_SIZE; ox++) {
            int sx = ox - pad_x, sy = oy - pad_y;
            if (sx < 0 || sx >= sw || sy < 0 || sy >= sh) {
                digit[oy * ND_DIGIT_SIZE + ox] = 0.0f;
                continue;
            }
            int x0 = min_x + sx * crop_w / sw;
            int x1 = min_x + (sx + 1) * crop_w / sw;
            int y0 = min_y + sy * crop_h / sh;
            int y1 = min_y + (sy + 1) * crop_h / sh;
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            float sum = 0.0f;
            int cnt = 0;
            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    sum += (float)s_ink[(uint32_t)y * w + (uint32_t)x];
                    cnt++;
                }
            }
            digit[oy * ND_DIGIT_SIZE + ox] = sum / (float)cnt;
        }
    }

    /* 预览 2：20x20 降采样结果 */
    float_to_gray(digit, s_gray, ND_DIGIT_SIZE * ND_DIGIT_SIZE, 1.0f);
    preview_render(&g_pv_digit, "2. 20x20 digit (u8)", s_gray, ND_DIGIT_SIZE, ND_DIGIT_SIZE, ND_PREVIEW_SCALE);
    preview_present(&g_pv_digit);

    /* 3. 质心居中到 28x28，直接写 uint8（不再 /255） */
    float m00 = 0.0f, mx = 0.0f, my = 0.0f;
    for (int y = 0; y < ND_DIGIT_SIZE; y++) {
        for (int x = 0; x < ND_DIGIT_SIZE; x++) {
            float v = digit[y * ND_DIGIT_SIZE + x];
            m00 += v;
            mx += (float)x * v;
            my += (float)y * v;
        }
    }
    float cx = (m00 > 0.0f) ? (mx / m00) : (ND_DIGIT_SIZE / 2.0f);
    float cy = (m00 > 0.0f) ? (my / m00) : (ND_DIGIT_SIZE / 2.0f);
    int shift_x = (int)(ND_GRID_SIZE / 2.0f - cx + 0.5f);
    int shift_y = (int)(ND_GRID_SIZE / 2.0f - cy + 0.5f);

    uint8_t input_q[Q8_INPUT_SIZE];
    for (int y = 0; y < ND_GRID_SIZE; y++) {
        for (int x = 0; x < ND_GRID_SIZE; x++) {
            int sx = x - shift_x, sy = y - shift_y;
            float v = 0.0f;
            if (sx >= 0 && sx < ND_DIGIT_SIZE && sy >= 0 && sy < ND_DIGIT_SIZE) {
                v = digit[sy * ND_DIGIT_SIZE + sx];
            }
            int iv = (int)(v + 0.5f);
            if (iv < 0) iv = 0;
            if (iv > 255) iv = 255;
            input_q[y * ND_GRID_SIZE + x] = (uint8_t)iv;
        }
    }

    /* 预览 3：模型真正看到的 uint8 输入 */
    preview_render(&g_pv_input, "3. 28x28 input (u8)", input_q, ND_GRID_SIZE, ND_GRID_SIZE, ND_PREVIEW_SCALE);
    preview_present(&g_pv_input);

    /* 4. 纯 uint8 推理 */
    float probs[Q8_OUTPUT_SIZE];
    int result = num_discern_uint8_predict(input_q, probs);
    INFO_LOG("recognized digit: %d", result);
    for (int k = 0; k < Q8_OUTPUT_SIZE; k++) {
        INFO_LOG("[ %d : %.1f%% ]", k, probs[k] * 100.0f);
    }
}

static int num_discern_uint8_init(void)
{
    canvas_service_register_trigger_callback(num_discern_uint8_trigger_cb, NULL);
    INFO_LOG("model weights: %u bytes, input: %u bytes",
             (unsigned)num_discern_uint8_model_size_bytes(),
             (unsigned)Q8_INPUT_SIZE);
    INFO_LOG("ready (pure uint8), waiting for canvas trigger");
    return 0;
}

REGEDIT_APP_INIT_FUNCTION(num_discern_uint8_init, 0);

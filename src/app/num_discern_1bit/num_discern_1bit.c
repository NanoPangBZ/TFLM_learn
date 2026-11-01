/* num_discern_1bit — 20x20 1-bit 输入的手写数字识别 app。
 *
 * 与 num_discern_uint8 的区别：
 *   - 预处理最终生成 20x20 灰度；
 *   - 阈值 >128 记为 1，否则 0；
 *   - 按 row-major、MSB 在前打包成 50 字节（400 bit）；
 *   - 模型输入就是这 50 字节，推理前解包成 400 个 0/1。
 *
 * 预处理代码暂时从 uint8 版复制，等实验稳定后再考虑抽公共模块。
 */
#include "framework/c_function_regedit.h"
#include "framework/e_heap.h"
#include "framework/e_log.h"
#include "service/canvas_service.h"
#include "app/common/debug_display.h"
#include "num_discern_1bit_model.h"

#include <stdint.h>

#define TAG "num_discern_1bit"
#define INFO_LOG(...)   E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

#define ND_GRID_SIZE     20   /* 模型输入 20x20 */
#define ND_DIGIT_SIZE    20   /* 数字占满 20x20，无额外边距 */
#define ND_INK_THRESHOLD 16   /* 求包围盒时的去噪阈值 */
#define ND_BIT_THRESHOLD 128  /* 二值化：>128 -> 1 */
#define ND_PREVIEW_SCALE 8

/* 墨迹缓冲：首次 trigger 时按画布尺寸从堆里申请，之后一直复用、不释放 */
static uint8_t* s_ink = NULL;
static uint32_t s_ink_size = 0;
static uint8_t s_gray[ND1_INPUT_BITS];

/* 中间图像输出到 display 1/2/3，不再是独立窗口 */
static app_debug_display_t g_pv_ink;
static app_debug_display_t g_pv_digit;
static app_debug_display_t g_pv_input;

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

static void num_discern_1bit_trigger_cb(const canvas_service_trigger_t* trigger, void* ctx)
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

    /* 首次 trigger 时按画布尺寸从堆里申请，之后复用、不释放 */
    if (!s_ink) {
        s_ink = (uint8_t*)e_heap_malloc(pixels, E_HEAP_ATTR_DEFAULT);
        if (!s_ink) {
            ERROR_LOG("malloc ink buffer failed (%u bytes)", (unsigned)pixels);
            return;
        }
        s_ink_size = pixels;
        INFO_LOG("ink buffer allocated: %u bytes", (unsigned)pixels);
    } else if (pixels > s_ink_size) {
        ERROR_LOG("canvas grew: %u > %u, skip", (unsigned)pixels, (unsigned)s_ink_size);
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

    /* 预览 1：墨迹图 + 包围盒（写入 display 1） */
    if (g_pv_ink.handle) {
        app_debug_display_show_gray(&g_pv_ink, s_ink, (int)w, (int)h, 1, 0);
        app_debug_display_draw_rect(&g_pv_ink, min_x, min_y, crop_w, crop_h, 0xFF0000);
        app_debug_display_flush_all(&g_pv_ink);
    }

    /* 2. 包围盒 -> 20x20（等比缩放，区域平均） */
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

    /* 预览 2：20x20 灰度（写入 display 2） */
    float_to_gray(digit, s_gray, ND_DIGIT_SIZE * ND_DIGIT_SIZE, 1.0f);
    if (g_pv_digit.handle) {
        app_debug_display_show_gray(&g_pv_digit, s_gray, ND_DIGIT_SIZE, ND_DIGIT_SIZE, ND_PREVIEW_SCALE, 1);
        app_debug_display_flush_all(&g_pv_digit);
    }

    /* 3. 20x20（数字已占满），>128 -> 1，打包 50 字节（MSB 在前） */
    int shift_x = 0, shift_y = 0;
    if (ND_GRID_SIZE > ND_DIGIT_SIZE) {
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
        shift_x = (int)(ND_GRID_SIZE / 2.0f - cx + 0.5f);
        shift_y = (int)(ND_GRID_SIZE / 2.0f - cy + 0.5f);
    }

    uint8_t packed[ND1_INPUT_BYTES] = {0};
    uint8_t preview[ND1_INPUT_BITS];
    for (int y = 0; y < ND_GRID_SIZE; y++) {
        for (int x = 0; x < ND_GRID_SIZE; x++) {
            int sx = x - shift_x, sy = y - shift_y;
            float v = 0.0f;
            if (sx >= 0 && sx < ND_DIGIT_SIZE && sy >= 0 && sy < ND_DIGIT_SIZE) {
                v = digit[sy * ND_DIGIT_SIZE + sx];
            }
            int iv = (int)(v + 0.5f);
            int bit = (iv > ND_BIT_THRESHOLD) ? 1 : 0;
            int idx = y * ND_GRID_SIZE + x;
            if (bit) {
                packed[idx >> 3] |= (uint8_t)(0x80u >> (idx & 7));
            }
            preview[idx] = bit ? 255 : 0;
        }
    }

    /* 预览 3：模型真正看到的 1-bit 输入（写入 display 3） */
    if (g_pv_input.handle) {
        app_debug_display_show_gray(&g_pv_input, preview, ND_GRID_SIZE, ND_GRID_SIZE, ND_PREVIEW_SCALE, 1);
        app_debug_display_flush_all(&g_pv_input);
    }

    /* 4. 纯 uint8 量化推理，输入是 50 字节 packed */
    float probs[ND1_OUTPUT_SIZE];
    int result = num_discern_1bit_predict(packed, probs);
    INFO_LOG("recognized digit: %d", result);
    for (int k = 0; k < ND1_OUTPUT_SIZE; k++) {
        INFO_LOG("[ %d : %.1f%% ]", k, probs[k] * 100.0f);
    }
}

static int num_discern_1bit_init(void)
{
    if (app_debug_display_open(&g_pv_ink, 1) != 0) {
        WARN_LOG("preview display 1 open failed");
    }
    if (app_debug_display_open(&g_pv_digit, 2) != 0) {
        WARN_LOG("preview display 2 open failed");
    }
    if (app_debug_display_open(&g_pv_input, 3) != 0) {
        WARN_LOG("preview display 3 open failed");
    }

    canvas_service_register_trigger_callback(num_discern_1bit_trigger_cb, NULL);
    INFO_LOG("model weights: %u bytes, input: %u bytes",
             (unsigned)num_discern_1bit_model_size_bytes(),
             (unsigned)ND1_INPUT_BYTES);
    INFO_LOG("ready (20x20 1-bit), waiting for canvas trigger");
    return 0;
}

static int num_discern_1bit_deinit(void)
{
    canvas_service_unregister_trigger_callback(num_discern_1bit_trigger_cb);
    app_debug_display_close(&g_pv_ink);
    app_debug_display_close(&g_pv_digit);
    app_debug_display_close(&g_pv_input);
    return 0;
}

REGEDIT_APP_INIT_FUNCTION(num_discern_1bit_init, 0);
REGEDIT_APP_DEINIT_FUNCTION(num_discern_1bit_deinit, 0);

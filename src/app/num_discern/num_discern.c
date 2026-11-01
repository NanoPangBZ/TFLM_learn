/* num_discern — 手写数字识别 app。
 * 依赖 canvas_service：绘制 -> 空闲触发 -> 回调收到 RGB565 画布。
 * 预处理成 28x28 灰度，跑 MLP 推理；并用 SDL 多窗口可视化预处理各阶段。 */
#include "framework/c_function_regedit.h"
#include "framework/e_log.h"
#include "service/canvas_service.h"
#include "app/common/debug_display.h"
#include "bsp_cfg.h"
#include "num_discern_model.h"

#include <stdint.h>

#define TAG "num_discern"
#define INFO_LOG(...)   E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

#define ND_GRID_SIZE   28   /* MNIST 输入尺寸 */
#define ND_DIGIT_SIZE  20   /* 数字在 28x28 内占 20x20 */
#define ND_INK_THRESHOLD 16 /* 墨迹阈值（去噪） */
#define ND_PREVIEW_SCALE 8  /* 预览放大倍数 */

/* 墨迹临时缓冲（静态）：e_heap 是 bump 分配器、free 为空操作，
 * 热路径不能反复 e_heap_malloc（会持续吃堆），改用固定静态缓冲。 */
static uint8_t s_ink[BSP_DISPLAY_WIDTH * BSP_DISPLAY_HEIGHT];
static uint8_t s_gray[ND_INPUT_SIZE];   /* 显示用灰度缓冲（够 20x20 与 28x28） */

/* 中间图像输出到 bsp_cfg.h 里配置的 display 1/2/3，不再是独立窗口 */
static app_debug_display_t g_pv_ink;
static app_debug_display_t g_pv_digit;
static app_debug_display_t g_pv_input;

/* float 灰度 -> uint8（乘以 scale255 后裁剪到 0..255） */
static void float_to_gray(const float* src, uint8_t* dst, int n, float scale255)
{
    for (int i = 0; i < n; i++) {
        float v = src[i] * scale255;
        if (v < 0.0f) v = 0.0f;
        if (v > 255.0f) v = 255.0f;
        dst[i] = (uint8_t)v;
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

static void num_discern_trigger_cb(const canvas_service_trigger_t* trigger, void* ctx)
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
    uint8_t* ink = s_ink;

    /* 1. RGB565 -> 墨迹，并求包围盒 */
    const uint16_t* src = (const uint16_t*)trigger->img_data;
    int min_x = (int)w, min_y = (int)h, max_x = -1, max_y = -1;
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint8_t v = rgb565_to_ink(src[y * w + x]);
            ink[y * w + x] = v;
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

    /* 预览 1：墨迹图 + 包围盒（红框，写入 display 1） */
    if (g_pv_ink.handle) {
        app_debug_display_show_gray(&g_pv_ink, ink, (int)w, (int)h, 1, 0);
        app_debug_display_draw_rect(&g_pv_ink, min_x, min_y, crop_w, crop_h, 0xFF0000);
        app_debug_display_flush_all(&g_pv_ink);
    }

    /* 2. 包围盒 -> 20x20（等比缩放保留宽高比，区域平均降采样） */
    float scale = (crop_w > crop_h) ? (20.0f / (float)crop_w) : (20.0f / (float)crop_h);
    int sw = (int)(crop_w * scale + 0.5f);   /* 缩放后宽度 */
    int sh = (int)(crop_h * scale + 0.5f);   /* 缩放后高度 */
    if (sw < 1) sw = 1;
    if (sh < 1) sh = 1;
    int pad_x = (ND_DIGIT_SIZE - sw) / 2;    /* 20x20 内的水平偏移 */
    int pad_y = (ND_DIGIT_SIZE - sh) / 2;    /* 20x20 内的垂直偏移 */

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
                    sum += (float)ink[(uint32_t)y * w + (uint32_t)x];
                    cnt++;
                }
            }
            digit[oy * ND_DIGIT_SIZE + ox] = sum / (float)cnt;
        }
    }

    /* 预览 2：20x20 降采样结果（写入 display 2） */
    float_to_gray(digit, s_gray, ND_DIGIT_SIZE * ND_DIGIT_SIZE, 1.0f);
    if (g_pv_digit.handle) {
        app_debug_display_show_gray(&g_pv_digit, s_gray, ND_DIGIT_SIZE, ND_DIGIT_SIZE, ND_PREVIEW_SCALE, 1);
        app_debug_display_flush_all(&g_pv_digit);
    }

    /* 3. 计算 20x20 数字的质心，居中到 28x28，归一化到 [0,1] */
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

    float input[ND_INPUT_SIZE];
    for (int y = 0; y < ND_GRID_SIZE; y++) {
        for (int x = 0; x < ND_GRID_SIZE; x++) {
            int sx = x - shift_x, sy = y - shift_y;
            float v = 0.0f;
            if (sx >= 0 && sx < ND_DIGIT_SIZE && sy >= 0 && sy < ND_DIGIT_SIZE) {
                v = digit[sy * ND_DIGIT_SIZE + sx];
            }
            input[y * ND_GRID_SIZE + x] = v / 255.0f;
        }
    }

    /* 预览 3：28x28 模型输入（模型真正看到的东西，写入 display 3） */
    float_to_gray(input, s_gray, ND_INPUT_SIZE, 255.0f);
    if (g_pv_input.handle) {
        app_debug_display_show_gray(&g_pv_input, s_gray, ND_GRID_SIZE, ND_GRID_SIZE, ND_PREVIEW_SCALE, 1);
        app_debug_display_flush_all(&g_pv_input);
    }

    /* 4. 推理并输出 */
    float probs[ND_OUTPUT_SIZE];
    int result = num_discern_predict(input, probs);
    INFO_LOG("recognized digit: %d", result);
    for (int k = 0; k < ND_OUTPUT_SIZE; k++) {
        INFO_LOG("[ %d : %.1f%% ]", k, probs[k] * 100.0f);
    }
}

static int num_discern_init(void)
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

    canvas_service_register_trigger_callback(num_discern_trigger_cb, NULL);
    INFO_LOG("model weights: %u bytes, input: %u bytes",
             (unsigned)num_discern_model_size_bytes(),
             (unsigned)(ND_INPUT_SIZE * sizeof(float)));
    INFO_LOG("ready, waiting for canvas trigger");
    return 0;
}

static int num_discern_deinit(void)
{
    canvas_service_unregister_trigger_callback(num_discern_trigger_cb);
    app_debug_display_close(&g_pv_ink);
    app_debug_display_close(&g_pv_digit);
    app_debug_display_close(&g_pv_input);
    return 0;
}

REGEDIT_APP_INIT_FUNCTION(num_discern_init, 0);
REGEDIT_APP_DEINIT_FUNCTION(num_discern_deinit, 0);

/* num_discern — 手写数字识别 app。
 * 依赖 canvas_service：绘制 -> 空闲触发 -> 回调收到 RGB565 画布，
 * 本 app 预处理成 28x28 灰度，跑 MLP 推理，日志输出识别结果。 */
#include "framework/c_function_regedit.h"
#include "framework/e_log.h"
#include "service/canvas_service.h"
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

/* 墨迹临时缓冲（静态）：e_heap 是 bump 分配器、free 为空操作，
 * 热路径不能反复 e_heap_malloc（会持续吃堆），改用固定静态缓冲。 */
static uint8_t s_ink[BSP_DISPLAY_WIDTH * BSP_DISPLAY_HEIGHT];

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

    /* 2. 包围盒 -> 20x20（区域平均降采样） */
    float digit[ND_DIGIT_SIZE * ND_DIGIT_SIZE];
    for (int oy = 0; oy < ND_DIGIT_SIZE; oy++) {
        for (int ox = 0; ox < ND_DIGIT_SIZE; ox++) {
            int x0 = min_x + ox * crop_w / ND_DIGIT_SIZE;
            int x1 = min_x + (ox + 1) * crop_w / ND_DIGIT_SIZE;
            int y0 = min_y + oy * crop_h / ND_DIGIT_SIZE;
            int y1 = min_y + (oy + 1) * crop_h / ND_DIGIT_SIZE;
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

    /* 3. 居中到 28x28，归一化到 [0,1] */
    int off = (ND_GRID_SIZE - ND_DIGIT_SIZE) / 2;
    float input[ND_INPUT_SIZE];
    for (int y = 0; y < ND_GRID_SIZE; y++) {
        for (int x = 0; x < ND_GRID_SIZE; x++) {
            int sx = x - off, sy = y - off;
            float v = 0.0f;
            if (sx >= 0 && sx < ND_DIGIT_SIZE && sy >= 0 && sy < ND_DIGIT_SIZE) {
                v = digit[sy * ND_DIGIT_SIZE + sx];
            }
            input[y * ND_GRID_SIZE + x] = v / 255.0f;
        }
    }

    /* 4. 推理并输出 */
    int result = num_discern_predict(input);
    INFO_LOG("recognized digit: %d", result);
}

static int num_discern_init(void)
{
    canvas_service_register_trigger_callback(num_discern_trigger_cb, NULL);
    INFO_LOG("ready, waiting for canvas trigger");
    return 0;
}

REGEDIT_APP_INIT_FUNCTION(num_discern_init, 0);

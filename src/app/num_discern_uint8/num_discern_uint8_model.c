/* num_discern_uint8_model.c — 纯 uint8 仿射量化 MLP 前向。
 *
 * 量化约定（与 tools/quantize_mnist_mlp.py 完全一致）：
 *   x ~= S * (q - Z)
 *   输入  : q = 像素 0..255, S=1/255, Z=0
 *   权重  : uint8, per-tensor S/Z
 *   隐藏层: ReLU 后 Z=0, S=max/255
 *   输出  : uint8 logits, per-tensor S/Z
 *
 * 累加：
 *   acc = Σ (qx - Zx)(qw - Zw) + qb
 * 本方案 Zx=0，因此：
 *   acc = Σ qx*qw + qb - Zw*Σqx
 *
 * 再量化（先用 float 乘子，保证与 Python 模拟一致）：
 *   hidden_q = clamp(qround(acc * M1), 0, 255)
 *   out_q    = clamp(qround(acc2 * M2) + Zo, 0, 255)
 */
#include "num_discern_uint8_model.h"
#include "num_discern_uint8_weights.h"

#include <math.h>

uint32_t num_discern_uint8_model_size_bytes(void)
{
    return (uint32_t)(sizeof(q8_w1) + sizeof(q8_b1) + sizeof(q8_w2) + sizeof(q8_b2));
}

static uint8_t s_hidden[Q8_HIDDEN_SIZE];
static uint8_t s_output[Q8_OUTPUT_SIZE];

/* 四舍五入、远离 0；与 Python 端 qround 保持一致 */
static inline int32_t qround_d(double x)
{
    return (int32_t)(x >= 0.0 ? x + 0.5 : x - 0.5);
}

static inline uint8_t clamp_u8(int32_t v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

void num_discern_uint8_infer(const uint8_t* input_q, uint8_t* hidden_q, uint8_t* out_q)
{
    if (!hidden_q) hidden_q = s_hidden;
    if (!out_q) out_q = s_output;

    /* 第一层：784 -> 32，ReLU + 再量化 */
    int32_t sum_x = 0;
    for (int j = 0; j < Q8_INPUT_SIZE; j++) {
        sum_x += (int32_t)input_q[j];
    }

    for (int i = 0; i < Q8_HIDDEN_SIZE; i++) {
        int32_t acc = q8_b1[i];
        for (int j = 0; j < Q8_INPUT_SIZE; j++) {
            acc += (int32_t)input_q[j] * (int32_t)q8_w1[j * Q8_HIDDEN_SIZE + i];
        }
        acc -= (int32_t)Q8_W1_ZERO_POINT * sum_x;

        /* M1 已包含 1/Sh；负数会被 clamp 到 0，等价于 ReLU */
        hidden_q[i] = clamp_u8(qround_d((double)acc * (double)Q8_M1));
    }

    /* 第二层：32 -> 10 */
    int32_t sum_h = 0;
    for (int i = 0; i < Q8_HIDDEN_SIZE; i++) {
        sum_h += (int32_t)hidden_q[i];
    }

    for (int k = 0; k < Q8_OUTPUT_SIZE; k++) {
        int32_t acc = q8_b2[k];
        for (int i = 0; i < Q8_HIDDEN_SIZE; i++) {
            acc += (int32_t)hidden_q[i] * (int32_t)q8_w2[i * Q8_OUTPUT_SIZE + k];
        }
        acc -= (int32_t)Q8_W2_ZERO_POINT * sum_h;

        out_q[k] = clamp_u8(qround_d((double)acc * (double)Q8_M2) + Q8_OUTPUT_ZERO_POINT);
    }
}

/* 反量化 logits -> softmax；同时返回 argmax（按反量化后数值） */
static int dequant_softmax_argmax(const uint8_t* out_q, float* probs)
{
    float logits[Q8_OUTPUT_SIZE];
    int best = 0;
    for (int k = 0; k < Q8_OUTPUT_SIZE; k++) {
        logits[k] = Q8_OUTPUT_SCALE * ((float)out_q[k] - (float)Q8_OUTPUT_ZERO_POINT);
        if (k > 0 && logits[k] > logits[best]) {
            best = k;
        }
    }

    if (probs) {
        float max_v = logits[best];
        float sum = 0.0f;
        for (int k = 0; k < Q8_OUTPUT_SIZE; k++) {
            probs[k] = expf(logits[k] - max_v);
            sum += probs[k];
        }
        if (sum > 0.0f) {
            for (int k = 0; k < Q8_OUTPUT_SIZE; k++) {
                probs[k] /= sum;
            }
        }
    }
    return best;
}

int num_discern_uint8_predict(const uint8_t* input_q, float* probs)
{
    uint8_t out[Q8_OUTPUT_SIZE];
    num_discern_uint8_infer(input_q, NULL, out);
    return dequant_softmax_argmax(out, probs);
}

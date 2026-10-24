/* num_discern_1bit_model.c — 20x20 1-bit 输入 + uint8 量化 MLP 前向。
 *
 * 输入：50 字节打包位图，MSB 在前，row-major（y*20+x）。
 * 量化：输入 q = 0/1（S=1, Z=0）；权重 uint8；隐藏/输出 uint8。
 *
 * 累加：
 *   acc = Σ qx*qw + qb - Zw*Σqx     （输入 Zx=0）
 */
#include "num_discern_1bit_model.h"
#include "num_discern_1bit_weights.h"

#define ND1_INPUT_BITS_TOTAL (ND1_INPUT_BYTES * 8)

static uint8_t s_bits[ND1_INPUT_BITS_TOTAL];
static uint8_t s_hidden[ND1_HIDDEN_SIZE];
static uint8_t s_output[ND1_OUTPUT_SIZE];

uint32_t num_discern_1bit_model_size_bytes(void)
{
    return (uint32_t)(sizeof(nd1_w1) + sizeof(nd1_b1) + sizeof(nd1_w2) + sizeof(nd1_b2));
}

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

/* 1-bit packed -> 每像素 0/1；同时返回 bit 为 1 的个数 */
static int32_t unpack_bits(const uint8_t* packed, uint8_t* bits)
{
    int32_t sum = 0;
    for (int b = 0; b < ND1_INPUT_BYTES; b++) {
        uint8_t v = packed[b];
        for (int k = 0; k < 8; k++) {
            uint8_t bit = (uint8_t)((v >> (7 - k)) & 1u);
            bits[b * 8 + k] = bit;
            sum += bit;
        }
    }
    return sum;
}

int num_discern_1bit_predict(const uint8_t* packed)
{
    int32_t sum_x = unpack_bits(packed, s_bits);

    /* 第一层：400 -> hidden，ReLU + 再量化 */
    for (int i = 0; i < ND1_HIDDEN_SIZE; i++) {
        int32_t acc = nd1_b1[i];
        for (int j = 0; j < ND1_INPUT_BITS_TOTAL; j++) {
            acc += (int32_t)s_bits[j] * (int32_t)nd1_w1[j * ND1_HIDDEN_SIZE + i];
        }
        acc -= (int32_t)ND1_W1_ZERO_POINT * sum_x;
        s_hidden[i] = clamp_u8(qround_d((double)acc * (double)ND1_M1));
    }

    /* 第二层：hidden -> 10 */
    int32_t sum_h = 0;
    for (int i = 0; i < ND1_HIDDEN_SIZE; i++) {
        sum_h += (int32_t)s_hidden[i];
    }

    for (int k = 0; k < ND1_OUTPUT_SIZE; k++) {
        int32_t acc = nd1_b2[k];
        for (int i = 0; i < ND1_HIDDEN_SIZE; i++) {
            acc += (int32_t)s_hidden[i] * (int32_t)nd1_w2[i * ND1_OUTPUT_SIZE + k];
        }
        acc -= (int32_t)ND1_W2_ZERO_POINT * sum_h;
        s_output[k] = clamp_u8(qround_d((double)acc * (double)ND1_M2) + ND1_OUTPUT_ZERO_POINT);
    }

    int best = 0;
    for (int k = 1; k < ND1_OUTPUT_SIZE; k++) {
        if (s_output[k] > s_output[best]) {
            best = k;
        }
    }
    return best;
}

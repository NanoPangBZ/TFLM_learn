/* num_discern_model.c — MNIST MLP 前向传播（784 -> 32 -> 10）。
 * 权重在 num_discern_model_weights.h（tools/train_mnist_mlp.py 生成）。 */
#include "num_discern_model.h"
#include "num_discern_model_weights.h"

uint32_t num_discern_model_size_bytes(void)
{
    return (uint32_t)(sizeof(nd_w1) + sizeof(nd_b1) + sizeof(nd_w2) + sizeof(nd_b2));
}

int num_discern_predict(const float* input)
{
    float hidden[ND_HIDDEN_SIZE];
    float out[ND_OUTPUT_SIZE];

    /* 第一层：784 -> 32，ReLU */
    for (int i = 0; i < ND_HIDDEN_SIZE; i++) {
        float s = nd_b1[i];
        for (int j = 0; j < ND_INPUT_SIZE; j++) {
            s += input[j] * nd_w1[j * ND_HIDDEN_SIZE + i];
        }
        hidden[i] = (s > 0.0f) ? s : 0.0f;  /* ReLU */
    }

    /* 第二层：32 -> 10 */
    for (int k = 0; k < ND_OUTPUT_SIZE; k++) {
        float s = nd_b2[k];
        for (int i = 0; i < ND_HIDDEN_SIZE; i++) {
            s += hidden[i] * nd_w2[i * ND_OUTPUT_SIZE + k];
        }
        out[k] = s;
    }

    /* argmax（softmax 单调递增，直接取最大下标即可） */
    int best = 0;
    for (int k = 1; k < ND_OUTPUT_SIZE; k++) {
        if (out[k] > out[best]) {
            best = k;
        }
    }
    return best;
}

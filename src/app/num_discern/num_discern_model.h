#pragma once

#include <stdint.h>

#define ND_INPUT_SIZE   784   /* 28x28 */
#define ND_HIDDEN_SIZE  32
#define ND_OUTPUT_SIZE  10

/* 输入 784 维（28x28 展平，值域 [0,1]），返回识别出的数字 0-9 */
int num_discern_predict(const float* input);

/* 模型权重+偏置占用的字节数（float32） */
uint32_t num_discern_model_size_bytes(void);

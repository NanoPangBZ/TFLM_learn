#pragma once

#include <stdint.h>

#define Q8_INPUT_SIZE   784   /* 28x28 uint8 灰度 */
#define Q8_HIDDEN_SIZE  32
#define Q8_OUTPUT_SIZE  10

/**
 * @brief 纯 uint8 量化推理（输入、权重、隐藏激活、输出均为 8-bit）
 * @param input_q 784 个像素，0..255（S=1/255, Z=0）
 * @param hidden_q 输出 32 个隐藏层激活，0..255（可选，NULL 表示不需要）
 * @param out_q 输出 10 个量化 logits，0..255（可选，NULL 表示不需要）
 */
void num_discern_uint8_infer(const uint8_t* input_q, uint8_t* hidden_q, uint8_t* out_q);

/**
 * @brief 推理并返回 argmax（0..9）
 * @param input_q 784 个量化像素
 * @param probs 输出 10 个 softmax 概率（反量化 logits 后计算；可为 NULL）
 */
int num_discern_uint8_predict(const uint8_t* input_q, float* probs);

/**
 * @brief 模型权重+偏置占用的字节数（uint8/int32）
 */
uint32_t num_discern_uint8_model_size_bytes(void);

#pragma once

#include <stdint.h>

#define ND1_INPUT_BITS   400    /* 20x20 */
#define ND1_INPUT_BYTES  50     /* 400 bit */
#define ND1_OUTPUT_SIZE  10

/**
 * @brief 纯 uint8 量化推理，输入是 20x20 的 1-bit 打包位图
 * @param packed 50 字节；MSB 在前，按 row-major 从左到右、从上到下
 * @return 识别出的数字 0..9
 *
 * 位序：bit_index = y*20 + x；byte = bit_index / 8；
 *       bit = 7 - (bit_index % 8)（高位在前）。
 */
int num_discern_1bit_predict(const uint8_t* packed);

/**
 * @brief 模型权重+偏置占用的字节数（uint8/int32）
 */
uint32_t num_discern_1bit_model_size_bytes(void);

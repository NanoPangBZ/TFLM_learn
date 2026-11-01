#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取外部符号地址
 * @param symbol_name 符号名称
*/
void* c_extern_symbol_get( const char* symbol_name);

/**
 * @brief 获取外部符号表中的符号数量
*/
uint32_t c_extern_symbol_table_symbol_num(void);

/**
 * @brief 获取外部符号名称
 * @param index 符号索引
 * @return 符号名称
*/
const char* c_extern_symbol_table_symbol_name(uint32_t index);

#ifdef __cplusplus
}
#endif


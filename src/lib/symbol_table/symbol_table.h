#pragma once

#include <stdint.h>
#include <stddef.h>

#pragma pack(1)
typedef struct symbol_item_t{
    uint32_t    magic;
    char        name[32];
    void*       symbol;
}symbol_item_t;

#pragma pack()

#define SYMBOL_TABLE_ITEM_MAGIC 0x53425442  // "SBTB"的ASCII码

/**
 * @brief 导出到外部符号
 * @param _symbol 符号名称
 * @note 该宏会将指定的符号导出到外部符号表中，供其他模块通过符号名称获取符号地址
*/
#define EXPORT_SYMBOL( _symbol ) \
    __attribute__(( used , section("__symbol_table"))) static const symbol_item_t __symbol_item_##_symbol = { \
        .magic = SYMBOL_TABLE_ITEM_MAGIC, \
        .name = #_symbol, \
        .symbol = (void*)(&_symbol), \
    }

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 偏移外部符号地址
 * @param symbol_table 外部符号表地址
 * @param offset 偏移量，单位为字节
*/
void symbol_offset( void* symbol_table , int64_t offset );

/**
 * @brief 获取符号表的大小
 * @param symbol_table 外部符号表地址
 * @return size_t 符号表的大小，单位为字节
*/
size_t symbol_table_size( void* symbol_table );

/**
 * @brief 获取外部符号地址
 * @param symbol_table 外部符号表地址
 * @param symbol_name 符号名称
 * @return void* 符号地址，如果未找到则返回 NULL
*/
void* symbol_get( void* symbol_table , const char* symbol_name);

/**
 * @brief 获取外部符号数量
 * @param symbol_table 外部符号表地址
*/
uint32_t symbol_table_num_get( void* symbol_table);

/**
 * @brief 按序号取符号名字符串（等价于连续 symbol_item_t 数组下标访问）
 * @param symbol_table 符号表首地址
 * @param index 第几项（不做范围与 magic 校验）
 */
const char* symbol_get_name( void* symbol_table , uint32_t index );

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 格式化数据
 * @param partition 分区
 * @return 0:成功,其他:失败
*/
uint32_t e_kv_format( const char* partition );

/**
 * @brief 获取二进制数据
 * @param partition 分区
 * @param key 键
 * @param buf 缓冲区
 * @param size 缓冲区大小
 * @return 获取到的数据大小
*/
uint32_t e_kv_read_blob( const char* partition , const char* key , void* buf , uint32_t size );

/**
 * @brief 写入二进制数据
 * @param partition 分区
 * @param key 键
 * @param buf 缓冲区
 * @param size 缓冲区大小
 * @return 写入的数据大小
*/
uint32_t e_kv_write_blob( const char* partition , const char* key , const void* buf , uint32_t size );

/**
 * @brief 删除数据
 * @param partition 分区
 * @param key 键
 * @return 0:成功,其他:失败
*/
int e_kv_delete( const char* partition , const char* key );

/**
 * @brief 判断数据是否存在
 * @param partition 分区
 * @param key 键
 * @return 0:不存在,其他:存在
*/
uint8_t e_kv_exists( const char* partition , const char* key );

#ifdef __cplusplus
}
#endif

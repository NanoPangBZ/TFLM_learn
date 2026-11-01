#pragma once

#include <stdint.h>

// 环形缓冲区句柄
typedef struct c_ringbuf_t* c_ringbuf_handle_t;

// 环形缓冲区属性枚举
typedef enum c_ringbuf_attr_e{
    C_RINGBUF_ATTR_DEFAULT = 0,         // 默认属性
    C_RINGBUF_ATTR_FAST_ACCESS = 1 << 0 // 快速访问
}c_ringbuf_attr_e;
typedef uint32_t c_ringbuf_attr_t;

// 环形缓冲区描述结构体
typedef struct c_ringbuf_desc_t{
    uint32_t size;              // 缓冲区大小
    c_ringbuf_attr_t attr;      // 缓冲区属性
}c_ringbuf_desc_t;

#ifdef __cplusplus
 extern "C" {
#endif

/**
 * @brief 创建环形缓冲区
 * @param desc 环形缓冲区描述结构体指针
 * @return 环形缓冲区句柄，失败返回NULL
*/
c_ringbuf_handle_t c_ringbuf_create(c_ringbuf_desc_t* desc);

/**
 * @brief 销毁环形缓冲区
*/
void c_ringbuf_destroy(c_ringbuf_handle_t ringbuf);

/**
 * @brief 向环形缓冲区写入数据
 * @param ringbuf 环形缓冲区句柄
 * @param data 要写入的数据指针
 * @param size 要写入的数据长度，单位字节
 * @return 实际写入的数据长度，失败返回-1
*/
int c_ringbuf_write(c_ringbuf_handle_t ringbuf, const uint8_t* data, uint32_t size);

/**
 * @brief 从环形缓冲区读取数据
 * @param ringbuf 环形缓冲区句柄
 * @param data 读取数据存放的指针
 * @param size 要读取的数据长度，单位字节
 * @return 实际读取的数据长度，失败返回0
*/
int c_ringbuf_read(c_ringbuf_handle_t ringbuf, uint8_t* data, uint32_t size);

/**
 * @brief 从ISR中写入环形缓冲区
 * @param ringbuf 环形缓冲区句柄
 * @param data 要写入的数据指针
 * @param size 要写入的数据长度，单位字节
 * @return 实际写入的数据长度，失败返回-1
*/
int c_ringbuf_write_from_isr(c_ringbuf_handle_t ringbuf, const uint8_t* data, uint32_t size);

/**
 * @brief 从ISR中读取环形缓冲区
 * @param ringbuf 环形缓冲区句柄
 * @param data 读取数据存放的指针
 * @param size 要读取的数据长度，单位字节
 * @return 实际读取的数据长度，失败返回0
*/
int c_ringbuf_read_from_isr(c_ringbuf_handle_t ringbuf, uint8_t* data, uint32_t size);

/**
 * @brief 检查环形缓冲区是否为空
 * @param ringbuf 环形缓冲区句柄
 * @return 1表示为空，0表示不为空
*/
uint8_t c_ringbuf_is_empty(c_ringbuf_handle_t ringbuf);

/**
 * @brief 检查环形缓冲区是否已满
 * @param ringbuf 环形缓冲区句柄
 * @return 1表示已满，0表示未满
*/
uint8_t c_ringbuf_is_full(c_ringbuf_handle_t ringbuf);

/**
 * @brief 清空环形缓冲区
 * @param ringbuf 环形缓冲区句柄
*/
void c_ringbuf_clear(c_ringbuf_handle_t ringbuf);

#ifdef __cplusplus
}
#endif

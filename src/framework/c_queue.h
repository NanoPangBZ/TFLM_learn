#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct c_queue_t* c_queue_handle_t;

typedef struct c_queue_desc_t{
    uint32_t    item_size;   //每个元素的大小
    uint32_t    queue_size;  //队列容量
}c_queue_desc_t;

/**
 * @brief 创建一个循环队列
 * @param desc 队列描述信息
 * @return 队列句柄
 */
c_queue_handle_t c_queue_create(c_queue_desc_t* desc);

/**
 * @brief 销毁一个循环队列
 * @param queue 队列句柄
 * @note 未实现
*/
void c_queue_destroy(c_queue_handle_t queue);

/**
 * @brief 向队列中添加一个元素
 * @param queue 队列句柄
 * @param item 要添加的元素指针
 * @return 0 成功，其他失败
*/
int c_queue_enqueue(c_queue_handle_t queue, const void* item);

/**
 * @brief 从队列中取出一个元素
 * @param queue 队列句柄
 * @param item 要取出的元素指针
 * @return 0 成功，其他失败
*/
int c_queue_dequeue(c_queue_handle_t queue, void* item);

/**
 * @brief 判断队列是否为空
 * @param queue 队列句柄
 * @return 1 空，0 非空
*/
uint8_t c_queue_is_empty(c_queue_handle_t queue);

/**
 * @brief 判断队列是否已满
 * @param queue 队列句柄
 * @return 1 满，0 未满
*/
uint8_t c_queue_is_full(c_queue_handle_t queue);

/**
 * @brief 清空队列
 * @param queue 队列句柄
*/
void c_queue_clear(c_queue_handle_t queue);

/**
 * @brief 获取队列中当前元素数量
 * @param queue 队列句柄
 * @return 当前元素数量
*/
uint32_t c_queue_get_count(c_queue_handle_t queue);

/**
 * @brief 获取队列中每个元素的大小
 * @param queue 队列句柄
 * @return 元素大小，单位字节
*/
uint16_t c_queue_get_element_size(c_queue_handle_t queue);

#ifdef __cplusplus
}
#endif


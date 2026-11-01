#pragma once

#include <stdint.h>

typedef struct e_mutex_t* e_mutex_handle_t;

#ifdef __plusplus
extern "C" {
#endif

/**
 * @brief 创建互斥锁
 * @return 互斥锁句柄
*/
e_mutex_handle_t e_mutex_create(void);

/**
 * @brief 销毁互斥锁
 * @param mutex 互斥锁句柄
*/
void e_mutex_destroy(e_mutex_handle_t mutex);

/**
 * @brief 尝试加锁互斥锁
 * @param mutex 互斥锁句柄
 * @param timeout_ms 超时时间，单位毫秒
 * @return 1:加锁成功 0:加锁失败
*/
uint8_t e_mutex_try_lock(e_mutex_handle_t mutex , uint32_t timeout_ms);

/**
 * @brief 加锁互斥锁
 * @param mutex 互斥锁句柄
*/
void e_mutex_lock(e_mutex_handle_t mutex);

/**
 * @brief 解锁互斥锁
 * @param mutex 互斥锁句柄
*/
void e_mutex_unlock(e_mutex_handle_t mutex);

/**
 * @brief 从ISR中解锁互斥锁
 * @param mutex 互斥锁句柄
*/
void e_mutex_unlock_from_isr(e_mutex_handle_t mutex);

#ifdef __cplusplus
}
#endif


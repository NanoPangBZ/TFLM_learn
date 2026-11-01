#pragma once

#include <stdint.h>

/* 信号句柄 */
typedef void* e_sig_handle_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建信号
 * @param name 信号名称
 * @return 信号句柄
 */
e_sig_handle_t e_sig_create(void);

/**
 * @brief 销毁信号
 * @param handle 信号句柄
*/
void e_sig_destroy(e_sig_handle_t handle);

/**
 * @brief 发送信号
 * @param handle 信号句柄
*/
void e_sig_send(e_sig_handle_t handle);

/**
 * @brief 从ISR发送信号
 * @param handle 信号句柄
*/
void e_sig_send_from_isr(e_sig_handle_t handle);

/**
 * @brief 等待信号
 * @param handle 信号句柄
*/
void e_sig_wait(e_sig_handle_t handle);

#ifdef __cplusplus
}
#endif


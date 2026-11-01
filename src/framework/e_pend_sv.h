#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 触发e_pend_sv
*/
void e_pend_sv_trigger(void);

/**
 * @brief 注册e_pend_sv回调函数
 * @param function 回调函数
*/
void e_pend_sv_register(void (*function)(void));

#ifdef __cplusplus
}
#endif

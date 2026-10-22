#pragma once

#include <stdint.h>
#include "framework_cfg.h"

typedef int16_t (*e_log_out_channel_func_t)(void* ctx , uint8_t* data, uint16_t len);
typedef uint32_t (*e_log_time_source_func_t)(void);

//日志等级枚举
typedef enum e_log_level_e{
    E_LOG_LEVEL_DEBUG = 0,
    E_LOG_LEVEL_INFO,
    E_LOG_LEVEL_WARN,
    E_LOG_LEVEL_ERROR,
}e_log_level_e;
typedef uint8_t e_log_level_t;

#if defined(E_LOG_ENABLE) && (E_LOG_ENABLE == 1)
    #define E_DEBUG_LOG(TAG, ...)    e_log_printf(E_LOG_LEVEL_DEBUG, TAG, __VA_ARGS__)
    #define E_INFO_LOG(TAG, ...)     e_log_printf(E_LOG_LEVEL_INFO, TAG, __VA_ARGS__)
    #define E_WARN_LOG(TAG, ...)     e_log_printf(E_LOG_LEVEL_WARN, TAG, __VA_ARGS__)
    #define E_ERROR_LOG(TAG, ...)    e_log_printf(E_LOG_LEVEL_ERROR, TAG, __VA_ARGS__)
#else
    #define E_DEBUG_LOG(TAG, ...)
    #define E_INFO_LOG(TAG, ...)
    #define E_WARN_LOG(TAG, ...)
    #define E_ERROR_LOG(TAG, ...)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 设置日志输出通道函数
 * @param out_channel_func 日志输出通道函数指针
 * @param ctx 上下文指针,传递给out_channel_func
*/
void e_log_set_out_channel(e_log_out_channel_func_t out_channel_func,void* ctx);

/**
 * @brief 设置日志时间源函数
 * @param time_source_func 日志时间源函数指针
*/
void e_log_set_time_source(e_log_time_source_func_t time_source_func);

/**
 * @brief 使能或禁用日志输出
 * @param enable 1:使能 0:禁用
*/
void e_log_enable(uint8_t enable);

/**
 * @brief 设置日志等级
 * @param level 日志等级
*/
void e_log_set_level(e_log_level_t level);

/**
 * @brief 设置日志等级
 * @param level 日志等级
 * @param tag 日志标签
 * @param fmt 格式化字符串
 * @param 其它参数
*/
void e_log_printf(e_log_level_t level, const char* tag ,const char* fmt, ...);

/**
 * @brief 设置日志关注的标签,只输出该标签的日志
 * @param tag 标签字符串指针
*/
void e_log_set_focus_tag(const char* tag);

#ifdef __cplusplus
}
#endif


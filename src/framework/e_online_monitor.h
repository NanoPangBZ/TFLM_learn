#pragma once

#include <stdint.h>
#include "framework_cfg.h"

// 在线监视器支持的数据类型
typedef enum e_online_monitor_value_type_e{
    E_ONLINE_MONITOR_VALUE_TYPE_INT8 = 0,
    E_ONLINE_MONITOR_VALUE_TYPE_UINT8,
    E_ONLINE_MONITOR_VALUE_TYPE_INT16,
    E_ONLINE_MONITOR_VALUE_TYPE_UINT16,
    E_ONLINE_MONITOR_VALUE_TYPE_INT32,
    E_ONLINE_MONITOR_VALUE_TYPE_UINT32,
    E_ONLINE_MONITOR_VALUE_TYPE_FLOAT,
}e_online_monitor_value_type_e;
typedef uint8_t e_online_monitor_value_type_t;

#if defined(ENABLE_E_ONLINE_MONITOR) && (ENABLE_E_ONLINE_MONITOR > 0)
    #define E_ONLINE_MONITOR_REG_VALUE(gruop , name , p_value , value_type)    e_online_monitor_register_value( gruop , name , p_value , value_type )
#else
    #define E_ONLINE_MONITOR_REG_VALUE(gruop , name , p_value , value_type)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(ENABLE_E_ONLINE_MONITOR) && (ENABLE_E_ONLINE_MONITOR > 0)

/**
 * @brief 注册一个在线监视器变量
 * @param gruop 变量所属组名
 * @param name 变量名称
 * @param p_value 指向变量的指针
 * @param value_type 变量的数据类型，见e_online_monitor_value_type_t定义
 * @return 0:成功，其它:失败
*/
int16_t e_online_monitor_register_value( const char* gruop , const char* name , const void* p_value , e_online_monitor_value_type_t value_type );

#endif

#ifdef __cplusplus
}
#endif

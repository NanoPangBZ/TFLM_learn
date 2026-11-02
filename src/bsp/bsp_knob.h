#pragma once

#include <stdint.h>

/******************************************************类型定义*************************************************************************/

/**
 * @brief knob句柄类型, 用于标识一个 knob 设备实例
 */
typedef struct bsp_knob_t *bsp_knob_handle_t;

#ifdef __cplusplus
extern "C"
{
#endif

/******************************************************打开/关闭 接口*****************************************************/

/**
 * @brief 获取knob设备句柄
 * @param name knob设备名称
 * @return knob设备句柄
*/
bsp_knob_handle_t bsp_knob_get( const char* name );

/******************************************************查询/控制 接口*****************************************************/

/**
 * @brief 获取knob当前值
 * @param handle knob设备句柄
 * @return knob当前值( 0-1.0f 范围)
 */
float bsp_knob_get_value(bsp_knob_handle_t handle);

#ifdef __cplusplus
}
#endif

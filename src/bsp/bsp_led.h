#pragma once

#include <stdint.h>

/******************************************************类型定义*************************************************************************/

/* bsp_led索引 */
typedef uint8_t bsp_led_idx_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 设置LED的值
 * @param handle LED设备句柄
 * @param value LED的值 - 根据实现可能是亮度值、开关状态等
*/
void bsp_led_set_value(bsp_led_idx_t led_idx , uint32_t value );

/**
 * @brief 获取LED的值
 * @param handle LED设备句柄
 * @return LED的值 - 根据实现可能是亮度值、开关状态等
*/
uint32_t bsp_led_get_value(bsp_led_idx_t led_idx);

#ifdef __cplusplus
}
#endif

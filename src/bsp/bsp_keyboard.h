#pragma once

#include <stdint.h>

/******************************************************类型定义*************************************************************************/

/**
 * @brief keyboard句柄类型, 用于标识一个 keyboard 设备实例
 */
typedef struct bsp_keyboard_t* bsp_keyboard_handle_t;

/**
 * @brief keyboard触发回调函数类型定义
 * @param handle keyboard设备句柄
 * @param ctx 回调函数上下文指针
 * @param key_mask 当前按键位图
 * @param timestamp_ms 消息中携带的时间戳(毫秒或协议原值，根据协议字段 Time_stamp 传递)
*/
typedef void (*bsp_keyboard_trigger_cb_t)( bsp_keyboard_handle_t handle , void* ctx , uint32_t key_mask , uint32_t timestamp_ms );

/**
 * @brief keyboard 类型枚举
 */
typedef enum bsp_keyboard_type_e{
    BSP_KEYBOARD_TYPE_UNKNOWN = 0,                  /**< 未知类型 */
    BSP_KEYBOARD_TYPE_FINGER_BOARD = 1,             /**< 指板键盘(吉他指板) */
    BSP_KEYBOARD_TYPE_TOUCH_PAD = 2,                /**< 触摸键盘(触摸板) */
    BSP_KEYBOARD_TYPE_SYSTEM_CONTROL = 3,          /**< 系统控制键盘(音量/播放等) */
    BSP_KEYBOARD_TYPE_VIRTUAL_TEST = 4,            /**< 虚拟测试键盘(用于测试和仿真) */
}bsp_keyboard_type_e;
typedef uint8_t bsp_keyboard_type_t;

/**
 * @brief keyboard 属性枚举
 */
typedef enum bsp_keyboard_attr_e{
    BSP_KEYBOARD_ATTR_DEFAULT = 0x00,                /**< 默认属性 */
}bsp_keyboard_attr_e;
typedef uint32_t bsp_keyboard_attr_t;

/**
 * @brief keyboard 描述结构体, 用于传入设备打开参数
 */
typedef struct bsp_keyboard_desc_t{
    bsp_keyboard_type_t type;
    uint8_t             idx;
    bsp_keyboard_attr_t attr;
}bsp_keyboard_desc_t;

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************打开/关闭 接口*****************************************************/

/**
 * @brief 打开 keyboard 设备
 * @param desc 设备描述结构体指针, 包含类型/索引/属性
 * @return 成功返回设备句柄, 失败返回 NULL
 */
bsp_keyboard_handle_t bsp_keyboard_open(bsp_keyboard_desc_t* desc);

/**
 * @brief 关闭 keyboard 设备
 * @param handle 设备句柄
 */
void bsp_keyboard_close( bsp_keyboard_handle_t handle );

/******************************************************查询/控制 接口*****************************************************/

/**
 * @brief 注册 keyboard 事件触发回调函数
 * @param handle 设备句柄
 * @param cb 回调函数指针, 事件发生时被调用
 * @param ctx 回调上下文指针, 原样传回回调
 * @return 操作结果, 成功返回0, 失败返回非0
 */
int bsp_keyboard_register_trigger_callback( bsp_keyboard_handle_t handle , bsp_keyboard_trigger_cb_t cb , void* ctx );

/**
 * @brief 获取当前 keyboard 全部按键位图
 * @param handle 设备句柄
 * @return 按键位图(每一位代表一个按键, 1=按下,0=未按)
 */
uint32_t bsp_keyboard_get_key_mask( bsp_keyboard_handle_t handle );

#ifdef __cplusplus
}
#endif




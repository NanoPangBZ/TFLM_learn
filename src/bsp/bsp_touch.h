#pragma once

#include <stdint.h>

/******************************************************类型定义*************************************************************************/

// 触摸屏句柄类型 - 用于标识触摸屏设备
typedef struct bsp_touch_t* bsp_touch_handle_t;

//触摸屏类型枚举 - 用于打开触摸屏设备
typedef enum bsp_touch_type_e{
    BSP_TOUCH_TYPE_UNKNOWN = 0,
    BSP_TOUCH_TYPE_USER,              //用户触摸屏
}bsp_touch_type_e;
typedef uint8_t bsp_touch_type_t;

// 触摸点坐标结构体 - 用于获取触摸点坐标
typedef struct bsp_touch_point_t{
    int x;
    int y;
}bsp_touch_point_t;

// 触摸屏属性枚举 - 用于打开触摸屏设备
typedef enum bsp_touch_attr_e{
    BSP_TOUCH_ATTR_DEFAULT = 0x00,    //默认属性
}bsp_touch_attr_e;
typedef uint32_t bsp_touch_attr_t;

// 触摸屏描述结构体 - 用于打开触摸屏设备
typedef struct bsp_touch_desc_t{
    bsp_touch_type_t    type;
    uint8_t             idx;
    bsp_touch_attr_t    attr;
}bsp_touch_desc_t;

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************打开/关闭 接口*****************************************************/

/**
 * @brief 打开touch设备
 * @param desc touch描述结构体指针
 * @return touch设备句柄
*/
bsp_touch_handle_t bsp_touch_open(bsp_touch_desc_t* desc);

/**
 * @brief 关闭touch设备
 * @param handle touch设备句柄
*/
void bsp_touch_close( bsp_touch_handle_t handle );

/******************************************************查询/控制 接口*****************************************************/

/**
 * @brief 获取触摸点数量
 * @param handle touch设备句柄
 * @return 返回触摸点数量
*/
uint8_t bsp_touch_count( bsp_touch_handle_t handle );

/**
 * @brief 获取touch设备的触摸点坐标
 * @param handle touch设备句柄
 * @param index 触摸点索引
 * @param point 触摸点坐标结构体指针
 * @return 返回0表示获取触摸点成功，非0表示获取触摸点失败
*/
uint8_t bsp_touch_get_touch_point( bsp_touch_handle_t handle, uint8_t index, bsp_touch_point_t* point );

#ifdef __cplusplus
}
#endif


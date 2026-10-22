#pragma once

#include <stdint.h>

/******************************************************类型定义*************************************************************************/

// 显示器句柄类型 - 用于标识显示器设备
typedef struct bsp_display_t* bsp_display_handle_t;

//状态枚举
typedef enum bsp_display_status_e{
    BSP_DISPLAY_STATUS_SUCCESS = 0,     //成功
    BSP_DISPLAY_STATUS_BUSY  = 1,       //忙碌
    BSP_DISPLAY_STATUS_ERROR = 2,       //错误
}bsp_display_status_e;
typedef uint8_t bsp_display_status_t;

//显示器类型枚举 - 用于打开display设备
typedef enum bsp_display_type_e{
    BSP_DISPLAY_TYPE_UNKNOWN = 0,
    BSP_DISPLAY_TYPE_USER,              //用户显示器
}bsp_display_type_e;
typedef uint8_t bsp_display_type_t;

// 显示器属性枚举 - 用于打开显示器设备
typedef enum bsp_display_attr_e{
    BSP_DISPLAY_ATTR_DEFAULT = 0x00,    //默认属性
}bsp_display_attr_e;
typedef uint32_t bsp_display_attr_t;

// 显示器描述结构体 - 用于打开显示器设备
typedef struct bsp_display_desc_t{
    bsp_display_type_t type;
    uint8_t            idx;
    bsp_display_attr_t attr;
}bsp_display_desc_t;

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************打开/关闭 接口*****************************************************/

/**
 * @brief 打开display设备
 * @param desc display描述结构体指针
 * @return display设备句柄
*/
bsp_display_handle_t bsp_display_open(bsp_display_desc_t* desc);

/**
 * @brief 关闭display设备
 * @param handle display设备句柄
*/
void bsp_display_close( bsp_display_handle_t handle );

/******************************************************查询/控制 接口*****************************************************/

/**
 * @brief 打开/关闭显示器
 * @param handle display设备句柄
 * @param on_off 0: 关闭显示器, 1: 打开显示器
 * @return 操作结果状态
*/
bsp_display_status_t bsp_display_on(bsp_display_handle_t handle , uint8_t on_off );

/**
 * @brief 查询显示器是否打开
 * @param handle display设备句柄
 * @return 0: 关闭, 1: 打开
*/
uint8_t bsp_display_is_on(bsp_display_handle_t handle);

/**
 * @brief 获取显示器尺寸
 * @param handle display设备句柄
 * @param width 显示器宽度指针，单位：像素
 * @param height 显示器高度指针，单位：像素
 * @return 操作结果状态
*/
bsp_display_status_t bsp_display_get_size(bsp_display_handle_t handle, uint16_t* width, uint16_t* height);

/**
 * @brief 填充显示器屏幕颜色
 * @param handle display设备句柄
 * @param color 填充颜色，格式为0xRRGGBB
 * @return 操作结果状态
 * @note 阻塞式接口
*/
bsp_display_status_t bsp_display_fill(bsp_display_handle_t handle, uint32_t color);

/**
 * @brief 检查显示器是否忙碌
 * @param handle display设备句柄
 * @return 0: 空闲, 1: 忙碌
*/
uint8_t bsp_display_is_busy(bsp_display_handle_t handle);

/**
 * @brief 设置显示器亮度
 * @param handle display设备句柄
 * @param brightness 亮度值，范围0-100
 * @return 操作结果状态
*/
bsp_display_status_t bsp_display_set_brightness(bsp_display_handle_t handle, uint8_t brightness);

/**
 * @brief 获取显示器亮度
 * @param handle display设备句柄
 * @return 亮度值，范围0-100
*/
uint8_t bsp_display_get_brightness(bsp_display_handle_t handle);

/**
 * @brief 获取显示器绘制缓存指针
 * @param handle display设备句柄
 * @param buf_size 绘制缓存大小指针，单位：字节
 * @return 绘制缓存指针
*/
uint8_t* bsp_display_get_draw_buffer(bsp_display_handle_t handle,uint32_t* buf_size);

/**
 * @brief 设置显示器绘制缓存指针
 * @param handle display设备句柄
 * @param draw_buffer 绘制缓存指针
 * @param buf_size 绘制缓存大小，单位：字节
 * @return 操作结果状态
*/
bsp_display_status_t bsp_display_set_draw_buffer(bsp_display_handle_t handle, uint8_t* draw_buffer, uint32_t buf_size);

/**
 * @brief 刷新显示器，将绘制缓存内容更新到屏幕
 * @param handle display设备句柄
 * @param x 刷新区域起始X坐标，单位：像素
 * @param y 刷新区域起始Y坐标，单位：像素
 * @param width 刷新区域宽度，单位：像素
 * @param height 刷新区域高度，单位：像素
 * @return 操作结果状态
 * @note 非阻塞式接口
*/
bsp_display_status_t bsp_display_flush(bsp_display_handle_t handle,uint16_t x,uint16_t y,uint16_t width,uint16_t height);

/**
 * @brief 注册显示器刷新完成回调函数
 * @param handle display设备句柄
 * @param callback 回调函数
 * @param ctx 回调函数上下文
 * @return 操作结果状态
*/
bsp_display_status_t bsp_display_register_flush_finnish_trigger(bsp_display_handle_t handle, void (*callback)(bsp_display_handle_t, void*), void* ctx);

/**
 * @brief 等待显示器刷新完成
 * @param handle display设备句柄
*/
void bsp_display_wait_flush_finish(bsp_display_handle_t handle);

/**
 * @brief 等待TE信号
 * @param handle display设备句柄
*/
void bsp_display_wait_te_signal(bsp_display_handle_t handle);

#ifdef __cplusplus
}
#endif


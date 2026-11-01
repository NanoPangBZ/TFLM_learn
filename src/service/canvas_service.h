#pragma once

#include <stdint.h>

/**
 * @brief 画布服务格式枚举
*/
typedef enum canvas_service_img_format_e{
    CANVAS_SERVICE_IMG_FORMAT_RGB565 = 0,
    CANVAS_SERVICE_IMG_FORMAT_RGB888 = 1,
}canvas_service_img_format_e;

/**
 * @brief 画布服务触发器结构体
*/
typedef struct canvas_service_trigger_t{
    uint32_t width;
    uint32_t height;
    uint32_t img_format;
    const uint8_t* img_data;
}canvas_service_trigger_t;

/**
 * @brief 画布服务触发器回调函数类型
 * @param trigger 触发器结构体指针
 * @param ctx 上下文指针
*/
typedef void (*canvas_service_trigger_callback_t)(const canvas_service_trigger_t* trigger,void* ctx);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启用画布服务
 * @param enable 1: 启用, 0: 禁用
*/
void canvas_service_enable( uint8_t enable );

/**
 * @brief 是否启用画布服务
 * @return 1: 启用, 0: 禁用
*/
uint8_t canvas_service_is_enabled(void);

/**
 * @brief 注册画布服务触发器回调函数（支持多个，追加到回调列表）
 * @param callback 回调函数指针
 * @param ctx 上下文指针
*/
void canvas_service_register_trigger_callback(canvas_service_trigger_callback_t callback,void* ctx);

/**
 * @brief 反注册画布服务触发器回调函数（按回调指针移除第一个匹配项）
 * @param callback 回调函数指针
 */
void canvas_service_unregister_trigger_callback(canvas_service_trigger_callback_t callback);

#ifdef __cplusplus
}
#endif


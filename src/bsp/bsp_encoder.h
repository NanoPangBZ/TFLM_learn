#pragma once

#include <stdint.h>

/******************************************************类型定义*************************************************************************/

// encoder句柄类型 - 用于标识encoder设备
typedef struct bsp_encoder_t* bsp_encoder_handle_t;

// encoder触发信息结构体 - 用于传递encoder触发事件的信息
typedef struct bsp_encoder_trigger_t{
    int16_t inc_value;          //增量值，正数表示顺时针，负数表示逆时针
    uint32_t position_value;    //位置值
    float    speed_rps_pulse;   //速度，单位：脉冲/秒
    uint8_t btn_status : 1;     //按钮状态，1表示按下，0表示释放
    uint8_t reserved   : 7;     //保留位
    uint32_t timestamp;         //时间戳
}bsp_encoder_trigger_t;

// encoder触发回调函数类型定义
typedef void (*bsp_encoder_trigger_cb_t)( bsp_encoder_handle_t handle ,  void* ctx , bsp_encoder_trigger_t* trigger );

//状态枚举
typedef enum bsp_encoder_status_e{
    BSP_ENCODER_STATUS_SUCCESS = 0,               //成功
    BSP_ENCODER_STATUS_ERROR = 1,                 //错误
}bsp_encoder_status_e;
typedef uint8_t bsp_encoder_status_t;

// encoder类型枚举 - 用于打开encoder设备
typedef enum bsp_encoder_type_e{
    BSP_ENCODER_TYPE_UNKNOWN = 0,
    BSP_ENCODER_TYPE_SYSTEM_CONTROL = 1,          //系统控制编码器
    BSP_ENCODER_TYPE_BPM_CONTROL = 2,             //BPM控制编码器
}bsp_encoder_type_e;
typedef uint8_t bsp_encoder_type_t;

// encoder属性枚举 - 用于打开encoder设备
typedef enum bsp_encoder_attr_e{
    BSP_ENCODER_ATTR_DEFAULT = 0x00,                    // 默认属性
}bsp_encoder_attr_e;
typedef uint32_t bsp_encoder_attr_t;

// encoder描述结构体 - 用于打开encoder设备
typedef struct bsp_encoder_desc_t{
    bsp_encoder_type_t type;
    uint8_t             idx;
    bsp_encoder_attr_t attr;
    int8_t             dir; //1:正向，-1:反向，0:默认
}bsp_encoder_desc_t;

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************打开/关闭 接口*****************************************************/

/**
 * @brief 打开encoder设备
 * @param desc encoder描述结构体指针
 * @return encoder设备句柄
*/
bsp_encoder_handle_t bsp_encoder_open(bsp_encoder_desc_t* desc);

/**
 * @brief 关闭encoder设备
 * @param handle encoder设备句柄
*/
void bsp_encoder_close( bsp_encoder_handle_t handle );

/******************************************************查询/控制 接口*****************************************************/

/**
 * @brief 注册encoder触发回调函数
 * @param handle encoder设备句柄
 * @param callback 触发回调函数
 * @param ctx 用户上下文
 * @return encoder设备句柄
*/
bsp_encoder_status_t bsp_encoder_register_trigger_callback(bsp_encoder_handle_t handle, bsp_encoder_trigger_cb_t callback, void* ctx);

/**
 * @brief 获取encoder当前位置
 * @param handle encoder设备句柄
 * @return encoder当前位置
*/
uint32_t bsp_encoder_position(bsp_encoder_handle_t handle);

/**
 * @brief 重置encoder当前位置为0
 * @param handle encoder设备句柄
 * @return 操作结果状态
*/
bsp_encoder_status_t bsp_encoder_reset_position(bsp_encoder_handle_t handle);

/**
 * @brief 获取encoder按钮状态
 * @param handle encoder设备句柄
 * @return 按钮状态，1表示按下，0表示释放
*/
uint8_t bsp_encoder_is_btn_pressed(bsp_encoder_handle_t handle);

#ifdef __cplusplus
}
#endif


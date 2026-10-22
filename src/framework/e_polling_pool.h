#pragma once

#include <stdint.h>
#include <stddef.h>

//轮询任务句柄
typedef struct e_polling_task_t* e_polling_task_handle_t;

//轮询任务属性
typedef enum e_polling_task_attr_e{
    E_POLLING_TASK_ATTR_NONE = 0x00,
    E_POLLING_TASK_ATTR_REALTIME = 0x01 << 0,    //实时任务
    E_POLLING_TASK_ATTR_DEEP_STACK = 0x01 << 1,  //深栈任务

    //其他扩展属性
    //...

    E_POLLING_TASK_ATTR_DEFAULT = E_POLLING_TASK_ATTR_NONE,
}e_polling_task_attr_e;
typedef uint32_t e_polling_task_attr_t;

//轮询任务描述
typedef struct e_polling_task_desc_t{
    const char* name;
    void* user_ctx;
    void (*polling_func)(void* user_ctx);
    uint32_t freq_hz;                       //轮询频率(Hz)，0表示不定时轮询
    uint8_t start_before_create;          //创建前是否启动
    e_polling_task_attr_t attr;
}e_polling_task_desc_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建轮询任务
 * @param desc 轮询任务描述
 * @return 轮询任务句柄
*/
e_polling_task_handle_t e_polling_task_create( e_polling_task_desc_t* desc );

/**
 * @brief 启动轮询任务
 * @param handle 轮询任务句柄
 * @return 0成功，非0失败
*/
void e_polling_task_destroy( e_polling_task_handle_t handle );

/**
 * @brief 启动轮询任务
 * @param handle 轮询任务句柄
*/
void e_polling_task_start( e_polling_task_handle_t handle );

/**
 * @brief 停止轮询任务
 * @param handle 轮询任务句柄
 * @note 当前任务正在执行时，停止操作会等待当前任务执行完毕后再停止
*/
void e_polling_task_stop( e_polling_task_handle_t handle );

/**
 * @brief 设置轮询任务的频率
 * @param handle 轮询任务句柄
 * @param freq_hz 轮询频率(Hz)
*/
void e_polling_task_set_freq( e_polling_task_handle_t handle, uint32_t freq_hz );

#ifdef __cplusplus
}
#endif


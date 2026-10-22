#pragma once

#include <stdint.h>

//任务句柄
typedef struct e_sched_task_t* e_sched_task_handle_t;

//任务运行环境枚举
typedef enum e_sched_stack_env_e{
    E_SCHED_STACK_ENV_USER = 0,
    E_SCHED_STACK_ENV_SYSTEM = 1,
    E_SCHED_STACK_ENV_INTERRUPT = 2,
}e_sched_stack_env_e;
typedef uint8_t e_sched_stack_env_t;

//任务属性枚举
typedef enum e_sched_task_attr_e{
    //优先级0-7，占属性位置3位，从第0位开始
    E_SCHED_TASK_ATTR_PRO_0 = 0 << 0,              //优先级0 最低
    E_SCHED_TASK_ATTR_PRO_1 = 1 << 0,              //优先级1
    E_SCHED_TASK_ATTR_PRO_2 = 2 << 0,              //优先级2
    E_SCHED_TASK_ATTR_PRO_3 = 3 << 0,              //优先级3
    E_SCHED_TASK_ATTR_PRO_4 = 4 << 0,              //优先级4
    E_SCHED_TASK_ATTR_PRO_5 = 5 << 0,              //优先级5
    E_SCHED_TASK_ATTR_PRO_6 = 6 << 0,              //优先级6
    E_SCHED_TASK_ATTR_PRO_7 = 7 << 0,              //优先级7 最高

    //最高权限线程属性，占第3位
    E_SCHED_TASK_ATTR_ADMIN_THREAD = 1 << 3,

    //独立初始化流程的线程 , 创建后立刻运行 ， 不受调度器状态影响，占第4位
    E_SCHED_TASK_ATTR_INDEPEND_INIT_THREAD = 1 << 4,

    //默认属性
    E_SCHED_TASK_ATTR_DEFAULT = E_SCHED_TASK_ATTR_PRO_3,
}e_sched_task_attr_e;
typedef uint32_t e_sched_task_attr_t;

//任务描述结构体
typedef struct e_sched_task_desc_t{
    const char* name;
    void (*task_entry)(void* arg);
    void* arg;
    e_sched_task_attr_t attr;   //使用e_sched_task_attr_e定义的属性
    uint32_t stack_size_byte;   //栈大小，单位字节，0表示使用属性中定义的栈大小
}e_sched_task_desc_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动调度器
*/
void e_sched_start(void);

/**
 * @brief 创建一个任务
 * @param desc 任务描述
 * @return 任务句柄
*/
e_sched_task_handle_t e_sched_task_create(const e_sched_task_desc_t* desc);

/**
 * @brief 删除一个任务
 * @param task 任务句柄
*/
void e_sched_task_delete(e_sched_task_handle_t task);

/**
 * @brief 启动一个任务
 * @param task 任务句柄
 */
void e_sched_task_resume(e_sched_task_handle_t task);

/**
 * @brief 暂停一个任务
 * @param task 任务句柄
 */
void e_sched_task_suspend(e_sched_task_handle_t task);

/**
 * @brief 任务延时，单位毫秒
 * @param ms 延时毫秒数
*/
void e_sched_task_delay_ms(uint32_t ms);

/**
 * @brief 获取系统运行时间，单位毫秒
 * @return 系统运行时间，单位毫秒
*/
uint32_t e_sched_time_ms(void);

/**
 * @brief 判断当前是否在中断上下文中运行
 * @return 1:在中断上下文中 0:不在中断上下文中
*/
uint8_t e_sched_current_is_interrupt_context(void);

/**
 * @brief 获取当前运行环境
 * @return 任务运行环境
*/
e_sched_stack_env_t e_sched_get_current_stack_env(void);

/**
 * @brief 判断调度器是否已经启动
 * @return 1:已启动 0:未启动
*/
uint8_t e_sched_is_started(void);

/**
 * @brief 让出当前任务的剩余时间片
 * @return 1:成功 0:失败
*/
void e_sched_yeild(void);

#ifdef __cplusplus
}
#endif


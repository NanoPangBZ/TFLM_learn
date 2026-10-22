#pragma once

#include <stdint.h>

typedef int16_t (*arch_log_out_t)(uint8_t* data , uint16_t len);

#ifdef __cplusplus
extern "C"
{
#endif  //__cplusplus

/**
 * @brief cpu平台架构初始化
 * @return 0:成功 其它:失败
 */
int arch_init(void);

/**
 * @brief 获取日志输出函数
 * @return 日志输出函数
 */
arch_log_out_t arch_log_out_init(void);

/**
 * @brief 平台主循环
 * @note SDL 后端等价于参考工程 FreeRTOS 后端的 vTaskStartScheduler()：
 *       框架任务在后台线程运行，本函数在调用线程泵事件保持窗口响应，
 *       直到收到退出事件（SDL_EVENT_QUIT）后返回。
 */
void arch_run(void);

#ifdef __cplusplus
}
#endif  //__cplusplus

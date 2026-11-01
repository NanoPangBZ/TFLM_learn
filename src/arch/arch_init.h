#pragma once

#include <stdint.h>

/******************************************************arch 说明***********************************************************************/
/* arch 层代表"板卡"：统一创建 SDL 主窗口与渲染器（见 arch_cfg.h）。
 * bsp_display 等外设只占用窗口中的一块区域，不再各自创建窗口。 */

typedef int16_t (*arch_log_out_t)(uint8_t* data , uint16_t len);

#ifdef __cplusplus
extern "C"
{
#endif  //__cplusplus

/**
 * @brief cpu平台架构初始化（初始化 SDL 并创建主窗口）
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
 *       框架任务在后台线程运行，本函数在调用线程泵事件并按需渲染，
 *       直到主窗口收到关闭请求（SDL_EVENT_QUIT / 主窗口 CLOSE_REQUESTED）后返回。
 */
void arch_run(void);

#ifdef __cplusplus
}
#endif  //__cplusplus

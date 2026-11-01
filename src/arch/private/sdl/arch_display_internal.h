#pragma once

/* arch 内部接口：主窗口生命周期与事件泵，仅供 arch_init.c 使用。 */

/**
 * @brief 创建主窗口、渲染器并注册帧事件（要求 SDL 已初始化）
 * @return 0:成功 其它:失败
 */
int arch_display_init(void);

/**
 * @brief 泵一次事件并按需渲染一帧（主线程）
 * @return 1:收到退出请求 0:继续运行
 */
int arch_display_pump(void);

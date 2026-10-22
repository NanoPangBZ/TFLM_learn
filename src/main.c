/* TFLM_learn framework — SDL3 后端统一入口。
 * 参考 YouWei Embedded Platform 的 src/main.c 角色：初始化后进入框架，
 * 框架按层级调用各模块注册的初始化函数并启动调度器（SDL 线程）。
 * 与 FreeRTOS 后端的 vTaskStartScheduler() 永不返回不同，SDL 后端下
 * enter_framework() 返回后由本入口保持进程存活。 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdint.h>

#include "framework_cfg.h"
#include "framework/e_framework_core.h"

/* 启动阶段日志输出：写到标准输出 */
static int16_t log_out(uint8_t* data, uint16_t len)
{
    fwrite(data, 1, len, stdout);
    fflush(stdout);
    return (int16_t)len;
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    /* 框架只使用 SDL 线程/互斥/信号量/定时，无需视频子系统；
     * 调用 SDL_Init(0) 以规范化底层状态。 */
    SDL_Init(0);

    /* 进入框架：堆初始化 -> 各层级模块初始化 -> 启动调度器（SDL 线程） */
    enter_framework(log_out);

    /* 框架任务已在后台线程运行；本线程保持存活。
     * 后续可在 SDL 事件循环中接入窗口/画板等外设模拟。 */
    for (;;) {
        SDL_Delay(1000);
    }

    return 0;
}

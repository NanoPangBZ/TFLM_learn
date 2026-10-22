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

    /* 初始化视频（bsp_display 独立窗口）+ 事件（窗口关闭等）子系统 */
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    /* 进入框架：堆初始化 -> 各层级模块初始化 -> 启动调度器（SDL 线程） */
    enter_framework(log_out);

    /* 框架任务在后台线程运行；本线程泵事件保持窗口响应，
     * 窗口关闭（SDL_EVENT_QUIT）时退出框架并结束进程。 */
    for (;;) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                exit_framework();
                return 0;
            }
        }
        SDL_Delay(16);
    }

    return 0;
}

/* arch_init.c — arch 层的 SDL3 后端实现。
 *
 * 对应参考工程 src/arch/private/N32H7xx/arch_init.c 的角色：把平台（此处为
 * SDL3 模拟的"单片机"）的低层初始化与运行细节收敛到本层，main 不直接接触 SDL。
 *   - arch_init()        初始化 SDL 视频/事件子系统；
 *   - arch_log_out_init()返回日志输出通道（标准输出）；
 *   - arch_run()         主循环，泵事件直到收到退出事件。 */
#include "arch/arch_init.h"

#include <SDL3/SDL.h>
#include <stdio.h>

/* SDL3 后端日志输出通道：标准输出 */
static int16_t arch_console_log_out(uint8_t* data, uint16_t len)
{
    fwrite(data, 1, len, stdout);
    fflush(stdout);
    return (int16_t)len;
}

int arch_init(void)
{
    /* 视频：bsp_display 独立窗口；事件：窗口关闭等 */
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        return -1;
    }
    return 0;
}

arch_log_out_t arch_log_out_init(void)
{
    return arch_console_log_out;
}

void arch_run(void)
{
    for (;;) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                return;
            }
        }
        SDL_Delay(16);
    }
}

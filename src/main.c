/* TFLM_learn framework — 统一入口。
 * 参考 YouWei Embedded Platform 的 src/main.c 角色：平台初始化 -> 进入框架 -> 主循环。
 * 平台（SDL3）细节全部隐藏于 arch 层（arch_init / arch_log_out_init / arch_run），
 * 本入口不直接接触 SDL。 */
#include "arch/arch_init.h"
#include "framework/e_framework_core.h"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    /* 平台初始化（SDL 子系统，细节隐藏于 arch 层） */
    arch_init();

    /* 进入框架：堆初始化 -> 各层级模块初始化 -> 启动调度器（SDL 线程） */
    enter_framework(arch_log_out_init());

    /* 平台主循环（泵事件保持窗口响应，直到退出） */
    arch_run();

    /* 退出框架 */
    exit_framework();
    return 0;
}

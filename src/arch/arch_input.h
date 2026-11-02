#pragma once

#include <stdint.h>

/* arch 输入状态：由 arch 主循环统一从 SDL 事件更新，
 * 外设轮询任务只读取这里的快照，不直接注册 SDL 事件回调。 */
typedef struct arch_input_state_t {
    int32_t mouse_x;        // 窗口渲染像素坐标，-1 表示未知
    int32_t mouse_y;
    uint8_t mouse_left;     // 鼠标左键是否按下
} arch_input_state_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取当前输入快照（线程安全）
 * @param state 输出状态
 */
void arch_input_get(arch_input_state_t* state);

/**
 * @brief 查看累积的滚轮增量（不清零）
 * @return 正数=向上滚，负数=向下滚
 */
int32_t arch_input_peek_wheel(void);

/**
 * @brief 取走并清零累积的滚轮增量
 * @return 正数=向上滚，负数=向下滚
 * @note 命中控件后再 take；多消费者用 peek 判断，谁用谁清。
 */
int32_t arch_input_take_wheel(void);

#ifdef __cplusplus
}
#endif

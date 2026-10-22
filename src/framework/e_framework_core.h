#pragma once

#include <stdint.h>

// SysTick函数注册项结构体
typedef struct systick_function_item_t{
    const char* name;
    void (*function)(void);
    uint32_t reserved:32; // Reserved for future use, ensuring 32-bit alignment
}systick_function_item_t;

/**
 * @brief 运行时注册一个SysTick函数项（由 DEFINE_SYSTICK_FUNCTION_ITEM 生成的构造函数调用）
 * @param item SysTick函数项指针
 */
void c_systick_regedit_add(const systick_function_item_t* item);

/**
 * @brief 定义Systick函数注册项的宏
 * @param _name 函数名称
 * @param _func 函数指针
 */
#define DEFINE_SYSTICK_FUNCTION_ITEM( _func ) \
    static const systick_function_item_t __function_item_##_func = { \
        .name = #_func, \
        .function = _func, \
        .reserved = 0 \
    }; \
    __attribute__(( constructor )) static void __function_item_##_func##_register(void) { \
        c_systick_regedit_add( &__function_item_##_func ); \
    }

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 进入框架核心函数
 * @param set_up_log_out 启动阶段日志输出函数指针,NULL表示不输出日志
*/
void enter_framework( int16_t (*set_up_log_out)(uint8_t* data , uint16_t len) );

/**
 * @brief 退出框架函数，执行框架和各个层级的反初始化函数
*/
void exit_framework(void);

/**
 * @brief 获取硬件版本信息
 * @param major 输出主版本号指针
 * @param minor 输出次版本号指针
 * @param patch 输出补丁版本号指针
*/
void framework_get_hd_version( uint8_t* major , uint8_t* minor , uint8_t* patch );

/**
 * @brief 获取CPU ID
 * @param cpu_id_len: 指向CPU ID长度的指针
 * @return 指向CPU ID的指针
*/
const uint8_t* framework_get_cpu_id( uint32_t* cpu_id_len );

/**
 * @brief 获取运行时ELF参数
 * @return ELF参数指针
*/
void* framework_get_run_elf_param(void);

/**
 * @brief 框架SysTick中断处理函数
 * @note 该函数应在SysTick中断服务函数中调用
*/
void framework_systick_handler(void);

#ifdef __cplusplus
}
#endif


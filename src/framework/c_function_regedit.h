
#pragma once

/**
 * @brief 函数表注册机制
 * @note 该机制允许在编译期将函数注册到函数表特定的组中，便于在运行时按优先级调用这些表中对应分组的函数。
 */

/**
 * @example
 * // 定义一个初始化函数
 * int my_init_function(void) {
 *     // 初始化代码
 *    return 0;
 * }
 * // 注册该函数到框架初始化函数组，优先级1
 * REGEDIT_FRAMEWORK_INIT_FUNCTION(my_init_function, 1);
 * 
 * //定义另一个初始化函数
 * int another_init_function(void) {
 *    // 其他初始化代码
 *   return 0;
 * }
 * // 注册该函数到框架初始化函数组，优先级0（最高）
 * REGEDIT_FRAMEWORK_INIT_FUNCTION(another_init_function, 0);
 * 
 * // 在系统初始化时调用该组的所有函数
 * void call_all_init_functions(void) {
 *     call_function_group(FUNC_GROUP_FRAMEWORK_INIT);
 * }
 */

#include <stdint.h>

typedef enum function_group_e {
    FUNC_GROUP_FRAMEWORK_INIT = 0,  // 框架初始化函数组
    FUNC_GROUP_BSP_INIT,            // BSP初始化函数组
    FUNC_GROUP_AFTER_BSP_INIT,      // BSP初始化完成后需要调用的函数组
    FUNC_GROUP_SERVICE_INIT,        // Service初始化函数组
    FUNC_GROUP_AFTER_SERVICE_INIT,  // Service初始化完成后需要调用的函数组
    FUNC_GROUP_APP_INIT,            // App初始化函数组
    FUNC_GROUP_AFTER_APP_INIT,      // App初始化完成后需要调用的函数组

    FUNC_GROUP_FRAMEWORK_DEINIT,          // 框架反初始化函数组
    FUNC_GROUP_BSP_DEINIT,                // BSP反初始化函数组
    FUNC_GROUP_SERVICE_DEINIT,            // Service反初始化函数组
    FUNC_GROUP_APP_DEINIT,                // App反初始化函数组

    FUNC_GROUP_BEFORE_SHUTDOWN,         // 系统关闭前调用的函数组
} function_group_e;
typedef uint32_t function_group_t;

#pragma pack(1)
typedef struct function_item_t{
    function_group_t group;
    const char* name;
    int (*function)(void);
    uint32_t priority:2;    // 优先级，0-3，0最高
    uint32_t reserved:30; // Reserved for future use, ensuring 32-bit alignment
}function_item_t;
#pragma pack()

/**
 * @brief 运行时注册一个函数项（由 DEFINE_FUNCTION_ITEM 生成的构造函数调用）
 * @param item 函数项指针
 * @note PC/SDL 后端使用 GCC 构造函数 + 静态注册表等价实现嵌入式链接段注册，
 *       规避 MinGW PE 下段名截断/段对齐导致的段迭代错位问题。
 */
void c_function_regedit_add(const function_item_t* item);

/**
 * @brief 定义函数注册项
 * @param _group 函数组
 * @param _name 函数名称
 * @param _func 函数指针
 * @param _pri 函数优先级，0-3，0最高
 */
#define DEFINE_FUNCTION_ITEM( _group , _name , _func , _pri ) \
    static const function_item_t __function_item_##_name = { \
        .group = _group, \
        .name = #_name, \
        .function = _func, \
        .priority = _pri, \
        .reserved = 0 \
    }; \
    __attribute__(( constructor )) static void __function_item_##_name##_register(void) { \
        c_function_regedit_add( &__function_item_##_name ); \
    }

/**
 * @brief 注册框架初始化函数
 * @param func 要注册的函数
 * @param pri 函数优先级，0-3，0最高
 */
#define REGEDIT_FRAMEWORK_INIT_FUNCTION(func, pri)   DEFINE_FUNCTION_ITEM(FUNC_GROUP_FRAMEWORK_INIT, func, func, pri)

/**
 * @brief 注册BSP初始化函数
 * @param func 要注册的函数
 * @param pri 函数优先级，0-3，0最高
 */
#define REGEDIT_BSP_INIT_FUNCTION(func, pri)   DEFINE_FUNCTION_ITEM(FUNC_GROUP_BSP_INIT, func, func, pri)

/**
 * @brief 注册BSP初始化完成后需要调用的函数
 * @param func 要注册的函数
 * @param pri 函数优先级，0-3，0最高
 */
#define REGEDIT_AFTER_BSP_INIT_FUNCTION(func, pri)   DEFINE_FUNCTION_ITEM(FUNC_GROUP_AFTER_BSP_INIT, func, func, pri)

/**
 * @brief 注册service初始化函数
 * @param func 要注册的函数
 * @param pri 函数优先级，0-3，0最高
 */
#define REGEDIT_SERVICE_INIT_FUNCTION(func, pri)   DEFINE_FUNCTION_ITEM(FUNC_GROUP_SERVICE_INIT, func, func, pri)

/**
 * @brief 注册service初始化完成后需要调用的函数
 * @param func 要注册的函数
 * @param pri 函数优先级，0-3，0最高
 */
#define REGEDIT_AFTER_SERVICE_INIT_FUNCTION(func, pri)   DEFINE_FUNCTION_ITEM(FUNC_GROUP_AFTER_SERVICE_INIT, func, func, pri)

/**
 * @brief 注册app初始化函数
 * @param func 要注册的函数
 * @param pri 函数优先级，0-3，0最高
 */
#define REGEDIT_APP_INIT_FUNCTION(func, pri)   DEFINE_FUNCTION_ITEM(FUNC_GROUP_APP_INIT, func, func, pri)

/**
 * @brief 注册app初始化完成后需要调用的函数
 * @param func 要注册的函数
 * @param pri 函数优先级，0-3，0最高
 */
#define REGEDIT_AFTER_APP_INIT_FUNCTION(func, pri)   DEFINE_FUNCTION_ITEM(FUNC_GROUP_AFTER_APP_INIT, func, func, 3 - pri)

/**
 * @brief 注册框架反初始化函数
 * @param func 要注册的函数
 * @param pri 与对应初始化函数的优先级保持一致
 */
#define REGEDIT_FRAMEWORK_DEINIT_FUNCTION(func, pri)   DEFINE_FUNCTION_ITEM(FUNC_GROUP_FRAMEWORK_DEINIT, func, func,  3 - pri)

/**
 * @brief 注册BSP反初始化函数
 * @param func 要注册的函数
 * @param pri 与对应初始化函数的优先级保持一致
 */
#define REGEDIT_BSP_DEINIT_FUNCTION(func, pri)   DEFINE_FUNCTION_ITEM(FUNC_GROUP_BSP_DEINIT, func, func,  3 - pri)

/**
 * @brief 注册Service反初始化函数
 * @param func 要注册的函数
 * @param pri 与对应初始化函数的优先级保持一致
 */
#define REGEDIT_SERVICE_DEINIT_FUNCTION(func, pri)   DEFINE_FUNCTION_ITEM(FUNC_GROUP_SERVICE_DEINIT, func, func, 3 - pri)

/**
 * @brief 注册App反初始化函数
 * @param func 要注册的函数
 * @param pri 与对应初始化函数的优先级保持一致
 */
#define REGEDIT_APP_DEINIT_FUNCTION(func, pri)   DEFINE_FUNCTION_ITEM(FUNC_GROUP_APP_DEINIT, func, func,  3 - pri)

/**
 * @brief 注册系统关闭前需要调用的函数
 * @param func 要注册的函数
 * @param pri 函数优先级，0-3，0最高
 */
#define REGEDIT_BEFORE_SHUTDOWN_FUNCTION(func, pri)   DEFINE_FUNCTION_ITEM(FUNC_GROUP_BEFORE_SHUTDOWN, func, func, pri)

/**
 * @brief 调用指定函数组中的所有函数
 * @param group 函数组
 * @return 0:成功 其他:失败
 * @note 按优先级从高到低调用组中的函数
 */
int call_function_group(function_group_t group);

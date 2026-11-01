#include "framework/e_framework_core.h"
#include "framework/e_heap.h"
#include "framework/e_sched.h"
#include "framework/c_function_regedit.h"
#include "framework/e_log.h"
#include "framework_cfg.h"
#include "version.h"
#include "git_info.h"
#include <stdio.h>
#include <string.h>

#define E_FRAMEWORK_ASSERT(x)    do { if (!(x)) while (1); } while (0)

#define TAG "framework"
#define INFO_LOG(...)   E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

/* SysTick 函数注册表：PC/SDL 后端用 GCC 构造函数 + 静态数组等价实现链接段，
 * 与 e_framework_core.h 的 DEFINE_SYSTICK_FUNCTION_ITEM 宏对应。 */
#define C_SYSTICK_REGEDIT_MAX_ITEMS   64

static const systick_function_item_t* s_systick_items[C_SYSTICK_REGEDIT_MAX_ITEMS];
static uint32_t s_systick_count = 0;

void c_systick_regedit_add(const systick_function_item_t* item)
{
    if (item != NULL && s_systick_count < C_SYSTICK_REGEDIT_MAX_ITEMS) {
        s_systick_items[s_systick_count++] = item;
    }
}

static int16_t (*user_set_up_log_out)(uint8_t* data, uint16_t len) = NULL;

/* 启动阶段日志输出适配：把裸的 (data,len) 回调适配为 e_log 通道签名 (ctx,data,len) */
static int16_t set_up_log_out_function(void* ctx, uint8_t* data, uint16_t len)
{
    typedef int16_t (*bare_log_out_t)(uint8_t*, uint16_t);
    int16_t (*fn)(uint8_t* data, uint16_t len) = (bare_log_out_t)ctx;
    return fn(data, len);
}

/* SDL 后端默认日志输出通道：标准输出 */
static int16_t console_log_out(void* ctx, uint8_t* data, uint16_t len)
{
    (void)ctx;
    fwrite(data, 1, len, stdout);
    fflush(stdout);
    return (int16_t)len;
}

#if defined(E_LOG_ENABLE) && (E_LOG_ENABLE == 1)

static void log_out_channel_register(void)
{
    /* 无 UART：正式渠道统一到控制台 */
    e_log_set_level(E_LOG_DEFAULT_LEVEL);
    e_log_set_out_channel(console_log_out, NULL);
    e_log_set_focus_tag(E_LOG_FOCUSED_TAG);
    e_log_set_time_source(e_sched_time_ms);
}

static int log_out_channel_disregister(void)
{
    e_log_set_out_channel(NULL, NULL);
    return 0;
}

#endif

void enter_framework(int16_t (*set_up_log_out)(uint8_t* data, uint16_t len))
{
#if defined(E_LOG_ENABLE) && (E_LOG_ENABLE == 1)
    user_set_up_log_out = set_up_log_out;

    e_log_set_level(E_LOG_INIT_STEP_LEVEL);
    e_log_set_time_source(e_sched_time_ms);
    if (user_set_up_log_out) {
        e_log_set_out_channel(set_up_log_out_function, user_set_up_log_out);
    } else {
        e_log_set_out_channel(console_log_out, NULL);
    }
#endif

    /* 初始化堆内存模块 */
    e_heap_init();

    /* 按层级和优先级调用各模块初始化函数 */
    E_FRAMEWORK_ASSERT(call_function_group(FUNC_GROUP_FRAMEWORK_INIT) == 0);
    E_FRAMEWORK_ASSERT(call_function_group(FUNC_GROUP_BSP_INIT) == 0);
    E_FRAMEWORK_ASSERT(call_function_group(FUNC_GROUP_AFTER_BSP_INIT) == 0);
    E_FRAMEWORK_ASSERT(call_function_group(FUNC_GROUP_SERVICE_INIT) == 0);
    E_FRAMEWORK_ASSERT(call_function_group(FUNC_GROUP_AFTER_SERVICE_INIT) == 0);
    E_FRAMEWORK_ASSERT(call_function_group(FUNC_GROUP_APP_INIT) == 0);
    E_FRAMEWORK_ASSERT(call_function_group(FUNC_GROUP_AFTER_APP_INIT) == 0);

#if defined(E_LOG_ENABLE) && (E_LOG_ENABLE == 1)
    log_out_channel_register();
#endif

    INFO_LOG("Framework initialized successfully!");
    INFO_LOG("Version: V%d.%d.%d - %s", MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION, VERSION_IS_RELEASE ? "Release" : "Debug");
    INFO_LOG("Board: %s", BOARD_NAME);
    INFO_LOG("Git Branch: %s", GIT_BRANCH_NAME);
    INFO_LOG("Git Commit: %s", GIT_COMMIT_NAME);

    /* 启动调度器（释放所有等待任务，进入运行态） */
    e_sched_start();
}

void exit_framework(void)
{
#if defined(E_LOG_ENABLE) && (E_LOG_ENABLE == 1)
    log_out_channel_disregister();
    if (user_set_up_log_out) {
        e_log_set_level(E_LOG_INIT_STEP_LEVEL);
        e_log_set_time_source(e_sched_time_ms);
        e_log_set_out_channel(set_up_log_out_function, user_set_up_log_out);
    }
#endif

    call_function_group(FUNC_GROUP_APP_DEINIT);
    call_function_group(FUNC_GROUP_SERVICE_DEINIT);
    call_function_group(FUNC_GROUP_BSP_DEINIT);
    call_function_group(FUNC_GROUP_FRAMEWORK_DEINIT);
}

void framework_get_hd_version(uint8_t* major, uint8_t* minor, uint8_t* patch)
{
    if (major) *major = (uint8_t)MAJOR_VERSION;
    if (minor) *minor = (uint8_t)MINOR_VERSION;
    if (patch) *patch = (uint8_t)PATCH_VERSION;
}

const uint8_t* framework_get_cpu_id(uint32_t* cpu_id_len)
{
    static const uint8_t cpu_id[16] = {
        'S', 'D', 'L', '3', '-', 'S', 'I', 'M', 'U', 'L', 'A', 'T', 'O', 'R', 0, 0
    };
    if (cpu_id_len) {
        *cpu_id_len = 16;
    }
    return cpu_id;
}

void* framework_get_run_elf_param(void)
{
    /* PC 上无 ELF 运行参数 */
    return NULL;
}

void framework_systick_handler(void)
{
    if (!e_sched_is_started()) {
        return;
    }

    for (uint32_t i = 0; i < s_systick_count; i++) {
        const systick_function_item_t* item = s_systick_items[i];
        if (item->function) {
            item->function();
        }
    }
}

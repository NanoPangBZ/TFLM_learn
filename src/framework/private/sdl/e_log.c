#include "framework/e_log.h"
#include "framework/c_function_regedit.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct e_log_ctx_t{
    void* out_channel_func_ctx;
    e_log_out_channel_func_t out_channel_func;
    e_log_time_source_func_t time_source_func;
    uint8_t enabled;
    e_log_level_t level;
    uint8_t focus_tag_enabled:1;
    uint8_t reserved:7;
    const char* focus_tag;
}e_log_ctx_t;

const static char* level_lables[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR"
};

static e_log_ctx_t g_e_log_ctx = {
    .out_channel_func_ctx = NULL,
    .out_channel_func = NULL,
    .time_source_func = NULL,
    .enabled = 1,
    .level = E_LOG_LEVEL_DEBUG,
};

void e_log_set_out_channel(e_log_out_channel_func_t out_channel_func,void* ctx)
{
    g_e_log_ctx.out_channel_func_ctx = ctx;
    g_e_log_ctx.out_channel_func = out_channel_func;
}

void e_log_set_time_source(e_log_time_source_func_t time_source_func)
{
    g_e_log_ctx.time_source_func = time_source_func;
}

void e_log_enable(uint8_t enable)
{
    g_e_log_ctx.enabled = enable;
}

void e_log_set_level(e_log_level_t level)
{
    g_e_log_ctx.level = level;
}

void e_log_printf(e_log_level_t level, const char* tag ,const char* fmt, ...)
{
    if( !g_e_log_ctx.enabled )
        return;
    
    if( g_e_log_ctx.out_channel_func == NULL )
        return;

    if( level < g_e_log_ctx.level )
        return;

    // 添加数组边界检查
    if( level >= sizeof(level_lables)/sizeof(level_lables[0]) )
        return;

    // 如果启用了聚焦标签，检查标签是否匹配
    if( g_e_log_ctx.focus_tag_enabled )
    {
        if( tag == NULL || strcmp(tag, g_e_log_ctx.focus_tag) != 0 )
            return;
    }

    char log_buffer[256];
    char msg_buffer[128];
    va_list args;
    
    // 检查并处理 NULL 参数
    if(tag == NULL) {
        tag = "unknown";
    }
    if(fmt == NULL) {
        fmt = "unknown";
    }
    
    // 格式化用户消息 - 使用安全的格式化函数
    va_start(args, fmt);
    int ret = vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);
    
    // 确保字符串以null结尾
    if(ret >= sizeof(msg_buffer)) {
        msg_buffer[sizeof(msg_buffer) - 1] = '\0';
    }
    
    // 构建完整的日志消息 - 使用安全的格式化函数
    if(g_e_log_ctx.time_source_func != NULL) {
        uint32_t timestamp = g_e_log_ctx.time_source_func();
        ret = snprintf(log_buffer, sizeof(log_buffer), "[%u] [%s] [%s]: %s\r\n",
                timestamp, level_lables[level], tag, msg_buffer);
    } else {
        ret = snprintf(log_buffer, sizeof(log_buffer), "[unknown] [%s] [%s]: %s\r\n",
                level_lables[level], tag, msg_buffer);
    }
    
    // 确保字符串以null结尾
    if(ret >= sizeof(log_buffer)) {
        log_buffer[sizeof(log_buffer) - 1] = '\0';
    }
    
    // 输出日志
    g_e_log_ctx.out_channel_func( g_e_log_ctx.out_channel_func_ctx, (uint8_t*)log_buffer, strlen(log_buffer));
}

void e_log_set_focus_tag(const char* tag)
{
    if( tag != NULL )
    {
        g_e_log_ctx.focus_tag = tag;
        g_e_log_ctx.focus_tag_enabled = 1;
    }
    else
    {
        g_e_log_ctx.focus_tag = NULL;
        g_e_log_ctx.focus_tag_enabled = 0;
    }
}

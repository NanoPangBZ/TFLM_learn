#include "bsp/bsp_display.h"
#include "framework/c_function_regedit.h"
#include "framework/e_heap.h"
#include "framework/e_polling_pool.h"
#include "framework/e_sched.h"
#include <stddef.h>

#include "framework/e_log.h"
#define TAG "bsp_display_test"
#define DEBUG_LOG(...)  //E_DEBUG_LOG(TAG,__VA_ARGS__)
#define INFO_LOG(...)   E_INFO_LOG(TAG,__VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG,__VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG,__VA_ARGS__)

//测试目标
const bsp_display_desc_t test_display_desc = {
    .type = BSP_DISPLAY_TYPE_USER,
    .idx  = 0,
    .attr = BSP_DISPLAY_ATTR_DEFAULT,
};

static void test_polling_task(void* args)
{
    static uint32_t test_color = 0xFF0000;
    static uint32_t last_update_time = 0;

    uint32_t now = e_sched_time_ms();

    bsp_display_handle_t display_handle = (bsp_display_handle_t)args;

    //判断flush是否完成
    if( bsp_display_is_busy(display_handle) )
        return;
    
    //判断是否到达更新时间
    if( now - last_update_time < 1000 )
        return;
    
    last_update_time = now;

    bsp_display_fill(display_handle, test_color);

    //切换颜色
    test_color >>= 8;
    if( test_color == 0 )
        test_color = 0xFF0000;
}

static int test_init(void)
{
    bsp_display_handle_t display_handle = bsp_display_open((bsp_display_desc_t*)&test_display_desc);
    if( display_handle == NULL )
    {
        ERROR_LOG("display test open display failed!");
        return -1;
    }

    bsp_display_on(display_handle, 1);
    bsp_display_set_brightness(display_handle, 100);

    uint16_t width = 0, height = 0;
    bsp_display_get_size(display_handle, &width, &height);

    uint8_t* draw_buff = (uint8_t*)e_heap_malloc( width * height * 2 , E_HEAP_ATTR_BIG_BLOCKS );
    if( draw_buff == NULL )
    {
        ERROR_LOG("display test malloc draw buffer failed!");
        return -1;
    }
    bsp_display_set_draw_buffer(display_handle, draw_buff, width * height * 2 );

    e_polling_task_desc_t polling_desc = {
        .attr = E_POLLING_TASK_ATTR_DEFAULT,
        .freq_hz = 100,
        .name = "bsp_display_test_polling",
        .polling_func = test_polling_task,
        .start_before_create = 1,
        .user_ctx = (void*)display_handle,
    };
    if( e_polling_task_create( &polling_desc ) == NULL )
    {
        ERROR_LOG("display test create polling task failed!");
        return -1;
    }

    return 0;
}
REGEDIT_AFTER_APP_INIT_FUNCTION(test_init,3);


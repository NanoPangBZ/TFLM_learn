#include "framework/c_function_regedit.h"
#include "framework/e_polling_pool.h"
#include "bsp/bsp_touch.h"
#include <stddef.h>

#include "framework/e_log.h"
#define TAG "bsp_touch_test"
#define DEBUG_LOG(...)  E_DEBUG_LOG(TAG,__VA_ARGS__)
#define INFO_LOG(...)   E_INFO_LOG(TAG,__VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG,__VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG,__VA_ARGS__)

static void bsp_polling_task(void* args)
{
    bsp_touch_handle_t touch = (bsp_touch_handle_t)args;

    if( !touch )
        return;

    bsp_touch_point_t point;
    if( bsp_touch_count(touch) > 0U && bsp_touch_get_touch_point(touch, 0U, &point) == 0U )
    {
        INFO_LOG("Touch point: x=%d, y=%d", point.x, point.y);
    }
}

static int test_init(void)
{
    bsp_touch_desc_t desc = {
        .type = BSP_TOUCH_TYPE_USER,
        .idx = 0,
        .attr = BSP_TOUCH_ATTR_DEFAULT,
    };
    bsp_touch_handle_t touch = bsp_touch_open(&desc);
    if( !touch )
    {
        ERROR_LOG("Failed to open touch device");
        return -1;
    }

    e_polling_task_desc_t polling_task_desc = {
        .attr = E_POLLING_TASK_ATTR_DEFAULT,
        .freq_hz = 10,
        .name = "bsp_touch_test_polling_task",
        .polling_func = bsp_polling_task,
        .start_before_create = 1,
        .user_ctx = touch,
    };
    if( e_polling_task_create(&polling_task_desc) == NULL )
    {
        ERROR_LOG("Failed to create polling task");
        bsp_touch_close(touch);
        return -1;
    }

    return 0;
}
REGEDIT_AFTER_APP_INIT_FUNCTION( test_init, 0 );


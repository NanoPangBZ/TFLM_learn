#include "bsp/bsp_led.h"
#include "framework/c_function_regedit.h"
#include "framework/e_polling_pool.h"

#include "framework/e_log.h"
#define TAG "bsp_led_flow_test"
#define INFO_LOG(...)  E_INFO_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...) E_ERROR_LOG(TAG, __VA_ARGS__)

#define BSP_LED_FLOW_TEST_START_IDX  0u
#define BSP_LED_FLOW_TEST_END_IDX    6u
#define BSP_LED_FLOW_CYCLE_MS        200u

static void bsp_led_flow_task(void* args)
{
    static uint32_t curr_idx = BSP_LED_FLOW_TEST_START_IDX;
    uint32_t i;
    (void)args;

    for (i = BSP_LED_FLOW_TEST_START_IDX; i <= BSP_LED_FLOW_TEST_END_IDX; i++) {
        bsp_led_set_value((bsp_led_idx_t)i, 0u);
    }

    bsp_led_set_value((bsp_led_idx_t)curr_idx, 1u);
    curr_idx++;
    if (curr_idx > BSP_LED_FLOW_TEST_END_IDX) {
        curr_idx = BSP_LED_FLOW_TEST_START_IDX;
    }

    INFO_LOG("curr_idx=%u", (unsigned)curr_idx);
}

static int bsp_led_flow_test_init(void)
{
    e_polling_task_desc_t desc = {
        .attr = E_POLLING_TASK_ATTR_DEFAULT,
        .freq_hz = 1000u / BSP_LED_FLOW_CYCLE_MS,
        .name = TAG,
        .polling_func = bsp_led_flow_task,
        .start_before_create = 1,
        .user_ctx = NULL,
    };

    if (e_polling_task_create(&desc) == NULL) {
        ERROR_LOG("create polling task failed");
        return -1;
    }

    INFO_LOG("flow LED %u..%u every %ums",
             (unsigned)BSP_LED_FLOW_TEST_START_IDX,
             (unsigned)BSP_LED_FLOW_TEST_END_IDX,
             (unsigned)BSP_LED_FLOW_CYCLE_MS);
    return 0;
}

REGEDIT_AFTER_APP_INIT_FUNCTION(bsp_led_flow_test_init, 3);

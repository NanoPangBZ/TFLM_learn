#include "framework/c_function_regedit.h"
#include "bsp/bsp_encoder.h"
#include <string.h>

#define BSP_ENCODER_TEST_NAME "VOL"

#include "framework/e_log.h"
#define TAG "bsp_encoder_test"
#define DEBUG_LOG(...)   E_DEBUG_LOG(TAG, __VA_ARGS__)
#define INFO_LOG(...)    E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)    E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)   E_ERROR_LOG(TAG, __VA_ARGS__)

static void bsp_encoder_trigger_cb(bsp_encoder_handle_t handle, void* ctx, bsp_encoder_trigger_t* trigger)
{
    INFO_LOG("encoder trigger cb: inc_value=%d, position_value=%d, speed_rps_pulse=%.2f, btn_status=%d, timestamp=%d",
              trigger->inc_value,
              trigger->position_value,
              trigger->speed_rps_pulse,
              trigger->btn_status,
              trigger->timestamp);
}

static int test_init(void)
{
    bsp_encoder_handle_t encoder_handle = bsp_encoder_open(BSP_ENCODER_TEST_NAME);
    if( !encoder_handle )
    {
        ERROR_LOG("Failed to open encoder");
        return -1;
    }

    bsp_encoder_register_trigger_callback( encoder_handle , bsp_encoder_trigger_cb , NULL );

    INFO_LOG("bsp_encoder_test init success");

    return 0;
}
REGEDIT_AFTER_APP_INIT_FUNCTION(test_init, 0);

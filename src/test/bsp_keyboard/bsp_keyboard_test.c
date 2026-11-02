#include "framework/c_function_regedit.h"
#include "bsp/bsp_keyboard.h"

#include "framework/e_log.h"
#define TAG "bsp_keyboard_test"
#define INFO_LOG(...)  E_INFO_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...) E_ERROR_LOG(TAG, __VA_ARGS__)

static void on_keyboard(bsp_keyboard_handle_t handle, void* ctx,
                        uint32_t key_mask, uint32_t timestamp_ms)
{
    (void)handle;
    (void)ctx;
    INFO_LOG("key_mask=0x%08X ts=%u", key_mask, timestamp_ms);
}

static int bsp_keyboard_test_init(void)
{
    bsp_keyboard_desc_t desc = {
        .type = BSP_KEYBOARD_TYPE_VIRTUAL_TEST,
        .idx = 0,
        .attr = BSP_KEYBOARD_ATTR_DEFAULT,
    };
    bsp_keyboard_handle_t h = bsp_keyboard_open(&desc);
    if (!h) {
        ERROR_LOG("open keyboard failed");
        return -1;
    }
    bsp_keyboard_register_trigger_callback(h, on_keyboard, NULL);
    INFO_LOG("keyboard test ready (click 8x4 pad)");
    return 0;
}

REGEDIT_AFTER_APP_INIT_FUNCTION(bsp_keyboard_test_init, 2);

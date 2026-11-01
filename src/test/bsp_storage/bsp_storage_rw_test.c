#include "framework/c_function_regedit.h"
#include "bsp/bsp_storage.h"

#include <stdint.h>
#include <string.h>

#include "framework/e_log.h"
#define TAG "test_bsp_storage"
#define INFO_LOG(...)  E_INFO_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...) E_ERROR_LOG(TAG, __VA_ARGS__)

#define STORAGE_TEST_BLOCK_SIZE 4096U
#define STORAGE_TEST_COUNT      128U
#define STORAGE_COUNTER_ADDRESS 0U
#define STORAGE_COUNTER_MAGIC   0x53545257U

typedef struct storage_test_counter_t
{
    uint32_t magic;
    uint32_t count;
    uint32_t count_inverse;
} storage_test_counter_t;

/* Large test buffers are kept in RAM instead of consuming task stack space. */
static uint8_t g_write_buf[STORAGE_TEST_BLOCK_SIZE];
static uint8_t g_read_buf[STORAGE_TEST_BLOCK_SIZE];
static uint32_t g_test_count;

static int storage_test_load_counter(bsp_storage_handle_t handle)
{
    storage_test_counter_t counter;

    if (bsp_storage_read(handle, STORAGE_COUNTER_ADDRESS,
                         (uint8_t*)&counter, sizeof(counter)) != sizeof(counter))
    {
        return -1;
    }

    if ((counter.magic == STORAGE_COUNTER_MAGIC) &&
        (counter.count_inverse == ~counter.count))
    {
        g_test_count = counter.count;
    }
    else
    {
        g_test_count = 0U;
    }

    g_test_count = (g_test_count == UINT32_MAX) ? 1U : (g_test_count + 1U);
    return 0;
}

static int storage_test_save_counter(bsp_storage_handle_t handle)
{
    storage_test_counter_t counter = {
        .magic = STORAGE_COUNTER_MAGIC,
        .count = g_test_count,
        .count_inverse = ~g_test_count,
    };

    if (bsp_storage_erase(handle, STORAGE_COUNTER_ADDRESS,
                          STORAGE_TEST_BLOCK_SIZE) != STORAGE_TEST_BLOCK_SIZE)
    {
        return -1;
    }
    if (bsp_storage_write(handle, STORAGE_COUNTER_ADDRESS,
                          (const uint8_t*)&counter, sizeof(counter)) != sizeof(counter))
    {
        return -1;
    }
    return 0;
}

static uint32_t storage_test_random(uint32_t* state)
{
    uint32_t value = *state;

    /* xorshift32: small, deterministic PRNG suitable for address/pattern testing. */
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static void storage_test_fill_pattern(uint32_t address, uint32_t test_count, uint32_t random_seed)
{
    uint32_t i;
    uint32_t pattern_state = random_seed ^ address ^ (test_count * 0x9E3779B9U);

    if (pattern_state == 0U)
    {
        pattern_state = 0x6D2B79F5U;
    }

    for (i = 0U; i < STORAGE_TEST_BLOCK_SIZE; i++)
    {
        if ((i & 3U) == 0U)
        {
            pattern_state = storage_test_random(&pattern_state);
        }
        g_write_buf[i] = (uint8_t)(pattern_state >> ((i & 3U) * 8U));
    }
}

static int storage_test_address_used(const uint32_t* addresses, uint32_t count, uint32_t address)
{
    uint32_t i;

    for (i = 0U; i < count; i++)
    {
        if (addresses[i] == address)
        {
            return 1;
        }
    }
    return 0;
}

static int test_bsp_storage_open(void)
{
    bsp_storage_desc_t desc = {
        .type = BSP_STORAGE_TYPE_USER,
        .index = 0U,
        .attr = BSP_STORAGE_ATTR_DEFAULT,
    };
    bsp_storage_handle_t handle;
    const bsp_storage_info_t* info;
    uint32_t tested_addresses[STORAGE_TEST_COUNT];
    uint32_t block_count;
    uint32_t random_state;
    uint32_t completed_count = 0U;
    uint32_t i;
    int counter_loaded = 0;
    int result = -1;

    g_test_count = 0U;
    handle = bsp_storage_open(&desc);
    if (handle == NULL)
    {
        ERROR_LOG("bsp_storage_open failed");
        goto finish;
    }

    info = bsp_storage_get_info(handle);
    if (info == NULL)
    {
        ERROR_LOG("bsp_storage_get_info failed");
        goto exit;
    }

    if ((info->erase_unit != STORAGE_TEST_BLOCK_SIZE) ||
        (info->total_storage < ((STORAGE_TEST_COUNT + 1U) * STORAGE_TEST_BLOCK_SIZE)))
    {
        ERROR_LOG("unsupported storage geometry: total=%u erase_unit=%u",
                  info->total_storage, info->erase_unit);
        goto exit;
    }

    block_count = info->total_storage / STORAGE_TEST_BLOCK_SIZE;
    if (storage_test_load_counter(handle) != 0)
    {
        ERROR_LOG("load test counter failed");
        goto exit;
    }
    counter_loaded = 1;

    random_state = 0;
    if (random_state == 0U)
    {
        random_state = 0x6D2B79F5U;
    }

    INFO_LOG("random 4K read/write test start: total=%u blocks=%u test_count=%u",
             info->total_storage, block_count, g_test_count);

    for (i = 0U; i < STORAGE_TEST_COUNT; i++)
    {
        uint32_t address;

        do
        {
            /* Block 0 stores the persistent power-on test counter. */
            address = ((storage_test_random(&random_state) % (block_count - 1U)) + 1U) *
                      STORAGE_TEST_BLOCK_SIZE;
        } while (storage_test_address_used(tested_addresses, i, address));

        tested_addresses[i] = address;
        storage_test_fill_pattern(address, g_test_count, random_state);
        memset(g_read_buf, 0, sizeof(g_read_buf));

        if (bsp_storage_erase(handle, address, STORAGE_TEST_BLOCK_SIZE) != STORAGE_TEST_BLOCK_SIZE)
        {
            ERROR_LOG("erase failed: index=%u addr=0x%08X", i, address);
            goto exit;
        }
        if (bsp_storage_write(handle, address, g_write_buf, STORAGE_TEST_BLOCK_SIZE) != STORAGE_TEST_BLOCK_SIZE)
        {
            ERROR_LOG("write failed: index=%u addr=0x%08X", i, address);
            goto exit;
        }
        if (bsp_storage_read(handle, address, g_read_buf, STORAGE_TEST_BLOCK_SIZE) != STORAGE_TEST_BLOCK_SIZE)
        {
            ERROR_LOG("read failed: index=%u addr=0x%08X", i, address);
            goto exit;
        }
        if (memcmp(g_write_buf, g_read_buf, STORAGE_TEST_BLOCK_SIZE) != 0)
        {
            ERROR_LOG("verify failed: index=%u addr=0x%08X", i, address);
            goto exit;
        }

        completed_count++;
        INFO_LOG("4K test PASS: test_count=%u progress=%u/%u addr=0x%08X",
                 g_test_count, completed_count, STORAGE_TEST_COUNT, address);
    }

    result = 0;

exit:
    if (counter_loaded && (storage_test_save_counter(handle) != 0))
    {
        ERROR_LOG("save test counter failed");
        result = -1;
    }
    if (handle != NULL)
    {
        bsp_storage_close(handle);
    }

finish:
    if (result == 0)
    {
        INFO_LOG("storage test PASS: test_count=%u completed=%u/%u",
                 g_test_count, completed_count, STORAGE_TEST_COUNT);
    }
    else
    {
        ERROR_LOG("storage test FAIL: test_count=%u completed=%u/%u",
                  g_test_count, completed_count, STORAGE_TEST_COUNT);
    }
    return result;
}

REGEDIT_AFTER_APP_INIT_FUNCTION(test_bsp_storage_open, 3);

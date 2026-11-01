/* bsp_storage_test.c — SDL 文件后端 flash 语义自检。
 *
 * 验证：
 *   - 文件大小严格等于 total_size，新建/重建时填 0xFF；
 *   - 读/写允许任意地址/长度（含跨 program_unit 页）；
 *   - 只有擦除要求按 erase_unit 对齐，擦除后为 0xFF；
 *   - 未擦除区域再次写入按 old & data 生效（0xFF 可以写任意值，0 不能再变 1）。
 *
 * 注意：这是诊断用例，失败只打日志、返回 0，避免框架初始化断言直接卡死。
 */
#include "bsp/bsp_storage.h"
#include "framework/c_function_regedit.h"
#include "framework/e_log.h"

#include <string.h>

#define TAG "bsp_storage_test"
#define INFO_LOG(...)   E_INFO_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

static int test_init(void)
{
    bsp_storage_desc_t desc = {
        .type = BSP_STORAGE_TYPE_USER,
        .index = 0,
        .attr = BSP_STORAGE_ATTR_DEFAULT,
    };

    bsp_storage_handle_t h = bsp_storage_open(&desc);
    if (!h) {
        ERROR_LOG("open storage failed");
        return 0;
    }

    const bsp_storage_info_t* info = bsp_storage_get_info(h);
    INFO_LOG("info: medium=%u total=%u erase=%u program=%u erase_value=0x%02X direct=%u",
             (unsigned)info->storage_medium, (unsigned)info->total_storage,
             (unsigned)info->erase_unit, (unsigned)info->program_unit,
             (unsigned)info->erase_value, (unsigned)info->write_direct);

    uint8_t buf[4096];
    uint8_t pattern[512];
    int ok = 1;

    /* 先擦掉第一块，保证从已知的 0xFF 状态开始 */
    uint32_t e0 = bsp_storage_erase(h, 0, 4096);
    INFO_LOG("erase initial 4096: %u", (unsigned)e0);
    ok = ok && (e0 == 4096);

    memset(buf, 0, sizeof(buf));
    uint32_t n = bsp_storage_read(h, 0, buf, 16);
    INFO_LOG("read erased: n=%u first=0x%02X (expect 0xFF)", (unsigned)n, buf[0]);
    ok = ok && (n == 16) && (buf[0] == 0xFF);

    /* 非对齐写 + 非对齐长度 + 跨 256B 页：addr=3, len=300 */
    for (uint32_t i = 0; i < sizeof(pattern); i++) {
        pattern[i] = (uint8_t)(i * 7u + 1u);
    }
    uint32_t w = bsp_storage_write(h, 3, pattern, 300);
    INFO_LOG("unaligned write: addr=3 len=300 -> %u", (unsigned)w);
    ok = ok && (w == 300);

    memset(buf, 0, sizeof(buf));
    n = bsp_storage_read(h, 3, buf, 300);
    int match = (n == 300) && (memcmp(buf, pattern, 300) == 0);
    INFO_LOG("unaligned readback: n=%u match=%d", (unsigned)n, match);
    ok = ok && match;

    /* 非对齐擦除必须拒绝 */
    uint32_t bad_e = bsp_storage_erase(h, 1, 4096);
    INFO_LOG("unaligned erase: %u (expect 0)", (unsigned)bad_e);
    ok = ok && (bad_e == 0);
    bad_e = bsp_storage_erase(h, 0, 100);
    INFO_LOG("unaligned erase length: %u (expect 0)", (unsigned)bad_e);
    ok = ok && (bad_e == 0);

    /* 擦除 4KB，读回 0xFF */
    uint32_t e = bsp_storage_erase(h, 0, 4096);
    INFO_LOG("erase 4096: %u", (unsigned)e);
    ok = ok && (e == 4096);
    memset(buf, 0, sizeof(buf));
    n = bsp_storage_read(h, 0, buf, 16);
    INFO_LOG("after erase: n=%u first=0x%02X (expect 0xFF)", (unsigned)n, buf[0]);
    ok = ok && (n == 16) && (buf[0] == 0xFF);

    /* 未擦除时写 0x00：只能把 1 清成 0 */
    (void)bsp_storage_write(h, 0, pattern, 256);
    uint8_t zeros[256];
    memset(zeros, 0, sizeof(zeros));
    (void)bsp_storage_write(h, 0, zeros, sizeof(zeros));
    memset(buf, 0, sizeof(buf));
    n = bsp_storage_read(h, 0, buf, 16);
    int all_zero = 1;
    for (uint32_t i = 0; i < 16; i++) {
        if (buf[i] != 0) all_zero = 0;
    }
    INFO_LOG("write 0x00 over pattern: n=%u all_zero=%d", (unsigned)n, all_zero);
    ok = ok && all_zero;

    /* 0 不能再变 1：对 0x00 写 0xFF，读回仍是 0x00 */
    (void)bsp_storage_write(h, 7, (const uint8_t*)"\xFF", 1);
    uint8_t one = 0xFFu;
    (void)bsp_storage_read(h, 7, &one, 1);
    INFO_LOG("0->1 attempt at addr 7: 0x%02X (expect 0x00)", (unsigned)one);
    ok = ok && (one == 0x00);

    bsp_storage_close(h);
    INFO_LOG("result: %s", ok ? "PASS" : "FAIL");
    return 0;
}

REGEDIT_AFTER_APP_INIT_FUNCTION(test_init, 3);

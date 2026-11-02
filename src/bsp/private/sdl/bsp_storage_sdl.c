/* bsp_storage_sdl.c — bsp_storage 的 SDL 文件后端（模拟 NOR flash）。
 *
 * 参考 N32H762 工程的 bsp_cfg.h：存储器由 type + index 在映射表里查自己的
 * 介质参数。SDL 后端把每个表项映射成 <BSP_STORAGE_BASE_DIR>/<filename>.bin：
 *   - 文件大小严格等于 total_size；不一致时自动重建并填 erase_value；
 *   - 新建/擦除后的内容为 erase_value（NOR flash = 0xFF）；
 *   - write 要求 address/length 按 program_unit 对齐；未擦除区域按
 *     old & data 生效（只能 1->0），模拟真实 flash 的编程语义；
 *   - erase 要求 address/length 按 erase_unit 对齐，整块填 erase_value；
 *   - read 不要求对齐。
 *
 * 所有文件操作走 SDL_IOStream（SDL_IOFromFile / SDL_ReadIO / SDL_WriteIO /
 * SDL_SeekIO / SDL_FlushIO），不直接依赖 stdio。
 */
#include "bsp/bsp_storage.h"
#include "bsp_cfg.h"

#include <SDL3/SDL.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "framework/e_log.h"
#define TAG "bsp_storage"
#define INFO_LOG(...)   E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

#define BSP_STORAGE_PATH_MAX   512
#define BSP_STORAGE_MAX_OPEN   8
#define BSP_STORAGE_FILL_CHUNK 4096

/* 存储器映射表项：只属于本实现，不污染 bsp_storage.h 标准接口 */
typedef struct bsp_storage_map_t {
    bsp_storage_type_t type;
    uint8_t index;
    bsp_storage_medium_t medium;
    const char* filename;
    uint32_t total_size;
    uint32_t erase_unit;
    uint32_t program_unit;
    uint32_t erase_value;
    uint8_t write_direct;
} bsp_storage_map_t;

/* 存储器映射表：由 bsp_cfg.h 提供，type + index 唯一确定一个 bin 文件 */
static const bsp_storage_map_t s_storage_map[] = BSP_STORAGE_MAP_TABLE;

typedef struct bsp_storage_t {
    const bsp_storage_map_t* map;
    SDL_IOStream* io;
    bsp_storage_info_t info;

    char path[BSP_STORAGE_PATH_MAX];

    uint8_t* page_buf;          /* program_unit 大小的读改写缓冲 */
    uint32_t page_buf_size;

    uint8_t* erase_buf;         /* 擦除填充缓冲（最多 4KB 一块） */
    uint32_t erase_buf_size;
} bsp_storage_t;

static bsp_storage_t* s_open_handles[BSP_STORAGE_MAX_OPEN];

static const bsp_storage_map_t* storage_map_find(bsp_storage_type_t type, uint8_t index)
{
    for (size_t i = 0; i < sizeof(s_storage_map) / sizeof(s_storage_map[0]); i++) {
        if (s_storage_map[i].type == type && s_storage_map[i].index == index) {
            return &s_storage_map[i];
        }
    }
    return NULL;
}

static int storage_handle_register(bsp_storage_t* h, uint8_t exclusive)
{
    if (exclusive) {
        for (uint32_t i = 0; i < BSP_STORAGE_MAX_OPEN; i++) {
            if (s_open_handles[i] && s_open_handles[i]->map == h->map) {
                WARN_LOG("'%s.bin' already opened exclusively", h->map->filename);
                return -1;
            }
        }
    }

    for (uint32_t i = 0; i < BSP_STORAGE_MAX_OPEN; i++) {
        if (s_open_handles[i] == NULL) {
            s_open_handles[i] = h;
            return 0;
        }
    }
    return -1;
}

static void storage_handle_unregister(bsp_storage_t* h)
{
    for (uint32_t i = 0; i < BSP_STORAGE_MAX_OPEN; i++) {
        if (s_open_handles[i] == h) {
            s_open_handles[i] = NULL;
            return;
        }
    }
}

/* 打开已有 bin；大小不符或不存在时重建并填 erase_value */
static int storage_file_open(bsp_storage_t* h)
{
    const bsp_storage_map_t* map = h->map;
    SDL_IOStream* io = SDL_IOFromFile(h->path, "rb+");

    if (io) {
        Sint64 size = SDL_GetIOSize(io);
        if (size == (Sint64)map->total_size) {
            h->io = io;
            return 0;
        }
        WARN_LOG("'%s' size %lld != %u, recreate", h->path,
                 (long long)size, (unsigned)map->total_size);
        SDL_CloseIO(io);
    }

    io = SDL_IOFromFile(h->path, "wb+");
    if (!io) {
        ERROR_LOG("create '%s' failed: %s", h->path, SDL_GetError());
        return -1;
    }

    uint8_t fill[BSP_STORAGE_FILL_CHUNK];
    memset(fill, (int)(map->erase_value & 0xFFu), sizeof(fill));

    uint32_t remain = map->total_size;
    while (remain > 0) {
        size_t chunk = remain < sizeof(fill) ? (size_t)remain : sizeof(fill);
        if (SDL_WriteIO(io, fill, chunk) != chunk) {
            ERROR_LOG("fill '%s' failed: %s", h->path, SDL_GetError());
            SDL_CloseIO(io);
            return -1;
        }
        remain -= (uint32_t)chunk;
    }

    SDL_FlushIO(io);
    if (SDL_SeekIO(io, 0, SDL_IO_SEEK_SET) < 0) {
        ERROR_LOG("seek '%s' failed: %s", h->path, SDL_GetError());
        SDL_CloseIO(io);
        return -1;
    }

    h->io = io;
    INFO_LOG("created '%s' (%u bytes, fill=0x%02X)",
             h->path, (unsigned)map->total_size, (unsigned)(map->erase_value & 0xFFu));
    return 0;
}

bsp_storage_handle_t bsp_storage_open(bsp_storage_desc_t* desc)
{
    if (!desc) {
        return NULL;
    }

    const bsp_storage_map_t* map = storage_map_find(desc->type, desc->index);
    if (!map) {
        WARN_LOG("unsupported storage type=%u index=%u",
                 (unsigned)desc->type, (unsigned)desc->index);
        return NULL;
    }
    if (!map->filename || map->total_size == 0) {
        ERROR_LOG("invalid storage map entry");
        return NULL;
    }

    bsp_storage_t* h = (bsp_storage_t*)calloc(1, sizeof(*h));
    if (!h) {
        return NULL;
    }
    h->map = map;

    h->info.storage_medium = map->medium;
    h->info.total_storage  = map->total_size;
    h->info.erase_unit     = map->erase_unit;
    h->info.program_unit   = map->program_unit;
    h->info.erase_value    = map->erase_value;
    h->info.write_direct   = map->write_direct ? 1u : 0u;
    h->info.reserved       = 0;

    h->page_buf_size = map->program_unit ? map->program_unit : 1u;
    h->page_buf = (uint8_t*)malloc(h->page_buf_size);
    if (!h->page_buf) {
        goto fail;
    }

    h->erase_buf_size = map->erase_unit ? map->erase_unit : 1u;
    if (h->erase_buf_size > BSP_STORAGE_FILL_CHUNK) {
        h->erase_buf_size = BSP_STORAGE_FILL_CHUNK;
    }
    h->erase_buf = (uint8_t*)malloc(h->erase_buf_size);
    if (!h->erase_buf) {
        goto fail;
    }

    int path_len = snprintf(h->path, sizeof(h->path), "%s/%s.bin",
                            BSP_STORAGE_BASE_DIR, map->filename);
    if (path_len < 0 || path_len >= (int)sizeof(h->path)) {
        ERROR_LOG("storage path too long");
        goto fail;
    }

    /* 目录已存在时 SDL_CreateDirectory 会返回 false；忽略，让后面的 open 报真实错误 */
    (void)SDL_CreateDirectory(BSP_STORAGE_BASE_DIR);

    if (storage_file_open(h) != 0) {
        goto fail;
    }

    if (storage_handle_register(h, (desc->attr & BSP_STORAGE_ATTR_EXCLUSIVE_OPEN) ? 1u : 0u) != 0) {
        goto fail;
    }

    INFO_LOG("open '%s': %u bytes, erase=%u, program=%u, erase_value=0x%02X, direct=%u",
             h->path, (unsigned)map->total_size, (unsigned)map->erase_unit,
             (unsigned)map->program_unit, (unsigned)(map->erase_value & 0xFFu),
             (unsigned)h->info.write_direct);
    return (bsp_storage_handle_t)h;

fail:
    if (h->io) SDL_CloseIO(h->io);
    free(h->page_buf);
    free(h->erase_buf);
    free(h);
    return NULL;
}

void bsp_storage_close(bsp_storage_handle_t handle)
{
    bsp_storage_t* h = (bsp_storage_t*)handle;
    if (!h) {
        return;
    }

    storage_handle_unregister(h);
    if (h->io) {
        SDL_FlushIO(h->io);
        SDL_CloseIO(h->io);
    }
    free(h->page_buf);
    free(h->erase_buf);
    free(h);
}

const bsp_storage_info_t* bsp_storage_get_info(bsp_storage_handle_t handle)
{
    bsp_storage_t* h = (bsp_storage_t*)handle;
    return h ? &h->info : NULL;
}

uint32_t bsp_storage_read(bsp_storage_handle_t handle, uint32_t address, uint8_t* data, uint32_t length)
{
    bsp_storage_t* h = (bsp_storage_t*)handle;
    if (!h || !h->io || !data || length == 0) {
        return 0;
    }
    if (address >= h->info.total_storage) {
        return 0;
    }
    if (length > h->info.total_storage - address) {
        length = h->info.total_storage - address;
    }

    if (SDL_SeekIO(h->io, (Sint64)address, SDL_IO_SEEK_SET) < 0) {
        return 0;
    }
    return (uint32_t)SDL_ReadIO(h->io, data, length);
}

uint32_t bsp_storage_write(bsp_storage_handle_t handle, uint32_t address, const uint8_t* data, uint32_t length)
{
    bsp_storage_t* h = (bsp_storage_t*)handle;
    if (!h || !h->io || !data || length == 0) {
        return 0;
    }
    if (h->info.program_unit == 0) {
        WARN_LOG("storage does not support program");
        return 0;
    }
    if (address >= h->info.total_storage ||
        length > h->info.total_storage - address) {
        WARN_LOG("write out of range: addr=%u len=%u total=%u",
                 (unsigned)address, (unsigned)length, (unsigned)h->info.total_storage);
        return 0;
    }

    /* 可直接写的介质：直接覆盖 */
    if (h->info.write_direct) {
        if (SDL_SeekIO(h->io, (Sint64)address, SDL_IO_SEEK_SET) < 0) {
            return 0;
        }
        return (uint32_t)SDL_WriteIO(h->io, data, length);
    }

    /* flash：允许任意起始地址/长度；按 program_unit 分页读改写。
     * 页内只修改落在 [address, address+length) 的字节，页外字节保持原值。 */
    uint32_t done = 0;
    uint32_t rejected = 0;
    const uint32_t unit = h->page_buf_size;
    const uint32_t end = address + length;

    while (address + done < end) {
        uint32_t pos = address + done;
        uint32_t page_base = (pos / unit) * unit;
        uint32_t page_off = pos - page_base;
        uint32_t chunk = unit - page_off;
        uint32_t read_len = unit;

        if (chunk > end - pos) {
            chunk = end - pos;
        }
        if (read_len > h->info.total_storage - page_base) {
            read_len = h->info.total_storage - page_base;
        }

        if (SDL_SeekIO(h->io, (Sint64)page_base, SDL_IO_SEEK_SET) < 0) {
            break;
        }
        if (SDL_ReadIO(h->io, h->page_buf, read_len) != read_len) {
            break;
        }

        for (uint32_t i = 0; i < chunk; i++) {
            uint8_t old = h->page_buf[page_off + i];
            uint8_t val = data[done + i];
            uint8_t merged = (uint8_t)(old & val);
            if (merged != val) {
                rejected++;
            }
            h->page_buf[page_off + i] = merged;
        }

        if (SDL_SeekIO(h->io, (Sint64)page_base, SDL_IO_SEEK_SET) < 0) {
            break;
        }
        if (SDL_WriteIO(h->io, h->page_buf, read_len) != read_len) {
            break;
        }
        done += chunk;
    }

    if (rejected > 0) {
        WARN_LOG("%u byte(s) attempted 0->1 without erase (addr=%u)",
                 (unsigned)rejected, (unsigned)address);
    }

    return done;
}

uint32_t bsp_storage_erase(bsp_storage_handle_t handle, uint32_t address, uint32_t length)
{
    bsp_storage_t* h = (bsp_storage_t*)handle;
    if (!h || !h->io || length == 0) {
        return 0;
    }
    if (h->info.erase_unit == 0) {
        WARN_LOG("storage does not support erase");
        return 0;
    }
    if (h->info.erase_unit > 1 &&
        ((address % h->info.erase_unit) != 0 || (length % h->info.erase_unit) != 0)) {
        WARN_LOG("unaligned erase: addr=%u len=%u erase_unit=%u",
                 (unsigned)address, (unsigned)length, (unsigned)h->info.erase_unit);
        return 0;
    }
    if (address >= h->info.total_storage ||
        length > h->info.total_storage - address) {
        WARN_LOG("erase out of range: addr=%u len=%u total=%u",
                 (unsigned)address, (unsigned)length, (unsigned)h->info.total_storage);
        return 0;
    }

    memset(h->erase_buf, (int)(h->info.erase_value & 0xFFu), h->erase_buf_size);

    uint32_t done = 0;
    while (done < length) {
        uint32_t chunk = length - done;
        if (chunk > h->erase_buf_size) {
            chunk = h->erase_buf_size;
        }
        if (SDL_SeekIO(h->io, (Sint64)(address + done), SDL_IO_SEEK_SET) < 0) {
            break;
        }
        if (SDL_WriteIO(h->io, h->erase_buf, chunk) != chunk) {
            break;
        }
        done += chunk;
    }

    return done;
}

void bsp_storage_flush(bsp_storage_handle_t handle)
{
    bsp_storage_t* h = (bsp_storage_t*)handle;
    if (h && h->io) {
        SDL_FlushIO(h->io);
    }
}

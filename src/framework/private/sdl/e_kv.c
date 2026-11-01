#include "framework/e_kv.h"
#include "framework_cfg.h"

#ifdef E_KV_PARTITION_MAP_TABLE

#include "framework/c_function_regedit.h"
#include "framework/e_log.h"
#include "framework/e_mutex.h"
#include "framework/e_sched.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define E_KV_MKDIR(p)  _mkdir(p)
#else
#include <sys/stat.h>
#define E_KV_MKDIR(p)  mkdir((p), 0755)
#endif

#define TAG "E_KV"
#define INFO_LOG(...)   E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

/* PC/SDL 后端以文件系统等价实现 EasyFlash ENV：
 * 每个分区对应一个子目录，键值各存一个文件 <base>/<dir>/<key>.bin。
 * 分区映射表由 framework_cfg.h 的 E_KV_PARTITION_MAP_TABLE 提供。 */

#define E_KV_MAX_PATH 512

typedef struct e_kv_partition_map_t {
    const char* partition;
    const char* dir;
} e_kv_partition_map_t;

typedef struct e_kv_partition_t {
    const e_kv_partition_map_t* map;
    char dir_path[E_KV_MAX_PATH];
} e_kv_partition_t;

static const e_kv_partition_map_t e_kv_partition_map[] = E_KV_PARTITION_MAP_TABLE;
#define E_KV_PARTITION_COUNT (sizeof(e_kv_partition_map) / sizeof(e_kv_partition_map[0]))

static e_kv_partition_t e_kv_partition[E_KV_PARTITION_COUNT];
static e_mutex_handle_t s_kv_mutex;
static uint8_t s_kv_ready;

static void e_kv_lock(void)
{
    if ((s_kv_mutex == NULL) || !e_sched_is_started() ||
        (e_sched_current_is_interrupt_context() != 0u)) {
        return;
    }
    e_mutex_lock(s_kv_mutex);
}

static void e_kv_unlock(void)
{
    if ((s_kv_mutex == NULL) || !e_sched_is_started() ||
        (e_sched_current_is_interrupt_context() != 0u)) {
        return;
    }
    e_mutex_unlock(s_kv_mutex);
}

static e_kv_partition_t* e_kv_find_partition(const char* partition)
{
    if (partition == NULL) {
        return NULL;
    }
    for (uint32_t i = 0; i < E_KV_PARTITION_COUNT; i++) {
        if (strcmp(e_kv_partition_map[i].partition, partition) == 0) {
            return &e_kv_partition[i];
        }
    }
    return NULL;
}

/* 构造键值文件完整路径：<base>/<dir>/<key>.bin */
static int e_kv_build_path(const e_kv_partition_t* part, const char* key, char* out, size_t out_size)
{
    if (part == NULL || key == NULL || out == NULL || out_size == 0) {
        return -1;
    }
    snprintf(out, out_size, "%s/%s/%s.bin", E_KV_BASE_DIR, part->map->dir, key);
    return 0;
}

/* 确保目录存在（递归创建到 base/dir 两级） */
static int e_kv_ensure_dirs(void)
{
    char path[E_KV_MAX_PATH];
    snprintf(path, sizeof(path), "%s", E_KV_BASE_DIR);
    if (E_KV_MKDIR(path) != 0) {
        /* 已存在则忽略 */
        struct stat st;
        if (stat(path, &st) != 0 || !(st.st_mode & 040000)) {
            return -1;
        }
    }
    for (uint32_t i = 0; i < E_KV_PARTITION_COUNT; i++) {
        snprintf(path, sizeof(path), "%s/%s", E_KV_BASE_DIR, e_kv_partition_map[i].dir);
        if (E_KV_MKDIR(path) != 0) {
            struct stat st;
            if (stat(path, &st) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

uint32_t e_kv_format(const char* partition)
{
    e_kv_partition_t* part;

    if ((partition == NULL) || (s_kv_ready == 0)) {
        return 1u;
    }

    part = e_kv_find_partition(partition);
    if (part == NULL) {
        return 1u;
    }

    e_kv_lock();

    /* 逐个删除该分区目录下的 *.bin 文件 */
    char pattern[E_KV_MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s/%s/*.bin", E_KV_BASE_DIR, part->map->dir);

#ifdef _WIN32
    {
        /* Windows：用 dir 通配扫描并删除 */
        char cmd[E_KV_MAX_PATH + 16];
        snprintf(cmd, sizeof(cmd), "del /q \"%s\" 2>nul", pattern);
        system(cmd);
    }
#else
    {
        char cmd[E_KV_MAX_PATH + 16];
        snprintf(cmd, sizeof(cmd), "rm -f %s", pattern);
        system(cmd);
    }
#endif

    e_kv_unlock();
    return 0u;
}

uint32_t e_kv_read_blob(const char* partition, const char* key, void* buf, uint32_t size)
{
    e_kv_partition_t* part;
    char path[E_KV_MAX_PATH];
    FILE* f;
    size_t n;

    if ((partition == NULL) || (key == NULL) || (buf == NULL) || (size == 0u) ||
        (s_kv_ready == 0)) {
        return 0u;
    }

    part = e_kv_find_partition(partition);
    if (part == NULL || e_kv_build_path(part, key, path, sizeof(path)) != 0) {
        return 0u;
    }

    e_kv_lock();
    f = fopen(path, "rb");
    if (f == NULL) {
        e_kv_unlock();
        return 0u;
    }
    n = fread(buf, 1, size, f);
    fclose(f);
    e_kv_unlock();

    return (uint32_t)n;
}

uint32_t e_kv_write_blob(const char* partition, const char* key, const void* buf, uint32_t size)
{
    e_kv_partition_t* part;
    char path[E_KV_MAX_PATH];
    FILE* f;

    if ((partition == NULL) || (key == NULL) || (buf == NULL) || (size == 0u) ||
        (s_kv_ready == 0)) {
        return 0u;
    }

    part = e_kv_find_partition(partition);
    if (part == NULL || e_kv_build_path(part, key, path, sizeof(path)) != 0) {
        return 0u;
    }

    e_kv_lock();
    f = fopen(path, "wb");
    if (f == NULL) {
        e_kv_unlock();
        return 0u;
    }
    size_t n = fwrite(buf, 1, size, f);
    fclose(f);
    e_kv_unlock();

    return (n == size) ? size : 0u;
}

int e_kv_delete(const char* partition, const char* key)
{
    e_kv_partition_t* part;
    char path[E_KV_MAX_PATH];

    if ((partition == NULL) || (key == NULL) || (s_kv_ready == 0)) {
        return -1;
    }

    part = e_kv_find_partition(partition);
    if (part == NULL || e_kv_build_path(part, key, path, sizeof(path)) != 0) {
        return -1;
    }

    e_kv_lock();
    int ret = remove(path);
    e_kv_unlock();

    return (ret == 0) ? 0 : -1;
}

uint8_t e_kv_exists(const char* partition, const char* key)
{
    e_kv_partition_t* part;
    char path[E_KV_MAX_PATH];
    FILE* f;

    if ((partition == NULL) || (key == NULL) || (s_kv_ready == 0)) {
        return 0u;
    }

    part = e_kv_find_partition(partition);
    if (part == NULL || e_kv_build_path(part, key, path, sizeof(path)) != 0) {
        return 0u;
    }

    e_kv_lock();
    f = fopen(path, "rb");
    if (f) {
        fclose(f);
    }
    e_kv_unlock();

    return (f != NULL) ? 1u : 0u;
}

static int e_kv_partition_init(void)
{
    s_kv_mutex = e_mutex_create();
    if (s_kv_mutex == NULL) {
        ERROR_LOG("mutex create failed");
        return -1;
    }

    for (uint32_t i = 0; i < E_KV_PARTITION_COUNT; i++) {
        e_kv_partition[i].map = &e_kv_partition_map[i];
    }

    if (e_kv_ensure_dirs() != 0) {
        ERROR_LOG("create kv directories failed");
        return -1;
    }

    s_kv_ready = 1u;
    INFO_LOG("init ok (file backend), partitions=%u", (unsigned)E_KV_PARTITION_COUNT);
    return 0;
}

REGEDIT_AFTER_BSP_INIT_FUNCTION(e_kv_partition_init, 1);

#else /* !E_KV_PARTITION_MAP_TABLE */

uint32_t e_kv_format(const char* partition)
{
    (void)partition;
    return 1u;
}

uint32_t e_kv_read_blob(const char* partition, const char* key, void* buf, uint32_t size)
{
    (void)partition; (void)key; (void)buf; (void)size;
    return 0u;
}

uint32_t e_kv_write_blob(const char* partition, const char* key, const void* buf, uint32_t size)
{
    (void)partition; (void)key; (void)buf; (void)size;
    return 0u;
}

int e_kv_delete(const char* partition, const char* key)
{
    (void)partition; (void)key;
    return -1;
}

uint8_t e_kv_exists(const char* partition, const char* key)
{
    (void)partition; (void)key;
    return 0u;
}

#endif /* E_KV_PARTITION_MAP_TABLE */

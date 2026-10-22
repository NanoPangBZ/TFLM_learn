#include "framework/e_heap.h"
#include <stddef.h>
#include "framework_cfg.h"

#include "framework/e_log.h"
#define TAG "e_heap"
#define DEBUG_LOG(...)  //E_DEBUG_LOG(TAG, __VA_ARGS__)
#define INFO_LOG(...)   //E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

/**
 * @todo 内部统一malloc api，支持多属性扩展
*/

// 堆描述结构体
typedef struct e_heap_t{
    uint8_t         is_init;    // 是否初始化
    uint8_t*        heap_mem;
    uint8_t         align_size; // 对齐大小，默认为4字节
    uint32_t        heap_used;  // 已使用大小
    uint32_t        heap_size;  // 堆大小
}e_heap_t;

// 默认堆实例
static uint8_t E_DEFAULT_HEAP_MEM_ATTR e_default_heap_mem[E_DEFAULT_HEAP_SIZE];
static e_heap_t g_default_heap = {0};

// 快速访问堆实例
#if defined(E_FAST_ACCESS_HEAP_ENABLE) && (E_FAST_ACCESS_HEAP_ENABLE == 1)
    static uint8_t E_FAST_ACCESS_HEAP_MEM_ATTR e_fast_access_heap_mem[E_FAST_ACCESS_HEAP_SIZE];
    static e_heap_t g_fast_access_heap = {0};
#endif

// 大块内存堆实例
#if defined(E_BIG_BLOCKS_HEAP_ENABLE) && (E_BIG_BLOCKS_HEAP_ENABLE == 1)
    static uint8_t E_BIG_BLOCKS_MEM_HEAP_ATTR e_big_blocks_heap_mem[E_BIG_BLOCKS_HEAP_SIZE];
    static e_heap_t g_big_blocks_heap = {0};
#endif

int e_heap_init(void)
{
    ENTER_CRITICAL();

    g_default_heap.heap_mem = e_default_heap_mem;       // 设置堆内存
    g_default_heap.heap_used = 0;                       // 初始化已使用大小为0  
    g_default_heap.heap_size = E_DEFAULT_HEAP_SIZE;     // 设置堆大小
    g_default_heap.is_init = 1;                         // 标记为已初始化
    g_default_heap.align_size = E_DEFAULT_HEAP_ALIGN_SIZE; // 设置对齐大小


#if defined(E_FAST_ACCESS_HEAP_ENABLE) && (E_FAST_ACCESS_HEAP_ENABLE == 1)
    g_fast_access_heap.heap_mem = e_fast_access_heap_mem;           // 设置堆内存
    g_fast_access_heap.heap_used = 0;                               // 初始化已使用大小为0
    g_fast_access_heap.heap_size = E_FAST_ACCESS_HEAP_SIZE;         // 设置堆大小
    g_fast_access_heap.is_init = 1;                                 // 标记为已初始化
    g_fast_access_heap.align_size = E_FAST_ACCESS_HEAP_ALIGN_SIZE; // 设置对齐大小
#endif

#if defined(E_BIG_BLOCKS_HEAP_ENABLE) && (E_BIG_BLOCKS_HEAP_ENABLE == 1)
    g_big_blocks_heap.heap_mem = e_big_blocks_heap_mem;         // 设置堆内存
    g_big_blocks_heap.heap_used = 0;                            // 初始化已使用大小为0
    g_big_blocks_heap.heap_size = E_BIG_BLOCKS_HEAP_SIZE;       // 设置堆大小
    g_big_blocks_heap.is_init = 1;                              // 标记为已初始化
    g_big_blocks_heap.align_size = E_BIG_BLOCKS_HEAP_ALIGN_SIZE;    //设置对齐大小
#endif

    EXIT_CRITICAL();

    return 0; // Initialization successful
}

void* e_heap_malloc(uint32_t size,e_heap_attr_t attr)
{
    ENTER_CRITICAL();

    // 使用快速访问堆
#if defined(E_FAST_ACCESS_HEAP_ENABLE) && (E_FAST_ACCESS_HEAP_ENABLE == 1)
    if( attr & E_HEAP_ATTR_FAST_ACCESS ){
        if(!g_fast_access_heap.is_init){
            return NULL; // 未初始化
        }

        if(size == 0 || size > g_fast_access_heap.heap_size - g_fast_access_heap.heap_used){
            return NULL; // 无效大小
        }

        // 分配内存
        // 对齐大小
        size = (size + g_fast_access_heap.align_size - 1) & ~(g_fast_access_heap.align_size - 1);
        uint8_t* ptr = g_fast_access_heap.heap_mem + g_fast_access_heap.heap_used;
        g_fast_access_heap.heap_used += size;

        EXIT_CRITICAL();

        return ptr;
    }
#endif

    //使用大块内存堆
#if defined(E_BIG_BLOCKS_HEAP_ENABLE) && (E_BIG_BLOCKS_HEAP_ENABLE == 1)
    if( attr & E_HEAP_ATTR_BIG_BLOCKS ){
        if(!g_big_blocks_heap.is_init){
            return NULL; // 未初始化
        }

        if(size == 0 || size > g_big_blocks_heap.heap_size - g_big_blocks_heap.heap_used){
            return NULL; // 无效大小
        }

        // 分配内存
        // 对齐大小
        size = (size + g_big_blocks_heap.align_size - 1) & ~(g_big_blocks_heap.align_size - 1);
        uint8_t* ptr = g_big_blocks_heap.heap_mem + g_big_blocks_heap.heap_used;
        g_big_blocks_heap.heap_used += size;

        EXIT_CRITICAL();
        return ptr;
    }
#endif

    // 使用默认堆
    if(!g_default_heap.is_init){
        return NULL; // 未初始化
    }

    if(size == 0 || size > g_default_heap.heap_size - g_default_heap.heap_used){
        WARN_LOG("e_heap_malloc: attr=0x%08X, size=%u failed! Not enough memory!",attr,size);
        return NULL; // 无效大小
    }

    // 分配内存
    // 对齐大小
    size = (size + g_default_heap.align_size - 1) & ~(g_default_heap.align_size - 1);
    uint8_t* ptr = g_default_heap.heap_mem + g_default_heap.heap_used;
    g_default_heap.heap_used += size;

    DEBUG_LOG("e_heap_malloc: attr=0x%08X, size=%u, used=%u/%u",attr,size,g_default_heap.heap_used,g_default_heap.heap_size);

    EXIT_CRITICAL();

    return ptr;
}

void e_heap_free(void* ptr)
{
    (void)ptr;
}

uint32_t e_heap_get_used_size(e_heap_attr_t attr)
{
    if( attr & E_HEAP_ATTR_FAST_ACCESS ){
#if defined(E_FAST_ACCESS_HEAP_ENABLE) && (E_FAST_ACCESS_HEAP_ENABLE == 1)
        return g_fast_access_heap.heap_used;
#endif
    }
    else if( attr & E_HEAP_ATTR_BIG_BLOCKS ){
#if defined(E_BIG_BLOCKS_HEAP_ENABLE) && (E_BIG_BLOCKS_HEAP_ENABLE == 1)
        return g_big_blocks_heap.heap_used;
#endif
    }
    else if( attr & E_HEAP_ATTR_DEFAULT ){
        return g_default_heap.heap_used;
    }

    return 0;
}

uint32_t e_heap_get_free_size(e_heap_attr_t attr)
{
    if( attr & E_HEAP_ATTR_FAST_ACCESS ){
#if defined(E_FAST_ACCESS_HEAP_ENABLE) && (E_FAST_ACCESS_HEAP_ENABLE == 1)
            return g_fast_access_heap.heap_size - g_fast_access_heap.heap_used;
#endif
    }
    else if( attr & E_HEAP_ATTR_BIG_BLOCKS ){
#if defined(E_BIG_BLOCKS_HEAP_ENABLE) && (E_BIG_BLOCKS_HEAP_ENABLE == 1)
        return g_big_blocks_heap.heap_size - g_big_blocks_heap.heap_used;
#endif
    }
    else if( attr & E_HEAP_ATTR_DEFAULT ){
        return g_default_heap.heap_size - g_default_heap.heap_used;
    }

    return 0;
}

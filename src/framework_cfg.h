#pragma once

/**************************SDL 后端说明*****************************************/
/* 本配置是 YouWei Embedded Platform framework 的 SDL3 后端配置。
 * 与参考工程的 FreeRTOS 后端一一对应：调度 -> SDL 线程，互斥/信号 -> SDL 信号量，
 * 临界区 -> SDL 互斥锁，日志 -> 控制台输出，PendSV -> SDL 延迟回调线程，
 * e_kv -> 文件存储，e_com -> com_host 内存端口。 */

#include <stdint.h>
#include <SDL3/SDL_mutex.h>

/**************************临界区配置****************************************/
/* PC/SDL 多线程环境下，用一把进程内递归互斥锁近似 MCU 的关中断临界区。
 * 惰性创建，首次进入临界区时初始化。 */
extern SDL_Mutex* e_critical_mutex_get(void);

#define ENTER_CRITICAL()            do { SDL_LockMutex(e_critical_mutex_get()); } while (0)
#define EXIT_CRITICAL()             do { SDL_UnlockMutex(e_critical_mutex_get()); } while (0)
#define ENTER_CRITICAL_FROM_ISR()   ENTER_CRITICAL()
#define EXIT_CRITICAL_FROM_ISR()    EXIT_CRITICAL()

/**************************堆内存配置****************************************/
/* 默认堆，必须配置 */
#define E_DEFAULT_HEAP_SIZE             (256 * 1024)                       // 256KB heap size
#define E_DEFAULT_HEAP_MEM_ATTR         __attribute__((aligned(16)))
#define E_DEFAULT_HEAP_ALIGN_SIZE       16

/* 快速访问堆：PC 上暂无独立存储介质需求，关闭 */
#define E_FAST_ACCESS_HEAP_ENABLE       0
#define E_FAST_ACCESS_HEAP_SIZE         (16 * 1024)
#define E_FAST_ACCESS_HEAP_MEM_ATTR     __attribute__((aligned(4)))
#define E_FAST_ACCESS_HEAP_ALIGN_SIZE   4

/* 大块内存堆：关闭（避免巨型静态缓冲） */
#define E_BIG_BLOCKS_HEAP_ENABLE        0
#define E_BIG_BLOCKS_HEAP_SIZE          (4 * 1024 * 1024)
#define E_BIG_BLOCKS_MEM_HEAP_ATTR      __attribute__((aligned(64)))
#define E_BIG_BLOCKS_HEAP_ALIGN_SIZE    64

/**************************e_polling_task(轮询任务池)配置********************/
/* SDL 后端下该值仅为兼容保留，线程栈由 OS 管理 */
#define E_POLLING_TASK_THREAD_STACK_SIZE        (8 * 1024)

/**************************e_log日志设置****************************************/

// 使能日志功能
#define E_LOG_ENABLE    1

// 初始化阶段日志等级
#define E_LOG_INIT_STEP_LEVEL    E_LOG_LEVEL_INFO

// 默认日志等级
#define E_LOG_DEFAULT_LEVEL     E_LOG_LEVEL_INFO

// 默认聚焦的 TAG（不聚焦）
#define E_LOG_FOCUSED_TAG   NULL

/***************************e_com配置************************************************/
// 使能 e_com 主机（基于 com_host 框架实现）
#define E_COM_HOST_ENABLE   (1)

// 主机运行频率，单位：Hz
#define E_COM_HOST_RUN_FREQ_HZ   (100)

// 使能基础消息响应功能
#define E_COM_BASE_MSG_RSP_ENABLE   (1)

// 主机 ID
#define E_COM_HOST_ID   COM_HOST_ID_MAIN_CONTROLLER

// 主机栈空间配置（com_host 内部 bump 堆）
#define E_COM_HOST_STACK_SIZE   (64 * 1024)
#define E_COM_HOST_STACK_ATTR   __attribute__((aligned(8)))

/**************************e_kv配置（文件存储等价实现）***************************/
/* PC 上以文件系统等价实现 EasyFlash ENV：每个分区一个子目录，键值各存一个文件。 */
#define E_KV_BASE_DIR   "kv_data"

#define E_KV_PARTITION_MAP_TABLE {\
        { .partition = "sys",  .dir = "sys"  },\
        { .partition = "user", .dir = "user" },\
    }

/**************************e_online_monitor配置***********************************/
/* 默认关闭（与参考一致：未定义即禁用）。需要时取消注释启用。 */
// #define ENABLE_E_ONLINE_MONITOR  1

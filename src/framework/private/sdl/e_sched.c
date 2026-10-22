#include "framework/e_sched.h"
#include "framework/c_list.h"
#include "framework/c_function_regedit.h"
#include "framework/e_heap.h"
#include "framework/e_log.h"
#include "framework_cfg.h"

#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_timer.h>
#include <string.h>

#define MALLOC(size)  e_heap_malloc((size), E_HEAP_ATTR_DEFAULT)
#define FREE(ptr)     e_heap_free(ptr)

#define TAG "e_sched"
#define INFO_LOG(...)   E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

/* ======================================================================
 * SDL3 后端任务：每个 e_sched 任务映射为一个 SDL_Thread。
 * 抢占式语义由 OS 线程调度近似；"创建但未启动" 通过条件变量的启动闸门实现。
 * 任务结构体按进程生命周期分配，不回收（避免跨线程 use-after-free）。
 * ==================================================================== */

typedef struct e_sched_task_t {
    SDL_Thread* thread;
    void (*entry)(void* arg);
    void* arg;
    e_sched_task_attr_t attr;

    SDL_Mutex* gate_lock;      /* 保护 released / suspended 标志 */
    SDL_Condition* gate_cond;  /* 启动闸门 */
    uint8_t released;          /* 调度器启动后可运行（或独立初始化线程立即为 1） */
    uint8_t suspended;         /* 显式挂起 */
} e_sched_task_t;

typedef struct e_sched_t {
    uint8_t is_started;
    list_handle_t need_set_up_task_list; /* list<e_sched_task_handle_t> */
} e_sched_t;

static e_sched_t* scheduler = NULL;

static SDL_ThreadPriority map_priority(e_sched_task_attr_t attr);

/* 任务线程体：先等待启动闸门，再执行用户入口 */
static int SDLCALL sched_task_wrapper(void* p)
{
    e_sched_task_t* task = (e_sched_task_t*)p;

    /* 由线程自己设置优先级（SDL3 无按目标线程设置优先级的接口） */
    SDL_SetCurrentThreadPriority(map_priority(task->attr));

    SDL_LockMutex(task->gate_lock);
    while (!task->released || task->suspended) {
        SDL_WaitCondition(task->gate_cond, task->gate_lock);
    }
    SDL_UnlockMutex(task->gate_lock);

    if (task->entry) {
        task->entry(task->arg);
    }
    return 0;
}

/* 解析优先级属性低 3 位 -> SDL 线程优先级 */
static SDL_ThreadPriority map_priority(e_sched_task_attr_t attr)
{
    if (attr & E_SCHED_TASK_ATTR_ADMIN_THREAD) {
        return SDL_THREAD_PRIORITY_HIGH;
    }

    uint32_t pri = attr & 0x07u;
    if (pri >= 7) return SDL_THREAD_PRIORITY_TIME_CRITICAL;
    if (pri >= 5) return SDL_THREAD_PRIORITY_HIGH;
    if (pri >= 2) return SDL_THREAD_PRIORITY_NORMAL;
    return SDL_THREAD_PRIORITY_LOW;
}

void e_sched_start(void)
{
    if (!scheduler) {
        return;
    }

    /* 释放所有等待启动的任务 */
    foreach_list(scheduler->need_set_up_task_list, handle_addr, e_sched_task_handle_t)
    {
        e_sched_task_t* task = (e_sched_task_t*)(*handle_addr);
        if (task) {
            SDL_LockMutex(task->gate_lock);
            task->released = 1;
            SDL_SignalCondition(task->gate_cond);
            SDL_UnlockMutex(task->gate_lock);
        }
    }

    scheduler->is_started = 1;
}

e_sched_task_handle_t e_sched_task_create(const e_sched_task_desc_t* desc)
{
    if (!desc || !desc->task_entry || !scheduler) {
        return NULL;
    }

    e_sched_task_t* task = (e_sched_task_t*)MALLOC(sizeof(e_sched_task_t));
    if (!task) {
        ERROR_LOG("Failed to allocate task structure");
        return NULL;
    }
    memset(task, 0, sizeof(*task));

    task->entry = desc->task_entry;
    task->arg = desc->arg;
    task->attr = desc->attr;

    task->gate_lock = SDL_CreateMutex();
    task->gate_cond = SDL_CreateCondition();
    if (!task->gate_lock || !task->gate_cond) {
        ERROR_LOG("Failed to create task gate primitives");
        goto err_recycle;
    }

    /* 独立初始化线程创建后立即运行，否则等待调度器启动 */
    if (scheduler->is_started || (desc->attr & E_SCHED_TASK_ATTR_INDEPEND_INIT_THREAD)) {
        task->released = 1;
    } else {
        task->released = 0;
    }

    task->thread = SDL_CreateThread(sched_task_wrapper, desc->name, task);
    if (!task->thread) {
        ERROR_LOG("Failed to create task(%s)", desc->name ? desc->name : "?");
        goto err_recycle;
    }
    SDL_DetachThread(task->thread);

    /* 调度器未启动且非独立初始化线程：挂起，等待 e_sched_start 释放 */
    if (!task->released) {
        list_add_element(scheduler->need_set_up_task_list, &task);
    }

    return (e_sched_task_handle_t)task;

err_recycle:
    if (task->gate_cond) SDL_DestroyCondition(task->gate_cond);
    if (task->gate_lock) SDL_DestroyMutex(task->gate_lock);
    FREE(task);
    return NULL;
}

void e_sched_task_delete(e_sched_task_handle_t task)
{
    /* SDL 后端下，task == NULL 表示删除当前线程：线程体返回即结束，无需额外动作。
     * 非当前线程的删除为尽力而为：不强制终止（SDL 无安全终止原语），
     * 任务结构体按进程生命周期保留。 */
    (void)task;
}

void e_sched_task_resume(e_sched_task_handle_t task)
{
    e_sched_task_t* t = (e_sched_task_t*)task;
    if (!t || !t->gate_lock) return;
    SDL_LockMutex(t->gate_lock);
    t->suspended = 0;
    if (t->released) {
        SDL_SignalCondition(t->gate_cond);
    }
    SDL_UnlockMutex(t->gate_lock);
}

void e_sched_task_suspend(e_sched_task_handle_t task)
{
    e_sched_task_t* t = (e_sched_task_t*)task;
    if (!t || !t->gate_lock) return;
    SDL_LockMutex(t->gate_lock);
    t->suspended = 1;
    SDL_UnlockMutex(t->gate_lock);
}

void e_sched_task_delay_ms(uint32_t ms)
{
    SDL_Delay(ms);
}

uint32_t e_sched_time_ms(void)
{
    return (uint32_t)SDL_GetTicks();
}

uint8_t e_sched_current_is_interrupt_context(void)
{
    /* PC/SDL 无中断上下文，恒为 0 */
    return 0;
}

e_sched_stack_env_t e_sched_get_current_stack_env(void)
{
    if (scheduler && scheduler->is_started) {
        return E_SCHED_STACK_ENV_SYSTEM;
    }
    return E_SCHED_STACK_ENV_USER;
}

uint8_t e_sched_is_started(void)
{
    if (!scheduler) {
        return 0;
    }
    return scheduler->is_started;
}

void e_sched_yeild(void)
{
    SDL_Delay(0);
}

static int e_sched_init(void)
{
    if (scheduler) {
        return -1;
    }

    scheduler = (e_sched_t*)MALLOC(sizeof(e_sched_t));
    if (!scheduler) {
        ERROR_LOG("Failed to allocate scheduler structure");
        return -1;
    }
    memset(scheduler, 0, sizeof(*scheduler));

    scheduler->need_set_up_task_list = list_create(sizeof(e_sched_task_handle_t));
    if (!scheduler->need_set_up_task_list) {
        ERROR_LOG("Failed to create need-set-up task list");
        return -1;
    }

    scheduler->is_started = 0;
    INFO_LOG("E-scheduler initialized successfully (SDL3 backend)");
    return 0;
}

static int e_sched_deinit(void)
{
    return 0;
}

REGEDIT_FRAMEWORK_INIT_FUNCTION(e_sched_init, 0);
REGEDIT_FRAMEWORK_DEINIT_FUNCTION(e_sched_deinit, 0);

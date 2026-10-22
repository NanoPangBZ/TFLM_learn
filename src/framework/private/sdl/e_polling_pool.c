#include "framework/e_polling_pool.h"
#include "framework/e_sched.h"
#include "framework/c_list.h"
#include "framework/c_function_regedit.h"
#include "framework_cfg.h"
#include <string.h>

#include "framework/e_heap.h"
#define MALLOC(_size)          e_heap_malloc(_size,E_HEAP_ATTR_DEFAULT)
#define FREE(_ptr)             e_heap_free(_ptr)

#include "framework/e_log.h"
#define TAG "e_polling_pool"
#define DEBUG_LOG(...)  //E_DEBUG_LOG(TAG, __VA_ARGS__)
#define INFO_LOG(...)   //E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

//轮询任务池句柄
typedef struct e_polling_pool_t* e_polling_pool_handle_t;

//轮询任务标志
typedef enum e_polling_task_flag_e{
    E_POLLING_TASK_FLAG_NONE = 0x00,
    E_POLLING_TASK_FLAG_RUNNING = 0x01 << 0,            //任务正在运行
    E_POLLING_TASK_FLAG_STARTED = 0x01 << 1,            //任务已启动
    E_POLLING_TASK_FLAG_DELETE_PENDING = 0x01 << 2,     //任务删除中
}e_polling_task_flag_e;
typedef uint8_t e_polling_task_flag_t;

//轮询任务
typedef struct e_polling_task_t{
    e_polling_task_flag_t flag;     //任务标志
    uint32_t cycle_ms;              //轮询周期，0表示不定时轮询
    uint32_t last_run_time_ms;      //上次运行时间
    e_polling_pool_handle_t pool;   //任务所属的池

    void* user_ctx;
    void (*polling_func)(void* user_ctx);
}e_polling_task_t;

//轮询任务池
typedef struct e_polling_pool_t{
    list_handle_t polling_task_list;
    uint32_t tick_ms;            //池轮询间隔

    uint32_t need_recycle_task:1;
    uint32_t need_recycle_thread:1;
    uint32_t reserved:30;
}e_polling_pool_t;

//轮询任务池列表
static e_polling_pool_handle_t e_base_polling_pool = NULL;

e_polling_task_handle_t e_polling_task_create( e_polling_task_desc_t* desc )
{
    e_polling_task_t task;
    memset( &task, 0, sizeof(e_polling_task_t));
    task.pool = e_base_polling_pool;
    task.user_ctx = desc->user_ctx;
    task.polling_func = desc->polling_func;
    task.cycle_ms = (desc->freq_hz == 0) ? 0 : (1000 / desc->freq_hz);
    task.last_run_time_ms = 0;
    task.flag = desc->start_before_create ? E_POLLING_TASK_FLAG_STARTED : E_POLLING_TASK_FLAG_NONE;

    return list_add_element( e_base_polling_pool->polling_task_list , &task );
}

void e_polling_task_destroy( e_polling_task_handle_t handle )
{
    handle->flag |= E_POLLING_TASK_FLAG_DELETE_PENDING;
    e_base_polling_pool->need_recycle_task = 1;
}

void e_polling_task_start( e_polling_task_handle_t handle )
{
    handle->flag |= E_POLLING_TASK_FLAG_STARTED;
}

void e_polling_task_stop( e_polling_task_handle_t handle )
{
    handle->flag &= ~E_POLLING_TASK_FLAG_STARTED;
}

void e_polling_task_set_freq( e_polling_task_handle_t handle, uint32_t freq_hz )
{
    handle->cycle_ms = (freq_hz == 0) ? 0 : (1000 / freq_hz);
}

void e_polling_pool_set_freq( e_polling_task_handle_t handle, uint32_t freq_hz )
{
    handle->cycle_ms = (freq_hz == 0) ? 0 : (1000 / freq_hz);
}

static void e_polling_pool_thread(void* arg)
{
    e_polling_pool_t* pool = (e_polling_pool_t*)arg;
    
    while(1)
    {
        uint32_t now = e_sched_time_ms();
        foreach_list( pool->polling_task_list , task , e_polling_task_t )
        {
            if( now - task->last_run_time_ms >= task->cycle_ms && 
                (task->flag & E_POLLING_TASK_FLAG_STARTED) )
            {
                task->last_run_time_ms = now;
                task->flag |= E_POLLING_TASK_FLAG_RUNNING;
                task->polling_func( task->user_ctx );
                task->flag &= ~E_POLLING_TASK_FLAG_RUNNING;
            }
        }
        //回收删除中的任务
        if( pool->need_recycle_task )
        {
            foreach_list( pool->polling_task_list , task , e_polling_task_t )
            {
                if( task->flag & E_POLLING_TASK_FLAG_DELETE_PENDING )
                {
                    list_remove_element( pool->polling_task_list, task );
                }
            }
            pool->need_recycle_task = 0;
        }

        //回收删除中的线程
        if( pool->need_recycle_thread )
        {
            break; //退出线程，等待被回收
        }

        e_sched_task_delay_ms(e_base_polling_pool->tick_ms);
    }

    //线程退出前的清理工作
    list_destroyed( pool->polling_task_list );
    FREE(pool);

    //删除当前线程
    e_sched_task_delete( NULL );
}

static int e_polling_pool_init(void)
{   
    e_sched_task_desc_t task_desc;

    //创建基本池
    e_base_polling_pool = (e_polling_pool_handle_t)MALLOC(sizeof(e_polling_pool_t));
    if( e_base_polling_pool == NULL )
    {
        goto err_recycle;
    }
    memset(e_base_polling_pool, 0, sizeof(e_polling_pool_t));

    //为基本池创建任务列表
    e_base_polling_pool->polling_task_list = list_create( sizeof(e_polling_task_t) );
    if( e_base_polling_pool->polling_task_list == NULL )
    {
        goto err_recycle;
    }

    //设置基本池参数
    e_base_polling_pool->tick_ms = 1; //默认1ms轮询一次

    //创建基本池线程
    memset(&task_desc, 0, sizeof(task_desc));
    task_desc.name = "e_polling_pool";
    task_desc.task_entry = e_polling_pool_thread;
    task_desc.arg = e_base_polling_pool;
    task_desc.attr = E_SCHED_TASK_ATTR_DEFAULT | E_SCHED_TASK_ATTR_INDEPEND_INIT_THREAD;
    task_desc.stack_size_byte = E_POLLING_TASK_THREAD_STACK_SIZE;
    if( e_sched_task_create(&task_desc) == NULL )
    {
        goto err_recycle;
    }

    return 0;

err_recycle:
    //todo...
    return -1;
}

static int e_polling_pool_deinit(void)
{
    //标记所有任务删除中
    foreach_list( e_base_polling_pool->polling_task_list , task , e_polling_task_t )
    {
        if( task->flag & E_POLLING_TASK_FLAG_STARTED )
        {
            WARN_LOG("Polling task %p is still running", task);
            task->flag |= E_POLLING_TASK_FLAG_DELETE_PENDING;
        }
    }
    e_base_polling_pool->need_recycle_task = 1;

    //标记线程需要回收
    e_base_polling_pool->need_recycle_thread = 1;
	
	return 0;
}

REGEDIT_FRAMEWORK_INIT_FUNCTION(e_polling_pool_init, 1);
REGEDIT_FRAMEWORK_DEINIT_FUNCTION(e_polling_pool_deinit, 1);

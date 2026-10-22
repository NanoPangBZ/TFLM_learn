#include "framework/c_queue.h"
#include "framework_cfg.h"
#include "framework/e_heap.h"
#include "framework/e_mutex.h"
#include "framework/e_sched.h"
#include <string.h>

typedef struct c_queue_t{
    uint32_t item_size;      //每个元素的大小
    uint32_t queue_size;     //队列容量
    uint32_t head;           //队列头索引
    uint32_t tail;           //队列尾索引
    uint32_t count;          //当前元素数量
    uint8_t* buffer;        //存放队列数据的缓冲区

    e_mutex_handle_t mutex; //互斥锁
}c_queue_t;

c_queue_handle_t c_queue_create(c_queue_desc_t* desc)
{
    c_queue_t* queue = NULL;

    if(!desc)
    {
        return NULL; //无效参数
    }

    if( desc->item_size == 0 || desc->queue_size == 0 )
    {
        return NULL; //无效参数
    }

    queue = (c_queue_t*)e_heap_malloc(sizeof(c_queue_t), E_HEAP_ATTR_DEFAULT);
    if(!queue)
    {
        goto err_recycle; //内存分配失败
    }

    queue->buffer = (uint8_t*)e_heap_malloc(desc->item_size * desc->queue_size, E_HEAP_ATTR_DEFAULT);
    if(!queue->buffer){
        goto err_recycle; //内存分配失败
    }

    queue->mutex = e_mutex_create();
    if(!queue->mutex)
    {
        goto err_recycle; //互斥锁创建失败
    }

    queue->item_size = desc->item_size;
    queue->queue_size = desc->queue_size;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;

    return (c_queue_handle_t)queue;

err_recycle:
    if(queue)
    {
        if( queue->buffer )
        {
            e_heap_free(queue->buffer);
        }
        e_heap_free(queue);
    }
    return NULL;
}

void c_queue_destroy(c_queue_handle_t queue)
{
    (void)queue;
    //未实现
}

int c_queue_enqueue(c_queue_handle_t queue, const void* item)
{
    if(!queue || !item)
    {
        return -1; //无效参数
    }

    if(queue->count == queue->queue_size)
    {
        return -2; //队列已满
    }

    uint8_t is_interrupt_context = e_sched_current_is_interrupt_context();

    if( !is_interrupt_context )
    {
        e_mutex_lock(queue->mutex);
    }

    memcpy(&queue->buffer[queue->tail * queue->item_size], item, queue->item_size);
    queue->tail = (queue->tail + 1) % queue->queue_size;
    queue->count++;

    if( !is_interrupt_context )
    {
        e_mutex_unlock(queue->mutex);
    }

    return 0; //成功
}

int c_queue_dequeue(c_queue_handle_t queue, void* item)
{
    if(!queue || !item)
    {
        return -1; //无效参数
    }

    if(queue->count == 0)
    {
        return -2; //队列为空
    }

    uint8_t is_interrupt_context = e_sched_current_is_interrupt_context();

    if( !is_interrupt_context )
    {
        e_mutex_lock(queue->mutex);
    }

    memcpy(item, &queue->buffer[queue->head * queue->item_size], queue->item_size);
    queue->head = (queue->head + 1) % queue->queue_size;
    queue->count--;

    if( !is_interrupt_context )
    {
        e_mutex_unlock(queue->mutex);
    }

    return 0; //成功
}

uint8_t c_queue_is_empty(c_queue_handle_t queue)
{
    if(!queue)
    {
        return 1; //空
    }

    return queue->count == 0 ? 1 : 0;
}

uint8_t c_queue_is_full(c_queue_handle_t queue)
{
    if(!queue)
    {
        return 0; //未满
    }

    return queue->count == queue->queue_size ? 1 : 0;
}

void c_queue_clear(c_queue_handle_t queue)
{
    if(!queue)
    {
        return;
    }

    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
}

uint32_t c_queue_get_count(c_queue_handle_t queue)
{
    if(!queue)
    {
        return 0;
    }
    return queue->count;
}

uint16_t c_queue_get_element_size(c_queue_handle_t queue)
{
    if(!queue)
    {
        return 0;
    }
    return (uint16_t)(queue->item_size);
}

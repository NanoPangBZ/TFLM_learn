/* app_demo.c — 演示/验证应用：通过模块注册机制接入框架 APP_INIT 生命周期，
 * 一次性验证 c_queue / c_ringbuf / c_list / e_mutex / e_sig / e_kv / e_sched /
 * e_polling_pool 等框架组件在 SDL 后端下可编译、可运行、行为正确。 */
#include "framework/c_function_regedit.h"
#include "framework/e_log.h"
#include "framework/e_polling_pool.h"
#include "framework/e_sched.h"
#include "framework/e_heap.h"
#include "framework/c_queue.h"
#include "framework/c_ringbuf.h"
#include "framework/c_list.h"
#include "framework/e_mutex.h"
#include "framework/e_sig.h"
#include "framework/e_kv.h"

#include <string.h>

#define TAG "demo"
#define INFO_LOG(...)   E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)

static uint32_t g_tick = 0;

/* 周期轮询任务：每 1 秒打印一次，验证 e_polling_pool */
static void demo_polling(void* ctx)
{
    (void)ctx;
    g_tick++;
    INFO_LOG("polling tick=%u, uptime=%u ms", (unsigned)g_tick, (unsigned)e_sched_time_ms());
}

/* 信号等待任务：验证 e_sched 线程 + e_sig */
static void demo_sig_task(void* arg)
{
    e_sig_handle_t sig = (e_sig_handle_t)arg;
    INFO_LOG("sig task started, waiting for signal...");
    e_sig_wait(sig);
    INFO_LOG("sig task received signal, done.");
}

/* 验证 c_queue */
static void demo_queue(void)
{
    c_queue_desc_t desc = { .item_size = sizeof(int), .queue_size = 8 };
    c_queue_handle_t q = c_queue_create(&desc);
    if (!q) {
        WARN_LOG("c_queue_create failed");
        return;
    }

    for (int i = 0; i < 5; i++) {
        c_queue_enqueue(q, &i);
    }
    int v = 0;
    int count = 0;
    while (c_queue_dequeue(q, &v) == 0) {
        count++;
    }
    INFO_LOG("c_queue: pushed 5, popped %d, is_empty=%u", count, (unsigned)c_queue_is_empty(q));
}

/* 验证 c_ringbuf */
static void demo_ringbuf(void)
{
    c_ringbuf_desc_t desc = { .size = 16, .attr = C_RINGBUF_ATTR_DEFAULT };
    c_ringbuf_handle_t rb = c_ringbuf_create(&desc);
    if (!rb) {
        WARN_LOG("c_ringbuf_create failed");
        return;
    }

    const uint8_t* msg = (const uint8_t*)"hello ringbuf";
    uint32_t len = (uint32_t)strlen((const char*)msg);
    int written = c_ringbuf_write(rb, msg, len);
    uint8_t out[32] = {0};
    int read = c_ringbuf_read(rb, out, sizeof(out) - 1);
    out[read] = 0;
    INFO_LOG("c_ringbuf: write=%d read=%d content=\"%s\"", written, read, (const char*)out);
}

/* 验证 c_list + e_mutex */
static void demo_list_and_mutex(void)
{
    list_handle_t lst = list_create(sizeof(int));
    if (!lst) {
        WARN_LOG("list_create failed");
        return;
    }
    for (int i = 10; i < 15; i++) {
        list_add_element(lst, &i);
    }
    INFO_LOG("c_list: length=%u element_size=%u",
             (unsigned)list_get_length(lst), (unsigned)list_get_element_size(lst));

    e_mutex_handle_t m = e_mutex_create();
    if (m) {
        e_mutex_lock(m);
        e_mutex_unlock(m);
        INFO_LOG("e_mutex: lock/unlock ok");
        e_mutex_destroy(m);
    }
}

/* 验证 e_kv（文件后端） */
static void demo_kv(void)
{
    const char* greeting = "hello tflm kv";
    uint32_t w = e_kv_write_blob("user", "greeting", greeting, (uint32_t)strlen(greeting) + 1);
    char buf[64] = {0};
    uint32_t r = e_kv_read_blob("user", "greeting", buf, sizeof(buf));
    INFO_LOG("e_kv: write=%u read=%u exists=%u content=\"%s\"",
             (unsigned)w, (unsigned)r, (unsigned)e_kv_exists("user", "greeting"), buf);
}

static int demo_app_init(void)
{
    INFO_LOG("=== demo app init (SDL3 backend) ===");

    demo_queue();
    demo_ringbuf();
    demo_list_and_mutex();
    demo_kv();

    /* 创建一个等待信号的任务（调度器启动后释放） */
    e_sig_handle_t sig = e_sig_create();
    e_sched_task_desc_t td;
    memset(&td, 0, sizeof(td));
    td.name = "demo_sig";
    td.task_entry = demo_sig_task;
    td.arg = sig;
    td.attr = E_SCHED_TASK_ATTR_DEFAULT;
    td.stack_size_byte = 0;
    e_sched_task_create(&td);

    /* 预发信号（信号量计数累积，任务启动后即可获得） */
    e_sig_send(sig);

    /* 创建 1Hz 周期轮询任务 */
    e_polling_task_desc_t pd;
    memset(&pd, 0, sizeof(pd));
    pd.name = "demo_poll";
    pd.polling_func = demo_polling;
    pd.user_ctx = NULL;
    pd.freq_hz = 1;
    pd.start_before_create = 1;
    pd.attr = E_POLLING_TASK_ATTR_DEFAULT;
    e_polling_task_create(&pd);

    INFO_LOG("=== demo app init done ===");
    return 0;
}

REGEDIT_APP_INIT_FUNCTION(demo_app_init, 0);

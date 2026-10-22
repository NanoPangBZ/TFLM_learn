#include "framework/e_mutex.h"

#include <SDL3/SDL_mutex.h>

/* 参考 FreeRTOS 后端用二值信号量（非递归）实现互斥锁，
 * SDL3 的 SDL_Mutex 是递归锁，为保持语义一致，这里用初始值为 1 的
 * SDL_Semaphore 等价实现（非递归、可超时等待）。 */
struct e_mutex_t {
    SDL_Semaphore* sem;
};

e_mutex_handle_t e_mutex_create(void)
{
    struct e_mutex_t* m = (struct e_mutex_t*)SDL_calloc(1, sizeof(*m));
    if (!m) {
        return NULL;
    }
    m->sem = SDL_CreateSemaphore(1);
    if (!m->sem) {
        SDL_free(m);
        return NULL;
    }
    return (e_mutex_handle_t)m;
}

void e_mutex_destroy(e_mutex_handle_t mutex)
{
    struct e_mutex_t* m = (struct e_mutex_t*)mutex;
    if (!m) {
        return;
    }
    if (m->sem) {
        SDL_DestroySemaphore(m->sem);
    }
    SDL_free(m);
}

uint8_t e_mutex_try_lock(e_mutex_handle_t mutex, uint32_t timeout_ms)
{
    struct e_mutex_t* m = (struct e_mutex_t*)mutex;
    if (!m || !m->sem) {
        return 0;
    }
    /* timeout_ms < 0 表示无限等待，此处调用方传入的 uint32 恒为有限值 */
    return SDL_WaitSemaphoreTimeout(m->sem, (Sint32)timeout_ms) ? 1 : 0;
}

void e_mutex_lock(e_mutex_handle_t mutex)
{
    struct e_mutex_t* m = (struct e_mutex_t*)mutex;
    if (!m || !m->sem) {
        return;
    }
    SDL_WaitSemaphore(m->sem);
}

void e_mutex_unlock(e_mutex_handle_t mutex)
{
    struct e_mutex_t* m = (struct e_mutex_t*)mutex;
    if (!m || !m->sem) {
        return;
    }
    SDL_SignalSemaphore(m->sem);
}

void e_mutex_unlock_from_isr(e_mutex_handle_t mutex)
{
    /* PC 无 ISR，与普通解锁一致 */
    e_mutex_unlock(mutex);
}

#include "framework/e_sig.h"

#include <SDL3/SDL_mutex.h>

/* 参考 FreeRTOS 后端用二值信号量（初始 0）实现事件信号，
 * SDL3 下用 SDL_Semaphore 等价实现。e_sig_handle_t 为 void*，直接封装信号量指针。 */

e_sig_handle_t e_sig_create(void)
{
    SDL_Semaphore* sem = SDL_CreateSemaphore(0);
    return (e_sig_handle_t)sem;
}

void e_sig_destroy(e_sig_handle_t handle)
{
    if (handle) {
        SDL_DestroySemaphore((SDL_Semaphore*)handle);
    }
}

void e_sig_send(e_sig_handle_t handle)
{
    if (handle) {
        SDL_SignalSemaphore((SDL_Semaphore*)handle);
    }
}

void e_sig_send_from_isr(e_sig_handle_t handle)
{
    /* PC 无 ISR，与普通发送一致 */
    e_sig_send(handle);
}

void e_sig_wait(e_sig_handle_t handle)
{
    if (handle) {
        SDL_WaitSemaphore((SDL_Semaphore*)handle);
    }
}

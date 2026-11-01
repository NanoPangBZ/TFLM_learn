#include "framework/e_pend_sv.h"
#include "framework/c_function_regedit.h"

#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_thread.h>

/* e_pend_sv：延迟服务回调。参考后端由 PendSV 中断触发，在低优先级执行注册的回调。
 * SDL 后端用一条专用 worker 线程 + 信号量等价实现：trigger() 唤醒 worker，
 * worker 调用最近一次 register 的回调。 */

static void (*pend_sv_callback)(void) = NULL;
static SDL_Semaphore* pend_sv_sem = NULL;
static SDL_Thread* pend_sv_thread = NULL;
static SDL_AtomicInt pend_sv_running;

static int SDLCALL pend_sv_worker(void* p)
{
    (void)p;
    while (SDL_GetAtomicInt(&pend_sv_running)) {
        SDL_WaitSemaphore(pend_sv_sem);
        if (!SDL_GetAtomicInt(&pend_sv_running)) {
            break;
        }
        if (pend_sv_callback) {
            pend_sv_callback();
        }
    }
    return 0;
}

void e_pend_sv_trigger(void)
{
    if (pend_sv_sem) {
        SDL_SignalSemaphore(pend_sv_sem);
    }
}

void e_pend_sv_register(void (*function)(void))
{
    pend_sv_callback = function;
}

static int e_pend_sv_init(void)
{
    SDL_SetAtomicInt(&pend_sv_running, 1);
    pend_sv_sem = SDL_CreateSemaphore(0);
    if (!pend_sv_sem) {
        return -1;
    }
    pend_sv_thread = SDL_CreateThread(pend_sv_worker, "e_pend_sv", NULL);
    if (!pend_sv_thread) {
        SDL_DestroySemaphore(pend_sv_sem);
        pend_sv_sem = NULL;
        return -1;
    }
    SDL_DetachThread(pend_sv_thread);
    return 0;
}

static int e_pend_sv_deinit(void)
{
    SDL_SetAtomicInt(&pend_sv_running, 0);
    if (pend_sv_sem) {
        SDL_SignalSemaphore(pend_sv_sem);
        SDL_DestroySemaphore(pend_sv_sem);
        pend_sv_sem = NULL;
    }
    return 0;
}

REGEDIT_FRAMEWORK_INIT_FUNCTION(e_pend_sv_init, 1);
REGEDIT_FRAMEWORK_DEINIT_FUNCTION(e_pend_sv_deinit, 1);

/* e_critical.c — SDL 后端的临界区互斥锁。
 *
 * 参考 FreeRTOS 后端通过 vPortEnterCritical/vPortExitCritical（关中断）保护
 * e_heap / c_ringbuf 等非重入数据结构。SDL 多线程后端下用一把进程内递归
 * SDL_Mutex 等价实现，惰性创建、原子发布，保证线程安全。 */
#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_mutex.h>

static void* s_critical_mutex; /* 零初始化 == NULL，实际存 SDL_Mutex* */

SDL_Mutex* e_critical_mutex_get(void)
{
    SDL_Mutex* m = (SDL_Mutex*)SDL_GetAtomicPointer(&s_critical_mutex);
    if (m == NULL) {
        SDL_Mutex* created = SDL_CreateMutex();
        if (created == NULL) {
            return NULL;
        }
        /* 竞争失败则销毁自己新建的，使用胜者发布的锁 */
        if (!SDL_CompareAndSwapAtomicPointer(&s_critical_mutex, NULL, created)) {
            SDL_DestroyMutex(created);
            m = (SDL_Mutex*)SDL_GetAtomicPointer(&s_critical_mutex);
        } else {
            m = created;
        }
    }
    return m;
}

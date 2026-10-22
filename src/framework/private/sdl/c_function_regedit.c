#include "framework/c_function_regedit.h"
#include <stddef.h>

#include "framework/e_log.h"
#define TAG "c_function_regedit"
#define DEBUG_LOG(...)  //E_DEBUG_LOG(TAG, __VA_ARGS__)
#define INFO_LOG(...)   //E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

/* PC/SDL 后端：函数表用 GCC 构造函数 + 静态数组等价实现嵌入式链接段注册。
 * DEFINE_FUNCTION_ITEM 生成的构造函数在 main() 之前调用 c_function_regedit_add，
 * 将函数项指针追加到静态注册表；call_function_group 按组和优先级遍历调用。 */

#define C_FUNCTION_REGEDIT_MAX_ITEMS   256

static const function_item_t* s_items[C_FUNCTION_REGEDIT_MAX_ITEMS];
static uint32_t s_count = 0;

void c_function_regedit_add(const function_item_t* item)
{
    if (item != NULL && s_count < C_FUNCTION_REGEDIT_MAX_ITEMS) {
        s_items[s_count++] = item;
    }
}

int call_function_group(function_group_t group)
{
    int ret = 0;

    /* 按优先级 0..3 从高到低调用（0 最高） */
    for (uint8_t pri = 0; pri < 4; pri++) {
        for (uint32_t i = 0; i < s_count; i++) {
            const function_item_t* item = s_items[i];
            if (item->group == group && item->priority == pri) {
                if (item->function != NULL) {
                    DEBUG_LOG("Calling function '%s' in group %u with priority %u",
                              item->name ? item->name : "Unnamed",
                              (unsigned int)item->group,
                              (unsigned int)item->priority);
                    ret = item->function();
                    if (ret) {
                        ERROR_LOG("Function '%s' in group %u with priority %u failed with return code %d",
                                  item->name ? item->name : "Unnamed",
                                  (unsigned int)item->group,
                                  (unsigned int)item->priority,
                                  ret);
                        return ret;
                    }
                }
            }
        }
    }

    return ret;
}

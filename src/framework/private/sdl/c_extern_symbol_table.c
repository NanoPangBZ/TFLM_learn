#include "framework/c_extern_symbol_table.h"
#include "lib/symbol_table/symbol_table.h"
#include <string.h>

/* 外部符号表：参考后端通过 flash 内固定地址映射（C_EXTERN_SYMBOL_TABLE_MAP_TABLE）读取，
 * PC/SDL 后端改为直接读取由 EXPORT_SYMBOL 宏（section "__symbol_table"）组织的链接段，
 * 段边界使用 GNU ld 自动符号 __start_/__stop_，并放置哨兵项保证段恒存在。 */

#if defined(__GNUC__)
extern const symbol_item_t __start___symbol_table[];
extern const symbol_item_t __stop___symbol_table[];
#define SYMBOL_TABLE_START (__start___symbol_table)
#define SYMBOL_TABLE_END   (__stop___symbol_table)
#else
#error "c_extern_symbol_table: only GCC toolchain is supported by the SDL backend"
#endif

__attribute__((used, section("__symbol_table")))
static const symbol_item_t __symbol_table_sentinel = {
    .magic = 0,
    .name = {0},
    .symbol = NULL,
};

void* c_extern_symbol_get(const char* symbol_name)
{
    if (symbol_name == NULL) {
        return NULL;
    }

    for (const symbol_item_t* it = SYMBOL_TABLE_START; it < SYMBOL_TABLE_END; it++) {
        if (it->magic == SYMBOL_TABLE_ITEM_MAGIC && strcmp(it->name, symbol_name) == 0) {
            return it->symbol;
        }
    }
    return NULL;
}

uint32_t c_extern_symbol_table_symbol_num(void)
{
    uint32_t num = 0;
    for (const symbol_item_t* it = SYMBOL_TABLE_START; it < SYMBOL_TABLE_END; it++) {
        if (it->magic == SYMBOL_TABLE_ITEM_MAGIC) {
            num++;
        }
    }
    return num;
}

const char* c_extern_symbol_table_symbol_name(uint32_t index)
{
    uint32_t i = 0;
    for (const symbol_item_t* it = SYMBOL_TABLE_START; it < SYMBOL_TABLE_END; it++) {
        if (it->magic == SYMBOL_TABLE_ITEM_MAGIC) {
            if (i == index) {
                return it->name;
            }
            i++;
        }
    }
    return NULL;
}

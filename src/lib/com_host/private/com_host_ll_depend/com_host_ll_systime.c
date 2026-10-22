#include "com_host_ll_systime.h"
#include <stddef.h>

static uint32_t (*get_time_ms_func)() = NULL;

void com_host_ll_systime_init( uint32_t (*get_time_ms)() )
{
    get_time_ms_func = get_time_ms;
}

uint32_t com_host_ll_get_sys_time_ms()
{
    if( get_time_ms_func != NULL )
    {
        return get_time_ms_func();
    }
    return 0;
}

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void com_host_ll_systime_init( uint32_t (*get_time_ms)() );

uint32_t com_host_ll_get_sys_time_ms(void);

#ifdef __cplusplus
}
#endif


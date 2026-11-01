#pragma once

#include <stdint.h>
#include "lib/com_host/com_host.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 获取本地主机句柄
 * @return 本地主机句柄
*/
com_host_handle_t e_com_get_local_host(void);

#ifdef __cplusplus
}
#endif

#pragma once

// 环形缓冲区句柄
typedef struct com_host_ringbuf_t* com_host_ringbuf_handle_t;

#ifdef __cplusplus
 extern "C" {
#endif 

#include <stdint.h>

com_host_ringbuf_handle_t com_host_ringbuf_create(uint32_t size);
void com_host_ringbuf_destroy(com_host_ringbuf_handle_t ringbuf);

int com_host_ringbuf_write(com_host_ringbuf_handle_t ringbuf, const uint8_t* data, uint32_t size);
int com_host_ringbuf_read(com_host_ringbuf_handle_t ringbuf, uint8_t* data, uint32_t size);
uint8_t com_host_ringbuf_is_empty(com_host_ringbuf_handle_t ringbuf);
uint8_t com_host_ringbuf_is_full(com_host_ringbuf_handle_t ringbuf);
void com_host_ringbuf_clear(com_host_ringbuf_handle_t ringbuf);

#ifdef __cplusplus
}
#endif

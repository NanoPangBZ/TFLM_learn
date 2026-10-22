#include "framework/c_ringbuf.h"
#include <stddef.h>
#include <string.h>
#include "framework/e_heap.h"
#include "framework_cfg.h"
#include "framework/e_sched.h"

typedef struct c_ringbuf_t {
    uint8_t* buffer;        // 环形缓冲区的实际数据存储
    uint32_t size;          // 缓冲区大小
    uint32_t head;          // 写指针
    uint32_t tail;          // 读指针
} c_ringbuf_t;

c_ringbuf_handle_t c_ringbuf_create(c_ringbuf_desc_t* desc){
    if (desc->size == 0) {
        return NULL;
    }
    
    e_heap_attr_t mem_attr = E_HEAP_ATTR_DEFAULT;
    if (desc->attr & C_RINGBUF_ATTR_FAST_ACCESS) {
        mem_attr |= E_HEAP_ATTR_FAST_ACCESS;
    }

    c_ringbuf_t* ringbuf = (c_ringbuf_t*)e_heap_malloc(sizeof(c_ringbuf_t),mem_attr);
    if (!ringbuf) {
        return NULL;
    }

    ringbuf->buffer = (uint8_t*)e_heap_malloc(desc->size, mem_attr);
    if (!ringbuf->buffer) {
        e_heap_free(ringbuf);
        return NULL;
    }

    ringbuf->size = desc->size;
    ringbuf->head = 0;
    ringbuf->tail = 0;
    
    return (c_ringbuf_handle_t)ringbuf;
}

void c_ringbuf_destroy(c_ringbuf_handle_t handle) {
    if (!handle) {
        return;
    }
    
    c_ringbuf_t* ringbuf = (c_ringbuf_t*)handle;
    if (ringbuf->buffer) {
        e_heap_free(ringbuf->buffer);
    }
    e_heap_free(ringbuf);
}

int c_ringbuf_write(c_ringbuf_handle_t handle, const uint8_t* data, uint32_t len) {
    if (!handle || !data || len == 0) {
        return -1;
    }
    
    ENTER_CRITICAL();
    c_ringbuf_t* ringbuf = (c_ringbuf_t*)handle;
    uint16_t free_space = (ringbuf->tail + ringbuf->size - ringbuf->head - 1) % ringbuf->size;
    uint32_t write_len = (len > free_space) ? free_space : len;

    if (write_len == 0) {
        EXIT_CRITICAL();
        return 0;
    }

    // Handle wrap-around case
    if (ringbuf->head + write_len > ringbuf->size) {
        // Split into two parts
        uint32_t first_part = ringbuf->size - ringbuf->head;
        uint32_t second_part = write_len - first_part;
        
        memcpy(&ringbuf->buffer[ringbuf->head], data, first_part);
        memcpy(&ringbuf->buffer[0], data + first_part, second_part);
    } else {
        // Single copy
        memcpy(&ringbuf->buffer[ringbuf->head], data, write_len);
    }

    ringbuf->head = (ringbuf->head + write_len) % ringbuf->size;
    EXIT_CRITICAL();

    return write_len;
}

int c_ringbuf_read(c_ringbuf_handle_t handle, uint8_t* data, uint32_t len) {
    if (!handle || !data || len == 0) {
        return 0;
    }
    
    c_ringbuf_t* ringbuf = (c_ringbuf_t*)handle;
    uint32_t available_data = (ringbuf->head + ringbuf->size - ringbuf->tail) % ringbuf->size;
    uint32_t read_len = (len > available_data) ? available_data : len;

    if (read_len == 0) {
        return 0;
    }

    ENTER_CRITICAL();
    // Handle wrap-around case
    if (ringbuf->tail + read_len > ringbuf->size) {
        // Split into two parts
        uint32_t first_part = ringbuf->size - ringbuf->tail;
        uint32_t second_part = read_len - first_part;
        
        memcpy(data, &ringbuf->buffer[ringbuf->tail], first_part);
        memcpy(data + first_part, &ringbuf->buffer[0], second_part);
    } else {
        // Single copy
        memcpy(data, &ringbuf->buffer[ringbuf->tail], read_len);
    }

    ringbuf->tail = (ringbuf->tail + read_len) % ringbuf->size;
    EXIT_CRITICAL();

    return read_len;
}

int c_ringbuf_write_from_isr(c_ringbuf_handle_t handle, const uint8_t* data, uint32_t len)
{
    if (!handle || !data || len == 0) {
        return -1;
    }
    
    ENTER_CRITICAL_FROM_ISR();
    c_ringbuf_t* ringbuf = (c_ringbuf_t*)handle;
    uint16_t free_space = (ringbuf->tail + ringbuf->size - ringbuf->head - 1) % ringbuf->size;
    uint32_t write_len = (len > free_space) ? free_space : len;

    if (write_len == 0) {
        EXIT_CRITICAL_FROM_ISR();
        return 0;
    }

    // Handle wrap-around case
    if (ringbuf->head + write_len > ringbuf->size) {
        // Split into two parts
        uint32_t first_part = ringbuf->size - ringbuf->head;
        uint32_t second_part = write_len - first_part;
        
        memcpy(&ringbuf->buffer[ringbuf->head], data, first_part);
        memcpy(&ringbuf->buffer[0], data + first_part, second_part);
    } else {
        // Single copy
        memcpy(&ringbuf->buffer[ringbuf->head], data, write_len);
    }

    ringbuf->head = (ringbuf->head + write_len) % ringbuf->size;
    EXIT_CRITICAL_FROM_ISR();

    return write_len;
}

int c_ringbuf_read_from_isr(c_ringbuf_handle_t handle, uint8_t* data, uint32_t len)
{
    if (!handle || !data || len == 0) {
        return 0;
    }
    
    c_ringbuf_t* ringbuf = (c_ringbuf_t*)handle;
    uint32_t available_data = (ringbuf->head + ringbuf->size - ringbuf->tail) % ringbuf->size;
    uint32_t read_len = (len > available_data) ? available_data : len;

    if (read_len == 0) {
        return 0;
    }

    ENTER_CRITICAL_FROM_ISR();
    // Handle wrap-around case
    if (ringbuf->tail + read_len > ringbuf->size) {
        // Split into two parts
        uint32_t first_part = ringbuf->size - ringbuf->tail;
        uint32_t second_part = read_len - first_part;
        
        memcpy(data, &ringbuf->buffer[ringbuf->tail], first_part);
        memcpy(data + first_part, &ringbuf->buffer[0], second_part);
    } else {
        // Single copy
        memcpy(data, &ringbuf->buffer[ringbuf->tail], read_len);
    }

    ringbuf->tail = (ringbuf->tail + read_len) % ringbuf->size;
    EXIT_CRITICAL_FROM_ISR();

    return read_len;
}

uint8_t c_ringbuf_is_empty(c_ringbuf_handle_t handle) {
    if (!handle) {
        return 1;
    }
    
    c_ringbuf_t* ringbuf = (c_ringbuf_t*)handle;
    return ringbuf->head == ringbuf->tail ? 1 : 0;
}

uint8_t c_ringbuf_is_full(c_ringbuf_handle_t handle) {
    if (!handle) {
        return 0;
    }
    
    c_ringbuf_t* ringbuf = (c_ringbuf_t*)handle;
    return ((ringbuf->head + 1) % ringbuf->size) == ringbuf->tail ? 1 : 0;
}

uint32_t c_ringbuf_get_size(c_ringbuf_handle_t handle) {
    if (!handle) {
        return 0;
    }
    
    c_ringbuf_t* ringbuf = (c_ringbuf_t*)handle;
    return ringbuf->size;
}

void c_ringbuf_clear(c_ringbuf_handle_t handle) {
    if (!handle) {
        return;
    }
    
    c_ringbuf_t* ringbuf = (c_ringbuf_t*)handle;
    ringbuf->head = 0;
    ringbuf->tail = 0;
}



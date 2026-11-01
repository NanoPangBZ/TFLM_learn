#include "com_host_packager.h"
#include "./com_host_package/com_host_packager_impl_api_def.h"
#include <string.h>

#include "./com_host_package/com_host_your_way_studio_packager.h"

#include "com_host_ll_depend/com_host_ll_heap.h"
#define MALLOC(_size)       com_host_ll_heap_malloc(_size)
#define FREE(_ptr)          com_host_ll_heap_free(_ptr)

#include "../com_host_cfg.h"
#define INFO_LOG(...)   COM_HOST_INFO_LOG(__VA_ARGS__)
#define WARN_LOG(...)   COM_HOST_WARN_LOG(__VA_ARGS__)
#define ERROR_LOG(...)  COM_HOST_ERROR_LOG(__VA_ARGS__)

typedef struct com_host_packager_t{
    com_host_packager_impl_handle_t impl_handle;
    com_host_packager_api_t*        api;
}com_host_packager_t;

com_host_packager_handle_t com_host_packager_create( com_host_packager_desc_t* desc )
{
    com_host_packager_t* packager = MALLOC(sizeof(com_host_packager_t));
    if (!packager) {
        ERROR_LOG("Failed to allocate memory for packager");
        return NULL;
    }
    memset(packager, 0, sizeof(com_host_packager_t));

    // 根据desc->type选择具体的打包器实现
    switch (desc->type)
    {
        case COM_HOST_PORT_ATTR_PACK_PROTOCOL_YOUR_WAY_STUDIO:
            packager->api = get_your_way_studio_packager_api();
            break;
        default:
            WARN_LOG("Unsupported packager type: %d", desc->type);
            goto err_recycle;
    }

    if (!packager->api) {
        ERROR_LOG("Packager API is NULL");
        goto err_recycle;
    }

    packager->impl_handle = packager->api->create(desc);
    if (!packager->impl_handle) {
        ERROR_LOG("Failed to create packager implementation");
        goto err_recycle;
    }

    return (com_host_packager_handle_t)packager;

err_recycle:
    ERROR_LOG("Failed to create packager(type=%d)", desc->type);
    //@todo...
    return NULL;
}

void com_host_packager_destroy( com_host_packager_handle_t packager )
{
    if (!packager) {
        return;
    }
    if (packager->api && packager->impl_handle) {
        packager->api->destroy(packager->impl_handle);
    }
    FREE(packager);
}

void com_host_packager_reset( com_host_packager_handle_t packager )
{
    if( packager == NULL )
    {
        return;
    }

    //假设每个打包器实现都有一个reset函数
    if( packager->api && packager->impl_handle && packager->api->reset )
    {
        packager->api->reset( packager->impl_handle );
    }
}

void com_host_packager_register_unpack_cb( com_host_packager_handle_t packager,
                                        void* user_ctx ,
                                        void (*unpack_cb)(void* user_ctx, msg_package_t* pkg) )
{
    if( packager == NULL || unpack_cb == NULL )
    {
        return;
    }

    packager->api->register_unpack_cb( packager->impl_handle, user_ctx, unpack_cb );
}

uint8_t* com_host_packager_pack( com_host_packager_handle_t packager, msg_package_t* pkgs , uint8_t pkgs_cnt,  uint32_t* out_size )
{
    return packager->api->pack( packager->impl_handle, pkgs, pkgs_cnt, out_size );
}

void com_host_packager_unpack( com_host_packager_handle_t packager, uint8_t* raw_data, uint16_t size )
{
    packager->api->unpack( packager->impl_handle, raw_data, size );
}

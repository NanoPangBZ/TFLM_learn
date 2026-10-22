#include "framework_cfg.h"
#include "framework/e_com.h"

#if defined(E_COM_HOST_ENABLE) && (E_COM_HOST_ENABLE == 1)

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "lib/com_host/com_host.h"
#include "framework/c_function_regedit.h"
#include "framework/e_polling_pool.h"
#include "framework/e_sched.h"
#include "framework/e_framework_core.h"

#include "version.h"
#include "git_info.h"

#include "framework/e_log.h"
#define TAG "e_com"
#define DEBUG_LOG(...)  //E_DEBUG_LOG(TAG, __VA_ARGS__)
#define INFO_LOG(...)   //E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)   E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...)  E_ERROR_LOG(TAG, __VA_ARGS__)

#ifndef E_COM_HOST_ID
    #warning "E_COM_HOST_ID is not defined, using default value COM_HOST_ID_MAIN_CONTROLLER"
    #define E_COM_HOST_ID COM_HOST_ID_MAIN_CONTROLLER
#endif

#ifndef E_COM_HOST_STACK_SIZE
    #warning "E_COM_HOST_STACK_SIZE is not defined, using default value 12 * 1024"
    #define E_COM_HOST_STACK_SIZE (12 * 1024)
#endif

#ifndef E_COM_HOST_STACK_ATTR
    #warning "E_COM_HOST_STACK_ATTR is not defined, using default value __attribute__((aligned(4)))"
    #define E_COM_HOST_STACK_ATTR __attribute__((aligned(4)))
#endif

static E_COM_HOST_STACK_ATTR uint8_t com_host_stack_buffer[E_COM_HOST_STACK_SIZE];

static e_polling_task_handle_t com_host_polling_task_handle = NULL;
static com_host_handle_t local_host = NULL;

/**
 * @brief 通信主机轮询任务
 * @param args 通信主机句柄
 * @note 通信主机轮询任务，用于运行通信主机
*/
static void com_host_polling_task(void* args)
{
    com_host_handle_t host = (com_host_handle_t)args;
    com_host_run( host );
}

com_host_handle_t e_com_get_local_host(void)
{
    return local_host;
}


/**
 * @brief 基础消息处理函数
 * @param host 通信主机句柄
 * @param port 通信端口句柄
 * @param sender_host_id 发送主机ID
 * @param msg 通信消息
 * @param seq 消息序列号
 * @param cb_ctx 用户上下文指针
*/
static void com_host_basic_msg_handler( com_host_handle_t host,
    com_host_port_handle_t port,
    com_host_id_t sender_host_id,
    com_host_msg_t* msg ,
    uint8_t seq ,
    void* cb_ctx )
{
    switch( msg->msg_id )
    {
        /* 响应ping命令 */
        case COM_HOST_MSG_ID_PING:
            {
                com_host_msg_ping_ack_data_t ack_data;
                memset(&ack_data, 0, sizeof(ack_data));
                ack_data.uptime = e_sched_time_ms();
                com_host_send_ack(host, port, sender_host_id, msg, (uint8_t*)&ack_data, sizeof(ack_data), seq);
            }
            break;
        
        /* 响应请求设备信息命令 */
        case COM_HOST_MSG_ID_REQ_DEVICE_INFO:
            {
                uint8_t ack_data_buf[ sizeof(com_host_msg_req_device_info_ack_data_t) + 128 ];
                com_host_msg_req_device_info_ack_data_t* ack_data = (com_host_msg_req_device_info_ack_data_t*)ack_data_buf;
                memset(ack_data_buf, 0, sizeof(ack_data_buf));

                ack_data->architecture = COM_HOST_MSG_ARCH_ARM_CORTEX_M;
                strncpy((char*)ack_data->device_name, PRODUCT_NAME, sizeof(ack_data->device_name));
                framework_get_hd_version(&ack_data->hardware_version.major, &ack_data->hardware_version.minor, &ack_data->hardware_version.patch);
                ack_data->firmware_version.major = MAJOR_VERSION;
                ack_data->firmware_version.minor = MINOR_VERSION;
                ack_data->firmware_version.patch = PATCH_VERSION;
                sscanf(FIRWARE_BUILD_DATE, "%hu-%hhu-%hhu", &ack_data->build_date.year, &ack_data->build_date.month, &ack_data->build_date.day);
                sscanf(FIRWARE_BUILD_TIME, "%hhu:%hhu:%hhu", &ack_data->build_date.hour, &ack_data->build_date.minute, &ack_data->build_date.second);
                ack_data->is_released = VERSION_IS_RELEASE;
                ack_data->is_demo = VERSION_IS_DEMO;
                ack_data->rsvd_bits = 0;    
                strncpy((char*)ack_data->git_branch, GIT_BRANCH_NAME, sizeof(ack_data->git_branch));
                sscanf(GIT_COMMIT_ID, "%16s", ack_data->git_commit_hash);
                sscanf(GIT_COMMIT_DATE, "%hu-%hhu-%hhu", &ack_data->git_commit_date.year, &ack_data->git_commit_date.month, &ack_data->git_commit_date.day);
                sscanf(GIT_COMMIT_TIME, "%hhu:%hhu:%hhu", &ack_data->git_commit_date.hour, &ack_data->git_commit_date.minute, &ack_data->git_commit_date.second);
                strncpy((char*)ack_data->board_name, BOARD_NAME, sizeof(ack_data->board_name));
                ack_data->brief_len = strlen(BRIEF_DESCRIPTION);
                strncpy( (char*)ack_data->brief, BRIEF_DESCRIPTION, 128 );

                com_host_send_ack(host, port, sender_host_id, msg, (uint8_t*)ack_data_buf, sizeof(com_host_msg_req_device_info_ack_data_t) + ack_data->brief_len , seq);
            }
            break;
        
        /* 响应回显测试命令 */
        case COM_HOST_MSG_ID_CMD_ECHO_TEST:
            {
                uint8_t ack_data_buf[ sizeof(com_host_msg_cmd_echo_test_ack_data_t) + msg->data_len - 1 ];
                com_host_msg_cmd_echo_test_ack_data_t* ack_data = (com_host_msg_cmd_echo_test_ack_data_t*)ack_data_buf;
                memset(ack_data_buf, 0, sizeof(ack_data_buf));

                ack_data->result = COM_HOST_MSG_RET_OK;
                //原数据返回
                memcpy(ack_data->test_data, msg->data, msg->data_len);

                com_host_send_ack(host, port, sender_host_id, msg, (uint8_t*)ack_data_buf, sizeof(com_host_msg_cmd_echo_test_ack_data_t) + msg->data_len - 1 , seq);
            }
            break;
    }
}


/**
 * @brief 基础消息响应初始化
*/
static int e_com_base_msg_rsp_init(void)
{
#if defined(E_COM_BASE_MSG_RSP_ENABLE) && (E_COM_BASE_MSG_RSP_ENABLE == 1)
    const com_host_msg_id_t base_msg_id_list[] = {
        COM_HOST_MSG_ID_PING,
        COM_HOST_MSG_ID_REQ_DEVICE_INFO,
        COM_HOST_MSG_ID_CMD_ECHO_TEST,
    };

    for( int i = 0; i < sizeof(base_msg_id_list) / sizeof(base_msg_id_list[0]); i++ )
    {
        com_host_msg_id_t msg_id = base_msg_id_list[i];
        if( com_host_add_msg_handler( local_host , msg_id , com_host_basic_msg_handler, NULL ) )
        {
            ERROR_LOG("add msg handler for msg_id(0x%04X) failed", msg_id);
            return -1;
        }
    }

    return 0;
#else
    return 0;
#endif
}

/**
 * @brief e_com初始化
 * @return 0:成功 其他:失败
*/
static int e_com_init(void)
{
    //初始化com_host库
    com_host_ll_depend_t ll_depend = {
        .get_time_ms = e_sched_time_ms,
        .stack_align = 4,
        .stack_buff = com_host_stack_buffer,
        .stack_buff_size = sizeof(com_host_stack_buffer),
    };
    com_host_init( &ll_depend );

    //创建本地主机
    com_host_desc_t host_desc = {
        .host_id = E_COM_HOST_ID,
    };
    local_host = com_host_create( &host_desc );
    if( local_host == NULL )
    {
        ERROR_LOG("create local host failed");
        return -1;
    }

    //创建端口
    //...

    //添加路由
    //...

    //初始化基础消息响应
    if( e_com_base_msg_rsp_init() )
    {
        ERROR_LOG("init base msg rsp failed");
        return -2;
    }

    //创建轮询任务
    e_polling_task_desc_t task_desc = {
        .attr = E_POLLING_TASK_ATTR_DEFAULT,
        .freq_hz = E_COM_HOST_RUN_FREQ_HZ,
        .name = TAG,
        .polling_func = com_host_polling_task,
        .start_before_create = 1,
        .user_ctx = local_host,
    };
    com_host_polling_task_handle = e_polling_task_create( &task_desc );
    if( com_host_polling_task_handle == NULL )
    {
        ERROR_LOG("create com host polling task failed");
        return -3;
    }

    INFO_LOG("local host created success");

    return 0;
}

/**
 * @brief e_com反初始化
 * @return 0:成功 其他:失败
*/
static int e_com_deinit(void)
{
    if( com_host_polling_task_handle != NULL )
    {
        e_polling_task_destroy( com_host_polling_task_handle );
        com_host_polling_task_handle = NULL;
    }

    if( local_host != NULL )
    {
        com_host_destroy( local_host );
        local_host = NULL;
    }

    INFO_LOG("e_com deinit success");
    return 0;
}

REGEDIT_FRAMEWORK_INIT_FUNCTION(e_com_init, 2);
REGEDIT_FRAMEWORK_DEINIT_FUNCTION(e_com_deinit, 2);

#else

#include <stddef.h>

com_host_handle_t e_com_get_local_host(void)
{
    return NULL;
}

#endif

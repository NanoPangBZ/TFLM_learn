#include "framework/e_online_monitor.h"

#if defined(ENABLE_E_ONLINE_MONITOR) && (ENABLE_E_ONLINE_MONITOR > 0)

#include "framework/c_list.h"
#include "framework/e_com.h"
#include "framework/e_polling_pool.h"
#include "framework/e_sched.h"
#include "framework/c_function_regedit.h"
#include <string.h>

#define DEBUG_LOG(...)
#define INFO_LOG(...)
#define WARN_LOG(...)
#define ERROR_LOG(...)

// 注册信息结构体定义
typedef struct value_reg_info_t{
    char name[16];
    uint8_t variable_id;
    const void* p_value;
    e_online_monitor_value_type_t value_type;
}value_reg_info_t;

// 订阅信息结构体定义
typedef struct subscribe_info_t{
    com_host_id_t       host_id;
}subscribe_info_t;

// 变量组结构体定义
typedef struct monitor_gruop_t{
    char            gruop_name[16];
    list_handle_t   reg_value_info_list;    // value_reg_info_t 列表
    list_handle_t   subscribe_info_list;    // subscribe_info_t 列表
    uint16_t push_cycle_ms;         // 推送周期，单位毫秒
    uint32_t last_push_time_ms;     // 上次推送时间，单位毫秒
    uint8_t group_id;               // 变量组ID
}monitor_gruop_t;

// 在线监视器结构体定义
typedef struct e_online_monitor_t{
    list_handle_t monitor_group_list;       // monitor_gruop_t 列表
}e_online_monitor_t;

static e_online_monitor_t g_online_monitor = {0};

/**
 * @brief 填充变量值数组
 * @param reg_value_info_list 已注册变量信息列表
 * @param buf 目标缓冲区
 * @param buf_size 目标缓冲区大小
 * @param out_filled_size 输出实际填充大小指针
*/
static void full_value_array( list_handle_t reg_value_info_list , uint8_t* buf , uint32_t buf_size , uint32_t* out_filled_size )
{
    uint8_t* p_buf = buf;
    uint32_t remaining_size = buf_size;

    foreach_list( reg_value_info_list , value_info , value_reg_info_t )
    {
        const uint32_t size_map[] = {
            sizeof(int8_t),
            sizeof(uint8_t),
            sizeof(int16_t),
            sizeof(uint16_t),
            sizeof(int32_t),
            sizeof(uint32_t),
            sizeof(float)
        };

        uint32_t value_size = size_map[value_info->value_type];

        if( remaining_size < value_size )
        {
            //空间不足，退出
            break;
        }

        switch( value_info->value_type )
        {
            case E_ONLINE_MONITOR_VALUE_TYPE_FLOAT:
                *(float*)p_buf = *(float*)value_info->p_value;
            break;

            case E_ONLINE_MONITOR_VALUE_TYPE_UINT8:
                *(uint8_t*)p_buf = *(uint8_t*)value_info->p_value;
            break;

            case E_ONLINE_MONITOR_VALUE_TYPE_UINT16:
                *(uint16_t*)p_buf = *(uint16_t*)value_info->p_value;
            break;

            case E_ONLINE_MONITOR_VALUE_TYPE_UINT32:
                *(uint32_t*)p_buf = *(uint32_t*)value_info->p_value;
            break;

            case E_ONLINE_MONITOR_VALUE_TYPE_INT8:
                *(int8_t*)p_buf = *(int8_t*)value_info->p_value;
            break;

            case E_ONLINE_MONITOR_VALUE_TYPE_INT16:
                *(int16_t*)p_buf = *(int16_t*)value_info->p_value;
            break;

            case E_ONLINE_MONITOR_VALUE_TYPE_INT32:
                *(int32_t*)p_buf = *(int32_t*)value_info->p_value;
            break;

            default:
                //不支持的类型，跳过
            break;
        }

        p_buf += value_size;
        remaining_size -= value_size;
    }

    *out_filled_size = buf_size - remaining_size;
}

/**
 * @brief 推送变量组数据
 * @param gruop 变量组指针
*/
static void push_gruop_data( monitor_gruop_t* gruop )
{
    //准备msg_data
    uint8_t msg_data_buf[256];
    com_host_msg_push_monitored_group_data_t* msg_data = (com_host_msg_push_monitored_group_data_t*)msg_data_buf;
    msg_data->group_id = gruop->group_id;
    msg_data->timestamp_ms = e_sched_time_ms();
    msg_data->var_data_array[0] = 0; //变量数据起始位置

    uint8_t* p_var_data = msg_data->var_data_array;

    //填充变量数据
    uint32_t filled_size = 0;
    full_value_array( gruop->reg_value_info_list , p_var_data , sizeof(msg_data_buf) - sizeof(com_host_msg_push_monitored_group_data_t) + 1 , &filled_size );

    //准备msg
    com_host_msg_t msg;
    msg.msg_id = COM_HOST_MSG_ID_PUSH_MONITORED_GROUP;
    msg.data = (uint8_t*)msg_data;
    msg.data_len = sizeof(com_host_msg_push_monitored_group_data_t) - 1 + filled_size;
    msg.need_ack = 0;
    msg.is_ack = 0;

    if( gruop->group_id == 0 )
    {
        DEBUG_LOG( "%.1f , %d " ,
            *(float*)(msg_data_buf + sizeof(com_host_msg_push_monitored_group_data_t) - 1),
            *(int32_t*)(msg_data_buf + sizeof(com_host_msg_push_monitored_group_data_t) - 1 + sizeof(float)) );
    }

    //发布
    foreach_list( gruop->subscribe_info_list , subscribe , subscribe_info_t )
    {
        com_host_send_msg( e_com_get_local_host() , subscribe->host_id , &msg , NULL );
    }
}

/**
 * @brief 在线监视器轮询任务
 * @param args 任务参数
*/
static void polling_task(void* args)
{
    (void)args;

    uint32_t now = e_sched_time_ms();

    foreach_list( g_online_monitor.monitor_group_list , gruop , monitor_gruop_t )
    {
        if( ( now - gruop->last_push_time_ms ) >= gruop->push_cycle_ms )
        {
            //到达推送时间，推送数据
            push_gruop_data( gruop );
            gruop->last_push_time_ms = now;
        }
    }
}

int16_t e_online_monitor_register_value( const char* gruop , const char* name , const void* p_value , e_online_monitor_value_type_t value_type )
{
    value_reg_info_t reg_info = {
        .name = {0},
        .p_value = p_value,
        .value_type = value_type
    };
    strncpy( reg_info.name , name , sizeof(reg_info.name) - 1 );
    reg_info.name[sizeof(reg_info.name) - 1] = '\0';

    //查找组
    monitor_gruop_t* monitor_gruop = NULL;

    foreach_list( g_online_monitor.monitor_group_list , it , monitor_gruop_t )
    {
        if( strcmp( it->gruop_name , gruop ) == 0 )
        {
            monitor_gruop = it;
            break;
        }
    }

    //不存在则创建组
    if( monitor_gruop == NULL )
    {
        //创建组
        monitor_gruop_t new_group;
        memset( &new_group , 0 , sizeof(new_group) );
        strncpy( new_group.gruop_name , gruop , sizeof(new_group.gruop_name) - 1 );
        new_group.gruop_name[sizeof(new_group.gruop_name) - 1] = '\0';
        new_group.push_cycle_ms = 1000;         //默认推送周期1秒
        new_group.group_id = (uint8_t)list_get_length( g_online_monitor.monitor_group_list );

        // 创建变量注册信息列表
        new_group.reg_value_info_list = list_create( sizeof(value_reg_info_t) );
        if( new_group.reg_value_info_list == NULL )
        {
            ERROR_LOG( "Failed to create monitor group list for group '%s'" , gruop );
            return -1;
        }

        // 创建订阅信息列表
        new_group.subscribe_info_list = list_create( sizeof(subscribe_info_t) );
        if( new_group.subscribe_info_list == NULL )
        {
            ERROR_LOG( "Failed to create subscribe info list for group '%s'" , gruop );
            list_destroyed( new_group.reg_value_info_list );
            return -1;
        }

        // 添加到在线监视器组列表
        monitor_gruop = list_add_element( g_online_monitor.monitor_group_list , &new_group );;
        if( monitor_gruop == NULL )
        {
            ERROR_LOG( "Failed to add new monitor group '%s' to group list" , gruop );
            list_destroyed( new_group.reg_value_info_list );
            list_destroyed( new_group.subscribe_info_list );
            return -2;
        }
    }

    reg_info.variable_id = (uint8_t)list_get_length( monitor_gruop->reg_value_info_list );

    //添加变量注册信息
    if( list_add_element( monitor_gruop->reg_value_info_list , &reg_info ) == NULL )
    {
        ERROR_LOG( "Failed to add register value '%s' to group '%s'" , name , gruop );
        return -3;
    }

    INFO_LOG( "e_online_monitor: Registered value '%s' to group '%s'" , name , gruop );

    return 0;
}

static void com_host_msg_handler( com_host_handle_t host,
                                  com_host_port_handle_t port,
                                  com_host_id_t sender_host_id,
                                  com_host_msg_t* msg ,
                                  uint8_t seq ,
                                  void* cb_ctx )
{
    if( msg->is_ack )
    {
        return;
    }

    uint8_t ack_data_buf[512];
    memset( ack_data_buf , 0 , sizeof(ack_data_buf) );

    switch( msg->msg_id )
    {
        case COM_HOST_MSG_ID_REQ_MONITORED_GROUP_LIST:
        {
            com_host_msg_req_monitored_group_list_ack_data_t* ack_data = (com_host_msg_req_monitored_group_list_ack_data_t*)ack_data_buf;
            ack_data->result = COM_HOST_MSG_RET_OK;
            ack_data->group_count = (uint8_t)list_get_length( g_online_monitor.monitor_group_list );

            com_host_msg_monitored_group_brief_t* p_brief = ack_data->brief;

            //填充变量组简要信息
            foreach_list( g_online_monitor.monitor_group_list , gruop , monitor_gruop_t )
            {
                p_brief->group_id = gruop->group_id;
                uint8_t name_len = (uint8_t)strlen( gruop->gruop_name );
                p_brief->name_len = name_len;
                strncpy( (char*)p_brief->name , gruop->gruop_name , name_len );
                p_brief = (com_host_msg_monitored_group_brief_t*)( (uint8_t*)p_brief + sizeof(com_host_msg_monitored_group_brief_t) - 1 + name_len );
            }

            //发送ack
            com_host_send_ack( host , port , sender_host_id , msg , (uint8_t*)ack_data , (uint16_t)( (uint8_t*)p_brief - ack_data_buf ) , seq );
            break;
        }

        case COM_HOST_MSG_ID_REQ_MONITORED_GRUOP_INFO:
        {
            com_host_msg_req_monitored_gruop_info_data_t* req_data = (com_host_msg_req_monitored_gruop_info_data_t*)msg->data;
            com_host_msg_req_monitored_gruop_info_ack_data_t* ack_data = (com_host_msg_req_monitored_gruop_info_ack_data_t*)ack_data_buf;
            
            monitor_gruop_t* monitor_gruop = NULL;
        
            foreach_list( g_online_monitor.monitor_group_list , it , monitor_gruop_t )
            {
                if( it->group_id == req_data->group_id )
                {
                    monitor_gruop = it;
                    break;
                }
            }

            if( monitor_gruop == NULL )
            {
                //变量组不存在
                ack_data->result = COM_HOST_MSG_RET_NOT_FOUND;
                com_host_send_ack( host , port , sender_host_id , msg , (uint8_t*)ack_data , sizeof(com_host_msg_req_monitored_gruop_info_ack_data_t) , seq );
                break;
            }

            ack_data->result = COM_HOST_MSG_RET_OK;
            ack_data->group_id = monitor_gruop->group_id;
            ack_data->var_count = (uint8_t)list_get_length( monitor_gruop->reg_value_info_list );
            //填充变量信息
            const com_host_msg_unity_data_type_t map[] = {
                COM_HOST_MSG_UNITY_DATA_TYPE_INT8,
                COM_HOST_MSG_UNITY_DATA_TYPE_UINT8,
                COM_HOST_MSG_UNITY_DATA_TYPE_INT16,
                COM_HOST_MSG_UNITY_DATA_TYPE_UINT16,
                COM_HOST_MSG_UNITY_DATA_TYPE_INT32,
                COM_HOST_MSG_UNITY_DATA_TYPE_UINT32,
                COM_HOST_MSG_UNITY_DATA_TYPE_FLOAT
            };
            com_host_msg_monitored_variable_info_t* p_var_info = ack_data->var_info;
            foreach_list( monitor_gruop->reg_value_info_list , reg_info , value_reg_info_t )
            {
                p_var_info->variable_id = reg_info->variable_id;
                uint8_t name_len = (uint8_t)strlen( reg_info->name );
                p_var_info->name_len = name_len;
                p_var_info->data_type = map[reg_info->value_type];
                strncpy( (char*)p_var_info->name , reg_info->name , name_len );
                p_var_info = (com_host_msg_monitored_variable_info_t*)( (uint8_t*)p_var_info + sizeof(com_host_msg_monitored_variable_info_t) - 1 + name_len );
            }
            com_host_send_ack( host , port , sender_host_id , msg , (uint8_t*)ack_data , (uint16_t)( (uint8_t*)p_var_info - ack_data_buf ) , seq );

            break;
        }

        case COM_HOST_MSG_ID_SUB_MONITORED_GROUP:
        {
            com_host_msg_sub_monitored_group_data_t* req_data = (com_host_msg_sub_monitored_group_data_t*)msg->data;
            com_host_msg_sub_monitored_group_ack_data_t ack_data;

            monitor_gruop_t* monitor_gruop = NULL;
        
            foreach_list( g_online_monitor.monitor_group_list , it , monitor_gruop_t )
            {
                if( it->group_id == req_data->group_id )
                {
                    monitor_gruop = it;
                    break;
                }
            }

            if( monitor_gruop == NULL )
            {
                //变量组不存在
                ack_data.result = COM_HOST_MSG_RET_NOT_FOUND;
                com_host_send_ack( host , port , sender_host_id , msg , (uint8_t*)&ack_data , sizeof(ack_data) , seq );
                break;
            }

            subscribe_info_t* sub_info = NULL;
            //检查是否已订阅
            foreach_list( monitor_gruop->subscribe_info_list , it , subscribe_info_t )
            {
                if( it->host_id == sender_host_id )
                {
                    sub_info = it;
                    break;
                }
            }

            //该主机未订阅则添加订阅
            if( sub_info == NULL )
            {
                subscribe_info_t new_sub_info;
                new_sub_info.host_id = sender_host_id;
                sub_info = list_add_element( monitor_gruop->subscribe_info_list , &new_sub_info );
                if( sub_info == NULL )
                {
                    //添加失败
                    ack_data.result = COM_HOST_MSG_RET_ERROR;
                    com_host_send_ack( host , port , sender_host_id , msg , (uint8_t*)&ack_data , sizeof(ack_data) , seq );
                    break;
                }
            }
            
            //更新该组的推送周期
            monitor_gruop->push_cycle_ms = req_data->subscribe_param.push_cycle_ms;

            //发送成功ack
            ack_data.result = COM_HOST_MSG_RET_OK;
            com_host_send_ack( host , port , sender_host_id , msg , (uint8_t*)&ack_data , sizeof(ack_data) , seq );
            break;
        }
    }
}

/**
 * @brief 注册com_host消息处理函数
*/
static int16_t register_com_host_msg_handler(void)
{
    com_host_handle_t host = e_com_get_local_host();

    if( host == NULL )
    {
        ERROR_LOG( "e_online_monitor: com host is NULL when register message handler" );
        return -1;
    }

    const com_host_msg_id_t rsp_msg[] = {
        COM_HOST_MSG_ID_REQ_MONITORED_GROUP_LIST,
        COM_HOST_MSG_ID_REQ_MONITORED_GRUOP_INFO,
        COM_HOST_MSG_ID_SUB_MONITORED_GROUP
    };

    for( uint8_t temp = 0 ; temp < sizeof(rsp_msg)/sizeof(rsp_msg[0]) ; temp++ )
    {
        if( com_host_add_msg_handler( host , rsp_msg[temp] , com_host_msg_handler , NULL ) != 0 )
        {
            ERROR_LOG( "e_online_monitor: register com host message handler failed for msg id: 0x%04X" , rsp_msg[temp] );
            return -2;
        }
    }

    INFO_LOG( "e_online_monitor: com host message handler registered" );

    return 0;
}

static int e_online_monitor_init(void)
{
    memset( &g_online_monitor , 0 , sizeof(g_online_monitor) );

    // 创建变量组列表
    g_online_monitor.monitor_group_list = list_create( sizeof(monitor_gruop_t) );
    if( g_online_monitor.monitor_group_list == NULL )
    {
        ERROR_LOG( "Failed to create monitor group list" );
        return -1;
    }

    // 注册com_host消息处理函数
    if( register_com_host_msg_handler() != 0 )
    {
        ERROR_LOG( "Failed to register com host message handler" );
        return -2;
    }

    // 创建轮询任务
    e_polling_task_desc_t task_desc = {
        .attr = E_POLLING_TASK_ATTR_DEFAULT,
        .freq_hz = 100,
        .name = "e_online_monitor",
        .polling_func = polling_task,
        .start_before_create = 1,
        .user_ctx = NULL
    };
    if( e_polling_task_create( &task_desc ) == NULL )
    {
        ERROR_LOG( "Failed to create online monitor polling task" );
        return -3;
    }

    INFO_LOG( "e_online_monitor initialized" );

    return 0;
}
REGEDIT_FRAMEWORK_INIT_FUNCTION( e_online_monitor_init , 3 );

#endif

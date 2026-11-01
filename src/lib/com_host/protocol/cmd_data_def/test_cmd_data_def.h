#pragma once

#include "../com_msg_comm.h"

#pragma pack(1)

/**************************************************测试消息私有枚举*******************************************************************/

//空

/**************************************************测试消息私有结构体*******************************************************************/

//可监控变量组简要信息
typedef struct com_host_msg_monitored_group_brief_t{
    uint8_t group_id;                     //变量组ID
    uint8_t name_len;                     //变量组名称长度，单位字节
    uint8_t name[1];                      //变量组名称，实际长度根据name_len确定
}com_host_msg_monitored_group_brief_t;

//可监控变量信息
typedef struct com_host_msg_monitored_variable_info_t{
    uint16_t variable_id;                           //变量ID
    com_host_msg_unity_data_type_t data_type;       //变量数据类型
    uint8_t name_len;                               //变量名称长度，单位字节
    uint8_t name[1];                                //变量名称，实际长度根据name_len确定
}com_host_msg_monitored_variable_info_t;

/************************************************测试消息数据/应答结构体****************************************************************/

/****************************msg id : 0xFF00*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_ECHO_TEST
 * @brief 回显测试命令
*/

//数据
typedef struct com_host_msg_cmd_echo_test_data_t{
    uint8_t test_data[1];       //测试数据，实际长度根据msg->data_len确定
}com_host_msg_cmd_echo_test_data_t;

//应答
typedef struct com_host_msg_cmd_echo_test_ack_data_t{
    com_host_msg_ret_t result;      //结果
    uint8_t rsvd[3];                //24位保留
    uint8_t test_data[1];           //测试数据，实际长度根据msg->data_len确定
}com_host_msg_cmd_echo_test_ack_data_t;

/****************************msg id : 0xFF01*************************************/
/**
 * @name COM_HOST_MSG_ID_PUSH_ONLINE_LOG
 * @brief 在线日志推送
*/

//数据
typedef struct com_host_msg_push_online_log_data_t{
    uint8_t text[1];                        //日志内容，实际长度根据msg->data_len确定
}com_host_msg_push_online_log_data_t;

/****************************msg id : 0xFF05*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_MONITORED_GROUP_LIST
 * @brief 请求可监控的变量组列表
*/

//数据
typedef void* com_host_msg_req_monitored_group_list_data_t;

//应答
typedef struct com_host_msg_req_monitored_group_list_ack_data_t{
    com_host_msg_ret_t result;                      //结果
    uint8_t group_count;                            //变量组数量
    com_host_msg_monitored_group_brief_t brief[1];  //变量组简要信息数组，实际长度根据group_count和各元素的长度确定
}com_host_msg_req_monitored_group_list_ack_data_t;

/****************************msg id : 0xFF06*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_MONITORED_GRUOP_INFO
 * @brief 请求可监控的变量组信息
*/

//数据
typedef struct com_host_msg_req_monitored_gruop_info_data_t{
    uint8_t group_id;     //变量组ID
    uint8_t rsvd[3];      //24位保留
}com_host_msg_req_monitored_gruop_info_data_t;

//应答
typedef struct com_host_msg_req_monitored_gruop_info_ack_data_t{
    com_host_msg_ret_t result;                          //结果
    uint8_t group_id;                                   //变量组ID
    uint8_t var_count;                                  //变量数量
    com_host_msg_monitored_variable_info_t var_info[1]; //变量信息数组，实际长度根据var_count和各元素的长度确定
}com_host_msg_req_monitored_gruop_info_ack_data_t;

/****************************msg id : 0xFF07*************************************/
/**
 * @name COM_HOST_MSG_ID_SUB_MONITORED_GROUP
 * @brief 订阅可监控的变量组
*/

//数据
typedef struct com_host_msg_sub_monitored_group_data_t{
    uint8_t group_id;                         //变量组ID
    com_host_msg_subscribe_param_t subscribe_param; //订阅参数
}com_host_msg_sub_monitored_group_data_t;

//应答
typedef struct com_host_msg_sub_monitored_group_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_sub_monitored_group_ack_data_t;

/****************************msg id : 0xFF08*************************************/
/**
 * @name COM_HOST_MSG_ID_PUSH_MONITORED_GROUP
 * @brief 可监控的变量组数据推送
*/

//数据
typedef struct com_host_msg_push_monitored_group_data_t{
    uint32_t timestamp_ms;          //数据时间戳，单位毫秒
    uint8_t group_id;               //变量组ID
    uint8_t rsvd[3];                //24位保留
    uint8_t var_data_array[1];     //变量数据数组，实际长度根据各变量的数据类型和数量确定
}com_host_msg_push_monitored_group_data_t;

//应答
typedef void* com_host_msg_push_monitored_group_ack_data_t;

/****************************msg id : 0xFF0B*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_ENTER_ADMIN_MODE
 * @brief 进入管理员模式命令
*/

//数据
typedef struct com_host_msg_cmd_enter_admin_mode_data_t{
    uint8_t secret_pswd_len;        //密钥长度，单位字节
    uint8_t secret_pswd[1];         //密钥，实际长度根据secret_pswd_len确定
}com_host_msg_cmd_enter_admin_mode_data_t;

//应答
typedef struct com_host_msg_cmd_enter_admin_mode_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_cmd_enter_admin_mode_ack_data_t;

#pragma pack()

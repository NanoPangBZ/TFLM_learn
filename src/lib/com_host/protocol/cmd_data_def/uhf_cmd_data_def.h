#pragma once

#include "../com_msg_comm.h"

#pragma pack(1)

/**************************************************UHF消息私有枚举*******************************************************************/

//空

/**************************************************UHF消息私有结构体*******************************************************************/

typedef struct com_host_msg_uhf_status_t{
    uint8_t is_on:1;                //1:开启 0:关闭
    uint8_t is_connected:1;         //1:连接 0:未连接
    uint8_t rsvd_bits:6;            //6位保留
    uint8_t freq_count;             //总频点数，固定 12
    uint8_t freq_index;             //当前频点索引，1~12
    uint32_t freq_hz;               //当前频点频率，单位Hz
}com_host_msg_uhf_status_t;

/************************************************UHF消息数据/应答结构体****************************************************************/

/****************************msg id : 0x01C0*************************************/
/**
 * @name COM_HOST_MSG_ID_PUSH_UHF_STATUS
 * @brief 从机向主机推送 UHF 状态
*/

//数据
typedef struct com_host_msg_push_uhf_status_data_t{
    com_host_msg_uhf_status_t status;   //UHF状态
}com_host_msg_push_uhf_status_data_t;

//应答
typedef void* com_host_msg_push_uhf_status_ack_data_t;

/****************************msg id : 0x01C1*************************************/
/**
 * @name COM_HOST_MSG_ID_SUB_UHF_STATUS
 * @brief 订阅UHF状态
*/

//数据
typedef struct com_host_msg_sub_uhf_status_data_t{
    com_host_msg_subscribe_param_t subscribe_param; //订阅参数
}com_host_msg_sub_uhf_status_data_t;

//应答
typedef struct com_host_msg_sub_uhf_status_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_sub_uhf_status_ack_data_t;

/****************************msg id : 0x01C2*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_UHF_STATUS
 * @brief 请求UHF状态
*/

//数据
typedef void* com_host_msg_req_uhf_status_data_t;

//应答
typedef struct com_host_msg_req_uhf_status_ack_data_t{
    com_host_msg_ret_t result;      //结果
    com_host_msg_uhf_status_t status; //UHF状态
}com_host_msg_req_uhf_status_ack_data_t;

/****************************msg id : 0x01C3*************************************/
/**
 * @name COM_HOST_MSG_ID_PUSH_UHF_KEY_EVENT
 * @brief 推送UHF按键事件
*/

//数据
typedef struct com_host_msg_push_uhf_key_event_data_t{
    uint32_t timestamp_ms;  //时间戳，单位ms
    uint32_t key_mask;      //按键掩码，1:按下 0:释放
}com_host_msg_push_uhf_key_event_data_t;

//应答
typedef void* com_host_msg_push_uhf_key_event_ack_data_t;

#pragma pack()

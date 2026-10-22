#pragma once

#include "../com_msg_comm.h"

#pragma pack(1)

/**************************************************蓝牙音乐消息私有枚举*******************************************************************/

//空

/**************************************************蓝牙音乐消息私有结构体*******************************************************************/

//BT音乐状态
typedef struct com_host_msg_bt_music_status_t{
    uint8_t enable : 1;             //使能状态，1:使能 0:禁用
    uint8_t is_connected : 1;       //连接状态，1:已连接 0:未连接
    uint8_t adv_is_active : 1;      //广播状态，1:广播中 0:未广播
    uint8_t is_playing : 1;         //播放状态，1:播放中 0:未播放
    uint8_t rsvd_bits : 3;          //3位保留
    uint8_t rsvd[15];               //保留
}com_host_msg_bt_music_status_t;

/************************************************蓝牙音乐消息数据/应答结构体****************************************************************/

/****************************msg id : 0x0100*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_BT_MUSIC_ENABLE
 * @brief 使能/禁用蓝牙音乐
*/

//数据
typedef struct com_host_msg_cmd_bt_music_enable_data_t{
    uint8_t enable;   //使能标志，1:使能 0:禁用
    uint8_t rsvd[3]; //24位保留
}com_host_msg_cmd_bt_music_enable_data_t;

//应答
typedef struct com_host_msg_cmd_bt_music_enable_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_cmd_bt_music_enable_ack_data_t;

/****************************msg id : 0x0101*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_BT_MUSIC_SET_ADV_NAME
 * @brief 设置蓝牙音乐广播名称
*/

//数据
typedef struct com_host_msg_cmd_bt_music_set_adv_name_data_t{
    uint8_t adv_name[1];       //广播名称，实际长度根据msg->data_len确定
}com_host_msg_cmd_bt_music_set_adv_name_data_t;

//应答
typedef struct com_host_msg_cmd_bt_music_set_adv_name_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_cmd_bt_music_set_adv_name_ack_data_t;

/****************************msg id : 0x0102*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_BT_MUSIC_ADV_NAME
 * @brief 请求当前蓝牙音乐广播名称
*/

//数据
typedef void* com_host_msg_req_bt_music_adv_name_data_t;

//应答
typedef struct com_host_msg_req_bt_music_adv_name_ack_data_t{
    com_host_msg_ret_t result;      //结果
    uint8_t adv_name[1];            //广播名称，实际长度根据msg->data_len确定
}com_host_msg_req_bt_music_adv_name_ack_data_t;

/****************************msg id : 0x0103*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_BT_MUSIC_STATUS
 * @brief 请求当前蓝牙音乐状态
*/

//数据
typedef void* com_host_msg_req_bt_music_status_data_t;

//应答
typedef struct com_host_msg_req_bt_music_status_ack_data_t{
    com_host_msg_ret_t          result;      //结果
    com_host_msg_bt_music_status_t   bt_music_status;  //BT音乐状态
}com_host_msg_req_bt_music_status_ack_data_t;

/****************************msg id : 0x0104*************************************/
/**
 * @name COM_HOST_MSG_ID_SUB_BT_MUSIC_STATUS
 * @brief 订阅蓝牙音乐状态
*/

//数据
typedef struct com_host_msg_sub_bt_music_status_data_t{
    com_host_msg_subscribe_param_t subscribe_param; //订阅参数
}com_host_msg_sub_bt_music_status_data_t;

//应答
typedef struct com_host_msg_sub_bt_music_status_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_sub_bt_music_status_ack_data_t;

/****************************msg id : 0x0105*************************************/
/**
 * @name COM_HOST_MSG_ID_PUSH_BT_MUSIC_STATUS
 * @brief 推送蓝牙音乐状态
*/

//数据
typedef struct com_host_msg_push_bt_music_status_data_t{
    uint32_t timestamp_ms;                      //状态时间戳，单位毫秒
    com_host_msg_bt_music_status_t   bt_music_status;     //BT音乐状态
}com_host_msg_push_bt_music_status_data_t;

//应答
typedef struct com_host_msg_push_bt_music_status_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_push_bt_music_status_ack_data_t;

/****************************msg id : 0x0106*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_BT_MUSIC_DISCONNECT
 * @brief 断开蓝牙音乐连接
*/

//数据
typedef void* com_host_msg_cmd_bt_music_disconnect_data_t;

//应答
typedef struct com_host_msg_cmd_bt_music_disconnect_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_cmd_bt_music_disconnect_ack_data_t;

#pragma pack()

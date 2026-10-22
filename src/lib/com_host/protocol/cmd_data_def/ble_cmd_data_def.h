#pragma once

#include "../com_msg_comm.h"

#pragma pack(1)

/**************************************************低功耗蓝牙消息私有枚举*******************************************************************/

//空

/**************************************************低功耗蓝牙消息私有结构体*******************************************************************/

//ble状态
typedef struct com_host_msg_ble_status_t{
    uint8_t enable : 1;             //使能状态，1:使能 0:禁用
    uint8_t is_connected : 1;       //连接状态，1:已连接 0:未连接
    uint8_t adv_is_active : 1;      //广播状态，1:广播中 0:未广播
    uint8_t rsvd_bits : 5;          //5位保留
    uint8_t rsvd[15];               //保留
}com_host_msg_ble_status_t;

/************************************************低功耗蓝牙消息数据/应答结构体****************************************************************/

/****************************msg id : 0x00C0*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_BLE_ENABLE
 * @brief 使能/禁用低功耗蓝牙
*/

//数据
typedef struct com_host_msg_cmd_ble_enable_data_t{
    uint8_t enable;   //使能标志，1:使能 0:禁用
    uint8_t rsvd[3]; //24位保留
}com_host_msg_cmd_ble_enable_data_t;

//应答
typedef struct com_host_msg_cmd_ble_enable_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_cmd_ble_enable_ack_data_t;


/****************************msg id : 0x00C1*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_BLE_SET_ADV_NAME
 * @brief 设置低功耗蓝牙广播名称
*/

//数据
typedef struct com_host_msg_cmd_ble_set_adv_name_data_t{
    uint8_t adv_name[1];       //广播名称，实际长度根据msg->data_len确定
}com_host_msg_cmd_ble_set_adv_name_data_t;

//应答
typedef struct com_host_msg_cmd_ble_set_adv_name_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_cmd_ble_set_adv_name_ack_data_t;

/****************************msg id : 0x00C2*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_BLE_ADV_NAME
 * @brief 请求当前低功耗蓝牙广播名称
*/

//数据
typedef void* com_host_msg_req_ble_adv_name_data_t;

//应答
typedef struct com_host_msg_req_ble_adv_name_ack_data_t{
    com_host_msg_ret_t result;      //结果
    uint8_t adv_name[1];            //广播名称，实际长度根据msg->data_len确定
}com_host_msg_req_ble_adv_name_ack_data_t;

/****************************msg id : 0x00C3*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_BLE_STATUS
 * @brief 请求当前低功耗蓝牙状态
*/

//数据
typedef void* com_host_msg_req_ble_status_data_t;

//应答
typedef struct com_host_msg_req_ble_status_ack_data_t{
    com_host_msg_ret_t          result;      //结果
    com_host_msg_ble_status_t   ble_status;  //ble状态
}com_host_msg_req_ble_status_ack_data_t;

/****************************msg id : 0x00C4*************************************/
/**
 * @name COM_HOST_MSG_ID_SUB_BLE_STATUS
 * @brief 订阅低功耗蓝牙状态
*/

//数据
typedef struct com_host_msg_sub_ble_status_data_t{
    com_host_msg_subscribe_param_t subscribe_param; //订阅参数
}com_host_msg_sub_ble_status_data_t;

//应答
typedef struct com_host_msg_sub_ble_status_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_sub_ble_status_ack_data_t;

/****************************msg id : 0x00C5*************************************/
/**
 * @name COM_HOST_MSG_ID_PUSH_BLE_STATUS
 * @brief 推送低功耗蓝牙状态
*/

//数据
typedef struct com_host_msg_push_ble_status_data_t{
    uint32_t timestamp_ms;                      //状态时间戳，单位毫秒
    com_host_msg_ble_status_t   ble_status;     //ble状态
}com_host_msg_push_ble_status_data_t;

//应答
typedef struct com_host_msg_push_ble_status_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_push_ble_status_ack_data_t;

/****************************msg id : 0x00C6*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_BLE_DISCONNECT
 * @brief 断开低功耗蓝牙连接
*/

//数据
typedef void* com_host_msg_cmd_ble_disconnect_data_t;

//应答
typedef struct com_host_msg_cmd_ble_disconnect_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_cmd_ble_disconnect_ack_data_t;

/****************************msg id : 0x00C7*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_BLE_SET_ADV_MANUE_DATA
 * @brief 设置低功耗蓝牙广播制造商数据
*/

//数据
typedef struct com_host_msg_cmd_ble_set_adv_manue_data_data_t{
    uint8_t manue_data_len;     //制造商数据长度，单位字节
    uint8_t manue_data[1];      //制造商数据，实际长度根据msg->data_len确定
}com_host_msg_cmd_ble_set_adv_manue_data_data_t;

//应答
typedef struct com_host_msg_cmd_ble_set_adv_manue_data_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_cmd_ble_set_adv_manue_data_ack_data_t;

/****************************msg id : 0x00C8*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_BLE_GET_ADV_MANUE_DATA
 * @brief 获取低功耗蓝牙广播制造商数据
*/

//数据
typedef void* com_host_msg_cmd_ble_get_adv_manue_data_data_t;

//应答
typedef struct com_host_msg_cmd_ble_get_adv_manue_data_ack_data_t{
    com_host_msg_ret_t result;      //结果
    uint8_t manue_data_len;     //制造商数据长度，单位字节
    uint8_t manue_data[1];      //制造商数据，实际长度根据msg->data_len确定
}com_host_msg_cmd_ble_get_adv_manue_data_ack_data_t;

#pragma pack()

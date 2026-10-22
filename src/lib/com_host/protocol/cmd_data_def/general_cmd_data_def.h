#pragma once

#include "../com_msg_comm.h"

#pragma pack(1)

/**************************************************通用消息私有枚举*******************************************************************/

//重启类型枚举
typedef enum com_host_msg_reboot_type_e{
    COM_HOST_MSG_REBOOT_TYPE_SOFT = 0x00,           //软重启
    COM_HOST_MSG_REBOOT_TYPE_HARD = 0x01,           //硬重启
    COM_HOST_MSG_REBOOT_TYPE_KEEP_BOOT = 0x02,      //重启并保持在bootloader
    COM_HOST_MSG_REBOOT_TYPE_ONLY_SHUTDOWN = 0x03,  //仅关机
}com_host_msg_reboot_type_e;
typedef uint8_t com_host_msg_reboot_type_t;

/**************************************************通用消息私有结构体*******************************************************************/

//空

/************************************************通用消息数据/应答结构体****************************************************************/

/****************************msg id : 0x0000*************************************/
/**
 * @name COM_HOST_MSG_ID_PING
 * @brief ping消息
*/

//数据
typedef void* com_host_msg_ping_data_t;

//应答
typedef struct com_host_msg_ping_ack_data_t{
    uint32_t uptime;        //系统运行时间，单位秒
    uint32_t rsvd;          //32位保留
}com_host_msg_ping_ack_data_t;

/****************************msg id : 0x0001*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_PRODUCT_INFO
 * @brief 请求产品信息
*/

//数据
typedef void* com_host_msg_req_product_info_data_t;

//应答
typedef struct com_host_msg_req_product_info_ack_data_t{
    com_host_msg_ret_t result;                  //结果
    uint8_t product_name[32];                   //产品名称，字符串结尾必须为'\0'
    uint8_t serial_number[32];                  //产品序列号，字符串结尾必须为'\0'
    uint8_t manufacturer_name[32];              //制造商名称，字符串结尾必须为'\0'
    com_host_msg_version_t hardware_version;    //硬件版本
    com_host_msg_version_t firmware_version;    //固件版本
    com_host_msg_rtc_time_t production_date;    //生产日期时间
    com_host_msg_rtc_time_t activate_date;      //激活日期时间
}com_host_msg_req_product_info_ack_data_t;

/****************************msg id : 0x0002*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_DEVICE_INFO
 * @brief 请求设备信息
*/

//数据
typedef void* com_host_msg_req_device_info_data_t;

//应答
typedef struct com_host_msg_req_device_info_ack_data_t{
    com_host_msg_ret_t result;                  //结果
    com_host_msg_arch_t architecture;           //架构类型
    uint8_t device_name[32];                    //设备名称，字符串结尾必须为'\0'
    com_host_msg_version_t hardware_version;    //硬件版本
    com_host_msg_version_t firmware_version;    //固件版本
    com_host_msg_rtc_time_t build_date;         //编译日期时间
    uint8_t is_released:1;                      //是否为发布版本，1:是 0:否
    uint8_t is_demo:1;                          //是否为演示版本，1:是 0:否
    uint8_t rsvd_bits:6;                        //6位保留
    uint8_t git_branch[16];                     //git branch
    uint8_t git_commit_hash[16];                //git commit hash
    com_host_msg_rtc_time_t git_commit_date;    //git commit日期时间
    uint8_t board_name[32];                     //板子名称，字符串结尾必须为'\0'
    uint16_t brief_len;                         //设备简要信息长度，单位字节
    uint8_t brief[1];                           //设备简要信息，实际长度根据msg->data->brief_len确定
}com_host_msg_req_device_info_ack_data_t;

/****************************msg id : 0x0003*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_REBOOT
 * @brief 重启命令
*/

//数据
typedef struct com_host_msg_cmd_reboot_data_t{
    com_host_msg_reboot_type_t reboot_type;
    uint32_t delay;          //延时重启时间，单位秒
}com_host_msg_cmd_reboot_data_t;

//应答
typedef struct com_host_msg_cmd_reboot_ack_data_t{
    com_host_msg_ret_t result;      //结果
    uint8_t rsvd[3];                //24位保留
}com_host_msg_cmd_reboot_ack_data_t;

/****************************msg id : 0x0004*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_LOCK
 * @brief 锁定命令
*/

//数据
typedef struct com_host_msg_cmd_lock_data_t{
    uint8_t lock;                   //1:锁定 0:解锁
    uint8_t psw[32];                //密码
}com_host_msg_cmd_lock_data_t;

//应答
typedef struct com_host_msg_cmd_lock_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_cmd_lock_ack_data_t;

/****************************msg id : 0x0005*************************************/
/**
 * @name COM_HOST_MSG_ID_SUB_SYS_STATUS
 * @brief 订阅系统状态推送
*/

//数据
typedef struct com_host_msg_sub_sys_status_data_t{
    com_host_msg_subscribe_param_t subscribe_param; //订阅参数
}com_host_msg_sub_sys_status_data_t;

//应答
typedef struct com_host_msg_sub_sys_status_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_sub_sys_status_ack_data_t;

/****************************msg id : 0x0006*************************************/
/**
 * @name COM_HOST_MSG_ID_PUSH_SYS_STATUS
 * @brief 系统状态推送 - 32字节
*/

//数据
typedef struct com_host_msg_push_sys_status_data_t{
    uint8_t battery_level;          //电池电量百分比，0~100
    uint8_t battery_is_charging:1;  //电池是否在充电
    uint8_t battery_is_full:1;      //电池是否充满，充满时为1
    uint8_t is_sleep:1;             //1:休眠 0:正常
    uint8_t rsvd_bits:5;            //5位保留
    uint8_t rsvd[30];               //30字节保留
}com_host_msg_push_sys_status_data_t;

//应答
typedef struct com_host_msg_push_sys_status_ack_data_t{
    com_host_msg_ret_t result;      //结果
}com_host_msg_push_sys_status_ack_data_t;

/****************************msg id : 0x0007*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_VIRTUAL_CHANNEL_COM
 * @brief 虚拟通道通信
*/

//数据
typedef struct com_host_msg_cmd_virtual_channel_com_data_t{
    uint8_t channel_id;               //通道ID
    uint8_t data_len;                 //数据长度，单位字节
    uint8_t data[1];                  //数据，实际长度根据data_len确定
}com_host_msg_cmd_virtual_channel_com_data_t;

//应答
typedef void* com_host_msg_cmd_virtual_channel_com_ack_data_t;

#pragma pack()

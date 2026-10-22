#pragma once

#include <stdint.h>

/**
 * @brief 通信消息ID枚举
 * @note 命名规范 COM_HOST_MSG_ID_(ping/req/cmd/sub/notify)_(功能简写)
 * @note ping:心跳类消息 req:请求类消息 cmd:命令类消息 sub:订阅类消息 push:推送类消息 notify:通知类消息
*/
typedef enum com_host_msg_id_e{
    //通用消息(0x0000~0x003F) 64个
    COM_HOST_MSG_ID_PING = 0x0000,                      //ping消息
    COM_HOST_MSG_ID_REQ_PRODUCT_INFO = 0x0001,          //请求产品信息
    COM_HOST_MSG_ID_REQ_DEVICE_INFO = 0x0002,           //请求设备信息
    COM_HOST_MSG_ID_CMD_REBOOT = 0x0003,                //重启命令
    COM_HOST_MSG_ID_CMD_LOCK = 0x0004,                  //锁定命令
    COM_HOST_MSG_ID_SUB_SYS_STATUS = 0x0005,            //订阅系统状态推送
    COM_HOST_MSG_ID_PUSH_SYS_STATUS = 0x0006,           //系统状态推送
    COM_HOST_MSG_ID_CMD_VIRTUAL_CHANNEL_COM = 0x0007,   //虚拟通道通信

    //预留(0x0040~0x00BF) 128个

    //BLE控制相关(0x00C0~0x00FF) 64个
    COM_HOST_MSG_ID_CMD_BLE_ENABLE = 0x00C0,            //BLE使能命令
    COM_HOST_MSG_ID_CMD_BLE_SET_ADV_NAME = 0x00C1,      //设置BLE广播名称命令
    COM_HOST_MSG_ID_REQ_BLE_ADV_NAME = 0x00C2,          //请求BLE广播名称
    COM_HOST_MSG_ID_REQ_BLE_STATUS = 0x00C3,            //请求BLE状态
    COM_HOST_MSG_ID_SUB_BLE_STATUS = 0x00C4,            //订阅BLE状态
    COM_HOST_MSG_ID_PUSH_BLE_STATUS = 0x00C5,           //BLE状态推送
    COM_HOST_MSG_ID_CMD_BLE_DISCONNECT = 0x00C6,            //BLE断开连接命令
    COM_HOST_MSG_ID_CMD_BLE_SET_ADV_MANUE_DATA = 0x00C7,    //设置BLE广播制造商数据命令
    COM_HOST_MSG_ID_CMD_BLE_GET_ADV_MANUE_DATA = 0x00C8,    //获取BLE广播制造商数据命令

    //BT Music相关(0X00100~0x013F) 64个
    COM_HOST_MSG_ID_CMD_BT_MUSIC_ENABLE = 0x0100,           //BT Music使能命令
    COM_HOST_MSG_ID_CMD_BT_MUSIC_SET_ADV_NAME = 0x0101,     //设置BT Music广播名称命令
    COM_HOST_MSG_ID_REQ_BT_MUSIC_ADV_NAME = 0x0102,         //请求BT Music广播名称
    COM_HOST_MSG_ID_REQ_BT_MUSIC_STATUS = 0x0103,           //请求BT Music状态
    COM_HOST_MSG_ID_SUB_BT_MUSIC_STATUS = 0x0104,           //订阅BT Music状态
    COM_HOST_MSG_ID_PUSH_BT_MUSIC_STATUS = 0x0105,          //BT Music状态推送
    COM_HOST_MSG_ID_CMD_BT_MUSIC_DISCONNECT = 0x0106,       //BT Music断开连接命令

    //效果配置相关(0x0140~0x01BF) 128个
    COM_HOST_MSG_ID_REQ_EFFECT_SUPPORT_LIST = 0x0140,       //向设备请求效果支持列表
    COM_HOST_MSG_ID_REQ_EFFECT_NET_HASH = 0x0141,           //向设备请求效果网络的哈希值
    COM_HOST_MSG_ID_REQ_EFFECT_BLOCK_NET_BRIEF = 0x0142,    //向设备请求效果块网络的简要信息
    COM_HOST_MSG_ID_REQ_EFFECT_BLOCK_CONN_BRIEF = 0x0143,   //向设备请求效果块连接的简要信息
    COM_HOST_MSG_ID_REQ_EFFECT_BLOCK_PARAM = 0x0144,        //向设备请求效果块参数
    COM_HOST_MSG_ID_CMD_SET_EFFECT_BLOCK = 0x0145,          //设置效果块参数命令
    COM_HOST_MSG_ID_CMD_SET_EFFECT_CONNECTION = 0x0146,     //设置效果块连接命令 - 遗弃
    COM_HOST_MSG_ID_CMD_MAKE_EFFECT_CONN = 0x0147,          //创建效果块连接
    COM_HOST_MSG_ID_CMD_DELETE_EFFECT_CONN = 0x0148,        //删除效果块连接
    COM_HOST_MSG_ID_CMD_MAKE_EFFECT_BLOCK = 0x0149,         //创建效果块
    COM_HOST_MSG_ID_CMD_DELETE_EFFECT_BLOCK = 0x014A,       //删除效果块
    COM_HOST_MSG_ID_CMD_EFFECT_NET_RESET = 0x014B,          //重置效果网络
    COM_HOST_MSG_ID_REQ_EFFECT_NET_VIEW_BRIEF = 0x014C,     //请求效果网络视图的简要信息
    COM_HOST_MSG_ID_SET_EFFECT_NET_VIEW = 0x014D,           //设置效果网络视图
    COM_HOST_MSG_ID_REQ_EFFECT_BLOCK_PORTS = 0x014E,        //向设备请求效果块端口描述
    COM_HOST_MSG_ID_REQ_EFFECT_ENUM_TABLE_VERSIONS = 0x014F,//向设备请求所有枚举表的版本清单
    COM_HOST_MSG_ID_REQ_EFFECT_ENUM_TABLE = 0x0150,         //向设备请求指定(type,field)的枚举清单

    //发射器配件(0x01C0~0x01FF)
    COM_HOST_MSG_ID_PUSH_UHF_STATUS = 0x01C0,           //推送UHF状态
    COM_HOST_MSG_ID_SUB_UHF_STATUS = 0x01C1,            //订阅UHF状态
    COM_HOST_MSG_ID_REQ_UHF_STATUS = 0x01C2,            //请求UHF状态
    COM_HOST_MSG_ID_PUSH_UHF_KEY_EVENT = 0x01C3,        //推送UHF按键事件

    //预留(0x01C4~0xFEFF)

    //用户自定义(0xFF00~0xFF79) 128个
    COM_HOST_MSG_ID_USER_DEFINED_0 = 0xFF00,    //用户自定义0
    COM_HOST_MSG_ID_USER_DEFINED_1 = 0xFF01,    //用户自定义1
    COM_HOST_MSG_ID_USER_DEFINED_2 = 0xFF02,    //用户自定义2
    COM_HOST_MSG_ID_USER_DEFINED_3 = 0xFF03,    //用户自定义3
    COM_HOST_MSG_ID_USER_DEFINED_4 = 0xFF04,    //用户自定义4
    COM_HOST_MSG_ID_USER_DEFINED_5 = 0xFF05,    //用户自定义5
    COM_HOST_MSG_ID_USER_DEFINED_6 = 0xFF06,    //用户自定义6
    COM_HOST_MSG_ID_USER_DEFINED_7 = 0xFF07,    //用户自定义7

    //测试相关(0xFF80~0xFFFF) 128个
    COM_HOST_MSG_ID_CMD_ECHO_TEST = 0xFF00,                 //回显测试命令 - 通信benckmark - 预留
    COM_HOST_MSG_ID_PUSH_ONLINE_LOG = 0xFF01,               //在线日志推送
    COM_HOST_MSG_ID_PUSH_CONSOLE_INPUT = 0xFF02,            //控制台输入推送 - 预留
    COM_HOST_MSG_ID_PUSH_CONSOLE_OUTPUT = 0xFF03,           //控制台输出推送 - 预留
    COM_HOST_MSG_ID_REQ_MEMORY_DUMP = 0xFF04,               //请求内存数据 - 预留
    COM_HOST_MSG_ID_REQ_MONITORED_GROUP_LIST = 0xFF05,      //请求可监控的变量组列表
    COM_HOST_MSG_ID_REQ_MONITORED_GRUOP_INFO = 0xFF06,      //请求可监控的变量组信息
    COM_HOST_MSG_ID_SUB_MONITORED_GROUP = 0xFF07,           //订阅可监控的变量组
    COM_HOST_MSG_ID_PUSH_MONITORED_GROUP = 0xFF08,          //可监控的变量组数据推送
    COM_HOST_MSG_ID_REQ_LOCAL_LOG_FILE_LIST = 0xFF09,       //请求本地日志文件列表 - 预留
    COM_HOST_MSG_ID_REQ_LOCAL_LOG_FILE = 0xFF0A,            //请求本地日志文件内容 - 预留
    COM_HOST_MSG_ID_CMD_ENTER_ADMIN_MODE = 0xFF0B,          //进入管理员模式命令
}com_host_msg_id_e;
typedef uint16_t com_host_msg_id_t;


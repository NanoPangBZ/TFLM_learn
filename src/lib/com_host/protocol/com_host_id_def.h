#pragma once

/**
 * @brief 主机ID枚举
 * @details com_host_id用于区分网络中的不同设备,仅支持树状网络,不支持环状网络,树状网络以COM_HOST_ID_MAIN_CONTROLLER(0x01)为根节点。
 * @details COM_HOST_ID_MAIN_CONTROLLER必须为产品主控设备ID。
 * @details 为同时支持新配件兼容老产品、多配件同时连接和配件热插拔,配件设备ID由配件向主控申请分配,未分配前使用COM_HOST_ID_AUX_DEVICE_NONE(0x10)。
 */

#include <stdint.h>

typedef enum com_host_id_e
{
    COM_HOST_ID_NONE = 0x00, // 未定义host

    // 以下为主控设备(0x01-0x0F) 15个
    COM_HOST_ID_MAIN_CONTROLLER = 0x01,  // 主控
    COM_HOST_ID_SUB_CONTROLLER_1 = 0x02, // 子控1
    COM_HOST_ID_SUB_CONTROLLER_2 = 0x03, // 子控2
    COM_HOST_ID_SUB_CONTROLLER_3 = 0x04, // 子控3

    // 以下为配件设备id,由配件向主控申请分配(0x10-0x17) 8个 - 保留给未来使用
    COM_HOST_ID_AUX_DEVICE_NONE = 0x10, // 未分id的配件设备ID
    COM_HOST_ID_AUX_DEVICE_1 = 0x11,    // 配件设备1
    COM_HOST_ID_AUX_DEVICE_2 = 0x12,    // 配件设备2
    COM_HOST_ID_AUX_DEVICE_3 = 0x13,    // 配件设备3
    COM_HOST_ID_AUX_DEVICE_4 = 0x14,    // 配件设备4
    COM_HOST_ID_AUX_DEVICE_5 = 0x15,    // 配件设备5

    // 以下为客户端id(0xE0-0xEF) 16个
    COM_HOST_ID_MOBOILE_APP = 0xE0,     // 手机APP
    COM_HOST_ID_PC_CLIENT = 0xE1,       // PC客户端
    COM_HOST_ID_NETWORK_BACKEND = 0xE2, // 网络后台

    // 通用从机id(0xF0-0xFF) 16个
    COM_HOST_ID_GENERIC_BLE_BT_SLAVE = 0xF0,    // 通用BLE/BT从机
    
    // 以下为开发者(0xF0-0xFE) 15个
    COM_HOST_ID_ADMIN = 0xF1,     // 管理员端
    COM_HOST_ID_DEVELOPER = 0xF1, // 开发者端
    COM_HOST_ID_TESTER = 0xF2,    // 测试者端
    COM_HOST_ID_FACTORY = 0xF3,   // 工厂端
} com_host_id_e;
typedef uint8_t com_host_id_t;

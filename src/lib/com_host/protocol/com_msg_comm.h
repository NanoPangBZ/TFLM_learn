#pragma once

#include <stdint.h>

/**
 * @brief 一字节对齐
 * @note 所有结构体均为一字节对齐，保证跨平台的兼容性
 */
#pragma pack(1)

/**********************************通用枚举********************************************/

//通用结果枚举
typedef enum com_host_msg_ret_e{
    COM_HOST_MSG_RET_OK = 0x00,                 //成功
    COM_HOST_MSG_RET_ERROR = 0x01,              //错误
    COM_HOST_MSG_RET_TIMEOUT = 0x02,            //超时
    COM_HOST_MSG_RET_NOT_FOUND = 0x03,          //未找到
    COM_HOST_MSG_RET_BUSY = 0x04,               //忙
    COM_HOST_MSG_RET_INVALID = 0x05,            //无效
    COM_HOST_MSG_RET_PERMISSION_DENIED = 0x06,  //权限不足
    COM_HOST_MSG_RET_STATE_ERROR = 0x07,        //状态错误
    COM_HOST_MSG_RET_MEMORY_NOT_ENOUGH = 0x08,  //内存不足
    COM_HOST_MSG_RET_LOCKED = 0x09,             //已锁定（命中块锁或连接锁，禁止删除/修改）
}com_host_msg_ret_e;
typedef uint8_t com_host_msg_ret_t;

//架构类型枚举
typedef enum com_host_msg_arch_e{
    COM_HOST_MSG_ARCH_ARM_CORTEX_M = 0x00,      //ARM Cortex-M
    COM_HOST_MSG_ARCH_ARM_CORTEX_A = 0x01,      //ARM Cortex-A
    COM_HOST_MSG_ARCH_ARM_CORTEX_R = 0x02,      //ARM Cortex-R
    COM_HOST_MSG_ARCH_ARM64 = 0x03,             //ARM 64位架构
    COM_HOST_MSG_ARCH_X86 = 0x04,               //X86架构
    COM_HOST_MSG_ARCH_RISCV = 0x05,             //RISC-V架构
    COM_HOST_MSG_ARCH_MIPS = 0x06,              //MIPS架构
}com_host_msg_arch_e;
typedef uint8_t com_host_msg_arch_t;

//数据类型枚举
typedef enum com_host_msg_unity_data_type_e{
    COM_HOST_MSG_UNITY_DATA_TYPE_INT8 = 0x00,       //8位有符号整数
    COM_HOST_MSG_UNITY_DATA_TYPE_UINT8 = 0x01,      //8位无符号整数
    COM_HOST_MSG_UNITY_DATA_TYPE_INT16 = 0x02,      //16位有符号整数
    COM_HOST_MSG_UNITY_DATA_TYPE_UINT16 = 0x03,     //16位无符号整数
    COM_HOST_MSG_UNITY_DATA_TYPE_INT32 = 0x04,      //32位有符号整数
    COM_HOST_MSG_UNITY_DATA_TYPE_UINT32 = 0x05,     //32位无符号整数
    COM_HOST_MSG_UNITY_DATA_TYPE_FLOAT = 0x06,      //单精度浮点数
    COM_HOST_MSG_UNITY_DATA_TYPE_DOUBLE = 0x07,     //双精度浮点数
}com_host_msg_unity_data_type_e;
typedef uint8_t com_host_msg_unity_data_type_t;

/**********************************通用结构体********************************************/

//版本结构体
typedef struct com_host_msg_version_t{
    uint8_t major;               //主版本号
    uint8_t minor;               //次版本号
    uint8_t patch;               //修订版本号
}com_host_msg_version_t;

//RTC时间结构体
typedef struct com_host_msg_rtc_time_t{
    uint16_t year;       //年
    uint8_t month;       //月，1~12
    uint8_t day;         //日，1~31
    uint8_t hour;        //时，0~23
    uint8_t minute;      //分，0~59
    uint8_t second;      //秒，0~59
}com_host_msg_rtc_time_t;

//订阅参数结构体
typedef struct com_host_msg_subscribe_param_t{
    uint8_t subscribe:1;        //1:订阅 0:取消订阅
    uint8_t notify_on_change:1; //状态变化时立刻推送，1:是 0:否
    uint8_t resvd_bits:6;       //6位保留
    uint16_t push_cycle_ms;     //推送周期，单位毫秒
    uint8_t rsvd[2];            //16位保留
}com_host_msg_subscribe_param_t;

//平面点结构体 - 4字节
typedef struct com_host_msg_surface_point_t{
    int16_t x;       //x坐标
    int16_t y;       //y坐标
}com_host_msg_surface_point_t;

//空间点结构体 - 6字节
typedef struct com_host_msg_space_point_t{
    int16_t x;       //x坐标
    int16_t y;       //y坐标
    int16_t z;       //z坐标
}com_host_msg_space_point_t;

#pragma pack()

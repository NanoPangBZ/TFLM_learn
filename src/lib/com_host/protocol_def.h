#pragma once

#include <stdint.h>

//协议版本
#define COM_HOST_PROTOCOL_VERSION_MAJOR     1
#define COM_HOST_PROTOCOL_VERSION_MINOR     0
#define COM_HOST_PROTOCOL_VERSION_PATCH     8
#define COM_HOST_PROTOCOL_VERSION_RELEASE   1

//主机id定义
#include "./protocol/com_host_id_def.h"
//消息id定义
#include "./protocol/com_msg_id_def.h"

//消息数据段定义
#include "./protocol/cmd_data_def/ble_cmd_data_def.h"
#include "./protocol/cmd_data_def/bt_music_cmd_data_def.h"
#include "./protocol/cmd_data_def/general_cmd_data_def.h"
#include "./protocol/cmd_data_def/template_cmd_data_def.h"
#include "./protocol/cmd_data_def/test_cmd_data_def.h"
#include "./protocol/cmd_data_def/effect_cmd_data_def.h"
#include "./protocol/cmd_data_def/uhf_cmd_data_def.h"

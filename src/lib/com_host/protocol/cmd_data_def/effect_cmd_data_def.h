#pragma once

#include "../com_msg_comm.h"

#pragma pack(1)

/**************************************************效果配置消息私有枚举*******************************************************************/

//效果块类型枚举
typedef enum com_host_msg_effect_bolck_type_e{
    COM_HOST_MSG_EFFECT_BOLCK_TYPE_UNKNOWN = 0x00,      //未知
    COM_HOST_MSG_EFFECT_BLOCK_TYPE_AUDIO_INPUT = 0x01,  //音频输入块
    COM_HOST_MSG_EFFECT_BLOCK_TYPE_AUDIO_OUTPUT = 0x02, //音频输出块
    COM_HOST_MSG_EFFECT_BOLCK_TYPE_MIXER = 0x03,        //音频混音
    COM_HOST_MSG_EFFECT_BLOCK_TYPE_EQ = 0x04,           //音频均衡器
    COM_HOST_MSG_EFFECT_BLOCK_TYPE_AMP = 0x05,          //箱头
    COM_HOST_MSG_EFFECT_BLOCK_TYPE_DRUM_MACHINE = 0x06, //鼓机（纯音源，0进1出）
    COM_HOST_MSG_EFFECT_BLOCK_TYPE_FX = 0x07,           //FX调制效果（1进1出）
    COM_HOST_MSG_EFFECT_BLOCK_TYPE_DELAY = 0x08,        //Delay延迟效果（1进1出）
    COM_HOST_MSG_EFFECT_BLOCK_TYPE_NOISE_GATE = 0x09,   //噪声门（1进1出）
    COM_HOST_MSG_EFFECT_BLOCK_TYPE_COMP = 0x0A,         //压缩器（1进1出）
    COM_HOST_MSG_EFFECT_BLOCK_TYPE_REVERB = 0x0B,       //混响（1进1出）
}com_host_msg_effect_bolck_type_e;
typedef uint16_t com_host_msg_effect_bolck_type_t;

//箱头类型枚举（协议层总池，50个槽位）
//  0x0150 协议 version=2 起传输 type_id（此枚举值），上位机查字典显示多语言文本。
//  规则：① 一旦分配ID永不更改  ② 新增类型只在末尾追加  ③ 预留扩展空间
typedef enum com_host_msg_effect_block_amp_type_e{
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_BLACKFACE_DELUXE = 0,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_BLACKFACE_TWIN = 1,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_TWEED_BASS = 2,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_BRITISH_800 = 3,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_BRITISH_900 = 4,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_PLEXI_LEAD = 5,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_GERMAN_CLEAN = 6,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_GERMAN_FIRE = 7,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_MARK_CLEAN = 8,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_MARK_LEAD = 9,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_MODERN_MARK_CLEAN = 10,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_MODERN_MARK_LEAD = 11,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_RECTO_CLEAN = 12,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_RECTO_HIGH_GAIN = 13,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_ORANGE_CLEAN = 14,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_ORANGE_DIRTY = 15,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_HOT_ROD_CRUNCH = 16,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_HOT_ROD_LEAD = 17,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_CHIME_CLEAN = 18,
    COM_HOST_MSG_EFFECT_BLOCK_AMP_TYPE_TOP_BOOST = 19,
    // 预留扩展（20-49）
}com_host_msg_effect_block_amp_type_e;
typedef uint16_t com_host_msg_effect_block_amp_type_t;

//FX 调制效果类型枚举（协议层总池，30个槽位）
typedef enum com_host_msg_effect_block_fx_type_e{
    COM_HOST_MSG_EFFECT_BLOCK_FX_TYPE_TREMOLO = 0,
    COM_HOST_MSG_EFFECT_BLOCK_FX_TYPE_CHORUS = 1,
    COM_HOST_MSG_EFFECT_BLOCK_FX_TYPE_FLANGER = 2,
    COM_HOST_MSG_EFFECT_BLOCK_FX_TYPE_AUTOWAH = 3,
    COM_HOST_MSG_EFFECT_BLOCK_FX_TYPE_VIBRATO = 4,
    COM_HOST_MSG_EFFECT_BLOCK_FX_TYPE_PHASER = 5,
    COM_HOST_MSG_EFFECT_BLOCK_FX_TYPE_LOFI = 6,
    COM_HOST_MSG_EFFECT_BLOCK_FX_TYPE_PITCH = 7,
    COM_HOST_MSG_EFFECT_BLOCK_FX_TYPE_RINGMOD = 8,
    COM_HOST_MSG_EFFECT_BLOCK_FX_TYPE_ROTARY = 9,
    COM_HOST_MSG_EFFECT_BLOCK_FX_TYPE_STUTTER = 10,
    COM_HOST_MSG_EFFECT_BLOCK_FX_TYPE_TOUCHWAH = 11,
    // 预留扩展（12-29）
}com_host_msg_effect_block_fx_type_e;
typedef uint16_t com_host_msg_effect_block_fx_type_t;

//Delay 延迟类型枚举（协议层总池，20个槽位）
typedef enum com_host_msg_effect_block_delay_type_e{
    COM_HOST_MSG_EFFECT_BLOCK_DELAY_TYPE_DIGITAL = 0,
    COM_HOST_MSG_EFFECT_BLOCK_DELAY_TYPE_ANALOG = 1,
    COM_HOST_MSG_EFFECT_BLOCK_DELAY_TYPE_TAPE = 2,
    COM_HOST_MSG_EFFECT_BLOCK_DELAY_TYPE_REAL = 3,
    COM_HOST_MSG_EFFECT_BLOCK_DELAY_TYPE_PINGPONG = 4,
    COM_HOST_MSG_EFFECT_BLOCK_DELAY_TYPE_REVERSE = 5,
    // 预留扩展（6-19）
}com_host_msg_effect_block_delay_type_e;
typedef uint16_t com_host_msg_effect_block_delay_type_t;

//Reverb 混响类型枚举（协议层总池，20个槽位）
typedef enum com_host_msg_effect_block_reverb_type_e{
    COM_HOST_MSG_EFFECT_BLOCK_REVERB_TYPE_HALL = 0,
    COM_HOST_MSG_EFFECT_BLOCK_REVERB_TYPE_ROOM = 1,
    COM_HOST_MSG_EFFECT_BLOCK_REVERB_TYPE_CHURCH = 2,
    COM_HOST_MSG_EFFECT_BLOCK_REVERB_TYPE_CAVE = 3,
    COM_HOST_MSG_EFFECT_BLOCK_REVERB_TYPE_MOD = 4,
    COM_HOST_MSG_EFFECT_BLOCK_REVERB_TYPE_SPRING = 5,
    COM_HOST_MSG_EFFECT_BLOCK_REVERB_TYPE_PLATE = 6,
    // 预留扩展（7-19）
}com_host_msg_effect_block_reverb_type_e;
typedef uint16_t com_host_msg_effect_block_reverb_type_t;

//EQ 均衡器类型枚举（协议层总池，20个槽位）
typedef enum com_host_msg_effect_block_eq_type_e{
    COM_HOST_MSG_EFFECT_BLOCK_EQ_TYPE_TONE = 0,
    COM_HOST_MSG_EFFECT_BLOCK_EQ_TYPE_5BAND = 1,
    COM_HOST_MSG_EFFECT_BLOCK_EQ_TYPE_7BAND = 2,
    COM_HOST_MSG_EFFECT_BLOCK_EQ_TYPE_10BAND = 3,
    // 预留扩展（4-19）
}com_host_msg_effect_block_eq_type_e;
typedef uint16_t com_host_msg_effect_block_eq_type_t;

//Drum 鼓机节奏类型枚举（协议层总池，50个槽位）
typedef enum com_host_msg_effect_block_drum_rhythm_type_e{
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_POP_4_4 = 0,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_POP_3_4 = 1,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_POP_6_8 = 2,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_POP_12_8 = 3,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_POP_5_4 = 4,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_FUNK_4_4 = 5,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_ROCK_4_4 = 6,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_DISCO_4_4 = 7,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_SWING_4_4 = 8,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_SAMBA_4_4 = 9,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_BLUES_4_4 = 10,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_BOSSA_NOVA_4_4 = 11,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_MAMBO_4_4 = 12,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_RUMBA_4_4 = 13,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_METAL_4_4 = 14,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_REGGAE_4_4 = 15,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_JAZZ_4_4 = 16,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_HIPHOP_4_4 = 17,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_HOUSE_4_4 = 18,
    COM_HOST_MSG_EFFECT_BLOCK_DRUM_RHYTHM_BLUES_SWING_4_4 = 19,
    // 预留扩展（20-49）
}com_host_msg_effect_block_drum_rhythm_type_e;
typedef uint16_t com_host_msg_effect_block_drum_rhythm_type_t;

/**************************************************效果配置消息私有结构体*******************************************************************/

//效果块简要信息结构体 - 8字节
typedef struct com_host_msg_effect_block_brief_t{
    uint16_t block_id;                      //效果块ID
    com_host_msg_effect_bolck_type_t type;  //效果块类型
    uint8_t is_lock:1;
    uint8_t rvsd_bits:7;                    //块锁标志位（is_lock=1：不可删+不可改参）
    uint8_t rvsd[3];                        //保留
}com_host_msg_effect_block_brief_t;

//效果器单条连接描述 - 8字节
typedef struct com_host_msg_effect_conn_t{
    uint16_t source_block_id;   //源效果块ID
    uint8_t source_port_id;     //源端口ID
    uint16_t target_block_id;   //目标效果块ID
    uint8_t target_port_id;     //目标端口ID
    uint8_t is_lock:1;          //连接锁标志位（is_lock=1：线不可删，两端块不可删）
    uint8_t rvsd_bits:7;
    uint8_t rvsd[1];            //保留
}com_host_msg_effect_conn_t;

//效果块端口方向枚举
typedef enum com_host_msg_effect_port_dir_e{
    COM_HOST_MSG_EFFECT_PORT_DIR_INPUT  = 0x00,  //输入口
    COM_HOST_MSG_EFFECT_PORT_DIR_OUTPUT = 0x01,  //输出口
}com_host_msg_effect_port_dir_e;
typedef uint8_t com_host_msg_effect_port_dir_t;

//单个端口描述 - 4字节
typedef struct com_host_msg_effect_port_desc_t{
    uint8_t port_id;     //端口号（对应上位机 in0/in1/out 的数字，out=0，inN=N）
    uint8_t direction;   //方向，见 com_host_msg_effect_port_dir_t
    uint8_t max_conn;    //该口最大连接数；输入口通常=1，输出口=0xFF（不限）
    uint8_t rvsd;        //保留
}com_host_msg_effect_port_desc_t;

//效果块参数结构体 - 4字节 + 对应类型参数长度字节
typedef struct com_host_msg_effect_block_param_t{
    com_host_msg_effect_bolck_type_t type;  //效果块类型
    uint16_t block_param_len;               //效果块参数长度，单位字节
    uint8_t block_param[1];                 //效果块参数，根据type不同，参数结构不同
}com_host_msg_effect_block_param_t;

//音频输入块参数结构体 - 不定长
typedef struct com_host_msg_effect_block_param_audio_input_t{
    uint8_t name_len;       //名称长度
    uint8_t name[1];        //名称，实际长度根据name_len确定
}com_host_msg_effect_block_param_audio_input_t;

//音频输出块参数结构体 - 不定长
typedef struct com_host_msg_effect_block_param_audio_output_t{
    uint8_t name_len;       //名称长度
    uint8_t name[1];        //名称，实际长度根据name_len确定
}com_host_msg_effect_block_param_audio_output_t;

//箱头参数结构体 - 24字节
typedef struct com_host_msg_effect_block_amp_param_t{
    uint16_t enable:1;                              //使能
    uint16_t rsvd:15;                               //保留
    com_host_msg_effect_block_amp_type_t amp_type;  //箱头类型
    uint8_t gain;                                   //箱头增益，(0~100)%
    uint8_t bass;                                   //低音，(0~100)%
    uint8_t mid;                                    //中音，(0~100)%
    uint8_t high;                                   //高音，(0~100)%
    uint8_t pre;                                    //预延时，(0~100)%
    uint8_t level;                                  //音量，(0~100)%
    uint8_t resvd[14];                              //保留
}com_host_msg_effect_block_amp_param_t;

//鼓机参数结构体 - 16字节
typedef struct com_host_msg_effect_block_drum_machine_param_t{
    uint16_t enable:1;          //使能：0=关闭，1=开启
    uint16_t rsvd_bits:15;      //保留位
    uint16_t bpm;               //鼓机速度（BPM）
    uint8_t volume;             //鼓机音量，(0~100)%
    uint8_t preset;             //鼓机预设编号
    uint8_t resvd[10];          //保留，便于后续扩展
}com_host_msg_effect_block_drum_machine_param_t;

//FX调制参数结构体 - 16字节
//
// rate/depth/range 是「位置槽」slot1/slot2/slot3，语义随 fx_type 变化
//（与上位机 resources/effectdefs.json 的 forTypes + offsetByType 对齐）：
//   0 Tremolo / 1 Chorus / 2 Flanger / 4 Vibrato / 5 Phaser:
//       slot1=Rate, slot2=Depth, slot3=未用, level=Level
//   3 AutoWah:
//       slot1=Rate, slot2=Range, slot3=Depth, level=Level
//   6 Lofi:
//       slot1=Downsample, slot2=Bit(0~24), slot3=Mix, level=Level
//   7 Pitch / 9 Rotary / 10 Stutter:
//       slot1=Rate, slot2=Mix, slot3=Tone, level=Level
//   8 RingMod:
//       slot1=Rate, slot2=Mix, slot3=未用, level=Level
//   11 TouchWah:
//       slot1=Attack, slot2=Sensitivity, slot3=Depth, level=Level
//
// 接到 DSP 时务必按 fx_type 解读槽位，不要把字段名当固定语义。
typedef struct com_host_msg_effect_block_fx_param_t{
    uint16_t enable:1;          //使能：0=关闭，1=开启
    uint16_t rsvd_bits:15;      //保留位
    uint16_t fx_type;           //FX类型：0=Tremolo, 1=Chorus, 2=Flanger, 3=AutoWah, 4=Vibrato, 5=Phaser, 6=Lofi, 7=Pitch, 8=RingMod, 9=Rotary, 10=Stutter, 11=TouchWah
    uint8_t rate;               //slot1（语义见上）
    uint8_t depth;              //slot2（语义见上）
    uint8_t range;              //slot3（语义见上）
    uint8_t level;              //音量，(0~100)%
    uint8_t resvd[8];           //保留，便于后续扩展
}com_host_msg_effect_block_fx_param_t;

//Delay延迟参数结构体 - 16字节
typedef struct com_host_msg_effect_block_delay_param_t{
    uint16_t enable:1;          //使能：0=关闭，1=开启
    uint16_t rsvd_bits:15;      //保留位
    uint16_t delay_type;        //Delay类型：0=Digital, 1=Analog, 2=Tape, 3=Real, 4=PingPong, 5=Reverse
    uint16_t time;              //延迟时间，(0~2000)ms
    uint8_t feedback;           //反馈量，(0~100)%
    uint8_t mix;                //混合量，(0~100)%
    uint8_t level;              //音量，(0~100)%
    uint8_t resvd[7];           //保留，便于后续扩展
}com_host_msg_effect_block_delay_param_t;

//噪声门参数结构体 - 16字节（与上位机 effectdefs.json wire 对齐）
//  Attack 0~150ms / Hold 0~1500ms / Release 0~3000ms / Threshold -60~0dB
typedef struct com_host_msg_effect_block_noise_gate_param_t{
    uint16_t enable:1;          //使能：0=关闭，1=开启
    uint16_t rsvd_bits:15;      //保留位
    int16_t  threshold;         //门限，(-60~0)dB
    uint16_t attack;            //起音时间，(0~150)ms
    uint16_t hold;              //保持时间，(0~1500)ms
    uint16_t release;           //释音时间，(0~3000)ms
    uint8_t  resvd[6];          //保留，便于后续扩展
}com_host_msg_effect_block_noise_gate_param_t;

//压缩器参数结构体 - 16字节（与上位机 effectdefs.json wire 对齐）
//  Attack 0~300ms / Threshold -60~0dB / Release 0~1000ms / Ratio 1~64 / Makeup 0~20dB
typedef struct com_host_msg_effect_block_comp_param_t{
    uint16_t enable:1;          //使能：0=关闭，1=开启
    uint16_t rsvd_bits:15;      //保留位
    int16_t  threshold;         //门限，(-60~0)dB
    uint16_t attack;            //起音时间，(0~300)ms
    uint16_t release;           //释音时间，(0~1000)ms
    uint8_t  ratio;             //压缩比，(1~64) 表示 1:1~64:1
    uint8_t  makeup;            //补偿增益，(0~20)dB
    uint8_t  resvd[6];          //保留，便于后续扩展
}com_host_msg_effect_block_comp_param_t;

//均衡器参数结构体 - 16字节（与上位机 effectdefs.json wire 对齐）
// payload 是位置槽，语义随 eq_type 变化：
//   0 ToneEQ:
//       payload[0..3]=Bass/Mid/High/Presence (0~100), payload[4]=Level (0~100)
//   1 5-Band EQ:
//       payload[0..4]=i8 增益 80/240/750/2.2k/6.6k Hz (-12~12)dB, payload[5]=Level
//   2 7-Band EQ:
//       payload[0..6]=i8 100/200/400/800/1.6k/3.2k/6.4k Hz, payload[7]=Level
//   3 10-Band EQ:
//       payload[0..9]=i8 32/64/125/250/500/1k/2k/4k/8k/16k Hz, payload[10]=Level
// Custom EQ 暂不实现。
typedef struct com_host_msg_effect_block_eq_param_t{
    uint16_t enable:1;          //使能：0=关闭，1=开启
    uint16_t rsvd_bits:15;      //保留位
    uint16_t eq_type;           //0=ToneEQ, 1=5-Band EQ, 2=7-Band EQ, 3=10-Band EQ
    uint8_t  payload[12];       //类型相关参数槽，见上方注释
}com_host_msg_effect_block_eq_param_t;

//混响参数结构体 - 16字节（与上位机 effectdefs.json wire 对齐）
//  7 种类型参数相同：Hall/Room/Church/Cave/Mod/Spring/Plate
//  Predelay 0~500ms / Tone 0~100 / Feedback 0~100 / Mix 0~100
typedef struct com_host_msg_effect_block_reverb_param_t{
    uint16_t enable:1;          //使能：0=关闭，1=开启
    uint16_t rsvd_bits:15;      //保留位
    uint16_t reverb_type;       //0=Hall, 1=Room, 2=Church, 3=Cave, 4=Mod, 5=Spring, 6=Plate
    uint16_t predelay;          //预延迟，(0~500)ms
    uint8_t  tone;              //音色，(0~100)
    uint8_t  feedback;          //反馈量，(0~100)
    uint8_t  mix;               //混合量，(0~100)
    uint8_t  resvd[7];          //保留，便于后续扩展
}com_host_msg_effect_block_reverb_param_t;

//效果网络视图块点结构体 - 8字节
typedef struct com_host_msg_effect_net_block_view_t{
    uint16_t block_id;                      //效果块ID
    com_host_msg_surface_point_t position;  //位置
    uint8_t rsvd[2];                        //保留
}com_host_msg_effect_net_block_view_t;

//枚举字段定位符 - 4字节，用(type, field_id)唯一定位一张枚举表（field_id 由设备自定义上报，上位机当不透明稳定键）
typedef struct com_host_msg_effect_enum_locator_t{
    com_host_msg_effect_bolck_type_t type;  //效果类型，如 AMP(0x05)/DRUM(0x06)
    uint16_t field_id;                      //枚举字段号（设备自定义，各效果内从1起编）
}com_host_msg_effect_enum_locator_t;

//枚举表版本项 - 8字节
typedef struct com_host_msg_effect_enum_table_version_t{
    com_host_msg_effect_enum_locator_t locator;  //定位(type, field_id)，4字节
    uint16_t version;                            //该表版本号，内容变更时+1
    uint16_t item_count;                         //该表条目数（便于上位机预分配/校验）
}com_host_msg_effect_enum_table_version_t;

//单个枚举条目 - 变长。布局：index(2)|symbolic_len(1)|name_len(1)|symbolic[..]|name[..]
//  symbolic_id：ASCII 稳定符号(如"british_800")，可为空(len=0)；name：UTF-8 显示名。逐条变长，须按 len 步进
typedef struct com_host_msg_effect_enum_item_t{
    uint16_t index;          //下发索引值（= amp_type / preset 实际写入的值）
    uint8_t  symbolic_len;   //symbolic_id 字节数，0 表示无
    uint8_t  name_len;       //name 字节数
    uint8_t  payload[1];     //变长：先 symbolic_id 后 name，各按自身长度
}com_host_msg_effect_enum_item_t;

/************************************************效果配置消息数据/应答结构体****************************************************************/

/****************************msg id : 0x0140*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_EFFECT_SUPPORT_LIST
 * @brief 请求效果支持列表
*/

//数据
typedef void* com_host_msg_req_effect_support_list_data_t;

//应答
typedef struct com_host_msg_req_effect_support_list_ack_data_t{
    com_host_msg_ret_t result;                          //结果
    uint16_t support_list_len;                          //支持列表长度，单位个
    com_host_msg_effect_bolck_type_t support_list[1];   //支持列表，实际长度根据ack_msg->data->support_list_len确定
}com_host_msg_req_effect_support_list_ack_data_t;

/****************************msg id : 0x0141*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_EFFECT_NET_HASH
 * @brief 向设备请求效果网络的哈希值
*/
//数据
typedef void* com_host_msg_req_effect_net_hash_data_t;
//应答
typedef struct com_host_msg_req_effect_net_hash_ack_data_t{
    com_host_msg_ret_t result;  //结果
    uint8_t hash[8];            //哈希值
}com_host_msg_req_effect_net_hash_ack_data_t;

/****************************msg id : 0x0142*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_EFFECT_BLOCK_NET_BRIEF
 * @brief 请求效果块网络的简要信息
*/
//数据
typedef void* com_host_msg_req_effect_block_net_brief_data_t;
//应答
typedef struct com_host_msg_req_effect_block_net_brief_ack_data_t{
    com_host_msg_ret_t result;                              //结果
    uint16_t net_brief_len;                                 //网络简要信息长度，单位个
    com_host_msg_effect_block_brief_t net_brief_list[1];    //网络中的效果块简要信息列表
}com_host_msg_req_effect_block_net_brief_ack_data_t;

/****************************msg id : 0x0143*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_EFFECT_BLOCK_CONN_BRIEF
 * @brief 请求效果块连接的简要信息
*/
//数据
typedef void* com_host_msg_req_effect_block_conn_brief_data_t;
//应答
typedef struct com_host_msg_req_effect_block_conn_brief_ack_data_t{
    com_host_msg_ret_t result;                      //结果
    uint16_t connection_len;                        //连接列表长度，单位个
    com_host_msg_effect_conn_t connection_list[1];  //连接列表，实际长度根据ack_msg->data->connection_len确定
}com_host_msg_req_effect_block_conn_brief_ack_data_t;

/****************************msg id : 0x0144*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_EFFECT_BLOCK_PARAM
 * @brief 请求效果块参数
*/
//数据
typedef struct com_host_msg_req_effect_block_param_data_t{
    uint16_t block_id;                          //效果块ID
}com_host_msg_req_effect_block_param_data_t;
//应答
typedef struct com_host_msg_req_effect_block_param_ack_data_t{
    com_host_msg_ret_t result;                  //结果
    com_host_msg_effect_block_param_t param;    //参数
}com_host_msg_req_effect_block_param_ack_data_t;

/****************************msg id : 0x0145*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_SET_EFFECT_BLOCK
 * @brief 设置效果块参数
*/
//数据
typedef struct com_host_msg_cmd_set_effect_block_data_t{
    uint16_t block_id;                          //效果块ID
    com_host_msg_effect_block_param_t param;    //参数
}com_host_msg_cmd_set_effect_block_data_t;
//应答
typedef struct com_host_msg_cmd_set_effect_block_ack_data_t{
    com_host_msg_ret_t result;  //结果
}com_host_msg_cmd_set_effect_block_ack_data_t;

/****************************msg id : 0x0146*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_SET_EFFECT_CONNECTION
 * @brief 设置效果块连接 - 遗弃
*/
//数据
typedef struct com_host_msg_cmd_set_effect_connection_data_t{
    uint16_t connection_id;                 //连接ID
    com_host_msg_effect_conn_t connection;  //连接
}com_host_msg_cmd_set_effect_connection_data_t;
//应答
typedef struct com_host_msg_cmd_set_effect_connection_ack_data_t{
    com_host_msg_ret_t result;  //结果
}com_host_msg_cmd_set_effect_connection_ack_data_t;

/****************************msg id : 0x0147*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_MAKE_EFFECT_CONN
 * @brief 创建效果块连接
*/
//数据
typedef struct com_host_msg_cmd_make_effect_conn_data_t{
    com_host_msg_effect_conn_t connection;  //连接
}com_host_msg_cmd_make_effect_conn_data_t;
//应答
typedef struct com_host_msg_cmd_make_effect_conn_ack_data_t{
    com_host_msg_ret_t result;              //结果
}com_host_msg_cmd_make_effect_conn_ack_data_t;

/****************************msg id : 0x0148*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_DELETE_EFFECT_CONN
 * @brief 删除效果块连接
*/
//数据
typedef struct com_host_msg_cmd_delete_effect_conn_data_t{
    com_host_msg_effect_conn_t connection;  //连接
}com_host_msg_cmd_delete_effect_conn_data_t;
//应答
typedef struct com_host_msg_cmd_delete_effect_conn_ack_data_t{
    com_host_msg_ret_t result;  //结果
}com_host_msg_cmd_delete_effect_conn_ack_data_t;

/****************************msg id : 0x0149*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_MAKE_EFFECT_BLOCK
 * @brief 创建效果块
*/
//数据
typedef struct com_host_msg_cmd_make_effect_block_data_t{
    com_host_msg_effect_block_param_t param;    //参数
}com_host_msg_cmd_make_effect_block_data_t;
//应答
typedef struct com_host_msg_cmd_make_effect_block_ack_data_t{
    com_host_msg_ret_t result;  //结果
    uint16_t block_id;          //效果块ID
}com_host_msg_cmd_make_effect_block_ack_data_t;

/****************************msg id : 0x014A*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_DELETE_EFFECT_BLOCK
 * @brief 删除效果块
*/
//数据
typedef struct com_host_msg_cmd_delete_effect_block_data_t{
    uint16_t block_id;     //效果块ID
}com_host_msg_cmd_delete_effect_block_data_t;
//应答
typedef struct com_host_msg_cmd_delete_effect_block_ack_data_t{
    com_host_msg_ret_t result;  //结果
}com_host_msg_cmd_delete_effect_block_ack_data_t;

/****************************msg id : 0x014B*************************************/
/**
 * @name COM_HOST_MSG_ID_CMD_EFFECT_NET_RESET
 * @brief 重置效果网络
*/
//数据
typedef void* com_host_msg_cmd_effect_net_reset_data_t;
//应答
typedef struct com_host_msg_cmd_effect_net_reset_ack_data_t{
    com_host_msg_ret_t result;  //结果
}com_host_msg_cmd_effect_net_reset_ack_data_t;

/****************************msg id : 0x014C*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_EFFECT_NET_VIEW_BRIEF
 * @brief 请求效果网络视图的简要信息
*/
//数据
typedef void* com_host_msg_req_effect_net_view_brief_data_t;
//应答
typedef struct com_host_msg_req_effect_net_view_brief_ack_data_t{
    com_host_msg_ret_t result;                                  //结果
    uint16_t view_brief_len;                                    //视图简要信息长度，单位个
    com_host_msg_effect_net_block_view_t view_brief_list[1];    //视图简要信息列表，实际长度根据ack_msg->data->view_brief_len确定
}com_host_msg_req_effect_net_view_brief_ack_data_t;

/****************************msg id : 0x014D*************************************/
/**
 * @name COM_HOST_MSG_ID_SET_EFFECT_NET_VIEW
 * @brief 设置效果网络视图
*/
//数据
typedef struct com_host_msg_set_effect_net_view_data_t{
    uint16_t view_len;                                      //视图长度，单位个
    com_host_msg_effect_net_block_view_t view_list[1];      //视图列表，实际长度根据data->view_len确定
}com_host_msg_set_effect_net_view_data_t;
//应答
typedef struct com_host_msg_set_effect_net_view_ack_data_t{
    com_host_msg_ret_t result;  //结果
}com_host_msg_set_effect_net_view_ack_data_t;

/****************************msg id : 0x014E*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_EFFECT_BLOCK_PORTS
 * @brief 向设备请求效果块端口描述
*/
//数据
typedef struct com_host_msg_req_effect_block_ports_data_t{
    uint16_t block_id;                          //效果块ID
}com_host_msg_req_effect_block_ports_data_t;
//应答 - 4字节 + N*4字节
typedef struct com_host_msg_req_effect_block_ports_ack_data_t{
    com_host_msg_ret_t result;                      //结果
    uint16_t port_len;                              //端口个数
    com_host_msg_effect_port_desc_t port_list[1];   //端口列表，实际长度根据port_len确定
}com_host_msg_req_effect_block_ports_ack_data_t;

/****************************msg id : 0x014F*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_EFFECT_ENUM_TABLE_VERSIONS
 * @brief 向设备请求所有枚举表的版本清单
 * @note 握手阶段调用一次，拿到设备所有枚举表版本，与本地缓存比对，决定哪些表需重拉
*/

//数据
typedef void* com_host_msg_req_effect_enum_table_versions_data_t;

//应答
typedef struct com_host_msg_req_effect_enum_table_versions_ack_data_t{
    com_host_msg_ret_t result;                              //结果
    uint16_t table_count;                                   //枚举表数量
    com_host_msg_effect_enum_table_version_t versions[1];   //变长，长度=table_count
}com_host_msg_req_effect_enum_table_versions_ack_data_t;

/****************************msg id : 0x0150*************************************/
/**
 * @name COM_HOST_MSG_ID_REQ_EFFECT_ENUM_TABLE
 * @brief 向设备请求指定(type,field)的枚举清单
 * @note 每条含 index(下发用) + symbolic_id(稳定标识,可空) + name(UTF-8显示名)
*/

//数据
typedef struct com_host_msg_req_effect_enum_table_data_t{
    com_host_msg_effect_enum_locator_t locator;  //要拉哪张表
}com_host_msg_req_effect_enum_table_data_t;

//应答
typedef struct com_host_msg_req_effect_enum_table_ack_data_t{
    com_host_msg_ret_t result;                   //结果
    com_host_msg_effect_enum_locator_t locator;  //回显定位符
    uint16_t version;                            //本表版本（与0x014F对应，写入缓存键）
    uint16_t item_count;                         //条目数
    com_host_msg_effect_enum_item_t items[1];    //变长，需按每条 len 步进解析
}com_host_msg_req_effect_enum_table_ack_data_t;

#pragma pack()

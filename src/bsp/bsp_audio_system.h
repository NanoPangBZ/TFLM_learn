#pragma once

#include <stdint.h>

//音频帧信息（双声道交错 L,R,L,R…；buf_size = frame_sample_cnt * 2 * sizeof(float)）
typedef struct bsp_audio_frame_t{
    float*      stream_buf;
    uint32_t    buf_size;
}bsp_audio_frame_t;

//音频系统帧信息
typedef struct bsp_audio_system_frame_info_t{
    uint32_t sample_rate;           //采样率

    bsp_audio_frame_t* tx_frame;   //发送音频帧信息
    uint8_t tx_frame_cnt;          //发送音频帧数量

    bsp_audio_frame_t* rx_frame;    //接收音频帧信息
    uint8_t rx_frame_cnt;           //接收音频帧数量

    uint32_t frame_sample_cnt;      //每帧采样点数量
}bsp_audio_system_frame_info_t;

/**
 * @brief 音频系统音频处理回调
 * @param info 音频系统帧信息
 * @return None
 */
typedef void (*bsp_audio_handler_t)( bsp_audio_system_frame_info_t* info );

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @brief 启动音频系统
 * @param None
 * @return 0 成功；-1 失败（已启动或内部资源创建失败）
 */
int bsp_audio_system_init(void);

/**
 * @brief 关闭音频系统
 * @param None
 * @return None
 */
void bsp_audio_system_deinit(void);

/**
 * @brief 注册音频处理回调函数
 * @param handler 音频处理回调函数
 * @return None
 */
void bsp_audio_system_set_handler( bsp_audio_handler_t handler );

#ifdef __cplusplus
}
#endif // __cplusplus

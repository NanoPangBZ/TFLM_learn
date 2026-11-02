#pragma once

#include <stdint.h>

/**
 * @file bsp_audio_system.h
 * @brief 多路浮点音频帧处理接口（交错立体声 L,R,L,R…）。
 *
 * 实现约定（一帧缓存）：
 *   - 每拍先输出「上一拍 handler 填好的 TX」，再拉 RX，再调 handler 填「下一拍 TX」。
 *   - handler 可占用接近一帧时长，而不堵塞当前拍的推送；首拍 TX 为静音。
 *   - handler 内只读写本帧 stream_buf，勿做重 I/O / 长时间阻塞。
 *
 * buf_size = frame_sample_cnt * 2 * sizeof(float)
 */

typedef struct bsp_audio_frame_t {
    float*   stream_buf;
    uint32_t buf_size;
} bsp_audio_frame_t;

typedef struct bsp_audio_system_frame_info_t {
    uint32_t sample_rate;

    bsp_audio_frame_t* tx_frame;   /* 写出：本拍填下一拍要推送的 TX */
    uint8_t            tx_frame_cnt;

    bsp_audio_frame_t* rx_frame;   /* 只读：本拍已拉取的 RX */
    uint8_t            rx_frame_cnt;

    uint32_t frame_sample_cnt;
} bsp_audio_system_frame_info_t;

typedef void (*bsp_audio_handler_t)(bsp_audio_system_frame_info_t* info);

#ifdef __cplusplus
extern "C" {
#endif

int bsp_audio_system_init(void);
void bsp_audio_system_deinit(void);
void bsp_audio_system_set_handler(bsp_audio_handler_t handler);

#ifdef __cplusplus
}
#endif

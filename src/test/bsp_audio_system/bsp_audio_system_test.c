/** TX 输出 1 kHz、0.2 幅值正弦到 WAV；RX 能量条显示系统输出 loopback。 */
#include "framework/c_function_regedit.h"

#include "bsp/bsp_audio_system.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "framework/e_log.h"
#define TAG "bsp_audio_system_test"
#define INFO_LOG(...)  E_INFO_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...) E_ERROR_LOG(TAG, __VA_ARGS__)

#define SINE_FREQUENCY_HZ (1000.0f)
#define SINE_AMPLITUDE    (0.2f)
#define TWO_PI_F          (6.28318530717958647692f)

static float s_sine_phase;

static void on_audio_system_frame(bsp_audio_system_frame_info_t* info)
{
    float phase;
    float phase_step;
    uint32_t n;
    uint32_t ch;

    if (info == NULL || info->tx_frame == NULL || info->sample_rate == 0u) {
        return;
    }

    phase = s_sine_phase;
    phase_step = TWO_PI_F * SINE_FREQUENCY_HZ / (float)info->sample_rate;

    for (n = 0u; n < info->frame_sample_cnt; n++) {
        float sample = sinf(phase) * SINE_AMPLITUDE;

        for (ch = 0u; ch < (uint32_t)info->tx_frame_cnt; ch++) {
            float* output = info->tx_frame[ch].stream_buf;

            if (output != NULL) {
                output[2u * n] = sample;
                output[2u * n + 1u] = sample;
            }
        }

        phase += phase_step;
        if (phase >= TWO_PI_F) {
            phase -= TWO_PI_F;
        }
    }

    s_sine_phase = phase;
}

static int bsp_audio_system_test_init(void)
{
    if (bsp_audio_system_init() != 0) {
        ERROR_LOG("bsp_audio_system_init failed");
        return -1;
    }
    bsp_audio_system_set_handler(on_audio_system_frame);
    INFO_LOG("TX: %.1f Hz sine amp=%.2f -> wav; RX: system output loopback",
             (double)SINE_FREQUENCY_HZ, (double)SINE_AMPLITUDE);
    return 0;
}

REGEDIT_AFTER_APP_INIT_FUNCTION(bsp_audio_system_test_init, 2);

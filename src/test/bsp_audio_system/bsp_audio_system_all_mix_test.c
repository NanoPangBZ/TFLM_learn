#include "bsp/bsp_audio_system.h"
#include "framework/c_function_regedit.h"
#include "framework/e_log.h"

#include <stddef.h>
#include <stdint.h>

#define TAG "audio_all_mix_test"
#define INFO_LOG(...)  E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)  E_WARN_LOG(TAG, __VA_ARGS__)

static float clamp_audio(float sample)
{
    if (sample > 1.0f)
    {
        return 1.0f;
    }
    if (sample < -1.0f)
    {
        return -1.0f;
    }
    return sample;
}

static void audio_all_mix_handler(bsp_audio_system_frame_info_t *info)
{
    uint32_t sample_count;

    if ((info == NULL) || (info->tx_frame == NULL) || (info->rx_frame == NULL))
    {
        return;
    }

    sample_count = info->frame_sample_cnt * 2U;
    for (uint32_t tx = 0U; tx < info->tx_frame_cnt; tx++)
    {
        float *output = info->tx_frame[tx].stream_buf;

        if (output == NULL)
        {
            continue;
        }

        for (uint32_t sample = 0U; sample < sample_count; sample++)
        {
            float mixed = 0.0f;

            for (uint32_t rx = 0U; rx < info->rx_frame_cnt; rx++)
            {
                const float *input = info->rx_frame[rx].stream_buf;
                if (input != NULL)
                {
                    mixed += input[sample];
                }
            }
            output[sample] = clamp_audio(mixed);
        }
    }
}

static int bsp_audio_system_all_mix_test_init(void)
{
    if (bsp_audio_system_init() != 0)
    {
        WARN_LOG("audio system may already be initialized, replace current handler");
    }

    bsp_audio_system_set_handler(audio_all_mix_handler);
    INFO_LOG("mix all input frames to all output frames");
    return 0;
}

REGEDIT_AFTER_APP_INIT_FUNCTION(bsp_audio_system_all_mix_test_init, 2);

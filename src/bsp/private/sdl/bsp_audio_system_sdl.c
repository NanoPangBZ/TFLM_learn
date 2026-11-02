/* bsp_audio_system_sdl.c — bsp_audio_system 的 SDL3 后端。
 *
 * 通道由 bsp_cfg.h 的 BSP_AUDIO_SYSTEM_RX_MAPTABLE / TX_MAPTABLE 配置：
 *   - RX：system output loopback / WAV 文件（可循环）/ 正弦源；
 *   - TX：写 PCM WAV / 播放到系统默认输出；
 *   - 能量条：每个映射项一条，显示 map.name。
 *
 * 线程模型：e_polling_pool 取数 / 凑帧 / 调 handler；arch 主线程画能量条。
 */
#include "bsp/bsp_audio_system.h"

#include "arch/arch_display.h"

#include "framework/c_function_regedit.h"
#include "framework/e_log.h"
#include "framework/e_polling_pool.h"

#include <SDL3/SDL.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define TAG "bsp_audio_system"
#define INFO_LOG(...)  E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)  E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...) E_ERROR_LOG(TAG, __VA_ARGS__)

/* 枚举/映射结构仅本文件可见；须在 include bsp_cfg.h 之前定义，
 * 以便 maptable 宏展开时直接写 RX_IMPL_TYPE_* / TX_IMPL_TYPE_*。 */
typedef enum rx_impl_type_e {
    RX_IMPL_TYPE_SYSTEM_OUTPUT_LOOPBACK = 0,
    RX_IMPL_TYPE_FILE = 1,
    RX_IMPL_TYPE_SINE_WAVE = 2,
} rx_impl_type_e;

typedef enum tx_impl_type_e {
    TX_IMPL_TYPE_FILE = 0,
    TX_IMPL_TYPE_SYSTEM_OUTPUT = 1,
} tx_impl_type_e;

typedef struct bsp_audio_system_rx_map_t {
    rx_impl_type_e rx_impl_type;
    const char*    name;
    union {
        struct {
            float gain;
        } system_output_loopback;
        struct {
            const char* file_path;
            float       gain;
            uint32_t    loop : 1;
            uint32_t    reserve : 31;
            uint32_t    loop_cnt; /* loop=1 时：0=无限，>0=循环次数上限 */
        } file;
        struct {
            float frequency;
            float amplitude;
        } sine_wave;
    } config;
} bsp_audio_system_rx_map_t;

typedef struct bsp_audio_system_tx_map_t {
    tx_impl_type_e tx_impl_type;
    const char*    name;
    union {
        struct {
            const char* file_path;
            float       gain;
        } file;
        struct {
            float gain;
        } system_output;
    } config;
} bsp_audio_system_tx_map_t;

#include "bsp_cfg.h"

#ifndef BSP_AUDIO_SAMPLE_RATE_HZ
#define BSP_AUDIO_SAMPLE_RATE_HZ    44100u
#endif
#ifndef BSP_AUDIO_FRAME_SAMPLES
#define BSP_AUDIO_FRAME_SAMPLES     128u
#endif
#ifndef BSP_AUDIO_CHANNELS
#define BSP_AUDIO_CHANNELS          2u
#endif
#ifndef BSP_AUDIO_POLL_FREQ_HZ
#define BSP_AUDIO_POLL_FREQ_HZ      500u
#endif
#ifndef BSP_AUDIO_TX_MAX
#define BSP_AUDIO_TX_MAX            4u
#endif
#ifndef BSP_AUDIO_RX_MAX
#define BSP_AUDIO_RX_MAX            4u
#endif
#ifndef BSP_AUDIO_ENERGY_X
#define BSP_AUDIO_ENERGY_X          8
#endif
#ifndef BSP_AUDIO_ENERGY_Y
#define BSP_AUDIO_ENERGY_Y          0
#endif
#ifndef BSP_AUDIO_ENERGY_W
#define BSP_AUDIO_ENERGY_W          200
#endif
#ifndef BSP_AUDIO_ENERGY_H
#define BSP_AUDIO_ENERGY_H          40
#endif

#define AUDIO_FRAME_FLOATS   (BSP_AUDIO_FRAME_SAMPLES * BSP_AUDIO_CHANNELS)
#define AUDIO_FRAME_BYTES    (AUDIO_FRAME_FLOATS * (uint32_t)sizeof(float))
#define AUDIO_ENERGY_DECAY   0.92f
#define AUDIO_TX_PLAY_MAX_QUEUED_FRAMES  4u
#define AUDIO_TWO_PI_F                   6.28318530717958647692f

#ifndef BSP_AUDIO_SYSTEM_RX_MAPTABLE
#error "bsp_cfg.h must define BSP_AUDIO_SYSTEM_RX_MAPTABLE"
#endif
#ifndef BSP_AUDIO_SYSTEM_TX_MAPTABLE
#error "bsp_cfg.h must define BSP_AUDIO_SYSTEM_TX_MAPTABLE"
#endif

static const bsp_audio_system_rx_map_t s_rx_map[] = BSP_AUDIO_SYSTEM_RX_MAPTABLE;
static const bsp_audio_system_tx_map_t s_tx_map[] = BSP_AUDIO_SYSTEM_TX_MAPTABLE;

typedef struct wav_writer_t {
    SDL_IOStream* io;
    uint32_t      data_bytes;
    uint32_t      sample_rate;
    uint16_t      channels;
} wav_writer_t;

typedef struct rx_runtime_t {
    const bsp_audio_system_rx_map_t* map;
    SDL_AudioStream* stream;     /* loopback 设备流，或文件解码后的拉取流 */
    /* file 源 */
    uint8_t*         file_raw;
    uint32_t         file_raw_len;
    SDL_AudioSpec    file_spec;
    uint32_t         loops_done;
    uint8_t          file_eof;
    /* sine 源 */
    float            sine_phase;
    float            accum[AUDIO_FRAME_FLOATS];
    uint32_t         accum_floats;
} rx_runtime_t;

typedef struct tx_runtime_t {
    const bsp_audio_system_tx_map_t* map;
    wav_writer_t     wav;        /* FILE */
    SDL_AudioStream* play;       /* SYSTEM_OUTPUT */
} tx_runtime_t;

static float s_tx_float[BSP_AUDIO_TX_MAX][AUDIO_FRAME_FLOATS];
static float s_rx_float[BSP_AUDIO_RX_MAX][AUDIO_FRAME_FLOATS];

static rx_runtime_t s_rx[BSP_AUDIO_RX_MAX];
static tx_runtime_t s_tx[BSP_AUDIO_TX_MAX];
static uint8_t s_rx_cnt;
static uint8_t s_tx_cnt;

static bsp_audio_handler_t volatile s_user_handler;
static volatile uint8_t s_initialized;

static e_polling_task_handle_t s_polling_task;
static SDL_Mutex* s_lock;
static uint64_t s_start_ns;
static uint64_t s_frames_done;
static uint8_t s_audio_subsystem_inited;

static SDL_AtomicInt s_energy_tx_q15[BSP_AUDIO_TX_MAX];
static SDL_AtomicInt s_energy_rx_q15[BSP_AUDIO_RX_MAX];
static float s_peak_tx[BSP_AUDIO_TX_MAX];
static float s_peak_rx[BSP_AUDIO_RX_MAX];

/* -------------------------------------------------------------------------- */
/* WAV 写出                                                                   */
/* -------------------------------------------------------------------------- */

static int wav_write_u16(SDL_IOStream* io, uint16_t v)
{
    return SDL_WriteIO(io, &v, sizeof(v)) == sizeof(v) ? 0 : -1;
}

static int wav_write_u32(SDL_IOStream* io, uint32_t v)
{
    return SDL_WriteIO(io, &v, sizeof(v)) == sizeof(v) ? 0 : -1;
}

static int wav_write_header(wav_writer_t* w)
{
    if (SDL_WriteIO(w->io, "RIFF", 4) != 4) return -1;
    if (wav_write_u32(w->io, 36u) != 0) return -1;
    if (SDL_WriteIO(w->io, "WAVE", 4) != 4) return -1;
    if (SDL_WriteIO(w->io, "fmt ", 4) != 4) return -1;
    if (wav_write_u32(w->io, 16u) != 0) return -1;
    if (wav_write_u16(w->io, 1u) != 0) return -1;
    if (wav_write_u16(w->io, w->channels) != 0) return -1;
    if (wav_write_u32(w->io, w->sample_rate) != 0) return -1;
    {
        uint32_t byte_rate = w->sample_rate * w->channels * 2u;
        uint16_t block_align = (uint16_t)(w->channels * 2u);
        if (wav_write_u32(w->io, byte_rate) != 0) return -1;
        if (wav_write_u16(w->io, block_align) != 0) return -1;
        if (wav_write_u16(w->io, 16u) != 0) return -1;
    }
    if (SDL_WriteIO(w->io, "data", 4) != 4) return -1;
    if (wav_write_u32(w->io, 0u) != 0) return -1;
    return 0;
}

static int wav_open(wav_writer_t* w, const char* path, uint32_t sample_rate, uint16_t channels)
{
    memset(w, 0, sizeof(*w));
    w->io = SDL_IOFromFile(path, "wb");
    if (!w->io) {
        ERROR_LOG("open wav '%s' failed: %s", path, SDL_GetError());
        return -1;
    }
    w->sample_rate = sample_rate;
    w->channels = channels;
    if (wav_write_header(w) != 0) {
        ERROR_LOG("write wav header failed: %s", SDL_GetError());
        SDL_CloseIO(w->io);
        w->io = NULL;
        return -1;
    }
    return 0;
}

static int16_t float_to_i16(float s)
{
    if (s > 1.0f) s = 1.0f;
    if (s < -1.0f) s = -1.0f;
    return (int16_t)lrintf(s * 32767.0f);
}

static int wav_write_float_frame(wav_writer_t* w, const float* interleaved, uint32_t float_cnt, float gain)
{
    int16_t pcm[AUDIO_FRAME_FLOATS];
    uint32_t i;

    if (!w->io || !interleaved || float_cnt == 0u || float_cnt > AUDIO_FRAME_FLOATS) {
        return -1;
    }
    for (i = 0; i < float_cnt; i++) {
        pcm[i] = float_to_i16(interleaved[i] * gain);
    }
    if (SDL_WriteIO(w->io, pcm, float_cnt * sizeof(int16_t)) !=
        (size_t)(float_cnt * sizeof(int16_t))) {
        return -1;
    }
    w->data_bytes += (uint32_t)(float_cnt * sizeof(int16_t));
    return 0;
}

static void wav_close(wav_writer_t* w)
{
    if (!w->io) {
        return;
    }
    (void)SDL_SeekIO(w->io, 4, SDL_IO_SEEK_SET);
    (void)wav_write_u32(w->io, 36u + w->data_bytes);
    (void)SDL_SeekIO(w->io, 40, SDL_IO_SEEK_SET);
    (void)wav_write_u32(w->io, w->data_bytes);
    (void)SDL_FlushIO(w->io);
    SDL_CloseIO(w->io);
    w->io = NULL;
}

/* -------------------------------------------------------------------------- */
/* 能量                                                                       */
/* -------------------------------------------------------------------------- */

static float calc_rms(const float* buf, uint32_t n)
{
    double acc = 0.0;
    uint32_t i;
    if (!buf || n == 0u) {
        return 0.0f;
    }
    for (i = 0; i < n; i++) {
        double s = (double)buf[i];
        acc += s * s;
    }
    return (float)sqrt(acc / (double)n);
}

static void energy_store_all(void)
{
    uint8_t i;
    for (i = 0; i < s_tx_cnt; i++) {
        float rms = calc_rms(s_tx_float[i], AUDIO_FRAME_FLOATS);
        if (rms > 1.0f) rms = 1.0f;
        SDL_SetAtomicInt(&s_energy_tx_q15[i], (int)lrintf(rms * 32767.0f));
    }
    for (i = 0; i < s_rx_cnt; i++) {
        float rms = calc_rms(s_rx_float[i], AUDIO_FRAME_FLOATS);
        if (rms > 1.0f) rms = 1.0f;
        SDL_SetAtomicInt(&s_energy_rx_q15[i], (int)lrintf(rms * 32767.0f));
    }
}

static int debug_text_width(const char* s)
{
    int n = 0;
    if (!s) {
        return 0;
    }
    while (s[n]) {
        n++;
    }
    return n * (int)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
}

static void draw_energy_bar(struct SDL_Renderer* renderer,
                            float x, float y, float w, float h,
                            const char* name, float level, float* peak,
                            int is_tx)
{
    float label_w;
    SDL_FRect track;
    SDL_FRect fill;
    const char* label = name ? name : (is_tx ? "TX" : "RX");

    if (level > *peak) {
        *peak = level;
    } else {
        *peak *= AUDIO_ENERGY_DECAY;
    }

    label_w = (float)debug_text_width(label) + 8.0f;
    if (label_w < 28.0f) {
        label_w = 28.0f;
    }
    if (label_w > w * 0.45f) {
        label_w = w * 0.45f;
    }

    if (is_tx) {
        SDL_SetRenderDrawColor(renderer, 180, 220, 160, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 160, 210, 230, 255);
    }
    SDL_RenderDebugText(renderer, x + 2.0f, y + (h - (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) * 0.5f, label);

    track.x = x + label_w;
    track.y = y;
    track.w = w - label_w - 4.0f;
    track.h = h;
    if (track.w < 4.0f) {
        return;
    }

    SDL_SetRenderDrawColor(renderer, 45, 52, 60, 255);
    SDL_RenderFillRect(renderer, &track);

    fill = track;
    fill.w = track.w * (*peak);
    if (is_tx) {
        SDL_SetRenderDrawColor(renderer, 60, 140, 70, 180);
    } else {
        SDL_SetRenderDrawColor(renderer, 40, 110, 150, 180);
    }
    SDL_RenderFillRect(renderer, &fill);

    fill.w = track.w * level;
    if (is_tx) {
        SDL_SetRenderDrawColor(renderer, 90, 220, 110, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 70, 190, 230, 255);
    }
    SDL_RenderFillRect(renderer, &fill);
}

static void audio_render_cb(struct SDL_Renderer* renderer, void* ctx)
{
    const float x = (float)BSP_AUDIO_ENERGY_X;
    const float y = (float)BSP_AUDIO_ENERGY_Y;
    const float w = (float)BSP_AUDIO_ENERGY_W;
    const float h = (float)BSP_AUDIO_ENERGY_H;
    const int total = (int)s_tx_cnt + (int)s_rx_cnt;
    float gap;
    float bar_h;
    float cursor_y;
    SDL_FRect bg;
    uint8_t i;

    (void)ctx;
    if (!renderer || !s_initialized || total <= 0) {
        return;
    }

    bg.x = x;
    bg.y = y;
    bg.w = w;
    bg.h = h;
    SDL_SetRenderDrawColor(renderer, 30, 34, 40, 255);
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawColor(renderer, 70, 80, 95, 255);
    SDL_RenderRect(renderer, &bg);

    gap = 3.0f;
    bar_h = (h - gap * (float)(total + 1)) / (float)total;
    if (bar_h < 6.0f) {
        bar_h = 6.0f;
    }
    cursor_y = y + gap;

    for (i = 0; i < s_tx_cnt; i++) {
        float lv = (float)SDL_GetAtomicInt(&s_energy_tx_q15[i]) / 32767.0f;
        draw_energy_bar(renderer, x + 4.0f, cursor_y, w - 8.0f, bar_h,
                        s_tx[i].map->name, lv, &s_peak_tx[i], 1);
        cursor_y += bar_h + gap;
    }
    for (i = 0; i < s_rx_cnt; i++) {
        float lv = (float)SDL_GetAtomicInt(&s_energy_rx_q15[i]) / 32767.0f;
        draw_energy_bar(renderer, x + 4.0f, cursor_y, w - 8.0f, bar_h,
                        s_rx[i].map->name, lv, &s_peak_rx[i], 0);
        cursor_y += bar_h + gap;
    }
}

/* -------------------------------------------------------------------------- */
/* RX：loopback / file                                                        */
/* -------------------------------------------------------------------------- */

static SDL_AudioDeviceID find_system_output_loopback(void)
{
    const char* playback_name;
    SDL_AudioDeviceID* rec_devs = NULL;
    int rec_count = 0;
    SDL_AudioDeviceID found = 0;
    int i;

    playback_name = SDL_GetAudioDeviceName(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK);
    if (!playback_name) {
        ERROR_LOG("no default playback device: %s", SDL_GetError());
        return 0;
    }

    rec_devs = SDL_GetAudioRecordingDevices(&rec_count);
    if (!rec_devs || rec_count <= 0) {
        ERROR_LOG("no recording devices (enable monitors?): %s", SDL_GetError());
        SDL_free(rec_devs);
        return 0;
    }

    for (i = 0; i < rec_count; i++) {
        const char* name = SDL_GetAudioDeviceName(rec_devs[i]);
        if (name && SDL_strcmp(name, playback_name) == 0) {
            found = rec_devs[i];
            INFO_LOG("RX loopback: '%s'", name);
            break;
        }
    }

    if (!found) {
        ERROR_LOG("loopback for playback '%s' not found (%d recording devices)",
                  playback_name, rec_count);
    }

    SDL_free(rec_devs);
    return found;
}

static int rx_file_feed_stream(rx_runtime_t* rx)
{
    if (!rx->stream || !rx->file_raw || rx->file_raw_len == 0u) {
        return -1;
    }

    (void)SDL_ClearAudioStream(rx->stream);
    if (!SDL_PutAudioStreamData(rx->stream, rx->file_raw, (int)rx->file_raw_len)) {
        ERROR_LOG("put file audio failed: %s", SDL_GetError());
        return -1;
    }
    (void)SDL_FlushAudioStream(rx->stream);
    rx->file_eof = 0;
    return 0;
}

static int rx_open_loopback(rx_runtime_t* rx)
{
    SDL_AudioSpec spec;
    SDL_AudioDeviceID id;

    id = find_system_output_loopback();
    if (!id) {
        return -1;
    }

    memset(&spec, 0, sizeof(spec));
    spec.format = SDL_AUDIO_F32;
    spec.channels = (int)BSP_AUDIO_CHANNELS;
    spec.freq = (int)BSP_AUDIO_SAMPLE_RATE_HZ;

    rx->stream = SDL_OpenAudioDeviceStream(id, &spec, NULL, NULL);
    if (!rx->stream) {
        ERROR_LOG("open RX loopback '%s' failed: %s",
                  rx->map->name ? rx->map->name : "?", SDL_GetError());
        return -1;
    }
    if (!SDL_ResumeAudioStreamDevice(rx->stream)) {
        ERROR_LOG("resume RX loopback '%s' failed: %s",
                  rx->map->name ? rx->map->name : "?", SDL_GetError());
        SDL_DestroyAudioStream(rx->stream);
        rx->stream = NULL;
        return -1;
    }
    return 0;
}

static int rx_open_file(rx_runtime_t* rx)
{
    const char* path = rx->map->config.file.file_path;
    SDL_AudioSpec dst;

    if (!path || !path[0]) {
        ERROR_LOG("RX file '%s': empty path", rx->map->name ? rx->map->name : "?");
        return -1;
    }

    if (!SDL_LoadWAV(path, &rx->file_spec, &rx->file_raw, &rx->file_raw_len) || !rx->file_raw) {
        ERROR_LOG("RX file '%s' load '%s' failed: %s",
                  rx->map->name ? rx->map->name : "?", path, SDL_GetError());
        rx->file_raw = NULL;
        rx->file_raw_len = 0;
        return -1;
    }

    memset(&dst, 0, sizeof(dst));
    dst.format = SDL_AUDIO_F32;
    dst.channels = (int)BSP_AUDIO_CHANNELS;
    dst.freq = (int)BSP_AUDIO_SAMPLE_RATE_HZ;

    rx->stream = SDL_CreateAudioStream(&rx->file_spec, &dst);
    if (!rx->stream) {
        ERROR_LOG("RX file '%s' create stream failed: %s",
                  rx->map->name ? rx->map->name : "?", SDL_GetError());
        SDL_free(rx->file_raw);
        rx->file_raw = NULL;
        return -1;
    }

    rx->loops_done = 0;
    if (rx_file_feed_stream(rx) != 0) {
        SDL_DestroyAudioStream(rx->stream);
        rx->stream = NULL;
        SDL_free(rx->file_raw);
        rx->file_raw = NULL;
        return -1;
    }

    INFO_LOG("RX file '%s': '%s' %d Hz ch=%d",
             rx->map->name ? rx->map->name : "?",
             path, rx->file_spec.freq, rx->file_spec.channels);
    return 0;
}

static void rx_close(rx_runtime_t* rx)
{
    if (rx->stream) {
        SDL_DestroyAudioStream(rx->stream);
        rx->stream = NULL;
    }
    if (rx->file_raw) {
        SDL_free(rx->file_raw);
        rx->file_raw = NULL;
        rx->file_raw_len = 0;
    }
    rx->accum_floats = 0;
    rx->file_eof = 0;
    rx->sine_phase = 0.0f;
}

static void apply_gain(float* buf, uint32_t n, float gain)
{
    uint32_t i;
    if (gain == 1.0f) {
        return;
    }
    for (i = 0; i < n; i++) {
        buf[i] *= gain;
    }
}

static float rx_gain_of(const rx_runtime_t* rx)
{
    if (rx->map->rx_impl_type == RX_IMPL_TYPE_SYSTEM_OUTPUT_LOOPBACK) {
        return rx->map->config.system_output_loopback.gain;
    }
    if (rx->map->rx_impl_type == RX_IMPL_TYPE_FILE) {
        return rx->map->config.file.gain;
    }
    return 1.0f;
}

static int rx_pull_sine_frame(rx_runtime_t* rx, float* dst)
{
    float phase = rx->sine_phase;
    float freq = rx->map->config.sine_wave.frequency;
    float amp = rx->map->config.sine_wave.amplitude;
    float step;
    uint32_t n;

    if (freq < 0.0f) {
        freq = 0.0f;
    }
    if (amp < 0.0f) {
        amp = 0.0f;
    }
    step = AUDIO_TWO_PI_F * freq / (float)BSP_AUDIO_SAMPLE_RATE_HZ;

    for (n = 0; n < BSP_AUDIO_FRAME_SAMPLES; n++) {
        float s = sinf(phase) * amp;
        dst[2u * n] = s;
        dst[2u * n + 1u] = s;
        phase += step;
        if (phase >= AUDIO_TWO_PI_F) {
            phase -= AUDIO_TWO_PI_F;
        }
    }
    rx->sine_phase = phase;
    return 1;
}

/* 尽量凑满一帧；不足则静音补齐。返回 1=本通道本帧有真实数据。 */
static int rx_pull_stream_frame(rx_runtime_t* rx, float* dst)
{
    float temp[AUDIO_FRAME_FLOATS];
    int got_real = 0;

    memset(dst, 0, AUDIO_FRAME_BYTES);

    if (!rx->stream) {
        return 0;
    }

    while (rx->accum_floats < AUDIO_FRAME_FLOATS) {
        uint32_t need = AUDIO_FRAME_FLOATS - rx->accum_floats;
        int avail = SDL_GetAudioStreamAvailable(rx->stream);
        int want;
        int got;

        if (avail < (int)sizeof(float)) {
            if (rx->map->rx_impl_type == RX_IMPL_TYPE_FILE && !rx->file_eof) {
                const uint8_t do_loop = (uint8_t)rx->map->config.file.loop;
                const uint32_t loop_cnt = rx->map->config.file.loop_cnt;
                if (do_loop && (loop_cnt == 0u || rx->loops_done + 1u < loop_cnt)) {
                    rx->loops_done++;
                    if (rx_file_feed_stream(rx) == 0) {
                        continue;
                    }
                }
                rx->file_eof = 1;
            }
            break;
        }

        want = (int)(need * sizeof(float));
        if (avail < want) {
            want = avail - (avail % (int)sizeof(float));
        }
        if (want <= 0) {
            break;
        }

        got = SDL_GetAudioStreamData(rx->stream, temp, want);
        if (got <= 0) {
            break;
        }
        {
            uint32_t nf = (uint32_t)got / (uint32_t)sizeof(float);
            memcpy(&rx->accum[rx->accum_floats], temp, nf * sizeof(float));
            rx->accum_floats += nf;
            got_real = 1;
        }
    }

    if (rx->accum_floats >= AUDIO_FRAME_FLOATS) {
        memcpy(dst, rx->accum, AUDIO_FRAME_BYTES);
        rx->accum_floats = 0;
        got_real = 1;
    } else if (rx->accum_floats > 0u) {
        memcpy(dst, rx->accum, rx->accum_floats * sizeof(float));
        rx->accum_floats = 0;
        got_real = 1;
    }

    apply_gain(dst, AUDIO_FRAME_FLOATS, rx_gain_of(rx));
    return got_real;
}

static int rx_pull_frame(rx_runtime_t* rx, float* dst)
{
    if (rx->map->rx_impl_type == RX_IMPL_TYPE_SINE_WAVE) {
        return rx_pull_sine_frame(rx, dst);
    }
    return rx_pull_stream_frame(rx, dst);
}

/* -------------------------------------------------------------------------- */
/* TX：file / system_output                                                   */
/* -------------------------------------------------------------------------- */

static int tx_open_system_output(tx_runtime_t* tx)
{
    SDL_AudioSpec spec;

    memset(&spec, 0, sizeof(spec));
    spec.format = SDL_AUDIO_F32;
    spec.channels = (int)BSP_AUDIO_CHANNELS;
    spec.freq = (int)BSP_AUDIO_SAMPLE_RATE_HZ;

    tx->play = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!tx->play) {
        ERROR_LOG("TX '%s' open system output failed: %s",
                  tx->map->name ? tx->map->name : "?", SDL_GetError());
        return -1;
    }
    if (!SDL_ResumeAudioStreamDevice(tx->play)) {
        ERROR_LOG("TX '%s' resume system output failed: %s",
                  tx->map->name ? tx->map->name : "?", SDL_GetError());
        SDL_DestroyAudioStream(tx->play);
        tx->play = NULL;
        return -1;
    }
    return 0;
}

static void tx_close(tx_runtime_t* tx)
{
    wav_close(&tx->wav);
    if (tx->play) {
        SDL_DestroyAudioStream(tx->play);
        tx->play = NULL;
    }
}

static void tx_push_frame(tx_runtime_t* tx, const float* interleaved)
{
    if (tx->map->tx_impl_type == TX_IMPL_TYPE_FILE) {
        (void)wav_write_float_frame(&tx->wav, interleaved, AUDIO_FRAME_FLOATS,
                                    tx->map->config.file.gain);
        return;
    }

    if (tx->map->tx_impl_type == TX_IMPL_TYPE_SYSTEM_OUTPUT && tx->play) {
        float gain = tx->map->config.system_output.gain;
        float scaled[AUDIO_FRAME_FLOATS];
        const float* src = interleaved;
        int queued = SDL_GetAudioStreamQueued(tx->play);

        /* 避免堆积过大延迟：队列超过上限则丢本帧 */
        if (queued > (int)(AUDIO_TX_PLAY_MAX_QUEUED_FRAMES * AUDIO_FRAME_BYTES)) {
            return;
        }

        if (gain != 1.0f) {
            uint32_t i;
            for (i = 0; i < AUDIO_FRAME_FLOATS; i++) {
                scaled[i] = interleaved[i] * gain;
            }
            src = scaled;
        }
        if (!SDL_PutAudioStreamData(tx->play, src, (int)AUDIO_FRAME_BYTES)) {
            WARN_LOG("TX '%s' put playback failed: %s",
                     tx->map->name ? tx->map->name : "?", SDL_GetError());
        }
    }
}

/* -------------------------------------------------------------------------- */
/* 帧处理                                                                     */
/* -------------------------------------------------------------------------- */

static void process_one_frame(void)
{
    bsp_audio_frame_t tx_frames[BSP_AUDIO_TX_MAX];
    bsp_audio_frame_t rx_frames[BSP_AUDIO_RX_MAX];
    bsp_audio_system_frame_info_t info;
    bsp_audio_handler_t handler = s_user_handler;
    uint8_t i;

    for (i = 0; i < s_rx_cnt; i++) {
        (void)rx_pull_frame(&s_rx[i], s_rx_float[i]);
    }

    memset(s_tx_float, 0, sizeof(s_tx_float));
    for (i = 0; i < s_tx_cnt; i++) {
        tx_frames[i].stream_buf = s_tx_float[i];
        tx_frames[i].buf_size = AUDIO_FRAME_BYTES;
    }
    for (i = 0; i < s_rx_cnt; i++) {
        rx_frames[i].stream_buf = s_rx_float[i];
        rx_frames[i].buf_size = AUDIO_FRAME_BYTES;
    }

    info.sample_rate = BSP_AUDIO_SAMPLE_RATE_HZ;
    info.tx_frame = tx_frames;
    info.tx_frame_cnt = s_tx_cnt;
    info.rx_frame = rx_frames;
    info.rx_frame_cnt = s_rx_cnt;
    info.frame_sample_cnt = BSP_AUDIO_FRAME_SAMPLES;

    if (handler) {
        handler(&info);
    }

    for (i = 0; i < s_tx_cnt; i++) {
        tx_push_frame(&s_tx[i], s_tx_float[i]);
    }

    energy_store_all();
    arch_display_request_frame();
    s_frames_done++;
}

static void audio_polling_task(void* arg)
{
    (void)arg;
    if (!s_initialized) {
        return;
    }

    SDL_LockMutex(s_lock);

    /* 统一按墙钟推帧：loopback 不足时该 RX 补静音，TX/文件仍按采样率前进 */
    {
        uint64_t now = SDL_GetTicksNS();
        uint64_t expect = (now - s_start_ns) * (uint64_t)BSP_AUDIO_SAMPLE_RATE_HZ /
                          1000000000ull / (uint64_t)BSP_AUDIO_FRAME_SAMPLES;
        while (s_frames_done < expect) {
            process_one_frame();
        }
    }

    SDL_UnlockMutex(s_lock);
}

/* -------------------------------------------------------------------------- */
/* init / deinit                                                              */
/* -------------------------------------------------------------------------- */

static void audio_shutdown_partial(void)
{
    uint8_t i;

    if (s_polling_task) {
        e_polling_task_destroy(s_polling_task);
        s_polling_task = NULL;
    }
    arch_display_remove_render_cb(audio_render_cb, NULL);

    for (i = 0; i < s_tx_cnt; i++) {
        tx_close(&s_tx[i]);
    }
    for (i = 0; i < s_rx_cnt; i++) {
        rx_close(&s_rx[i]);
    }
    s_tx_cnt = 0;
    s_rx_cnt = 0;

    if (s_lock) {
        SDL_DestroyMutex(s_lock);
        s_lock = NULL;
    }
    if (s_audio_subsystem_inited) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        s_audio_subsystem_inited = 0;
    }
}

int bsp_audio_system_init(void)
{
    e_polling_task_desc_t poll_desc;
    uint8_t i;
    size_t rx_n;
    size_t tx_n;

    if (s_initialized) {
        return -1;
    }

    rx_n = sizeof(s_rx_map) / sizeof(s_rx_map[0]);
    tx_n = sizeof(s_tx_map) / sizeof(s_tx_map[0]);
    if (rx_n == 0u && tx_n == 0u) {
        ERROR_LOG("RX/TX map tables are both empty");
        return -1;
    }
    if (rx_n > BSP_AUDIO_RX_MAX || tx_n > BSP_AUDIO_TX_MAX) {
        ERROR_LOG("map table too large (rx=%u/%u tx=%u/%u)",
                  (unsigned)rx_n, (unsigned)BSP_AUDIO_RX_MAX,
                  (unsigned)tx_n, (unsigned)BSP_AUDIO_TX_MAX);
        return -1;
    }

    s_lock = SDL_CreateMutex();
    if (!s_lock) {
        ERROR_LOG("create mutex failed: %s", SDL_GetError());
        return -1;
    }

    SDL_SetHint(SDL_HINT_AUDIO_INCLUDE_MONITORS, "1");
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        ERROR_LOG("SDL_InitSubSystem(AUDIO) failed: %s", SDL_GetError());
        SDL_DestroyMutex(s_lock);
        s_lock = NULL;
        return -1;
    }
    s_audio_subsystem_inited = 1;

    memset(s_rx, 0, sizeof(s_rx));
    memset(s_tx, 0, sizeof(s_tx));
    s_rx_cnt = (uint8_t)rx_n;
    s_tx_cnt = (uint8_t)tx_n;

    for (i = 0; i < s_rx_cnt; i++) {
        s_rx[i].map = &s_rx_map[i];
        if (s_rx_map[i].rx_impl_type == RX_IMPL_TYPE_SYSTEM_OUTPUT_LOOPBACK) {
            if (rx_open_loopback(&s_rx[i]) != 0) {
                audio_shutdown_partial();
                return -1;
            }
        } else if (s_rx_map[i].rx_impl_type == RX_IMPL_TYPE_FILE) {
            if (rx_open_file(&s_rx[i]) != 0) {
                audio_shutdown_partial();
                return -1;
            }
        } else if (s_rx_map[i].rx_impl_type == RX_IMPL_TYPE_SINE_WAVE) {
            s_rx[i].sine_phase = 0.0f;
            INFO_LOG("RX[%u] '%s' sine %.1f Hz amp=%.3f",
                     (unsigned)i,
                     s_rx_map[i].name ? s_rx_map[i].name : "?",
                     (double)s_rx_map[i].config.sine_wave.frequency,
                     (double)s_rx_map[i].config.sine_wave.amplitude);
        } else {
            ERROR_LOG("RX[%u] unknown impl_type=%u", (unsigned)i, s_rx_map[i].rx_impl_type);
            audio_shutdown_partial();
            return -1;
        }
        if (s_rx_map[i].rx_impl_type != RX_IMPL_TYPE_SINE_WAVE) {
            INFO_LOG("RX[%u] '%s' type=%u",
                     (unsigned)i,
                     s_rx_map[i].name ? s_rx_map[i].name : "?",
                     s_rx_map[i].rx_impl_type);
        }
    }

    for (i = 0; i < s_tx_cnt; i++) {
        s_tx[i].map = &s_tx_map[i];
        if (s_tx_map[i].tx_impl_type == TX_IMPL_TYPE_FILE) {
            const char* path = s_tx_map[i].config.file.file_path;
            if (!path || !path[0]) {
                ERROR_LOG("TX[%u] '%s': empty file_path",
                          (unsigned)i, s_tx_map[i].name ? s_tx_map[i].name : "?");
                audio_shutdown_partial();
                return -1;
            }
            if (wav_open(&s_tx[i].wav, path,
                         BSP_AUDIO_SAMPLE_RATE_HZ, (uint16_t)BSP_AUDIO_CHANNELS) != 0) {
                audio_shutdown_partial();
                return -1;
            }
            INFO_LOG("TX[%u] '%s' file -> '%s'",
                     (unsigned)i,
                     s_tx_map[i].name ? s_tx_map[i].name : "?",
                     path);
        } else if (s_tx_map[i].tx_impl_type == TX_IMPL_TYPE_SYSTEM_OUTPUT) {
            if (tx_open_system_output(&s_tx[i]) != 0) {
                audio_shutdown_partial();
                return -1;
            }
            INFO_LOG("TX[%u] '%s' system output gain=%.3f",
                     (unsigned)i,
                     s_tx_map[i].name ? s_tx_map[i].name : "?",
                     (double)s_tx_map[i].config.system_output.gain);
        } else {
            ERROR_LOG("TX[%u] unknown impl_type=%u", (unsigned)i, s_tx_map[i].tx_impl_type);
            audio_shutdown_partial();
            return -1;
        }
    }

    memset(s_tx_float, 0, sizeof(s_tx_float));
    memset(s_rx_float, 0, sizeof(s_rx_float));
    memset(s_peak_tx, 0, sizeof(s_peak_tx));
    memset(s_peak_rx, 0, sizeof(s_peak_rx));
    for (i = 0; i < BSP_AUDIO_TX_MAX; i++) {
        SDL_SetAtomicInt(&s_energy_tx_q15[i], 0);
    }
    for (i = 0; i < BSP_AUDIO_RX_MAX; i++) {
        SDL_SetAtomicInt(&s_energy_rx_q15[i], 0);
    }

    s_start_ns = SDL_GetTicksNS();
    s_frames_done = 0;

    if (arch_display_add_render_cb(audio_render_cb, NULL) != 0) {
        WARN_LOG("register energy render cb failed");
    }

    memset(&poll_desc, 0, sizeof(poll_desc));
    poll_desc.attr = E_POLLING_TASK_ATTR_DEFAULT;
    poll_desc.freq_hz = BSP_AUDIO_POLL_FREQ_HZ;
    poll_desc.name = "bsp_audio_poll";
    poll_desc.polling_func = audio_polling_task;
    poll_desc.start_before_create = 1;
    poll_desc.user_ctx = NULL;
    s_polling_task = e_polling_task_create(&poll_desc);
    if (!s_polling_task) {
        ERROR_LOG("create polling task failed");
        audio_shutdown_partial();
        return -1;
    }

    s_initialized = 1u;
    INFO_LOG("ready: %u Hz frame=%u tx=%u rx=%u",
             (unsigned)BSP_AUDIO_SAMPLE_RATE_HZ,
             (unsigned)BSP_AUDIO_FRAME_SAMPLES,
             (unsigned)s_tx_cnt,
             (unsigned)s_rx_cnt);
    return 0;
}

void bsp_audio_system_deinit(void)
{
    if (!s_initialized) {
        return;
    }
    s_initialized = 0u;

    if (s_polling_task) {
        e_polling_task_destroy(s_polling_task);
        s_polling_task = NULL;
    }
    arch_display_remove_render_cb(audio_render_cb, NULL);

    if (s_lock) {
        SDL_LockMutex(s_lock);
    }

    {
        uint8_t i;
        for (i = 0; i < s_tx_cnt; i++) {
            tx_close(&s_tx[i]);
        }
        for (i = 0; i < s_rx_cnt; i++) {
            rx_close(&s_rx[i]);
        }
        s_tx_cnt = 0;
        s_rx_cnt = 0;
    }

    if (s_lock) {
        SDL_UnlockMutex(s_lock);
        SDL_DestroyMutex(s_lock);
        s_lock = NULL;
    }

    s_user_handler = NULL;
    if (s_audio_subsystem_inited) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        s_audio_subsystem_inited = 0;
    }
    INFO_LOG("deinit done");
}

void bsp_audio_system_set_handler(bsp_audio_handler_t handler)
{
    s_user_handler = handler;
}

static int bsp_audio_system_bsp_deinit(void)
{
    bsp_audio_system_deinit();
    return 0;
}

REGEDIT_BSP_DEINIT_FUNCTION(bsp_audio_system_bsp_deinit, 1);

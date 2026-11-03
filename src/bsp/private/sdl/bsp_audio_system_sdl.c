/* bsp_audio_system_sdl.c — bsp_audio_system 的 SDL3 后端。
 *
 * 编译期（bsp_cfg.h）固定：采样率 / 帧长 / RX·TX 通道数。
 * 运行时（点击能量框）每路选实现类型；参数用本文件内置默认值，cfg 不配类型。
 *   RX: NONE / SYS SPK(环回) / SYS MIC / FILE / DEMO / SINE
 *   TX: NONE / FILE / SYS
 * NONE = 关闭该路。启动时全部为 NONE。
 * RX DEMO：播构建嵌入的 demo.wav（g_embedded_demo_wav）；FILE 仍读可配置磁盘路径。
 *
 * 一帧 TX 缓存：每拍先推送上一拍 handler 结果，再拉 RX；handler 在 s_lock 外执行，
 * 长时间占用不堵推帧 / reopen。首拍输出静音。
 *
 * 线程：e_polling_pool 凑帧与 UI 点击；arch 主线程画能量条/配置页。
 */
#include "bsp/bsp_audio_system.h"

#include "arch/arch_display.h"
#include "arch/arch_input.h"

#include "framework/c_function_regedit.h"
#include "framework/e_log.h"
#include "framework/e_polling_pool.h"

#include "bsp_cfg.h"

#include <SDL3/SDL.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

/* 由 cmake embed_bin_attach(demo.wav) 生成 */
extern const unsigned char g_embedded_demo_wav[];
extern const unsigned int  g_embedded_demo_wav_size;

#define TAG "bsp_audio_system"
#define INFO_LOG(...)  E_INFO_LOG(TAG, __VA_ARGS__)
#define WARN_LOG(...)  E_WARN_LOG(TAG, __VA_ARGS__)
#define ERROR_LOG(...) E_ERROR_LOG(TAG, __VA_ARGS__)

typedef enum rx_impl_type_e {
    RX_IMPL_TYPE_NONE = 0,
    RX_IMPL_TYPE_SYSTEM_OUTPUT_LOOPBACK = 1, /* 系统扬声器环回 → UI: SYS SPK */
    RX_IMPL_TYPE_SYSTEM_MIC = 2,             /* 系统麦克风     → UI: SYS MIC */
    RX_IMPL_TYPE_FILE = 3,                   /* 磁盘 WAV，可配路径 */
    RX_IMPL_TYPE_DEMO = 4,                   /* 内嵌 demo.wav */
    RX_IMPL_TYPE_SINE_WAVE = 5,
} rx_impl_type_e;

typedef enum tx_impl_type_e {
    TX_IMPL_TYPE_NONE = 0,
    TX_IMPL_TYPE_FILE = 1,
    TX_IMPL_TYPE_SYSTEM_OUTPUT = 2,
} tx_impl_type_e;

/* 运行时通道状态（非 cfg）；类型参数切到该 type 时填内置默认值。 */
typedef struct bsp_audio_system_rx_map_t {
    rx_impl_type_e rx_impl_type;
    const char*    name;
    union {
        struct {
            float gain;
        } system_output_loopback;
        struct {
            float gain;
        } system_mic;
        struct {
            const char* file_path; /* 指向 path 缓冲 */
            float       gain;
            uint32_t    loop : 1;
            uint32_t    reserve : 31;
            uint32_t    loop_cnt;
        } file;
        struct {
            float    gain;
            uint32_t loop : 1;
            uint32_t reserve : 31;
            uint32_t loop_cnt;
        } demo;
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

#ifndef BSP_AUDIO_SAMPLE_RATE_HZ
#error "bsp_cfg.h must define BSP_AUDIO_SAMPLE_RATE_HZ"
#endif
#ifndef BSP_AUDIO_FRAME_SAMPLES
#error "bsp_cfg.h must define BSP_AUDIO_FRAME_SAMPLES"
#endif
#ifndef BSP_AUDIO_RX_CHANNELS
#error "bsp_cfg.h must define BSP_AUDIO_RX_CHANNELS"
#endif
#ifndef BSP_AUDIO_TX_CHANNELS
#error "bsp_cfg.h must define BSP_AUDIO_TX_CHANNELS"
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
#ifndef BSP_AUDIO_UI_X
#define BSP_AUDIO_UI_X              8
#endif
#ifndef BSP_AUDIO_UI_Y
#define BSP_AUDIO_UI_Y              0
#endif
#ifndef BSP_AUDIO_PANEL_W
#define BSP_AUDIO_PANEL_W           280
#endif
#ifndef BSP_AUDIO_METER_H
#define BSP_AUDIO_METER_H           140
#endif
#ifndef BSP_AUDIO_METER_GAP
#define BSP_AUDIO_METER_GAP         8
#endif
/* 显示条：固定 10Hz 步进，覆盖 [0 .. AUDIO_SPEC_FMAX_HZ) → 200 根 */
#ifndef AUDIO_SPEC_STEP_HZ
#define AUDIO_SPEC_STEP_HZ          10u
#endif
#ifndef AUDIO_SPEC_FMAX_HZ
#define AUDIO_SPEC_FMAX_HZ          2000u
#endif
#if ((AUDIO_SPEC_FMAX_HZ) % (AUDIO_SPEC_STEP_HZ)) != 0
#error "AUDIO_SPEC_FMAX_HZ must be divisible by AUDIO_SPEC_STEP_HZ"
#endif
#define AUDIO_SPEC_BINS             ((AUDIO_SPEC_FMAX_HZ) / (AUDIO_SPEC_STEP_HZ))
/* 频谱分析窗：须为 2 的幂；4096 → Δf≈10.8Hz @44.1kHz，对齐 10Hz 显示步进 */
#ifndef AUDIO_SPEC_FFT_N
#define AUDIO_SPEC_FFT_N            4096u
#endif
#if (AUDIO_SPEC_FFT_N) < (BSP_AUDIO_FRAME_SAMPLES)
#error "AUDIO_SPEC_FFT_N must be >= BSP_AUDIO_FRAME_SAMPLES"
#endif
#if (AUDIO_SPEC_FFT_N & (AUDIO_SPEC_FFT_N - 1u)) != 0
#error "AUDIO_SPEC_FFT_N must be a power of 2"
#endif

#if (BSP_AUDIO_RX_CHANNELS) > (BSP_AUDIO_RX_MAX)
#error "BSP_AUDIO_RX_CHANNELS exceeds BSP_AUDIO_RX_MAX"
#endif
#if (BSP_AUDIO_TX_CHANNELS) > (BSP_AUDIO_TX_MAX)
#error "BSP_AUDIO_TX_CHANNELS exceeds BSP_AUDIO_TX_MAX"
#endif

#define AUDIO_FRAME_FLOATS   (BSP_AUDIO_FRAME_SAMPLES * BSP_AUDIO_CHANNELS)
#define AUDIO_FRAME_BYTES    (AUDIO_FRAME_FLOATS * (uint32_t)sizeof(float))
#define AUDIO_ENERGY_DECAY   0.92f
#define AUDIO_TX_PLAY_MAX_QUEUED_FRAMES  16u
#define AUDIO_TX_PLAY_CATCHUP_HEADROOM   2u /* 队列剩余不足此帧数则暂停追帧 */
#define AUDIO_TWO_PI_F                   6.28318530717958647692f
#define AUDIO_PATH_MAX                   160
#define AUDIO_NAME_MAX                   12
#define AUDIO_PATH_FIELD_H               18.0f
#define AUDIO_ENERGY_UI_PERIOD_NS        33333333ull /* ~30 Hz 刷新能量条 */

/* 配置页布局：绘制与命中必须共用同一套常量（对齐 YouWei） */
#define AUDIO_CFG_LIST_W                 96.0f
#define AUDIO_CFG_ROW_H                  18.0f
#define AUDIO_CFG_ROW_GAP                2.0f
#define AUDIO_CFG_HEADER_H               22.0f
#define AUDIO_CFG_TITLE_H                16.0f
#define AUDIO_CFG_TYPE_BTN_W             48.0f
#define AUDIO_CFG_TYPE_BTN_H             18.0f
#define AUDIO_CFG_TYPE_BTN_GAP           4.0f
#define AUDIO_CFG_GAIN_BTN_W             22.0f
#define AUDIO_CFG_GAIN_BTN_X0            100.0f
#define AUDIO_CFG_GAIN_BTN_X1            126.0f
#define AUDIO_CFG_LOOP_BTN_X             154.0f
#define AUDIO_CFG_LOOP_BTN_W             40.0f
#define AUDIO_CFG_CLOSE_X                28.0f
#define AUDIO_CFG_CLOSE_W                22.0f
#define AUDIO_CFG_CLOSE_H                16.0f
#define AUDIO_CFG_FOOT_H                 22.0f
#define AUDIO_CFG_FOOT_BTN_W             52.0f
#define AUDIO_CFG_FOOT_BTN_H             18.0f
#define AUDIO_CFG_APPLY_OFF              120.0f
#define AUDIO_CFG_CLOSE_OFF              62.0f

/* 各 type 内置默认（cfg 不配） */
#define AUDIO_DEFAULT_RX_WAV_PATH   "audio_rx.wav"
#define AUDIO_DEFAULT_TX_WAV_PATH   "audio_tx.wav"
#define AUDIO_DEFAULT_GAIN          1.0f
#define AUDIO_DEFAULT_SINE_FREQ_HZ  1000.0f
#define AUDIO_DEFAULT_SINE_AMP      0.2f

typedef struct wav_writer_t {
    SDL_IOStream* io;
    uint32_t      data_bytes;
    uint32_t      sample_rate;
    uint16_t      channels;
} wav_writer_t;

typedef struct rx_runtime_t {
    bsp_audio_system_rx_map_t* map; /* 指向可变 cfg */
    SDL_AudioStream* stream;
    uint8_t*         file_raw;
    uint32_t         file_raw_len;
    SDL_AudioSpec    file_spec;
    uint32_t         loops_done;
    uint8_t          file_eof;
    float            sine_phase;
    float            accum[AUDIO_FRAME_FLOATS];
    uint32_t         accum_floats;
} rx_runtime_t;

typedef struct tx_runtime_t {
    bsp_audio_system_tx_map_t* map;
    wav_writer_t     wav;
    SDL_AudioStream* play;
} tx_runtime_t;

typedef enum audio_ui_mode_e {
    AUDIO_UI_ENERGY = 0,
    AUDIO_UI_CONFIG = 1,
} audio_ui_mode_e;

/* TX 双缓冲：play 本拍推送；work 给 handler 填下一拍。 */
static float s_tx_bank[2][BSP_AUDIO_TX_MAX][AUDIO_FRAME_FLOATS];
static uint8_t s_tx_play_bank;
static float s_rx_float[BSP_AUDIO_RX_MAX][AUDIO_FRAME_FLOATS];

static bsp_audio_system_rx_map_t s_rx_cfg[BSP_AUDIO_RX_MAX];
static bsp_audio_system_tx_map_t s_tx_cfg[BSP_AUDIO_TX_MAX];
static bsp_audio_system_rx_map_t s_rx_draft[BSP_AUDIO_RX_MAX];
static bsp_audio_system_tx_map_t s_tx_draft[BSP_AUDIO_TX_MAX];

/* 可编辑路径缓冲 + 通道名 */
static char s_rx_path[BSP_AUDIO_RX_MAX][AUDIO_PATH_MAX];
static char s_tx_path[BSP_AUDIO_TX_MAX][AUDIO_PATH_MAX];
static char s_rx_path_draft[BSP_AUDIO_RX_MAX][AUDIO_PATH_MAX];
static char s_tx_path_draft[BSP_AUDIO_TX_MAX][AUDIO_PATH_MAX];
static char s_rx_name[BSP_AUDIO_RX_MAX][AUDIO_NAME_MAX];
static char s_tx_name[BSP_AUDIO_TX_MAX][AUDIO_NAME_MAX];

#ifdef BSP_AUDIO_RX_NAMES
static const char* const s_rx_names_cfg[] = BSP_AUDIO_RX_NAMES;
#endif
#ifdef BSP_AUDIO_TX_NAMES
static const char* const s_tx_names_cfg[] = BSP_AUDIO_TX_NAMES;
#endif

static rx_runtime_t s_rx[BSP_AUDIO_RX_MAX];
static tx_runtime_t s_tx[BSP_AUDIO_TX_MAX];
static uint8_t s_rx_cnt; /* = BSP_AUDIO_RX_CHANNELS，编译期固定 */
static uint8_t s_tx_cnt;

static bsp_audio_handler_t volatile s_user_handler;
static volatile uint8_t s_initialized;

static e_polling_task_handle_t s_polling_task;
static SDL_Mutex* s_lock;
static uint64_t s_start_ns;
static uint64_t s_frames_done;
static uint64_t s_last_energy_ui_ns;
static uint8_t s_audio_subsystem_inited;

static SDL_AtomicInt s_energy_tx_q15[BSP_AUDIO_TX_MAX];
static SDL_AtomicInt s_energy_rx_q15[BSP_AUDIO_RX_MAX];
static float s_peak_tx[BSP_AUDIO_TX_MAX];
static float s_peak_rx[BSP_AUDIO_RX_MAX];
static float s_spec_tx[BSP_AUDIO_TX_MAX][AUDIO_SPEC_BINS];
static float s_spec_rx[BSP_AUDIO_RX_MAX][AUDIO_SPEC_BINS];
static float s_pkhz_tx[BSP_AUDIO_TX_MAX];
static float s_pkhz_rx[BSP_AUDIO_RX_MAX];
/* 单声道环形分析缓冲：写指针指向下一写入位置 */
static float s_spec_hist_tx[BSP_AUDIO_TX_MAX][AUDIO_SPEC_FFT_N];
static float s_spec_hist_rx[BSP_AUDIO_RX_MAX][AUDIO_SPEC_FFT_N];
static uint32_t s_spec_hist_pos_tx[BSP_AUDIO_TX_MAX];
static uint32_t s_spec_hist_pos_rx[BSP_AUDIO_RX_MAX];
static uint32_t s_spec_hist_fill_tx[BSP_AUDIO_TX_MAX];
static uint32_t s_spec_hist_fill_rx[BSP_AUDIO_RX_MAX];

/* UI */
static audio_ui_mode_e s_ui_mode = AUDIO_UI_ENERGY;
static int s_ui_sel;          /* 0..tx_cnt-1 = TX；tx_cnt.. = RX */
static uint8_t s_mouse_was_down;
static SDL_Mutex* s_ui_lock;
static uint8_t s_path_editing;     /* 1=正在编辑当前选中通道的 WAV 路径 */
static uint8_t s_text_watch_on;
static uint32_t s_caret_blink_ms;

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
/* 右侧频率表 UI                                                              */
/* -------------------------------------------------------------------------- */

static const char* rx_type_name(rx_impl_type_e t);
static const char* tx_type_name(tx_impl_type_e t);

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

/* 将一帧交错立体声压成单声道，写入环形分析窗。 */
static void spec_hist_push(float* hist, uint32_t* pos, uint32_t* fill,
                           const float* interleaved)
{
    uint32_t i;
    uint32_t p = *pos;
    for (i = 0; i < BSP_AUDIO_FRAME_SAMPLES; i++) {
        hist[p] = 0.5f * (interleaved[2u * i] + interleaved[2u * i + 1u]);
        p++;
        if (p >= AUDIO_SPEC_FFT_N) {
            p = 0u;
        }
    }
    *pos = p;
    if (*fill < AUDIO_SPEC_FFT_N) {
        *fill += BSP_AUDIO_FRAME_SAMPLES;
        if (*fill > AUDIO_SPEC_FFT_N) {
            *fill = AUDIO_SPEC_FFT_N;
        }
    }
}

/* 原地 radix-2 DIT FFT（n 为 2 的幂）。 */
static void fft_radix2(float* re, float* im, uint32_t n)
{
    uint32_t i;
    uint32_t j = 0u;
    uint32_t len;

    for (i = 0u; i < n - 1u; i++) {
        uint32_t bit;
        if (i < j) {
            float tr = re[i];
            float ti = im[i];
            re[i] = re[j];
            im[i] = im[j];
            re[j] = tr;
            im[j] = ti;
        }
        bit = n >> 1u;
        while (j & bit) {
            j ^= bit;
            bit >>= 1u;
        }
        j ^= bit;
    }

    for (len = 2u; len <= n; len <<= 1u) {
        uint32_t half = len >> 1u;
        double ang = -(double)AUDIO_TWO_PI_F / (double)len;
        double wlen_re = cos(ang);
        double wlen_im = sin(ang);
        uint32_t p0;
        for (p0 = 0u; p0 < n; p0 += len) {
            double wr = 1.0;
            double wi = 0.0;
            uint32_t k;
            for (k = 0u; k < half; k++) {
                uint32_t i0 = p0 + k;
                uint32_t i1 = i0 + half;
                float tr = (float)(wr * (double)re[i1] - wi * (double)im[i1]);
                float ti = (float)(wr * (double)im[i1] + wi * (double)re[i1]);
                float ur = re[i0];
                float ui = im[i0];
                re[i0] = ur + tr;
                im[i0] = ui + ti;
                re[i1] = ur - tr;
                im[i1] = ui - ti;
                {
                    double nwr = wr * wlen_re - wi * wlen_im;
                    wi = wr * wlen_im + wi * wlen_re;
                    wr = nwr;
                }
            }
        }
    }
}

/*
 * 频谱分析：
 *   一次 FFT；显示条按 AUDIO_SPEC_STEP_HZ 从 0 步进到 AUDIO_SPEC_FMAX_HZ，段内 max-pool；
 *   peak 在显示频段内取最大并抛物线插值。f = k * fs / N。
 */
static float spectrum_update(const float* hist, uint32_t write_pos, uint32_t fill,
                             float* bins_io)
{
    const uint32_t n = AUDIO_SPEC_FFT_N;
    const uint32_t k_nyquist = n / 2u;
    const uint32_t fs = BSP_AUDIO_SAMPLE_RATE_HZ;
    static float s_re[AUDIO_SPEC_FFT_N];
    static float s_im[AUDIO_SPEC_FFT_N];
    static float s_mag[AUDIO_SPEC_FFT_N / 2u + 1u];
    uint32_t b;
    uint32_t i;
    uint32_t idx;
    uint32_t k;
    uint32_t k_fmax;
    uint32_t k_peak = 1u;
    float peak_mag = 0.0f;
    float m_l;
    float m_c;
    float m_r;
    double delta;
    const float scale = 4.0f / (float)n; /* Hann 相干增益×2 + 单边×2/N */

    if (fill < n) {
        return 0.0f;
    }

    idx = write_pos;
    for (i = 0u; i < n; i++) {
        float hann = 0.5f - 0.5f * cosf((float)AUDIO_TWO_PI_F * (float)i / (float)(n - 1u));
        s_re[i] = hist[idx] * hann;
        s_im[i] = 0.0f;
        idx++;
        if (idx >= n) {
            idx = 0u;
        }
    }

    fft_radix2(s_re, s_im, n);

    for (k = 0u; k <= k_nyquist; k++) {
        float mag = scale * sqrtf(s_re[k] * s_re[k] + s_im[k] * s_im[k]);
        if (mag > 1.0f) {
            mag = 1.0f;
        }
        s_mag[k] = mag;
    }

    /* 显示上限对应的 FFT bin（含） */
    k_fmax = (AUDIO_SPEC_FMAX_HZ * n) / fs;
    if (k_fmax > k_nyquist) {
        k_fmax = k_nyquist;
    }

    /* 显示条：每条覆盖 [b*STEP, (b+1)*STEP) Hz，映射到 FFT bin 后取 max */
    for (b = 0u; b < AUDIO_SPEC_BINS; b++) {
        uint32_t f_lo = b * AUDIO_SPEC_STEP_HZ;
        uint32_t f_hi = f_lo + AUDIO_SPEC_STEP_HZ;
        uint32_t k0 = (f_lo * n) / fs;                 /* floor */
        uint32_t k1 = (f_hi * n + fs - 1u) / fs;       /* ceil */
        float mag = 0.0f;
        if (k1 <= k0) {
            k1 = k0 + 1u;
        }
        if (k0 > k_nyquist) {
            bins_io[b] = bins_io[b] * AUDIO_ENERGY_DECAY;
            continue;
        }
        if (k1 > k_nyquist + 1u) {
            k1 = k_nyquist + 1u;
        }
        for (k = k0; k < k1; k++) {
            if (s_mag[k] > mag) {
                mag = s_mag[k];
            }
        }
        if (mag < bins_io[b] * AUDIO_ENERGY_DECAY) {
            mag = bins_io[b] * AUDIO_ENERGY_DECAY;
        }
        bins_io[b] = mag;
    }

    /* peak 跳过 DC，避免直流把读数钉在 0Hz */
    for (k = 1u; k <= k_fmax; k++) {
        if (s_mag[k] > peak_mag) {
            peak_mag = s_mag[k];
            k_peak = k;
        }
    }
    if (peak_mag < 0.02f) {
        return 0.0f;
    }

    m_c = s_mag[k_peak];
    m_l = (k_peak > 1u) ? s_mag[k_peak - 1u] : m_c;
    m_r = (k_peak < k_fmax) ? s_mag[k_peak + 1u] : m_c;
    delta = 0.0;
    if ((2.0f * m_c - m_l - m_r) > 1.0e-6f) {
        delta = 0.5 * (double)(m_r - m_l) / (double)(2.0f * m_c - m_l - m_r);
        if (delta > 0.5) {
            delta = 0.5;
        }
        if (delta < -0.5) {
            delta = -0.5;
        }
    }
    return (float)(((double)k_peak + delta) * (double)fs / (double)n);
}

static void energy_store_all(const float tx[][AUDIO_FRAME_FLOATS],
                             const float rx[][AUDIO_FRAME_FLOATS],
                             int do_spectrum)
{
    uint8_t i;
    for (i = 0; i < s_tx_cnt; i++) {
        float rms = calc_rms(tx[i], AUDIO_FRAME_FLOATS);
        if (rms > 1.0f) rms = 1.0f;
        SDL_SetAtomicInt(&s_energy_tx_q15[i], (int)lrintf(rms * 32767.0f));
        spec_hist_push(s_spec_hist_tx[i], &s_spec_hist_pos_tx[i],
                       &s_spec_hist_fill_tx[i], tx[i]);
        if (do_spectrum) {
            s_pkhz_tx[i] = spectrum_update(s_spec_hist_tx[i], s_spec_hist_pos_tx[i],
                                           s_spec_hist_fill_tx[i], s_spec_tx[i]);
        }
    }
    for (i = 0; i < s_rx_cnt; i++) {
        float rms = calc_rms(rx[i], AUDIO_FRAME_FLOATS);
        if (rms > 1.0f) rms = 1.0f;
        SDL_SetAtomicInt(&s_energy_rx_q15[i], (int)lrintf(rms * 32767.0f));
        spec_hist_push(s_spec_hist_rx[i], &s_spec_hist_pos_rx[i],
                       &s_spec_hist_fill_rx[i], rx[i]);
        if (do_spectrum) {
            s_pkhz_rx[i] = spectrum_update(s_spec_hist_rx[i], s_spec_hist_pos_rx[i],
                                           s_spec_hist_fill_rx[i], s_spec_rx[i]);
        }
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

static int audio_channel_total(void)
{
    return (int)s_tx_cnt + (int)s_rx_cnt;
}

static void audio_meter_rect(int ch_index, float* x, float* y, float* w, float* h)
{
    *x = (float)BSP_AUDIO_UI_X;
    *y = (float)BSP_AUDIO_UI_Y + 22.0f +
         (float)ch_index * ((float)BSP_AUDIO_METER_H + (float)BSP_AUDIO_METER_GAP);
    *w = (float)BSP_AUDIO_PANEL_W; /* 与配置页同宽 */
    *h = (float)BSP_AUDIO_METER_H;
}

static void audio_panel_rect(float* x, float* y, float* w, float* h)
{
    int total = audio_channel_total();
    float meters_h;

    if (total < 1) {
        total = 1;
    }
    meters_h = (float)total * (float)BSP_AUDIO_METER_H +
               (float)(total - 1) * (float)BSP_AUDIO_METER_GAP;

    *x = (float)BSP_AUDIO_UI_X;
    *y = (float)BSP_AUDIO_UI_Y;
    *w = (float)BSP_AUDIO_PANEL_W;
    *h = 22.0f + meters_h + 8.0f;

    if (s_ui_mode == AUDIO_UI_CONFIG && *h < 280.0f) {
        *h = 280.0f;
    }
}

static void draw_mini_energy(struct SDL_Renderer* r, float x, float y, float w, float h,
                             float level, float* peak, int is_tx)
{
    SDL_FRect track = { x, y, w, h };
    SDL_FRect fill;

    if (level > *peak) {
        *peak = level;
    } else {
        *peak *= AUDIO_ENERGY_DECAY;
    }
    if (*peak > 1.0f) {
        *peak = 1.0f;
    }

    SDL_SetRenderDrawColor(r, 40, 46, 54, 255);
    SDL_RenderFillRect(r, &track);

    fill = track;
    fill.w = w * (*peak);
    if (is_tx) {
        SDL_SetRenderDrawColor(r, 50, 120, 60, 200);
    } else {
        SDL_SetRenderDrawColor(r, 40, 100, 140, 200);
    }
    SDL_RenderFillRect(r, &fill);

    fill.w = w * level;
    if (is_tx) {
        SDL_SetRenderDrawColor(r, 90, 220, 110, 255);
    } else {
        SDL_SetRenderDrawColor(r, 70, 190, 230, 255);
    }
    SDL_RenderFillRect(r, &fill);
    SDL_SetRenderDrawColor(r, 90, 100, 115, 255);
    SDL_RenderRect(r, &track);
}

static void draw_spectrum(struct SDL_Renderer* r, float x, float y, float w, float h,
                          const float* bins, int is_tx)
{
    /* 200 根 10Hz 条：无间隙铺满，按浮点边界取整避免累积缝隙 */
    uint32_t b;
    SDL_FRect bg = { x, y, w, h };

    SDL_SetRenderDrawColor(r, 22, 26, 32, 255);
    SDL_RenderFillRect(r, &bg);
    SDL_SetRenderDrawColor(r, 55, 62, 72, 255);
    SDL_RenderRect(r, &bg);

    for (b = 0; b < AUDIO_SPEC_BINS; b++) {
        float mag = bins[b];
        float x0 = x + 1.0f + ((w - 2.0f) * (float)b) / (float)AUDIO_SPEC_BINS;
        float x1 = x + 1.0f + ((w - 2.0f) * (float)(b + 1u)) / (float)AUDIO_SPEC_BINS;
        float bh;
        SDL_FRect bar;
        if (mag < 0.0f) mag = 0.0f;
        if (mag > 1.0f) mag = 1.0f;
        bh = (h - 4.0f) * mag;
        bar.x = x0;
        bar.y = y + h - 2.0f - bh;
        bar.w = x1 - x0;
        if (bar.w < 1.0f) {
            bar.w = 1.0f;
        }
        bar.h = bh;
        if (is_tx) {
            Uint8 g = (Uint8)(80 + (int)(mag * 140.0f));
            SDL_SetRenderDrawColor(r, 60, g, 80, 255);
        } else {
            Uint8 bl = (Uint8)(100 + (int)(mag * 120.0f));
            SDL_SetRenderDrawColor(r, 50, 140, bl, 255);
        }
        SDL_RenderFillRect(r, &bar);
    }
}

static void draw_channel_meter(struct SDL_Renderer* r, float x, float y, float w, float h,
                               const char* name, const char* type_name,
                               float level, float* peak, float pk_hz,
                               const float* bins, int is_tx)
{
    char line[96];
    float header_h = 18.0f;
    float foot_h = 16.0f;
    float ebar_w;
    float spec_y;
    float spec_h;
    float db;
    SDL_FRect card = { x, y, w, h };

    SDL_SetRenderDrawColor(r, 28, 32, 38, 255);
    SDL_RenderFillRect(r, &card);
    SDL_SetRenderDrawColor(r, is_tx ? 70 : 60, is_tx ? 110 : 100, is_tx ? 80 : 120, 255);
    SDL_RenderRect(r, &card);

    /* 标题：通道名 + type */
    snprintf(line, sizeof(line), "%s %s", name ? name : "?", type_name ? type_name : "?");
    SDL_SetRenderDrawColor(r, 210, 220, 230, 255);
    SDL_RenderDebugText(r, x + 4.0f, y + 3.0f, line);

    /* 右上角简易能量条 */
    ebar_w = w * 0.28f;
    if (ebar_w < 36.0f) {
        ebar_w = 36.0f;
    }
    if (ebar_w > 72.0f) {
        ebar_w = 72.0f;
    }
    draw_mini_energy(r, x + w - ebar_w - 4.0f, y + 3.0f, ebar_w, 12.0f, level, peak, is_tx);

    spec_y = y + header_h;
    spec_h = h - header_h - foot_h;
    if (spec_h < 24.0f) {
        spec_h = 24.0f;
    }
    draw_spectrum(r, x + 3.0f, spec_y, w - 6.0f, spec_h, bins, is_tx);

    db = (level > 1.0e-6f) ? (20.0f * log10f(level)) : -80.0f;
    if (pk_hz >= 1000.0f) {
        snprintf(line, sizeof(line), "rms:%5.1fdB  pk:%.1fkHz", (double)db,
                 (double)(pk_hz * 0.001f));
    } else {
        snprintf(line, sizeof(line), "rms:%5.1fdB  pk:%.0fHz", (double)db, (double)pk_hz);
    }
    SDL_SetRenderDrawColor(r, 160, 170, 185, 255);
    SDL_RenderDebugText(r, x + 4.0f, y + h - foot_h + 1.0f, line);
}

/* 仅 FILE 类型可写 file_path：与 sine/gain 等同属 union，乱写会踩参数。 */
static void bind_live_path_ptrs(void)
{
    uint8_t i;
    for (i = 0; i < s_rx_cnt; i++) {
        if (s_rx_cfg[i].rx_impl_type == RX_IMPL_TYPE_FILE) {
            s_rx_cfg[i].config.file.file_path = s_rx_path[i];
        }
    }
    for (i = 0; i < s_tx_cnt; i++) {
        if (s_tx_cfg[i].tx_impl_type == TX_IMPL_TYPE_FILE) {
            s_tx_cfg[i].config.file.file_path = s_tx_path[i];
        }
    }
}

static void bind_draft_path_ptrs(void)
{
    uint8_t i;
    for (i = 0; i < s_rx_cnt; i++) {
        if (s_rx_draft[i].rx_impl_type == RX_IMPL_TYPE_FILE) {
            s_rx_draft[i].config.file.file_path = s_rx_path_draft[i];
        }
    }
    for (i = 0; i < s_tx_cnt; i++) {
        if (s_tx_draft[i].tx_impl_type == TX_IMPL_TYPE_FILE) {
            s_tx_draft[i].config.file.file_path = s_tx_path_draft[i];
        }
    }
}

static char* draft_path_buf_for_sel(void)
{
    int is_tx = (s_ui_sel < (int)s_tx_cnt);
    int idx = is_tx ? s_ui_sel : (s_ui_sel - (int)s_tx_cnt);
    if (is_tx && idx >= 0 && idx < (int)s_tx_cnt &&
        s_tx_draft[idx].tx_impl_type == TX_IMPL_TYPE_FILE) {
        return s_tx_path_draft[idx];
    }
    if (!is_tx && idx >= 0 && idx < (int)s_rx_cnt &&
        s_rx_draft[idx].rx_impl_type == RX_IMPL_TYPE_FILE) {
        return s_rx_path_draft[idx];
    }
    return NULL;
}

static void path_edit_stop(void)
{
    SDL_Window* win = arch_display_window();
    s_path_editing = 0;
    if (win) {
        SDL_StopTextInput(win);
    }
}

static void path_edit_start(void)
{
    SDL_Window* win = arch_display_window();
    char* buf = draft_path_buf_for_sel();
    if (!buf) {
        return;
    }
    s_path_editing = 1;
    s_caret_blink_ms = (uint32_t)SDL_GetTicks();
    if (win) {
        SDL_StartTextInput(win);
    }
}

static bool SDLCALL audio_text_event_watch(void* userdata, SDL_Event* event)
{
    char* buf;
    size_t len;
    (void)userdata;

    if (!s_path_editing || s_ui_mode != AUDIO_UI_CONFIG) {
        return true;
    }
    buf = draft_path_buf_for_sel();
    if (!buf) {
        return true;
    }

    if (event->type == SDL_EVENT_TEXT_INPUT && event->text.text) {
        len = strlen(buf);
        SDL_strlcat(buf, event->text.text, AUDIO_PATH_MAX);
        if (strlen(buf) != len) {
            arch_display_request_frame();
        }
    } else if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.key == SDLK_BACKSPACE) {
            len = strlen(buf);
            if (len > 0) {
                buf[len - 1] = '\0';
                arch_display_request_frame();
            }
        } else if (event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER ||
                   event->key.key == SDLK_ESCAPE) {
            path_edit_stop();
            arch_display_request_frame();
        }
    }
    return true;
}

static int point_in_rect(float px, float py, float x, float y, float w, float h)
{
    return (px >= x && px < x + w && py >= y && py < y + h);
}

/* -------------------------------------------------------------------------- */
/* 配置页绘制                                                                 */
/* -------------------------------------------------------------------------- */

static void draw_btn(struct SDL_Renderer* r, float x, float y, float w, float h,
                     const char* label, int active)
{
    SDL_FRect rc = { x, y, w, h };
    if (active) {
        SDL_SetRenderDrawColor(r, 70, 120, 160, 255);
    } else {
        SDL_SetRenderDrawColor(r, 50, 56, 64, 255);
    }
    SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 100, 110, 125, 255);
    SDL_RenderRect(r, &rc);
    if (label) {
        float tw = (float)debug_text_width(label);
        SDL_SetRenderDrawColor(r, 220, 230, 240, 255);
        SDL_RenderDebugText(r, x + (w - tw) * 0.5f,
                            y + (h - (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) * 0.5f,
                            label);
    }
}

/* type 按钮自动换行：放不下就另起一行，避免超出配置子区宽度。
 * 返回下一内容行的 y（末行底 + 间距）。 */
static float draw_type_btn_wrap(struct SDL_Renderer* r,
                                float x, float y, float area_w,
                                float btn_w, float btn_h, float gap,
                                const char* const* labels, const int* active, int n)
{
    float cx = x;
    float cy = y;
    int i;

    for (i = 0; i < n; i++) {
        if (i > 0 && (cx + btn_w) > (x + area_w + 0.5f)) {
            cx = x;
            cy += btn_h + gap;
        }
        draw_btn(r, cx, cy, btn_w, btn_h, labels[i], active ? active[i] : 0);
        cx += btn_w + gap;
    }
    return cy + btn_h + 6.0f;
}

/* 与 draw_type_btn_wrap 同布局做命中；命中返回按钮下标，否则 -1。
 * *io_y 为按钮区起始 y，返回后推进到下一内容行 y。 */
static int hit_type_btn_wrap(float px, float py,
                             float x, float* io_y, float area_w,
                             float btn_w, float btn_h, float gap, int n)
{
    float cx = x;
    float cy = *io_y;
    float end_y = cy;
    int i;
    int hit = -1;

    for (i = 0; i < n; i++) {
        if (i > 0 && (cx + btn_w) > (x + area_w + 0.5f)) {
            cx = x;
            cy += btn_h + gap;
        }
        if (point_in_rect(px, py, cx, cy, btn_w, btn_h)) {
            hit = i;
        }
        end_y = cy + btn_h;
        cx += btn_w + gap;
    }
    *io_y = end_y + 6.0f;
    return hit;
}

static const char* rx_type_name(rx_impl_type_e t)
{
    switch (t) {
        case RX_IMPL_TYPE_NONE: return "NONE";
        case RX_IMPL_TYPE_SYSTEM_OUTPUT_LOOPBACK: return "SYS SPK";
        case RX_IMPL_TYPE_SYSTEM_MIC: return "SYS MIC";
        case RX_IMPL_TYPE_FILE: return "FILE";
        case RX_IMPL_TYPE_DEMO: return "DEMO";
        case RX_IMPL_TYPE_SINE_WAVE: return "SINE";
        default: return "?";
    }
}

static const char* tx_type_name(tx_impl_type_e t)
{
    switch (t) {
        case TX_IMPL_TYPE_NONE: return "NONE";
        case TX_IMPL_TYPE_FILE: return "FILE";
        case TX_IMPL_TYPE_SYSTEM_OUTPUT: return "SYS";
        default: return "?";
    }
}

/* 切到某 type 时写入内置默认；同 type 再点只保证 path 指针绑定。 */
static void ensure_rx_type_defaults(bsp_audio_system_rx_map_t* m, rx_impl_type_e t, char* path_buf)
{
    int changed = (m->rx_impl_type != t);
    m->rx_impl_type = t;
    if (!changed) {
        if (t == RX_IMPL_TYPE_FILE) {
            m->config.file.file_path = path_buf;
        }
        return;
    }

    memset(&m->config, 0, sizeof(m->config));
    if (t == RX_IMPL_TYPE_SYSTEM_OUTPUT_LOOPBACK) {
        m->config.system_output_loopback.gain = AUDIO_DEFAULT_GAIN;
    } else if (t == RX_IMPL_TYPE_SYSTEM_MIC) {
        m->config.system_mic.gain = AUDIO_DEFAULT_GAIN;
    } else if (t == RX_IMPL_TYPE_FILE) {
        SDL_strlcpy(path_buf, AUDIO_DEFAULT_RX_WAV_PATH, AUDIO_PATH_MAX);
        m->config.file.file_path = path_buf;
        m->config.file.gain = AUDIO_DEFAULT_GAIN;
        m->config.file.loop = 1;
        m->config.file.loop_cnt = 0;
    } else if (t == RX_IMPL_TYPE_DEMO) {
        m->config.demo.gain = AUDIO_DEFAULT_GAIN;
        m->config.demo.loop = 1;
        m->config.demo.loop_cnt = 0;
    } else if (t == RX_IMPL_TYPE_SINE_WAVE) {
        m->config.sine_wave.frequency = AUDIO_DEFAULT_SINE_FREQ_HZ;
        m->config.sine_wave.amplitude = AUDIO_DEFAULT_SINE_AMP;
    }
}

static void ensure_tx_type_defaults(bsp_audio_system_tx_map_t* m, tx_impl_type_e t, char* path_buf)
{
    int changed = (m->tx_impl_type != t);
    m->tx_impl_type = t;
    if (!changed) {
        if (t == TX_IMPL_TYPE_FILE) {
            m->config.file.file_path = path_buf;
        }
        return;
    }

    memset(&m->config, 0, sizeof(m->config));
    if (t == TX_IMPL_TYPE_FILE) {
        SDL_strlcpy(path_buf, AUDIO_DEFAULT_TX_WAV_PATH, AUDIO_PATH_MAX);
        m->config.file.file_path = path_buf;
        m->config.file.gain = AUDIO_DEFAULT_GAIN;
    } else if (t == TX_IMPL_TYPE_SYSTEM_OUTPUT) {
        m->config.system_output.gain = AUDIO_DEFAULT_GAIN;
    }
}

static void render_config_page(struct SDL_Renderer* renderer)
{
    float x, y, w, h;
    const float list_w = AUDIO_CFG_LIST_W;
    const float row_h = AUDIO_CFG_ROW_H;
    const float btn_h = AUDIO_CFG_TYPE_BTN_H;
    const float btn_w = AUDIO_CFG_TYPE_BTN_W;
    const float gap = AUDIO_CFG_TYPE_BTN_GAP;
    float right_x;
    float cy;
    char line[AUDIO_PATH_MAX + 8];
    int i;
    int total = (int)s_tx_cnt + (int)s_rx_cnt;
    int sel = s_ui_sel;
    int is_tx_sel;
    int ch_idx;

    audio_panel_rect(&x, &y, &w, &h);
    {
        SDL_FRect bg = { x, y, w, h };
        SDL_SetRenderDrawColor(renderer, 28, 32, 38, 255);
        SDL_RenderFillRect(renderer, &bg);
        SDL_SetRenderDrawColor(renderer, 90, 100, 115, 255);
        SDL_RenderRect(renderer, &bg);
    }

    SDL_SetRenderDrawColor(renderer, 220, 230, 240, 255);
    SDL_RenderDebugText(renderer, x + 6.0f, y + 4.0f, "AUDIO CFG");
    draw_btn(renderer, x + w - AUDIO_CFG_CLOSE_X, y + 2.0f,
             AUDIO_CFG_CLOSE_W, AUDIO_CFG_CLOSE_H, "X", 0);

    right_x = x + list_w + 8.0f;
    cy = y + AUDIO_CFG_HEADER_H;

    for (i = 0; i < total; i++) {
        char lab[AUDIO_NAME_MAX + 4];
        const char* nm;
        int active = (i == sel);
        if (i < (int)s_tx_cnt) {
            nm = s_tx_cfg[i].name;
        } else {
            nm = s_rx_cfg[i - (int)s_tx_cnt].name;
        }
        snprintf(lab, sizeof(lab), "%s", nm ? nm : "?");
        draw_btn(renderer, x + 4.0f, cy, list_w - 8.0f, row_h, lab, active);
        cy += row_h + AUDIO_CFG_ROW_GAP;
    }

    is_tx_sel = (sel < (int)s_tx_cnt);
    ch_idx = is_tx_sel ? sel : (sel - (int)s_tx_cnt);
    cy = y + AUDIO_CFG_HEADER_H;

    if (is_tx_sel && ch_idx >= 0 && ch_idx < (int)s_tx_cnt) {
        bsp_audio_system_tx_map_t* d = &s_tx_draft[ch_idx];
        const char* tx_labs[3] = { "NONE", "FILE", "SYS" };
        int tx_act[3] = {
            d->tx_impl_type == TX_IMPL_TYPE_NONE,
            d->tx_impl_type == TX_IMPL_TYPE_FILE,
            d->tx_impl_type == TX_IMPL_TYPE_SYSTEM_OUTPUT
        };
        float type_area_w = w - (right_x - x) - 8.0f;

        SDL_SetRenderDrawColor(renderer, 200, 210, 220, 255);
        snprintf(line, sizeof(line), "TX '%s' type=%s",
                 d->name ? d->name : "?", tx_type_name(d->tx_impl_type));
        SDL_RenderDebugText(renderer, right_x, cy, line);
        cy += AUDIO_CFG_TITLE_H;
        cy = draw_type_btn_wrap(renderer, right_x, cy, type_area_w, btn_w, btn_h, gap,
                                tx_labs, tx_act, 3);

        if (d->tx_impl_type == TX_IMPL_TYPE_FILE) {
            SDL_FRect path_box = { right_x, cy, w - (right_x - x) - 8.0f, AUDIO_PATH_FIELD_H };
            if (path_box.w < 80.0f) {
                path_box.w = 80.0f;
            }
            if (s_path_editing) {
                SDL_SetRenderDrawColor(renderer, 55, 70, 90, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 40, 46, 54, 255);
            }
            SDL_RenderFillRect(renderer, &path_box);
            SDL_SetRenderDrawColor(renderer, 110, 120, 135, 255);
            SDL_RenderRect(renderer, &path_box);
            {
                const char* p = s_tx_path_draft[ch_idx];
                size_t plen = strlen(p);
                size_t max_chars = (size_t)((path_box.w - 6.0f) /
                                            (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
                int blink = (((uint32_t)SDL_GetTicks() - s_caret_blink_ms) / 400u) & 1u;
                if (max_chars < 4u) {
                    max_chars = 4u;
                }
                if (max_chars >= sizeof(line)) {
                    max_chars = sizeof(line) - 1u;
                }
                if (s_path_editing && blink) {
                    if (plen + 1u > max_chars) {
                        snprintf(line, sizeof(line), "...%s|",
                                 p + (plen - (max_chars - 4u)));
                    } else {
                        snprintf(line, sizeof(line), "%s|", p);
                    }
                } else if (plen > max_chars) {
                    snprintf(line, sizeof(line), "...%s",
                             p + (plen - (max_chars - 3u)));
                } else {
                    snprintf(line, sizeof(line), "%s", p);
                }
                SDL_SetRenderDrawColor(renderer, 220, 230, 240, 255);
                SDL_RenderDebugText(renderer, path_box.x + 3.0f,
                                    path_box.y + (path_box.h - (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) * 0.5f,
                                    line);
            }
            cy += AUDIO_PATH_FIELD_H + 4.0f;
            snprintf(line, sizeof(line), "gain:%.2f", (double)d->config.file.gain);
            SDL_SetRenderDrawColor(renderer, 180, 190, 200, 255);
            SDL_RenderDebugText(renderer, right_x, cy, line);
            draw_btn(renderer, right_x + AUDIO_CFG_GAIN_BTN_X0, cy - 2.0f,
                     AUDIO_CFG_GAIN_BTN_W, btn_h, "-", 0);
            draw_btn(renderer, right_x + AUDIO_CFG_GAIN_BTN_X1, cy - 2.0f,
                     AUDIO_CFG_GAIN_BTN_W, btn_h, "+", 0);
        } else if (d->tx_impl_type == TX_IMPL_TYPE_SYSTEM_OUTPUT) {
            snprintf(line, sizeof(line), "gain:%.2f", (double)d->config.system_output.gain);
            SDL_SetRenderDrawColor(renderer, 180, 190, 200, 255);
            SDL_RenderDebugText(renderer, right_x, cy, line);
            draw_btn(renderer, right_x + AUDIO_CFG_GAIN_BTN_X0, cy - 2.0f,
                     AUDIO_CFG_GAIN_BTN_W, btn_h, "-", 0);
            draw_btn(renderer, right_x + AUDIO_CFG_GAIN_BTN_X1, cy - 2.0f,
                     AUDIO_CFG_GAIN_BTN_W, btn_h, "+", 0);
        }
    } else if (!is_tx_sel && ch_idx >= 0 && ch_idx < (int)s_rx_cnt) {
        bsp_audio_system_rx_map_t* d = &s_rx_draft[ch_idx];
        const char* rx_labs[6] = { "NONE", "SPK", "MIC", "FILE", "DEMO", "SINE" };
        int rx_act[6] = {
            d->rx_impl_type == RX_IMPL_TYPE_NONE,
            d->rx_impl_type == RX_IMPL_TYPE_SYSTEM_OUTPUT_LOOPBACK,
            d->rx_impl_type == RX_IMPL_TYPE_SYSTEM_MIC,
            d->rx_impl_type == RX_IMPL_TYPE_FILE,
            d->rx_impl_type == RX_IMPL_TYPE_DEMO,
            d->rx_impl_type == RX_IMPL_TYPE_SINE_WAVE
        };
        float type_area_w = w - (right_x - x) - 8.0f;

        SDL_SetRenderDrawColor(renderer, 200, 210, 220, 255);
        snprintf(line, sizeof(line), "RX '%s' type=%s",
                 d->name ? d->name : "?", rx_type_name(d->rx_impl_type));
        SDL_RenderDebugText(renderer, right_x, cy, line);
        cy += AUDIO_CFG_TITLE_H;
        cy = draw_type_btn_wrap(renderer, right_x, cy, type_area_w, btn_w, btn_h, gap,
                                rx_labs, rx_act, 6);

        if (d->rx_impl_type == RX_IMPL_TYPE_SYSTEM_OUTPUT_LOOPBACK ||
            d->rx_impl_type == RX_IMPL_TYPE_SYSTEM_MIC) {
            float g = (d->rx_impl_type == RX_IMPL_TYPE_SYSTEM_MIC)
                          ? d->config.system_mic.gain
                          : d->config.system_output_loopback.gain;
            snprintf(line, sizeof(line), "gain:%.2f", (double)g);
            SDL_SetRenderDrawColor(renderer, 180, 190, 200, 255);
            SDL_RenderDebugText(renderer, right_x, cy, line);
            draw_btn(renderer, right_x + AUDIO_CFG_GAIN_BTN_X0, cy - 2.0f,
                     AUDIO_CFG_GAIN_BTN_W, btn_h, "-", 0);
            draw_btn(renderer, right_x + AUDIO_CFG_GAIN_BTN_X1, cy - 2.0f,
                     AUDIO_CFG_GAIN_BTN_W, btn_h, "+", 0);
        } else if (d->rx_impl_type == RX_IMPL_TYPE_FILE) {
            SDL_FRect path_box = { right_x, cy, w - (right_x - x) - 8.0f, AUDIO_PATH_FIELD_H };
            if (path_box.w < 80.0f) {
                path_box.w = 80.0f;
            }
            if (s_path_editing) {
                SDL_SetRenderDrawColor(renderer, 55, 70, 90, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 40, 46, 54, 255);
            }
            SDL_RenderFillRect(renderer, &path_box);
            SDL_SetRenderDrawColor(renderer, 110, 120, 135, 255);
            SDL_RenderRect(renderer, &path_box);
            {
                const char* p = s_rx_path_draft[ch_idx];
                size_t plen = strlen(p);
                size_t max_chars = (size_t)((path_box.w - 6.0f) /
                                            (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE);
                int blink = (((uint32_t)SDL_GetTicks() - s_caret_blink_ms) / 400u) & 1u;
                if (max_chars < 4u) {
                    max_chars = 4u;
                }
                if (max_chars >= sizeof(line)) {
                    max_chars = sizeof(line) - 1u;
                }
                if (s_path_editing && blink) {
                    if (plen + 1u > max_chars) {
                        snprintf(line, sizeof(line), "...%s|",
                                 p + (plen - (max_chars - 4u)));
                    } else {
                        snprintf(line, sizeof(line), "%s|", p);
                    }
                } else if (plen > max_chars) {
                    snprintf(line, sizeof(line), "...%s",
                             p + (plen - (max_chars - 3u)));
                } else {
                    snprintf(line, sizeof(line), "%s", p);
                }
                SDL_SetRenderDrawColor(renderer, 220, 230, 240, 255);
                SDL_RenderDebugText(renderer, path_box.x + 3.0f,
                                    path_box.y + (path_box.h - (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) * 0.5f,
                                    line);
            }
            cy += AUDIO_PATH_FIELD_H + 4.0f;
            snprintf(line, sizeof(line), "gain:%.2f loop:%u",
                     (double)d->config.file.gain, (unsigned)d->config.file.loop);
            SDL_SetRenderDrawColor(renderer, 180, 190, 200, 255);
            SDL_RenderDebugText(renderer, right_x, cy, line);
            draw_btn(renderer, right_x + AUDIO_CFG_GAIN_BTN_X0, cy - 2.0f,
                     AUDIO_CFG_GAIN_BTN_W, btn_h, "-", 0);
            draw_btn(renderer, right_x + AUDIO_CFG_GAIN_BTN_X1, cy - 2.0f,
                     AUDIO_CFG_GAIN_BTN_W, btn_h, "+", 0);
            draw_btn(renderer, right_x + AUDIO_CFG_LOOP_BTN_X, cy - 2.0f,
                     AUDIO_CFG_LOOP_BTN_W, btn_h, "LOOP",
                     (int)d->config.file.loop);
        } else if (d->rx_impl_type == RX_IMPL_TYPE_DEMO) {
            snprintf(line, sizeof(line), "gain:%.2f loop:%u",
                     (double)d->config.demo.gain, (unsigned)d->config.demo.loop);
            SDL_SetRenderDrawColor(renderer, 180, 190, 200, 255);
            SDL_RenderDebugText(renderer, right_x, cy, line);
            draw_btn(renderer, right_x + AUDIO_CFG_GAIN_BTN_X0, cy - 2.0f,
                     AUDIO_CFG_GAIN_BTN_W, btn_h, "-", 0);
            draw_btn(renderer, right_x + AUDIO_CFG_GAIN_BTN_X1, cy - 2.0f,
                     AUDIO_CFG_GAIN_BTN_W, btn_h, "+", 0);
            draw_btn(renderer, right_x + AUDIO_CFG_LOOP_BTN_X, cy - 2.0f,
                     AUDIO_CFG_LOOP_BTN_W, btn_h, "LOOP",
                     (int)d->config.demo.loop);
        } else if (d->rx_impl_type == RX_IMPL_TYPE_SINE_WAVE) {
            snprintf(line, sizeof(line), "freq:%.0f amp:%.2f",
                     (double)d->config.sine_wave.frequency,
                     (double)d->config.sine_wave.amplitude);
            SDL_SetRenderDrawColor(renderer, 180, 190, 200, 255);
            SDL_RenderDebugText(renderer, right_x, cy, line);
            cy += AUDIO_CFG_TITLE_H;
            draw_btn(renderer, right_x, cy, 28.0f, btn_h, "F-", 0);
            draw_btn(renderer, right_x + 32.0f, cy, 28.0f, btn_h, "F+", 0);
            draw_btn(renderer, right_x + 68.0f, cy, 28.0f, btn_h, "A-", 0);
            draw_btn(renderer, right_x + 100.0f, cy, 28.0f, btn_h, "A+", 0);
        }
    }

    draw_btn(renderer, x + w - AUDIO_CFG_APPLY_OFF, y + h - AUDIO_CFG_FOOT_H,
             AUDIO_CFG_FOOT_BTN_W, AUDIO_CFG_FOOT_BTN_H, "APPLY", 1);
    draw_btn(renderer, x + w - AUDIO_CFG_CLOSE_OFF, y + h - AUDIO_CFG_FOOT_H,
             AUDIO_CFG_FOOT_BTN_W, AUDIO_CFG_FOOT_BTN_H, "CLOSE", 0);
}

static void render_energy_page(struct SDL_Renderer* renderer)
{
    float panel_x, panel_y, panel_w, panel_h;
    float mx, my, mw, mh;
    int total = audio_channel_total();
    int idx = 0;
    uint8_t i;
    SDL_FRect hdr;

    if (total <= 0) {
        return;
    }

    audio_panel_rect(&panel_x, &panel_y, &panel_w, &panel_h);
    hdr.x = panel_x;
    hdr.y = panel_y;
    hdr.w = panel_w;
    hdr.h = 20.0f;
    SDL_SetRenderDrawColor(renderer, 32, 36, 44, 255);
    SDL_RenderFillRect(renderer, &hdr);
    SDL_SetRenderDrawColor(renderer, 200, 210, 220, 255);
    SDL_RenderDebugText(renderer, panel_x + 4.0f, panel_y + 4.0f, "AUDIO");
    SDL_SetRenderDrawColor(renderer, 130, 140, 155, 255);
    SDL_RenderDebugText(renderer, panel_x + panel_w - (float)debug_text_width("click:cfg") - 4.0f,
                        panel_y + 4.0f, "click:cfg");

    for (i = 0; i < s_tx_cnt; i++, idx++) {
        float lv = (float)SDL_GetAtomicInt(&s_energy_tx_q15[i]) / 32767.0f;
        audio_meter_rect(idx, &mx, &my, &mw, &mh);
        draw_channel_meter(renderer, mx, my, mw, mh,
                           s_tx[i].map->name,
                           tx_type_name(s_tx[i].map->tx_impl_type),
                           lv, &s_peak_tx[i], s_pkhz_tx[i], s_spec_tx[i], 1);
    }
    for (i = 0; i < s_rx_cnt; i++, idx++) {
        float lv = (float)SDL_GetAtomicInt(&s_energy_rx_q15[i]) / 32767.0f;
        audio_meter_rect(idx, &mx, &my, &mw, &mh);
        draw_channel_meter(renderer, mx, my, mw, mh,
                           s_rx[i].map->name,
                           rx_type_name(s_rx[i].map->rx_impl_type),
                           lv, &s_peak_rx[i], s_pkhz_rx[i], s_spec_rx[i], 0);
    }
}

static void audio_render_cb(struct SDL_Renderer* renderer, void* ctx)
{
    audio_ui_mode_e mode;
    (void)ctx;
    if (!renderer || !s_initialized) {
        return;
    }
    if (s_ui_lock) {
        SDL_LockMutex(s_ui_lock);
    }
    mode = s_ui_mode;
    if (s_ui_lock) {
        SDL_UnlockMutex(s_ui_lock);
    }

    if (mode == AUDIO_UI_CONFIG) {
        render_config_page(renderer);
    } else {
        render_energy_page(renderer);
    }
}

/* -------------------------------------------------------------------------- */
/* RX / TX open-close / reopen                                                */
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

static int rx_open_recording_device(rx_runtime_t* rx, SDL_AudioDeviceID id, const char* kind)
{
    SDL_AudioSpec spec;

    if (!id) {
        return -1;
    }

    memset(&spec, 0, sizeof(spec));
    spec.format = SDL_AUDIO_F32;
    spec.channels = (int)BSP_AUDIO_CHANNELS;
    spec.freq = (int)BSP_AUDIO_SAMPLE_RATE_HZ;

    rx->stream = SDL_OpenAudioDeviceStream(id, &spec, NULL, NULL);
    if (!rx->stream) {
        ERROR_LOG("open RX %s '%s' failed: %s",
                  kind, rx->map->name ? rx->map->name : "?", SDL_GetError());
        return -1;
    }
    if (!SDL_ResumeAudioStreamDevice(rx->stream)) {
        ERROR_LOG("resume RX %s '%s' failed: %s",
                  kind, rx->map->name ? rx->map->name : "?", SDL_GetError());
        SDL_DestroyAudioStream(rx->stream);
        rx->stream = NULL;
        return -1;
    }
    INFO_LOG("RX '%s' %s opened", rx->map->name ? rx->map->name : "?", kind);
    return 0;
}

static int rx_open_loopback(rx_runtime_t* rx)
{
    return rx_open_recording_device(rx, find_system_output_loopback(), "SYS SPK");
}

static int rx_open_mic(rx_runtime_t* rx)
{
    return rx_open_recording_device(rx, SDL_AUDIO_DEVICE_DEFAULT_RECORDING, "SYS MIC");
}

static int rx_finish_wav_open(rx_runtime_t* rx, const char* src_label)
{
    SDL_AudioSpec dst;

    memset(&dst, 0, sizeof(dst));
    dst.format = SDL_AUDIO_F32;
    dst.channels = (int)BSP_AUDIO_CHANNELS;
    dst.freq = (int)BSP_AUDIO_SAMPLE_RATE_HZ;

    rx->stream = SDL_CreateAudioStream(&rx->file_spec, &dst);
    if (!rx->stream) {
        ERROR_LOG("RX '%s' create stream failed (%s): %s",
                  rx->map->name ? rx->map->name : "?", src_label, SDL_GetError());
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

    INFO_LOG("RX '%s': %s %d Hz ch=%d",
             rx->map->name ? rx->map->name : "?",
             src_label, rx->file_spec.freq, rx->file_spec.channels);
    return 0;
}

static int rx_open_file(rx_runtime_t* rx)
{
    const char* path = rx->map->config.file.file_path;

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

    return rx_finish_wav_open(rx, path);
}

static int rx_open_demo(rx_runtime_t* rx)
{
    SDL_IOStream* io;

    if (g_embedded_demo_wav_size == 0u) {
        ERROR_LOG("RX demo '%s': embedded demo.wav empty",
                  rx->map->name ? rx->map->name : "?");
        return -1;
    }

    io = SDL_IOFromConstMem(g_embedded_demo_wav, (size_t)g_embedded_demo_wav_size);
    if (!io) {
        ERROR_LOG("RX demo '%s': IOFromConstMem failed: %s",
                  rx->map->name ? rx->map->name : "?", SDL_GetError());
        return -1;
    }
    if (!SDL_LoadWAV_IO(io, true, &rx->file_spec, &rx->file_raw, &rx->file_raw_len) ||
        !rx->file_raw) {
        ERROR_LOG("RX demo '%s' load failed: %s",
                  rx->map->name ? rx->map->name : "?", SDL_GetError());
        rx->file_raw = NULL;
        rx->file_raw_len = 0;
        return -1;
    }

    return rx_finish_wav_open(rx, "demo.wav (embedded)");
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

static int rx_open_by_cfg(rx_runtime_t* rx)
{
    if (rx->map->rx_impl_type == RX_IMPL_TYPE_NONE) {
        INFO_LOG("RX '%s' NONE", rx->map->name ? rx->map->name : "?");
        return 0;
    }
    if (rx->map->rx_impl_type == RX_IMPL_TYPE_SYSTEM_OUTPUT_LOOPBACK) {
        return rx_open_loopback(rx);
    }
    if (rx->map->rx_impl_type == RX_IMPL_TYPE_SYSTEM_MIC) {
        return rx_open_mic(rx);
    }
    if (rx->map->rx_impl_type == RX_IMPL_TYPE_FILE) {
        return rx_open_file(rx);
    }
    if (rx->map->rx_impl_type == RX_IMPL_TYPE_DEMO) {
        return rx_open_demo(rx);
    }
    if (rx->map->rx_impl_type == RX_IMPL_TYPE_SINE_WAVE) {
        rx->sine_phase = 0.0f;
        INFO_LOG("RX '%s' sine %.1f Hz amp=%.3f",
                 rx->map->name ? rx->map->name : "?",
                 (double)rx->map->config.sine_wave.frequency,
                 (double)rx->map->config.sine_wave.amplitude);
        return 0;
    }
    ERROR_LOG("RX unknown type=%u", (unsigned)rx->map->rx_impl_type);
    return -1;
}

static int rx_reopen(uint8_t idx)
{
    if (idx >= s_rx_cnt) {
        return -1;
    }
    rx_close(&s_rx[idx]);
    return rx_open_by_cfg(&s_rx[idx]);
}

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

static int tx_open_by_cfg(tx_runtime_t* tx)
{
    if (tx->map->tx_impl_type == TX_IMPL_TYPE_NONE) {
        INFO_LOG("TX '%s' NONE", tx->map->name ? tx->map->name : "?");
        return 0;
    }
    if (tx->map->tx_impl_type == TX_IMPL_TYPE_FILE) {
        const char* path = tx->map->config.file.file_path;
        if (!path || !path[0]) {
            ERROR_LOG("TX '%s': empty file_path", tx->map->name ? tx->map->name : "?");
            return -1;
        }
        if (wav_open(&tx->wav, path, BSP_AUDIO_SAMPLE_RATE_HZ, (uint16_t)BSP_AUDIO_CHANNELS) != 0) {
            return -1;
        }
        INFO_LOG("TX '%s' file -> '%s'", tx->map->name ? tx->map->name : "?", path);
        return 0;
    }
    if (tx->map->tx_impl_type == TX_IMPL_TYPE_SYSTEM_OUTPUT) {
        if (tx_open_system_output(tx) != 0) {
            return -1;
        }
        INFO_LOG("TX '%s' system output gain=%.3f",
                 tx->map->name ? tx->map->name : "?",
                 (double)tx->map->config.system_output.gain);
        return 0;
    }
    ERROR_LOG("TX unknown type=%u", (unsigned)tx->map->tx_impl_type);
    return -1;
}

static int tx_reopen(uint8_t idx)
{
    if (idx >= s_tx_cnt) {
        return -1;
    }
    tx_close(&s_tx[idx]);
    return tx_open_by_cfg(&s_tx[idx]);
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
    if (rx->map->rx_impl_type == RX_IMPL_TYPE_SYSTEM_MIC) {
        return rx->map->config.system_mic.gain;
    }
    if (rx->map->rx_impl_type == RX_IMPL_TYPE_FILE) {
        return rx->map->config.file.gain;
    }
    if (rx->map->rx_impl_type == RX_IMPL_TYPE_DEMO) {
        return rx->map->config.demo.gain;
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
        while (phase >= AUDIO_TWO_PI_F) {
            phase -= AUDIO_TWO_PI_F;
        }
    }
    rx->sine_phase = phase;
    return 1;
}

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
            if ((rx->map->rx_impl_type == RX_IMPL_TYPE_FILE ||
                 rx->map->rx_impl_type == RX_IMPL_TYPE_DEMO) && !rx->file_eof) {
                const uint8_t do_loop = (rx->map->rx_impl_type == RX_IMPL_TYPE_DEMO)
                                            ? (uint8_t)rx->map->config.demo.loop
                                            : (uint8_t)rx->map->config.file.loop;
                const uint32_t loop_cnt = (rx->map->rx_impl_type == RX_IMPL_TYPE_DEMO)
                                              ? rx->map->config.demo.loop_cnt
                                              : rx->map->config.file.loop_cnt;
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
    if (rx->map->rx_impl_type == RX_IMPL_TYPE_NONE) {
        memset(dst, 0, AUDIO_FRAME_BYTES);
        return 0;
    }
    if (rx->map->rx_impl_type == RX_IMPL_TYPE_SINE_WAVE) {
        return rx_pull_sine_frame(rx, dst);
    }
    return rx_pull_stream_frame(rx, dst);
}

/* 系统播放队列是否还能再收一帧；满则追帧应停表，禁止丢帧抢时钟。 */
static int tx_sys_can_accept_frame(void)
{
    uint8_t i;
    const int limit = (int)((AUDIO_TX_PLAY_MAX_QUEUED_FRAMES - AUDIO_TX_PLAY_CATCHUP_HEADROOM) *
                            AUDIO_FRAME_BYTES);

    for (i = 0; i < s_tx_cnt; i++) {
        if (s_tx[i].map->tx_impl_type != TX_IMPL_TYPE_SYSTEM_OUTPUT || !s_tx[i].play) {
            continue;
        }
        if (SDL_GetAudioStreamQueued(s_tx[i].play) > limit) {
            return 0;
        }
    }
    return 1;
}

static void tx_push_frame(tx_runtime_t* tx, const float* interleaved)
{
    if (tx->map->tx_impl_type == TX_IMPL_TYPE_NONE) {
        return;
    }

    if (tx->map->tx_impl_type == TX_IMPL_TYPE_FILE) {
        (void)wav_write_float_frame(&tx->wav, interleaved, AUDIO_FRAME_FLOATS,
                                    tx->map->config.file.gain);
        return;
    }

    if (tx->map->tx_impl_type == TX_IMPL_TYPE_SYSTEM_OUTPUT && tx->play) {
        float gain = tx->map->config.system_output.gain;
        float scaled[AUDIO_FRAME_FLOATS];
        const float* src = interleaved;

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
/* 配置 APPLY / UI 点击                                                       */
/* -------------------------------------------------------------------------- */

static void ui_enter_config(void)
{
    uint8_t i;
    memcpy(s_rx_draft, s_rx_cfg, sizeof(bsp_audio_system_rx_map_t) * s_rx_cnt);
    memcpy(s_tx_draft, s_tx_cfg, sizeof(bsp_audio_system_tx_map_t) * s_tx_cnt);
    for (i = 0; i < s_rx_cnt; i++) {
        SDL_strlcpy(s_rx_path_draft[i], s_rx_path[i], AUDIO_PATH_MAX);
    }
    for (i = 0; i < s_tx_cnt; i++) {
        SDL_strlcpy(s_tx_path_draft[i], s_tx_path[i], AUDIO_PATH_MAX);
    }
    bind_draft_path_ptrs();
    s_ui_sel = 0;
    s_path_editing = 0;
    s_ui_mode = AUDIO_UI_CONFIG;
    arch_display_request_frame();
}

static int rx_need_reopen(uint8_t i)
{
    if (s_rx_cfg[i].rx_impl_type != s_rx_draft[i].rx_impl_type) {
        return 1;
    }
    if (s_rx_draft[i].rx_impl_type == RX_IMPL_TYPE_FILE &&
        strcmp(s_rx_path[i], s_rx_path_draft[i]) != 0) {
        return 1;
    }
    return 0;
}

static int tx_need_reopen(uint8_t i)
{
    if (s_tx_cfg[i].tx_impl_type != s_tx_draft[i].tx_impl_type) {
        return 1;
    }
    if (s_tx_draft[i].tx_impl_type == TX_IMPL_TYPE_FILE &&
        strcmp(s_tx_path[i], s_tx_path_draft[i]) != 0) {
        return 1;
    }
    return 0;
}

static void ui_close_config(int apply)
{
    uint8_t i;

    path_edit_stop();

    if (apply) {
        SDL_LockMutex(s_lock);
        for (i = 0; i < s_rx_cnt; i++) {
            int reopen = rx_need_reopen(i);
            bsp_audio_system_rx_map_t backup = s_rx_cfg[i];
            char path_backup[AUDIO_PATH_MAX];
            SDL_strlcpy(path_backup, s_rx_path[i], sizeof(path_backup));

            s_rx_cfg[i] = s_rx_draft[i];
            SDL_strlcpy(s_rx_path[i], s_rx_path_draft[i], AUDIO_PATH_MAX);
            if (s_rx_cfg[i].rx_impl_type == RX_IMPL_TYPE_FILE) {
                s_rx_cfg[i].config.file.file_path = s_rx_path[i];
            }

            if (reopen) {
                if (rx_reopen(i) != 0) {
                    WARN_LOG("RX[%u] reopen failed, restore", (unsigned)i);
                    s_rx_cfg[i] = backup;
                    SDL_strlcpy(s_rx_path[i], path_backup, AUDIO_PATH_MAX);
                    if (s_rx_cfg[i].rx_impl_type == RX_IMPL_TYPE_FILE) {
                        s_rx_cfg[i].config.file.file_path = s_rx_path[i];
                    }
                    s_rx_draft[i] = s_rx_cfg[i];
                    SDL_strlcpy(s_rx_path_draft[i], s_rx_path[i], AUDIO_PATH_MAX);
                    (void)rx_reopen(i);
                }
            }
        }
        for (i = 0; i < s_tx_cnt; i++) {
            int reopen = tx_need_reopen(i);
            bsp_audio_system_tx_map_t backup = s_tx_cfg[i];
            char path_backup[AUDIO_PATH_MAX];
            SDL_strlcpy(path_backup, s_tx_path[i], sizeof(path_backup));

            s_tx_cfg[i] = s_tx_draft[i];
            SDL_strlcpy(s_tx_path[i], s_tx_path_draft[i], AUDIO_PATH_MAX);
            if (s_tx_cfg[i].tx_impl_type == TX_IMPL_TYPE_FILE) {
                s_tx_cfg[i].config.file.file_path = s_tx_path[i];
            }

            if (reopen) {
                if (tx_reopen(i) != 0) {
                    WARN_LOG("TX[%u] reopen failed, restore", (unsigned)i);
                    s_tx_cfg[i] = backup;
                    SDL_strlcpy(s_tx_path[i], path_backup, AUDIO_PATH_MAX);
                    if (s_tx_cfg[i].tx_impl_type == TX_IMPL_TYPE_FILE) {
                        s_tx_cfg[i].config.file.file_path = s_tx_path[i];
                    }
                    s_tx_draft[i] = s_tx_cfg[i];
                    SDL_strlcpy(s_tx_path_draft[i], s_tx_path[i], AUDIO_PATH_MAX);
                    (void)tx_reopen(i);
                }
            }
        }
        bind_live_path_ptrs();
        bind_draft_path_ptrs();
        SDL_UnlockMutex(s_lock);
        INFO_LOG("config applied");
    }

    s_ui_mode = AUDIO_UI_ENERGY;
    arch_display_request_frame();
}

static float* sel_gain_ptr(void)
{
    int is_tx = (s_ui_sel < (int)s_tx_cnt);
    int idx = is_tx ? s_ui_sel : (s_ui_sel - (int)s_tx_cnt);
    if (is_tx && idx >= 0 && idx < (int)s_tx_cnt) {
        if (s_tx_draft[idx].tx_impl_type == TX_IMPL_TYPE_FILE) {
            return &s_tx_draft[idx].config.file.gain;
        }
        return &s_tx_draft[idx].config.system_output.gain;
    }
    if (!is_tx && idx >= 0 && idx < (int)s_rx_cnt) {
        if (s_rx_draft[idx].rx_impl_type == RX_IMPL_TYPE_SYSTEM_OUTPUT_LOOPBACK) {
            return &s_rx_draft[idx].config.system_output_loopback.gain;
        }
        if (s_rx_draft[idx].rx_impl_type == RX_IMPL_TYPE_SYSTEM_MIC) {
            return &s_rx_draft[idx].config.system_mic.gain;
        }
        if (s_rx_draft[idx].rx_impl_type == RX_IMPL_TYPE_FILE) {
            return &s_rx_draft[idx].config.file.gain;
        }
        if (s_rx_draft[idx].rx_impl_type == RX_IMPL_TYPE_DEMO) {
            return &s_rx_draft[idx].config.demo.gain;
        }
    }
    return NULL;
}

static void nudge_gain(float delta)
{
    float* g = sel_gain_ptr();
    if (!g) {
        return;
    }
    *g += delta;
    if (*g < 0.05f) {
        *g = 0.05f;
    }
    if (*g > 4.0f) {
        *g = 4.0f;
    }
}

static void ui_handle_click(float px, float py)
{
    float x, y, w, h;
    const float list_w = AUDIO_CFG_LIST_W;
    const float row_h = AUDIO_CFG_ROW_H;
    const float btn_h = AUDIO_CFG_TYPE_BTN_H;
    const float btn_w = AUDIO_CFG_TYPE_BTN_W;
    const float gap = AUDIO_CFG_TYPE_BTN_GAP;
    float right_x;
    float cy;
    float path_w;
    int i;
    int total = (int)s_tx_cnt + (int)s_rx_cnt;
    int is_tx_sel;
    int ch_idx;

    audio_panel_rect(&x, &y, &w, &h);

    if (s_ui_mode == AUDIO_UI_ENERGY) {
        int energy_total = audio_channel_total();
        int ei;
        if (!point_in_rect(px, py, x, y, w, h)) {
            return;
        }
        for (ei = 0; ei < energy_total; ei++) {
            float mx, my, mw, mh;
            audio_meter_rect(ei, &mx, &my, &mw, &mh);
            if (point_in_rect(px, py, mx, my, mw, mh)) {
                s_ui_sel = ei;
                break;
            }
        }
        ui_enter_config();
        return;
    }

    /* CONFIG — 命中矩形必须与 render_config_page 完全一致 */
    if (point_in_rect(px, py, x + w - AUDIO_CFG_CLOSE_X, y + 2.0f,
                      AUDIO_CFG_CLOSE_W, AUDIO_CFG_CLOSE_H)) {
        ui_close_config(0);
        return;
    }
    if (point_in_rect(px, py, x + w - AUDIO_CFG_APPLY_OFF, y + h - AUDIO_CFG_FOOT_H,
                      AUDIO_CFG_FOOT_BTN_W, AUDIO_CFG_FOOT_BTN_H)) {
        ui_close_config(1);
        return;
    }
    if (point_in_rect(px, py, x + w - AUDIO_CFG_CLOSE_OFF, y + h - AUDIO_CFG_FOOT_H,
                      AUDIO_CFG_FOOT_BTN_W, AUDIO_CFG_FOOT_BTN_H)) {
        ui_close_config(0);
        return;
    }

    cy = y + AUDIO_CFG_HEADER_H;
    for (i = 0; i < total; i++) {
        if (point_in_rect(px, py, x + 4.0f, cy, list_w - 8.0f, row_h)) {
            if (s_ui_sel != i) {
                path_edit_stop();
                s_ui_sel = i;
            }
            arch_display_request_frame();
            return;
        }
        cy += row_h + AUDIO_CFG_ROW_GAP;
    }

    is_tx_sel = (s_ui_sel < (int)s_tx_cnt);
    ch_idx = is_tx_sel ? s_ui_sel : (s_ui_sel - (int)s_tx_cnt);
    right_x = x + list_w + 8.0f;
    cy = y + AUDIO_CFG_HEADER_H + AUDIO_CFG_TITLE_H;
    path_w = w - (right_x - x) - 8.0f;
    if (path_w < 80.0f) {
        path_w = 80.0f;
    }

    if (is_tx_sel && ch_idx >= 0 && ch_idx < (int)s_tx_cnt) {
        bsp_audio_system_tx_map_t* d = &s_tx_draft[ch_idx];
        float type_area_w = w - (right_x - x) - 8.0f;
        int hit = hit_type_btn_wrap(px, py, right_x, &cy, type_area_w, btn_w, btn_h, gap, 3);
        if (hit >= 0) {
            static const tx_impl_type_e s_tx_types[3] = {
                TX_IMPL_TYPE_NONE, TX_IMPL_TYPE_FILE, TX_IMPL_TYPE_SYSTEM_OUTPUT
            };
            path_edit_stop();
            ensure_tx_type_defaults(d, s_tx_types[hit], s_tx_path_draft[ch_idx]);
            arch_display_request_frame();
            return;
        }
        if (d->tx_impl_type == TX_IMPL_TYPE_NONE) {
            if (s_path_editing) {
                path_edit_stop();
            }
            return;
        }
        if (d->tx_impl_type == TX_IMPL_TYPE_FILE) {
            if (point_in_rect(px, py, right_x, cy, path_w, AUDIO_PATH_FIELD_H)) {
                path_edit_start();
                arch_display_request_frame();
                return;
            }
            cy += AUDIO_PATH_FIELD_H + 4.0f;
        } else if (s_path_editing) {
            path_edit_stop();
        }
        if (point_in_rect(px, py, right_x + AUDIO_CFG_GAIN_BTN_X0, cy - 2.0f,
                          AUDIO_CFG_GAIN_BTN_W, btn_h)) {
            nudge_gain(-0.1f);
            arch_display_request_frame();
            return;
        }
        if (point_in_rect(px, py, right_x + AUDIO_CFG_GAIN_BTN_X1, cy - 2.0f,
                          AUDIO_CFG_GAIN_BTN_W, btn_h)) {
            nudge_gain(0.1f);
            arch_display_request_frame();
            return;
        }
    } else if (!is_tx_sel && ch_idx >= 0 && ch_idx < (int)s_rx_cnt) {
        bsp_audio_system_rx_map_t* d = &s_rx_draft[ch_idx];
        float type_area_w = w - (right_x - x) - 8.0f;
        int hit = hit_type_btn_wrap(px, py, right_x, &cy, type_area_w, btn_w, btn_h, gap, 6);
        if (hit >= 0) {
            static const rx_impl_type_e s_rx_types[6] = {
                RX_IMPL_TYPE_NONE,
                RX_IMPL_TYPE_SYSTEM_OUTPUT_LOOPBACK,
                RX_IMPL_TYPE_SYSTEM_MIC,
                RX_IMPL_TYPE_FILE,
                RX_IMPL_TYPE_DEMO,
                RX_IMPL_TYPE_SINE_WAVE
            };
            path_edit_stop();
            ensure_rx_type_defaults(d, s_rx_types[hit], s_rx_path_draft[ch_idx]);
            arch_display_request_frame();
            return;
        }
        if (d->rx_impl_type == RX_IMPL_TYPE_NONE) {
            if (s_path_editing) {
                path_edit_stop();
            }
            return;
        }
        if (d->rx_impl_type == RX_IMPL_TYPE_SINE_WAVE) {
            if (s_path_editing) {
                path_edit_stop();
            }
            cy += AUDIO_CFG_TITLE_H;
            if (point_in_rect(px, py, right_x, cy, 28.0f, btn_h)) {
                d->config.sine_wave.frequency -= 50.0f;
                if (d->config.sine_wave.frequency < 50.0f) {
                    d->config.sine_wave.frequency = 50.0f;
                }
                arch_display_request_frame();
                return;
            }
            if (point_in_rect(px, py, right_x + 32.0f, cy, 28.0f, btn_h)) {
                d->config.sine_wave.frequency += 50.0f;
                if (d->config.sine_wave.frequency > 8000.0f) {
                    d->config.sine_wave.frequency = 8000.0f;
                }
                arch_display_request_frame();
                return;
            }
            if (point_in_rect(px, py, right_x + 68.0f, cy, 28.0f, btn_h)) {
                d->config.sine_wave.amplitude -= 0.05f;
                if (d->config.sine_wave.amplitude < 0.05f) {
                    d->config.sine_wave.amplitude = 0.05f;
                }
                arch_display_request_frame();
                return;
            }
            if (point_in_rect(px, py, right_x + 100.0f, cy, 28.0f, btn_h)) {
                d->config.sine_wave.amplitude += 0.05f;
                if (d->config.sine_wave.amplitude > 1.0f) {
                    d->config.sine_wave.amplitude = 1.0f;
                }
                arch_display_request_frame();
                return;
            }
        } else {
            if (d->rx_impl_type == RX_IMPL_TYPE_FILE) {
                if (point_in_rect(px, py, right_x, cy, path_w, AUDIO_PATH_FIELD_H)) {
                    path_edit_start();
                    arch_display_request_frame();
                    return;
                }
                cy += AUDIO_PATH_FIELD_H + 4.0f;
            } else if (s_path_editing) {
                path_edit_stop();
            }
            if (point_in_rect(px, py, right_x + AUDIO_CFG_GAIN_BTN_X0, cy - 2.0f,
                              AUDIO_CFG_GAIN_BTN_W, btn_h)) {
                nudge_gain(-0.1f);
                arch_display_request_frame();
                return;
            }
            if (point_in_rect(px, py, right_x + AUDIO_CFG_GAIN_BTN_X1, cy - 2.0f,
                              AUDIO_CFG_GAIN_BTN_W, btn_h)) {
                nudge_gain(0.1f);
                arch_display_request_frame();
                return;
            }
            if (d->rx_impl_type == RX_IMPL_TYPE_FILE &&
                point_in_rect(px, py, right_x + AUDIO_CFG_LOOP_BTN_X, cy - 2.0f,
                              AUDIO_CFG_LOOP_BTN_W, btn_h)) {
                d->config.file.loop = d->config.file.loop ? 0u : 1u;
                arch_display_request_frame();
                return;
            }
            if (d->rx_impl_type == RX_IMPL_TYPE_DEMO &&
                point_in_rect(px, py, right_x + AUDIO_CFG_LOOP_BTN_X, cy - 2.0f,
                              AUDIO_CFG_LOOP_BTN_W, btn_h)) {
                d->config.demo.loop = d->config.demo.loop ? 0u : 1u;
                arch_display_request_frame();
                return;
            }
        }
    }
}

static void ui_poll_clicks(void)
{
    arch_input_state_t input;
    float px;
    float py;
    uint8_t down;

    arch_input_get(&input);
    down = input.mouse_left ? 1u : 0u;
    px = (float)input.mouse_x;
    py = (float)input.mouse_y;

    if (s_mouse_was_down && !down) {
        /* 抬起沿：一次点击 */
        if (s_ui_lock) {
            SDL_LockMutex(s_ui_lock);
        }
        ui_handle_click(px, py);
        if (s_ui_lock) {
            SDL_UnlockMutex(s_ui_lock);
        }
    }
    s_mouse_was_down = down;
}

/* -------------------------------------------------------------------------- */
/* 帧处理                                                                     */
/* -------------------------------------------------------------------------- */

/* 返回 0 已处理一帧；-1 播放队列满，应停表等设备消耗（勿丢帧抢时钟）。 */
static int process_one_frame(void)
{
    bsp_audio_frame_t tx_frames[BSP_AUDIO_TX_MAX];
    bsp_audio_frame_t rx_frames[BSP_AUDIO_RX_MAX];
    bsp_audio_system_frame_info_t info;
    bsp_audio_handler_t handler;
    uint8_t i;
    uint8_t play = s_tx_play_bank;
    uint8_t work = (uint8_t)(play ^ 1u);
    uint64_t now_ns;

    if (!tx_sys_can_accept_frame()) {
        return -1;
    }

    /* 1) 先推送已缓存的上一拍 TX（不依赖本拍 CB） */
    for (i = 0; i < s_tx_cnt; i++) {
        tx_push_frame(&s_tx[i], s_tx_bank[play][i]);
    }

    /* 2) 拉本拍 RX */
    for (i = 0; i < s_rx_cnt; i++) {
        (void)rx_pull_frame(&s_rx[i], s_rx_float[i]);
    }

    /* 频谱 DFT 与 UI 同频 ~30Hz；每帧仍累加分析窗 + 更新 RMS */
    now_ns = SDL_GetTicksNS();
    {
        int do_spectrum = (now_ns - s_last_energy_ui_ns >= AUDIO_ENERGY_UI_PERIOD_NS) ? 1 : 0;
        energy_store_all(s_tx_bank[play], s_rx_float, do_spectrum);
        if (do_spectrum) {
            s_last_energy_ui_ns = now_ns;
            arch_display_request_frame();
        }
    }

    handler = s_user_handler;
    memset(s_tx_bank[work], 0, sizeof(s_tx_bank[0]));
    for (i = 0; i < s_tx_cnt; i++) {
        tx_frames[i].stream_buf = s_tx_bank[work][i];
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

    /* 3) 锁外跑 CB，长时间占用不堵推帧 / reopen */
    SDL_UnlockMutex(s_lock);
    if (handler) {
        handler(&info);
    }
    SDL_LockMutex(s_lock);

    s_tx_play_bank = work;
    s_frames_done++;
    return 0;
}

static void audio_polling_task(void* arg)
{
    (void)arg;
    if (!s_initialized) {
        return;
    }

    ui_poll_clicks();

    SDL_LockMutex(s_lock);
    {
        uint64_t now = SDL_GetTicksNS();
        uint64_t expect = (now - s_start_ns) * (uint64_t)BSP_AUDIO_SAMPLE_RATE_HZ /
                          1000000000ull / (uint64_t)BSP_AUDIO_FRAME_SAMPLES;
        while (s_frames_done < expect) {
            if (process_one_frame() != 0) {
                break; /* 队列满：等下次 poll，保持 frames_done 与可播放数据一致 */
            }
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

    path_edit_stop();
    if (s_text_watch_on) {
        SDL_RemoveEventWatch(audio_text_event_watch, NULL);
        s_text_watch_on = 0;
    }

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
    if (s_ui_lock) {
        SDL_DestroyMutex(s_ui_lock);
        s_ui_lock = NULL;
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

    if (s_initialized) {
        return -1;
    }

    if (BSP_AUDIO_RX_CHANNELS == 0u && BSP_AUDIO_TX_CHANNELS == 0u) {
        ERROR_LOG("RX/TX channel counts are both zero");
        return -1;
    }

    s_lock = SDL_CreateMutex();
    s_ui_lock = SDL_CreateMutex();
    if (!s_lock || !s_ui_lock) {
        ERROR_LOG("create mutex failed: %s", SDL_GetError());
        audio_shutdown_partial();
        return -1;
    }

    SDL_SetHint(SDL_HINT_AUDIO_INCLUDE_MONITORS, "1");
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        ERROR_LOG("SDL_InitSubSystem(AUDIO) failed: %s", SDL_GetError());
        audio_shutdown_partial();
        return -1;
    }
    s_audio_subsystem_inited = 1;

    memset(s_rx, 0, sizeof(s_rx));
    memset(s_tx, 0, sizeof(s_tx));
    memset(s_rx_cfg, 0, sizeof(s_rx_cfg));
    memset(s_tx_cfg, 0, sizeof(s_tx_cfg));
    memset(s_rx_path, 0, sizeof(s_rx_path));
    memset(s_tx_path, 0, sizeof(s_tx_path));
    memset(s_rx_path_draft, 0, sizeof(s_rx_path_draft));
    memset(s_tx_path_draft, 0, sizeof(s_tx_path_draft));
    s_rx_cnt = (uint8_t)BSP_AUDIO_RX_CHANNELS;
    s_tx_cnt = (uint8_t)BSP_AUDIO_TX_CHANNELS;

    for (i = 0; i < s_rx_cnt; i++) {
#ifdef BSP_AUDIO_RX_NAMES
        if (i < (uint8_t)(sizeof(s_rx_names_cfg) / sizeof(s_rx_names_cfg[0])) &&
            s_rx_names_cfg[i] != NULL && s_rx_names_cfg[i][0] != '\0') {
            SDL_strlcpy(s_rx_name[i], s_rx_names_cfg[i], sizeof(s_rx_name[i]));
        } else
#endif
        {
            snprintf(s_rx_name[i], sizeof(s_rx_name[i]), "RX%u", (unsigned)i);
        }
        s_rx_cfg[i].name = s_rx_name[i];
        s_rx_cfg[i].rx_impl_type = RX_IMPL_TYPE_NONE;
        SDL_strlcpy(s_rx_path[i], AUDIO_DEFAULT_RX_WAV_PATH, AUDIO_PATH_MAX);
        SDL_strlcpy(s_rx_path_draft[i], s_rx_path[i], AUDIO_PATH_MAX);
    }
    for (i = 0; i < s_tx_cnt; i++) {
#ifdef BSP_AUDIO_TX_NAMES
        if (i < (uint8_t)(sizeof(s_tx_names_cfg) / sizeof(s_tx_names_cfg[0])) &&
            s_tx_names_cfg[i] != NULL && s_tx_names_cfg[i][0] != '\0') {
            SDL_strlcpy(s_tx_name[i], s_tx_names_cfg[i], sizeof(s_tx_name[i]));
        } else
#endif
        {
            snprintf(s_tx_name[i], sizeof(s_tx_name[i]), "TX%u", (unsigned)i);
        }
        s_tx_cfg[i].name = s_tx_name[i];
        s_tx_cfg[i].tx_impl_type = TX_IMPL_TYPE_NONE;
        SDL_strlcpy(s_tx_path[i], AUDIO_DEFAULT_TX_WAV_PATH, AUDIO_PATH_MAX);
        SDL_strlcpy(s_tx_path_draft[i], s_tx_path[i], AUDIO_PATH_MAX);
    }
    bind_live_path_ptrs();
    memcpy(s_rx_draft, s_rx_cfg, sizeof(bsp_audio_system_rx_map_t) * s_rx_cnt);
    memcpy(s_tx_draft, s_tx_cfg, sizeof(bsp_audio_system_tx_map_t) * s_tx_cnt);
    bind_draft_path_ptrs();
    s_ui_mode = AUDIO_UI_ENERGY;
    s_ui_sel = 0;
    s_mouse_was_down = 0;
    s_path_editing = 0;

    if (!SDL_AddEventWatch(audio_text_event_watch, NULL)) {
        WARN_LOG("AddEventWatch failed: %s", SDL_GetError());
    } else {
        s_text_watch_on = 1;
    }

    for (i = 0; i < s_rx_cnt; i++) {
        s_rx[i].map = &s_rx_cfg[i];
        if (rx_open_by_cfg(&s_rx[i]) != 0) {
            audio_shutdown_partial();
            return -1;
        }
    }

    for (i = 0; i < s_tx_cnt; i++) {
        s_tx[i].map = &s_tx_cfg[i];
        if (tx_open_by_cfg(&s_tx[i]) != 0) {
            audio_shutdown_partial();
            return -1;
        }
    }

    memset(s_tx_bank, 0, sizeof(s_tx_bank));
    memset(s_rx_float, 0, sizeof(s_rx_float));
    s_tx_play_bank = 0;
    s_last_energy_ui_ns = 0;
    memset(s_peak_tx, 0, sizeof(s_peak_tx));
    memset(s_peak_rx, 0, sizeof(s_peak_rx));
    memset(s_spec_tx, 0, sizeof(s_spec_tx));
    memset(s_spec_rx, 0, sizeof(s_spec_rx));
    memset(s_pkhz_tx, 0, sizeof(s_pkhz_tx));
    memset(s_pkhz_rx, 0, sizeof(s_pkhz_rx));
    memset(s_spec_hist_tx, 0, sizeof(s_spec_hist_tx));
    memset(s_spec_hist_rx, 0, sizeof(s_spec_hist_rx));
    memset(s_spec_hist_pos_tx, 0, sizeof(s_spec_hist_pos_tx));
    memset(s_spec_hist_pos_rx, 0, sizeof(s_spec_hist_pos_rx));
    memset(s_spec_hist_fill_tx, 0, sizeof(s_spec_hist_fill_tx));
    memset(s_spec_hist_fill_rx, 0, sizeof(s_spec_hist_fill_rx));
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
    INFO_LOG("ready: %u Hz frame=%u tx=%u rx=%u (runtime type cfg via energy panel)",
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

    path_edit_stop();
    if (s_text_watch_on) {
        SDL_RemoveEventWatch(audio_text_event_watch, NULL);
        s_text_watch_on = 0;
    }

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
    if (s_ui_lock) {
        SDL_DestroyMutex(s_ui_lock);
        s_ui_lock = NULL;
    }

    s_user_handler = NULL;
    s_ui_mode = AUDIO_UI_ENERGY;
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

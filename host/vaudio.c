/* Video soundtrack. A second libav demuxer, swresample to packed float
   stereo at 44100 Hz, then Pulse. The peek buffer is what the check reads
   when no server is running. It is not the spectrum capture. */
#include "vaudio.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int bw_audio_play_write(const float* interleaved, int nfloats);
int bw_audio_play_active(void);
void bw_audio_play_stop(void);

static pthread_mutex_t g_peek_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_thr;
static int g_on;
static volatile sig_atomic_t g_stop;
static BwGainFn g_gain;
static char g_path[1100];
static float g_peek[4096];
static int g_peek_n;

static void remember(const float* s, int n) {
    int drop;
    if (!s || n < 2) return;
    if (n > 4096) {
        s += n - 4096;
        n = 4096;
    }
    pthread_mutex_lock(&g_peek_mu);
    drop = g_peek_n + n - 4096;
    if (drop < 0) drop = 0;
    if (drop > g_peek_n) drop = g_peek_n;
    if (drop > 0 && g_peek_n > drop)
        memmove(g_peek, g_peek + drop, (size_t)(g_peek_n - drop) * sizeof(float));
    g_peek_n -= drop;
    memcpy(g_peek + g_peek_n, s, (size_t)n * sizeof(float));
    g_peek_n += n;
    pthread_mutex_unlock(&g_peek_mu);
}

static int chunk_audible(const float* s, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (s[i] > 0.0001f || s[i] < -0.0001f) return 1;
    }
    return 0;
}

static void pace(int nfloats) {
    int left = nfloats / 2;
    while (left > 0 && !g_stop) {
        int frames = left > 1024 ? 1024 : left;
        usleep((useconds_t)((double)frames * 1000000.0 / 44100.0));
        left -= frames;
    }
}

static void emit(const float* s, int n) {
    int i = 0;
    remember(s, n);
    /* A muted track must not open a playback stream. Silence is still
       paced so the decoder stays near the picture. Once the device is
       open, silence keeps that stream from underrunning. */
    if (!chunk_audible(s, n) && !bw_audio_play_active()) {
        pace(n);
        return;
    }
    while (i < n && !g_stop) {
        int chunk = n - i;
        if (chunk > 2048) chunk = 2048;
        if (!bw_audio_play_write(s + i, chunk))
            usleep((useconds_t)((double)(chunk / 2) * 1000000.0 / 44100.0));
        i += chunk;
    }
}

static int setup_swr(AVCodecContext* ctx, struct SwrContext** swr) {
    AVChannelLayout in_layout;
    AVChannelLayout out_layout;
    int rate;
    int err;
    *swr = NULL;
    if (!ctx) return 0;
    rate = ctx->sample_rate;
    if (rate < 1000) return 0;
    if (ctx->sample_fmt == AV_SAMPLE_FMT_NONE) return 0;
    memset(&in_layout, 0, sizeof in_layout);
    memset(&out_layout, 0, sizeof out_layout);
    if (ctx->ch_layout.nb_channels > 0) {
        if (av_channel_layout_copy(&in_layout, &ctx->ch_layout) < 0) return 0;
    } else {
        av_channel_layout_default(&in_layout, 2);
    }
    av_channel_layout_default(&out_layout, 2);
    err = swr_alloc_set_opts2(swr, &out_layout, AV_SAMPLE_FMT_FLT, 44100,
                              &in_layout, ctx->sample_fmt, rate, 0, NULL);
    av_channel_layout_uninit(&in_layout);
    av_channel_layout_uninit(&out_layout);
    if (err < 0 || !*swr || swr_init(*swr) < 0) {
        swr_free(swr);
        return 0;
    }
    return 1;
}

static void convert_frame(struct SwrContext* swr, AVFrame* fr, float gain) {
    int out_count, got, i, n;
    float* buf;
    uint8_t* planes[1];
    if (!swr || !fr || fr->nb_samples < 1) return;
    out_count = swr_get_out_samples(swr, fr->nb_samples);
    if (out_count < fr->nb_samples) out_count = fr->nb_samples;
    if (out_count < 1) return;
    if (out_count > 16384) out_count = 16384;
    buf = (float*)malloc((size_t)(out_count + 32) * 2 * sizeof(float));
    if (!buf) return;
    planes[0] = (uint8_t*)buf;
    got = swr_convert(swr, planes, out_count, (const uint8_t**)fr->extended_data, fr->nb_samples);
    if (got > 0) {
        n = got * 2;
        if (gain <= 0.f) memset(buf, 0, (size_t)n * sizeof(float));
        else if (gain < 0.999f) {
            for (i = 0; i < n; i++) buf[i] *= gain;
        }
        emit(buf, n);
    }
    free(buf);
}

static void* va_main(void* unused) {
    AVFormatContext* fmt = NULL;
    AVCodecContext* ctx = NULL;
    AVPacket* pkt = NULL;
    AVFrame* fr = NULL;
    const AVCodec* codec;
    struct SwrContext* swr = NULL;
    int stream = -1;
    int i, empty = 0;
    (void)unused;
    if (avformat_open_input(&fmt, g_path, NULL, NULL) < 0) return NULL;
    if (avformat_find_stream_info(fmt, NULL) < 0) goto done;
    for (i = 0; i < (int)fmt->nb_streams; i++) {
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            stream = i;
            break;
        }
    }
    if (stream < 0) goto done;
    codec = avcodec_find_decoder(fmt->streams[stream]->codecpar->codec_id);
    if (!codec) goto done;
    ctx = avcodec_alloc_context3(codec);
    if (!ctx) goto done;
    if (avcodec_parameters_to_context(ctx, fmt->streams[stream]->codecpar) < 0) goto done;
    if (avcodec_open2(ctx, codec, NULL) < 0) goto done;
    if (!setup_swr(ctx, &swr)) goto done;
    pkt = av_packet_alloc();
    fr = av_frame_alloc();
    if (!pkt || !fr) goto done;
    while (!g_stop) {
        int ret = avcodec_receive_frame(ctx, fr);
        float gain = g_gain ? g_gain() : 1.f;
        if (ret == 0) {
            empty = 0;
            convert_frame(swr, fr, gain);
            av_frame_unref(fr);
            continue;
        }
        if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) break;
        if (av_read_frame(fmt, pkt) < 0) {
            avcodec_send_packet(ctx, NULL);
            ret = avcodec_receive_frame(ctx, fr);
            if (ret == 0) {
                empty = 0;
                convert_frame(swr, fr, g_gain ? g_gain() : 1.f);
                av_frame_unref(fr);
                continue;
            }
            empty++;
            if (empty >= 3) break;
            av_seek_frame(fmt, -1, 0, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(ctx);
            continue;
        }
        if (pkt->stream_index == stream) avcodec_send_packet(ctx, pkt);
        av_packet_unref(pkt);
    }
done:
    swr_free(&swr);
    if (fr) av_frame_free(&fr);
    if (pkt) av_packet_free(&pkt);
    if (ctx) avcodec_free_context(&ctx);
    if (fmt) avformat_close_input(&fmt);
    return NULL;
}

void bw_va_open(const char* path, BwGainFn gain) {
    bw_va_close();
    if (!path || !path[0]) return;
    snprintf(g_path, sizeof g_path, "%s", path);
    g_gain = gain ? gain : NULL;
    g_stop = 0;
    pthread_mutex_lock(&g_peek_mu);
    g_peek_n = 0;
    pthread_mutex_unlock(&g_peek_mu);
    if (pthread_create(&g_thr, NULL, va_main, NULL) != 0) return;
    g_on = 1;
}

void bw_va_close(void) {
    if (!g_on) return;
    g_stop = 1;
    pthread_join(g_thr, NULL);
    g_on = 0;
    g_stop = 0;
    pthread_mutex_lock(&g_peek_mu);
    g_peek_n = 0;
    pthread_mutex_unlock(&g_peek_mu);
    bw_audio_play_stop();
}

int bw_va_pcm(float* dst, int cap) {
    int n;
    if (!dst || cap < 2) return 0;
    pthread_mutex_lock(&g_peek_mu);
    n = g_peek_n < cap ? g_peek_n : cap;
    if (n > 0) memcpy(dst, g_peek, (size_t)n * sizeof(float));
    pthread_mutex_unlock(&g_peek_mu);
    return n;
}

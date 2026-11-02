/* Scene image for the desktop. Bend still plans the headless 32x32 proof.
   This process keeps the video decoder and the GL context open, draws the
   picture and the text bitmap, then runs the built-in passes. */
#include "plan.h"
#include "player.h"
#include "vaudio.h"

#include <ctype.h>
#include <stdarg.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <math.h>
#include <png.h>
#include <time.h>
#define GL_GLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>

int bw_gl_init(int w, int h);
int bw_gl_begin(void);
int bw_gl_end(void);
void bw_gl_shutdown(void);
int bw_gl_bind(int which);
int bw_gl_clear(void);
int bw_gl_tex(int which);
int bw_gl_prog(const char* vs, const char* fs);
int bw_gl_quad(int prog, int tex,
    uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
    uint32_t u0, uint32_t v0, uint32_t u1, uint32_t v1);
int bw_gl_verts(int prog, int tex, const float* xyuv);
int bw_gl_image(int tex, int w, int h, const uint8_t* px, int n);
int bw_gl_plane(int tex, int w, int h, const uint8_t* px, int comps);
int bw_gl_i1(int prog, const char* name, int v);
int bw_gl_quad_planes(int prog, const int* tex, int ntex,
    uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
    uint32_t u0, uint32_t v0, uint32_t u1, uint32_t v1);
int bw_gl_f1(int prog, const char* name, uint32_t bits);
int bw_gl_fvn(int prog, const char* name, const float* v, int count);
int bw_gl_read(uint8_t* dst, int n);
int bw_gl_export_color(void);
void* bw_gl_color_image(void);
void* bw_gl_present_begin(void);
void bw_gl_present_end(void);
int bw_gl_present_epoch(void);
int bw_gl_bind_shown(void);
const char* bw_gl_log(void);
int bw_gl_vendor_match(const char* peer);
int bw_gl_peer_mismatch(void);
int bw_text_rgba(const char* s, int* w, int* h, uint8_t** rgba);
void bw_audio_inject(const float* s, int n);
int bw_audio_samples(float* dst, int cap);
void bw_audio_live_start(void);
int bw_audio_others(void);
int bw_wm_fullscreen(void);

static const char* k_vs =
  "#version 330 core\n"
  "layout(location=0) in vec3 a_Position;\n"
  "layout(location=1) in vec2 a_TexCoord;\n"
  "out vec2 v;\n"
  "void main(){ v = a_TexCoord; gl_Position = vec4(a_Position, 1.0); }\n";

static const char* k_copy =
  "#version 330 core\n"
  "in vec2 v;\n"
  "uniform sampler2D g_Texture0;\n"
  "uniform float g_Alpha;\n"
  "uniform vec3 g_Color;\n"
  "out vec4 fragColor;\n"
  "void main(){ vec4 c = texture(g_Texture0, v); c.rgb *= g_Color; c.a *= g_Alpha; fragColor = c; }\n";

/* Y and chroma stay in the planes the decoder wrote. The GPU does the
   range expand and the matrix, so the CPU never builds an RGBA frame. */
static const char* k_yuv =
  "#version 330 core\n"
  "in vec2 v;\n"
  "uniform sampler2D g_Texture0;\n"
  "uniform sampler2D g_Tex1;\n"
  "uniform sampler2D g_Tex2;\n"
  "uniform int g_Nv12;\n"
  "uniform int g_Limited;\n"
  "uniform int g_Bt709;\n"
  "uniform float g_Alpha;\n"
  "out vec4 fragColor;\n"
  "void main(){\n"
  "  float y = texture(g_Texture0, v).r;\n"
  "  float u; float c;\n"
  "  if (g_Nv12 == 1) { u = texture(g_Tex1, v).r; c = texture(g_Tex1, v).g; }\n"
  "  else { u = texture(g_Tex1, v).r; c = texture(g_Tex2, v).r; }\n"
  "  float yy = y; float uu = u - 0.5; float vv = c - 0.5;\n"
  "  if (g_Limited == 1) yy = (y - 0.062745098) * 1.164383;\n"
  "  float r; float g; float b;\n"
  "  if (g_Limited == 1 && g_Bt709 == 1) {\n"
  "    r = yy + 1.792741 * vv; g = yy - 0.213249 * uu - 0.532909 * vv; b = yy + 2.112402 * uu;\n"
  "  } else if (g_Limited == 1) {\n"
  "    r = yy + 1.596027 * vv; g = yy - 0.391762 * uu - 0.812968 * vv; b = yy + 2.017232 * uu;\n"
  "  } else if (g_Bt709 == 1) {\n"
  "    r = yy + 1.5748 * vv; g = yy - 0.187324 * uu - 0.468124 * vv; b = yy + 1.8556 * uu;\n"
  "  } else {\n"
  "    r = yy + 1.402 * vv; g = yy - 0.344136 * uu - 0.714136 * vv; b = yy + 1.772 * uu;\n"
  "  }\n"
  "  fragColor = vec4(clamp(vec3(r, g, b), 0.0, 1.0), g_Alpha);\n"
  "}\n";

typedef struct Vid {
  AVFormatContext* fmt;
  AVCodecContext* ctx;
  AVPacket* pkt;
  AVFrame* fr;
  AVFrame* xfer;
  AVBufferRef* hw_dev;
  enum AVPixelFormat hw_pix;
  enum AVHWDeviceType hw_type;
  struct SwsContext* sws;
  int stream;
  int w, h;
  int sw, sh;
  int src_w, src_h;
  int nplane;
  int limited;
  int bt709;
  int serial;
  int said;
  uint8_t* plane[3];
  int plane_cap[3];
  uint8_t* rgba;
  int frame_ms;
  int acc;
  int open;
} Vid;

static Vid g_vid;
static enum AVPixelFormat g_hw_wanted = AV_PIX_FMT_NONE;
static int g_au_logged;
static int g_w, g_h, g_gl, g_open;
static int g_clients, g_gen;
static char g_open_dir[1100];
static int g_pace;
static int g_refresh_hz = 60;
static int g_have_slot;
static long long g_pace_slot;
static int g_copy_prog, g_fx_prog, g_yuv_prog;
static int g_tex, g_text_tex, g_white, g_checker;
static int g_plane[3];
static int g_uploaded;
static int g_dec_us, g_dec_frames;
static char g_dec_how[32];
static int g_text_w, g_text_h;
static float g_scene_w, g_scene_h;
static char g_dir[1100];
#define MAXL 32
typedef struct Live {
  int tex;
  float cx, cy, lw, lh, deg, alpha;
  int full;
  BwSheet* sheets;
  int nsheets;
  BwFrame* frames;
  int nframes;
  int frame_i;
} Live;
static Live g_live[MAXL];
static int g_nlive;
static int g_miss;
static uint8_t* g_px;
/* want is the caller flag. force latches a failed export, a failed import,
   or a GL renderer that is not the same GPU. BENDWALL_READBACK=1 wins. */
static int g_want_readback = 1;
static int g_force_readback;

static int readback_env(void) {
  const char* e = getenv("BENDWALL_READBACK");
  return e && e[0] == '1' && e[1] == 0;
}

static int readback_on(void) {
  return readback_env() || g_want_readback || g_force_readback;
}
static int g_sim_us, g_gl_us, g_read_us;
static int g_ptr_ok;
static float g_ptr_x, g_ptr_y;
static double g_sim;
static float* g_inst;
static int g_inst_cap;

static double mono_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}
static int g_time_ms;
static float g_video_vol = 1.f;
static float g_proj_vol = 1.f;
static int g_user_mute = 1;
static int g_proj_mute;
static int g_duck = 0;
static pthread_mutex_t g_gain_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_play_mu = PTHREAD_MUTEX_INITIALIZER;
static int g_uni;
static int g_scroll, g_spin, g_opacity, g_colorkey, g_tint, g_pulse;
static int g_has_text, g_has_video, g_has_image, g_text_layer;
static char g_text[512];
static char g_page[1100];
static char g_kind[16];
static char g_line[640];
static float g_b16l[16], g_b16r[16], g_b32l[32], g_b32r[32], g_b64l[64], g_b64r[64];
static float g_web[128];

static uint32_t fbits(float f) {
  uint32_t u;
  memcpy(&u, &f, 4);
  return u;
}


static void vid_avlog(void* avcl, int level, const char* fmt, va_list ap) {
  if (fmt && strstr(fmt, "missing picture in access unit")) {
    /* SPS, PPS, SEI, and AUD units are not pictures. One line is enough. */
    if (!g_au_logged) {
      g_au_logged = 1;
      fprintf(stderr, "bendwall: h264 access unit has no picture; further lines suppressed\n");
    }
    return;
  }
  av_log_default_callback(avcl, level, fmt, ap);
}

static void vid_close(void) {
  int i;
  if (g_vid.sws) sws_freeContext(g_vid.sws);
  if (g_vid.fr) av_frame_free(&g_vid.fr);
  if (g_vid.xfer) av_frame_free(&g_vid.xfer);
  if (g_vid.pkt) av_packet_free(&g_vid.pkt);
  if (g_vid.ctx) avcodec_free_context(&g_vid.ctx);
  if (g_vid.fmt) avformat_close_input(&g_vid.fmt);
  if (g_vid.hw_dev) av_buffer_unref(&g_vid.hw_dev);
  free(g_vid.rgba);
  for (i = 0; i < 3; i++) free(g_vid.plane[i]);
  memset(&g_vid, 0, sizeof g_vid);
  g_vid.hw_pix = AV_PIX_FMT_NONE;
  g_vid.hw_type = AV_HWDEVICE_TYPE_NONE;
}

static int copy_plane(uint8_t** dst, int* cap, const uint8_t* src, int src_stride, int w, int h) {
  int y;
  uint8_t* buf;
  if (!src || w < 1 || h < 1 || src_stride < w) return 0;
  buf = *dst;
  if (*cap < w * h) {
    buf = (uint8_t*)realloc(*dst, (size_t)w * (size_t)h);
    if (!buf) return 0;
    *dst = buf;
    *cap = w * h;
  }
  for (y = 0; y < h; y++)
    memcpy(buf + (size_t)y * (size_t)w, src + (size_t)y * (size_t)src_stride, (size_t)w);
  return 1;
}

static int vid_convert_rgba(AVFrame* fr) {
  int dw, dh, y;
  uint8_t* buf;
  AVFrame* rgb;
  if (!fr || fr->width < 1 || fr->height < 1) return 0;
  dw = fr->width;
  dh = fr->height;
  if (dw > 1920) { dh = dh * 1920 / dw; if (dh < 1) dh = 1; dw = 1920; }
  if (dh > 1080) { dw = dw * 1080 / dh; if (dw < 1) dw = 1; dh = 1080; }
  if (!g_vid.sws || g_vid.sw != dw || g_vid.sh != dh || g_vid.src_w != fr->width || g_vid.src_h != fr->height) {
    if (g_vid.sws) sws_freeContext(g_vid.sws);
    g_vid.sws = sws_getContext(fr->width, fr->height, (enum AVPixelFormat)fr->format, dw, dh,
                               AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);
    g_vid.sw = dw;
    g_vid.sh = dh;
    g_vid.src_w = fr->width;
    g_vid.src_h = fr->height;
  }
  if (!g_vid.sws) return 0;
  rgb = av_frame_alloc();
  if (!rgb) return 0;
  rgb->format = AV_PIX_FMT_RGBA;
  rgb->width = dw;
  rgb->height = dh;
  if (av_frame_get_buffer(rgb, 1) < 0) { av_frame_free(&rgb); return 0; }
  sws_scale(g_vid.sws, (const uint8_t* const*)fr->data, fr->linesize, 0, fr->height, rgb->data, rgb->linesize);
  buf = (uint8_t*)malloc((size_t)dw * (size_t)dh * 4);
  if (!buf) { av_frame_free(&rgb); return 0; }
  for (y = 0; y < dh; y++)
    memcpy(buf + (size_t)y * (size_t)dw * 4, rgb->data[0] + (size_t)y * (size_t)rgb->linesize[0], (size_t)dw * 4);
  free(g_vid.rgba);
  g_vid.rgba = buf;
  g_vid.w = dw;
  g_vid.h = dh;
  g_vid.nplane = 0;
  av_frame_free(&rgb);
  return 1;
}

static int vid_store(AVFrame* fr) {
  AVFrame* src = fr;
  int chroma_w, chroma_h;
  if (!fr) return 0;
  if (g_vid.hw_pix != AV_PIX_FMT_NONE && fr->format == g_vid.hw_pix) {
    if (!g_vid.xfer) g_vid.xfer = av_frame_alloc();
    if (!g_vid.xfer) return 0;
    av_frame_unref(g_vid.xfer);
    if (av_hwframe_transfer_data(g_vid.xfer, fr, 0) < 0) return 0;
    src = g_vid.xfer;
  }
  g_vid.limited = src->color_range != AVCOL_RANGE_JPEG && src->format != AV_PIX_FMT_YUVJ420P;
  /* HD is BT.709 unless the stream says BT.601. SD stays BT.601. */
  if (src->colorspace == AVCOL_SPC_BT709) g_vid.bt709 = 1;
  else if (src->colorspace == AVCOL_SPC_BT470BG || src->colorspace == AVCOL_SPC_SMPTE170M)
    g_vid.bt709 = 0;
  else g_vid.bt709 = src->height >= 720;
  if (src->format == AV_PIX_FMT_YUV420P || src->format == AV_PIX_FMT_YUVJ420P) {
    chroma_w = src->width / 2;
    chroma_h = src->height / 2;
    if (chroma_w < 1) chroma_w = 1;
    if (chroma_h < 1) chroma_h = 1;
    if (!copy_plane(&g_vid.plane[0], &g_vid.plane_cap[0], src->data[0], src->linesize[0], src->width, src->height) ||
        !copy_plane(&g_vid.plane[1], &g_vid.plane_cap[1], src->data[1], src->linesize[1], chroma_w, chroma_h) ||
        !copy_plane(&g_vid.plane[2], &g_vid.plane_cap[2], src->data[2], src->linesize[2], chroma_w, chroma_h))
      return 0;
    g_vid.nplane = 3;
    g_vid.w = src->width;
    g_vid.h = src->height;
  } else if (src->format == AV_PIX_FMT_NV12) {
    chroma_w = src->width;
    chroma_h = src->height / 2;
    if (chroma_h < 1) chroma_h = 1;
    if (!copy_plane(&g_vid.plane[0], &g_vid.plane_cap[0], src->data[0], src->linesize[0], src->width, src->height) ||
        !copy_plane(&g_vid.plane[1], &g_vid.plane_cap[1], src->data[1], src->linesize[1], chroma_w, chroma_h))
      return 0;
    g_vid.nplane = 2;
    g_vid.w = src->width;
    g_vid.h = src->height;
  } else if (!vid_convert_rgba(src)) {
    return 0;
  }
  g_vid.serial++;
  if (!g_vid.said) {
    const char* pix = av_get_pix_fmt_name((enum AVPixelFormat)src->format);
    const char* how = "software";
    g_vid.said = 1;
    if (g_vid.hw_dev && fr->format == g_vid.hw_pix && g_vid.hw_pix != AV_PIX_FMT_NONE)
      how = av_hwdevice_get_type_name(g_vid.hw_type);
    if (!how) how = "software";
    snprintf(g_dec_how, sizeof g_dec_how, "%s", how);
    fprintf(stderr, "bendwall: video decode %s %dx%d %s\n", how, g_vid.w, g_vid.h, pix ? pix : "?");
  }
  return 1;
}

static int vid_next_body(int allow_loop) {
  int loops = 0, guard = 0;
  if (!g_vid.open) return 0;
  while (guard++ < 8000) {
    int ret = avcodec_receive_frame(g_vid.ctx, g_vid.fr);
    if (ret == 0) {
      int ok = vid_store(g_vid.fr);
      av_frame_unref(g_vid.fr);
      return ok;
    }
    if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) return 0;
    if (av_read_frame(g_vid.fmt, g_vid.pkt) < 0) {
      avcodec_send_packet(g_vid.ctx, NULL);
      ret = avcodec_receive_frame(g_vid.ctx, g_vid.fr);
      if (ret == 0) {
        int ok = vid_store(g_vid.fr);
        av_frame_unref(g_vid.fr);
        return ok;
      }
      if (!allow_loop || loops) return 0;
      loops = 1;
      av_seek_frame(g_vid.fmt, -1, 0, AVSEEK_FLAG_BACKWARD);
      avcodec_flush_buffers(g_vid.ctx);
      continue;
    }
    if (g_vid.pkt->stream_index == g_vid.stream && g_vid.pkt->size > 0)
      avcodec_send_packet(g_vid.ctx, g_vid.pkt);
    av_packet_unref(g_vid.pkt);
  }
  return 0;
}

static int vid_next(int allow_loop) {
  double t0 = mono_us();
  int ok = vid_next_body(allow_loop);
  if (ok) {
    g_dec_us += (int)(mono_us() - t0);
    g_dec_frames++;
  }
  return ok;
}

static enum AVPixelFormat vid_get_format(AVCodecContext* ctx, const enum AVPixelFormat* fmts) {
  const enum AVPixelFormat* p;
  (void)ctx;
  for (p = fmts; *p != AV_PIX_FMT_NONE; p++)
    if (*p == g_hw_wanted) return *p;
  return fmts[0];
}

static int vid_hw_pix(const AVCodec* codec, enum AVHWDeviceType type, enum AVPixelFormat* pix) {
  int i;
  for (i = 0;; i++) {
    const AVCodecHWConfig* cfg = avcodec_get_hw_config(codec, i);
    if (!cfg) return 0;
    if ((cfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) && cfg->device_type == type) {
      *pix = cfg->pix_fmt;
      return 1;
    }
  }
}

/* VA-API covers Intel and NVIDIA's NVDEC frontend. CUDA is NVDEC itself.
   A missing device leaves the software decoder in place. */
static int vid_attach_hw(const AVCodec* codec) {
  static const enum AVHWDeviceType order[] = {
    AV_HWDEVICE_TYPE_VAAPI, AV_HWDEVICE_TYPE_CUDA, AV_HWDEVICE_TYPE_NONE
  };
  int i, level;
  level = av_log_get_level();
  av_log_set_level(AV_LOG_QUIET);
  for (i = 0; order[i] != AV_HWDEVICE_TYPE_NONE; i++) {
    enum AVPixelFormat pix = AV_PIX_FMT_NONE;
    AVBufferRef* dev = NULL;
    if (!vid_hw_pix(codec, order[i], &pix)) continue;
    if (av_hwdevice_ctx_create(&dev, order[i], NULL, NULL, 0) < 0) continue;
    av_log_set_level(level);
    g_vid.hw_dev = dev;
    g_vid.hw_pix = pix;
    g_vid.hw_type = order[i];
    g_hw_wanted = pix;
    return 1;
  }
  av_log_set_level(level);
  g_hw_wanted = AV_PIX_FMT_NONE;
  return 0;
}

static int vid_open_decoder(const AVCodec* codec, int use_hw) {
  if (g_vid.ctx) avcodec_free_context(&g_vid.ctx);
  if (g_vid.hw_dev) {
    av_buffer_unref(&g_vid.hw_dev);
    g_vid.hw_dev = NULL;
  }
  g_vid.hw_pix = AV_PIX_FMT_NONE;
  g_vid.hw_type = AV_HWDEVICE_TYPE_NONE;
  g_hw_wanted = AV_PIX_FMT_NONE;
  if (use_hw) vid_attach_hw(codec);
  g_vid.ctx = avcodec_alloc_context3(codec);
  if (!g_vid.ctx) return 0;
  if (avcodec_parameters_to_context(g_vid.ctx, g_vid.fmt->streams[g_vid.stream]->codecpar) < 0)
    return 0;
  if (g_vid.hw_dev) {
    g_vid.ctx->hw_device_ctx = av_buffer_ref(g_vid.hw_dev);
    g_vid.ctx->get_format = vid_get_format;
    g_vid.ctx->pix_fmt = g_vid.hw_pix;
  }
  if (avcodec_open2(g_vid.ctx, codec, NULL) < 0) return 0;
  return 1;
}

static int vid_open(const char* path) {
  const AVCodec* codec;
  int i;
  AVRational rate;
  double fps;
  vid_close();
  av_log_set_callback(vid_avlog);
  if (!path || !path[0]) return 0;
  if (avformat_open_input(&g_vid.fmt, path, NULL, NULL) < 0) return 0;
  if (avformat_find_stream_info(g_vid.fmt, NULL) < 0) { vid_close(); return 0; }
  g_vid.stream = -1;
  for (i = 0; i < (int)g_vid.fmt->nb_streams; i++) {
    if (g_vid.fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      g_vid.stream = i;
      break;
    }
  }
  if (g_vid.stream < 0) { vid_close(); return 0; }
  /* Audio packets stay on this demuxer unread. The soundtrack is a second
     demuxer, so a picture seek cannot starve it or land in the spectrum. */
  codec = avcodec_find_decoder(g_vid.fmt->streams[g_vid.stream]->codecpar->codec_id);
  if (!codec) { vid_close(); return 0; }
  if (!vid_open_decoder(codec, 1)) {
    if (!vid_open_decoder(codec, 0)) { vid_close(); return 0; }
  }
  g_vid.pkt = av_packet_alloc();
  g_vid.fr = av_frame_alloc();
  if (!g_vid.pkt || !g_vid.fr) { vid_close(); return 0; }
  rate = g_vid.fmt->streams[g_vid.stream]->avg_frame_rate;
  if (rate.num <= 0 || rate.den <= 0) rate = g_vid.fmt->streams[g_vid.stream]->r_frame_rate;
  fps = (rate.num > 0 && rate.den > 0) ? (double)rate.num / (double)rate.den : 30.0;
  if (fps < 0.5 || fps > 240.0) fps = 30.0;
  g_vid.frame_ms = (int)(1000.0 / fps + 0.5);
  if (g_vid.frame_ms < 1) g_vid.frame_ms = 1;
  g_vid.open = 1;
  if (!vid_next(0)) {
    /* The device opened and then refused the stream. Software still has to play. */
    if (g_vid.hw_dev) {
      av_seek_frame(g_vid.fmt, -1, 0, AVSEEK_FLAG_BACKWARD);
      if (!vid_open_decoder(codec, 0) || !vid_next(0)) { vid_close(); return 0; }
    } else {
      vid_close();
      return 0;
    }
  }
  g_has_video = 1;
  return 1;
}

static void dft_mag(const float* x, int n, float* mag, int bins) {
  const double pi2 = 6.283185307179586;
  int k, i;
  for (k = 0; k < bins; k++) {
    double re = 0, im = 0, m;
    for (i = 0; i < n; i++) {
      double a = -pi2 * (double)k * (double)i / (double)n;
      re += (double)x[i] * cos(a);
      im += (double)x[i] * sin(a);
    }
    m = sqrt(re * re + im * im);
    if (k == 0) m /= (double)n;
    else m /= (double)n * 0.5;
    if (m < 0) m = 0;
    if (m > 1) m = 1;
    mag[k] = (float)m;
  }
}

static void fold_max(const float* src, int src_n, float* dst, int dst_n) {
  int group = src_n / dst_n;
  int i, j;
  for (i = 0; i < dst_n; i++) {
    float m = 0;
    for (j = 0; j < group; j++) {
      float v = src[i * group + j];
      if (v > m) m = v;
    }
    dst[i] = m;
  }
}

static void spectrum_update(void) {
  float raw[4096];
  float left[128], right[128];
  int n, frames, use, i, start;
  memset(g_b64l, 0, sizeof g_b64l);
  memset(g_b64r, 0, sizeof g_b64r);
  memset(g_web, 0, sizeof g_web);
  n = bw_audio_samples(raw, 4096);
  frames = n / 2;
  use = frames > 128 ? 128 : frames;
  if (use >= 32) {
    start = frames - use;
    for (i = 0; i < use; i++) {
      left[i] = raw[(start + i) * 2];
      right[i] = raw[(start + i) * 2 + 1];
    }
    dft_mag(left, use, g_b64l, 64);
    dft_mag(right, use, g_b64r, 64);
  }
  fold_max(g_b64l, 64, g_b32l, 32);
  fold_max(g_b64r, 64, g_b32r, 32);
  fold_max(g_b64l, 64, g_b16l, 16);
  fold_max(g_b64r, 64, g_b16r, 16);
  memcpy(g_web, g_b64l, sizeof g_b64l);
  memcpy(g_web + 64, g_b64r, sizeof g_b64r);
}

static void set_rgb(int prog, float r, float g, float b) {
  GLint loc;
  if (prog <= 0) return;
  glUseProgram((GLuint)prog);
  loc = glGetUniformLocation((GLuint)prog, "g_Color");
  if (loc >= 0) glUniform3f(loc, r, g, b);
}

static int draw_quad(int prog, int tex, int x, int y, int w, int h, float alpha) {
  float x0, y0, x1, y1;
  if (w < 1 || h < 1 || tex <= 0) return 0;
  set_rgb(prog, 1.f, 1.f, 1.f);
  bw_gl_f1(prog, "g_Alpha", fbits(alpha));
  x0 = (float)x / (float)g_w * 2.f - 1.f;
  x1 = (float)(x + w) / (float)g_w * 2.f - 1.f;
  y0 = 1.f - (float)y / (float)g_h * 2.f;
  y1 = 1.f - (float)(y + h) / (float)g_h * 2.f;
  return bw_gl_quad(prog, tex, fbits(x0), fbits(y0), fbits(x1), fbits(y1),
                    fbits(0.f), fbits(0.f), fbits(1.f), fbits(1.f));
}

/* y is top-down scene pixels. Positive degrees rotate clockwise on screen. */
static int draw_live(int prog, const Live* L) {
  float x, y, w, h, cx, cy, hx, hy, rad, c, s;
  float v[20];
  float uvs[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
  float corners[4][2];
  int i;
  if (L->tex <= 0) return 0;
  set_rgb(prog, 1.f, 1.f, 1.f);
  if (L->full || L->lw < 1.f || L->lh < 1.f) {
    x = 0;
    y = 0;
    w = (float)g_w;
    h = (float)g_h;
  } else {
    float sw = g_scene_w > 1.f ? g_scene_w : (float)g_w;
    float sh = g_scene_h > 1.f ? g_scene_h : (float)g_h;
    float cover = sw > 0 && sh > 0 ? fmaxf((float)g_w / sw, (float)g_h / sh) : 1.f;
    float ox = ((float)g_w - sw * cover) * 0.5f;
    float oy = ((float)g_h - sh * cover) * 0.5f;
    w = L->lw * cover;
    h = L->lh * cover;
    x = ox + L->cx * cover - w * 0.5f;
    y = oy + L->cy * cover - h * 0.5f;
  }
  bw_gl_f1(prog, "g_Alpha", fbits(L->alpha <= 0 ? 1.f : L->alpha));
  cx = x + w * 0.5f;
  cy = y + h * 0.5f;
  hx = w * 0.5f;
  hy = h * 0.5f;
  rad = L->deg * 0.01745329251f;
  c = cosf(rad);
  s = sinf(rad);
  corners[0][0] = -hx; corners[0][1] = -hy;
  corners[1][0] = hx; corners[1][1] = -hy;
  corners[2][0] = -hx; corners[2][1] = hy;
  corners[3][0] = hx; corners[3][1] = hy;
  for (i = 0; i < 4; i++) {
    float lx = corners[i][0] * c - corners[i][1] * s;
    float ly = corners[i][0] * s + corners[i][1] * c;
    float px = cx + lx;
    float py = cy + ly;
    v[i * 5] = px / (float)g_w * 2.f - 1.f;
    v[i * 5 + 1] = 1.f - py / (float)g_h * 2.f;
    v[i * 5 + 2] = 0;
    v[i * 5 + 3] = uvs[i][0];
    v[i * 5 + 4] = uvs[i][1];
  }
  return bw_gl_verts(prog, L->tex, v);
}

static int upload_arr(int prog, const char* name, const float* v, int n) {
  char alt[96];
  if (bw_gl_fvn(prog, name, v, n)) return 1;
  snprintf(alt, sizeof alt, "%s[0]", name);
  return bw_gl_fvn(prog, alt, v, n);
}

static char* effect_src(void) {
  char* s = (char*)malloc(4096);
  if (!s) return NULL;
  snprintf(s, 4096,
    "#version 330 core\n"
    "in vec2 v;\n"
    "uniform sampler2D g_Texture0;\n"
    "uniform float g_Time;\n"
    "uniform float g_AudioSpectrum16Left[16];\n"
    "uniform float g_AudioSpectrum16Right[16];\n"
    "uniform float g_AudioSpectrum32Left[32];\n"
    "uniform float g_AudioSpectrum32Right[32];\n"
    "uniform float g_AudioSpectrum64Left[64];\n"
    "uniform float g_AudioSpectrum64Right[64];\n"
    "out vec4 fragColor;\n"
    "void main(){\n"
    "  vec2 uv = v;\n"
    "  %s\n"
    "  %s\n"
    "  vec4 c = texture(g_Texture0, uv);\n"
    "  %s\n"
    "  %s\n"
    "  %s\n"
    "  %s\n"
    "  float keep = g_AudioSpectrum16Left[0] + g_AudioSpectrum16Right[0]\n"
    "    + g_AudioSpectrum32Left[0] + g_AudioSpectrum32Right[0]\n"
    "    + g_AudioSpectrum64Left[0] + g_AudioSpectrum64Right[0];\n"
    "  c.r = clamp(c.r + keep * 0.0001, 0.0, 1.0);\n"
    "  fragColor = vec4(c.rgb * c.a, 1.0);\n"
    "}\n",
    g_scroll ? "uv.x = fract(uv.x + g_Time * 0.15);" : "",
    g_spin ? "{ vec2 c = uv - vec2(0.5); float a = g_Time * 0.8; float s = sin(a); float co = cos(a); uv = vec2(co*c.x - s*c.y, s*c.x + co*c.y) + vec2(0.5); }" : "",
    g_colorkey ? "{ float cut = step(0.5, c.r) * step(c.g, 0.1) * step(c.b, 0.1); c.a *= (1.0 - cut); }" : "",
    g_tint ? "c.rgb = vec3(c.r, c.g * 0.35, c.b * 0.15);" : "",
    g_opacity ? "c.a *= 0.5;" : "",
    g_pulse ? "c.rgb *= 0.5 + 0.5 * sin(g_Time * 6.2831853);" : "");
  return s;
}

static int ensure_fx(void) {
  char* fs;
  if (g_fx_prog) return 1;
  fs = effect_src();
  if (!fs) return 0;
  g_fx_prog = bw_gl_prog(k_vs, fs);
  free(fs);
  return g_fx_prog > 0;
}

static int ensure_checker(void) {
  enum { N = 32 };
  uint8_t* px;
  int x, y;
  if (g_checker > 0) return g_checker;
  px = (uint8_t*)malloc(N * N * 4);
  if (!px) return 0;
  for (y = 0; y < N; y++) {
    for (x = 0; x < N; x++) {
      int on = ((x / 8) ^ (y / 8)) & 1;
      uint8_t* p = px + (y * N + x) * 4;
      p[0] = (uint8_t)(on ? 20 : 220);
      p[1] = (uint8_t)(on ? 180 : 40);
      p[2] = 36;
      p[3] = 255;
    }
  }
  g_checker = bw_gl_image(0, N, N, px, N * N * 4);
  free(px);
  return g_checker;
}

static int draw_yuv(void) {
  float x0, y0, x1, y1;
  if (g_vid.nplane < 2 || g_plane[0] <= 0 || g_plane[1] <= 0) return 0;
  if (!g_yuv_prog) g_yuv_prog = bw_gl_prog(k_vs, k_yuv);
  if (!g_yuv_prog) return 0;
  bw_gl_i1(g_yuv_prog, "g_Nv12", g_vid.nplane == 2 ? 1 : 0);
  bw_gl_i1(g_yuv_prog, "g_Limited", g_vid.limited ? 1 : 0);
  bw_gl_i1(g_yuv_prog, "g_Bt709", g_vid.bt709 ? 1 : 0);
  bw_gl_f1(g_yuv_prog, "g_Alpha", fbits(1.f));
  x0 = -1.f;
  x1 = 1.f;
  y0 = 1.f;
  y1 = -1.f;
  return bw_gl_quad_planes(g_yuv_prog, g_plane, g_vid.nplane,
                           fbits(x0), fbits(y0), fbits(x1), fbits(y1),
                           fbits(0.f), fbits(0.f), fbits(1.f), fbits(1.f));
}

static void upload_picture(void) {
  int uw, uh;
  if (!g_has_video) return;
  if (g_vid.nplane >= 2) {
    if (g_vid.serial == g_uploaded && g_plane[0] > 0) return;
    uw = g_vid.w / 2;
    uh = g_vid.h / 2;
    if (uw < 1) uw = 1;
    if (uh < 1) uh = 1;
    g_plane[0] = bw_gl_plane(g_plane[0], g_vid.w, g_vid.h, g_vid.plane[0], 1);
    if (g_vid.nplane == 2)
      g_plane[1] = bw_gl_plane(g_plane[1], uw, uh, g_vid.plane[1], 2);
    else {
      g_plane[1] = bw_gl_plane(g_plane[1], uw, uh, g_vid.plane[1], 1);
      g_plane[2] = bw_gl_plane(g_plane[2], uw, uh, g_vid.plane[2], 1);
    }
    if (g_plane[0] > 0) g_uploaded = g_vid.serial;
    return;
  }
  if (g_vid.rgba)
    g_tex = bw_gl_image(g_tex, g_vid.w, g_vid.h, g_vid.rgba, g_vid.w * g_vid.h * 4);
}

static void free_live_anim(Live* L) {
  int s;
  if (!L) return;
  if (L->sheets) {
    for (s = 0; s < L->nsheets; s++) free(L->sheets[s].rgba);
    free(L->sheets);
  }
  free(L->frames);
  L->sheets = NULL;
  L->frames = NULL;
  L->nsheets = 0;
  L->nframes = 0;
  L->frame_i = 0;
}

/* TEXS frames follow g_Time. The still upload is frame 0, so the first
   draw can skip until the clock moves onto another frame. */
static void sync_sprites(void) {
  int i;
  for (i = 0; i < g_nlive; i++) {
    Live* L = &g_live[i];
    int fi;
    uint8_t* px = NULL;
    int w = 0, h = 0, tex;
    if (L->nframes < 2 || !L->sheets || !L->frames) continue;
    fi = bw_frame_index(L->frames, L->nframes, g_time_ms);
    if (fi == L->frame_i && L->tex > 0) continue;
    if (!bw_tex_crop(L->sheets, L->nsheets, &L->frames[fi], &px, &w, &h) || !px) continue;
    tex = bw_gl_image(L->tex, w, h, px, w * h * 4);
    free(px);
    if (tex > 0) {
      L->tex = tex;
      L->frame_i = fi;
    }
  }
}

#define MAX_SYS 256
#define MAX_COUNT 4096
#define ROPE_N 12

typedef struct Part {
  float x, y, vx, vy, age, life, size, ang, angv, r, g, b, a;
  float lx, ly, lastdt;
  uint32_t id, fid;
  int frame;
  int osc_init;
  float osc_freq, osc_phase;
  float rx[ROPE_N], ry[ROPE_N], rt[ROPE_N];
  int rn, ri;
} Part;

typedef struct Sys {
  BwPartSys spec;
  int tex;
  int frame_tex[16];
  int frame_w[16], frame_h[16];
  int nframe_tex;
  int n;
  int instant_done;
  int rr;
  float acc;
  uint32_t rng;
  float op_phase, op_speed;
  int op_ready;
  Part* p;
} Sys;

static Sys* g_sys;
static int g_nsys;
static uint32_t g_pid = 1;

static void spawn_one(Sys* S, const Part* host);

static float lerand(uint32_t* s, float a, float b) {
  float t;
  *s = *s * 1664525u + 1013904223u;
  t = (float)(*s >> 8) * (1.f / 16777216.f);
  return a + (b - a) * t;
}

static int alive_count(void) {
  int s, n = 0;
  for (s = 0; s < g_nsys; s++) n += g_sys[s].n;
  return n;
}

static int can_spawn(Sys* S) {
  if (!S || !S->p) return 0;
  if (S->n >= S->spec.maxcount || S->n >= MAX_COUNT) return 0;
  return 1;
}

static float hash3(int x, int y, int z) {
  uint32_t n = (uint32_t)(x * 374761393 + y * 668265263 + z * 2147483647);
  n = (n ^ (n >> 13)) * 1274126177u;
  return (float)(n & 65535) / 65535.f * 2.f - 1.f;
}

static float fade01(float t) { return t * t * (3.f - 2.f * t); }

static float vnoise(float x, float y, float z) {
  int ix = (int)floorf(x), iy = (int)floorf(y), iz = (int)floorf(z);
  float fx = fade01(x - (float)ix), fy = fade01(y - (float)iy), fz = fade01(z - (float)iz);
  float n000 = hash3(ix, iy, iz), n100 = hash3(ix + 1, iy, iz);
  float n010 = hash3(ix, iy + 1, iz), n110 = hash3(ix + 1, iy + 1, iz);
  float n001 = hash3(ix, iy, iz + 1), n101 = hash3(ix + 1, iy, iz + 1);
  float n011 = hash3(ix, iy + 1, iz + 1), n111 = hash3(ix + 1, iy + 1, iz + 1);
  float x00 = n000 + (n100 - n000) * fx;
  float x10 = n010 + (n110 - n010) * fx;
  float x01 = n001 + (n101 - n001) * fx;
  float x11 = n011 + (n111 - n011) * fx;
  float y0 = x00 + (x10 - x00) * fy;
  float y1 = x01 + (x11 - x01) * fy;
  return y0 + (y1 - y0) * fz;
}

static void curl_at(float x, float y, float z, float* ox, float* oy, float* oz) {
  float e = 0.25f;
  float dAzdy = (vnoise(x, y + e, z + 19.1f) - vnoise(x, y - e, z + 19.1f)) / (2.f * e);
  float dAydz = (vnoise(x + 19.1f, y, z + e) - vnoise(x + 19.1f, y, z - e)) / (2.f * e);
  float dAxdz = (vnoise(x, y, z + e) - vnoise(x, y, z - e)) / (2.f * e);
  float dAzdx = (vnoise(x + e, y, z + 19.1f) - vnoise(x - e, y, z + 19.1f)) / (2.f * e);
  float dAydx = (vnoise(x + e + 19.1f, y, z) - vnoise(x - e + 19.1f, y, z)) / (2.f * e);
  float dAxdy = (vnoise(x, y + e, z) - vnoise(x, y - e, z)) / (2.f * e);
  *ox = dAzdy - dAydz;
  *oy = dAxdz - dAzdx;
  *oz = dAydx - dAxdy;
}

static void turb_add(Sys* S, uint32_t* rng, float x, float y, float* vx, float* vy) {
  float sp, fx, fy, len, ox, oy, c, sn;
  if (!S || !S->spec.has_turb) return;
  sp = lerand(rng, S->spec.turb_s0, S->spec.turb_s1);
  fx = S->spec.turb_fx;
  fy = S->spec.turb_fy;
  len = sqrtf(fx * fx + fy * fy);
  if (len < 1e-5f) {
    fx = 0.f;
    fy = 1.f;
    len = 1.f;
  }
  fx /= len;
  fy /= len;
  /* scale 0 is motion only along forward. Offset then rotates that direction. */
  if (S->spec.turb_scale <= 0.0001f) {
    ox = fx;
    oy = fy;
  } else {
    float phase = lerand(rng, S->spec.turb_p0, S->spec.turb_p1);
    float nx, ny, nz, s, along, lat;
    curl_at(x * S->spec.turb_scale + phase, y * S->spec.turb_scale, (float)g_sim * S->spec.turb_time, &nx, &ny, &nz);
    s = S->spec.turb_scale;
    if (s > 2.f) s = 2.f;
    along = 1.f - fminf(s, 1.f);
    lat = fminf(s, 1.f);
    ox = fx * along + nx * lat + S->spec.turb_rx * nz * lat;
    oy = fy * along + ny * lat + S->spec.turb_ry * nz * lat;
    if (s > 1.f) {
      ox += nx * (s - 1.f);
      oy += ny * (s - 1.f);
    }
    len = sqrtf(ox * ox + oy * oy);
    if (len < 1e-5f) {
      ox = fx;
      oy = fy;
    } else {
      ox /= len;
      oy /= len;
    }
  }
  c = cosf(S->spec.turb_off);
  sn = sinf(S->spec.turb_off);
  *vx += (ox * c - oy * sn) * sp;
  *vy += (ox * sn + oy * c) * sp;
}

static void rope_push(Part* p, float x, float y, float t) {
  if (!p) return;
  p->rx[p->ri] = x;
  p->ry[p->ri] = y;
  p->rt[p->ri] = t;
  p->ri = (p->ri + 1) % ROPE_N;
  if (p->rn < ROPE_N) p->rn++;
}

static Part* find_part(uint32_t id) {
  int s, i;
  if (!id) return NULL;
  for (s = 0; s < g_nsys; s++) {
    Sys* S = &g_sys[s];
    for (i = 0; i < S->n; i++)
      if (S->p[i].id == id) return &S->p[i];
  }
  return NULL;
}

static void emit_followers(Sys* parentS, Part* host) {
  int s, k, n, pi;
  if (!parentS || !host || !g_sys) return;
  pi = (int)(parentS - g_sys);
  for (s = 0; s < g_nsys; s++) {
    Sys* C = &g_sys[s];
    if (!C->spec.follow || C->spec.parent != pi) continue;
    n = C->spec.instant;
    for (k = 0; k < n; k++) spawn_one(C, host);
  }
}

static void spawn_one(Sys* S, const Part* host) {
  Part* p;
  float dirx, diry, dist, sp, life, sz, a, cr, cg, cb, ang;
  float lx, ly, vx, vy;
  uint32_t rng;
  if (!can_spawn(S)) return;
  rng = S->rng;
  lx = ly = vx = vy = 0.f;
  if (S->spec.box) {
    float tx = lerand(&rng, -1.f, 1.f);
    float ty = lerand(&rng, -1.f, 1.f);
    float px = (S->spec.bminx + (S->spec.bmaxx - S->spec.bminx) * tx) * S->spec.dvx;
    float py = (S->spec.bminy + (S->spec.bmaxy - S->spec.bminy) * ty) * S->spec.dvy;
    float rx, ry, len;
    px *= S->spec.sx;
    py *= S->spec.sy;
    {
      float rad = S->spec.deg * 0.01745329251f;
      float c = cosf(rad), sn = sinf(rad);
      rx = px * c - py * sn;
      ry = px * sn + py * c;
    }
    lx = rx;
    ly = ry;
    sp = lerand(&rng, S->spec.speed0, S->spec.speed1);
    len = sqrtf(rx * rx + ry * ry);
    if (sp != 0.f && len > 1e-4f) {
      vx = rx / len * sp;
      vy = ry / len * sp;
    }
  } else {
    if (S->spec.dirx * S->spec.dirx + S->spec.diry * S->spec.diry > 1e-8f) {
      dirx = S->spec.dirx;
      diry = S->spec.diry;
    } else {
      float t = lerand(&rng, 0.f, 6.2831853f);
      dirx = cosf(t);
      diry = sinf(t);
    }
    dist = lerand(&rng, S->spec.dist0, S->spec.dist1);
    sp = lerand(&rng, S->spec.speed0, S->spec.speed1);
    lx = dirx * dist;
    ly = diry * dist;
    vx = dirx * sp;
    vy = diry * sp;
  }
  if (S->spec.has_vel) {
    float vx2 = lerand(&rng, S->spec.vminx, S->spec.vmaxx);
    float vy2 = lerand(&rng, S->spec.vminy, S->spec.vmaxy);
    float rad = S->spec.deg * 0.01745329251f;
    float c = cosf(rad), sn = sinf(rad);
    vx += vx2 * c - vy2 * sn;
    vy += vx2 * sn + vy2 * c;
  }
  life = lerand(&rng, S->spec.life0, S->spec.life1);
  if (life < 0.05f) life = 0.05f;
  if (S->spec.size_exp > 0.f && fabsf(S->spec.size_exp - 1.f) > 0.0001f) {
    float u = lerand(&rng, 0.f, 1.f);
    u = powf(u, S->spec.size_exp);
    sz = S->spec.size0 + (S->spec.size1 - S->spec.size0) * u;
  } else
    sz = lerand(&rng, S->spec.size0, S->spec.size1);
  a = lerand(&rng, S->spec.a0, S->spec.a1);
  cr = lerand(&rng, S->spec.r0, S->spec.r1);
  cg = lerand(&rng, S->spec.g0, S->spec.g1);
  cb = lerand(&rng, S->spec.b0, S->spec.b1);
  ang = lerand(&rng, S->spec.ang0, S->spec.ang1);
  turb_add(S, &rng, S->spec.ox + lx, S->spec.oy + ly, &vx, &vy);
  S->rng = rng;
  p = &S->p[S->n++];
  memset(p, 0, sizeof *p);
  p->x = S->spec.ox + lx;
  p->y = S->spec.oy + ly;
  p->vx = vx;
  p->vy = vy;
  p->life = life;
  p->size = sz;
  p->ang = ang;
  p->r = cr;
  p->g = cg;
  p->b = cb;
  p->a = a;
  p->id = g_pid++;
  if (g_pid == 0) g_pid = 1;
  if (S->spec.has_angv) p->angv = lerand(&S->rng, S->spec.av0, S->spec.av1);
  if (S->spec.osc) {
    p->osc_freq = lerand(&S->rng, S->spec.of0, S->spec.of1);
    p->osc_phase = lerand(&S->rng, S->spec.oph0, S->spec.oph1 + 6.2831853f);
    p->osc_init = 1;
  }
  if (S->spec.anim == 1 && S->nframe_tex > 1) {
    float t = lerand(&S->rng, 0.f, (float)S->nframe_tex);
    p->frame = (int)t;
    if (p->frame >= S->nframe_tex) p->frame = S->nframe_tex - 1;
    if (p->frame < 0) p->frame = 0;
  }
  if (host && host->id) {
    p->lx = lx;
    p->ly = ly;
    p->x = host->x + lx;
    p->y = host->y + ly;
    p->fid = host->id;
  }
  if (S->spec.rope) rope_push(p, p->x, p->y, (float)g_sim);
  emit_followers(S, p);
}

/* Phase and speed were chosen once. Position is shifted on x, scaled by
   scale*2, then the normalized curl is masked and added to velocity. */
static void op_turb_at(Sys* S, Part* p, float dt) {
  float nx, ny, nz, len, scale, px, py, pz;
  if (!S || !p || !S->spec.op_turb || !S->op_ready || S->op_speed <= 0.0001f) return;
  scale = S->spec.op_scale * 2.f;
  px = p->x + S->op_phase + S->spec.op_time * (float)g_sim;
  py = p->y;
  pz = 0.f;
  px *= scale;
  py *= scale;
  pz *= scale;
  curl_at(px, py, pz, &nx, &ny, &nz);
  len = sqrtf(nx * nx + ny * ny + nz * nz);
  if (len > 0.0001f) {
    nx = nx / len * S->op_speed * S->spec.op_mx;
    ny = ny / len * S->op_speed * S->spec.op_my;
  } else {
    nx = ny = 0.f;
  }
  p->vx += nx * dt;
  p->vy += ny * dt;
}

static void attract_at(Sys* S, Part* p, float dt) {
  float ax, ay, dx, dy, d;
  if (!S->spec.has_attract || !p) return;
  if (S->spec.cp_mouse) {
    if (!g_ptr_ok) return;
    ax = g_ptr_x;
    ay = g_ptr_y;
  } else {
    ax = S->spec.cpx;
    ay = S->spec.cpy;
  }
  dx = ax - p->x;
  dy = ay - p->y;
  d = sqrtf(dx * dx + dy * dy);
  if (d > 0.001f && d < S->spec.attract_thr) {
    p->vx += dx / d * S->spec.attract * dt;
    p->vy += dy / d * S->spec.attract * dt;
  }
}

static void spin_at(Sys* S, Part* p, float dt) {
  float drag;
  if (!S->spec.ang_op || !p) return;
  p->angv += S->spec.ang_force * dt;
  drag = 1.f - S->spec.ang_drag * dt;
  if (drag < 0.f) drag = 0.f;
  p->angv *= drag;
  p->ang += (p->angv + S->spec.ang_rate) * dt;
}

static void integrate_sys(Sys* S, float dt) {
  int i, w = 0;
  float drag = 1.f - S->spec.drag * dt;
  if (drag < 0.f) drag = 0.f;
  for (i = 0; i < S->n; i++) {
    Part* p = &S->p[i];
    op_turb_at(S, p, dt);
    p->lastdt = dt;
    if (p->fid) {
      Part* q;
      p->lx += p->vx * dt;
      p->ly += p->vy * dt;
      p->vx = (p->vx + S->spec.gravx * dt) * drag;
      p->vy = (p->vy + S->spec.gravy * dt) * drag;
      q = find_part(p->fid);
      if (q && q != p) {
        p->x = q->x + p->lx;
        p->y = q->y + p->ly;
      } else {
        p->fid = 0;
        p->x += p->vx * dt;
        p->y += p->vy * dt;
      }
      attract_at(S, p, dt);
    } else {
      p->x += p->vx * dt;
      p->y += p->vy * dt;
      p->vx = (p->vx + S->spec.gravx * dt) * drag;
      p->vy = (p->vy + S->spec.gravy * dt) * drag;
      attract_at(S, p, dt);
    }
    spin_at(S, p, dt);
    if (S->spec.rope) rope_push(p, p->x, p->y, (float)g_sim);
    p->age += dt;
    if (p->age < p->life) {
      if (w != i) S->p[w] = *p;
      w++;
    }
  }
  S->n = w;
}

static void sim_sys(Sys* S, float dt) {
  int guard = 0;
  if (!S || dt <= 0.f) return;
  if (!S->instant_done) {
    int k;
    S->instant_done = 1;
    /* eventfollow children are born from the parent, not on their own clock. */
    if (!S->spec.follow) {
      for (k = 0; k < S->spec.instant && can_spawn(S); k++) spawn_one(S, NULL);
    }
  }
  if (S->spec.follow && S->spec.parent >= 0 && S->spec.parent < g_nsys) {
    int parents = g_sys[S->spec.parent].n;
    if (S->spec.rate > 0.f && parents > 0) {
      S->acc += S->spec.rate * dt * (float)parents;
      while (S->acc >= 1.f && can_spawn(S) && guard++ < S->spec.maxcount) {
        Part* host = &g_sys[S->spec.parent].p[S->rr % parents];
        S->rr++;
        spawn_one(S, host);
        S->acc -= 1.f;
      }
    }
  } else if (!S->spec.follow) {
    S->acc += S->spec.rate * dt;
    while (S->acc >= 1.f && can_spawn(S) && guard++ < S->spec.maxcount) {
      spawn_one(S, NULL);
      S->acc -= 1.f;
    }
  }
  integrate_sys(S, dt);
}

static void sim_particles(float dt) {
  int s;
  if (dt <= 0.f || g_nsys <= 0 || !g_sys) return;
  g_sim += (double)dt;
  for (s = 0; s < g_nsys; s++) sim_sys(&g_sys[s], dt);
}

static float warm_horizon(const Sys* S) {
  float h;
  if (!S) return 0.f;
  h = S->spec.start;
  if (S->spec.follow && S->spec.parent >= 0 && S->spec.parent < g_nsys) {
    float p = g_sys[S->spec.parent].spec.start;
    if (p > h) h = p;
  }
  if (h > 12.f) h = 12.f;
  return h;
}

static void warmup_particles(void) {
  float horizon = 0.f, t, dt;
  int s, logged = 0;
  if (!g_sys || g_nsys <= 0) return;
  for (s = 0; s < g_nsys; s++) {
    if (g_sys[s].spec.start > 12.f && !logged) {
      fprintf(stderr, "bendwall: particle starttime clamped\n");
      logged = 1;
    }
    if (warm_horizon(&g_sys[s]) > horizon) horizon = warm_horizon(&g_sys[s]);
  }
  if (horizon <= 0.f) return;
  t = 0.f;
  while (t < horizon - 1e-4f) {
    dt = 1.f / 30.f;
    if (t + dt > horizon) dt = horizon - t;
    g_sim += (double)dt;
    for (s = 0; s < g_nsys; s++) {
      if (warm_horizon(&g_sys[s]) > t) sim_sys(&g_sys[s], dt);
    }
    t += dt;
  }
}

static int draw_sprite(int prog, int tex, float cx, float cy, float w, float h, float deg, float alpha, float r, float g, float b) {
  float hx, hy, rad, c, s;
  float v[20];
  float uvs[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
  float corners[4][2];
  int i;
  if (tex <= 0 || w < 0.5f || h < 0.5f) return 0;
  if (r < 0.f) r = 0.f;
  if (g < 0.f) g = 0.f;
  if (b < 0.f) b = 0.f;
  set_rgb(prog, r, g, b);
  bw_gl_f1(prog, "g_Alpha", fbits(alpha));
  hx = w * 0.5f;
  hy = h * 0.5f;
  rad = deg * 0.01745329251f;
  c = cosf(rad);
  s = sinf(rad);
  corners[0][0] = -hx; corners[0][1] = -hy;
  corners[1][0] = hx; corners[1][1] = -hy;
  corners[2][0] = -hx; corners[2][1] = hy;
  corners[3][0] = hx; corners[3][1] = hy;
  for (i = 0; i < 4; i++) {
    float lx = corners[i][0] * c - corners[i][1] * s;
    float ly = corners[i][0] * s + corners[i][1] * c;
    float px = cx + lx;
    float py = cy + ly;
    v[i * 5] = px / (float)g_w * 2.f - 1.f;
    v[i * 5 + 1] = 1.f - py / (float)g_h * 2.f;
    v[i * 5 + 2] = 0;
    v[i * 5 + 3] = uvs[i][0];
    v[i * 5 + 4] = uvs[i][1];
  }
  return bw_gl_verts(prog, tex, v);
}

static void cover_of(float* ox, float* oy, float* cover) {
  float sw = g_scene_w > 1.f ? g_scene_w : (float)g_w;
  float sh = g_scene_h > 1.f ? g_scene_h : (float)g_h;
  float c = 1.f;
  if (sw > 0.f && sh > 0.f) c = fmaxf((float)g_w / sw, (float)g_h / sh);
  *cover = c;
  *ox = ((float)g_w - sw * c) * 0.5f;
  *oy = ((float)g_h - sh * c) * 0.5f;
}

static int g_part_prog;
static GLuint g_part_vao, g_part_quad, g_part_inst;
static GLint g_part_view = -1, g_part_samp = -1;
static int g_part_tried;

static const char* k_part_vs =
  "#version 330 core\n"
  "layout(location=0) in vec3 a_Position;\n"
  "layout(location=1) in vec2 a_TexCoord;\n"
  "layout(location=2) in vec4 a_Center;\n"
  "layout(location=3) in vec4 a_Rot;\n"
  "layout(location=4) in vec3 a_Color;\n"
  "uniform vec2 g_View;\n"
  "out vec2 v;\n"
  "out vec4 v_Tint;\n"
  "void main(){\n"
  "  vec2 local = vec2(a_Position.x * a_Center.z, a_Position.y * a_Center.w);\n"
  "  float c = a_Rot.x, s = a_Rot.y;\n"
  "  vec2 rot = vec2(local.x * c - local.y * s, local.x * s + local.y * c);\n"
  "  vec2 px = a_Center.xy + rot;\n"
  "  gl_Position = vec4(px.x / g_View.x * 2.0 - 1.0, 1.0 - px.y / g_View.y * 2.0, 0.0, 1.0);\n"
  "  v = a_TexCoord;\n"
  "  v_Tint = vec4(a_Color, a_Rot.z);\n"
  "}\n";

static const char* k_part_fs =
  "#version 330 core\n"
  "in vec2 v;\n"
  "in vec4 v_Tint;\n"
  "uniform sampler2D g_Texture0;\n"
  "out vec4 fragColor;\n"
  "void main(){ vec4 c = texture(g_Texture0, v); c.rgb *= v_Tint.rgb; c.a *= v_Tint.a; fragColor = c; }\n";

static void free_part_batch(void) {
  if (g_part_prog > 0) glDeleteProgram((GLuint)g_part_prog);
  if (g_part_vao) glDeleteVertexArrays(1, &g_part_vao);
  if (g_part_quad) glDeleteBuffers(1, &g_part_quad);
  if (g_part_inst) glDeleteBuffers(1, &g_part_inst);
  g_part_prog = 0;
  g_part_vao = g_part_quad = g_part_inst = 0;
  g_part_view = g_part_samp = -1;
  g_part_tried = 0;
  free(g_inst);
  g_inst = NULL;
  g_inst_cap = 0;
}

static int ensure_part_batch(void) {
  static const float quad[20] = {
    -1.f, -1.f, 0.f, 0.f, 0.f,
     1.f, -1.f, 0.f, 1.f, 0.f,
    -1.f,  1.f, 0.f, 0.f, 1.f,
     1.f,  1.f, 0.f, 1.f, 1.f
  };
  int prog;
  if (g_part_tried) return g_part_prog > 0;
  g_part_tried = 1;
  prog = bw_gl_prog(k_part_vs, k_part_fs);
  if (prog <= 0) return 0;
  glGenVertexArrays(1, &g_part_vao);
  glGenBuffers(1, &g_part_quad);
  glGenBuffers(1, &g_part_inst);
  glBindVertexArray(g_part_vao);
  glBindBuffer(GL_ARRAY_BUFFER, g_part_quad);
  glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * (GLsizei)sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * (GLsizei)sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glVertexAttribDivisor(0, 0);
  glVertexAttribDivisor(1, 0);
  glBindBuffer(GL_ARRAY_BUFFER, g_part_inst);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(64 * 11 * sizeof(float)), NULL, GL_DYNAMIC_DRAW);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 11 * (GLsizei)sizeof(float), (void*)0);
  glEnableVertexAttribArray(2);
  glVertexAttribDivisor(2, 1);
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 11 * (GLsizei)sizeof(float), (void*)(4 * sizeof(float)));
  glEnableVertexAttribArray(3);
  glVertexAttribDivisor(3, 1);
  glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 11 * (GLsizei)sizeof(float), (void*)(8 * sizeof(float)));
  glEnableVertexAttribArray(4);
  glVertexAttribDivisor(4, 1);
  glBindVertexArray(0);
  g_part_prog = prog;
  g_part_view = glGetUniformLocation((GLuint)prog, "g_View");
  g_part_samp = glGetUniformLocation((GLuint)prog, "g_Texture0");
  return 1;
}

static int part_batch_draw(int tex, const float* inst, int n) {
  if (n < 1 || tex <= 0 || g_part_prog <= 0 || !inst) return 0;
  glUseProgram((GLuint)g_part_prog);
  if (g_part_view >= 0) glUniform2f(g_part_view, (float)g_w, (float)g_h);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
  if (g_part_samp >= 0) glUniform1i(g_part_samp, 0);
  glBindVertexArray(g_part_vao);
  glBindBuffer(GL_ARRAY_BUFFER, g_part_inst);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(n * 11 * (int)sizeof(float)), inst, GL_DYNAMIC_DRAW);
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, n);
  glBindVertexArray(0);
  return 1;
}

static void particle_tint(const Sys* S, const Part* p, float* sz, float* a, float* r, float* g, float* b) {
  float u;
  *sz = 0.f;
  *a = 0.f;
  *r = *g = *b = 1.f;
  if (!S || !p || p->life <= 0.f) return;
  u = p->age / p->life;
  if (u < 0.f) u = 0.f;
  if (u > 1.f) u = 1.f;
  if (S->spec.size_b >= 0.f)
    *sz = p->size * (S->spec.size_a + (S->spec.size_b - S->spec.size_a) * u);
  else
    *sz = p->size + S->spec.size_rate * p->age;
  *a = p->a;
  /* fadeintime and fadeouttime are fractions of the lifetime. */
  if (S->spec.alphafade) {
    float fin = 1.f, fout = 1.f;
    if (S->spec.fade_in > 0.f && u < S->spec.fade_in) fin = u / S->spec.fade_in;
    if (S->spec.fade_out > 0.f && S->spec.fade_out < 1.f && u > S->spec.fade_out)
      fout = (1.f - u) / (1.f - S->spec.fade_out);
    if (fin < 0.f) fin = 0.f;
    if (fin > 1.f) fin = 1.f;
    if (fout < 0.f) fout = 0.f;
    if (fout > 1.f) fout = 1.f;
    *a *= fin * fout;
  }
  if (S->spec.osc && p->osc_init) {
    float cosv = (cosf(p->osc_freq * p->age + p->osc_phase) + 1.f) * 0.5f;
    float mul = S->spec.os0 + (S->spec.os1 - S->spec.os0) * cosv;
    *a *= mul;
  }
  *r = p->r;
  *g = p->g;
  *b = p->b;
  if (S->spec.color_lerp) {
    *r = p->r + (S->spec.er - p->r) * u;
    *g = p->g + (S->spec.eg - p->g) * u;
    *b = p->b + (S->spec.eb - p->b) * u;
  }
}

static int part_frame(const Sys* S, const Part* p) {
  int n, f;
  float u, mul;
  if (!S || !p) return 0;
  n = S->nframe_tex;
  if (n <= 1 || S->spec.anim == 0) return 0;
  if (S->spec.anim == 1) {
    f = p->frame;
    if (f < 0) f = 0;
    if (f >= n) f = n - 1;
    return f;
  }
  u = p->life > 0.f ? p->age / p->life : 0.f;
  if (u < 0.f) u = 0.f;
  if (u > 1.f) u = 1.f;
  mul = S->spec.seqmul;
  f = (int)(u * (float)n * mul);
  if (f < 0) f = 0;
  if (n > 0) f %= n;
  return f;
}

static float* inst_need(int n) {
  float* p;
  if (n < 1) n = 1;
  if (n <= g_inst_cap) return g_inst;
  p = (float*)realloc(g_inst, (size_t)n * 11u * sizeof(float));
  if (!p) return NULL;
  g_inst = p;
  g_inst_cap = n;
  return g_inst;
}

static void draw_rope(const Sys* S, const Part* p, float ox, float oy, float cover, float sz, float a, float r, float g, float b) {
  float pts[ROPE_N + 2][2];
  int n = 0, k, oldest, tex;
  if (!S || !p || S->tex <= 0) return;
  tex = S->tex;
  oldest = (p->ri - p->rn + ROPE_N) % ROPE_N;
  for (k = 0; k < p->rn; k++) {
    int idx = (oldest + k) % ROPE_N;
    float age = (float)g_sim - p->rt[idx];
    if (age > S->spec.rope_len + 0.001f) continue;
    pts[n][0] = p->rx[idx];
    pts[n][1] = p->ry[idx];
    n++;
  }
  if (n < 2 && p->lastdt > 0.f) {
    pts[0][0] = p->x - p->vx * p->lastdt;
    pts[0][1] = p->y - p->vy * p->lastdt;
    pts[1][0] = p->x;
    pts[1][1] = p->y;
    n = 2;
  }
  for (k = 1; k < n; k++) {
    float x0 = ox + pts[k - 1][0] * cover;
    float y0 = oy + pts[k - 1][1] * cover;
    float x1 = ox + pts[k][0] * cover;
    float y1 = oy + pts[k][1] * cover;
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    float deg;
    if (len < 0.5f) continue;
    deg = atan2f(dy, dx) * 57.2957795f;
    draw_sprite(g_copy_prog, tex, (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, len, sz * cover, deg, a, r, g, b);
  }
}

static void draw_particles(void) {
  int s, i, batch, pass;
  float ox, oy, cover;
  if (g_nsys <= 0 || !g_sys || g_copy_prog <= 0) return;
  cover_of(&ox, &oy, &cover);
  batch = ensure_part_batch();
  glEnable(GL_BLEND);
  for (s = 0; s < g_nsys; s++) {
    Sys* S = &g_sys[s];
    if (S->tex <= 0 && S->nframe_tex < 1) continue;
    if (S->spec.additive) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (S->spec.rope && !S->spec.trail) {
      for (i = 0; i < S->n; i++) {
        float sz, a, r, g, b;
        particle_tint(S, &S->p[i], &sz, &a, &r, &g, &b);
        if (a <= 0.001f || sz < 0.5f) continue;
        draw_rope(S, &S->p[i], ox, oy, cover, sz, a, r, g, b);
      }
      continue;
    }
    for (pass = 0; pass < (S->nframe_tex > 0 ? S->nframe_tex : 1); pass++) {
      float* inst = NULL;
      int n = 0;
      int tex = S->nframe_tex > pass ? S->frame_tex[pass] : S->tex;
      if (tex <= 0) continue;
      if (batch) inst = inst_need(S->n > 0 ? S->n : 1);
      for (i = 0; i < S->n; i++) {
        Part* p = &S->p[i];
        float sz, a, r, g, b, c, sn, hx, hy, aspect;
        int fr, fw, fh;
        if (part_frame(S, p) != pass) continue;
        particle_tint(S, p, &sz, &a, &r, &g, &b);
        if (a <= 0.001f || sz < 0.5f) continue;
        fr = pass;
        fw = S->nframe_tex > fr ? S->frame_w[fr] : 1;
        fh = S->nframe_tex > fr ? S->frame_h[fr] : 1;
        aspect = fw > 0 ? (float)fh / (float)fw : 1.f;
        {
          float dw = sz;
          float dh = sz * aspect;
          float deg = p->ang * 57.2957795f;
          float ang = p->ang;
          if (S->spec.trail) {
            float spd = sqrtf(p->vx * p->vx + p->vy * p->vy);
            float stretch = spd * S->spec.trail_len;
            if (stretch < S->spec.trail_min) stretch = S->spec.trail_min;
            if (stretch > S->spec.trail_max) stretch = S->spec.trail_max;
            dw = sz * stretch;
            if (dw < 0.5f) continue;
            ang = atan2f(p->vy, p->vx);
            deg = ang * 57.2957795f;
          }
          if (!batch || !inst) {
            draw_sprite(g_copy_prog, tex,
                        ox + p->x * cover, oy + p->y * cover,
                        dw * cover, dh * cover, deg, a, r, g, b);
            continue;
          }
          c = cosf(ang);
          sn = sinf(ang);
          hx = dw * cover * 0.5f;
          hy = dh * cover * 0.5f;
        }
        inst[n * 11 + 0] = ox + p->x * cover;
        inst[n * 11 + 1] = oy + p->y * cover;
        inst[n * 11 + 2] = hx;
        inst[n * 11 + 3] = hy;
        inst[n * 11 + 4] = c;
        inst[n * 11 + 5] = sn;
        inst[n * 11 + 6] = a;
        inst[n * 11 + 7] = 0.f;
        inst[n * 11 + 8] = r;
        inst[n * 11 + 9] = g;
        inst[n * 11 + 10] = b;
        n++;
      }
      if (batch && inst && n > 0) part_batch_draw(tex, inst, n);
    }
  }
  glDisable(GL_BLEND);
}

static int upload_frames(Sys* S, const BwPartSys* src) {
  int n, i;
  if (!S || !src) return 0;
  if (src->nframes >= 2 && src->sheets && src->frames) {
    n = src->nframes;
    if (n > 16) n = 16;
    for (i = 0; i < n; i++) {
      uint8_t* px = NULL;
      int w = 0, h = 0, tex;
      if (!bw_tex_crop(src->sheets, src->nsheets, &src->frames[i], &px, &w, &h) || !px) continue;
      tex = bw_gl_image(0, w, h, px, w * h * 4);
      free(px);
      if (tex <= 0) continue;
      S->frame_tex[S->nframe_tex] = tex;
      S->frame_w[S->nframe_tex] = w;
      S->frame_h[S->nframe_tex] = h;
      S->nframe_tex++;
    }
  }
  if (S->nframe_tex < 1 && src->rgba && src->tw > 0 && src->th > 0) {
    int tex = bw_gl_image(0, src->tw, src->th, src->rgba, src->tw * src->th * 4);
    if (tex > 0) {
      S->frame_tex[0] = tex;
      S->frame_w[0] = src->tw;
      S->frame_h[0] = src->th;
      S->nframe_tex = 1;
    }
  }
  S->tex = S->nframe_tex > 0 ? S->frame_tex[0] : 0;
  return S->tex > 0;
}

static void load_particles(const BwPlan* plan) {
  int i;
  g_nsys = 0;
  g_pid = 1;
  g_sim = 0;
  if (!plan || plan->nparts < 1) return;
  g_sys = (Sys*)calloc((size_t)plan->nparts, sizeof(Sys));
  if (!g_sys) return;
  g_nsys = plan->nparts;
  for (i = 0; i < plan->nparts; i++) {
    const BwPartSys* src = &plan->parts[i];
    Sys* S = &g_sys[i];
    int cap;
    S->spec = *src;
    S->spec.rgba = NULL;
    S->spec.sheets = NULL;
    S->spec.frames = NULL;
    S->rng = 1u + (uint32_t)i * 997u;
    if (S->spec.op_turb) {
      S->op_phase = lerand(&S->rng, S->spec.op_p0, S->spec.op_p1);
      S->op_speed = lerand(&S->rng, S->spec.op_s0, S->spec.op_s1);
      S->op_ready = 1;
    }
    cap = S->spec.maxcount;
    if (cap < 0) cap = 0;
    if (cap > MAX_COUNT) cap = MAX_COUNT;
    S->spec.maxcount = cap;
    if (cap > 0) S->p = (Part*)calloc((size_t)cap, sizeof(Part));
    upload_frames(S, src);
  }
  /* starttime pre-simulates. It does not delay the first visible frame,
     and it does not move the wallpaper clock. */
  warmup_particles();
}

static void free_particle_tex(void) {
  int i, j;
  if (g_sys) {
    for (i = 0; i < g_nsys; i++) {
      for (j = 0; j < g_sys[i].nframe_tex; j++) {
        if (g_sys[i].frame_tex[j] > 0) {
          GLuint t = (GLuint)g_sys[i].frame_tex[j];
          glDeleteTextures(1, &t);
        }
      }
      free(g_sys[i].p);
      g_sys[i].p = NULL;
      g_sys[i].tex = 0;
      g_sys[i].n = 0;
    }
    free(g_sys);
  }
  g_sys = NULL;
  g_nsys = 0;
}

static int draw(void) {
  int bar_h, band_w, i, scale, nread;
  double tdraw = mono_us();
  const float* bands[6];
  const char* names[6] = {
    "g_AudioSpectrum16Left", "g_AudioSpectrum16Right",
    "g_AudioSpectrum32Left", "g_AudioSpectrum32Right",
    "g_AudioSpectrum64Left", "g_AudioSpectrum64Right"
  };
  const int counts[6] = {16, 16, 32, 32, 64, 64};
  if (!g_gl) return 0;
  if (!bw_gl_begin()) return 0;
  if (!g_copy_prog) g_copy_prog = bw_gl_prog(k_vs, k_copy);
  if (!ensure_fx() || !g_copy_prog) { bw_gl_end(); return 0; }
  upload_picture();
  sync_sprites();
  if (!bw_gl_bind(1) || !bw_gl_clear()) { bw_gl_end(); return 0; }
  if (g_has_video && g_vid.nplane >= 2) {
    draw_yuv();
  } else if (g_has_video && g_tex > 0) {
    draw_quad(g_copy_prog, g_tex, 0, 0, g_w, g_h, 1.f);
  } else if (g_nlive > 0) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (i = 0; i < g_nlive; i++) draw_live(g_copy_prog, &g_live[i]);
    glDisable(GL_BLEND);
  } else if (g_nsys <= 0) {
    if (g_dir[0] && strcmp(g_kind, "web") != 0 && strcmp(g_kind, "demo") != 0 && !g_text_layer)
      fprintf(stderr, "bendwall: no picture in %s\n", g_dir);
    if (ensure_checker() > 0) draw_quad(g_copy_prog, g_checker, 0, 0, g_w, g_h, 1.f);
  }
  draw_particles();
  if (!bw_gl_bind(0) || !bw_gl_clear()) { bw_gl_end(); return 0; }
  bw_gl_f1(g_fx_prog, "g_Time", fbits((float)g_time_ms / 1000.f));
  bands[0] = g_b16l; bands[1] = g_b16r; bands[2] = g_b32l;
  bands[3] = g_b32r; bands[4] = g_b64l; bands[5] = g_b64r;
  g_uni = 0;
  for (i = 0; i < 6; i++) if (upload_arr(g_fx_prog, names[i], bands[i], counts[i])) g_uni++;
  /* The picture was drawn with v=0 at the top of FBO 1. Sampling that
     attachment with the same UVs reads the bottom row first, which flips
     the video and the textures. The text quad is drawn after this pass. */
  {
    float x0 = -1.f, y0 = 1.f, x1 = 1.f, y1 = -1.f;
    bw_gl_quad(g_fx_prog, bw_gl_tex(1), fbits(x0), fbits(y0), fbits(x1), fbits(y1),
               fbits(0.f), fbits(1.f), fbits(1.f), fbits(0.f));
  }
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  if (g_has_text && g_text_tex > 0) {
    scale = g_h / 90;
    if (scale < 1) scale = 1;
    if (scale > 2) scale = 2;
    draw_quad(g_copy_prog, g_text_tex, 8, 8, g_text_w * scale, g_text_h * scale, 1.f);
  }
  bar_h = g_h / 10;
  if (bar_h < 4) bar_h = 4;
  band_w = g_w / 32;
  if (band_w < 1) band_w = 1;
  if (g_white <= 0) {
    uint8_t wpx[4] = {255, 255, 255, 255};
    g_white = bw_gl_image(0, 1, 1, wpx, 4);
  }
  if (g_white > 0) {
    for (i = 0; i < 16; i++) {
      int lh = (int)(g_b16l[i] * (float)bar_h);
      int rh = (int)(g_b16r[i] * (float)bar_h);
      if (lh > 0) draw_quad(g_copy_prog, g_white, i * band_w, g_h - lh, band_w, lh, 1.f);
      if (rh > 0) draw_quad(g_copy_prog, g_white, g_w / 2 + i * band_w, g_h - rh, band_w, rh, 1.f);
    }
  }
  glDisable(GL_BLEND);
  g_gl_us = (int)(mono_us() - tdraw);
  if (!readback_on()) {
    if (!bw_gl_export_color()) g_force_readback = 1;
    else {
      glFlush();
      g_read_us = 0;
      bw_gl_end();
      return 1;
    }
  }
  {
    double tr = mono_us();
    uint8_t* next = (uint8_t*)realloc(g_px, (size_t)g_w * (size_t)g_h * 4);
    if (!next) { bw_gl_end(); return 0; }
    g_px = next;
    nread = bw_gl_read(g_px, g_w * g_h * 4);
    g_read_us = (int)(mono_us() - tr);
  }
  bw_gl_end();
  return nread > 0;
}

static void refresh_line(void) {
  snprintf(g_line, sizeof g_line,
    "kind=%s video=%d text=%d image=%d scroll=%d spin=%d opacity=%d colorkey=%d tint=%d pulse=%d uni=%d step=%d b16=%.3f b32=%.3f b64=%.3f r16=%.3f shader=%d miss=%d parts=%d alive=%d vol=%.2f t=%d",
    g_kind, g_has_video, g_has_text, g_has_image, g_scroll, g_spin, g_opacity, g_colorkey,
    g_tint, g_pulse, g_uni, g_vid.frame_ms, g_b16l[0], g_b32l[0], g_b64l[0], g_b16r[0],
    g_fx_prog > 0 ? 1 : 0, g_miss, g_nsys, alive_count(), g_video_vol, g_time_ms);
}

static void prepare_text(void) {
  uint8_t* px = NULL;
  int w = 0, h = 0;
  if (!g_text[0]) { g_has_text = 0; return; }
  if (!bw_text_rgba(g_text, &w, &h, &px) || !px) { g_has_text = 0; return; }
  g_text_tex = bw_gl_image(g_text_tex, w, h, px, w * h * 4);
  g_text_w = w;
  g_text_h = h;
  g_has_text = g_text_tex > 0;
  free(px);
}

static void reset_plan(void) {
  g_scroll = g_spin = g_opacity = g_colorkey = g_tint = g_pulse = 0;
  g_has_text = g_has_video = g_has_image = g_text_layer = 0;
  g_text[0] = 0;
  g_page[0] = 0;
  snprintf(g_kind, sizeof g_kind, "demo");
  g_time_ms = 0;
  g_tex = 0;
  g_text_tex = 0;
  g_white = 0;
  g_checker = 0;
  g_plane[0] = g_plane[1] = g_plane[2] = 0;
  g_uploaded = 0;
  g_copy_prog = 0;
  g_fx_prog = 0;
  g_yuv_prog = 0;
  g_part_prog = 0;
  g_part_vao = g_part_quad = g_part_inst = 0;
  g_part_view = g_part_samp = -1;
  g_part_tried = 0;
  g_nlive = 0;
  g_nsys = 0;
  g_miss = 0;
  g_scene_w = g_scene_h = 0;
  g_dir[0] = 0;
}

static void play_close_unlocked(void) {
  if (g_gl && bw_gl_begin()) {
    if (g_tex > 0) { GLuint t = (GLuint)g_tex; glDeleteTextures(1, &t); }
    if (g_plane[0] > 0) { GLuint t = (GLuint)g_plane[0]; glDeleteTextures(1, &t); }
    if (g_plane[1] > 0) { GLuint t = (GLuint)g_plane[1]; glDeleteTextures(1, &t); }
    if (g_plane[2] > 0) { GLuint t = (GLuint)g_plane[2]; glDeleteTextures(1, &t); }
    if (g_text_tex > 0) { GLuint t = (GLuint)g_text_tex; glDeleteTextures(1, &t); }
    if (g_white > 0) { GLuint t = (GLuint)g_white; glDeleteTextures(1, &t); }
    if (g_checker > 0) { GLuint t = (GLuint)g_checker; glDeleteTextures(1, &t); }
    for (int i = 0; i < g_nlive; i++) {
      if (g_live[i].tex > 0) {
        GLuint t = (GLuint)g_live[i].tex;
        glDeleteTextures(1, &t);
      }
      free_live_anim(&g_live[i]);
    }
    free_particle_tex();
    free_part_batch();
    if (g_copy_prog > 0) glDeleteProgram((GLuint)g_copy_prog);
    if (g_fx_prog > 0) glDeleteProgram((GLuint)g_fx_prog);
    if (g_yuv_prog > 0) glDeleteProgram((GLuint)g_yuv_prog);
    bw_gl_shutdown();
    bw_gl_end();
  }
  bw_va_close();
  vid_close();
  free(g_px);
  g_px = NULL;
  g_gl = 0;
  g_open = 0;
  reset_plan();
}

static float video_gain(void) {
  float vol, proj, g;
  int mute, proj_mute, duck;
  const char* env;
  pthread_mutex_lock(&g_gain_mu);
  vol = g_video_vol;
  proj = g_proj_vol;
  mute = g_user_mute;
  proj_mute = g_proj_mute;
  duck = g_duck;
  pthread_mutex_unlock(&g_gain_mu);
  if (mute || proj_mute) return 0.f;
  if (duck > 0) return 0.f;
  if (duck < 0) {
    env = getenv("BENDWALL_VIDEO_DUCK");
    if (!(env && env[0] == '0' && env[1] == 0)) {
      if (bw_audio_others() || bw_wm_fullscreen()) return 0.f;
    }
  }
  g = vol * proj;
  if (g < 0.f) g = 0.f;
  if (g > 1.f) g = 1.f;
  return g;
}

void bw_play_set_video_volume(float volume) {
  if (volume < 0.f) volume = 0.f;
  if (volume > 1.f) volume = 1.f;
  pthread_mutex_lock(&g_gain_mu);
  g_video_vol = volume;
  pthread_mutex_unlock(&g_gain_mu);
}

float bw_play_video_volume(void) {
  float v;
  pthread_mutex_lock(&g_gain_mu);
  v = g_video_vol;
  pthread_mutex_unlock(&g_gain_mu);
  return v;
}

void bw_play_set_video_mute(int on) {
  pthread_mutex_lock(&g_gain_mu);
  g_user_mute = on ? 1 : 0;
  pthread_mutex_unlock(&g_gain_mu);
}

void bw_play_set_video_duck(int mode) {
  if (mode > 1) mode = 1;
  if (mode < -1) mode = -1;
  pthread_mutex_lock(&g_gain_mu);
  g_duck = mode;
  pthread_mutex_unlock(&g_gain_mu);
}

int bw_play_video_pcm(float* dst, int cap) { return bw_va_pcm(dst, cap); }

void bw_play_audio(const float* interleaved, int nfloats) {
  bw_audio_inject(interleaved, nfloats);
}

static int pace_ms_unlocked(void);

void bw_play_close(void) {
  pthread_mutex_lock(&g_play_mu);
  g_clients = 0;
  g_gen++;
  g_open_dir[0] = 0;
  play_close_unlocked();
  pthread_mutex_unlock(&g_play_mu);
}

int bw_play_generation(void) {
  int gen;
  pthread_mutex_lock(&g_play_mu);
  gen = g_gen;
  pthread_mutex_unlock(&g_play_mu);
  return gen;
}

void bw_play_release(int generation) {
  pthread_mutex_lock(&g_play_mu);
  if (generation != g_gen || g_clients <= 0) {
    pthread_mutex_unlock(&g_play_mu);
    return;
  }
  g_clients--;
  if (g_clients == 0) {
    g_open_dir[0] = 0;
    play_close_unlocked();
  }
  pthread_mutex_unlock(&g_play_mu);
}

void bw_play_set_pace(int on) {
  pthread_mutex_lock(&g_play_mu);
  g_pace = on ? 1 : 0;
  pthread_mutex_unlock(&g_play_mu);
}

void bw_play_set_refresh(int hz) {
  pthread_mutex_lock(&g_play_mu);
  if (hz < 20) hz = 60;
  if (hz > 360) hz = 360;
  g_refresh_hz = hz;
  pthread_mutex_unlock(&g_play_mu);
}

int bw_play_pace_ms(void) {
  int ms;
  pthread_mutex_lock(&g_play_mu);
  ms = pace_ms_unlocked();
  pthread_mutex_unlock(&g_play_mu);
  return ms;
}

void bw_play_video_cost(int* cpu_us, int* frames, char* how, int how_n) {
  pthread_mutex_lock(&g_play_mu);
  if (cpu_us) *cpu_us = g_dec_us;
  if (frames) *frames = g_dec_frames;
  if (how && how_n > 0) snprintf(how, (size_t)how_n, "%s", g_dec_how[0] ? g_dec_how : "none");
  pthread_mutex_unlock(&g_play_mu);
}

int bw_play_open(const char* dir, int w, int h) {
  BwPlan plan;
  const char* assets;
  int i, keep_read, opened;
  pthread_mutex_lock(&g_play_mu);
  if (g_open && g_clients > 0 && dir && dir[0] && g_open_dir[0] && strcmp(g_open_dir, dir) == 0) {
    g_clients++;
    pthread_mutex_unlock(&g_play_mu);
    return 1;
  }
  keep_read = g_want_readback;
  play_close_unlocked();
  g_clients = 0;
  g_gen++;
  g_open_dir[0] = 0;
  g_have_slot = 0;
  g_dec_us = 0;
  g_dec_frames = 0;
  g_dec_how[0] = 0;
  g_want_readback = keep_read;
  g_force_readback = 0;
  g_ptr_ok = 0;
  g_sim_us = g_gl_us = g_read_us = 0;
  if (w < 8) w = 8;
  if (h < 8) h = 8;
  if (w > 1920) w = 1920;
  if (h > 1080) h = 1080;
  g_w = w;
  g_h = h;
  reset_plan();
  if (dir && dir[0]) snprintf(g_dir, sizeof g_dir, "%s", dir);
  assets = getenv("BENDWALL_ASSETS");
  bw_plan_load(dir, assets, &plan);
  snprintf(g_kind, sizeof g_kind, "%s", plan.kind[0] ? plan.kind : "scene");
  snprintf(g_text, sizeof g_text, "%s", plan.text);
  snprintf(g_page, sizeof g_page, "%s", plan.page);
  g_scroll = plan.scroll;
  g_spin = plan.spin;
  g_opacity = plan.opacity;
  g_colorkey = plan.colorkey;
  g_tint = plan.tint;
  g_pulse = plan.pulse;
  g_has_image = plan.image_ok > 0;
  g_text_layer = plan.has_text;
  g_miss = plan.missing;
  g_scene_w = plan.scene_w;
  g_scene_h = plan.scene_h;
  pthread_mutex_lock(&g_gain_mu);
  g_proj_vol = plan.video_volume;
  if (g_proj_vol < 0.f) g_proj_vol = 0.f;
  if (g_proj_vol > 1.f) g_proj_vol = 1.f;
  g_proj_mute = plan.video_mute ? 1 : 0;
  pthread_mutex_unlock(&g_gain_mu);
  if (plan.video[0] && strcmp(g_kind, "video") == 0) {
    if (!vid_open(plan.video)) fprintf(stderr, "bendwall: missing video %s\n", plan.video);
    bw_va_open(plan.video, video_gain);
  }
  if (!bw_gl_init(g_w, g_h)) {
    bw_plan_free(&plan);
    snprintf(g_line, sizeof g_line, "kind=fail shader=0");
    pthread_mutex_unlock(&g_play_mu);
    return 0;
  }
  if (bw_gl_peer_mismatch()) g_force_readback = 1;
  g_gl = 1;
  bw_audio_live_start();
  if (!g_has_video && bw_gl_begin()) {
    for (i = 0; i < plan.nlayers && g_nlive < MAXL; i++) {
      BwLayer* L = &plan.layers[i];
      int tex;
      if (!L->rgba) continue;
      tex = bw_gl_image(0, L->w, L->h, L->rgba, L->w * L->h * 4);
      if (tex <= 0) continue;
      g_live[g_nlive].tex = tex;
      g_live[g_nlive].cx = L->cx;
      g_live[g_nlive].cy = L->cy;
      g_live[g_nlive].lw = L->lw;
      g_live[g_nlive].lh = L->lh;
      g_live[g_nlive].deg = L->deg;
      g_live[g_nlive].alpha = L->alpha;
      g_live[g_nlive].full = L->full;
      g_live[g_nlive].sheets = NULL;
      g_live[g_nlive].frames = NULL;
      g_live[g_nlive].nsheets = 0;
      g_live[g_nlive].nframes = 0;
      g_live[g_nlive].frame_i = 0;
      if (L->nframes >= 2 && L->sheets && L->frames) {
        g_live[g_nlive].sheets = L->sheets;
        g_live[g_nlive].nsheets = L->nsheets;
        g_live[g_nlive].frames = L->frames;
        g_live[g_nlive].nframes = L->nframes;
        L->sheets = NULL;
        L->frames = NULL;
        L->nsheets = 0;
        L->nframes = 0;
      }
      g_nlive++;
    }
    load_particles(&plan);
    prepare_text();
    bw_gl_end();
  } else if (bw_gl_begin()) {
    load_particles(&plan);
    prepare_text();
    bw_gl_end();
  }
  bw_plan_free(&plan);
  spectrum_update();
  g_open = draw();
  if (g_open && dir && dir[0]) {
    snprintf(g_open_dir, sizeof g_open_dir, "%s", dir);
    g_clients = 1;
  }
  refresh_line();
  opened = g_open;
  pthread_mutex_unlock(&g_play_mu);
  return opened;
}

static int pace_ms_unlocked(void) {
  int frame = g_vid.frame_ms > 0 ? g_vid.frame_ms : 33;
  int refresh = g_refresh_hz > 0 ? (int)(1000.0 / (double)g_refresh_hz + 0.5) : 16;
  int ms = frame > refresh ? frame : refresh;
  if (ms < 8) ms = 8;
  if (ms > 100) ms = 100;
  return ms;
}

int bw_play_step(int dt_ms) {
  int guard = 0;
  int cap = 180;
  int paced = 0;
  double t0;
  int ok = 0;
  pthread_mutex_lock(&g_play_mu);
  if (!g_open) goto step_out;
  if (dt_ms < 0) dt_ms = 0;
  if (g_pace && g_vid.open) {
    double now = mono_us();
    int ms = pace_ms_unlocked();
    long long slot = (long long)(now / ((double)ms * 1000.0));
    /* Both screens tick on one thread. A step that lands in the slot just
       drawn, including one queued while that draw ran past the boundary,
       reuses the frame. */
    if (g_have_slot && slot <= g_pace_slot) {
      ok = 1;
      goto step_out;
    }
    g_have_slot = 1;
    g_pace_slot = slot;
    dt_ms = ms;
    cap = 1;
    paced = 1;
  }
  g_time_ms += dt_ms;
  t0 = mono_us();
  sim_particles((float)dt_ms / 1000.f);
  g_sim_us = (int)(mono_us() - t0);
  if (g_vid.open) {
    g_vid.acc += dt_ms;
    /* A one-second step on a 30fps clip is ~30 frames. The old cap of 8
       left the picture almost where it started, so --play-frames compared
       two copies of the same image. */
    while (g_vid.frame_ms > 0 && g_vid.acc >= g_vid.frame_ms && guard++ < cap) {
      g_vid.acc -= g_vid.frame_ms;
      if (!vid_next(1)) break;
    }
    /* A 30 Hz panel showing 60 fps video adds more time than one frame.
       Leave the extra behind so the next tick does not owe a burst. */
    if (paced && g_vid.frame_ms > 0 && g_vid.acc >= g_vid.frame_ms) g_vid.acc = 0;
  }
  spectrum_update();
  if (!draw()) goto step_out;
  if (paced) {
    double now = mono_us();
    int ms = pace_ms_unlocked();
    long long slot = (long long)(now / ((double)ms * 1000.0));
    if (slot > g_pace_slot) g_pace_slot = slot;
  }
  refresh_line();
  ok = 1;
step_out:
  pthread_mutex_unlock(&g_play_mu);
  return ok;
}

int bw_play_rgba(uint8_t* dst, int cap) {
  int n, gotn = 0;
  pthread_mutex_lock(&g_play_mu);
  if (!dst || !g_open) goto rgba_out;
  n = g_w * g_h * 4;
  if (cap < n) goto rgba_out;
  if (!readback_on() || !g_px) {
    uint8_t* next;
    int got;
    if (!bw_gl_begin()) goto rgba_out;
    if (!bw_gl_bind_shown()) { bw_gl_end(); goto rgba_out; }
    next = (uint8_t*)realloc(g_px, (size_t)n);
    if (!next) { bw_gl_end(); goto rgba_out; }
    g_px = next;
    got = bw_gl_read(g_px, n);
    bw_gl_end();
    if (got <= 0) goto rgba_out;
  }
  if (!g_px) goto rgba_out;
  memcpy(dst, g_px, (size_t)n);
  gotn = n;
rgba_out:
  pthread_mutex_unlock(&g_play_mu);
  return gotn;
}

void bw_play_set_readback(int on) {
  pthread_mutex_lock(&g_play_mu);
  if (!readback_env()) g_want_readback = on ? 1 : 0;
  pthread_mutex_unlock(&g_play_mu);
}

void bw_play_pointer(float x, float y, float item_w, float item_h, int valid) {
  float ox, oy, cover;
  pthread_mutex_lock(&g_play_mu);
  if (!valid || item_w < 1.f || item_h < 1.f || g_w < 1 || g_h < 1) {
    g_ptr_ok = 0;
    pthread_mutex_unlock(&g_play_mu);
    return;
  }
  cover_of(&ox, &oy, &cover);
  if (cover < 1e-6f) cover = 1.f;
  /* Item pixels map through the same cover fit as the scene. */
  g_ptr_x = ((x / item_w) * (float)g_w - ox) / cover;
  g_ptr_y = ((y / item_h) * (float)g_h - oy) / cover;
  g_ptr_ok = 1;
  pthread_mutex_unlock(&g_play_mu);
}

void bw_play_profile(int* sim_us, int* gl_us, int* read_us) {
  pthread_mutex_lock(&g_play_mu);
  if (sim_us) *sim_us = g_sim_us;
  if (gl_us) *gl_us = g_gl_us;
  if (read_us) *read_us = g_read_us;
  pthread_mutex_unlock(&g_play_mu);
}

int bw_play_exported(void) { return bw_gl_color_image() != NULL; }

static int blit_prog(EGLContext cur, GLuint* prog, GLuint* vao) {
  static const char* vs =
    "#version 330 core\n"
    "layout(location=0) in vec2 a;\n"
    "out vec2 v;\n"
    "void main(){\n"
    "  v = a * 0.5 + 0.5;\n"
    "  gl_Position = vec4(a, 0.0, 1.0);\n"
    "}\n";
  static const char* fs =
    "#version 330 core\n"
    "in vec2 v;\n"
    "uniform sampler2D g_Texture0;\n"
    "out vec4 fragColor;\n"
    "void main(){ fragColor = texture(g_Texture0, v); }\n";
  static struct {
    EGLContext ctx;
    GLuint prog, vao, buf;
  } gpu[4];
  int i, free_gpu = -1;
  for (i = 0; i < 4; i++) {
    if (gpu[i].ctx == cur && gpu[i].prog) {
      *prog = gpu[i].prog;
      *vao = gpu[i].vao;
      return 1;
    }
    if (free_gpu < 0 && !gpu[i].prog) free_gpu = i;
  }
  if (free_gpu < 0) return 0;
  {
    GLuint vsid, fsid, p, va, buf;
    const float quad[8] = {-1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f};
    int ok;
    vsid = glCreateShader(GL_VERTEX_SHADER);
    fsid = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vsid, 1, &vs, NULL);
    glShaderSource(fsid, 1, &fs, NULL);
    glCompileShader(vsid);
    glCompileShader(fsid);
    p = glCreateProgram();
    glAttachShader(p, vsid);
    glAttachShader(p, fsid);
    glBindAttribLocation(p, 0, "a");
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    glDeleteShader(vsid);
    glDeleteShader(fsid);
    if (!ok) {
      glDeleteProgram(p);
      return 0;
    }
    glGenVertexArrays(1, &va);
    glGenBuffers(1, &buf);
    glBindVertexArray(va);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    gpu[free_gpu].ctx = cur;
    gpu[free_gpu].prog = p;
    gpu[free_gpu].vao = va;
    gpu[free_gpu].buf = buf;
    *prog = p;
    *vao = va;
  }
  return 1;
}

static int blit_unlocked(void) {
  enum { CONS_N = 8 };
  static struct {
    EGLContext ctx;
    void* img;
    GLuint tex;
  } cons[CONS_N];
  static int cons_epoch;
  static int failed;
  void* img;
  EGLContext cur;
  typedef void (*PFN_TARGET)(GLenum, void*);
  PFN_TARGET target;
  GLuint tex = 0, prog = 0, vao = 0;
  GLint loc;
  int i, slot;
  if (g_force_readback || readback_env()) return 0;
  cur = eglGetCurrentContext();
  if (cur == EGL_NO_CONTEXT) return 0;
  {
    const char* peer = (const char*)glGetString(GL_RENDERER);
    if (peer && !bw_gl_vendor_match(peer)) {
      g_force_readback = 1;
      return 0;
    }
  }
  if (bw_gl_present_epoch() != cons_epoch) {
    for (i = 0; i < CONS_N; i++) {
      if (cons[i].tex && cons[i].ctx == cur) glDeleteTextures(1, &cons[i].tex);
      cons[i].tex = 0;
      cons[i].img = NULL;
      cons[i].ctx = EGL_NO_CONTEXT;
    }
    cons_epoch = bw_gl_present_epoch();
    failed = 0;
  }
  if (failed) return 0;
  /* Wait until the player's draws of this frame have landed, then sample
     that slot's image. The image is imported once per Qt context. */
  img = bw_gl_present_begin();
  if (!img) return 0;
  target = (PFN_TARGET)eglGetProcAddress("glEGLImageTargetTexture2DOES");
  if (!target || !blit_prog(cur, &prog, &vao)) {
    bw_gl_present_end();
    if (!target) failed = 1;
    return 0;
  }
  slot = -1;
  for (i = 0; i < CONS_N; i++) {
    if (cons[i].ctx == cur && cons[i].img == img && cons[i].tex) {
      tex = cons[i].tex;
      break;
    }
    if (slot < 0 && !cons[i].tex) slot = i;
  }
  if (!tex) {
    if (slot < 0) {
      bw_gl_present_end();
      return 0;
    }
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGetError();
    target(GL_TEXTURE_2D, img);
    if (glGetError() != GL_NO_ERROR) {
      glDeleteTextures(1, &tex);
      bw_gl_present_end();
      g_force_readback = 1;
      failed = 1;
      return 0;
    }
    cons[slot].ctx = cur;
    cons[slot].img = img;
    cons[slot].tex = tex;
  }
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glUseProgram(prog);
  loc = glGetUniformLocation(prog, "g_Texture0");
  if (loc >= 0) glUniform1i(loc, 0);
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindVertexArray(0);
  glUseProgram(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  bw_gl_present_end();
  return 1;
}

int bw_play_blit(void) {
  int ok;
  pthread_mutex_lock(&g_play_mu);
  ok = blit_unlocked();
  pthread_mutex_unlock(&g_play_mu);
  return ok;
}

int bw_play_size(int* w, int* h) {
  if (w) *w = g_w;
  if (h) *h = g_h;
  return g_open;
}

const char* bw_play_line(void) { return g_line; }
const char* bw_play_kind(void) { return g_kind; }
const char* bw_play_page(void) { return g_page; }

int bw_play_audio_out(float* dst128) {
  if (!dst128) return 0;
  memcpy(dst128, g_web, sizeof g_web);
  return 1;
}

int bw_play_band(int which, int index, float* out) {
  const float* p = NULL;
  int n = 0;
  if (!out) return 0;
  if (which == 0) { p = g_b16l; n = 16; }
  else if (which == 1) { p = g_b16r; n = 16; }
  else if (which == 2) { p = g_b32l; n = 32; }
  else if (which == 3) { p = g_b32r; n = 32; }
  else if (which == 4) { p = g_b64l; n = 64; }
  else if (which == 5) { p = g_b64r; n = 64; }
  else return 0;
  if (index < 0 || index >= n) return 0;
  *out = p[index];
  return 1;
}

int bw_play_png(const char* path) {
  png_image img;
  if (!g_px || !path) return 0;
  memset(&img, 0, sizeof img);
  img.version = PNG_IMAGE_VERSION;
  img.width = (png_uint_32)g_w;
  img.height = (png_uint_32)g_h;
  img.format = PNG_FORMAT_RGBA;
  if (!png_image_write_to_file(&img, path, 0, g_px, g_w * 4, NULL)) return 0;
  return 1;
}

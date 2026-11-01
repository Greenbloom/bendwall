/* Scene image for the desktop. Bend still plans the headless 32x32 proof.
   This process keeps the video decoder and the GL context open, draws the
   picture and the text bitmap, then runs the built-in passes. */
#include "plan.h"
#include "player.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <math.h>
#include <png.h>

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
int bw_gl_f1(int prog, const char* name, uint32_t bits);
int bw_gl_fvn(int prog, const char* name, const float* v, int count);
int bw_gl_read(uint8_t* dst, int n);
const char* bw_gl_log(void);
int bw_text_rgba(const char* s, int* w, int* h, uint8_t** rgba);
void bw_audio_inject(const float* s, int n);
int bw_audio_samples(float* dst, int cap);
void bw_audio_live_start(void);

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
  "out vec4 fragColor;\n"
  "void main(){ vec4 c = texture(g_Texture0, v); c.a *= g_Alpha; fragColor = c; }\n";

typedef struct Vid {
  AVFormatContext* fmt;
  AVCodecContext* ctx;
  AVPacket* pkt;
  AVFrame* fr;
  struct SwsContext* sws;
  int stream;
  int w, h;
  int sw, sh;
  int src_w, src_h;
  uint8_t* rgba;
  int frame_ms;
  int acc;
  int open;
} Vid;

static Vid g_vid;
static int g_w, g_h, g_gl, g_open;
static int g_copy_prog, g_fx_prog;
static int g_tex, g_text_tex, g_white, g_checker;
static int g_text_w, g_text_h;
static float g_scene_w, g_scene_h;
static char g_dir[1100];
#define MAXL 32
typedef struct Live {
  int tex;
  float cx, cy, lw, lh, deg, alpha;
  int full;
} Live;
static Live g_live[MAXL];
static int g_nlive;
static int g_miss;
static uint8_t* g_px;
static int g_time_ms;
static int g_uni;
static int g_scroll, g_spin, g_opacity, g_colorkey, g_tint, g_pulse;
static int g_has_text, g_has_video, g_has_image, g_text_layer;
static char g_text[160];
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


static void vid_close(void) {
  if (g_vid.sws) sws_freeContext(g_vid.sws);
  if (g_vid.fr) av_frame_free(&g_vid.fr);
  if (g_vid.pkt) av_packet_free(&g_vid.pkt);
  if (g_vid.ctx) avcodec_free_context(&g_vid.ctx);
  if (g_vid.fmt) avformat_close_input(&g_vid.fmt);
  free(g_vid.rgba);
  memset(&g_vid, 0, sizeof g_vid);
}

static int vid_convert(AVFrame* fr) {
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
    g_vid.sws = sws_getContext(fr->width, fr->height, fr->format, dw, dh,
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
  av_frame_free(&rgb);
  return 1;
}

static int vid_next(int allow_loop) {
  int loops = 0, guard = 0;
  if (!g_vid.open) return 0;
  while (guard++ < 8000) {
    int ret = avcodec_receive_frame(g_vid.ctx, g_vid.fr);
    if (ret == 0) {
      int ok = vid_convert(g_vid.fr);
      av_frame_unref(g_vid.fr);
      return ok;
    }
    if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) return 0;
    if (av_read_frame(g_vid.fmt, g_vid.pkt) < 0) {
      avcodec_send_packet(g_vid.ctx, NULL);
      ret = avcodec_receive_frame(g_vid.ctx, g_vid.fr);
      if (ret == 0) {
        int ok = vid_convert(g_vid.fr);
        av_frame_unref(g_vid.fr);
        return ok;
      }
      if (!allow_loop || loops) return 0;
      loops = 1;
      av_seek_frame(g_vid.fmt, -1, 0, AVSEEK_FLAG_BACKWARD);
      avcodec_flush_buffers(g_vid.ctx);
      continue;
    }
    if (g_vid.pkt->stream_index == g_vid.stream)
      avcodec_send_packet(g_vid.ctx, g_vid.pkt);
    av_packet_unref(g_vid.pkt);
  }
  return 0;
}

static int vid_open(const char* path) {
  const AVCodec* codec;
  int i;
  AVRational rate;
  double fps;
  vid_close();
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
  codec = avcodec_find_decoder(g_vid.fmt->streams[g_vid.stream]->codecpar->codec_id);
  if (!codec) { vid_close(); return 0; }
  g_vid.ctx = avcodec_alloc_context3(codec);
  if (!g_vid.ctx) { vid_close(); return 0; }
  if (avcodec_parameters_to_context(g_vid.ctx, g_vid.fmt->streams[g_vid.stream]->codecpar) < 0) {
    vid_close();
    return 0;
  }
  if (avcodec_open2(g_vid.ctx, codec, NULL) < 0) { vid_close(); return 0; }
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
  if (!vid_next(0)) { vid_close(); return 0; }
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

static int draw_quad(int prog, int tex, int x, int y, int w, int h, float alpha) {
  float x0, y0, x1, y1;
  if (w < 1 || h < 1 || tex <= 0) return 0;
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

static void upload_picture(void) {
  if (g_has_video && g_vid.rgba)
    g_tex = bw_gl_image(g_tex, g_vid.w, g_vid.h, g_vid.rgba, g_vid.w * g_vid.h * 4);
}

static int draw(void) {
  int bar_h, band_w, i, scale, nread;
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
  if (!bw_gl_bind(1) || !bw_gl_clear()) { bw_gl_end(); return 0; }
  if (g_has_video && g_tex > 0) {
    draw_quad(g_copy_prog, g_tex, 0, 0, g_w, g_h, 1.f);
  } else if (g_nlive > 0) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (i = 0; i < g_nlive; i++) draw_live(g_copy_prog, &g_live[i]);
    glDisable(GL_BLEND);
  } else {
    if (g_dir[0] && strcmp(g_kind, "web") != 0 && strcmp(g_kind, "demo") != 0 && !g_text_layer)
      fprintf(stderr, "bendwall: no picture in %s\n", g_dir);
    if (ensure_checker() > 0) draw_quad(g_copy_prog, g_checker, 0, 0, g_w, g_h, 1.f);
  }
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
    if (scale > 8) scale = 8;
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
  {
    uint8_t* next = (uint8_t*)realloc(g_px, (size_t)g_w * (size_t)g_h * 4);
    if (!next) { bw_gl_end(); return 0; }
    g_px = next;
  }
  nread = bw_gl_read(g_px, g_w * g_h * 4);
  bw_gl_end();
  return nread > 0;
}

static void refresh_line(void) {
  snprintf(g_line, sizeof g_line,
    "kind=%s video=%d text=%d image=%d scroll=%d spin=%d opacity=%d colorkey=%d tint=%d pulse=%d uni=%d step=%d b16=%.3f b32=%.3f b64=%.3f r16=%.3f shader=%d miss=%d",
    g_kind, g_has_video, g_has_text, g_has_image, g_scroll, g_spin, g_opacity, g_colorkey,
    g_tint, g_pulse, g_uni, g_vid.frame_ms, g_b16l[0], g_b32l[0], g_b64l[0], g_b16r[0],
    g_fx_prog > 0 ? 1 : 0, g_miss);
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
  g_copy_prog = 0;
  g_fx_prog = 0;
  g_nlive = 0;
  g_miss = 0;
  g_scene_w = g_scene_h = 0;
  g_dir[0] = 0;
}

void bw_play_close(void) {
  if (g_gl && bw_gl_begin()) {
    if (g_tex > 0) { GLuint t = (GLuint)g_tex; glDeleteTextures(1, &t); }
    if (g_text_tex > 0) { GLuint t = (GLuint)g_text_tex; glDeleteTextures(1, &t); }
    if (g_white > 0) { GLuint t = (GLuint)g_white; glDeleteTextures(1, &t); }
    if (g_checker > 0) { GLuint t = (GLuint)g_checker; glDeleteTextures(1, &t); }
    for (int i = 0; i < g_nlive; i++) {
      if (g_live[i].tex > 0) {
        GLuint t = (GLuint)g_live[i].tex;
        glDeleteTextures(1, &t);
      }
    }
    if (g_copy_prog > 0) glDeleteProgram((GLuint)g_copy_prog);
    if (g_fx_prog > 0) glDeleteProgram((GLuint)g_fx_prog);
    bw_gl_shutdown();
    bw_gl_end();
  }
  vid_close();
  free(g_px);
  g_px = NULL;
  g_gl = 0;
  g_open = 0;
  reset_plan();
}

void bw_play_audio(const float* interleaved, int nfloats) {
  bw_audio_inject(interleaved, nfloats);
}

int bw_play_open(const char* dir, int w, int h) {
  BwPlan plan;
  const char* assets;
  int i;
  bw_play_close();
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
  if (plan.video[0] && strcmp(g_kind, "video") == 0) {
    if (!vid_open(plan.video)) fprintf(stderr, "bendwall: missing video %s\n", plan.video);
  }
  if (!bw_gl_init(g_w, g_h)) {
    bw_plan_free(&plan);
    snprintf(g_line, sizeof g_line, "kind=fail shader=0");
    return 0;
  }
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
      g_nlive++;
    }
    prepare_text();
    bw_gl_end();
  } else if (bw_gl_begin()) {
    prepare_text();
    bw_gl_end();
  }
  bw_plan_free(&plan);
  spectrum_update();
  g_open = draw();
  refresh_line();
  return g_open;
}

int bw_play_step(int dt_ms) {
  int guard = 0;
  if (!g_open) return 0;
  if (dt_ms < 0) dt_ms = 0;
  g_time_ms += dt_ms;
  if (g_vid.open) {
    g_vid.acc += dt_ms;
    while (g_vid.acc >= g_vid.frame_ms && guard++ < 8) {
      g_vid.acc -= g_vid.frame_ms;
      vid_next(1);
    }
  }
  spectrum_update();
  if (!draw()) return 0;
  refresh_line();
  return 1;
}

int bw_play_rgba(uint8_t* dst, int cap) {
  int n;
  if (!dst || !g_px) return 0;
  n = g_w * g_h * 4;
  if (cap < n) return 0;
  memcpy(dst, g_px, (size_t)n);
  return n;
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

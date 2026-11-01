/* Scene image for the desktop. Bend still plans the headless 32x32 proof.
   This process keeps the video decoder and the GL context open, draws the
   picture and the text bitmap, then runs the built-in passes. */
#include "player.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <math.h>
#include <png.h>
#include <setjmp.h>

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
  "out vec4 fragColor;\n"
  "void main(){ fragColor = texture(g_Texture0, v); }\n";

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
static int g_tex, g_text_tex, g_white;
static int g_text_w, g_text_h;
static uint8_t* g_px;
static int g_time_ms;
static int g_uni;
static int g_scroll, g_spin, g_opacity, g_colorkey, g_tint, g_pulse;
static int g_has_text, g_has_video, g_has_image;
static char g_text[160];
static char g_page[1024];
static char g_kind[16];
static char g_line[512];
static float g_b16l[16], g_b16r[16], g_b32l[32], g_b32r[32], g_b64l[64], g_b64r[64];
static float g_web[128];

static uint32_t fbits(float f) {
  uint32_t u;
  memcpy(&u, &f, 4);
  return u;
}

static uint32_t ru32(const unsigned char* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int ends_with(const char* s, const char* ext) {
  size_t n, e;
  if (!s || !ext) return 0;
  n = strlen(s);
  e = strlen(ext);
  if (n < e) return 0;
  return strcasecmp(s + n - e, ext) == 0;
}

static void slash(char* s) {
  for (; s && *s; s++) if (*s == '\\') *s = '/';
}

static char* read_text(const char* path, int cap, int* n_out) {
  FILE* f;
  char* b;
  int n;
  f = fopen(path, "rb");
  if (!f) return NULL;
  b = (char*)malloc((size_t)cap + 1);
  if (!b) { fclose(f); return NULL; }
  n = (int)fread(b, 1, (size_t)cap, f);
  fclose(f);
  b[n] = 0;
  if (n_out) *n_out = n;
  return b;
}

static int pkg_extract(const char* path, const char* want, char** out) {
  FILE* f;
  unsigned char hdr[8];
  uint32_t magic_len, count, i;
  uint32_t found_off = 0, found_sz = 0;
  int found = 0;
  long start;
  char* buf;
  if (!path || !want || !out) return 0;
  *out = NULL;
  f = fopen(path, "rb");
  if (!f) return 0;
  if (fread(hdr, 1, 4, f) != 4) goto fail;
  magic_len = ru32(hdr);
  if (magic_len < 4 || magic_len > 8) goto fail;
  if (fread(hdr, 1, magic_len, f) != magic_len) goto fail;
  if (memcmp(hdr, "PKGV", 4) != 0) goto fail;
  if (fread(hdr, 1, 4, f) != 4) goto fail;
  count = ru32(hdr);
  if (count > 100000) goto fail;
  for (i = 0; i < count; i++) {
    uint32_t nl, off, sz;
    char* name;
    if (fread(hdr, 1, 4, f) != 4) goto fail;
    nl = ru32(hdr);
    if (nl > 1000000) goto fail;
    name = (char*)malloc((size_t)nl + 1);
    if (!name) goto fail;
    if (nl && fread(name, 1, nl, f) != nl) { free(name); goto fail; }
    name[nl] = 0;
    if (fread(hdr, 1, 8, f) != 8) { free(name); goto fail; }
    off = ru32(hdr);
    sz = ru32(hdr + 4);
    if (!found && nl > 0 && strcasecmp(name, want) == 0) {
      found = 1;
      found_off = off;
      found_sz = sz;
    }
    free(name);
  }
  start = ftell(f);
  if (!found || found_sz < 1 || found_sz > 2000000) goto fail;
  if (fseek(f, start + (long)found_off, SEEK_SET) != 0) goto fail;
  buf = (char*)malloc((size_t)found_sz + 1);
  if (!buf) goto fail;
  if (fread(buf, 1, found_sz, f) != found_sz) { free(buf); goto fail; }
  buf[found_sz] = 0;
  fclose(f);
  *out = buf;
  return 1;
fail:
  fclose(f);
  return 0;
}

static int key_string(const char* json, const char* key, char* out, int cap) {
  char pat[96];
  const char* p;
  int i;
  if (!json || !key || !out || cap < 2) return 0;
  snprintf(pat, sizeof pat, "\"%s\"", key);
  p = json;
  while ((p = strstr(p, pat)) != NULL) {
    p += strlen(pat);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != ':') continue;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '"') continue;
    p++;
    i = 0;
    while (*p && *p != '"' && i + 1 < cap) {
      if (*p == '\\' && p[1]) p++;
      out[i++] = *p++;
    }
    out[i] = 0;
    slash(out);
    return 1;
  }
  return 0;
}

static int quoted_ext(const char* json, const char* ext, char* out, int cap) {
  const char* p = json;
  if (!json) return 0;
  while ((p = strchr(p, '"')) != NULL) {
    const char* e;
    int n;
    p++;
    e = strchr(p, '"');
    if (!e) return 0;
    n = (int)(e - p);
    if (n > 1 && n < cap) {
      char tmp[1024];
      if (n < (int)sizeof tmp) {
        memcpy(tmp, p, (size_t)n);
        tmp[n] = 0;
        if (ends_with(tmp, ext)) {
          memcpy(out, tmp, (size_t)n + 1);
          slash(out);
          return 1;
        }
      }
    }
    p = e + 1;
  }
  return 0;
}

static void note_effect(const char* name) {
  if (!strcmp(name, "scroll")) g_scroll = 1;
  else if (!strcmp(name, "spin")) g_spin = 1;
  else if (!strcmp(name, "opacity")) g_opacity = 1;
  else if (!strcmp(name, "colorkey")) g_colorkey = 1;
  else if (!strcmp(name, "tint")) g_tint = 1;
  else if (!strcmp(name, "pulse")) g_pulse = 1;
}

static void scan_effects(const char* json) {
  const char* p = json;
  if (!json) return;
  while ((p = strstr(p, "effects/")) != NULL) {
    char name[64];
    int i = 0;
    p += 8;
    while (*p && *p != '/' && *p != '"' && *p != ' ' && i < 63) name[i++] = (char)tolower((unsigned char)*p++);
    name[i] = 0;
    note_effect(name);
  }
}

static int load_scene(const char* dir, const char* file_field, char** out) {
  char path[1100];
  const char* names[4];
  int n = 0, i;
  if (file_field && strstr(file_field, ".json")) names[n++] = file_field;
  names[n++] = "scene.json";
  names[n++] = "gifscene.json";
  for (i = 0; i < n; i++) {
    snprintf(path, sizeof path, "%s/%s", dir, names[i]);
    *out = read_text(path, 2000000, NULL);
    if (*out) return 1;
  }
  snprintf(path, sizeof path, "%s/scene.pkg", dir);
  if (pkg_extract(path, "scene.json", out)) return 1;
  snprintf(path, sizeof path, "%s/gifscene.pkg", dir);
  if (pkg_extract(path, "gifscene.json", out)) return 1;
  if (file_field && file_field[0]) {
    snprintf(path, sizeof path, "%s/scene.pkg", dir);
    if (pkg_extract(path, file_field, out)) return 1;
  }
  return 0;
}

struct jerr_s {
  struct jpeg_error_mgr pub;
  jmp_buf jb;
};

static void jerr_exit(j_common_ptr c) {
  struct jerr_s* e = (struct jerr_s*)c->err;
  longjmp(e->jb, 1);
}

static int load_jpeg(const char* path, int* w, int* h, uint8_t** rgba) {
  FILE* fp;
  struct jpeg_decompress_struct cinfo;
  struct jerr_s jerr;
  uint8_t* out = NULL;
  int y, x, comps;
  fp = fopen(path, "rb");
  if (!fp) return 0;
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jerr_exit;
  if (setjmp(jerr.jb)) {
    jpeg_destroy_decompress(&cinfo);
    fclose(fp);
    free(out);
    return 0;
  }
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, fp);
  jpeg_read_header(&cinfo, TRUE);
  cinfo.out_color_space = JCS_RGB;
  jpeg_start_decompress(&cinfo);
  *w = (int)cinfo.output_width;
  *h = (int)cinfo.output_height;
  comps = (int)cinfo.output_components;
  if (*w < 1 || *h < 1 || *w > 8192 || *h > 8192 || comps < 3) {
    jpeg_destroy_decompress(&cinfo);
    fclose(fp);
    return 0;
  }
  out = (uint8_t*)malloc((size_t)(*w) * (size_t)(*h) * 4);
  if (!out) { jpeg_destroy_decompress(&cinfo); fclose(fp); return 0; }
  while ((int)cinfo.output_scanline < *h) {
    uint8_t row[8192 * 3];
    uint8_t* rp = row;
    JSAMPROW rows[1];
    int yy = (int)cinfo.output_scanline;
    rows[0] = row;
    if (*w > 8192) break;
    jpeg_read_scanlines(&cinfo, rows, 1);
    for (x = 0; x < *w; x++) {
      uint8_t* d = out + ((size_t)yy * (size_t)(*w) + (size_t)x) * 4;
      d[0] = rp[x * comps];
      d[1] = rp[x * comps + 1];
      d[2] = rp[x * comps + 2];
      d[3] = 255;
    }
    (void)y;
  }
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  fclose(fp);
  *rgba = out;
  return 1;
}

static int load_png(const char* path, int* w, int* h, uint8_t** rgba) {
  png_image image;
  memset(&image, 0, sizeof image);
  image.version = PNG_IMAGE_VERSION;
  if (!png_image_begin_read_from_file(&image, path)) return 0;
  image.format = PNG_FORMAT_RGBA;
  *w = (int)image.width;
  *h = (int)image.height;
  if (*w < 1 || *h < 1 || *w > 8192 || *h > 8192) { png_image_free(&image); return 0; }
  *rgba = (uint8_t*)malloc((size_t)(*w) * (size_t)(*h) * 4);
  if (!*rgba || !png_image_finish_read(&image, NULL, *rgba, 0, NULL)) {
    free(*rgba);
    *rgba = NULL;
    png_image_free(&image);
    return 0;
  }
  png_image_free(&image);
  return 1;
}

static int load_image(const char* path, int* w, int* h, uint8_t** rgba) {
  if (ends_with(path, ".png")) return load_png(path, w, h, rgba);
  if (ends_with(path, ".jpg") || ends_with(path, ".jpeg")) return load_jpeg(path, w, h, rgba);
  return 0;
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

static int draw_quad(int prog, int tex, int x, int y, int w, int h) {
  float x0, y0, x1, y1;
  if (w < 1 || h < 1 || tex <= 0) return 0;
  x0 = (float)x / (float)g_w * 2.f - 1.f;
  x1 = (float)(x + w) / (float)g_w * 2.f - 1.f;
  y0 = 1.f - (float)y / (float)g_h * 2.f;
  y1 = 1.f - (float)(y + h) / (float)g_h * 2.f;
  return bw_gl_quad(prog, tex, fbits(x0), fbits(y0), fbits(x1), fbits(y1),
                    fbits(0.f), fbits(0.f), fbits(1.f), fbits(1.f));
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

static int make_solid(int r, int g, int b) {
  uint8_t px[4];
  px[0] = (uint8_t)r;
  px[1] = (uint8_t)g;
  px[2] = (uint8_t)b;
  px[3] = 255;
  g_tex = bw_gl_image(g_tex, 1, 1, px, 4);
  return g_tex > 0;
}

static int make_checker(void) {
  enum { N = 32 };
  uint8_t* px;
  int x, y;
  px = (uint8_t*)malloc(N * N * 4);
  if (!px) return 0;
  for (y = 0; y < N; y++) {
    for (x = 0; x < N; x++) {
      int on = ((x / 8) ^ (y / 8)) & 1;
      uint8_t* p = px + (y * N + x) * 4;
      p[0] = on ? 20 : 220;
      p[1] = on ? 180 : 40;
      p[2] = 36;
      p[3] = 255;
    }
  }
  g_tex = bw_gl_image(g_tex, N, N, px, N * N * 4);
  free(px);
  return g_tex > 0;
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
  if (g_tex <= 0) {
    if (g_scroll || g_spin || strcmp(g_kind, "demo") == 0) make_checker();
    else make_solid(220, 48, 32);
  }
  if (!bw_gl_bind(1) || !bw_gl_clear()) { bw_gl_end(); return 0; }
  draw_quad(g_copy_prog, g_tex, 0, 0, g_w, g_h);
  if (!bw_gl_bind(0) || !bw_gl_clear()) { bw_gl_end(); return 0; }
  bw_gl_f1(g_fx_prog, "g_Time", fbits((float)g_time_ms / 1000.f));
  bands[0] = g_b16l; bands[1] = g_b16r; bands[2] = g_b32l;
  bands[3] = g_b32r; bands[4] = g_b64l; bands[5] = g_b64r;
  g_uni = 0;
  for (i = 0; i < 6; i++) if (upload_arr(g_fx_prog, names[i], bands[i], counts[i])) g_uni++;
  draw_quad(g_fx_prog, bw_gl_tex(1), 0, 0, g_w, g_h);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  if (g_has_text && g_text_tex > 0) {
    scale = g_h / 90;
    if (scale < 1) scale = 1;
    if (scale > 8) scale = 8;
    draw_quad(g_copy_prog, g_text_tex, 8, 8, g_text_w * scale, g_text_h * scale);
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
      if (lh > 0) draw_quad(g_copy_prog, g_white, i * band_w, g_h - lh, band_w, lh);
      if (rh > 0) draw_quad(g_copy_prog, g_white, g_w / 2 + i * band_w, g_h - rh, band_w, rh);
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
    "kind=%s video=%d text=%d image=%d scroll=%d spin=%d opacity=%d colorkey=%d tint=%d pulse=%d uni=%d step=%d b16=%.3f b32=%.3f b64=%.3f r16=%.3f shader=%d",
    g_kind, g_has_video, g_has_text, g_has_image, g_scroll, g_spin, g_opacity, g_colorkey,
    g_tint, g_pulse, g_uni, g_vid.frame_ms, g_b16l[0], g_b32l[0], g_b64l[0], g_b16r[0],
    g_fx_prog > 0 ? 1 : 0);
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
  g_has_text = g_has_video = g_has_image = 0;
  g_text[0] = 0;
  g_page[0] = 0;
  snprintf(g_kind, sizeof g_kind, "demo");
  g_time_ms = 0;
  g_tex = 0;
  g_text_tex = 0;
  g_white = 0;
  g_copy_prog = 0;
  g_fx_prog = 0;
}

void bw_play_close(void) {
  if (g_gl && bw_gl_begin()) {
    if (g_tex > 0) { GLuint t = (GLuint)g_tex; glDeleteTextures(1, &t); }
    if (g_text_tex > 0) { GLuint t = (GLuint)g_text_tex; glDeleteTextures(1, &t); }
    if (g_white > 0) { GLuint t = (GLuint)g_white; glDeleteTextures(1, &t); }
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
  char project[1100], type[32], file[512], title[128], image[512], video[1100];
  char* scene = NULL;
  char* proj = NULL;
  int have_dir = 0;
  bw_play_close();
  if (w < 8) w = 8;
  if (h < 8) h = 8;
  if (w > 1920) w = 1920;
  if (h > 1080) h = 1080;
  g_w = w;
  g_h = h;
  reset_plan();
  type[0] = file[0] = title[0] = image[0] = video[0] = 0;
  if (dir && dir[0]) {
    snprintf(project, sizeof project, "%s/project.json", dir);
    proj = read_text(project, 1000000, NULL);
    have_dir = proj != NULL;
  }
  if (proj) {
    key_string(proj, "type", type, sizeof type);
    key_string(proj, "file", file, sizeof file);
    key_string(proj, "title", title, sizeof title);
    for (char* p = type; *p; p++) *p = (char)tolower((unsigned char)*p);
  }
  if (have_dir && load_scene(dir, file, &scene)) {
    char value[128];
    scan_effects(scene);
    if (key_string(strstr(scene, "\"text\"") ? strstr(scene, "\"text\"") : scene, "value", value, sizeof value)) {
      if (strstr(scene, "\"text\"")) snprintf(g_text, sizeof g_text, "%s", value);
    }
    if (!quoted_ext(scene, ".png", image, sizeof image)) {
      if (!quoted_ext(scene, ".jpg", image, sizeof image))
        quoted_ext(scene, ".jpeg", image, sizeof image);
    }
    if (!quoted_ext(scene, ".mp4", video, sizeof video) &&
        !quoted_ext(scene, ".webm", video, sizeof video) &&
        !quoted_ext(scene, ".mkv", video, sizeof video) &&
        !quoted_ext(scene, ".avi", video, sizeof video) &&
        !quoted_ext(scene, ".mov", video, sizeof video))
      video[0] = 0;
  }
  if (!strcmp(type, "video") && file[0]) snprintf(video, sizeof video, "%s", file);
  if (!strcmp(type, "web") && file[0]) snprintf(g_page, sizeof g_page, "%s/%s", dir, file);
  if (!g_text[0] && title[0]) snprintf(g_text, sizeof g_text, "%s", title);
  if (!have_dir) {
    snprintf(g_kind, sizeof g_kind, "demo");
    snprintf(g_text, sizeof g_text, "Bendwall");
    g_scroll = 1;
  } else if (!strcmp(type, "web")) {
    snprintf(g_kind, sizeof g_kind, "web");
  } else if (!strcmp(type, "video") || video[0]) {
    snprintf(g_kind, sizeof g_kind, "video");
  } else {
    snprintf(g_kind, sizeof g_kind, "scene");
  }
  if (!bw_gl_init(g_w, g_h)) {
    free(scene);
    free(proj);
    snprintf(g_line, sizeof g_line, "kind=fail shader=0");
    return 0;
  }
  g_gl = 1;
  bw_audio_live_start();
  if (video[0] && dir) {
    char path[1100];
    if (video[0] == '/') snprintf(path, sizeof path, "%s", video);
    else snprintf(path, sizeof path, "%s/%s", dir, video);
    vid_open(path);
  }
  if (image[0] && dir && !g_has_video) {
    char path[1100];
    uint8_t* px = NULL;
    int iw = 0, ih = 0;
    if (image[0] == '/') snprintf(path, sizeof path, "%s", image);
    else snprintf(path, sizeof path, "%s/%s", dir, image);
    if (load_image(path, &iw, &ih, &px) && px) {
      if (!bw_gl_begin()) { free(px); }
      else {
        g_tex = bw_gl_image(0, iw, ih, px, iw * ih * 4);
        g_has_image = g_tex > 0;
        bw_gl_end();
        free(px);
      }
    }
  }
  if (!bw_gl_begin()) { free(scene); free(proj); return 0; }
  prepare_text();
  bw_gl_end();
  spectrum_update();
  g_open = draw();
  refresh_line();
  free(scene);
  free(proj);
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

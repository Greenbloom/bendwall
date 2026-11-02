/* Headless scene-image check. Two video frames differ, the text bitmap
   lands in the picture, tint and pulse change pixels, and the spectrum
   uniforms follow an injected buffer. */
#include "plan.h"
#include "player.h"

#define GL_GLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <GL/gl.h>
#include <locale.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int center(int* r, int* g, int* b, int* a) {
  int w = 0, h = 0, n;
  uint8_t* px;
  if (!bw_play_size(&w, &h) || w < 2 || h < 2) return 0;
  px = (uint8_t*)malloc((size_t)w * (size_t)h * 4);
  if (!px) return 0;
  n = bw_play_rgba(px, w * h * 4);
  if (n < w * h * 4) { free(px); return 0; }
  {
    int x = w / 2, y = h / 2;
    uint8_t* p = px + ((size_t)y * (size_t)w + (size_t)x) * 4;
    *r = p[0];
    *g = p[1];
    *b = p[2];
    *a = p[3];
  }
  free(px);
  return 1;
}

static int white_count(void) {
  int w = 0, h = 0, n, i, c = 0;
  uint8_t* px;
  if (!bw_play_size(&w, &h)) return 0;
  px = (uint8_t*)malloc((size_t)w * (size_t)h * 4);
  if (!px) return 0;
  n = bw_play_rgba(px, w * h * 4);
  if (n < w * h * 4) { free(px); return 0; }
  for (i = 0; i < w * h; i++) {
    uint8_t* p = px + (size_t)i * 4;
    if (p[0] > 250 && p[1] > 250 && p[2] > 250) c++;
  }
  free(px);
  return c;
}

static int off_white(int cy, int band) {
  int w = 0, h = 0, n, x, y, c = 0;
  uint8_t* px;
  if (!bw_play_size(&w, &h)) return 0;
  px = (uint8_t*)malloc((size_t)w * (size_t)h * 4);
  if (!px) return 0;
  n = bw_play_rgba(px, w * h * 4);
  if (n < w * h * 4) { free(px); return 0; }
  for (y = 0; y < h; y++) {
    if (abs(y - cy) <= band) continue;
    for (x = 0; x < w; x++) {
      uint8_t* p = px + ((size_t)y * (size_t)w + (size_t)x) * 4;
      if (p[0] > 200 && p[1] > 200 && p[2] > 200) c++;
    }
  }
  free(px);
  return c;
}

static int white_out(int cx, int cy, int rad) {
  int w = 0, h = 0, n, x, y, c = 0;
  uint8_t* px;
  if (!bw_play_size(&w, &h)) return 0;
  px = (uint8_t*)malloc((size_t)w * (size_t)h * 4);
  if (!px) return 0;
  n = bw_play_rgba(px, w * h * 4);
  if (n < w * h * 4) { free(px); return 0; }
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      uint8_t* p;
      if (abs(x - cx) <= rad && abs(y - cy) <= rad) continue;
      p = px + ((size_t)y * (size_t)w + (size_t)x) * 4;
      if (p[0] > 200 && p[1] > 200 && p[2] > 200) c++;
    }
  }
  free(px);
  return c;
}

static int white_x(int xmin) {
  int w = 0, h = 0, n, x, y, c = 0;
  uint8_t* px;
  if (!bw_play_size(&w, &h)) return 0;
  px = (uint8_t*)malloc((size_t)w * (size_t)h * 4);
  if (!px) return 0;
  n = bw_play_rgba(px, w * h * 4);
  if (n < w * h * 4) { free(px); return 0; }
  for (y = 0; y < h; y++) {
    for (x = xmin; x < w; x++) {
      uint8_t* p = px + ((size_t)y * (size_t)w + (size_t)x) * 4;
      if (p[0] > 250 && p[1] > 250 && p[2] > 250) c++;
    }
  }
  free(px);
  return c;
}

static int blit_sample(int* r, int* g, int* b) {
  EGLDisplay dpy;
  EGLConfig cfg;
  EGLSurface surf;
  EGLContext ctx;
  EGLint n = 0;
  EGLint attr[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
    EGL_NONE
  };
  EGLint pbuf[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE};
  EGLint ctxattr[] = {
    EGL_CONTEXT_MAJOR_VERSION, 3,
    EGL_CONTEXT_MINOR_VERSION, 3,
    EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
    EGL_NONE
  };
  uint8_t px[4];
  dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, NULL, NULL)) return 0;
  if (!eglChooseConfig(dpy, attr, &cfg, 1, &n) || n < 1) return 0;
  if (!eglBindAPI(EGL_OPENGL_API)) return 0;
  surf = eglCreatePbufferSurface(dpy, cfg, pbuf);
  ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxattr);
  if (ctx == EGL_NO_CONTEXT) ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, NULL);
  if (surf == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT) return 0;
  if (!eglMakeCurrent(dpy, surf, surf, ctx)) return 0;
  glViewport(0, 0, 64, 64);
  if (!bw_play_blit()) {
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    return 0;
  }
  glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
  *r = px[0];
  *g = px[1];
  *b = px[2];
  eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(dpy, ctx);
  eglDestroySurface(dpy, surf);
  return 1;
}

void* bw_gl_color_image(void);

struct Flick {
  EGLDisplay dpy;
  EGLSurface surf;
  EGLContext ctx;
  atomic_int* stop;
  atomic_int* ok;
  atomic_int* black;
};

static int peer_pbuffer(EGLDisplay dpy, EGLSurface* surf, EGLContext* ctx) {
  EGLConfig cfg;
  EGLint n = 0;
  EGLint attr[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
    EGL_NONE
  };
  EGLint pbuf[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
  EGLint ctxattr[] = {
    EGL_CONTEXT_MAJOR_VERSION, 3,
    EGL_CONTEXT_MINOR_VERSION, 3,
    EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
    EGL_NONE
  };
  if (!eglBindAPI(EGL_OPENGL_API)) return 0;
  if (!eglChooseConfig(dpy, attr, &cfg, 1, &n) || n < 1) return 0;
  *surf = eglCreatePbufferSurface(dpy, cfg, pbuf);
  *ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxattr);
  if (*ctx == EGL_NO_CONTEXT) *ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, NULL);
  return *surf != EGL_NO_SURFACE && *ctx != EGL_NO_CONTEXT;
}

static void* flicker_thread(void* arg) {
  struct Flick* f = (struct Flick*)arg;
  uint8_t px[4];
  if (!eglMakeCurrent(f->dpy, f->surf, f->surf, f->ctx)) return NULL;
  glViewport(0, 0, 32, 32);
  while (!atomic_load(f->stop)) {
    if (!bw_play_blit()) {
      usleep(1000);
      continue;
    }
    glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    if ((int)px[0] + (int)px[1] + (int)px[2] < 8) atomic_fetch_add(f->black, 1);
    else atomic_fetch_add(f->ok, 1);
    usleep(1000);
  }
  eglMakeCurrent(f->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  return NULL;
}

static int color_count(int want_r, int want_g) {
  int w = 0, h = 0, n, i, c = 0;
  uint8_t* px;
  if (!bw_play_size(&w, &h)) return 0;
  px = (uint8_t*)malloc((size_t)w * (size_t)h * 4);
  if (!px) return 0;
  n = bw_play_rgba(px, w * h * 4);
  if (n < w * h * 4) { free(px); return 0; }
  for (i = 0; i < w * h; i++) {
    uint8_t* p = px + (size_t)i * 4;
    if (want_r && p[0] > 200 && p[1] < 40 && p[2] < 40) c++;
    if (want_g && p[1] > 200 && p[0] < 40 && p[2] < 40) c++;
  }
  free(px);
  return c;
}

static int at(int x, int y, int* r, int* g, int* b) {
  int w = 0, h = 0, n, a;
  uint8_t* px;
  if (!bw_play_size(&w, &h) || x < 0 || y < 0 || x >= w || y >= h) return 0;
  px = (uint8_t*)malloc((size_t)w * (size_t)h * 4);
  if (!px) return 0;
  n = bw_play_rgba(px, w * h * 4);
  if (n < w * h * 4) { free(px); return 0; }
  {
    uint8_t* p = px + ((size_t)y * (size_t)w + (size_t)x) * 4;
    *r = p[0];
    *g = p[1];
    *b = p[2];
    a = p[3];
    (void)a;
  }
  free(px);
  return 1;
}

static int varied(void) {
  int w = 0, h = 0, n, i;
  uint8_t* px;
  if (!bw_play_size(&w, &h)) return 0;
  px = (uint8_t*)malloc((size_t)w * (size_t)h * 4);
  if (!px) return 0;
  n = bw_play_rgba(px, w * h * 4);
  if (n < w * h * 4) { free(px); return 0; }
  for (i = 4; i < n; i += 4) {
    if (px[i] != px[0] || px[i + 1] != px[1] || px[i + 2] != px[2]) {
      free(px);
      return 1;
    }
  }
  free(px);
  return 0;
}

static int decode_tex_at(const char* path, int time_ms, int* r, int* g, int* b) {
  FILE* f;
  long sz;
  uint8_t* buf;
  uint8_t* rgba = NULL;
  int w = 0, h = 0, n;
  f = fopen(path, "rb");
  if (!f) return 0;
  if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
  sz = ftell(f);
  if (sz < 16 || sz > 8000000) { fclose(f); return 0; }
  if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }
  buf = (uint8_t*)malloc((size_t)sz);
  if (!buf) { fclose(f); return 0; }
  n = (int)fread(buf, 1, (size_t)sz, f);
  fclose(f);
  if (n != (int)sz || !bw_tex_decode_time(buf, n, time_ms, &rgba, &w, &h) || !rgba || w < 1 || h < 1) {
    free(buf);
    free(rgba);
    return 0;
  }
  *r = rgba[0];
  *g = rgba[1];
  *b = rgba[2];
  free(rgba);
  free(buf);
  return 1;
}

static int decode_tex(const char* path, int* r, int* g, int* b) {
  return decode_tex_at(path, 0, r, g, b);
}

static int step_one_frame(void) {
  const char* line = bw_play_line();
  const char* s = line ? strstr(line, "step=") : NULL;
  int ms = s ? atoi(s + 5) : 1000;
  if (ms < 1) ms = 1000;
  return bw_play_step(ms);
}

static void constant_audio(void) {
  float s[256];
  int i;
  for (i = 0; i < 128; i++) {
    s[i * 2] = 1.f;
    s[i * 2 + 1] = 0.f;
  }
  bw_play_audio(s, 256);
}

static void pcm_stats(const float* s, int n, float* peak, float* power) {
  int i, frames;
  double pk = 0, k, w, coeff, s0 = 0, s1 = 0, s2 = 0;
  frames = n / 2;
  if (frames < 8) {
    *peak = 0;
    *power = 0;
    return;
  }
  for (i = 0; i < n; i++) {
    double a = s[i] < 0 ? -(double)s[i] : (double)s[i];
    if (a > pk) pk = a;
  }
  k = 440.0 * (double)frames / 44100.0;
  w = 6.283185307179586 * k / (double)frames;
  coeff = 2.0 * cos(w);
  for (i = 0; i < frames; i++) {
    s0 = (double)s[i * 2] + coeff * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  *peak = (float)pk;
  *power = (float)(s1 * s1 + s2 * s2 - coeff * s1 * s2);
  if (*power < 0.f) *power = 0.f;
}

static int pull_pcm(float* dst, int cap, int ms) {
  /* Always wait the full interval. A short peek still holds the previous
     gain, and returning as soon as it is non-empty would report that. */
  usleep((useconds_t)ms * 1000);
  return bw_play_video_pcm(dst, cap);
}

static void tone_audio(void) {
  float s[256];
  int i;
  const double pi2 = 6.283185307179586;
  for (i = 0; i < 128; i++) {
    s[i * 2] = (float)cos(pi2 * 8.0 * (double)i / 128.0);
    s[i * 2 + 1] = 0.f;
  }
  bw_play_audio(s, 256);
}

int main(int argc, char** argv) {
  const char* root = argc > 1 ? argv[1] : "fixtures/play";
  char path[1200];
  setlocale(LC_NUMERIC, "C");
  setlocale(LC_ALL, "C");
  int r0 = 0, g0 = 0, b0 = 0, a0 = 0, r1 = 0, g1 = 0, b1 = 0, a1 = 0;
  int pr0 = 0, pg = 0, pb = 0, pa = 0, pr1 = 0, p1g = 0, p1b = 0, p1a = 0;
  int tr = 0, tg = 0, tb = 0, ta = 0;
  float b16 = 0, b32 = 0, b64 = 0, r16 = 0, tone64 = 0, tone16 = 0;
  int diff, white, bad = 0;
  {
    float v0 = bw_play_video_volume();
    float v1, v2;
    bw_play_set_video_volume(2.f);
    v1 = bw_play_video_volume();
    bw_play_set_video_volume(-1.f);
    v2 = bw_play_video_volume();
    printf("VOL %.2f %.2f %.2f\n", v0, v1, v2);
    if (v0 != 1.f || v1 != 1.f || v2 != 0.f) bad = 1;
    bw_play_set_video_volume(1.f);
  }
  snprintf(path, sizeof path, "%s/video", root);
  constant_audio();
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open video failed\n%s\n", bw_play_line());
    return 1;
  }
  bw_play_step(0);
  if (!center(&r0, &g0, &b0, &a0)) return 1;
  printf("VIDEO0 %d %d %d %d\n", r0, g0, b0, a0);
  printf("LINE %s\n", bw_play_line());
  if (!step_one_frame() || !center(&r1, &g1, &b1, &a1)) return 1;
  printf("VIDEO1 %d %d %d %d\n", r1, g1, b1, a1);
  diff = (r0 != r1) || (g0 != g1) || (b0 != b1);
  printf("DIFF %d\n", diff ? 1 : 0);
  bw_play_band(0, 0, &b16);
  bw_play_band(2, 0, &b32);
  bw_play_band(4, 0, &b64);
  bw_play_band(1, 0, &r16);
  printf("SPECTRUM %.3f %.3f %.3f %.3f\n", b16, b32, b64, r16);
  {
    float web[128];
    bw_play_audio_out(web);
    printf("WEB %.3f %.3f\n", web[0], web[64]);
  }
  tone_audio();
  bw_play_step(0);
  bw_play_band(4, 8, &tone64);
  bw_play_band(0, 2, &tone16);
  printf("TONE %.3f %.3f\n", tone64, tone16);
  printf("UNI %s\n", strstr(bw_play_line(), "uni=") ? strstr(bw_play_line(), "uni=") : "uni=0");
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/label", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open label failed\n");
    return 1;
  }
  white = white_count();
  printf("LABEL white=%d\n", white);
  printf("LINE %s\n", bw_play_line());
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/pulse", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open pulse failed\n");
    return 1;
  }
  center(&pr0, &pg, &pb, &pa);
  bw_play_step(250);
  center(&pr1, &p1g, &p1b, &p1a);
  printf("PULSE %d %d\n", pr0, pr1);
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/tint", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open tint failed\n");
    return 1;
  }
  center(&tr, &tg, &tb, &ta);
  printf("TINT %d %d %d\n", tr, tg, tb);
  bw_play_close();

  snprintf(path, sizeof path, "%s/../workshop/23", root);
  /* fixtures/play/../workshop/23 when root is fixtures/play */
  if (!bw_play_open(path, 64, 64)) {
    fprintf(stderr, "open packed failed\n");
    return 1;
  }
  printf("PACK %s\n", bw_play_line());
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/nested", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open nested failed\n%s\n", bw_play_line());
    return 1;
  }
  {
    int tr = 0, tg = 0, tb = 0, br = 0, bg = 0, bb = 0;
    int tr2 = 0, tg2 = 0, tb2 = 0, br2 = 0, bg2 = 0, bb2 = 0;
    int w = 0, h = 0;
    const char* kind = bw_play_kind();
    bw_play_size(&w, &h);
    if (!at(w - 8, 4, &tr, &tg, &tb) || !at(w - 8, h - 4, &br, &bg, &bb)) return 1;
    printf("ORIENT kind=%s top=%d %d %d bot=%d %d %d\n", kind ? kind : "", tr, tg, tb, br, bg, bb);
    if (!step_one_frame()) return 1;
    if (!at(w - 8, 4, &tr2, &tg2, &tb2) || !at(w - 8, h - 4, &br2, &bg2, &bb2)) return 1;
    printf("ORIENT2 top=%d %d %d bot=%d %d %d\n", tr2, tg2, tb2, br2, bg2, bb2);
    if (!kind || strcmp(kind, "video") != 0) bad = 1;
    if (tr < 180 || tg > 80 || br > 80 || bg < 180) bad = 1;
    if (tr2 > 80 || tg2 < 180 || br2 < 180 || bg2 > 80) bad = 1;
  }
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/texscene", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open texscene failed\n%s\n", bw_play_line());
    return 1;
  }
  {
    int cr = 0, cg = 0, cb = 0, mr = 0, mg = 0, mb = 0;
    int fr = 0, fg = 0, fb = 0, lr = 0, lg = 0, lb = 0;
    if (!at(10, 10, &cr, &cg, &cb) || !at(60, 10, &mr, &mg, &mb)) return 1;
    if (!at(120, 10, &fr, &fg, &fb) || !at(10, 70, &lr, &lg, &lb)) return 1;
    printf("TEX c=%d %d %d m=%d %d %d far=%d %d %d low=%d %d %d\n",
           cr, cg, cb, mr, mg, mb, fr, fg, fb, lr, lg, lb);
    printf("LINE %s\n", bw_play_line());
    if (cr > 40 || cg < 200 || cb < 200) bad = 1;
    if (mr < 200 || mg > 40 || mb < 200) bad = 1;
    if (fr > 20 || fg > 20 || fb > 20 || lr > 20 || lg > 20 || lb > 20) bad = 1;
    if (!varied()) bad = 1;
  }
  bw_play_close();

  {
    int dr = 0, dg = 0, db = 0, pr = 0, pg = 0, pb = 0;
    snprintf(path, sizeof path, "%s/dxt.tex", root);
    if (!decode_tex(path, &dr, &dg, &db)) {
      fprintf(stderr, "dxt decode failed\n");
      return 1;
    }
    snprintf(path, sizeof path, "%s/png.tex", root);
    if (!decode_tex(path, &pr, &pg, &pb)) {
      fprintf(stderr, "png tex decode failed\n");
      return 1;
    }
    printf("DXT %d %d %d\n", dr, dg, db);
    printf("PNG %d %d %d\n", pr, pg, pb);
    if (dr > 40 || dg < 200 || db > 40) bad = 1;
    if (pr < 200 || pg > 40 || pb > 40) bad = 1;
  }

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/missing", root);
  if (!bw_play_open(path, 64, 64)) {
    fprintf(stderr, "open missing failed\n%s\n", bw_play_line());
    return 1;
  }
  {
    const char* line = bw_play_line();
    const char* miss = line ? strstr(line, "miss=") : NULL;
    int nmiss = miss ? atoi(miss + 5) : 0;
    int var = varied();
    printf("MISS miss=%d varied=%d\n", nmiss, var);
    if (nmiss < 1 || !var) bad = 1;
  }
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/named", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open named failed\n%s\n", bw_play_line());
    return 1;
  }
  {
    int cr = 0, cg = 0, cb = 0, mr = 0, mg = 0, mb = 0;
    if (!at(20, 45, &cr, &cg, &cb) || !at(140, 45, &mr, &mg, &mb)) return 1;
    printf("NAME c=%d %d %d m=%d %d %d\n", cr, cg, cb, mr, mg, mb);
    if (cr > 40 || cg < 200 || cb < 200 || mr < 200 || mg > 40 || mb < 200) bad = 1;
  }
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/pkg9big/lz", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open biglz failed\n%s\n", bw_play_line());
    return 1;
  }
  {
    int cr = 0, cg = 0, cb = 0, mr = 0, mg = 0, mb = 0;
    if (!at(20, 45, &cr, &cg, &cb) || !at(140, 45, &mr, &mg, &mb)) return 1;
    printf("BIGLZ c=%d %d %d m=%d %d %d\n", cr, cg, cb, mr, mg, mb);
    if (cr > 40 || cg < 200 || cb < 200 || mr < 200 || mg > 40 || mb < 200) bad = 1;
  }
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/pkg9big/dxt", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open bigdxt failed\n%s\n", bw_play_line());
    return 1;
  }
  {
    int dr = 0, dg = 0, db = 0;
    if (!at(80, 45, &dr, &dg, &db)) return 1;
    printf("BIGDXT %d %d %d\n", dr, dg, db);
    if (dr > 40 || dg < 200 || db > 40) bad = 1;
  }
  bw_play_close();

  bw_play_audio(NULL, 0);
  {
    const char* old_assets = getenv("BENDWALL_ASSETS");
    char saved[1100];
    saved[0] = 0;
    if (old_assets) snprintf(saved, sizeof saved, "%s", old_assets);
    snprintf(path, sizeof path, "%s/assetroot", root);
    setenv("BENDWALL_ASSETS", path, 1);
    snprintf(path, sizeof path, "%s/fromassets", root);
    if (!bw_play_open(path, 160, 90)) {
      fprintf(stderr, "open assets failed\n%s\n", bw_play_line());
      return 1;
    }
    {
      int ar = 0, ag = 0, ab = 0;
      if (!at(80, 45, &ar, &ag, &ab)) return 1;
      printf("ASSET %d %d %d\n", ar, ag, ab);
      if (ab < 200 || ar > 40 || ag > 80) bad = 1;
    }
    bw_play_close();
    if (saved[0]) setenv("BENDWALL_ASSETS", saved, 1);
    else unsetenv("BENDWALL_ASSETS");
  }

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/badtex", root);
  if (!bw_play_open(path, 64, 64)) {
    fprintf(stderr, "open badtex failed\n%s\n", bw_play_line());
    return 1;
  }
  printf("BAD %s\n", bw_play_line());
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/objimg", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open objimg failed\n%s\n", bw_play_line());
    return 1;
  }
  {
    int cr = 0, cg = 0, cb = 0, mr = 0, mg = 0, mb = 0;
    if (!at(10, 10, &cr, &cg, &cb) || !at(60, 10, &mr, &mg, &mb)) return 1;
    printf("OBJ c=%d %d %d m=%d %d %d\n", cr, cg, cb, mr, mg, mb);
    if (cr > 40 || cg < 200 || cb < 200 || mr < 200 || mg > 40 || mb < 200) bad = 1;
  }
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/matref", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open matref failed\n%s\n", bw_play_line());
    return 1;
  }
  {
    int cr = 0, cg = 0, cb = 0, mr = 0, mg = 0, mb = 0;
    if (!at(10, 10, &cr, &cg, &cb) || !at(60, 10, &mr, &mg, &mb)) return 1;
    printf("MAT c=%d %d %d m=%d %d %d\n", cr, cg, cb, mr, mg, mb);
    if (cr > 40 || cg < 200 || cb < 200 || mr < 200 || mg > 40 || mb < 200) bad = 1;
  }
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/objmiss", root);
  if (!bw_play_open(path, 64, 64)) {
    fprintf(stderr, "open objmiss failed\n%s\n", bw_play_line());
    return 1;
  }
  printf("OBJMISS %s\n", bw_play_line());
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/mixed", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open mixed failed\n%s\n", bw_play_line());
    return 1;
  }
  {
    int cr = 0, cg = 0, cb = 0;
    const char* line = bw_play_line();
    const char* miss = line ? strstr(line, "miss=") : NULL;
    int nmiss = miss ? atoi(miss + 5) : 0;
    if (!at(20, 45, &cr, &cg, &cb)) return 1;
    printf("MIX c=%d %d %d miss=%d\n", cr, cg, cb, nmiss);
    if (cr > 40 || cg < 200 || cb < 200 || nmiss < 1) bad = 1;
  }
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/gifscene", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open gifscene failed\n%s\n", bw_play_line());
    return 1;
  }
  {
    int r0 = 0, g0 = 0, b0 = 0, r1 = 0, g1 = 0, b1 = 0;
    if (!at(80, 45, &r0, &g0, &b0)) return 1;
    if (!bw_play_step(500) || !at(80, 45, &r1, &g1, &b1)) return 1;
    printf("GIF0 %d %d %d\n", r0, g0, b0);
    printf("GIF1 %d %d %d\n", r1, g1, b1);
    if (r0 < 200 || g0 > 40 || b0 > 40) bad = 1;
    if (g1 < 200 || r1 > 40 || b1 > 40) bad = 1;
  }
  bw_play_close();

  {
    int sr = 0, sg = 0, sb = 0, br = 0, bg = 0, bb = 0;
    snprintf(path, sizeof path, "%s/sheet.tex", root);
    if (!decode_tex_at(path, 0, &sr, &sg, &sb) || !decode_tex_at(path, 500, &br, &bg, &bb)) {
      fprintf(stderr, "sheet decode failed\n");
      return 1;
    }
    printf("SHEET %d %d %d %d %d %d\n", sr, sg, sb, br, bg, bb);
    if (sr < 200 || sg > 40 || sb > 40) bad = 1;
    if (bb < 200 || br > 40 || bg > 80) bad = 1;
  }

  {
    char mixed[1200], gif[1200], label[1200];
    int rm, rgf, rl;
    snprintf(mixed, sizeof mixed, "%s/mixed", root);
    snprintf(gif, sizeof gif, "%s/gifscene", root);
    snprintf(label, sizeof label, "%s/label", root);
    rm = bw_scene_ready(mixed, NULL);
    rgf = bw_scene_ready(gif, NULL);
    rl = bw_scene_ready(label, NULL);
    printf("READY mixed=%d gif=%d label=%d\n", rm, rgf, rl);
    if (rm != 0 || rgf != 1 || rl != 1) bad = 1;
  }

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/spark", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open spark failed\n%s\n", bw_play_line());
    return 1;
  }
  printf("SPARK0 %s\n", bw_play_line());
  {
    int hr = 0, hg = 0, hb = 0, ar = 0, ag = 0, ab = 0;
    if (!bw_play_step(1000)) return 1;
    printf("SPARK1 %s\n", bw_play_line());
    if (!at(24, 45, &hr, &hg, &hb) || !at(94, 45, &ar, &ag, &ab)) return 1;
    printf("SPARK home=%d %d %d away=%d %d %d\n", hr, hg, hb, ar, ag, ab);
    if (hr > 40 || hg > 40 || hb > 50) bad = 1;
    if (ar < 240 || ag < 140 || ag > 210 || ab < 40 || ab > 100) bad = 1;
  }
  bw_play_close();

  {
    const char* old_assets = getenv("BENDWALL_ASSETS");
    char saved[1100];
    saved[0] = 0;
    if (old_assets) snprintf(saved, sizeof saved, "%s", old_assets);
    snprintf(path, sizeof path, "%s/spark-assets", root);
    setenv("BENDWALL_ASSETS", path, 1);
    snprintf(path, sizeof path, "%s/sparkasset", root);
    if (!bw_play_open(path, 160, 90)) {
      fprintf(stderr, "open sparkasset failed\n%s\n", bw_play_line());
      return 1;
    }
    if (!bw_play_step(1000)) return 1;
    {
      int ar = 0, ag = 0, ab = 0;
      if (!at(94, 45, &ar, &ag, &ab)) return 1;
      printf("SPARKASSET %d %d %d\n", ar, ag, ab);
      if (ar < 240 || ag < 140 || ag > 210 || ab < 40 || ab > 100) bad = 1;
    }
    bw_play_close();
    if (saved[0]) setenv("BENDWALL_ASSETS", saved, 1);
    else unsetenv("BENDWALL_ASSETS");
  }

  {
    const char* old_assets = getenv("BENDWALL_ASSETS");
    char saved[1100];
    int hr = 0, hg = 0, hb = 0, ar = 0, ag = 0, ab = 0;
    saved[0] = 0;
    if (old_assets) snprintf(saved, sizeof saved, "%s", old_assets);
    snprintf(path, sizeof path, "%s/halo-assets", root);
    setenv("BENDWALL_ASSETS", path, 1);
    snprintf(path, sizeof path, "%s/halo", root);
    if (!bw_play_open(path, 160, 90)) {
      fprintf(stderr, "open halo failed\n%s\n", bw_play_line());
      return 1;
    }
    if (!bw_play_step(1000)) return 1;
    if (!at(24, 45, &hr, &hg, &hb) || !at(94, 45, &ar, &ag, &ab)) return 1;
    printf("HALO home=%d %d %d away=%d %d %d\n", hr, hg, hb, ar, ag, ab);
    if (hr > 40 || hg > 40 || hb > 50) bad = 1;
    if (ar < 220 || ag < 160 || ab > 40) bad = 1;
    bw_play_close();
    if (saved[0]) setenv("BENDWALL_ASSETS", saved, 1);
    else unsetenv("BENDWALL_ASSETS");
  }

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/softpart", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open softpart failed\n%s\n", bw_play_line());
    return 1;
  }
  if (!bw_play_step(50)) return 1;
  {
    int sr = 0, sg = 0, sb = 0;
    const char* line = bw_play_line();
    const char* miss = line ? strstr(line, "miss=") : NULL;
    int nmiss = miss ? atoi(miss + 5) : -1;
    if (!at(80, 45, &sr, &sg, &sb)) return 1;
    printf("SOFT %d %d %d miss=%d\n", sr, sg, sb, nmiss);
    if (sr < 200 || sg < 200 || sb < 200 || nmiss != 0) bad = 1;
  }
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/nope", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open nope failed\n%s\n", bw_play_line());
    return 1;
  }
  if (!bw_play_step(50)) return 1;
  {
    int sr = 0, sg = 0, sb = 0;
    const char* line = bw_play_line();
    const char* miss = line ? strstr(line, "miss=") : NULL;
    int nmiss = miss ? atoi(miss + 5) : 0;
    if (!at(80, 45, &sr, &sg, &sb)) return 1;
    printf("NOPE %d %d %d miss=%d\n", sr, sg, sb, nmiss);
    if (sr < 200 || sg < 200 || sb < 200 || nmiss < 1) bad = 1;
  }
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/city", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open city failed\n%s\n", bw_play_line());
    return 1;
  }
  {
    int cr = 0, cg = 0, cb = 0;
    if (!at(80, 45, &cr, &cg, &cb)) return 1;
    printf("CITY %d %d %d\n", cr, cg, cb);
    if (cr > 40 || cg < 50 || cb < 200) bad = 1;
  }
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/userpic", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open userpic failed\n%s\n", bw_play_line());
    return 1;
  }
  {
    int ur = 0, ug = 0, ub = 0;
    if (!at(80, 45, &ur, &ug, &ub)) return 1;
    printf("USER %d %d %d\n", ur, ug, ub);
    if (ur > 40 || ug < 160 || ub > 100) bad = 1;
  }
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/instpic", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open instpic failed\n%s\n", bw_play_line());
    return 1;
  }
  {
    int ir = 0, ig = 0, ib = 0;
    if (!at(80, 45, &ir, &ig, &ib)) return 1;
    printf("INST %d %d %d\n", ir, ig, ib);
    if (ir > 40 || ig < 160 || ib > 100) bad = 1;
  }
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/titled", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open titled failed\n%s\n", bw_play_line());
    return 1;
  }
  {
    int tw = white_count();
    printf("TITLE white=%d\n", tw);
    if (tw >= 8) bad = 1;
  }
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/blankgif", root);
  if (!bw_play_open(path, 160, 90)) {
    fprintf(stderr, "open blankgif failed\n%s\n", bw_play_line());
    return 1;
  }
  {
    int br = 0, bg = 0, bb = 0;
    if (!at(80, 45, &br, &bg, &bb)) return 1;
    printf("BLANK %d %d %d\n", br, bg, bb);
    if (br < 200 || bg > 40 || bb > 40) bad = 1;
  }
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/boxdef", root);
  if (!bw_play_open(path, 640, 360) || !bw_play_step(1000)) return 1;
  {
    int n = off_white(180, 20);
    printf("BOXDEF off=%d\n", n);
    if (n < 20) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/boxnum", root);
  if (!bw_play_open(path, 160, 90) || !bw_play_step(50)) return 1;
  {
    int n = off_white(45, 14);
    printf("BOXNUM off=%d\n", n);
    if (n < 1) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/boxfix", root);
  if (!bw_play_open(path, 160, 90) || !bw_play_step(50)) return 1;
  {
    int cr = 0, cg = 0, cb = 0, fr = 0, fg = 0, fb = 0, yr = 0, yg = 0, yb = 0;
    int n = off_white(45, 16);
    if (!at(40, 45, &cr, &cg, &cb) || !at(100, 45, &fr, &fg, &fb) || !at(40, 15, &yr, &yg, &yb)) return 1;
    printf("BOXFIX center=%d %d %d far=%d %d %d off=%d\n", cr, cg, cb, fr, fg, fb, n);
    if (cr < 200 || fr > 40 || yr > 40 || n > 0) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/velrand", root);
  if (!bw_play_open(path, 160, 90) || !bw_play_step(1000)) return 1;
  {
    int hr = 0, hg = 0, hb = 0, ar = 0, ag = 0, ab = 0;
    if (!at(24, 45, &hr, &hg, &hb) || !at(104, 45, &ar, &ag, &ab)) return 1;
    printf("VEL home=%d %d %d away=%d %d %d\n", hr, hg, hb, ar, ag, ab);
    if (hr > 40 || ar < 200) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/turb", root);
  if (!bw_play_open(path, 160, 90) || !bw_play_step(1000)) return 1;
  {
    int hr = 0, hg = 0, hb = 0, ar = 0, ag = 0, ab = 0;
    if (!at(24, 45, &hr, &hg, &hb) || !at(104, 45, &ar, &ag, &ab)) return 1;
    printf("TURB home=%d %d %d away=%d %d %d\n", hr, hg, hb, ar, ag, ab);
    if (hr > 40 || ar < 200) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/spin", root);
  if (!bw_play_open(path, 160, 90) || !bw_play_step(16)) return 1;
  {
    int r0 = 0, g0 = 0, b0 = 0, r1 = 0, g1 = 0, b1 = 0;
    if (!at(80, 35, &r0, &g0, &b0)) return 1;
    if (!bw_play_step(1000) || !at(80, 35, &r1, &g1, &b1)) return 1;
    printf("SPIN flat=%d %d %d turn=%d %d %d\n", r0, g0, b0, r1, g1, b1);
    if (r0 > 40 || r1 < 200) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/warm", root);
  if (!bw_play_open(path, 160, 90)) return 1;
  {
    const char* line = bw_play_line();
    const char* ap = line ? strstr(line, "alive=") : NULL;
    const char* tp = line ? strstr(line, " t=") : NULL;
    int alive = ap ? atoi(ap + 6) : 0;
    int t = tp ? atoi(tp + 3) : -1;
    printf("WARM alive=%d t=%d\n", alive, t);
    if (alive < 10 || alive > 40 || t != 0) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/mouse", root);
  if (!bw_play_open(path, 160, 90) || !bw_play_step(200)) return 1;
  {
    int r0 = 0, g0 = 0, b0 = 0, r1 = 0, g1 = 0, b1 = 0;
    if (!at(30, 45, &r0, &g0, &b0)) return 1;
    bw_play_pointer(30.f, 10.f, 160.f, 90.f, 1);
    /* movement runs before controlpointattract, so the push shows on the next step. */
    if (!bw_play_step(200) || !bw_play_step(200) || !at(30, 45, &r1, &g1, &b1)) return 1;
    printf("MOUSE still=%d %d %d push=%d %d %d\n", r0, g0, b0, r1, g1, b1);
    if (r0 < 200 || r1 > 40) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/rope", root);
  if (!bw_play_open(path, 160, 90) || !bw_play_step(500)) return 1;
  {
    int mr = 0, mg = 0, mb = 0, sr = 0, sg = 0, sb = 0;
    if (!at(45, 45, &mr, &mg, &mb) || !at(45, 61, &sr, &sg, &sb)) return 1;
    printf("ROPE mid=%d %d %d side=%d %d %d\n", mr, mg, mb, sr, sg, sb);
    if (mr < 200 || sr > 40) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/follow", root);
  if (!bw_play_open(path, 160, 90) || !bw_play_step(500)) return 1;
  {
    int ar = 0, ag = 0, ab = 0, br = 0, bg = 0, bb = 0;
    if (!at(70, 53, &ar, &ag, &ab) || !at(20, 53, &br, &bg, &bb)) return 1;
    printf("FOLLOW on=%d %d %d off=%d %d %d\n", ar, ag, ab, br, bg, bb);
    if (ar < 200 || br > 40) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/frames", root);
  if (!bw_play_open(path, 160, 90) || !bw_play_step(50)) return 1;
  {
    int red = color_count(1, 0);
    int grn = color_count(0, 1);
    int r0 = 0, g0 = 0, b0 = 0, r1 = 0, g1 = 0, b1 = 0;
    if (!at(80, 70, &r0, &g0, &b0)) return 1;
    if (!bw_play_step(1000) || !at(80, 70, &r1, &g1, &b1)) return 1;
    red = color_count(1, 0);
    grn = color_count(0, 1);
    printf("FRAME rand r=%d g=%d seq0=%d %d %d seq1=%d %d %d\n", red, grn, r0, g0, b0, r1, g1, b1);
    if (red < 1 || grn < 1) bad = 1;
    if (r0 < 200 || g0 > 40 || g1 < 200 || r1 > 40) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/page", root);
  if (!bw_play_open(path, 160, 90)) return 1;
  {
    int n = white_x(70);
    printf("PAGE right=%d\n", n);
    if (n < 8) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/osc", root);
  if (!bw_play_open(path, 160, 90) || !bw_play_step(50)) return 1;
  {
    int r = 0, g = 0, b = 0;
    if (!at(80, 45, &r, &g, &b)) return 1;
    printf("OSC %d %d %d\n", r, g, b);
    if (r > 40) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/trail", root);
  if (!bw_play_open(path, 160, 90) || !bw_play_step(16)) return 1;
  {
    int ar = 0, ag = 0, ab = 0, sr = 0, sg = 0, sb = 0;
    if (!at(70, 45, &ar, &ag, &ab) || !at(40, 20, &sr, &sg, &sb)) return 1;
    printf("TRAIL along=%d %d %d side=%d %d %d\n", ar, ag, ab, sr, sg, sb);
    if (ar < 200 || sr > 40) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/oturb", root);
  if (!bw_play_open(path, 160, 90) || !bw_play_step(1000)) return 1;
  {
    int n = white_out(80, 45, 14);
    printf("OTURB off=%d\n", n);
    if (n < 1) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/ember", root);
  if (!bw_play_open(path, 160, 90) || !bw_play_step(500)) return 1;
  {
    int ar = 0, ag = 0, ab = 0, br = 0, bg = 0, bb = 0;
    if (!at(70, 53, &ar, &ag, &ab) || !at(20, 53, &br, &bg, &bb)) return 1;
    printf("EMBER on=%d %d %d off=%d %d %d\n", ar, ag, ab, br, bg, bb);
    if (ar < 200 || br > 40) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/note", root);
  if (!bw_play_open(path, 160, 90)) return 1;
  {
    int n = white_x(70);
    printf("NOTE right=%d\n", n);
    if (n < 8) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/notefile", root);
  if (!bw_play_open(path, 160, 90)) return 1;
  {
    int n = white_x(70);
    printf("FILE right=%d\n", n);
    if (n < 8) bad = 1;
  }
  bw_play_close();

  snprintf(path, sizeof path, "%s/gapart", root);
  if (!bw_play_open(path, 160, 90)) return 1;
  bw_play_close();

  bw_play_audio(NULL, 0);
  snprintf(path, sizeof path, "%s/swarm", root);
  if (!bw_play_open(path, 640, 360)) {
    fprintf(stderr, "open swarm failed\n%s\n", bw_play_line());
    return 1;
  }
  if (!bw_play_step(33) || !bw_play_step(33)) return 1;
  {
    struct timespec t0, t1;
    long ticks0, ticks1, clk;
    double cpu, wall;
    int i, alive = 0;
    const char* line;
    const char* ap;
    FILE* stf;
    char buf[1024];
    char* rp;
    unsigned long ut = 0, stm = 0;
    ticks0 = 0;
    stf = fopen("/proc/self/stat", "r");
    if (stf && fgets(buf, sizeof buf, stf)) {
      rp = strrchr(buf, ')');
      if (rp && sscanf(rp + 1, " %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &ut, &stm) == 2)
        ticks0 = (long)(ut + stm);
    }
    if (stf) fclose(stf);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (i = 0; i < 30; i++) {
      if (!bw_play_step(33)) return 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ut = stm = 0;
    ticks1 = ticks0;
    stf = fopen("/proc/self/stat", "r");
    if (stf && fgets(buf, sizeof buf, stf)) {
      rp = strrchr(buf, ')');
      if (rp && sscanf(rp + 1, " %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &ut, &stm) == 2)
        ticks1 = (long)(ut + stm);
    }
    if (stf) fclose(stf);
    clk = sysconf(_SC_CLK_TCK);
    if (clk < 1) clk = 100;
    wall = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    /* Percent of one core against a 30 fps budget, not against a tight loop. */
    cpu = (clk > 0) ? ((double)(ticks1 - ticks0) / (double)clk) / (30.0 * 0.033) * 100.0 : 0.0;
    line = bw_play_line();
    ap = line ? strstr(line, "alive=") : NULL;
    alive = ap ? atoi(ap + 6) : 0;
    printf("SWARM cpu=%.1f alive=%d wall=%.3f\n", cpu, alive, wall);
    /* instantaneous is a count. 8 emitters, one each, plus rate 8 over ~1s. */
    if (alive < 60 || alive > 100) bad = 1;
    if (cpu >= 30.0) bad = 1;
    (void)wall;
  }
  bw_play_close();

  {
    float buf[4096];
    float full_power = 0;
    int n;
    float peak, power, band;
    bw_play_audio(NULL, 0);
    bw_play_set_video_volume(1.f);
    bw_play_set_video_duck(0);
    snprintf(path, sizeof path, "%s/vidaud", root);
    if (!bw_play_open(path, 64, 36)) {
      fprintf(stderr, "open vidadu failed\n%s\n", bw_play_line());
      return 1;
    }
    n = pull_pcm(buf, 4096, 400);
    pcm_stats(buf, n, &peak, &power);
    printf("VAUDDEF peak=%.3f\n", peak);
    if (peak > 0.02f) bad = 1;
    bw_play_set_video_mute(0);
    n = pull_pcm(buf, 4096, 600);
    pcm_stats(buf, n, &peak, &power);
    full_power = power;
    printf("VAUD peak=%.3f power=%.0f\n", peak, power);
    if (peak < 0.5f || power < 1000.f) bad = 1;
    bw_play_set_video_volume(0.f);
    n = pull_pcm(buf, 4096, 350);
    pcm_stats(buf, n, &peak, &power);
    printf("VAUD0 peak=%.3f\n", peak);
    if (peak > 0.02f) bad = 1;
    bw_play_set_video_volume(1.f);
    bw_play_set_video_mute(1);
    n = pull_pcm(buf, 4096, 350);
    pcm_stats(buf, n, &peak, &power);
    printf("VAUDUSER peak=%.3f\n", peak);
    if (peak > 0.02f) bad = 1;
    bw_play_set_video_mute(0);
    bw_play_set_video_duck(1);
    n = pull_pcm(buf, 4096, 350);
    pcm_stats(buf, n, &peak, &power);
    printf("VAUDDUCK peak=%.3f\n", peak);
    if (peak > 0.02f) bad = 1;
    bw_play_set_video_duck(0);
    n = pull_pcm(buf, 4096, 500);
    pcm_stats(buf, n, &peak, &power);
    bw_play_step(0);
    bw_play_band(0, 0, &band);
    printf("VAUDISO band=%.3f peak=%.3f\n", band, peak);
    if (peak < 0.5f || band > 0.05f) bad = 1;
    bw_play_close();

    snprintf(path, sizeof path, "%s/vidaudmute", root);
    if (!bw_play_open(path, 64, 36)) {
      fprintf(stderr, "open vidadumute failed\n%s\n", bw_play_line());
      return 1;
    }
    n = pull_pcm(buf, 4096, 500);
    pcm_stats(buf, n, &peak, &power);
    printf("VAUDPROJ peak=%.3f\n", peak);
    if (peak > 0.02f) bad = 1;
    bw_play_close();

    snprintf(path, sizeof path, "%s/vidaudhalf", root);
    if (!bw_play_open(path, 64, 36)) {
      fprintf(stderr, "open vidaudhalf failed\n%s\n", bw_play_line());
      return 1;
    }
    n = pull_pcm(buf, 4096, 500);
    pcm_stats(buf, n, &peak, &power);
    printf("VAUDHALF peak=%.3f ratio=%.3f\n", peak, full_power > 1.f ? power / full_power : 0.f);
    if (peak < 0.15f || peak > 0.75f) bad = 1;
    if (!(full_power > 1.f && power / full_power >= 0.12f && power / full_power <= 0.45f)) bad = 1;
    bw_play_close();

    snprintf(path, sizeof path, "%s/vidaudmap", root);
    if (!bw_play_open(path, 64, 36)) {
      fprintf(stderr, "open vidaudmap failed\n%s\n", bw_play_line());
      return 1;
    }
    n = pull_pcm(buf, 4096, 500);
    pcm_stats(buf, n, &peak, &power);
    printf("VAUDMAP peak=%.3f ratio=%.3f\n", peak, full_power > 1.f ? power / full_power : 0.f);
    if (peak < 0.08f || peak > 0.45f) bad = 1;
    if (!(full_power > 1.f && power / full_power >= 0.02f && power / full_power <= 0.15f)) bad = 1;
    bw_play_close();

    snprintf(path, sizeof path, "%s/vidaudmp4", root);
    if (!bw_play_open(path, 64, 36)) {
      fprintf(stderr, "open vidadump4 failed\n%s\n", bw_play_line());
      return 1;
    }
    n = pull_pcm(buf, 4096, 700);
    pcm_stats(buf, n, &peak, &power);
    printf("VAUDAAC peak=%.3f ratio=%.3f\n", peak, full_power > 1.f ? power / full_power : 0.f);
    if (peak < 0.2f || !(full_power > 1.f && power / full_power >= 0.05f)) bad = 1;
    bw_play_close();
    bw_play_set_video_volume(1.f);
    bw_play_set_video_mute(0);
    bw_play_set_video_duck(-1);
  }

  snprintf(path, sizeof path, "%s/livecpu", root);
  if (!bw_play_open(path, 960, 540)) return 1;
  {
    struct timespec t0, t1;
    long ticks0 = 0, ticks1 = 0, clk;
    double back = 0, present = 0, wall;
    int i, sim = 0, glus = 0, rd = 0, psim = 0, pgl = 0, prd = 0, expb = 0;
    long accs, accg, accr;
    FILE* stf;
    char buf[1024];
    char* rp;
    unsigned long ut, stm;
    if (!bw_play_step(33)) return 1;
    accs = accg = accr = 0;
    ut = stm = 0;
    stf = fopen("/proc/self/stat", "r");
    if (stf && fgets(buf, sizeof buf, stf)) {
      rp = strrchr(buf, ')');
      if (rp && sscanf(rp + 1, " %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &ut, &stm) == 2)
        ticks0 = (long)(ut + stm);
    }
    if (stf) fclose(stf);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (i = 0; i < 20; i++) {
      int s = 0, g = 0, r = 0;
      if (!bw_play_step(33)) return 1;
      bw_play_profile(&s, &g, &r);
      accs += s;
      accg += g;
      accr += r;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ut = stm = 0;
    ticks1 = ticks0;
    stf = fopen("/proc/self/stat", "r");
    if (stf && fgets(buf, sizeof buf, stf)) {
      rp = strrchr(buf, ')');
      if (rp && sscanf(rp + 1, " %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &ut, &stm) == 2)
        ticks1 = (long)(ut + stm);
    }
    if (stf) fclose(stf);
    clk = sysconf(_SC_CLK_TCK);
    if (clk < 1) clk = 100;
    wall = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    (void)wall;
    back = ((double)(ticks1 - ticks0) / (double)clk) / (20.0 * 0.033) * 100.0;
    sim = (int)(accs / 20);
    glus = (int)(accg / 20);
    rd = (int)(accr / 20);
    bw_play_set_readback(0);
    if (!bw_play_step(33)) return 1;
    accs = accg = accr = 0;
    ut = stm = 0;
    ticks0 = 0;
    stf = fopen("/proc/self/stat", "r");
    if (stf && fgets(buf, sizeof buf, stf)) {
      rp = strrchr(buf, ')');
      if (rp && sscanf(rp + 1, " %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &ut, &stm) == 2)
        ticks0 = (long)(ut + stm);
    }
    if (stf) fclose(stf);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (i = 0; i < 20; i++) {
      int s = 0, g = 0, r = 0;
      if (!bw_play_step(33)) return 1;
      bw_play_profile(&s, &g, &r);
      accs += s;
      accg += g;
      accr += r;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ut = stm = 0;
    ticks1 = ticks0;
    stf = fopen("/proc/self/stat", "r");
    if (stf && fgets(buf, sizeof buf, stf)) {
      rp = strrchr(buf, ')');
      if (rp && sscanf(rp + 1, " %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &ut, &stm) == 2)
        ticks1 = (long)(ut + stm);
    }
    if (stf) fclose(stf);
    present = ((double)(ticks1 - ticks0) / (double)clk) / (20.0 * 0.033) * 100.0;
    psim = (int)(accs / 20);
    pgl = (int)(accg / 20);
    prd = (int)(accr / 20);
    expb = bw_play_exported();
    printf("LIVECPU back=%.1f sim=%d gl=%d read=%d present=%.1f psim=%d pgl=%d pread=%d export=%d\n",
           back, sim, glus, rd, present, psim, pgl, prd, expb);
    if (prd != 0) bad = 1;
    {
      int br = 0, bg = 0, bb = 0, bok;
      bok = blit_sample(&br, &bg, &bb);
      printf("BLIT ok=%d %d %d %d\n", bok, br, bg, bb);
      if (!bok || bb < 10) bad = 1;
    }
    {
      void* imgs[8];
      int ni = 0, k, u, seen;
      for (k = 0; k < 6; k++) {
        void* img;
        if (!bw_play_step(33)) return 1;
        img = bw_gl_color_image();
        if (!img) {
          bad = 1;
          break;
        }
        seen = 0;
        for (u = 0; u < ni; u++) if (imgs[u] == img) seen = 1;
        if (!seen && ni < 8) imgs[ni++] = img;
      }
      printf("RING %d\n", ni);
      if (ni < 3) bad = 1;
    }
    {
      EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
      struct Flick fa, fb;
      pthread_t ta, tb;
      atomic_int stop = 0, ok = 0, black = 0;
      int started = 0, i;
      fa.surf = fb.surf = EGL_NO_SURFACE;
      fa.ctx = fb.ctx = EGL_NO_CONTEXT;
      if (dpy != EGL_NO_DISPLAY && eglInitialize(dpy, NULL, NULL)
          && peer_pbuffer(dpy, &fa.surf, &fa.ctx)
          && peer_pbuffer(dpy, &fb.surf, &fb.ctx)) {
        fa.dpy = fb.dpy = dpy;
        fa.stop = fb.stop = &stop;
        fa.ok = fb.ok = &ok;
        fa.black = fb.black = &black;
        if (pthread_create(&ta, NULL, flicker_thread, &fa) != 0) {
          started = 0;
        } else if (pthread_create(&tb, NULL, flicker_thread, &fb) != 0) {
          atomic_store(&stop, 1);
          pthread_join(ta, NULL);
        } else {
          started = 1;
          for (i = 0; i < 8; i++) {
            if (!bw_play_step(33)) bad = 1;
            usleep(4000);
          }
          atomic_store(&stop, 1);
          pthread_join(ta, NULL);
          pthread_join(tb, NULL);
        }
      }
      if (!started) bad = 1;
      printf("FLICKER ok=%d black=%d\n", atomic_load(&ok), atomic_load(&black));
      if (atomic_load(&ok) < 4 || atomic_load(&black) != 0) bad = 1;
      if (fa.ctx != EGL_NO_CONTEXT) eglDestroyContext(dpy, fa.ctx);
      if (fb.ctx != EGL_NO_CONTEXT) eglDestroyContext(dpy, fb.ctx);
      if (fa.surf != EGL_NO_SURFACE) eglDestroySurface(dpy, fa.surf);
      if (fb.surf != EGL_NO_SURFACE) eglDestroySurface(dpy, fb.surf);
    }
    bw_play_set_readback(1);
  }
  bw_play_close();

  if (!diff) bad = 1;
  if (b16 < 0.9f || b32 < 0.9f || b64 < 0.9f || r16 > 0.05f) bad = 1;
  if (tone64 < 0.8f || tone16 < 0.8f) bad = 1;
  if (white < 8) bad = 1;
  if (pr1 < pr0 + 40) bad = 1;
  if (tg < 50 || tg > 140 || tr > 40) bad = 1;
  printf("PLAYMAIN %s\n", bad ? "fail" : "ok");
  return bad;
}

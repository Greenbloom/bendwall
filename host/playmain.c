/* Headless scene-image check. Two video frames differ, the text bitmap
   lands in the picture, tint and pulse change pixels, and the spectrum
   uniforms follow an injected buffer. */
#include "plan.h"
#include "player.h"

#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int decode_tex(const char* path, int* r, int* g, int* b) {
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
  if (n != (int)sz || !bw_tex_decode(buf, n, &rgba, &w, &h) || !rgba || w < 1 || h < 1) {
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

  if (!diff) bad = 1;
  if (b16 < 0.9f || b32 < 0.9f || b64 < 0.9f || r16 > 0.05f) bad = 1;
  if (tone64 < 0.8f || tone16 < 0.8f) bad = 1;
  if (white < 8) bad = 1;
  if (pr1 < pr0 + 40) bad = 1;
  if (tg < 50 || tg > 140 || tr > 40) bad = 1;
  printf("PLAYMAIN %s\n", bad ? "fail" : "ok");
  return bad;
}

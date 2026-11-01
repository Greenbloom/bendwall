/* Headless scene-image check. Two video frames differ, the text bitmap
   lands in the picture, tint and pulse change pixels, and the spectrum
   uniforms follow an injected buffer. */
#include "player.h"

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

  if (!diff) bad = 1;
  if (b16 < 0.9f || b32 < 0.9f || b64 < 0.9f || r16 > 0.05f) bad = 1;
  if (tone64 < 0.8f || tone16 < 0.8f) bad = 1;
  if (white < 8) bad = 1;
  if (pr1 < pr0 + 40) bad = 1;
  if (tg < 50 || tg > 140 || tr > 40) bad = 1;
  printf("PLAYMAIN %s\n", bad ? "fail" : "ok");
  return bad;
}

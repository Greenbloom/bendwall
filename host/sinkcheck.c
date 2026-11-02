/* Wallpaper soundtrack on a null sink. Exit 2 unless PULSE_SINK is bendwall_null. */
#include "player.h"

#include <math.h>
#include <pthread.h>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t g_other_stop;
static volatile sig_atomic_t g_other_ready;
static pa_simple* g_other;

static void* other_main(void* unused) {
  pa_sample_spec ss;
  pa_simple* s;
  int err = 0;
  double phase = 0.0;
  const double turn = 6.283185307179586;
  float buf[2048];
  (void)unused;
  memset(&ss, 0, sizeof ss);
  ss.format = PA_SAMPLE_FLOAT32LE;
  ss.rate = 44100;
  ss.channels = 2;
  s = pa_simple_new(NULL, "OtherApp", PA_STREAM_PLAYBACK, NULL, "tone", &ss, NULL, NULL, &err);
  if (!s) {
    fprintf(stderr, "OtherApp pa_simple_new failed: %s\n", pa_strerror(err));
    return NULL;
  }
  g_other = s;
  g_other_ready = 1;
  while (!g_other_stop) {
    int i;
    for (i = 0; i < 2048; i += 2) {
      float v = (float)(0.2 * sin(phase));
      phase += turn * 440.0 / 44100.0;
      if (phase >= turn) phase -= turn;
      buf[i] = v;
      buf[i + 1] = v;
    }
    if (pa_simple_write(s, buf, sizeof buf, &err) < 0) {
      fprintf(stderr, "OtherApp write failed: %s\n", pa_strerror(err));
      break;
    }
  }
  return NULL;
}

static float peak_of(const float* s, int n) {
  float peak = 0.f;
  int i;
  for (i = 0; i < n; i++) {
    float a = s[i] < 0.f ? -s[i] : s[i];
    if (a > peak) peak = a;
  }
  return peak;
}

static void other_join(pthread_t thr) {
  g_other_stop = 1;
  pthread_join(thr, NULL);
  if (g_other) {
    pa_simple_free(g_other);
    g_other = NULL;
  }
}

int main(int argc, char** argv) {
  const char* sink = getenv("PULSE_SINK");
  pthread_t thr;
  float buf[4096];
  int n, bad = 0;
  float peak;
  if (!sink || strcmp(sink, "bendwall_null") != 0) return 2;
  if (argc < 2 || !argv[1] || !argv[1][0]) return 1;
  if (pthread_create(&thr, NULL, other_main, NULL) != 0) return 1;
  usleep(300000);
  if (!g_other_ready) {
    fprintf(stderr, "OtherApp playback did not start\n");
    other_join(thr);
    return 1;
  }
  bw_play_set_video_volume(1.f);
  bw_play_set_video_mute(0);
  bw_play_set_video_duck(0);
  if (!bw_play_open(argv[1], 64, 36)) {
    fprintf(stderr, "open vidaud failed\n%s\n", bw_play_line() ? bw_play_line() : "");
    other_join(thr);
    return 1;
  }
  usleep(500000);
  n = bw_play_video_pcm(buf, 4096);
  peak = peak_of(buf, n);
  printf("PEAKOFF %.3f\n", peak);
  fflush(stdout);
  if (!(peak > 0.2f)) bad = 1;
  bw_play_set_video_duck(-1);
  usleep(500000);
  n = bw_play_video_pcm(buf, 4096);
  peak = peak_of(buf, n);
  printf("PEAKON %.3f\n", peak);
  fflush(stdout);
  if (!(peak < 0.02f)) bad = 1;
  bw_play_set_video_duck(0);
  usleep(400000);
  printf("HOLD 1\n");
  fflush(stdout);
  sleep(3);
  bw_play_close();
  other_join(thr);
  return bad ? 1 : 0;
}

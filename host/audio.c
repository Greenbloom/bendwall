/* Pulse/PipeWire loopback. Bend has no audio binding. A missing server, or a
   read that does not return within a second, answers 0 so the wallpaper
   keeps drawing. Samples stay in this process; Bend only sees the count. */
#include <pulse/error.h>
#include <pulse/simple.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static void bw_alrm(int s) { (void)s; }

int bw_audio_capture(float* dst, int n) {
  pa_sample_spec ss;
  pa_buffer_attr attr;
  pa_simple* s;
  struct sigaction sa, old;
  int err = 0;
  int got;
  if (!dst || n < 2) return 0;
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = bw_alrm;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGALRM, &sa, &old);
  alarm(1);
  memset(&ss, 0, sizeof ss);
  ss.format = PA_SAMPLE_FLOAT32LE;
  ss.rate = 44100;
  ss.channels = 2;
  memset(&attr, 0, sizeof attr);
  attr.maxlength = (uint32_t)n * (uint32_t)sizeof(float);
  attr.fragsize = attr.maxlength;
  s = pa_simple_new(NULL, "bendwall", PA_STREAM_RECORD, NULL, "loop", &ss, NULL, &attr, &err);
  if (!s) {
    alarm(0);
    sigaction(SIGALRM, &old, NULL);
    return 0;
  }
  got = pa_simple_read(s, dst, (size_t)n * sizeof(float), &err);
  alarm(0);
  sigaction(SIGALRM, &old, NULL);
  pa_simple_free(s);
  if (got < 0) return 0;
  return n;
}

/* A wallpaper cannot open a new stream on every frame. One background read
   keeps the latest chunk. A missing server ends the thread; the picture
   still draws. bw_audio_capture above is unchanged for the one-shot probe. */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t g_stop;
static int g_thr_on;
static float g_live[1024];
static int g_live_n;
static float g_inject[4096];
static int g_inject_n;
static int g_use_inject;

/* Playback, not the microphone: the monitor of the default sink. */
static int monitor_device(char* out, int cap) {
  FILE* p;
  char sink[240];
  size_t n;
  const char* env = getenv("BENDWALL_AUDIO_SOURCE");
  if (env && env[0]) {
    snprintf(out, (size_t)cap, "%s", env);
    return 1;
  }
  p = popen("timeout 1 pactl get-default-sink 2>/dev/null", "r");
  if (!p) return 0;
  if (!fgets(sink, sizeof sink, p)) { pclose(p); return 0; }
  pclose(p);
  n = strlen(sink);
  while (n && (sink[n - 1] == '\n' || sink[n - 1] == '\r' || sink[n - 1] == ' ')) sink[--n] = 0;
  if (!n || strchr(sink, ' ')) return 0;
  snprintf(out, (size_t)cap, "%s.monitor", sink);
  return 1;
}

static void* audio_main(void* unused) {
  pa_sample_spec ss;
  pa_buffer_attr attr;
  pa_simple* s = NULL;
  struct sigaction sa, old;
  float buf[1024];
  char dev[300];
  int err = 0;
  int have_dev;
  (void)unused;
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = bw_alrm;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGALRM, &sa, &old);
  alarm(2);
  memset(&ss, 0, sizeof ss);
  ss.format = PA_SAMPLE_FLOAT32LE;
  ss.rate = 44100;
  ss.channels = 2;
  memset(&attr, 0, sizeof attr);
  attr.maxlength = (uint32_t)sizeof buf;
  attr.fragsize = (uint32_t)sizeof buf;
  have_dev = monitor_device(dev, (int)sizeof dev);
  if (have_dev)
    s = pa_simple_new(NULL, "bendwall", PA_STREAM_RECORD, dev, "wallpaper", &ss, NULL, &attr, &err);
  if (!s)
    s = pa_simple_new(NULL, "bendwall", PA_STREAM_RECORD, NULL, "wallpaper", &ss, NULL, &attr, &err);
  alarm(0);
  sigaction(SIGALRM, &old, NULL);
  if (!s) return NULL;
  while (!g_stop) {
    if (pa_simple_read(s, buf, sizeof buf, &err) < 0) break;
    pthread_mutex_lock(&g_mu);
    memcpy(g_live, buf, sizeof buf);
    g_live_n = 1024;
    pthread_mutex_unlock(&g_mu);
  }
  pa_simple_free(s);
  return NULL;
}

void bw_audio_live_start(void) {
  pthread_t thr;
  if (g_thr_on) return;
  g_stop = 0;
  if (pthread_create(&thr, NULL, audio_main, NULL) != 0) return;
  pthread_detach(thr);
  g_thr_on = 1;
}

void bw_audio_inject(const float* s, int n) {
  if (!s || n < 2) {
    g_use_inject = 0;
    g_inject_n = 0;
    return;
  }
  if (n > 4096) n = 4096;
  memcpy(g_inject, s, (size_t)n * sizeof(float));
  g_inject_n = n;
  g_use_inject = 1;
}

int bw_audio_samples(float* dst, int cap) {
  int n;
  if (!dst || cap < 2) return 0;
  if (g_use_inject) {
    n = g_inject_n < cap ? g_inject_n : cap;
    memcpy(dst, g_inject, (size_t)n * sizeof(float));
    return n;
  }
  pthread_mutex_lock(&g_mu);
  n = g_live_n < cap ? g_live_n : cap;
  if (n > 0) memcpy(dst, g_live, (size_t)n * sizeof(float));
  pthread_mutex_unlock(&g_mu);
  return n;
}

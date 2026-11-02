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
   keeps the latest chunk. A missing server ends the wait; the picture still
   draws. bw_audio_capture above is unchanged for the one-shot probe.

   The live read is not the default sink monitor. That monitor includes the
   soundtrack this process plays, which would feed the spectrum. The thread
   records one other sink-input. Corked inputs are skipped. Bendwall's own
   playback is skipped too, or a duck would treat the wallpaper as another
   app and silence it. Own playback is application.id com.philip.bendwall,
   application.name Bendwall, or the old name bendwall-video. */
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/mainloop.h>
#include <pulse/proplist.h>
#include <pulse/stream.h>
#include <pulse/thread-mainloop.h>

static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t g_stop;
static int g_thr_on;
static float g_live[1024];
static int g_live_n;
static float g_inject[4096];
static int g_inject_n;
static int g_use_inject;
static volatile sig_atomic_t g_others;

static pthread_mutex_t g_play_mu = PTHREAD_MUTEX_INITIALIZER;
static pa_threaded_mainloop* g_play_ml;
static pa_context* g_play_ctx;
static pa_stream* g_play_st;
static int g_play_failed;

typedef struct SpecScan {
  int list_done;
  int sink_done;
  int others;
  uint32_t idx;
  uint32_t sink;
  char mon[256];
} SpecScan;

static int input_is_ours(const pa_proplist* pl) {
  const char* name;
  const char* id;
  if (!pl) return 0;
  id = pa_proplist_gets(pl, PA_PROP_APPLICATION_ID);
  if (id && strcmp(id, "com.philip.bendwall") == 0) return 1;
  name = pa_proplist_gets(pl, PA_PROP_APPLICATION_NAME);
  if (!name) return 0;
  if (strcmp(name, "Bendwall") == 0) return 1;
  if (strcmp(name, "bendwall-video") == 0) return 1;
  return 0;
}

static void on_input(pa_context* c, const pa_sink_input_info* i, int eol, void* ud) {
  SpecScan* s = ud;
  (void)c;
  if (eol) {
    s->list_done = 1;
    return;
  }
  if (!i || i->corked) return;
  if (input_is_ours(i->proplist)) return;
  s->others = 1;
  if (s->idx == PA_INVALID_INDEX) {
    s->idx = i->index;
    s->sink = i->sink;
  }
}

static void on_sink(pa_context* c, const pa_sink_info* i, int eol, void* ud) {
  SpecScan* s = ud;
  (void)c;
  if (eol) {
    s->sink_done = 1;
    return;
  }
  if (!i || !i->monitor_source_name || !i->monitor_source_name[0]) return;
  snprintf(s->mon, sizeof s->mon, "%s", i->monitor_source_name);
}

static void on_spec_read(pa_stream* st, size_t nbytes, void* ud) {
  const void* data = NULL;
  size_t n = 0;
  (void)nbytes;
  (void)ud;
  while (pa_stream_peek(st, &data, &n) == 0 && n > 0) {
    if (data && n >= sizeof(float) * 2) {
      int nf = (int)(n / sizeof(float));
      const float* src = data;
      if (nf > 1024) {
        src += nf - 1024;
        nf = 1024;
      }
      pthread_mutex_lock(&g_mu);
      memcpy(g_live, src, (size_t)nf * sizeof(float));
      g_live_n = nf;
      pthread_mutex_unlock(&g_mu);
    }
    pa_stream_drop(st);
  }
}

static int wait_op(pa_mainloop* ml, int* flag, int spins) {
  int i;
  for (i = 0; i < spins && !*flag && !g_stop; i++) {
    if (pa_mainloop_iterate(ml, 0, NULL) < 0) return 0;
    usleep(5000);
  }
  return *flag;
}

static void clear_live(void) {
  pthread_mutex_lock(&g_mu);
  g_live_n = 0;
  pthread_mutex_unlock(&g_mu);
}

static void* audio_main(void* unused) {
  pa_mainloop* ml = NULL;
  pa_context* ctx = NULL;
  pa_sample_spec ss;
  (void)unused;
  memset(&ss, 0, sizeof ss);
  ss.format = PA_SAMPLE_FLOAT32LE;
  ss.rate = 44100;
  ss.channels = 2;
  ml = pa_mainloop_new();
  if (!ml) return NULL;
  ctx = pa_context_new(pa_mainloop_get_api(ml), "bendwall");
  if (!ctx || pa_context_connect(ctx, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) {
    if (ctx) pa_context_unref(ctx);
    pa_mainloop_free(ml);
    return NULL;
  }
  {
    int spins = 0;
    pa_context_state_t st = PA_CONTEXT_CONNECTING;
    while (spins < 300 && !g_stop) {
      st = pa_context_get_state(ctx);
      if (st == PA_CONTEXT_READY || st == PA_CONTEXT_FAILED || st == PA_CONTEXT_TERMINATED) break;
      if (pa_mainloop_iterate(ml, 0, NULL) < 0) break;
      usleep(5000);
      spins++;
    }
    if (st != PA_CONTEXT_READY) {
      pa_context_disconnect(ctx);
      pa_context_unref(ctx);
      pa_mainloop_free(ml);
      return NULL;
    }
  }
  while (!g_stop) {
    SpecScan scan;
    pa_operation* op;
    pa_stream* st = NULL;
    pa_buffer_attr attr;
    memset(&scan, 0, sizeof scan);
    scan.idx = PA_INVALID_INDEX;
    scan.sink = PA_INVALID_INDEX;
    if (pa_context_get_state(ctx) != PA_CONTEXT_READY) break;
    op = pa_context_get_sink_input_info_list(ctx, on_input, &scan);
    if (op) {
      wait_op(ml, &scan.list_done, 100);
      pa_operation_unref(op);
    }
    g_others = scan.others ? 1 : 0;
    if (scan.idx == PA_INVALID_INDEX) {
      clear_live();
      usleep(200000);
      continue;
    }
    op = pa_context_get_sink_info_by_index(ctx, scan.sink, on_sink, &scan);
    if (op) {
      wait_op(ml, &scan.sink_done, 100);
      pa_operation_unref(op);
    }
    if (!scan.mon[0]) {
      usleep(200000);
      continue;
    }
    st = pa_stream_new(ctx, "bendwall-spectrum", &ss, NULL);
    if (!st) {
      usleep(200000);
      continue;
    }
    pa_stream_set_read_callback(st, on_spec_read, NULL);
    if (pa_stream_set_monitor_stream(st, scan.idx) < 0) {
      pa_stream_unref(st);
      usleep(200000);
      continue;
    }
    memset(&attr, 0, sizeof attr);
    attr.maxlength = (uint32_t)-1;
    attr.fragsize = 1024 * (uint32_t)sizeof(float);
    if (pa_stream_connect_record(st, scan.mon, &attr, PA_STREAM_DONT_MOVE) < 0) {
      pa_stream_unref(st);
      usleep(200000);
      continue;
    }
    {
      int t;
      for (t = 0; t < 40 && !g_stop; t++) {
        if (pa_mainloop_iterate(ml, 0, NULL) < 0) break;
        usleep(10000);
      }
    }
    pa_stream_disconnect(st);
    pa_stream_unref(st);
  }
  pa_context_disconnect(ctx);
  pa_context_unref(ctx);
  pa_mainloop_free(ml);
  return NULL;
}

int bw_audio_others(void) { return g_others ? 1 : 0; }

static int class_ignored(const unsigned char* data, unsigned long n) {
  char buf[512];
  unsigned long i, m;
  if (!data || n == 0) return 0;
  m = n < sizeof buf - 1 ? n : sizeof buf - 1;
  for (i = 0; i < m; i++) {
    unsigned char c = data[i];
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
    if (c == 0) c = ' ';
    buf[i] = (char)c;
  }
  buf[m] = 0;
  return strstr(buf, "plasmashell") || strstr(buf, "kwin") || strstr(buf, "bendwall");
}

int bw_wm_fullscreen(void) {
  static void* lib;
  static int tried, cached;
  static long long stamp;
  struct timespec ts;
  long long now;
  Display* (*xopen)(const char*);
  int (*xclose)(Display*);
  Atom (*xintern)(Display*, const char*, Bool);
  int (*xprop)(Display*, Window, Atom, long, long, Bool, Atom, Atom*, int*, unsigned long*, unsigned long*, unsigned char**);
  int (*xfree)(void*);
  Window (*xroot)(Display*);
  Display* d;
  Atom client_list, wm_state, fullscreen, wm_class;
  Atom actual;
  int format;
  unsigned long n = 0, after = 0;
  unsigned char* data = NULL;
  Window* wins;
  unsigned long i;
  int yes = 0;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  now = (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
  if (stamp && now - stamp < 250) return cached;
  stamp = now;
  cached = 0;
  if (!tried) {
    tried = 1;
    lib = dlopen("libX11.so.6", RTLD_LAZY);
  }
  if (!lib) return 0;
  xopen = (Display * (*)(const char*))dlsym(lib, "XOpenDisplay");
  xclose = (int (*)(Display*))dlsym(lib, "XCloseDisplay");
  xintern = (Atom (*)(Display*, const char*, Bool))dlsym(lib, "XInternAtom");
  xprop = (int (*)(Display*, Window, Atom, long, long, Bool, Atom, Atom*, int*, unsigned long*, unsigned long*, unsigned char**))dlsym(lib, "XGetWindowProperty");
  xfree = (int (*)(void*))dlsym(lib, "XFree");
  xroot = (Window (*)(Display*))dlsym(lib, "XDefaultRootWindow");
  if (!xopen || !xclose || !xintern || !xprop || !xfree || !xroot) return 0;
  d = xopen(NULL);
  if (!d) return 0;
  client_list = xintern(d, "_NET_CLIENT_LIST", False);
  wm_state = xintern(d, "_NET_WM_STATE", False);
  fullscreen = xintern(d, "_NET_WM_STATE_FULLSCREEN", False);
  wm_class = xintern(d, "WM_CLASS", False);
  if (xprop(d, xroot(d), client_list, 0, 1024, False, XA_WINDOW, &actual, &format, &n, &after, &data) == Success && data) {
    wins = (Window*)data;
    for (i = 0; i < n && !yes; i++) {
      unsigned char* state = NULL;
      unsigned char* cls = NULL;
      unsigned long ns = 0, nc = 0, a2 = 0;
      Atom at = 0;
      int fmt = 0;
      int skip = 0;
      unsigned long k;
      if (xprop(d, wins[i], wm_class, 0, 128, False, XA_STRING, &at, &fmt, &nc, &a2, &cls) == Success && cls)
        skip = class_ignored(cls, nc);
      if (cls) xfree(cls);
      if (skip) continue;
      if (xprop(d, wins[i], wm_state, 0, 64, False, XA_ATOM, &at, &fmt, &ns, &a2, &state) == Success && state) {
        Atom* atoms = (Atom*)state;
        for (k = 0; k < ns; k++)
          if (atoms[k] == fullscreen) yes = 1;
      }
      if (state) xfree(state);
    }
  }
  if (data) xfree(data);
  xclose(d);
  cached = yes;
  return yes;
}

static pa_proplist* play_props(void) {
  pa_proplist* pl = pa_proplist_new();
  if (!pl) return NULL;
  pa_proplist_sets(pl, PA_PROP_APPLICATION_NAME, "Bendwall");
  pa_proplist_sets(pl, PA_PROP_APPLICATION_ID, "com.philip.bendwall");
  pa_proplist_sets(pl, PA_PROP_APPLICATION_ICON_NAME, "preferences-desktop-wallpaper");
  pa_proplist_sets(pl, PA_PROP_MEDIA_ROLE, "music");
  pa_proplist_sets(pl, PA_PROP_MEDIA_NAME, "Wallpaper");
  return pl;
}

/* This libpulse has pa_threaded_mainloop_wait and no timed wait.
   Sleep outside the lock so a dead server cannot hang the wallpaper. */
static int wait_play(pa_threaded_mainloop* ml, int (*state_of)(void*), int ready, int dead1, int dead2, void* obj) {
  int i;
  for (i = 0; i < 200; i++) {
    int st;
    pa_threaded_mainloop_lock(ml);
    st = state_of(obj);
    pa_threaded_mainloop_unlock(ml);
    if (st == ready) return 1;
    if (st == dead1 || st == dead2) return 0;
    usleep(5000);
  }
  return 0;
}

static int ctx_state(void* obj) { return (int)pa_context_get_state(obj); }

static int stream_state(void* obj) { return (int)pa_stream_get_state(obj); }

static void play_shutdown(pa_threaded_mainloop* ml, pa_context* ctx, pa_stream* st) {
  if (!ml) return;
  pa_threaded_mainloop_lock(ml);
  if (st) {
    if (pa_stream_get_state(st) != PA_STREAM_UNCONNECTED && pa_stream_get_state(st) != PA_STREAM_TERMINATED)
      pa_stream_disconnect(st);
    pa_stream_unref(st);
  }
  if (ctx) {
    if (pa_context_get_state(ctx) != PA_CONTEXT_UNCONNECTED && pa_context_get_state(ctx) != PA_CONTEXT_TERMINATED)
      pa_context_disconnect(ctx);
    pa_context_unref(ctx);
  }
  pa_threaded_mainloop_unlock(ml);
  pa_threaded_mainloop_stop(ml);
  pa_threaded_mainloop_free(ml);
}

static int play_connect(void) {
  pa_threaded_mainloop* ml;
  pa_context* ctx = NULL;
  pa_stream* st = NULL;
  pa_proplist* pl;
  pa_sample_spec ss;
  pa_buffer_attr attr;
  int rc;
  ml = pa_threaded_mainloop_new();
  if (!ml) return 0;
  if (pa_threaded_mainloop_start(ml) < 0) {
    pa_threaded_mainloop_free(ml);
    return 0;
  }
  pl = play_props();
  if (!pl) {
    pa_threaded_mainloop_stop(ml);
    pa_threaded_mainloop_free(ml);
    return 0;
  }
  pa_threaded_mainloop_lock(ml);
  ctx = pa_context_new_with_proplist(pa_threaded_mainloop_get_api(ml), "Bendwall", pl);
  if (!ctx) {
    pa_threaded_mainloop_unlock(ml);
    pa_proplist_free(pl);
    pa_threaded_mainloop_stop(ml);
    pa_threaded_mainloop_free(ml);
    return 0;
  }
  rc = pa_context_connect(ctx, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);
  pa_threaded_mainloop_unlock(ml);
  if (rc < 0 || !wait_play(ml, ctx_state, PA_CONTEXT_READY, PA_CONTEXT_FAILED, PA_CONTEXT_TERMINATED, ctx)) {
    pa_proplist_free(pl);
    play_shutdown(ml, ctx, NULL);
    return 0;
  }
  memset(&ss, 0, sizeof ss);
  ss.format = PA_SAMPLE_FLOAT32LE;
  ss.rate = 44100;
  ss.channels = 2;
  memset(&attr, 0, sizeof attr);
  attr.maxlength = (uint32_t)-1;
  attr.tlength = (uint32_t)(44100 / 5 * 2 * sizeof(float));
  attr.prebuf = (uint32_t)-1;
  attr.minreq = (uint32_t)-1;
  attr.fragsize = (uint32_t)-1;
  pa_threaded_mainloop_lock(ml);
  st = pa_stream_new_with_proplist(ctx, "Wallpaper", &ss, NULL, pl);
  pa_proplist_free(pl);
  if (!st) {
    pa_threaded_mainloop_unlock(ml);
    play_shutdown(ml, ctx, NULL);
    return 0;
  }
  rc = pa_stream_connect_playback(st, NULL, &attr, 0, NULL, NULL);
  pa_threaded_mainloop_unlock(ml);
  if (rc < 0 || !wait_play(ml, stream_state, PA_STREAM_READY, PA_STREAM_FAILED, PA_STREAM_TERMINATED, st)) {
    play_shutdown(ml, ctx, st);
    return 0;
  }
  g_play_ml = ml;
  g_play_ctx = ctx;
  g_play_st = st;
  return 1;
}

static void play_fail(void) {
  pa_threaded_mainloop* ml = g_play_ml;
  pa_context* ctx = g_play_ctx;
  pa_stream* st = g_play_st;
  g_play_ml = NULL;
  g_play_ctx = NULL;
  g_play_st = NULL;
  g_play_failed = 1;
  if (ml) play_shutdown(ml, ctx, st);
}

static int play_send(const float* data, int nfloats) {
  size_t need = (size_t)nfloats * sizeof(float);
  int spins = 0;
  for (;;) {
    pa_stream_state_t sst;
    size_t room;
    pa_threaded_mainloop_lock(g_play_ml);
    sst = pa_stream_get_state(g_play_st);
    if (sst == PA_STREAM_FAILED || sst == PA_STREAM_TERMINATED) {
      pa_threaded_mainloop_unlock(g_play_ml);
      play_fail();
      return 0;
    }
    room = pa_stream_writable_size(g_play_st);
    if (room == (size_t)-1) {
      pa_threaded_mainloop_unlock(g_play_ml);
      play_fail();
      return 0;
    }
    if (room >= need) {
      int wr = pa_stream_write(g_play_st, data, need, NULL, 0, PA_SEEK_RELATIVE);
      pa_threaded_mainloop_unlock(g_play_ml);
      if (wr < 0) {
        play_fail();
        return 0;
      }
      return 1;
    }
    pa_threaded_mainloop_unlock(g_play_ml);
    if (spins >= 10) return 0;
    usleep(5000);
    spins++;
  }
}

int bw_audio_play_write(const float* interleaved, int nfloats) {
  int ok;
  if (!interleaved || nfloats < 2) return 0;
  if (nfloats & 1) nfloats--;
  pthread_mutex_lock(&g_play_mu);
  if (g_play_failed) {
    pthread_mutex_unlock(&g_play_mu);
    return 0;
  }
  if (!g_play_st) {
    if (!play_connect()) {
      g_play_failed = 1;
      pthread_mutex_unlock(&g_play_mu);
      return 0;
    }
  }
  ok = play_send(interleaved, nfloats);
  pthread_mutex_unlock(&g_play_mu);
  return ok;
}

int bw_audio_play_active(void) {
  int on;
  pthread_mutex_lock(&g_play_mu);
  on = g_play_st != NULL;
  pthread_mutex_unlock(&g_play_mu);
  return on;
}

void bw_audio_play_stop(void) {
  pa_threaded_mainloop* ml;
  pa_context* ctx;
  pa_stream* st;
  pthread_mutex_lock(&g_play_mu);
  ml = g_play_ml;
  ctx = g_play_ctx;
  st = g_play_st;
  g_play_ml = NULL;
  g_play_ctx = NULL;
  g_play_st = NULL;
  g_play_failed = 0;
  if (ml) play_shutdown(ml, ctx, st);
  pthread_mutex_unlock(&g_play_mu);
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

#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef BW_AV_EFF
#define BW_AV_EFF

static void* bw_av_so;
static int (*bw_cap)(float*, int);
static int (*bw_vid)(const char*);
static int (*bw_vtex)(const char*, int);
static int (*bw_ready)(const char*, const char*);

static void bw_av_load(void) {
  const char* p;
  if (bw_av_so) return;
  p = getenv("BENDWALL_NATIVE");
  if (!p || !p[0]) p = "host/libbendwall.so";
  bw_av_so = dlopen(p, RTLD_NOW);
  if (!bw_av_so) return;
  bw_cap = (int (*)(float*, int))dlsym(bw_av_so, "bw_audio_capture");
  bw_vid = (int (*)(const char*))dlsym(bw_av_so, "bw_video_ok");
  bw_vtex = (int (*)(const char*, int))dlsym(bw_av_so, "bw_video_tex");
  bw_ready = (int (*)(const char*, const char*))dlsym(bw_av_so, "bw_scene_ready");
}

Term av_cap_run(Env e, Term* f, IoWork* w) {
  float tmp[8];
  int n;
  (void)e;
  (void)f;
  (void)w;
  bw_av_load();
  n = bw_cap ? bw_cap(tmp, 8) : 0;
  return (Term)(uint32_t)n;
}

Term av_video_run(Env e, Term* f, IoWork* w) {
  char* path;
  u64 n = 0;
  int r;
  (void)w;
  bw_av_load();
  path = io_cstr(e, f[0], &n);
  r = (bw_vid && path) ? bw_vid(path) : 0;
  free(path);
  return (Term)(uint32_t)r;
}

Term av_tex_run(Env e, Term* f, IoWork* w) {
  char* path;
  u64 n = 0;
  int r;
  (void)w;
  bw_av_load();
  path = io_cstr(e, f[0], &n);
  r = (bw_vtex && path) ? bw_vtex(path, (int)(uint32_t)f[1]) : 0;
  free(path);
  return (Term)(uint32_t)r;
}

Term scene_ready_run(Env e, Term* f, IoWork* w) {
  char* dir;
  char* assets;
  u64 n = 0, m = 0;
  int r;
  (void)w;
  bw_av_load();
  dir = io_cstr(e, f[0], &n);
  assets = io_cstr(e, f[1], &m);
  r = (bw_ready && dir) ? bw_ready(dir, assets ? assets : "") : 0;
  free(dir);
  free(assets);
  return (Term)(uint32_t)r;
}

#endif

#ifdef CID(av_cap)
static void __attribute__((constructor)) av_cap_use(void) { io_eff(CID(av_cap), av_cap_run); }
#endif
#ifdef CID(av_video)
static void __attribute__((constructor)) av_video_use(void) { io_eff(CID(av_video), av_video_run); }
#endif
#ifdef CID(av_tex)
static void __attribute__((constructor)) av_tex_use(void) { io_eff(CID(av_tex), av_tex_run); }
#endif
#ifdef CID(scene_ready)
static void __attribute__((constructor)) scene_ready_use(void) { io_eff(CID(scene_ready), scene_ready_run); }
#endif

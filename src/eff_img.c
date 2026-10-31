#include <dlfcn.h>
#include <stdlib.h>

static uint8_t* (*bw_image)(const uint8_t*, int, int*);
static void (*bw_free)(void*);
static int bw_ready;

static void bw_load(void) {
  const char* p;
  void* h;
  if (bw_ready) return;
  bw_ready = 1;
  p = getenv("BENDWALL_NATIVE");
  if (!p || !p[0]) p = "host/libbendwall.so";
  h = dlopen(p, RTLD_NOW);
  if (!h) return;
  bw_image = (uint8_t* (*)(const uint8_t*, int, int*))dlsym(h, "bw_image");
  bw_free = (void (*)(void*))dlsym(h, "bw_free");
}

Term img_rgba_run(Env e, Term* f, IoWork* w) {
  u64 n = 0;
  char* src;
  int out_n = 0;
  uint8_t* dec;
  Term r;
  char empty = 0;
  bw_load();
  src = io_cbuf(e, f[0], &n, CID(Con));
  dec = (bw_image && src) ? bw_image((const uint8_t*)src, (int)n, &out_n) : NULL;
  r = io_list(e, dec ? (char*)dec : &empty, dec ? (u64)out_n : 0);
  free(src);
  if (dec && bw_free) bw_free(dec);
  return r;
}

static void __attribute__((constructor)) img_rgba_use(void) {
  io_eff(CID(Img.rgba), img_rgba_run);
}

#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef BW_TEXT_EFF
#define BW_TEXT_EFF

static void* bw_tx_so;
static int (*bw_tx_ink)(const char*);
static int (*bw_tx_tex)(const char*);

static void bw_text_load(void) {
  const char* p;
  if (bw_tx_so) return;
  p = getenv("BENDWALL_NATIVE");
  if (!p || !p[0]) p = "host/libbendwall.so";
  bw_tx_so = dlopen(p, RTLD_NOW);
  if (!bw_tx_so) return;
  bw_tx_ink = (int (*)(const char*))dlsym(bw_tx_so, "bw_text_ink");
  bw_tx_tex = (int (*)(const char*))dlsym(bw_tx_so, "bw_text_tex");
}

Term text_ink_run(Env e, Term* f, IoWork* w) {
  char* s;
  u64 n = 0;
  int r;
  (void)w;
  bw_text_load();
  s = io_cstr(e, f[0], &n);
  r = (bw_tx_ink && s) ? bw_tx_ink(s) : 0;
  free(s);
  return (Term)(uint32_t)r;
}

Term text_tex_run(Env e, Term* f, IoWork* w) {
  char* s;
  u64 n = 0;
  int r;
  (void)w;
  bw_text_load();
  s = io_cstr(e, f[0], &n);
  r = (bw_tx_tex && s) ? bw_tx_tex(s) : 0;
  free(s);
  return (Term)(uint32_t)r;
}

#endif

#ifdef CID(text_ink)
static void __attribute__((constructor)) text_ink_use(void) { io_eff(CID(text_ink), text_ink_run); }
#endif
#ifdef CID(text_tex)
static void __attribute__((constructor)) text_tex_use(void) { io_eff(CID(text_tex), text_tex_run); }
#endif

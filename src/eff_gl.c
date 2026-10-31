#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef BW_GL_EFF
#define BW_GL_EFF

static void* bw_so;
static int (*bw_init)(int, int);
static int (*bw_tex)(int);
static int (*bw_bind)(int);
static int (*bw_clear)(void);
static int (*bw_rgba)(int, int, const uint8_t*, int);
static int (*bw_prog)(const char*, const char*);
static int (*bw_eye)(int, const char*);
static int (*bw_f1)(int, const char*, uint32_t);
static int (*bw_f2)(int, const char*, uint32_t, uint32_t);
static int (*bw_quad)(int, int, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
static int (*bw_png)(const char*);
static const char* (*bw_log)(void);
static char bw_err[512];

static void bw_load(void) {
  const char* p;
  if (bw_so) return;
  p = getenv("BENDWALL_NATIVE");
  if (!p || !p[0]) p = "host/libbendwall.so";
  bw_so = dlopen(p, RTLD_NOW);
  if (!bw_so) {
    snprintf(bw_err, sizeof bw_err, "dlopen: %s", dlerror());
    return;
  }
  bw_init = dlsym(bw_so, "bw_gl_init");
  bw_tex = dlsym(bw_so, "bw_gl_tex");
  bw_bind = dlsym(bw_so, "bw_gl_bind");
  bw_clear = dlsym(bw_so, "bw_gl_clear");
  bw_rgba = dlsym(bw_so, "bw_gl_rgba");
  bw_prog = dlsym(bw_so, "bw_gl_prog");
  bw_eye = dlsym(bw_so, "bw_gl_eye");
  bw_f1 = dlsym(bw_so, "bw_gl_f1");
  bw_f2 = dlsym(bw_so, "bw_gl_f2");
  bw_quad = dlsym(bw_so, "bw_gl_quad");
  bw_png = dlsym(bw_so, "bw_gl_png");
  bw_log = dlsym(bw_so, "bw_gl_log");
}

static uint32_t u32_of(Term t) { return (uint32_t)t; }

Term gl_init_run(Env e, Term* f, IoWork* w) {
  bw_load();
  (void)e; (void)w;
  return (Term)(uint32_t)(bw_init ? bw_init((int)u32_of(f[0]), (int)u32_of(f[1])) : 0);
}
Term gl_tex_run(Env e, Term* f, IoWork* w) {
  bw_load();
  (void)e; (void)w;
  return (Term)(uint32_t)(bw_tex ? bw_tex((int)u32_of(f[0])) : 0);
}
Term gl_bind_run(Env e, Term* f, IoWork* w) {
  bw_load();
  (void)e; (void)w;
  return (Term)(uint32_t)(bw_bind ? bw_bind((int)u32_of(f[0])) : 0);
}
Term gl_clear_run(Env e, Term* f, IoWork* w) {
  bw_load();
  (void)e; (void)f; (void)w;
  return (Term)(uint32_t)(bw_clear ? bw_clear() : 0);
}
Term gl_rgba_run(Env e, Term* f, IoWork* w) {
  u64 n = 0;
  char* src;
  int r;
  (void)w;
  bw_load();
  src = io_cbuf(e, f[2], &n, CID(Con));
  r = (bw_rgba && src) ? bw_rgba((int)u32_of(f[0]), (int)u32_of(f[1]), (const uint8_t*)src, (int)n) : 0;
  free(src);
  return (Term)(uint32_t)r;
}
Term gl_prog_run(Env e, Term* f, IoWork* w) {
  u64 n1 = 0, n2 = 0;
  char *a, *b;
  int r;
  (void)w;
  bw_load();
  a = io_cstr(e, f[0], &n1);
  b = io_cstr(e, f[1], &n2);
  r = (bw_prog && a && b) ? bw_prog(a, b) : 0;
  free(a);
  free(b);
  return (Term)(uint32_t)r;
}
Term gl_eye_run(Env e, Term* f, IoWork* w) {
  u64 n = 0;
  char* name;
  int r;
  (void)w;
  bw_load();
  name = io_cstr(e, f[1], &n);
  r = (bw_eye && name) ? bw_eye((int)u32_of(f[0]), name) : 0;
  free(name);
  return (Term)(uint32_t)r;
}
Term gl_f1_run(Env e, Term* f, IoWork* w) {
  u64 n = 0;
  char* name;
  int r;
  (void)w;
  bw_load();
  name = io_cstr(e, f[1], &n);
  r = (bw_f1 && name) ? bw_f1((int)u32_of(f[0]), name, u32_of(f[2])) : 0;
  free(name);
  return (Term)(uint32_t)r;
}
Term gl_f2_run(Env e, Term* f, IoWork* w) {
  u64 n = 0;
  char* name;
  int r;
  (void)w;
  bw_load();
  name = io_cstr(e, f[1], &n);
  r = (bw_f2 && name) ? bw_f2((int)u32_of(f[0]), name, u32_of(f[2]), u32_of(f[3])) : 0;
  free(name);
  return (Term)(uint32_t)r;
}
Term gl_quad_run(Env e, Term* f, IoWork* w) {
  (void)e; (void)w;
  bw_load();
  if (!bw_quad) return (Term)0;
  return (Term)(uint32_t)bw_quad((int)u32_of(f[0]), (int)u32_of(f[1]),
    u32_of(f[2]), u32_of(f[3]), u32_of(f[4]), u32_of(f[5]),
    u32_of(f[6]), u32_of(f[7]), u32_of(f[8]), u32_of(f[9]));
}
Term gl_png_run(Env e, Term* f, IoWork* w) {
  u64 n = 0;
  char* path;
  int r;
  (void)w;
  bw_load();
  path = io_cstr(e, f[0], &n);
  r = (bw_png && path) ? bw_png(path) : 0;
  free(path);
  return (Term)(uint32_t)r;
}
Term gl_log_run(Env e, Term* f, IoWork* w) {
  const char* s;
  (void)f; (void)w;
  bw_load();
  s = bw_log ? bw_log() : bw_err;
  if (!s) s = "";
  return io_str(e, (char*)s, (u64)strlen(s));
}

#endif

#ifdef CID(gl_init)
static void __attribute__((constructor)) gl_init_use(void) { io_eff(CID(gl_init), gl_init_run); }
#endif
#ifdef CID(gl_tex)
static void __attribute__((constructor)) gl_tex_use(void) { io_eff(CID(gl_tex), gl_tex_run); }
#endif
#ifdef CID(gl_bind)
static void __attribute__((constructor)) gl_bind_use(void) { io_eff(CID(gl_bind), gl_bind_run); }
#endif
#ifdef CID(gl_clear)
static void __attribute__((constructor)) gl_clear_use(void) { io_eff(CID(gl_clear), gl_clear_run); }
#endif
#ifdef CID(gl_rgba)
static void __attribute__((constructor)) gl_rgba_use(void) { io_eff(CID(gl_rgba), gl_rgba_run); }
#endif
#ifdef CID(gl_prog)
static void __attribute__((constructor)) gl_prog_use(void) { io_eff(CID(gl_prog), gl_prog_run); }
#endif
#ifdef CID(gl_eye)
static void __attribute__((constructor)) gl_eye_use(void) { io_eff(CID(gl_eye), gl_eye_run); }
#endif
#ifdef CID(gl_f1)
static void __attribute__((constructor)) gl_f1_use(void) { io_eff(CID(gl_f1), gl_f1_run); }
#endif
#ifdef CID(gl_f2)
static void __attribute__((constructor)) gl_f2_use(void) { io_eff(CID(gl_f2), gl_f2_run); }
#endif
#ifdef CID(gl_quad)
static void __attribute__((constructor)) gl_quad_use(void) { io_eff(CID(gl_quad), gl_quad_run); }
#endif
#ifdef CID(gl_png)
static void __attribute__((constructor)) gl_png_use(void) { io_eff(CID(gl_png), gl_png_run); }
#endif
#ifdef CID(gl_log)
static void __attribute__((constructor)) gl_log_use(void) { io_eff(CID(gl_log), gl_log_run); }
#endif

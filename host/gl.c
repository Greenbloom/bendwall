/* EGL/OpenGL glue. Bend has no GL bindings (re-checked against Bend 2.0.39:
   no EGL/GL effect in the base library), so this file is the whole GL surface.
   Bend computes pixel rects, NDC bits, translated GLSL and uniforms.
   Coordinates: NDC y-up, y0 is the top edge. Texture (0,0) is the first
   byte of the buffer, which we treat as the top-left of the image. */
#define GL_GLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EGLDisplay g_dpy;
static EGLSurface g_surf;
static EGLContext g_ctx;
static int g_w, g_h, g_ok;
/* 1 when g_dpy came from eglGetPlatformDisplay. Qt's display is never terminated. */
static int g_own_display;
static int g_surfaceless;
static int g_peer_mismatch;
static int g_said;
static char g_renderer[256];
static char g_version[128];
static char g_peer[256];
static PFNEGLCREATEIMAGEKHRPROC g_create_image;
static PFNEGLDESTROYIMAGEKHRPROC g_destroy_image;
static int g_img_tried;

#ifndef EGL_GL_TEXTURE_2D_KHR
#define EGL_GL_TEXTURE_2D_KHR 0x30B5
#endif
#ifndef EGL_IMAGE_PRESERVED_KHR
#define EGL_IMAGE_PRESERVED_KHR 0x30D2
#endif
/* Three finished frames. Qt samples one while the player draws another,
   and a third is free when the sample fence has not signaled yet. */
#define BW_PRESENT_N 3
#define BW_CONS_FENCE 4
typedef struct PresentSlot {
  GLuint tex;
  GLuint fbo;
  EGLImageKHR img;
  EGLSync prod;
  EGLSync cons[BW_CONS_FENCE];
  EGLDisplay cons_dpy[BW_CONS_FENCE];
  int ncons;
} PresentSlot;
static PresentSlot g_slot[BW_PRESENT_N];
static int g_write = -1;
static int g_pub = -1;
static int g_sampling = -1;
static int g_next = 0;
static int g_epoch = 1;
static GLuint g_scene_tex, g_scene_fbo;
static GLuint g_vao, g_vbo;
static char g_log[8192];
static int g_logn;

static void blog(const char* s) {
  size_t n;
  if (!s) return;
  n = strlen(s);
  if (g_logn + (int)n + 2 >= (int)sizeof g_log) return;
  memcpy(g_log + g_logn, s, n);
  g_logn += (int)n;
  g_log[g_logn++] = '\n';
  g_log[g_logn] = 0;
}

static float fbits(uint32_t u) {
  float f;
  memcpy(&f, &u, 4);
  return f;
}

static uint32_t ubits(float f) {
  uint32_t u;
  memcpy(&u, &f, 4);
  return u;
}

int bw_gl_prog(const char* vs, const char* fs);
int bw_gl_bind(int which);
int bw_gl_clear(void);
int bw_gl_image(int tex, int w, int h, const uint8_t* px, int n);
int bw_gl_fvn(int prog, const char* name, const float* v, int count);
int bw_gl_read(uint8_t* dst, int n);
int bw_gl_begin(void);
int bw_gl_end(void);
void bw_gl_shutdown(void);
int bw_gl_quad(int prog, int tex,
    uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
    uint32_t u0, uint32_t v0, uint32_t u1, uint32_t v1);

static EGLDisplay g_save_dpy;
static EGLContext g_save_ctx;
static EGLSurface g_save_draw, g_save_read;
static int g_saved;

static int make_color(GLuint* tex, GLuint* fbo) {
  glGenTextures(1, tex);
  glBindTexture(GL_TEXTURE_2D, *tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_w, g_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1, fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    blog("fbo incomplete");
    return 0;
  }
  return 1;
}

static void destroy_sync(EGLDisplay dpy, EGLSync sync) {
  if (sync == EGL_NO_SYNC || dpy == EGL_NO_DISPLAY) return;
  eglDestroySync(dpy, sync);
}

static void release_present(void) {
  int i, k;
  for (i = 0; i < BW_PRESENT_N; i++) {
    PresentSlot* s = &g_slot[i];
    if (s->img != EGL_NO_IMAGE_KHR && g_destroy_image && g_dpy != EGL_NO_DISPLAY)
      g_destroy_image(g_dpy, s->img);
    destroy_sync(g_dpy, s->prod);
    for (k = 0; k < s->ncons; k++) destroy_sync(s->cons_dpy[k], s->cons[k]);
    if (s->fbo) glDeleteFramebuffers(1, &s->fbo);
    if (s->tex) glDeleteTextures(1, &s->tex);
    memset(s, 0, sizeof *s);
  }
  if (g_scene_fbo) glDeleteFramebuffers(1, &g_scene_fbo);
  if (g_scene_tex) glDeleteTextures(1, &g_scene_tex);
  g_scene_fbo = g_scene_tex = 0;
  g_write = g_pub = g_sampling = -1;
  g_next = 0;
  g_epoch++;
}

/* The caller's EGL context, saved around a probe. A missing context stays
   missing: headless tools keep the player context current when they return. */
typedef struct EglHold {
  EGLDisplay dpy;
  EGLContext ctx;
  EGLSurface draw, read;
} EglHold;

static void egl_hold(EglHold* h) {
  h->dpy = eglGetCurrentDisplay();
  h->ctx = eglGetCurrentContext();
  h->draw = eglGetCurrentSurface(EGL_DRAW);
  h->read = eglGetCurrentSurface(EGL_READ);
}

void bw_gl_shutdown(void) {
  EglHold held;
  EGLContext ours = g_ctx;
  EGLDisplay doomed = EGL_NO_DISPLAY;
  egl_hold(&held);
  if (g_ok && g_dpy != EGL_NO_DISPLAY) {
    eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx);
    release_present();
    if (g_vbo) glDeleteBuffers(1, &g_vbo);
    if (g_vao) glDeleteVertexArrays(1, &g_vao);
    eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (g_ctx != EGL_NO_CONTEXT) eglDestroyContext(g_dpy, g_ctx);
    if (g_surf != EGL_NO_SURFACE) eglDestroySurface(g_dpy, g_surf);
  } else {
    g_write = g_pub = g_sampling = -1;
    g_next = 0;
  }
  g_ctx = EGL_NO_CONTEXT;
  g_surf = EGL_NO_SURFACE;
  g_ok = 0;
  g_vao = g_vbo = 0;
  g_img_tried = 0;
  g_create_image = NULL;
  g_destroy_image = NULL;
  /* Only a display this process opened with eglGetPlatformDisplay is ours.
     Terminating EGL_DEFAULT_DISPLAY would take plasmashell's EGL down too. */
  if (g_own_display && g_dpy != EGL_NO_DISPLAY) {
    doomed = g_dpy;
    eglTerminate(g_dpy);
  }
  if (g_own_display) g_dpy = EGL_NO_DISPLAY;
  g_own_display = 0;
  g_surfaceless = 0;
  if (held.ctx != EGL_NO_CONTEXT && held.ctx != ours
      && held.dpy != EGL_NO_DISPLAY && held.dpy != doomed)
    eglMakeCurrent(held.dpy, held.draw, held.read, held.ctx);
}

int bw_gl_begin(void) {
  if (!g_ok) return 0;
  g_save_dpy = eglGetCurrentDisplay();
  g_save_ctx = eglGetCurrentContext();
  g_save_draw = eglGetCurrentSurface(EGL_DRAW);
  g_save_read = eglGetCurrentSurface(EGL_READ);
  g_saved = 1;
  if (g_save_ctx == g_ctx) return 1;
  if (!eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) { blog("make current"); return 0; }
  return 1;
}

int bw_gl_end(void) {
  if (!g_saved) return 1;
  g_saved = 0;
  if (g_save_ctx == EGL_NO_CONTEXT || g_save_ctx == g_ctx) return 1;
  if (g_save_dpy == EGL_NO_DISPLAY) return 1;
  return eglMakeCurrent(g_save_dpy, g_save_draw, g_save_read, g_save_ctx) ? 1 : 0;
}

static void egl_back(const EglHold* h) {
  if (!h || h->ctx == EGL_NO_CONTEXT || h->dpy == EGL_NO_DISPLAY) return;
  if (h->ctx == g_ctx) return;
  eglMakeCurrent(h->dpy, h->draw, h->read, h->ctx);
}

static int ext_has(const char* list, const char* name) {
  size_t n;
  const char* p;
  if (!list || !name) return 0;
  n = strlen(name);
  for (p = list; (p = strstr(p, name)) != NULL; p += n) {
    if ((p == list || p[-1] == ' ') && (p[n] == 0 || p[n] == ' ')) return 1;
  }
  return 0;
}

static int software_name(const char* r) {
  if (!r || !r[0]) return 1;
  if (strstr(r, "llvmpipe") || strstr(r, "softpipe") || strstr(r, "swrast")
      || strstr(r, "lavapipe") || strstr(r, "Software Rasterizer"))
    return 1;
  return 0;
}

static int vendor_match(const char* a, const char* b) {
  if (!a || !b || !a[0] || !b[0]) return 0;
  if (software_name(a) && software_name(b)) return 1;
  if (strstr(a, "NVIDIA") && strstr(b, "NVIDIA")) return 1;
  if ((strstr(a, "Intel") || strstr(a, "Iris")) && (strstr(b, "Intel") || strstr(b, "Iris")))
    return 1;
  if ((strstr(a, "Radeon") || strstr(a, "AMD") || strstr(a, "radeonsi"))
      && (strstr(b, "Radeon") || strstr(b, "AMD") || strstr(b, "radeonsi")))
    return 1;
  if (strstr(a, b) || strstr(b, a)) return 1;
  return 0;
}

static int software_forced(void) {
  const char* e = getenv("LIBGL_ALWAYS_SOFTWARE");
  return e && e[0] == '1' && e[1] == 0;
}

/* Mesa exports this only through eglGetProcAddress, not as a linkable symbol. */
static PFNEGLQUERYDEVICESEXTPROC query_devices(void) {
  static PFNEGLQUERYDEVICESEXTPROC fn;
  static int tried;
  if (!tried) {
    tried = 1;
    fn = (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
  }
  return fn;
}

static EGLenum api_hold(void) { return eglQueryAPI(); }

static void api_back(EGLenum api) {
  if (api == EGL_NONE) return;
  eglBindAPI(api);
}

struct DispCand {
  EGLDisplay dpy;
  int own;
  int software;
  int surfaceless;
  char rend[256];
  char ver[128];
};

/* A throwaway context on one display. The display stays initialized.
   surfaceless is set when the device has no pbuffer. */
static int probe_renderer(EGLDisplay dpy, char* rend, int rcap, char* ver, int vcap, int* surfaceless) {
  EGLConfig cfg = NULL;
  EGLSurface surf = EGL_NO_SURFACE;
  EGLContext ctx = EGL_NO_CONTEXT;
  EGLint n = 0, major = 0, minor = 0;
  EGLint attr[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
    EGL_NONE
  };
  EGLint loose[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE};
  EGLint pbuf[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
  EGLint ctxattr[] = {
    EGL_CONTEXT_MAJOR_VERSION, 3,
    EGL_CONTEXT_MINOR_VERSION, 3,
    EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
    EGL_NONE
  };
  const char* r;
  const char* v;
  int ok = 0;
  if (surfaceless) *surfaceless = 0;
  if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &major, &minor)) return 0;
  if (!eglBindAPI(EGL_OPENGL_API)) return 0;
  if (!eglChooseConfig(dpy, attr, &cfg, 1, &n) || n < 1) {
    if (surfaceless) *surfaceless = 1;
    if (!eglChooseConfig(dpy, loose, &cfg, 1, &n) || n < 1) return 0;
  }
  if (!surfaceless || !*surfaceless) surf = eglCreatePbufferSurface(dpy, cfg, pbuf);
  if (surf == EGL_NO_SURFACE && surfaceless) *surfaceless = 1;
  ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxattr);
  if (ctx == EGL_NO_CONTEXT) ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, NULL);
  if (ctx == EGL_NO_CONTEXT) {
    if (surf != EGL_NO_SURFACE) eglDestroySurface(dpy, surf);
    return 0;
  }
  if (!eglMakeCurrent(dpy, surf, surf, ctx)) {
    eglDestroyContext(dpy, ctx);
    if (surf != EGL_NO_SURFACE) eglDestroySurface(dpy, surf);
    return 0;
  }
  r = (const char*)glGetString(GL_RENDERER);
  v = (const char*)glGetString(GL_VERSION);
  if (r && v && rcap > 1 && vcap > 1) {
    snprintf(rend, (size_t)rcap, "%s", r);
    snprintf(ver, (size_t)vcap, "%s", v);
    ok = rend[0] != 0;
  }
  eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(dpy, ctx);
  if (surf != EGL_NO_SURFACE) eglDestroySurface(dpy, surf);
  return ok;
}

/* Prefer a hardware device that matches the context already current on this
   thread (Qt's). Never setenv, and never eglTerminate a display we did not
   open. Software-only hosts, including LIBGL_ALWAYS_SOFTWARE=1, keep
   EGL_DEFAULT_DISPLAY so an EGLImage still imports there. */
static int select_display(void) {
  struct DispCand c[9];
  EGLDeviceEXT devs[8];
  EglHold held;
  EGLint nd = 0;
  int n = 0, i, pick = -1, defi = -1, any_hw = 0;
  const char* ext;
  PFNEGLQUERYDEVICESEXTPROC qdev;
  EGLenum api;
  egl_hold(&held);
  api = api_hold();
  g_peer[0] = 0;
  g_peer_mismatch = 0;
  if (held.ctx != EGL_NO_CONTEXT) {
    const char* r = (const char*)glGetString(GL_RENDERER);
    if (r) snprintf(g_peer, sizeof g_peer, "%s", r);
  }
  ext = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
  qdev = query_devices();
  if (!software_forced()
      && qdev
      && ext_has(ext, "EGL_EXT_device_enumeration")
      && ext_has(ext, "EGL_EXT_platform_device")
      && qdev(8, devs, &nd) && nd > 0) {
    if (nd > 8) nd = 8;
    for (i = 0; i < (int)nd && n < 8; i++) {
      EGLDisplay d = eglGetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, (void*)devs[i], NULL);
      int sl = 0;
      int foreign;
      if (d == EGL_NO_DISPLAY) continue;
      foreign = held.dpy != EGL_NO_DISPLAY && d == held.dpy;
      if (!probe_renderer(d, c[n].rend, (int)sizeof c[n].rend, c[n].ver, (int)sizeof c[n].ver, &sl)) {
        if (!foreign) eglTerminate(d);
        continue;
      }
      c[n].dpy = d;
      c[n].own = foreign ? 0 : 1;
      c[n].surfaceless = sl;
      c[n].software = software_name(c[n].rend);
      if (!c[n].software) any_hw = 1;
      n++;
    }
  }
  {
    EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    int sl = 0, seen = -1;
    for (i = 0; i < n; i++)
      if (c[i].dpy == d) seen = i;
    if (seen >= 0) {
      defi = seen;
      c[seen].own = 0;
    } else if (d != EGL_NO_DISPLAY
               && probe_renderer(d, c[n].rend, (int)sizeof c[n].rend, c[n].ver, (int)sizeof c[n].ver, &sl)) {
      c[n].dpy = d;
      c[n].own = 0;
      c[n].surfaceless = sl;
      c[n].software = software_name(c[n].rend);
      if (!c[n].software) any_hw = 1;
      defi = n;
      n++;
    }
  }
  if (g_peer[0]) {
    for (i = 0; i < n; i++)
      if (vendor_match(c[i].rend, g_peer)) {
        pick = i;
        break;
      }
  }
  if (pick < 0) {
    for (i = 0; i < n; i++)
      if (!c[i].software) {
        pick = i;
        break;
      }
  }
  /* No hardware device: stay on the default display. A second llvmpipe
     display will not import the image the tests sample. */
  if (!any_hw && defi >= 0) pick = defi;
  if (pick < 0 && defi >= 0) pick = defi;
  if (pick < 0 && n > 0) pick = 0;
  for (i = 0; i < n; i++) {
    if (i == pick || !c[i].own) continue;
    if (c[i].dpy != EGL_NO_DISPLAY) eglTerminate(c[i].dpy);
  }
  if (pick < 0) {
    egl_back(&held);
    api_back(api);
    return 0;
  }
  g_dpy = c[pick].dpy;
  g_own_display = c[pick].own;
  g_surfaceless = c[pick].surfaceless;
  snprintf(g_renderer, sizeof g_renderer, "%s", c[pick].rend);
  snprintf(g_version, sizeof g_version, "%s", c[pick].ver);
  if (g_peer[0] && !vendor_match(g_renderer, g_peer)) g_peer_mismatch = 1;
  egl_back(&held);
  api_back(api);
  return 1;
}

int bw_gl_vendor_match(const char* peer) {
  if (!peer || !peer[0] || !g_renderer[0]) return 1;
  return vendor_match(g_renderer, peer);
}

int bw_gl_peer_mismatch(void) { return g_peer_mismatch; }

const char* bw_gl_renderer(void) { return g_renderer; }

int bw_gl_init(int w, int h) {
  EglHold held;
  EGLConfig cfg = NULL;
  EGLint n = 0;
  EGLint attr[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
    EGL_NONE
  };
  EGLint loose[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE};
  EGLint pbuf[] = {EGL_WIDTH, w, EGL_HEIGHT, h, EGL_NONE};
  EGLint ctx_attr[] = {
    EGL_CONTEXT_MAJOR_VERSION, 3,
    EGL_CONTEXT_MINOR_VERSION, 3,
    EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
    EGL_NONE
  };
  const char* rend;
  const char* ver;
  EGLenum api;
  int rc = 0;
  if (g_ok) bw_gl_shutdown();
  egl_hold(&held);
  api = api_hold();
  g_logn = 0;
  g_log[0] = 0;
  g_w = w;
  g_h = h;
  g_peer_mismatch = 0;
  if (g_dpy == EGL_NO_DISPLAY) {
    if (!select_display()) {
      blog("no display");
      goto done;
    }
  }
  if (!eglBindAPI(EGL_OPENGL_API)) {
    blog("bind api");
    goto done;
  }
  if (g_surfaceless || !eglChooseConfig(g_dpy, attr, &cfg, 1, &n) || n < 1) {
    g_surfaceless = 1;
    n = 0;
    if (!eglChooseConfig(g_dpy, loose, &cfg, 1, &n) || n < 1) {
      blog("no config");
      goto done;
    }
  }
  g_surf = EGL_NO_SURFACE;
  if (!g_surfaceless) {
    g_surf = eglCreatePbufferSurface(g_dpy, cfg, pbuf);
    if (g_surf == EGL_NO_SURFACE) g_surfaceless = 1;
  }
  g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
  if (g_ctx == EGL_NO_CONTEXT) {
    blog("core 3.3 failed");
    g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, NULL);
  }
  if (g_ctx == EGL_NO_CONTEXT) {
    blog("no context");
    goto done;
  }
  if (!eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) {
    blog("make current");
    goto done;
  }
  if (!make_color(&g_slot[0].tex, &g_slot[0].fbo)
      || !make_color(&g_slot[1].tex, &g_slot[1].fbo)
      || !make_color(&g_slot[2].tex, &g_slot[2].fbo)
      || !make_color(&g_scene_tex, &g_scene_fbo)) {
    blog("fbo");
    goto done;
  }
  g_write = -1;
  g_pub = -1;
  g_sampling = -1;
  g_next = 0;
  glGenVertexArrays(1, &g_vao);
  glGenBuffers(1, &g_vbo);
  glBindVertexArray(g_vao);
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * (GLsizei)sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * (GLsizei)sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glViewport(0, 0, w, h);
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_DITHER);
  rend = (const char*)glGetString(GL_RENDERER);
  ver = (const char*)glGetString(GL_VERSION);
  if (rend && rend[0]) snprintf(g_renderer, sizeof g_renderer, "%s", rend);
  if (ver && ver[0]) snprintf(g_version, sizeof g_version, "%s", ver);
  if (g_peer[0] && g_renderer[0]) g_peer_mismatch = !vendor_match(g_renderer, g_peer);
  blog(g_renderer);
  blog(g_version);
  if (!g_said) {
    fprintf(stderr, "bendwall: renderer %s | %s\n",
            g_renderer[0] ? g_renderer : "unknown",
            g_version[0] ? g_version : "unknown");
    g_said = 1;
  }
  g_ok = 1;
  rc = 1;
done:
  if (!rc) {
    if (g_dpy != EGL_NO_DISPLAY && g_ctx != EGL_NO_CONTEXT)
      eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx);
    release_present();
    if (g_dpy != EGL_NO_DISPLAY && (g_ctx != EGL_NO_CONTEXT || g_surf != EGL_NO_SURFACE))
      eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (g_dpy != EGL_NO_DISPLAY && g_ctx != EGL_NO_CONTEXT) eglDestroyContext(g_dpy, g_ctx);
    if (g_dpy != EGL_NO_DISPLAY && g_surf != EGL_NO_SURFACE) eglDestroySurface(g_dpy, g_surf);
    g_ctx = EGL_NO_CONTEXT;
    g_surf = EGL_NO_SURFACE;
    g_vao = g_vbo = 0;
    g_ok = 0;
  }
  egl_back(&held);
  api_back(api);
  return rc;
}

/* present_end finishes the sample before it drops the player lock, so this
   fence is already signaled. A wait that cannot see the consumer display is
   dropped instead of stalling the next frame. */
static void drain_consumer(PresentSlot* s) {
  int k;
  if (!s || s->ncons <= 0) return;
  for (k = 0; k < s->ncons; k++) {
    if (s->cons[k] != EGL_NO_SYNC && s->cons_dpy[k] != EGL_NO_DISPLAY)
      eglClientWaitSync(s->cons_dpy[k], s->cons[k], EGL_SYNC_FLUSH_COMMANDS_BIT, 0);
    destroy_sync(s->cons_dpy[k], s->cons[k]);
    s->cons[k] = EGL_NO_SYNC;
    s->cons_dpy[k] = EGL_NO_DISPLAY;
  }
  s->ncons = 0;
}

static int claim_write(int slot) {
  destroy_sync(g_dpy, g_slot[slot].prod);
  g_slot[slot].prod = EGL_NO_SYNC;
  g_write = slot;
  g_next = (slot + 1) % BW_PRESENT_N;
  return slot;
}

/* Rotate through the slots. The published frame stays put until every other
   slot is busy, so a clear cannot land on the texture Qt is sampling. */
static int pick_write(void) {
  int n, slot;
  for (n = 0; n < BW_PRESENT_N; n++) {
    slot = (g_next + n) % BW_PRESENT_N;
    if (!g_slot[slot].fbo || slot == g_pub) continue;
    drain_consumer(&g_slot[slot]);
    return claim_write(slot);
  }
  if (g_pub >= 0 && g_slot[g_pub].fbo) {
    drain_consumer(&g_slot[g_pub]);
    slot = g_pub;
    g_pub = -1;
    return claim_write(slot);
  }
  return -1;
}

int bw_gl_tex(int which) {
  int slot;
  if (which == 1) return (int)g_scene_tex;
  if (which != 0) return 0;
  slot = g_write >= 0 ? g_write : 0;
  return (int)g_slot[slot].tex;
}

int bw_gl_bind(int which) {
  GLuint fbo;
  if (!g_ok || which < 0 || which > 1) return 0;
  if (which == 1) {
    fbo = g_scene_fbo;
  } else {
    if (g_write < 0 && pick_write() < 0) {
      static int said;
      if (!said) {
        fprintf(stderr, "bendwall: present slot busy\n");
        said = 1;
      }
      return 0;
    }
    fbo = g_slot[g_write].fbo;
  }
  if (!fbo) return 0;
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, g_w, g_h);
  return 1;
}

/* The frame Qt is sampling, not the one about to be cleared. */
int bw_gl_bind_shown(void) {
  int slot;
  if (!g_ok) return 0;
  slot = g_pub >= 0 ? g_pub : g_write;
  if (slot < 0 || !g_slot[slot].fbo) return bw_gl_bind(0);
  glBindFramebuffer(GL_FRAMEBUFFER, g_slot[slot].fbo);
  glViewport(0, 0, g_w, g_h);
  return 1;
}

int bw_gl_clear(void) {
  if (!g_ok) return 0;
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  return 1;
}

int bw_gl_rgba(int w, int h, const uint8_t* px, int n) {
  return bw_gl_image(0, w, h, px, n);
}

int bw_gl_image(int tex, int w, int h, const uint8_t* px, int n) {
  GLuint t;
  if (!g_ok || !px || n < w * h * 4 || w < 1 || h < 1) return 0;
  if (tex <= 0) {
    glGenTextures(1, &t);
    tex = (int)t;
  }
  glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
  return tex;
}

int bw_gl_plane(int tex, int w, int h, const uint8_t* px, int comps) {
  GLuint t;
  GLenum fmt, internal;
  if (!g_ok || !px || w < 1 || h < 1 || (comps != 1 && comps != 2)) return 0;
  fmt = comps == 2 ? GL_RG : GL_RED;
  internal = comps == 2 ? GL_RG8 : GL_R8;
  if (tex <= 0) {
    glGenTextures(1, &t);
    tex = (int)t;
  }
  glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internal, w, h, 0, fmt, GL_UNSIGNED_BYTE, px);
  return tex;
}

int bw_gl_i1(int prog, const char* name, int v) {
  GLint loc;
  if (!g_ok || prog <= 0 || !name) return 0;
  glUseProgram((GLuint)prog);
  loc = glGetUniformLocation((GLuint)prog, name);
  if (loc < 0) return 0;
  glUniform1i(loc, v);
  return 1;
}

int bw_gl_quad_planes(int prog, const int* tex, int ntex,
    uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
    uint32_t u0, uint32_t v0, uint32_t u1, uint32_t v1) {
  float v[20];
  float X0 = fbits(x0), Y0 = fbits(y0), X1 = fbits(x1), Y1 = fbits(y1);
  float U0 = fbits(u0), V0 = fbits(v0), U1 = fbits(u1), V1 = fbits(v1);
  int i;
  const char* names[3] = {"g_Texture0", "g_Tex1", "g_Tex2"};
  if (!g_ok || prog <= 0 || !tex || ntex < 1) return 0;
  v[0] = X0; v[1] = Y0; v[2] = 0; v[3] = U0; v[4] = V0;
  v[5] = X1; v[6] = Y0; v[7] = 0; v[8] = U1; v[9] = V0;
  v[10] = X0; v[11] = Y1; v[12] = 0; v[13] = U0; v[14] = V1;
  v[15] = X1; v[16] = Y1; v[17] = 0; v[18] = U1; v[19] = V1;
  glUseProgram((GLuint)prog);
  for (i = 0; i < ntex && i < 3; i++) {
    GLint loc;
    if (tex[i] <= 0) return 0;
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex[i]);
    loc = glGetUniformLocation((GLuint)prog, names[i]);
    if (loc >= 0) glUniform1i(loc, i);
  }
  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(g_vao);
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  glBufferData(GL_ARRAY_BUFFER, 20 * (int)sizeof(float), v, GL_DYNAMIC_DRAW);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  return 1;
}

static GLuint compile_one(GLenum type, const char* src) {
  GLuint s;
  GLint ok = 0;
  char buf[4096];
  s = glCreateShader(type);
  glShaderSource(s, 1, &src, NULL);
  glCompileShader(s);
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    glGetShaderInfoLog(s, sizeof buf, NULL, buf);
    blog(type == GL_VERTEX_SHADER ? "vert" : "frag");
    blog(buf);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

int bw_gl_prog(const char* vs, const char* fs) {
  GLuint v, f, p;
  GLint ok = 0;
  char buf[4096];
  if (!g_ok || !vs || !fs) return 0;
  v = compile_one(GL_VERTEX_SHADER, vs);
  f = compile_one(GL_FRAGMENT_SHADER, fs);
  if (!v || !f) return 0;
  p = glCreateProgram();
  glAttachShader(p, v);
  glAttachShader(p, f);
  glBindAttribLocation(p, 0, "a_Position");
  glBindAttribLocation(p, 1, "a_TexCoord");
  glLinkProgram(p);
  glDeleteShader(v);
  glDeleteShader(f);
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    glGetProgramInfoLog(p, sizeof buf, NULL, buf);
    blog("link");
    blog(buf);
    glDeleteProgram(p);
    return 0;
  }
  return (int)p;
}

int bw_gl_eye(int prog, const char* name) {
  GLint loc;
  static const float id[16] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
  };
  if (!g_ok || prog <= 0 || !name) return 0;
  glUseProgram((GLuint)prog);
  loc = glGetUniformLocation((GLuint)prog, name);
  if (loc < 0) { blog("missing eye uniform"); blog(name); return 0; }
  glUniformMatrix4fv(loc, 1, GL_FALSE, id);
  return 1;
}

int bw_gl_f1(int prog, const char* name, uint32_t bits) {
  GLint loc;
  if (!g_ok || prog <= 0 || !name) return 0;
  glUseProgram((GLuint)prog);
  loc = glGetUniformLocation((GLuint)prog, name);
  if (loc < 0) { blog("missing f1"); blog(name); return 0; }
  glUniform1f(loc, fbits(bits));
  return 1;
}

int bw_gl_fv(int prog, const char* name, uint32_t b0, uint32_t b1, uint32_t b2, uint32_t b3) {
  GLint loc;
  float v[4];
  if (!g_ok || prog <= 0 || !name) return 0;
  glUseProgram((GLuint)prog);
  loc = glGetUniformLocation((GLuint)prog, name);
  if (loc < 0) { blog("missing fv"); blog(name); return 0; }
  v[0] = fbits(b0);
  v[1] = fbits(b1);
  v[2] = fbits(b2);
  v[3] = fbits(b3);
  glUniform1fv(loc, 4, v);
  return 1;
}

int bw_gl_fvn(int prog, const char* name, const float* v, int count) {
  GLint loc;
  if (!g_ok || prog <= 0 || !name || !v || count < 1) return 0;
  glUseProgram((GLuint)prog);
  loc = glGetUniformLocation((GLuint)prog, name);
  if (loc < 0) { blog("missing fvn"); blog(name); return 0; }
  glUniform1fv(loc, count, v);
  return 1;
}

static int ensure_image_procs(void) {
  const char* ext;
  if (g_img_tried) return g_create_image && g_destroy_image;
  g_img_tried = 1;
  ext = eglQueryString(g_dpy, EGL_EXTENSIONS);
  if (!ext || !strstr(ext, "EGL_KHR_image")) return 0;
  g_create_image = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  g_destroy_image = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  return g_create_image && g_destroy_image;
}

int bw_gl_export_color(void) {
  PresentSlot* s;
  EGLint attr[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
  if (!g_ok || g_write < 0 || !g_slot[g_write].tex || !ensure_image_procs()) return 0;
  s = &g_slot[g_write];
  /* One image per slot. Later frames rewrite the texels; the fence below
     is what makes those texels visible to Qt's context. */
  if (s->img == EGL_NO_IMAGE_KHR) {
    s->img = g_create_image(g_dpy, g_ctx, EGL_GL_TEXTURE_2D_KHR,
                            (EGLClientBuffer)(uintptr_t)s->tex, attr);
    if (s->img == EGL_NO_IMAGE_KHR) return 0;
  }
  destroy_sync(g_dpy, s->prod);
  s->prod = eglCreateSync(g_dpy, EGL_SYNC_FENCE, NULL);
  if (s->prod == EGL_NO_SYNC) glFinish();
  else glFlush();
  g_pub = g_write;
  g_write = -1;
  return 1;
}

void* bw_gl_color_image(void) {
  if (g_pub < 0 || g_slot[g_pub].img == EGL_NO_IMAGE_KHR) return NULL;
  return (void*)g_slot[g_pub].img;
}

int bw_gl_present_epoch(void) { return g_epoch; }

/* Consumer thread, Qt's context current. Blocks until this frame's draws
   have finished on the player context, then returns that frame's image. */
void* bw_gl_present_begin(void) {
  PresentSlot* s;
  if (g_pub < 0) return NULL;
  s = &g_slot[g_pub];
  if (s->img == EGL_NO_IMAGE_KHR) return NULL;
  if (s->prod != EGL_NO_SYNC) {
    EGLint st = eglClientWaitSync(g_dpy, s->prod, EGL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ull);
    if (st == EGL_FALSE || st == EGL_TIMEOUT_EXPIRED) return NULL;
  }
  g_sampling = g_pub;
  return (void*)s->img;
}

/* After the consumer has queued its sample. glFinish runs before this thread
   drops the player lock, so the next step cannot clear a texture this draw
   is still reading. The fence is what a later reuse waits on if finish is
   ever removed. Two screens each append one. */
void bw_gl_present_end(void) {
  PresentSlot* s;
  EGLDisplay dpy;
  EGLSync sync;
  if (g_sampling < 0) return;
  s = &g_slot[g_sampling];
  g_sampling = -1;
  dpy = eglGetCurrentDisplay();
  sync = EGL_NO_SYNC;
  if (dpy != EGL_NO_DISPLAY && s->ncons < BW_CONS_FENCE)
    sync = eglCreateSync(dpy, EGL_SYNC_FENCE, NULL);
  if (sync != EGL_NO_SYNC) {
    s->cons[s->ncons] = sync;
    s->cons_dpy[s->ncons] = dpy;
    s->ncons++;
  }
  if (eglGetCurrentContext() != EGL_NO_CONTEXT) glFinish();
}

int bw_gl_read(uint8_t* dst, int n) {
  uint8_t* raw;
  int y, stride;
  if (!g_ok || !dst) return 0;
  stride = g_w * 4;
  if (n < stride * g_h) return 0;
  raw = (uint8_t*)malloc((size_t)stride * (size_t)g_h);
  if (!raw) return 0;
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, g_w, g_h, GL_RGBA, GL_UNSIGNED_BYTE, raw);
  for (y = 0; y < g_h; y++)
    memcpy(dst + (size_t)y * (size_t)stride, raw + (size_t)(g_h - 1 - y) * (size_t)stride, (size_t)stride);
  free(raw);
  return stride * g_h;
}

int bw_gl_px(int tex, int x, int y) {
  static const char* vs =
    "#version 330 core\n"
    "layout(location=0) in vec3 a_Position;\n"
    "layout(location=1) in vec2 a_TexCoord;\n"
    "out vec2 v;\n"
    "void main(){ v = a_TexCoord; gl_Position = vec4(a_Position, 1.0); }\n";
  static const char* fs =
    "#version 330 core\n"
    "in vec2 v;\n"
    "uniform sampler2D g_Texture0;\n"
    "out vec4 fragColor;\n"
    "void main(){ fragColor = texture(g_Texture0, v); }\n";
  static GLuint prog = 0;
  uint8_t px[4];
  uint32_t n1 = ubits(-1.f), p1 = ubits(1.f), z = ubits(0.f), o = ubits(1.f);
  if (!g_ok || tex <= 0) return 0;
  if (!prog) {
    prog = (GLuint)bw_gl_prog(vs, fs);
    if (!prog) return 0;
  }
  if (!bw_gl_bind(0)) return 0;
  if (!bw_gl_clear()) return 0;
  if (!bw_gl_quad((int)prog, tex, n1, p1, p1, n1, z, z, o, o)) return 0;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x >= g_w) x = g_w - 1;
  if (y >= g_h) y = g_h - 1;
  if (g_write >= 0 && g_slot[g_write].fbo)
    glBindFramebuffer(GL_FRAMEBUFFER, g_slot[g_write].fbo);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
  return (int)(px[0] | (px[1] << 8) | (px[2] << 16) | (px[3] << 24));
}

int bw_gl_f2(int prog, const char* name, uint32_t a, uint32_t b) {
  GLint loc;
  if (!g_ok || prog <= 0 || !name) return 0;
  glUseProgram((GLuint)prog);
  loc = glGetUniformLocation((GLuint)prog, name);
  if (loc < 0) { blog("missing f2"); blog(name); return 0; }
  glUniform2f(loc, fbits(a), fbits(b));
  return 1;
}

int bw_gl_verts(int prog, int tex, const float* v) {
  GLint loc;
  if (!g_ok || prog <= 0 || tex <= 0 || !v) return 0;
  glUseProgram((GLuint)prog);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
  loc = glGetUniformLocation((GLuint)prog, "g_Texture0");
  if (loc >= 0) glUniform1i(loc, 0);
  glBindVertexArray(g_vao);
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  glBufferData(GL_ARRAY_BUFFER, 20 * (int)sizeof(float), v, GL_DYNAMIC_DRAW);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  return 1;
}

int bw_gl_quad(int prog, int tex,
    uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
    uint32_t u0, uint32_t v0, uint32_t u1, uint32_t v1) {
  float v[20];
  float X0 = fbits(x0), Y0 = fbits(y0), X1 = fbits(x1), Y1 = fbits(y1);
  float U0 = fbits(u0), V0 = fbits(v0), U1 = fbits(u1), V1 = fbits(v1);
  v[0] = X0; v[1] = Y0; v[2] = 0; v[3] = U0; v[4] = V0;
  v[5] = X1; v[6] = Y0; v[7] = 0; v[8] = U1; v[9] = V0;
  v[10] = X0; v[11] = Y1; v[12] = 0; v[13] = U0; v[14] = V1;
  v[15] = X1; v[16] = Y1; v[17] = 0; v[18] = U1; v[19] = V1;
  return bw_gl_verts(prog, tex, v);
}

int bw_gl_png(const char* path) {
  uint8_t* raw;
  uint8_t* flip;
  png_image img;
  int y, stride, ok;
  if (!g_ok || !path) return 0;
  stride = g_w * 4;
  raw = (uint8_t*)malloc((size_t)stride * (size_t)g_h);
  flip = (uint8_t*)malloc((size_t)stride * (size_t)g_h);
  if (!raw || !flip) { free(raw); free(flip); return 0; }
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, g_w, g_h, GL_RGBA, GL_UNSIGNED_BYTE, raw);
  for (y = 0; y < g_h; y++)
    memcpy(flip + (size_t)y * (size_t)stride, raw + (size_t)(g_h - 1 - y) * (size_t)stride, (size_t)stride);
  memset(&img, 0, sizeof img);
  img.version = PNG_IMAGE_VERSION;
  img.width = (png_uint_32)g_w;
  img.height = (png_uint_32)g_h;
  img.format = PNG_FORMAT_RGBA;
  ok = png_image_write_to_file(&img, path, 0, flip, stride, NULL);
  free(raw);
  free(flip);
  if (!ok) { blog(img.message); return 0; }
  return 1;
}

const char* bw_gl_log(void) { return g_log; }

int bw_gl_ok(void) { return g_ok; }

/* EGL/OpenGL glue. Bend has no GL bindings (re-checked against Bend 2.0.39:
   no EGL/GL effect in the base library), so this file is the whole GL surface.
   Bend computes pixel rects, NDC bits, translated GLSL and uniforms.
   Coordinates: NDC y-up, y0 is the top edge. Texture (0,0) is the first
   byte of the buffer, which we treat as the top-left of the image. */
#define GL_GLEXT_PROTOTYPES 1
#include <EGL/egl.h>
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
static GLuint g_fbo[2], g_ctex[2], g_vao, g_vbo;
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

static int make_target(int i) {
  glGenTextures(1, &g_ctex[i]);
  glBindTexture(GL_TEXTURE_2D, g_ctex[i]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_w, g_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1, &g_fbo[i]);
  glBindFramebuffer(GL_FRAMEBUFFER, g_fbo[i]);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_ctex[i], 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    blog("fbo incomplete");
    return 0;
  }
  return 1;
}

int bw_gl_init(int w, int h) {
  EGLint major = 0, minor = 0, n = 0;
  EGLConfig cfg;
  EGLint attr[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
    EGL_NONE
  };
  EGLint pbuf[] = { EGL_WIDTH, w, EGL_HEIGHT, h, EGL_NONE };
  EGLint ctx_attr[] = {
    EGL_CONTEXT_MAJOR_VERSION, 3,
    EGL_CONTEXT_MINOR_VERSION, 3,
    EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
    EGL_NONE
  };
  g_logn = 0;
  g_log[0] = 0;
  g_w = w;
  g_h = h;
  g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (g_dpy == EGL_NO_DISPLAY) { blog("no display"); return 0; }
  if (!eglInitialize(g_dpy, &major, &minor)) { blog("eglInitialize"); return 0; }
  if (!eglChooseConfig(g_dpy, attr, &cfg, 1, &n) || n < 1) { blog("no config"); return 0; }
  if (!eglBindAPI(EGL_OPENGL_API)) { blog("bind api"); return 0; }
  g_surf = eglCreatePbufferSurface(g_dpy, cfg, pbuf);
  if (g_surf == EGL_NO_SURFACE) { blog("pbuffer"); return 0; }
  g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
  if (g_ctx == EGL_NO_CONTEXT) {
    blog("core 3.3 failed");
    g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, NULL);
  }
  if (g_ctx == EGL_NO_CONTEXT) { blog("no context"); return 0; }
  if (!eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) { blog("make current"); return 0; }
  if (!make_target(0) || !make_target(1)) return 0;
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
  g_ok = 1;
  blog((const char*)glGetString(GL_RENDERER));
  blog((const char*)glGetString(GL_VERSION));
  return 1;
}

int bw_gl_tex(int which) {
  if (which < 0 || which > 1) return 0;
  return (int)g_ctex[which];
}

int bw_gl_bind(int which) {
  if (!g_ok || which < 0 || which > 1) return 0;
  glBindFramebuffer(GL_FRAMEBUFFER, g_fbo[which]);
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
  GLuint t;
  if (!g_ok || !px || n < w * h * 4 || w < 1 || h < 1) return 0;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
  return (int)t;
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

int bw_gl_f2(int prog, const char* name, uint32_t a, uint32_t b) {
  GLint loc;
  if (!g_ok || prog <= 0 || !name) return 0;
  glUseProgram((GLuint)prog);
  loc = glGetUniformLocation((GLuint)prog, name);
  if (loc < 0) { blog("missing f2"); blog(name); return 0; }
  glUniform2f(loc, fbits(a), fbits(b));
  return 1;
}

int bw_gl_quad(int prog, int tex,
    uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
    uint32_t u0, uint32_t v0, uint32_t u1, uint32_t v1) {
  float v[20];
  GLint loc;
  float X0 = fbits(x0), Y0 = fbits(y0), X1 = fbits(x1), Y1 = fbits(y1);
  float U0 = fbits(u0), V0 = fbits(v0), U1 = fbits(u1), V1 = fbits(v1);
  if (!g_ok || prog <= 0 || tex <= 0) return 0;
  v[0] = X0; v[1] = Y0; v[2] = 0; v[3] = U0; v[4] = V0;
  v[5] = X1; v[6] = Y0; v[7] = 0; v[8] = U1; v[9] = V0;
  v[10] = X0; v[11] = Y1; v[12] = 0; v[13] = U0; v[14] = V1;
  v[15] = X1; v[16] = Y1; v[17] = 0; v[18] = U1; v[19] = V1;
  glUseProgram((GLuint)prog);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
  loc = glGetUniformLocation((GLuint)prog, "g_Texture0");
  if (loc >= 0) glUniform1i(loc, 0);
  glBindVertexArray(g_vao);
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_DYNAMIC_DRAW);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  return 1;
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

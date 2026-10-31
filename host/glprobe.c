#include <stdint.h>
#include <stdio.h>
#include <string.h>

int bw_gl_init(int w, int h);
int bw_gl_tex(int which);
int bw_gl_bind(int which);
int bw_gl_clear(void);
int bw_gl_rgba(int w, int h, const uint8_t* px, int n);
int bw_gl_prog(const char* vs, const char* fs);
int bw_gl_eye(int prog, const char* name);
int bw_gl_quad(int prog, int tex, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
               uint32_t u0, uint32_t v0, uint32_t u1, uint32_t v1);
int bw_gl_png(const char* path);
const char* bw_gl_log(void);

static const char* VS =
  "#version 330 core\n"
  "in vec3 a_Position;\n"
  "in vec2 a_TexCoord;\n"
  "uniform mat4 g_ModelViewProjectionMatrix;\n"
  "out vec2 v_TexCoord;\n"
  "void main() {\n"
  "  gl_Position = vec4(a_Position, 1.0) * g_ModelViewProjectionMatrix;\n"
  "  v_TexCoord = a_TexCoord;\n"
  "}\n";

static const char* FS =
  "#version 330 core\n"
  "in vec2 v_TexCoord;\n"
  "uniform sampler2D g_Texture0;\n"
  "out vec4 fragColor;\n"
  "void main() { fragColor = texture(g_Texture0, v_TexCoord); }\n";

static const char* FST =
  "#version 330 core\n"
  "in vec2 v_TexCoord;\n"
  "uniform sampler2D g_Texture0;\n"
  "out vec4 fragColor;\n"
  "void main() {\n"
  "  vec4 c = texture(g_Texture0, v_TexCoord);\n"
  "  c.g = clamp(c.g + 1.0, 0.0, 1.0);\n"
  "  fragColor = c;\n"
  "}\n";

/* NDC bits: -1, 1, 0, 0.5, 0.625, 0.875, 0.25 */
enum {
  N1 = 3212836864u, P1 = 1065353216u, Z = 0u,
  H = 1056964608u, X0 = 1059061760u, X1 = 1063256064u, Y1 = 1048576000u
};

int main(void) {
  uint8_t bg[8] = {255, 0, 0, 255, 0, 0, 255, 255};
  uint8_t yel[16];
  int plain, tint, tb, ty, ta, i;
  memset(yel, 0, sizeof yel);
  for (i = 0; i < 4; i++) { yel[i * 4] = 255; yel[i * 4 + 1] = 255; yel[i * 4 + 3] = 255; }
  if (!bw_gl_init(32, 32)) { fprintf(stderr, "init\n%s\n", bw_gl_log()); return 1; }
  plain = bw_gl_prog(VS, FS);
  tint = bw_gl_prog(VS, FST);
  tb = bw_gl_rgba(2, 1, bg, 8);
  ty = bw_gl_rgba(2, 2, yel, 16);
  fprintf(stderr, "LOG\n%s\n", bw_gl_log());
  if (!plain || !tint || !tb || !ty) return 1;
  bw_gl_bind(0);
  bw_gl_clear();
  bw_gl_eye(plain, "g_ModelViewProjectionMatrix");
  bw_gl_quad(plain, tb, N1, P1, P1, N1, Z, Z, P1, P1);
  ta = bw_gl_tex(0);
  bw_gl_bind(1);
  bw_gl_clear();
  bw_gl_eye(tint, "g_ModelViewProjectionMatrix");
  bw_gl_quad(tint, ta, N1, P1, P1, N1, Z, Z, P1, P1);
  bw_gl_eye(plain, "g_ModelViewProjectionMatrix");
  bw_gl_quad(plain, ty, X0, H, X1, Y1, Z, Z, P1, P1);
  if (!bw_gl_png("/tmp/glprobe.png")) { fprintf(stderr, "png\n%s\n", bw_gl_log()); return 1; }
  printf("probe wrote /tmp/glprobe.png\n");
  return 0;
}

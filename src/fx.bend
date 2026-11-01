import Base

# Built-in passes. Each fragment shader samples g_Texture0. Scroll wraps U
# by half a texture, spin turns the layer 180 degrees, opacity halves alpha,
# colorkey drops red. Tint keeps red and darkens green and blue. Pulse
# multiplies rgb by 0.5 + 0.5*sin(g_Time), which is 0.5 when g_Time is 0.

@unsafe
def vert() -> String:
  "#version 330 core\nlayout(location=0) in vec3 a_Position;\nlayout(location=1) in vec2 a_TexCoord;\nout vec2 v_TexCoord;\nvoid main(){ v_TexCoord = a_TexCoord; gl_Position = vec4(a_Position, 1.0); }\n"

@unsafe
def frag(name: String) -> String:
  frag.scroll(pair(name))

@unsafe
def pair(s: String) -> String & String:
  match s:
    case SNil{}:
      (SNil{}, SNil{})
    case SCon{Chr{c}, t}:
      pair.c(c, pair(t))

@unsafe
def pair.c(+c: U32, p: String & String) -> String & String:
  (a, b) = p
  (SCon{Chr{c}, a}, SCon{Chr{c}, b})

@unsafe
def frag.scroll(p: String & String) -> String:
  (a, b) = p
  frag.spin(b, String.eq(a, "scroll"))

@unsafe
def frag.spin(rest: String, hit: Bool) -> String:
  match hit:
    case True{}:
      frag.drop(rest, scroll_fs())
    case False{}:
      frag.spin2(pair(rest))

@unsafe
def frag.drop(rest: String, src: String) -> String:
  match rest:
    case SNil{}:
      src
    case SCon{_, t}:
      frag.drop(t, src)

@unsafe
def frag.spin2(p: String & String) -> String:
  (a, b) = p
  frag.op(b, String.eq(a, "spin"))

@unsafe
def frag.op(rest: String, hit: Bool) -> String:
  match hit:
    case True{}:
      frag.drop(rest, spin_fs())
    case False{}:
      frag.op2(pair(rest))

@unsafe
def frag.op2(p: String & String) -> String:
  (a, b) = p
  frag.ck(b, String.eq(a, "opacity"))

@unsafe
def frag.ck(rest: String, hit: Bool) -> String:
  match hit:
    case True{}:
      frag.drop(rest, opacity_fs())
    case False{}:
      frag.ck2(pair(rest))

@unsafe
def frag.ck2(p: String & String) -> String:
  (a, b) = p
  frag.ck3(b, String.eq(a, "colorkey"))

@unsafe
def frag.ck3(rest: String, hit: Bool) -> String:
  match hit:
    case True{}:
      frag.drop(rest, colorkey_fs())
    case False{}:
      frag.tint2(pair(rest))

@unsafe
def frag.tint2(p: String & String) -> String:
  (a, b) = p
  frag.tint3(b, String.eq(a, "tint"))

@unsafe
def frag.tint3(rest: String, hit: Bool) -> String:
  match hit:
    case True{}:
      frag.drop(rest, tint_fs())
    case False{}:
      frag.pulse2(pair(rest))

@unsafe
def frag.pulse2(p: String & String) -> String:
  (a, b) = p
  frag.pulse3(b, String.eq(a, "pulse"))

@unsafe
def frag.pulse3(rest: String, hit: Bool) -> String:
  match hit:
    case True{}:
      frag.drop(rest, pulse_fs())
    case False{}:
      frag.drop(rest, opacity_fs())

@unsafe
def scroll_fs() -> String:
  "#version 330 core\nin vec2 v_TexCoord;\nuniform sampler2D g_Texture0;\nout vec4 fragColor;\nvoid main(){ fragColor = texture(g_Texture0, vec2(fract(v_TexCoord.x + 0.5), v_TexCoord.y)); }\n"

@unsafe
def spin_fs() -> String:
  "#version 330 core\nin vec2 v_TexCoord;\nuniform sampler2D g_Texture0;\nout vec4 fragColor;\nvoid main(){ fragColor = texture(g_Texture0, vec2(1.0 - v_TexCoord.x, 1.0 - v_TexCoord.y)); }\n"

@unsafe
def opacity_fs() -> String:
  "#version 330 core\nin vec2 v_TexCoord;\nuniform sampler2D g_Texture0;\nout vec4 fragColor;\nvoid main(){ vec4 c = texture(g_Texture0, v_TexCoord); fragColor = vec4(c.rgb, c.a * 0.5); }\n"

@unsafe
def colorkey_fs() -> String:
  "#version 330 core\nin vec2 v_TexCoord;\nuniform sampler2D g_Texture0;\nout vec4 fragColor;\nvoid main(){ vec4 c = texture(g_Texture0, v_TexCoord); float cut = step(0.5, c.r) * step(c.g, 0.1) * step(c.b, 0.1); fragColor = vec4(c.rgb, c.a * (1.0 - cut)); }\n"

@unsafe
def tint_fs() -> String:
  "#version 330 core\nin vec2 v_TexCoord;\nuniform sampler2D g_Texture0;\nout vec4 fragColor;\nvoid main(){ vec4 c = texture(g_Texture0, v_TexCoord); fragColor = vec4(c.r, c.g * 0.35, c.b * 0.15, c.a); }\n"

@unsafe
def pulse_fs() -> String:
  "#version 330 core\nin vec2 v_TexCoord;\nuniform sampler2D g_Texture0;\nuniform float g_Time;\nout vec4 fragColor;\nvoid main(){ vec4 c = texture(g_Texture0, v_TexCoord); float p = 0.5 + 0.5 * sin(g_Time); fragColor = vec4(c.rgb * p, c.a); }\n"

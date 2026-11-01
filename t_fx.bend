import Base
import ./src/fx.bend as Fx
import ./src/gl.bend as GL

# One 2x1 texture, red then green. Each effect is a fullscreen pass.
# Scroll and spin move the left pixel onto the green texel. Opacity halves
# alpha. Colorkey clears the red texel.

@unsafe
def pix() -> List<&2, U32>:
  Con{255, Con{0, Con{0, Con{255, Con{0, Con{255, Con{0, Con{255, Nil{}}}}}}}}}

@unsafe
def bits_n() -> U32:
  F32.bits((0.0 - 1.0 : F32))

@unsafe
def bits_p() -> U32:
  F32.bits(1.0)

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
def one(name: String, path: String, +tex: U32) -> IO(Unit):
  one.n(pair(name), path, tex)

@unsafe
def one.n(p: String & String, path: String, +tex: U32) -> IO(Unit):
  (a, b) = p
  one.go(b, path, tex, a)

@unsafe
def one.go(label: String, path: String, +tex: U32, name: String) -> IO(Unit):
  do IO<Unit>:
    b : U32 <- GL.gl_bind(0)
    c : U32 <- GL.gl_clear()
    prog : U32 <- GL.gl_prog(Fx.vert(), Fx.frag(name))
    q : U32 <- GL.gl_quad(prog, tex, bits_n(), bits_p(), bits_p(), bits_n(), 0, 0, bits_p(), bits_p())
    png : U32 <- GL.gl_png(path)
    IO.print("FX " ++ label ++ "=" ++ U32.show(png))

@unsafe
def main() -> IO(Unit):
  do IO<Unit>:
    i : U32 <- GL.gl_init(8, 8)
    IO.print("FX init=" ++ U32.show(i))
    +tex : U32 <- GL.gl_rgba(2, 1, pix())
    u1 : Unit <- one("scroll", "build/fx-scroll.png", tex)
    u2 : Unit <- one("spin", "build/fx-spin.png", tex)
    u3 : Unit <- one("opacity", "build/fx-opacity.png", tex)
    u4 : Unit <- one("colorkey", "build/fx-colorkey.png", tex)
    log : String <- GL.gl_log()
    IO.print("LOG")
    IO.print(log)

import Base
import ./frame.bend as Fr
import ./gl.bend as GL

# Headless draw. Bend already computed NDC and the translated shaders.
# The PNG path is an argument so the pkg-scene proof does not overwrite m3.

@unsafe
def blit(prog: U32, tex: U32, q: Fr.Q) -> IO(U32):
  Fr.Q{x0, y0, x1, y1, u0, v0, u1, v1} = q
  blit.go(prog, tex, x0, y0, x1, y1, u0, v0, u1, v1)

@unsafe
def blit.go(prog: U32, tex: U32, x0: U32, y0: U32, x1: U32, y1: U32, u0: U32, v0: U32, u1: U32, v1: U32) -> IO(U32):
  GL.gl_quad(prog, tex, x0, y0, x1, y1, u0, v0, u1, v1)

@unsafe
def setu(+tag: String, +prog: U32, boost: U32, par: U32) -> IO(Unit):
  do IO<Unit>:
    e : U32 <- GL.gl_eye(prog, "g_ModelViewProjectionMatrix")
    IO.print("GL " ++ tag ++ " eye=" ++ U32.show(e))
    t : U32 <- GL.gl_f1(prog, "g_Time", 0)
    IO.print("GL " ++ tag ++ " time=" ++ U32.show(t))
    p : U32 <- GL.gl_f2(prog, "g_PointerPosition", 0, 0)
    IO.print("GL " ++ tag ++ " ptr=" ++ U32.show(p))
    a : U32 <- GL.gl_f2(prog, "g_ParallaxPosition", par, 0)
    IO.print("GL " ++ tag ++ " par=" ++ U32.show(a))
    b : U32 <- GL.gl_f1(prog, "g_Boost", boost)
    IO.print("GL " ++ tag ++ " boost=" ++ U32.show(b))
    s : U32 <- GL.gl_fv(prog, "g_AudioSpectrum", 0, 0, 0, 0)
    IO.print("GL " ++ tag ++ " spec=" ++ U32.show(s))

@unsafe
def draw(r: Fr.Run, path: String) -> IO(Unit):
  Fr.Run{plan, sh, quads, boost, dim, bg, sp} = r
  draw.sh(plan, sh, quads, boost, dim, bg, sp, path)

@unsafe
def draw.sh(plan: String, sh: Fr.Sh, quads: Fr.Qs, boost: U32, dim: Fr.Dim, bg: Fr.Px, sp: Fr.Px, path: String) -> IO(Unit):
  Fr.Sh{vshow, v1, v2, f0s, f0, f1s, f1} = sh
  draw.dim(plan, vshow, v1, v2, f0s, f0, f1s, f1, quads, boost, dim, bg, sp, path)

@unsafe
def draw.dim(plan: String, vshow: String, v1: String, v2: String, f0s: String, f0: String, f1s: String, f1: String, quads: Fr.Qs, boost: U32, dim: Fr.Dim, bg: Fr.Px, sp: Fr.Px, path: String) -> IO(Unit):
  Fr.Dim{w, h, fw, fh} = dim
  draw.bg(plan, vshow, v1, v2, f0s, f0, f1s, f1, quads, boost, w, h, bg, sp, path)

@unsafe
def draw.bg(plan: String, vshow: String, v1: String, v2: String, f0s: String, f0: String, f1s: String, f1: String, quads: Fr.Qs, boost: U32, w: U32, h: U32, bg: Fr.Px, sp: Fr.Px, path: String) -> IO(Unit):
  Fr.Px{bw, bh, bpx, be} = bg
  draw.sp(plan, vshow, v1, v2, f0s, f0, f1s, f1, quads, boost, w, h, bw, bh, bpx, be, sp, path)

@unsafe
def draw.sp(plan: String, vshow: String, v1: String, v2: String, f0s: String, f0: String, f1s: String, f1: String, quads: Fr.Qs, boost: U32, w: U32, h: U32, bw: U32, bh: U32, bpx: List<&2, U32>, be: U32, sp: Fr.Px, path: String) -> IO(Unit):
  Fr.Px{sw, sh, spx, se} = sp
  draw.q(plan, vshow, v1, v2, f0s, f0, f1s, f1, boost, w, h, bw, bh, bpx, be, sw, sh, spx, se, quads, path)

@unsafe
def draw.q(plan: String, vshow: String, v1: String, v2: String, f0s: String, f0: String, f1s: String, f1: String, boost: U32, w: U32, h: U32, bw: U32, bh: U32, bpx: List<&2, U32>, be: U32, sw: U32, sh: U32, spx: List<&2, U32>, se: U32, quads: Fr.Qs, path: String) -> IO(Unit):
  Fr.Qs{bgq, spq, fq} = quads
  draw.io(plan, vshow, v1, v2, f0s, f0, f1s, f1, boost, w, h, bw, bh, bpx, be, sw, sh, spx, se, bgq, spq, fq, Fr.one_bits(), path)

@unsafe
def draw.io(plan: String, vshow: String, v1: String, v2: String, f0s: String, f0: String, f1s: String, f1: String, +boost: U32, w: U32, h: U32, bw: U32, bh: U32, bpx: List<&2, U32>, be: U32, sw: U32, sh: U32, spx: List<&2, U32>, se: U32, bgq: Fr.Q, spq: Fr.Q, fq: Fr.Q, +par: U32, path: String) -> IO(Unit):
  do IO<Unit>:
    IO.print(plan)
    IO.print("TEX bg=" ++ U32.show(be) ++ " sp=" ++ U32.show(se))
    IO.print("VERT")
    IO.print(vshow)
    IO.print("END")
    IO.print("FRAG0")
    IO.print(f0s)
    IO.print("END")
    IO.print("FRAG1")
    IO.print(f1s)
    IO.print("END")
    i : U32 <- GL.gl_init(w, h)
    IO.print("GL init=" ++ U32.show(i))
    +tb : U32 <- GL.gl_rgba(bw, bh, bpx)
    IO.print("GL bgtex=" ++ U32.show(tb))
    +ts : U32 <- GL.gl_rgba(sw, sh, spx)
    IO.print("GL sptex=" ++ U32.show(ts))
    +plain : U32 <- GL.gl_prog(v1, f0)
    IO.print("GL plain=" ++ U32.show(plain))
    +tint : U32 <- GL.gl_prog(v2, f1)
    IO.print("GL tint=" ++ U32.show(tint))
    b0 : U32 <- GL.gl_bind(0)
    c0 : U32 <- GL.gl_clear()
    u0 : Unit <- setu("plain", plain, boost, par)
    q0 : U32 <- blit(plain, tb, bgq)
    IO.print("GL qbg=" ++ U32.show(q0))
    ct : U32 <- GL.gl_tex(0)
    b1 : U32 <- GL.gl_bind(1)
    c1 : U32 <- GL.gl_clear()
    u1 : Unit <- setu("tint", tint, boost, par)
    q1 : U32 <- blit(tint, ct, fq)
    IO.print("GL qfx=" ++ U32.show(q1))
    u2 : Unit <- setu("sprite", plain, boost, par)
    q2 : U32 <- blit(plain, ts, spq)
    IO.print("GL qsp=" ++ U32.show(q2))
    png : U32 <- GL.gl_png(path)
    IO.print("GL png=" ++ U32.show(png))
    log : String <- GL.gl_log()
    IO.print("LOG")
    IO.print(log)

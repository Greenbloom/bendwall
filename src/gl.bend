import Base

# EGL/OpenGL calls. Bend 2.0.39 has no GL effect in Base, so these
# defs only cross into host/libbendwall.so.

def gl_init(w: U32, h: U32) -> IO(U32):
  import "./eff_gl.c"
  import "./eff_gl.js"

def gl_tex(which: U32) -> IO(U32):
  import "./eff_gl.c"
  import "./eff_gl.js"

def gl_bind(which: U32) -> IO(U32):
  import "./eff_gl.c"
  import "./eff_gl.js"

def gl_clear() -> IO(U32):
  import "./eff_gl.c"
  import "./eff_gl.js"

def gl_rgba(w: U32, h: U32, px: List<&2, U32>) -> IO(U32):
  import "./eff_gl.c"
  import "./eff_gl.js"

def gl_prog(vs: String, fs: String) -> IO(U32):
  import "./eff_gl.c"
  import "./eff_gl.js"

def gl_eye(prog: U32, name: String) -> IO(U32):
  import "./eff_gl.c"
  import "./eff_gl.js"

def gl_f1(prog: U32, name: String, bits: U32) -> IO(U32):
  import "./eff_gl.c"
  import "./eff_gl.js"

def gl_f2(prog: U32, name: String, a: U32, b: U32) -> IO(U32):
  import "./eff_gl.c"
  import "./eff_gl.js"

def gl_quad(prog: U32, tex: U32, x0: U32, y0: U32, x1: U32, y1: U32, u0: U32, v0: U32, u1: U32, v1: U32) -> IO(U32):
  import "./eff_gl.c"
  import "./eff_gl.js"

def gl_png(path: String) -> IO(U32):
  import "./eff_gl.c"
  import "./eff_gl.js"

def gl_log() -> IO(String):
  import "./eff_gl.c"
  import "./eff_gl.js"

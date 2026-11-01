import Base
import ./json.bend as J

# A deterministic slice of a Wallpaper Engine particle system:
# one sphererandom emitter (speedmin along +x when directions are absent),
# lifetimerandom, and movement with gravity and drag.
# Alphafade is 1 - age/life. Sequence frames are age/life.

type P is Data:
  P{x: F32, y: F32, vx: F32, vy: F32, age: F32, life: F32, on: U32}

type Sys is Data:
  Sys{speed: F32, life: F32, gx: F32, gy: F32, drag: F32, n: U32}

@unsafe
def lit0() -> U32 & U32 & U32:
  (0, 0, 0)

@unsafe
def lit_go(s: String, acc: U32, neg: U32, dig: U32) -> U32 & U32 & U32:
  match s:
    case SNil{}:
      (acc, neg, dig)
    case SCon{Chr{c}, t}:
      lit_step(t, acc, neg, dig, c)

@unsafe
def lit_step(t: String, +acc: U32, neg: U32, dig: U32, +c: U32) -> U32 & U32 & U32:
  lit_d(t, acc, neg, dig, c, Bool.and(U32.is_ge(c, 48), U32.is_le(c, 57)))

@unsafe
def lit_d(t: String, +acc: U32, neg: U32, dig: U32, +c: U32, isd: Bool) -> U32 & U32 & U32:
  match isd:
    case True{}:
      lit_go(t, ((acc * 10 : U32) + (c - 48 : U32) : U32), neg, 1)
    case False{}:
      lit_m(t, acc, neg, dig, U32.is_eq(c, 45))

@unsafe
def lit_m(t: String, acc: U32, neg: U32, dig: U32, minus: Bool) -> U32 & U32 & U32:
  match minus:
    case True{}:
      lit_go(t, acc, 1, dig)
    case False{}:
      (acc, neg, dig)

@unsafe
def f_int(s: String) -> F32:
  f_int.s(lit_go(s, 0, 0, 0))

@unsafe
def f_int.s(r: U32 & U32 & U32) -> F32:
  (acc, neg, dig) = r
  f_int.n(U32.to_f32(acc), U32.is_eq(neg, 1))

@unsafe
def f_int.n(v: F32, neg: Bool) -> F32:
  match neg:
    case True{}:
      (0.0 - v : F32)
    case False{}:
      v

@unsafe
def skip_sp(s: String) -> String:
  match s:
    case SNil{}:
      SNil{}
    case SCon{Chr{+c}, +t}:
      skip_sp.m(s, t, U32.is_eq(c, 32))

@unsafe
def skip_sp.m(s: String, t: String, sp: Bool) -> String:
  match sp:
    case True{}:
      skip_sp(t)
    case False{}:
      s

@unsafe
def word(s: String) -> String & String:
  word.go(skip_sp(s), "")

@unsafe
def word.go(s: String, acc: String) -> String & String:
  match s:
    case SNil{}:
      (acc, SNil{})
    case SCon{Chr{+c}, t}:
      word.c(t, acc, c, U32.is_eq(c, 32))

@unsafe
def word.c(t: String, acc: String, c: U32, sp: Bool) -> String & String:
  match sp:
    case True{}:
      (acc, skip_sp(t))
    case False{}:
      word.go(t, SCon{Chr{c}, acc})

@unsafe
def rev(s: String) -> String:
  rev.go(s, "")

@unsafe
def rev.go(s: String, acc: String) -> String:
  match s:
    case SNil{}:
      acc
    case SCon{Chr{c}, t}:
      rev.go(t, SCon{Chr{c}, acc})

@unsafe
def word_fix(s: String) -> String & String:
  word_fix.r(word(s))

@unsafe
def word_fix.r(r: String & String) -> String & String:
  (w, rest) = r
  (rev(w), rest)

@unsafe
def grav(s: String) -> F32 & F32:
  grav.b(word_fix(s))

@unsafe
def grav.b(r: String & String) -> F32 & F32:
  (x, rest) = r
  grav.c(f_int(x), word_fix(rest))

@unsafe
def grav.c(x: F32, r: String & String) -> F32 & F32:
  (y, rest) = r
  (x, f_int(y))

@unsafe
def kids(j: J.J) -> List<&2, J.J>:
  J.J{k, key, s, xs} = j
  xs

@unsafe
def child(j: J.J, key: String) -> J.J:
  J.get(j, key)

@unsafe
def num_of(j: J.J) -> F32:
  J.J{k, key, s, xs} = j
  f_int(s)

@unsafe
def str_of(j: J.J) -> String:
  J.J{k, key, s, xs} = j
  s

@unsafe
def named(xs: List<&2, J.J>, name: String) -> J.J:
  match xs:
    case Nil{}:
      J.null()
    case Con{+h, t}:
      named.m(t, name, h, J.get(h, "name"))

@unsafe
def named.m(t: List<&2, J.J>, name: String, h: J.J, +nm: J.J) -> J.J:
  named.eq(t, name, h, nm, J.lit(nm))

@unsafe
def named.eq(t: List<&2, J.J>, +name: String, h: J.J, nm: J.J, s: String) -> J.J:
  named.b(t, name, h, String.eq(s, name))

@unsafe
def named.b(t: List<&2, J.J>, name: String, h: J.J, eq: Bool) -> J.J:
  match eq:
    case True{}:
      h
    case False{}:
      named(t, name)

@unsafe
def sys_of(src: String) -> Sys:
  sys_j(J.parse(src))

@unsafe
def sys_j(+j: J.J) -> Sys:
  sys_e(named(kids(child(j, "emitter")), "sphererandom"), named(kids(child(j, "initializer")), "lifetimerandom"), named(kids(child(j, "operator")), "movement"))

@unsafe
def sys_e(em: J.J, life: J.J, mov: J.J) -> Sys:
  Sys{num_of(child(em, "speedmin")), num_of(child(life, "min")), 0.0, 0.0, num_of(child(mov, "drag")), 1}

@unsafe
def sys_g(s: Sys, g: String) -> Sys:
  Sys{speed, life, gx, gy, drag, n} = s
  sys_gy(speed, life, drag, n, grav(g))

@unsafe
def sys_gy(speed: F32, life: F32, drag: F32, n: U32, g: F32 & F32) -> Sys:
  (gx, gy) = g
  Sys{speed, life, gx, gy, drag, n}

@unsafe
def load(src: String) -> Sys:
  load.j(J.parse(src))

@unsafe
def load.j(+j: J.J) -> Sys:
  load.m(j, named(kids(child(j, "operator")), "movement"))

@unsafe
def load.m(j: J.J, mov: J.J) -> Sys:
  sys_g(sys_j(j), str_of(child(mov, "gravity")))

@unsafe
def spawn(s: Sys) -> P:
  Sys{speed, life, gx, gy, drag, n} = s
  P{0.0, 0.0, speed, 0.0, 0.0, life, 1}

@unsafe
def step(p: P, +dt: F32, gx: F32, gy: F32, drag: F32) -> P:
  P{x, y, vx, vy, age, life, on} = p
  step.v(x, y, vx, vy, age, life, on, dt, gx, gy, drag)

@unsafe
def step.v(+x: F32, +y: F32, +vx: F32, +vy: F32, age: F32, life: F32, on: U32, +dt: F32, gx: F32, gy: F32, +drag: F32) -> P:
  P{(x + (vx * dt : F32) : F32), (y + (vy * dt : F32) : F32), ((vx + (gx * dt : F32) : F32) * (1.0 - (drag * dt : F32) : F32) : F32), ((vy + (gy * dt : F32) : F32) * (1.0 - (drag * dt : F32) : F32) : F32), (age + dt : F32), life, on}

@unsafe
def steps(p: P, +n: U32, dt: F32, gx: F32, gy: F32, drag: F32) -> P:
  steps.m(p, n, dt, gx, gy, drag, U32.is_zero(n))

@unsafe
def steps.m(p: P, +n: U32, +dt: F32, +gx: F32, +gy: F32, +drag: F32, z: Bool) -> P:
  match z:
    case True{}:
      p
    case False{}:
      steps(step(p, dt, gx, gy, drag), (n - 1 : U32), dt, gx, gy, drag)

@unsafe
def alpha(p: P) -> F32:
  P{x, y, vx, vy, age, +life, on} = p
  (1.0 - (age / life : F32) : F32)

@unsafe
def show(p: P) -> String:
  P{+x, +y, vx, vy, age, life, on} = p
  "PART x=" ++ U32.show(F32.bits(x)) ++ " y=" ++ U32.show(F32.bits(y)) ++ " a=" ++ U32.show(F32.bits(alpha(P{x, y, vx, vy, age, life, on}))) ++ " n=1"

@unsafe
def run(src: String) -> String:
  run.go(load(src))

@unsafe
def run.go(s: Sys) -> String:
  Sys{speed, life, gx, gy, drag, n} = s
  show(steps(P{0.0, 0.0, speed, 0.0, 0.0, life, 1}, 2, 1.0, gx, gy, drag))

# sizechange, angularmovement and colorchange. start size 1, angle 0, color 0.
# Two steps of dt 1 add rate each step. run() above is unchanged.

@unsafe
def dup_s(s: String) -> String & String:
  match s:
    case SNil{}:
      (SNil{}, SNil{})
    case SCon{Chr{c}, t}:
      dup_s.c(c, dup_s(t))

@unsafe
def dup_s.c(+c: U32, p: String & String) -> String & String:
  (a, b) = p
  (SCon{Chr{c}, a}, SCon{Chr{c}, b})

@unsafe
def op_num(src: String, op: String, key: String) -> F32:
  op_num.j(J.parse(src), op, key)

@unsafe
def op_num.j(j: J.J, op: String, key: String) -> F32:
  num_of(child(named(kids(child(j, "operator")), op), key))

@unsafe
def step_lin(+v: F32, +rate: F32, +dt: F32) -> F32:
  (v + (rate * dt : F32) : F32)

@unsafe
def steps_lin(+v: F32, +rate: F32, +dt: F32, +n: U32) -> F32:
  steps_lin.m(v, rate, dt, n, U32.is_zero(n))

@unsafe
def steps_lin.m(+v: F32, +rate: F32, +dt: F32, +n: U32, z: Bool) -> F32:
  match z:
    case True{}:
      v
    case False{}:
      steps_lin(step_lin(v, rate, dt), rate, dt, (n - 1 : U32))

@unsafe
def run_more(src: String) -> String:
  run_more.a(dup_s(src))

@unsafe
def run_more.a(p: String & String) -> String:
  (a, b) = p
  run_more.b(op_num(a, "sizechange", "rate"), b)

@unsafe
def run_more.b(+sz: F32, src: String) -> String:
  run_more.c(sz, dup_s(src))

@unsafe
def run_more.c(+sz: F32, p: String & String) -> String:
  (a, b) = p
  run_more.d(sz, op_num(a, "angularmovement", "speed"), b)

@unsafe
def run_more.d(+sz: F32, +ang: F32, src: String) -> String:
  run_more.v(sz, ang, op_num(src, "colorchange", "rate"))

@unsafe
def run_more.v(+sz: F32, +ang: F32, +col: F32) -> String:
  "SZ size=" ++ U32.show(F32.bits(steps_lin(1.0, sz, 1.0, 2))) ++ " ang=" ++ U32.show(F32.bits(steps_lin(0.0, ang, 1.0, 2))) ++ " col=" ++ U32.show(F32.bits(steps_lin(0.0, col, 1.0, 2)))

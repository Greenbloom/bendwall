import Base
import ./json.bend as J
import ./scene.bend as Scn
import ./tex.bend as T
import ./str.bend as S
import ./glsl.bend as G

# Frame plan for one synthetic scene: pixel-edge NDC, origin at the
# object center, parallax in pixels, sprite frame chosen by time.

type Lit is Data:
  Lit{ip: U32, fp: U32, sc: U32, dot: U32}

type Q is Data:
  Q{x0: U32, y0: U32, x1: U32, y1: U32, u0: U32, v0: U32, u1: U32, v1: U32}

type Uv is Data:
  Uv{u0: U32, v0: U32, u1: U32, v1: U32}

type Dim is Data:
  Dim{w: U32, h: U32, fw: F32, fh: F32}

type Px is Data:
  Px{w: U32, h: U32, px: List<&2, U32>, err: U32}

type Loc is Data:
  Loc{origin: String, size: String, depth: String}

type Two is Data:
  Two{bg: Scn.Obj, sp: Scn.Obj}

type Mat is Data:
  Mat{oname: String, oval: String, prop: String, fallback: String}

type Sh is Data:
  Sh{vshow: String, v1: String, v2: String, f0s: String, f0: String, f1s: String, f1: String}

type Qs is Data:
  Qs{bg: Q, sp: Q, full: Q}

type Run is Data:
  Run{plan: String, sh: Sh, quads: Qs, boost: U32, dim: Dim, bg: Px, sp: Px}

@unsafe
def ch(c: U32) -> String:
  SCon{Chr{c}, SNil{}}

@unsafe
def f_of(a: U32) -> F32:
  match a:
    case U32{w}:
      F32{w}

@unsafe
def lit0() -> Lit:
  Lit{0, 0, 1, 0}

@unsafe
def f_lit(s: String) -> F32:
  f_end(f_go(s, lit0()))

@unsafe
def f_go(s: String, st: Lit) -> Lit:
  match s:
    case SNil{}:
      st
    case SCon{Chr{c}, t}:
      f_step(t, st, c)

@unsafe
def f_step(t: String, st: Lit, +c: U32) -> Lit:
  Lit{+ip, +fp, +sc, dot} = st
  f_dig(t, ip, fp, sc, dot, c, Bool.and(U32.is_ge(c, 48), U32.is_le(c, 57)))

@unsafe
def f_dig(t: String, +ip: U32, +fp: U32, +sc: U32, dot: U32, +c: U32, dig: Bool) -> Lit:
  match dig:
    case True{}:
      f_num(t, ip, fp, sc, dot, (c - 48 : U32))
    case False{}:
      f_dot(t, ip, fp, sc, dot, U32.is_eq(c, 46))

@unsafe
def f_num(t: String, +ip: U32, +fp: U32, +sc: U32, dot: U32, +d: U32) -> Lit:
  f_num.m(t, ip, fp, sc, d, U32.is_eq(dot, 0))

@unsafe
def f_num.m(t: String, +ip: U32, +fp: U32, +sc: U32, +d: U32, before: Bool) -> Lit:
  match before:
    case True{}:
      f_go(t, Lit{((ip * 10 : U32) + d : U32), fp, sc, 0})
    case False{}:
      f_go(t, Lit{ip, ((fp * 10 : U32) + d : U32), (sc * 10 : U32), 1})

@unsafe
def f_dot(t: String, ip: U32, fp: U32, sc: U32, dot: U32, isdot: Bool) -> Lit:
  match isdot:
    case True{}:
      f_go(t, Lit{ip, fp, sc, 1})
    case False{}:
      Lit{ip, fp, sc, dot}

@unsafe
def f_end(st: Lit) -> F32:
  Lit{ip, fp, +sc, dot} = st
  F32.div(F32.add(F32.mul(U32.to_f32(ip), U32.to_f32(sc)), U32.to_f32(fp)), U32.to_f32(sc))

@unsafe
def ndc_x(+px: F32, +w: F32) -> U32:
  F32.bits(F32.div(F32.sub(F32.mul(U32.to_f32(2), px), w), w))

@unsafe
def ndc_y(+py: F32, +h: F32) -> U32:
  F32.bits(F32.div(F32.sub(h, F32.mul(U32.to_f32(2), py)), h))

@unsafe
def one_bits() -> U32:
  F32.bits(U32.to_f32(1))

@unsafe
def t_now() -> F32:
  F32.div(U32.to_f32(3), U32.to_f32(5))

@unsafe
def span(o: F32, +sz: F32, dep: F32, par: F32) -> F32 & F32:
  span.l(F32.add(F32.sub(o, F32.div(sz, U32.to_f32(2))), F32.mul(dep, par)), sz)

@unsafe
def span.l(+l: F32, sz: F32) -> F32 & F32:
  (l, F32.add(l, sz))

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
def take_word(s: String) -> String & String:
  take_go(skip_sp(s), "")

@unsafe
def take_go(s: String, acc: String) -> String & String:
  match s:
    case SNil{}:
      (acc, SNil{})
    case SCon{Chr{+c}, t}:
      take_c(t, acc, c, U32.is_eq(c, 32))

@unsafe
def take_c(t: String, acc: String, c: U32, sp: Bool) -> String & String:
  match sp:
    case True{}:
      (acc, skip_sp(t))
    case False{}:
      take_go(t, acc ++ ch(c))

@unsafe
def two(s: String) -> F32 & F32:
  two.b(take_word(s))

@unsafe
def two.b(r: String & String) -> F32 & F32:
  (a, rest) = r
  two.c(f_lit(a), take_word(rest))

@unsafe
def two.c(x: F32, r: String & String) -> F32 & F32:
  (b, rest) = r
  (x, f_lit(b))

@unsafe
def colon(s: String) -> String & String:
  colon.go(s, "")

@unsafe
def colon.go(s: String, acc: String) -> String & String:
  match s:
    case SNil{}:
      (acc, "")
    case SCon{Chr{+c}, t}:
      colon.c(t, acc, c, U32.is_eq(c, 58))

@unsafe
def colon.c(t: String, acc: String, c: U32, hit: Bool) -> String & String:
  match hit:
    case True{}:
      (acc, t)
    case False{}:
      colon.go(t, acc ++ ch(c))

@unsafe
def props_of(r: String & List<&2, Scn.Prop> & List<&2, Scn.Over>) -> List<&2, Scn.Prop>:
  (h, ps, os) = r
  ps

@unsafe
def overs_of(r: String & List<&2, Scn.Prop> & List<&2, Scn.Over>) -> List<&2, Scn.Over>:
  (h, ps, os) = r
  os

@unsafe
def resolved(base: String, preset: String) -> List<&2, Scn.Prop>:
  resolved.o(props_of(Scn.split_proj(Scn.project(J.parse(base)))), preset)

@unsafe
def resolved.o(ps: List<&2, Scn.Prop>, preset: String) -> List<&2, Scn.Prop>:
  Scn.apply(ps, overs_of(Scn.split_proj(Scn.project(J.parse(preset)))), "preset")

@unsafe
def general_of(root: J.J) -> J.J:
  general_t(Scn.take(root, "general"))

@unsafe
def general_t(r: J.J & J.J) -> J.J:
  (g, rest) = r
  g

@unsafe
def objects_of(root: J.J) -> List<&2, J.J>:
  objects_t(Scn.take(root, "objects"))

@unsafe
def objects_t(r: J.J & J.J) -> List<&2, J.J>:
  (o, rest) = r
  Scn.open_j(o)

@unsafe
def dim_of(sc: Scn.Sc) -> Dim:
  Scn.Sc{w, h, clear, bloom} = sc
  dim_wh(G.dup_s(w), G.dup_s(h))

@unsafe
def dim_wh(ws: String & String, hs: String & String) -> Dim:
  (w1, w2) = ws
  dim_h(w1, w2, hs)

@unsafe
def dim_h(w1: String, w2: String, hs: String & String) -> Dim:
  (h1, h2) = hs
  Dim{S.atoi(w1, 0), S.atoi(h1, 0), f_lit(w2), f_lit(h2)}

@unsafe
def name_of(o: Scn.Obj) -> String & Scn.Obj:
  Scn.Obj{+name, kind, origin, size, parallax, alpha, angles, payload, effects} = o
  (name, Scn.Obj{name, kind, origin, size, parallax, alpha, angles, payload, effects})

@unsafe
def keep(acc: Two, o: Scn.Obj) -> Two:
  keep.n(acc, name_of(o))

@unsafe
def keep.n(acc: Two, r: String & Scn.Obj) -> Two:
  (name, obj) = r
  keep.e(acc, obj, eq2(name))

@unsafe
def eq2(+name: String) -> Bool & Bool:
  (String.eq(name, "bg"), String.eq(name, "star"))

@unsafe
def keep.e(acc: Two, obj: Scn.Obj, r: Bool & Bool) -> Two:
  (isbg, issp) = r
  keep.acc(acc, obj, isbg, issp)

@unsafe
def keep.acc(acc: Two, obj: Scn.Obj, isbg: Bool, issp: Bool) -> Two:
  Two{bg, sp} = acc
  keep.bg(bg, sp, obj, isbg, issp)

@unsafe
def keep.bg(bg: Scn.Obj, sp: Scn.Obj, obj: Scn.Obj, isbg: Bool, issp: Bool) -> Two:
  match isbg:
    case True{}:
      Two{obj, sp}
    case False{}:
      keep.sp(bg, sp, obj, issp)

@unsafe
def keep.sp(bg: Scn.Obj, sp: Scn.Obj, obj: Scn.Obj, issp: Bool) -> Two:
  match issp:
    case True{}:
      Two{bg, obj}
    case False{}:
      Two{bg, sp}

@unsafe
def collect(xs: List<&2, J.J>) -> Two:
  collect.go(xs, Two{Scn.obj0(), Scn.obj0()})

@unsafe
def collect.go(xs: List<&2, J.J>, acc: Two) -> Two:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      collect.step(t, acc, Scn.obj_one(h, Nil{}))

@unsafe
def collect.step(t: List<&2, J.J>, acc: Two, r: Scn.Obj & List<&2, Scn.Prop>) -> Two:
  (o, props) = r
  collect.go(t, keep(acc, o))

@unsafe
def loc_of(o: Scn.Obj) -> Loc:
  Scn.Obj{name, kind, origin, size, parallax, alpha, angles, payload, effects} = o
  Loc{origin, size, parallax}

@unsafe
def place(loc: Loc, +fw: F32, +fh: F32, uv: Uv) -> Q:
  Uv{u0, v0, u1, v1} = uv
  place.o(loc, fw, fh, u0, v0, u1, v1)

@unsafe
def place.o(loc: Loc, fw: F32, fh: F32, u0: U32, v0: U32, u1: U32, v1: U32) -> Q:
  Loc{origin, size, depth} = loc
  place.xy(two(origin), two(size), two(depth), fw, fh, u0, v0, u1, v1)

@unsafe
def place.xy(o: F32 & F32, s: F32 & F32, d: F32 & F32, fw: F32, fh: F32, u0: U32, v0: U32, u1: U32, v1: U32) -> Q:
  (ox, oy) = o
  place.s(ox, oy, s, d, fw, fh, u0, v0, u1, v1)

@unsafe
def place.s(ox: F32, oy: F32, s: F32 & F32, d: F32 & F32, fw: F32, fh: F32, u0: U32, v0: U32, u1: U32, v1: U32) -> Q:
  (sx, sy) = s
  place.d(ox, oy, sx, sy, d, fw, fh, u0, v0, u1, v1)

@unsafe
def place.d(ox: F32, oy: F32, sx: F32, sy: F32, d: F32 & F32, fw: F32, fh: F32, u0: U32, v0: U32, u1: U32, v1: U32) -> Q:
  (dx, dy) = d
  place.span(span(ox, sx, dx, U32.to_f32(1)), oy, sy, dy, fw, fh, u0, v0, u1, v1)

@unsafe
def place.span(xs: F32 & F32, oy: F32, sy: F32, dy: F32, fw: F32, fh: F32, u0: U32, v0: U32, u1: U32, v1: U32) -> Q:
  (x0, x1) = xs
  place.ys(x0, x1, span(oy, sy, dy, U32.to_f32(0)), fw, fh, u0, v0, u1, v1)

@unsafe
def place.ys(x0: F32, x1: F32, ys: F32 & F32, +fw: F32, +fh: F32, u0: U32, v0: U32, u1: U32, v1: U32) -> Q:
  (y0, y1) = ys
  Q{ndc_x(x0, fw), ndc_y(y0, fh), ndc_x(x1, fw), ndc_y(y1, fh), u0, v0, u1, v1}

@unsafe
def uv_full() -> Uv:
  Uv{0, 0, one_bits(), one_bits()}

@unsafe
def uv_div(bits: U32, den: U32) -> U32:
  F32.bits(F32.div(f_of(bits), U32.to_f32(den)))

@unsafe
def uv_sum(a: U32, b: U32, den: U32) -> U32:
  F32.bits(F32.div(F32.add(f_of(a), f_of(b)), U32.to_f32(den)))

@unsafe
def uv_frame(fr: T.Frame, +sw: U32, +sh: U32) -> Uv:
  T.Frame{img, tb, +x, +y, w, wy, hx, h} = fr
  Uv{uv_div(x, sw), uv_div(y, sh), uv_sum(x, w, sw), uv_sum(y, h, sh)}

@unsafe
def pick(xs: List<&2, T.Frame>, t: F32) -> T.Frame:
  pick.go(xs, t, U32.to_f32(0), T.Frame{0, 0, 0, 0, 0, 0, 0, 0})

@unsafe
def pick.go(xs: List<&2, T.Frame>, +t: F32, acc: F32, last: T.Frame) -> T.Frame:
  match xs:
    case Nil{}:
      last
    case Con{h, rest}:
      pick.one(rest, t, acc, h)

@unsafe
def pick.one(rest: List<&2, T.Frame>, +t: F32, +acc: F32, h: T.Frame) -> T.Frame:
  pick.tb(rest, t, acc, tb_of(h))

@unsafe
def tb_of(h: T.Frame) -> U32 & T.Frame:
  T.Frame{img, +tb, x, y, w, wy, hx, hh} = h
  (tb, T.Frame{img, tb, x, y, w, wy, hx, hh})

@unsafe
def pick.tb(rest: List<&2, T.Frame>, +t: F32, +acc: F32, r: U32 & T.Frame) -> T.Frame:
  (+tb, h) = r
  pick.ft(rest, t, acc, h, f_of(tb))

@unsafe
def pick.ft(rest: List<&2, T.Frame>, +t: F32, +acc: F32, h: T.Frame, +ft: F32) -> T.Frame:
  pick.test(rest, t, acc, h, ft, F32.is_lt(t, F32.add(acc, ft)))

@unsafe
def pick.test(rest: List<&2, T.Frame>, +t: F32, +acc: F32, h: T.Frame, +ft: F32, hit: Bool) -> T.Frame:
  match hit:
    case True{}:
      h
    case False{}:
      pick.go(rest, t, F32.add(acc, ft), h)

@unsafe
def head_im(xs: List<&2, T.IM>) -> T.IM:
  match xs:
    case Con{im, _}:
      im
    case Nil{}:
      T.IM{0, T.mip0()}

@unsafe
def pix_of(bs: List<&2, U32>) -> Px & U32 & U32 & T.Frame:
  pix_t(T.parse(bs))

@unsafe
def pix_t(tex: T.Tex) -> Px & U32 & U32 & T.Frame:
  T.Tex{err, ver, format, flags, tw, th, iw, ih, depth, unk, bv, fif, cond, imgs, sw, sh, sv, frames} = tex
  pix_i(err, format, sw, sh, frames, head_im(imgs))

@unsafe
def pix_i(err: U32, format: U32, sw: U32, sh: U32, frames: List<&2, T.Frame>, im: T.IM) -> Px & U32 & U32 & T.Frame:
  T.IM{n, mip} = im
  pix_m(err, format, sw, sh, frames, mip)

@unsafe
def pix_m(err: U32, format: U32, sw: U32, sh: U32, frames: List<&2, T.Frame>, mip: T.Mip) -> Px & U32 & U32 & T.Frame:
  T.Mip{w, h, d, lz, raw, csz, px} = mip
  (Px{w, h, T.expand(format, px), err}, sw, sh, pick(frames, t_now()))

@unsafe
def fr_x(fr: T.Frame) -> U32 & T.Frame:
  T.Frame{img, tb, +x, y, w, wy, hx, h} = fr
  (x, T.Frame{img, tb, x, y, w, wy, hx, h})

@unsafe
def full_q(+fw: F32, +fh: F32) -> Q:
  Q{ndc_x(U32.to_f32(0), fw), ndc_y(U32.to_f32(0), fh), ndc_x(fw, fw), ndc_y(fh, fh), 0, 0, one_bits(), one_bits()}

@unsafe
def mat_ps(s: String) -> Scn.Ps:
  mat_t(Scn.take(J.parse(s), "passes"))

@unsafe
def mat_t(r: J.J & J.J) -> Scn.Ps:
  (p, rest) = r
  mat_h(Scn.open_j(p))

@unsafe
def mat_h(xs: List<&2, J.J>) -> Scn.Ps:
  match xs:
    case Con{h, _}:
      Scn.pass_go(Scn.ps0(), Scn.open_j(h))
    case Nil{}:
      Scn.ps0()

@unsafe
def mat_of(p: Scn.Ps) -> Mat:
  Scn.Ps{shader, blend, combo, constv, user, tex} = p
  mat_c(colon(combo), constv, user)

@unsafe
def mat_c(r: String & String, constv: String, user: String) -> Mat:
  (oname, oval) = r
  mat_u(oname, oval, colon(user), constv)

@unsafe
def mat_u(oname: String, oval: String, r: String & String, constv: String) -> Mat:
  (k, prop) = r
  mat_f(oname, oval, prop, colon(constv))

@unsafe
def mat_f(oname: String, oval: String, prop: String, r: String & String) -> Mat:
  (ck, fallback) = r
  Mat{oname, oval, prop, fallback}

@unsafe
def mat_boost(m: Mat, ps: List<&2, Scn.Prop>) -> String & Mat:
  Mat{+oname, +oval, +prop, +fallback} = m
  mat_boost.v(oname, oval, prop, fallback, Scn.find(ps, prop))

@unsafe
def mat_boost.v(+oname: String, +oval: String, +prop: String, +fallback: String, r: String & List<&2, Scn.Prop>) -> String & Mat:
  (+v, ps) = r
  mat_boost.e(oname, oval, prop, fallback, v, G.empty(v))

@unsafe
def mat_boost.e(+oname: String, +oval: String, +prop: String, +fallback: String, v: String, r: Bool & String) -> String & Mat:
  (e, val) = r
  mat_boost.m(oname, oval, prop, fallback, val, e)

@unsafe
def mat_boost.m(oname: String, oval: String, prop: String, +fallback: String, val: String, e: Bool) -> String & Mat:
  match e:
    case True{}:
      (fallback, Mat{oname, oval, prop, fallback})
    case False{}:
      (val, Mat{oname, oval, prop, fallback})

@unsafe
def q_show(q: Q) -> String:
  Q{x0, y0, x1, y1, u0, v0, u1, v1} = q
  " x0=" ++ U32.show(x0) ++ " y0=" ++ U32.show(y0) ++ " x1=" ++ U32.show(x1) ++ " y1=" ++ U32.show(y1) ++ " u0=" ++ U32.show(u0) ++ " v0=" ++ U32.show(v0) ++ " u1=" ++ U32.show(u1) ++ " v1=" ++ U32.show(v1)

@unsafe
def plan_line(fx: U32, q: Q, boost: String) -> String:
  "PLAN fx=" ++ U32.show(fx) ++ q_show(q) ++ " boost=" ++ boost

@unsafe
def tri(s: String) -> String & String & String:
  tri.b(G.dup_s(s))

@unsafe
def tri.b(p: String & String) -> String & String & String:
  (a, b) = p
  tri.c(a, G.dup_s(b))

@unsafe
def tri.c(a: String, p: String & String) -> String & String & String:
  (b, c) = p
  (a, b, c)

@unsafe
def sh_of(vert: String, frag: String, inc: String, +oname: String, +oval: String) -> Sh:
  sh_v(tri(G.translate(vert, 0, "", oname, oval)), frag, inc, oname, oval)

@unsafe
def sh_v(vs: String & String & String, frag: String, inc: String, oname: String, oval: String) -> Sh:
  (vshow, v1, v2) = vs
  sh_f(vshow, v1, v2, G.dup_s(frag), G.dup_s(inc), oname, oval)

@unsafe
def sh_f(vshow: String, v1: String, v2: String, fs: String & String, ics: String & String, oname: String, oval: String) -> Sh:
  (f1, f2) = fs
  sh_i(vshow, v1, v2, f1, f2, ics, oname, oval)

@unsafe
def sh_i(vshow: String, v1: String, v2: String, f1: String, f2: String, ics: String & String, oname: String, oval: String) -> Sh:
  (i1, i2) = ics
  sh_f1(vshow, v1, v2, tri(G.translate(f1, 1, i1, "", "")), G.dup_s(G.translate(f2, 1, i2, oname, oval)))

@unsafe
def sh_f1(vshow: String, v1: String, v2: String, zs: String & String & String, fs: String & String) -> Sh:
  (f0s, f0, drop) = zs
  sh_f1b(vshow, v1, v2, f0s, f0, fs)

@unsafe
def sh_f1b(vshow: String, v1: String, v2: String, f0s: String, f0: String, fs: String & String) -> Sh:
  (f1s, f1) = fs
  Sh{vshow, v1, v2, f0s, f0, f1s, f1}

@unsafe
def quads_of(dim: Dim, two: Two, fr: T.Frame, sw: U32, sh: U32) -> Qs & U32:
  Dim{w, h, +fw, +fh} = dim
  quads_fr(fw, fh, two, fr_x(fr), sw, sh)

@unsafe
def quads_fr(+fw: F32, +fh: F32, two: Two, r: U32 & T.Frame, sw: U32, sh: U32) -> Qs & U32:
  (fx, fr) = r
  quads_bg(fx, fw, fh, two, uv_frame(fr, sw, sh))

@unsafe
def quads_bg(fx: U32, +fw: F32, +fh: F32, two: Two, uv: Uv) -> Qs & U32:
  Two{bg, sp} = two
  (Qs{place(loc_of(bg), fw, fh, uv_full()), place(loc_of(sp), fw, fh, uv), full_q(fw, fh)}, fx)

@unsafe
def assemble(base: String, preset: String, scene: String, mat: String, vert: String, frag: String, inc: String, bgbs: List<&2, U32>, spbs: List<&2, U32>) -> Run:
  assemble.m(resolved(base, preset), J.parse(scene), mat_of(mat_ps(mat)), vert, frag, inc, pix_of(bgbs), pix_of(spbs))

@unsafe
def assemble.m(ps: List<&2, Scn.Prop>, root: J.J, m: Mat, vert: String, frag: String, inc: String, bgp: Px & U32 & U32 & T.Frame, spp: Px & U32 & U32 & T.Frame) -> Run:
  assemble.mb(mat_boost(m, ps), root, vert, frag, inc, bgp, spp)

@unsafe
def assemble.mb(r: String & Mat, +root: J.J, vert: String, frag: String, inc: String, bgp: Px & U32 & U32 & T.Frame, spp: Px & U32 & U32 & T.Frame) -> Run:
  (boost, m) = r
  assemble.b(boost, m, dim_of(Scn.gen_scene(general_of(root))), collect(objects_of(root)), vert, frag, inc, bgp, spp)

@unsafe
def assemble.b(boost: String, m: Mat, dim: Dim, two: Two, vert: String, frag: String, inc: String, bgp: Px & U32 & U32 & T.Frame, spp: Px & U32 & U32 & T.Frame) -> Run:
  Mat{oname, oval, prop, fallback} = m
  assemble.s(boost, dim, two, sh_of(vert, frag, inc, oname, oval), bgp, spp)

@unsafe
def assemble.s(boost: String, dim: Dim, two: Two, sh: Sh, bgp: Px & U32 & U32 & T.Frame, spp: Px & U32 & U32 & T.Frame) -> Run:
  (bg, sw0, sh0, fr0) = bgp
  assemble.p(boost, dim, two, sh, bg, spp)

@unsafe
def assemble.p(boost: String, +dim: Dim, two: Two, sh: Sh, bg: Px, spp: Px & U32 & U32 & T.Frame) -> Run:
  (sp, sw, shh, fr) = spp
  assemble.q(boost, dim, sh, bg, sp, quads_of(dim, two, fr, sw, shh))

@unsafe
def assemble.q(boost: String, dim: Dim, sh: Sh, bg: Px, sp: Px, r: Qs & U32) -> Run:
  (qs, fx) = r
  assemble.run(dim, sh, bg, sp, fx, boost, qs_open(qs))

@unsafe
def qs_open(qs: Qs) -> Qs & Q & Q:
  Qs{bgq, +spq, +fq} = qs
  (Qs{bgq, spq, fq}, spq, fq)

@unsafe
def assemble.run(dim: Dim, sh: Sh, bg: Px, sp: Px, fx: U32, +boost: String, r: Qs & Q & Q) -> Run:
  (qs, spq, fq) = r
  Run{plan_line(fx, spq, boost) ++ "\nFULL" ++ q_show(fq), sh, qs, F32.bits(f_lit(boost)), dim, bg, sp}

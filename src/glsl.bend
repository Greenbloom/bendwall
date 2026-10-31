import Base
import ./json.bend as J

# Wallpaper Engine shader dialect to GLSL 330.
# Combos come from // [COMBO] JSON, overridden by the caller.
# Identifiers are rewritten. saturate/mul/texSample*/CAST* stay as macros.
# stage 0 is the vertex shader (varying -> out), stage 1 is the fragment shader.

@unsafe
def macros() -> String:
  "#define saturate(x) clamp((x), 0.0, 1.0)\n#define mul(a, b) ((a) * (b))\n#define texSample2D(s, uv) texture((s), (uv))\n#define texSample2DLod(s, uv, l) textureLod((s), (uv), (l))\n#define texLoad2D(s, p) texelFetch((s), (p), 0)\n#define CAST2(x) vec2(x)\n#define CAST3(x) vec3(x)\n#define CAST4(x) vec4(x)\n"

@unsafe
def ch(c: U32) -> String:
  SCon{Chr{c}, SNil{}}

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
def empty(s: String) -> Bool & String:
  match s:
    case SNil{}:
      (True{}, SNil{})
    case SCon{c, t}:
      (False{}, SCon{c, t})

@unsafe
def eq_keep(a: String, b: String) -> Bool & String:
  eq_keep.p(dup_s(a), b)

@unsafe
def eq_keep.p(p: String & String, b: String) -> Bool & String:
  (a1, a2) = p
  (String.eq(a1, b), a2)

@unsafe
def is_digit(+c: U32) -> Bool:
  Bool.and(U32.is_ge(c, 48), U32.is_le(c, 57))

@unsafe
def is_letter(+c: U32) -> Bool:
  Bool.or(Bool.and(U32.is_ge(c, 65), U32.is_le(c, 90)), Bool.and(U32.is_ge(c, 97), U32.is_le(c, 122)))

@unsafe
def is_start(+c: U32) -> Bool:
  Bool.or(is_letter(c), U32.is_eq(c, 95))

@unsafe
def is_cont(+c: U32) -> Bool:
  Bool.or(is_start(c), is_digit(c))

@unsafe
def vary(stage: U32) -> String:
  vary.m(U32.is_eq(stage, 0))

@unsafe
def vary.m(vert: Bool) -> String:
  match vert:
    case True{}:
      "out"
    case False{}:
      "in"

@unsafe
def map.v(id: String, stage: U32, r: Bool & String) -> String:
  (eq, id2) = r
  map.v2(id2, stage, eq)

@unsafe
def map.v2(+id: String, stage: U32, eq: Bool) -> String:
  match eq:
    case True{}:
      vary(stage)
    case False{}:
      map.l(id, eq_keep(id, "lerp"))

@unsafe
def map.l(id: String, r: Bool & String) -> String:
  (eq, +id2) = r
  match eq:
    case True{}:
      "mix"
    case False{}:
      map.f(id2, eq_keep(id2, "frac"))

@unsafe
def map.f(id: String, r: Bool & String) -> String:
  (eq, +id2) = r
  match eq:
    case True{}:
      "fract"
    case False{}:
      map.dx(id2, eq_keep(id2, "ddx"))

@unsafe
def map.dx(id: String, r: Bool & String) -> String:
  (eq, +id2) = r
  match eq:
    case True{}:
      "dFdx"
    case False{}:
      map.dy(id2, eq_keep(id2, "ddy"))

@unsafe
def map.dy(id: String, r: Bool & String) -> String:
  (eq, id2) = r
  match eq:
    case True{}:
      "dFdy"
    case False{}:
      id2

@unsafe
def map.a(id: String, stage: U32, r: Bool & String) -> String:
  (eq, +id2) = r
  match eq:
    case True{}:
      "in"
    case False{}:
      map.v(id2, stage, eq_keep(id2, "varying"))

@unsafe
def map.f2(id: String, stage: U32, r: Bool & String) -> String:
  (eq, +id2) = r
  match eq:
    case True{}:
      "vec2"
    case False{}:
      map.a(id2, stage, eq_keep(id2, "attribute"))

@unsafe
def map.f3(id: String, stage: U32, r: Bool & String) -> String:
  (eq, +id2) = r
  match eq:
    case True{}:
      "vec3"
    case False{}:
      map.f2(id2, stage, eq_keep(id2, "float2"))

@unsafe
def map.f4(id: String, stage: U32, r: Bool & String) -> String:
  (eq, +id2) = r
  match eq:
    case True{}:
      "vec4"
    case False{}:
      map.f3(id2, stage, eq_keep(id2, "float3"))

@unsafe
def map.m2(id: String, stage: U32, r: Bool & String) -> String:
  (eq, +id2) = r
  match eq:
    case True{}:
      "mat2"
    case False{}:
      map.f4(id2, stage, eq_keep(id2, "float4"))

@unsafe
def map.m3(id: String, stage: U32, r: Bool & String) -> String:
  (eq, +id2) = r
  match eq:
    case True{}:
      "mat3"
    case False{}:
      map.m2(id2, stage, eq_keep(id2, "float2x2"))

@unsafe
def map.m4(id: String, stage: U32, r: Bool & String) -> String:
  (eq, +id2) = r
  match eq:
    case True{}:
      "mat4"
    case False{}:
      map.m3(id2, stage, eq_keep(id2, "float3x3"))

@unsafe
def map_id(+id: String, stage: U32) -> String:
  map.m4(id, stage, eq_keep(id, "float4x4"))

@unsafe
def rw(s: String, stage: U32, acc: String) -> String:
  match s:
    case SNil{}:
      acc
    case SCon{Chr{c}, t}:
      rw.b(t, stage, acc, c)

@unsafe
def rw.b(t: String, stage: U32, acc: String, +c: U32) -> String:
  rw.c(t, stage, acc, c, is_start(c))

@unsafe
def rw.c(t: String, stage: U32, acc: String, c: U32, start: Bool) -> String:
  match start:
    case True{}:
      rw.id(t, stage, acc, ch(c))
    case False{}:
      rw(t, stage, acc ++ ch(c))

@unsafe
def rw.id(s: String, stage: U32, acc: String, id: String) -> String:
  match s:
    case SNil{}:
      acc ++ map_id(id, stage)
    case SCon{Chr{c}, t}:
      rw.id.b(t, stage, acc, id, c)

@unsafe
def rw.id.b(t: String, stage: U32, acc: String, id: String, +c: U32) -> String:
  rw.id.c(t, stage, acc, id, c, is_cont(c))

@unsafe
def rw.id.c(t: String, +stage: U32, acc: String, id: String, c: U32, cont: Bool) -> String:
  match cont:
    case True{}:
      rw.id(t, stage, acc, id ++ ch(c))
    case False{}:
      rw(SCon{Chr{c}, t}, stage, acc ++ map_id(id, stage))

@unsafe
def starts(s: String, p: String) -> Bool & String:
  starts.d(dup_s(s), p)

@unsafe
def starts.d(ss: String & String, p: String) -> Bool & String:
  (a, b) = ss
  starts.go(a, b, p)

@unsafe
def starts.go(walk: String, orig: String, p: String) -> Bool & String:
  match p:
    case SNil{}:
      (True{}, walk)
    case SCon{Chr{c}, pt}:
      starts.ch(walk, orig, pt, c)

@unsafe
def starts.ch(walk: String, orig: String, pt: String, c: U32) -> Bool & String:
  match walk:
    case SNil{}:
      (False{}, orig)
    case SCon{Chr{d}, rest}:
      starts.eq(rest, orig, pt, U32.is_eq(c, d))

@unsafe
def starts.eq(rest: String, orig: String, pt: String, eq: Bool) -> Bool & String:
  match eq:
    case True{}:
      starts.go(rest, orig, pt)
    case False{}:
      (False{}, orig)

@unsafe
def find_after(s: String, needle: String) -> Bool & String:
  find_after.d(dup_s(needle), s)

@unsafe
def find_after.d(ns: String & String, s: String) -> Bool & String:
  (n1, n2) = ns
  find_after.t(n2, starts(s, n1))

@unsafe
def find_after.t(needle: String, r: Bool & String) -> Bool & String:
  (ok, rest) = r
  match ok:
    case True{}:
      (True{}, rest)
    case False{}:
      find_after.skip(rest, needle)

@unsafe
def find_after.skip(s: String, needle: String) -> Bool & String:
  match s:
    case SNil{}:
      (False{}, SNil{})
    case SCon{_, t}:
      find_after(t, needle)

@unsafe
def skip_line(s: String) -> String:
  match s:
    case SNil{}:
      SNil{}
    case SCon{Chr{c}, t}:
      skip_line.m(t, U32.is_eq(c, 10))

@unsafe
def skip_line.m(t: String, nl: Bool) -> String:
  match nl:
    case True{}:
      t
    case False{}:
      skip_line(t)

@unsafe
def is_ws(+c: U32) -> Bool:
  Bool.or(Bool.or(U32.is_eq(c, 32), U32.is_eq(c, 9)), Bool.or(U32.is_eq(c, 10), U32.is_eq(c, 13)))

@unsafe
def skip(s: String) -> String:
  skip.d(dup_s(s))

@unsafe
def skip.d(p: String & String) -> String:
  (a, b) = p
  skip.go(a, b)

@unsafe
def skip.go(walk: String, orig: String) -> String:
  match walk:
    case SNil{}:
      SNil{}
    case SCon{Chr{c}, t}:
      skip.m(t, orig, is_ws(c))

@unsafe
def skip.m(t: String, s: String, ws: Bool) -> String:
  match ws:
    case True{}:
      skip(t)
    case False{}:
      s

@unsafe
def grab(s: String) -> String & String:
  grab.s(skip(s))

@unsafe
def grab.p(c: U32, rest: String, ok: Bool) -> String & String:
  match ok:
    case True{}:
      grab.obj(rest, "{")
    case False{}:
      ("", SCon{Chr{c}, rest})

@unsafe
def grab.s(s: String) -> String & String:
  match s:
    case SNil{}:
      ("", SNil{})
    case SCon{Chr{+c}, rest}:
      grab.p(c, rest, U32.is_eq(c, 123))

@unsafe
def grab.obj(s: String, acc: String) -> String & String:
  match s:
    case SNil{}:
      (acc, SNil{})
    case SCon{Chr{c}, t}:
      grab.obj.c(t, acc, c)

@unsafe
def grab.obj.c(t: String, acc: String, +c: U32) -> String & String:
  grab.more(t, acc ++ ch(c), U32.is_eq(c, 125))

@unsafe
def grab.more(t: String, acc: String, end: Bool) -> String & String:
  match end:
    case True{}:
      (acc, t)
    case False{}:
      grab.obj(t, acc)

@unsafe
def field_s(j: J.J) -> String:
  J.J{k, key, s, xs} = j
  s

@unsafe
def combo_one(name: String, val: String, j: J.J) -> String & String:
  J.J{k, key, s, xs} = j
  combo_one.n(name, val, s, eq_keep(key, "combo"))

@unsafe
def combo_one.n(name: String, val: String, s: String, r: Bool & String) -> String & String:
  (eq, key) = r
  match eq:
    case True{}:
      (s, val)
    case False{}:
      combo_one.d(name, val, s, eq_keep(key, "default"))

@unsafe
def combo_one.d(name: String, val: String, s: String, r: Bool & String) -> String & String:
  (eq, key) = r
  match eq:
    case True{}:
      (name, s)
    case False{}:
      (name, val)

@unsafe
def combo_xs(xs: List<&2, J.J>, name: String, val: String) -> String & String:
  match xs:
    case Nil{}:
      (name, val)
    case Con{h, t}:
      combo_xs.s(combo_one(name, val, h), t)

@unsafe
def combo_xs.s(r: String & String, t: List<&2, J.J>) -> String & String:
  (name, val) = r
  combo_xs(t, name, val)

@unsafe
def combo_read(j: J.J) -> String & String:
  J.J{k, key, s, xs} = j
  combo_xs(xs, "", "")

@unsafe
def combo(src: String) -> String & String:
  combo.f(find_after(src, "[COMBO]"))

@unsafe
def combo.f(r: Bool & String) -> String & String:
  (ok, rest) = r
  match ok:
    case False{}:
      ("", "")
    case True{}:
      combo.g(grab(rest))

@unsafe
def combo.g(r: String & String) -> String & String:
  (json, rest) = r
  combo.e(empty(json))

@unsafe
def combo.e(r: Bool & String) -> String & String:
  (e, js) = r
  match e:
    case True{}:
      ("", "")
    case False{}:
      combo_read(J.parse(js))

@unsafe
def splice(src: String, +inc: String) -> String:
  splice.e(src, inc, empty(inc))

@unsafe
def splice.e(src: String, inc: String, r: Bool & String) -> String:
  (e, body) = r
  match e:
    case True{}:
      src
    case False{}:
      splice.find(src, "", body)

@unsafe
def splice.find(s: String, before: String, inc: String) -> String:
  splice.try(before, inc, starts(s, "#include"))

@unsafe
def splice.try(before: String, inc: String, r: Bool & String) -> String:
  (ok, rest) = r
  match ok:
    case True{}:
      before ++ inc ++ skip_line(rest)
    case False{}:
      splice.shift(before, inc, rest)

@unsafe
def splice.shift(before: String, inc: String, s: String) -> String:
  match s:
    case SNil{}:
      before
    case SCon{Chr{c}, t}:
      splice.find(t, before ++ ch(c), inc)

@unsafe
def choose(n: String, d: String, oname: String, oval: String) -> String & String:
  choose.d(d, dup_s(oval), eq_keep(n, oname))

@unsafe
def choose.d(d: String, ovs: String & String, r: Bool & String) -> String & String:
  (o1, o2) = ovs
  choose.d2(d, o1, o2, r)

@unsafe
def choose.d2(d: String, o1: String, o2: String, r: Bool & String) -> String & String:
  (same, n) = r
  choose.e(n, d, o2, same, empty(o1))

@unsafe
def choose.e(n: String, d: String, ov: String, same: Bool, er: Bool & String) -> String & String:
  (blank, o1) = er
  choose.m(n, d, ov, same, blank)

@unsafe
def choose.m(n: String, d: String, ov: String, same: Bool, blank: Bool) -> String & String:
  match same:
    case False{}:
      (n, d)
    case True{}:
      choose.b(n, d, ov, blank)

@unsafe
def choose.b(n: String, d: String, ov: String, blank: Bool) -> String & String:
  match blank:
    case True{}:
      (n, d)
    case False{}:
      (n, ov)

@unsafe
def defline(r: String & String) -> String:
  (n, v) = r
  defline.e(v, empty(n))

@unsafe
def defline.e(v: String, r: Bool & String) -> String:
  (e, name) = r
  match e:
    case True{}:
      ""
    case False{}:
      "#define " ++ name ++ " " ++ v ++ "\n"

@unsafe
def frag_out(stage: U32) -> String:
  frag_out.m(U32.is_eq(stage, 1))

@unsafe
def frag_out.m(frag: Bool) -> String:
  match frag:
    case True{}:
      "out vec4 fragColor;\n#define gl_FragColor fragColor\n"
    case False{}:
      ""

@unsafe
def header(stage: U32, picked: String & String) -> String:
  "#version 330 core\n" ++ defline(picked) ++ macros() ++ frag_out(stage)

@unsafe
def translate(src: String, stage: U32, inc: String, oname: String, oval: String) -> String:
  translate.b(splice(src, inc), stage, oname, oval)

@unsafe
def translate.b(body: String, stage: U32, oname: String, oval: String) -> String:
  translate.c(dup_s(body), stage, oname, oval)

@unsafe
def choose_pack(c: String & String, oname: String, oval: String) -> String & String:
  (n, d) = c
  choose(n, d, oname, oval)

@unsafe
def translate.c(bs: String & String, stage: U32, oname: String, oval: String) -> String:
  (b1, b2) = bs
  translate.d(b2, stage, choose_pack(combo(b1), oname, oval))

@unsafe
def translate.d(body: String, +stage: U32, picked: String & String) -> String:
  header(stage, picked) ++ rw(body, stage, "")

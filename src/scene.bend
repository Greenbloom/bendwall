import Base
import ./json.bend as J
import ./str.bend as S

# Wallpaper project, scene and material, projected from JSON.
# Prop is one user property after preset and user overlay.
# Over is one preset or user override (name, source literal).
# Obj is one scene object. Ps is one material pass.
# Resolution: base properties, then preset values by name, then user overrides.
# A bound value is lit:text, user:name:resolved, script:text or anim:text.

type Prop is Data:
  Prop{name: String, typ: String, value: String}

type Over is Data:
  Over{name: String, value: String}

type Proj is Data:
  Proj{typ: String, file: String, title: String, dep: String, audio: String, props: List<&2, Prop>, overs: List<&2, Over>}

type Obj is Data:
  Obj{name: String, kind: String, origin: String, size: String, parallax: String, alpha: String, angles: String, payload: String, effects: String}

type Sc is Data:
  Sc{w: String, h: String, clear: String, bloom: String}

type Ps is Data:
  Ps{shader: String, blend: String, combo: String, constv: String, user: String, tex: String}

type Acc is Data:
  Acc{acc: List<&2, Prop>, found: Bool, name: String, value: String}

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
def fold_type(s: String) -> String:
  fold_type.p(empty(s))

@unsafe
def fold_type.p(r: Bool & String) -> String:
  (e, s) = r
  match e:
    case True{}:
      "scene"
    case False{}:
      S.lower_str(s)

@unsafe
def open_j(j: J.J) -> List<&2, J.J>:
  J.J{k, key, s, xs} = j
  xs

@unsafe
def field_s(j: J.J) -> String:
  J.J{k, key, s, xs} = j
  s

@unsafe
def split_obj(j: J.J) -> Bool & J.J:
  J.J{+k, key, s, xs} = j
  (U32.is_eq(k, 6), J.J{k, key, s, xs})

@unsafe
def split_str(j: J.J) -> Bool & J.J:
  J.J{+k, key, s, xs} = j
  (U32.is_eq(k, 4), J.J{k, key, s, xs})

@unsafe
def dup_xs(xs: List<&2, J.J>) -> List<&2, J.J> & List<&2, J.J>:
  match xs:
    case Nil{}:
      (Nil{}, Nil{})
    case Con{h, t}:
      dup_xs.c(dup_j(h), dup_xs(t))

@unsafe
def dup_xs.c(hp: J.J & J.J, tp: List<&2, J.J> & List<&2, J.J>) -> List<&2, J.J> & List<&2, J.J>:
  (h1, h2) = hp
  (t1, t2) = tp
  (Con{h1, t1}, Con{h2, t2})

@unsafe
def dup_j(j: J.J) -> J.J & J.J:
  J.J{k, key, s, xs} = j
  dup_j.p(k, dup_s(key), dup_s(s), dup_xs(xs))

@unsafe
def dup_j.p(+k: U32, ks: String & String, ss: String & String, xs: List<&2, J.J> & List<&2, J.J>) -> J.J & J.J:
  (k1, k2) = ks
  (s1, s2) = ss
  (a, b) = xs
  (J.J{k, k1, s1, a}, J.J{k, k2, s2, b})

@unsafe
def take(j: J.J, key: String) -> J.J & J.J:
  take.d(dup_j(j), key)

@unsafe
def take.d(p: J.J & J.J, key: String) -> J.J & J.J:
  (a, b) = p
  (J.get(a, key), b)

@unsafe
def empty_list(xs: List<&2, J.J>) -> Bool & List<&2, J.J>:
  match xs:
    case Nil{}:
      (True{}, Nil{})
    case Con{h, t}:
      (False{}, Con{h, t})

@unsafe
def set_pname(p: Prop, v: String) -> Prop:
  Prop{n, t, val} = p
  Prop{v, t, val}

@unsafe
def set_ptype(p: Prop, v: String) -> Prop:
  Prop{n, t, val} = p
  Prop{n, v, val}

@unsafe
def set_pvalue(p: Prop, v: String) -> Prop:
  Prop{n, t, val} = p
  Prop{n, t, v}

@unsafe
def read_props(xs: List<&2, J.J>) -> List<&2, Prop>:
  match xs:
    case Nil{}:
      Nil{}
    case Con{h, t}:
      Con{read_prop(h), read_props(t)}

@unsafe
def read_prop(j: J.J) -> Prop:
  prop_go(Prop{"", "", ""}, open_j(j))

@unsafe
def prop_go(p: Prop, xs: List<&2, J.J>) -> Prop:
  match xs:
    case Nil{}:
      p
    case Con{h, t}:
      prop_go(prop_kv(p, h), t)

@unsafe
def prop_kv(p: Prop, j: J.J) -> Prop:
  J.J{k, key, s, xs} = j
  prop_kv.n(p, s, eq_keep(key, "name"))

@unsafe
def prop_kv.n(p: Prop, s: String, r: Bool & String) -> Prop:
  (eq, key) = r
  match eq:
    case True{}:
      set_pname(p, s)
    case False{}:
      prop_kv.t(p, s, eq_keep(key, "type"))

@unsafe
def prop_kv.t(p: Prop, s: String, r: Bool & String) -> Prop:
  (eq, key) = r
  match eq:
    case True{}:
      set_ptype(p, s)
    case False{}:
      prop_kv.v(p, s, eq_keep(key, "value"))

@unsafe
def prop_kv.v(p: Prop, s: String, r: Bool & String) -> Prop:
  (eq, key) = r
  match eq:
    case True{}:
      set_pvalue(p, s)
    case False{}:
      p

@unsafe
def gen_go(xs: List<&2, J.J>, props: List<&2, Prop>, audio: String) -> List<&2, Prop> & String:
  match xs:
    case Nil{}:
      (props, audio)
    case Con{h, t}:
      gen_go.s(gen_kv(props, audio, h), t)

@unsafe
def gen_go.s(r: List<&2, Prop> & String, t: List<&2, J.J>) -> List<&2, Prop> & String:
  (props, audio) = r
  gen_go(t, props, audio)

@unsafe
def gen_kv(props: List<&2, Prop>, audio: String, j: J.J) -> List<&2, Prop> & String:
  J.J{k, key, s, xs} = j
  gen_kv.p(props, audio, s, xs, eq_keep(key, "properties"))

@unsafe
def gen_kv.p(props: List<&2, Prop>, audio: String, s: String, xs: List<&2, J.J>, r: Bool & String) -> List<&2, Prop> & String:
  (eq, key) = r
  match eq:
    case True{}:
      (read_props(xs), audio)
    case False{}:
      gen_kv.a(props, audio, s, xs, eq_keep(key, "supportsaudioprocessing"))

@unsafe
def gen_kv.a(props: List<&2, Prop>, audio: String, s: String, xs: List<&2, J.J>, r: Bool & String) -> List<&2, Prop> & String:
  (eq, key) = r
  match eq:
    case True{}:
      (props, s)
    case False{}:
      (props, audio)

@unsafe
def pair_of(j: J.J) -> Over:
  J.J{k, key, s, xs} = j
  Over{key, s}

@unsafe
def pairs_xs(xs: List<&2, J.J>) -> List<&2, Over>:
  match xs:
    case Nil{}:
      Nil{}
    case Con{h, t}:
      Con{pair_of(h), pairs_xs(t)}

@unsafe
def pairs_of(j: J.J) -> List<&2, Over>:
  pairs_xs(open_j(j))

@unsafe
def project(j: J.J) -> Proj:
  project.ty(take(j, "type"))

@unsafe
def project.ty(r: J.J & J.J) -> Proj:
  (tv, j) = r
  project.fi(field_s(tv), take(j, "file"))

@unsafe
def project.fi(typ: String, r: J.J & J.J) -> Proj:
  (fv, j) = r
  project.ti(typ, field_s(fv), take(j, "title"))

@unsafe
def project.ti(typ: String, file: String, r: J.J & J.J) -> Proj:
  (tv, j) = r
  project.de(typ, file, field_s(tv), take(j, "dependency"))

@unsafe
def project.de(typ: String, file: String, title: String, r: J.J & J.J) -> Proj:
  (dv, j) = r
  project.ge(typ, file, title, field_s(dv), take(j, "general"))

@unsafe
def project.ge(typ: String, file: String, title: String, dep: String, r: J.J & J.J) -> Proj:
  (gv, j) = r
  project.pr(typ, file, title, dep, gen_go(open_j(gv), Nil{}, ""), take(j, "preset"))

@unsafe
def project.pr(typ: String, file: String, title: String, dep: String, g: List<&2, Prop> & String, r: J.J & J.J) -> Proj:
  (pv, j) = r
  project.end(typ, file, title, dep, g, pairs_xs(open_j(pv)))

@unsafe
def project.end(typ: String, file: String, title: String, dep: String, g: List<&2, Prop> & String, overs: List<&2, Over>) -> Proj:
  (props, audio) = g
  Proj{fold_type(typ), file, title, dep, audio, props, overs}

@unsafe
def split_proj(p: Proj) -> String & List<&2, Prop> & List<&2, Over>:
  Proj{typ, file, title, dep, audio, props, overs} = p
  ("PROJ type=" ++ typ ++ " file=" ++ file ++ " title=" ++ title ++ " dep=" ++ dep ++ " audio=" ++ audio ++ "\n", props, overs)

@unsafe
def show_prop(p: Prop) -> String:
  Prop{n, t, v} = p
  "PROP name=" ++ n ++ " type=" ++ t ++ " value=" ++ v ++ "\n"

@unsafe
def show_props(ps: List<&2, Prop>) -> String:
  match ps:
    case Nil{}:
      ""
    case Con{h, t}:
      show_prop(h) ++ show_props(t)

@unsafe
def show_pair(p: Over) -> String:
  Over{n, v} = p
  "PAIR " ++ n ++ "=" ++ v ++ "\n"

@unsafe
def show_pairs(ps: List<&2, Over>) -> String:
  match ps:
    case Nil{}:
      ""
    case Con{h, t}:
      show_pair(h) ++ show_pairs(t)

@unsafe
def rev_props(xs: List<&2, Prop>, acc: List<&2, Prop>) -> List<&2, Prop>:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      rev_props(t, Con{h, acc})

@unsafe
def apply(ps: List<&2, Prop>, os: List<&2, Over>, newtyp: String) -> List<&2, Prop>:
  match os:
    case Nil{}:
      ps
    case Con{h, t}:
      apply.step(ps, t, newtyp, h)

@unsafe
def apply.step(ps: List<&2, Prop>, t: List<&2, Over>, +newtyp: String, h: Over) -> List<&2, Prop>:
  Over{n, v} = h
  apply(set_one(ps, n, v, newtyp), t, newtyp)

@unsafe
def set_one(ps: List<&2, Prop>, name: String, value: String, newtyp: String) -> List<&2, Prop>:
  set.fin(set.go(ps, Nil{}, name, value), newtyp)

@unsafe
def set.go(ps: List<&2, Prop>, acc: List<&2, Prop>, name: String, value: String) -> Acc:
  match ps:
    case Nil{}:
      Acc{acc, False{}, name, value}
    case Con{p, t}:
      set.onep(p, t, acc, name, value)

@unsafe
def set.onep(p: Prop, t: List<&2, Prop>, acc: List<&2, Prop>, name: String, value: String) -> Acc:
  Prop{n, typ, v} = p
  set.cmp(t, acc, value, typ, v, dup_s(n), dup_s(name))

@unsafe
def set.cmp(t: List<&2, Prop>, acc: List<&2, Prop>, value: String, typ: String, v: String, ns: String & String, names: String & String) -> Acc:
  (n1, n2) = ns
  (a, b) = names
  set.cmp2(t, acc, value, typ, v, n2, b, String.eq(n1, a))

@unsafe
def set.cmp2(t: List<&2, Prop>, acc: List<&2, Prop>, value: String, typ: String, v: String, n: String, name: String, eq: Bool) -> Acc:
  match eq:
    case True{}:
      set.hit(t, acc, value, typ, n)
    case False{}:
      set.go(t, Con{Prop{n, typ, v}, acc}, name, value)

@unsafe
def set.hit(t: List<&2, Prop>, acc: List<&2, Prop>, value: String, typ: String, n: String) -> Acc:
  Acc{rev_props(t, Con{Prop{n, typ, value}, acc}), True{}, "", ""}

@unsafe
def set.fin(st: Acc, newtyp: String) -> List<&2, Prop>:
  Acc{acc, found, name, value} = st
  set.fin.m(acc, name, value, newtyp, found)

@unsafe
def set.fin.m(acc: List<&2, Prop>, name: String, value: String, newtyp: String, found: Bool) -> List<&2, Prop>:
  match found:
    case True{}:
      rev_props(acc, Nil{})
    case False{}:
      rev_props(Con{Prop{name, newtyp, value}, acc}, Nil{})

@unsafe
def dup_prop(p: Prop) -> Prop & Prop:
  Prop{n, t, v} = p
  dup_prop.p(dup_s(n), dup_s(t), dup_s(v))

@unsafe
def dup_prop.p(ns: String & String, ts: String & String, vs: String & String) -> Prop & Prop:
  (n1, n2) = ns
  (t1, t2) = ts
  (v1, v2) = vs
  (Prop{n1, t1, v1}, Prop{n2, t2, v2})

@unsafe
def dup_props(ps: List<&2, Prop>) -> List<&2, Prop> & List<&2, Prop>:
  match ps:
    case Nil{}:
      (Nil{}, Nil{})
    case Con{h, t}:
      dup_props.c(dup_prop(h), dup_props(t))

@unsafe
def dup_props.c(hp: Prop & Prop, tp: List<&2, Prop> & List<&2, Prop>) -> List<&2, Prop> & List<&2, Prop>:
  (h1, h2) = hp
  (t1, t2) = tp
  (Con{h1, t1}, Con{h2, t2})

@unsafe
def dup_pair(p: Over) -> Over & Over:
  Over{n, v} = p
  dup_pair.p(dup_s(n), dup_s(v))

@unsafe
def dup_pair.p(ns: String & String, vs: String & String) -> Over & Over:
  (n1, n2) = ns
  (v1, v2) = vs
  (Over{n1, v1}, Over{n2, v2})

@unsafe
def dup_pairs(ps: List<&2, Over>) -> List<&2, Over> & List<&2, Over>:
  match ps:
    case Nil{}:
      (Nil{}, Nil{})
    case Con{h, t}:
      dup_pairs.c(dup_pair(h), dup_pairs(t))

@unsafe
def dup_pairs.c(hp: Over & Over, tp: List<&2, Over> & List<&2, Over>) -> List<&2, Over> & List<&2, Over>:
  (h1, h2) = hp
  (t1, t2) = tp
  (Con{h1, t1}, Con{h2, t2})

@unsafe
def find(ps: List<&2, Prop>, name: String) -> String & List<&2, Prop>:
  match ps:
    case Nil{}:
      ("", Nil{})
    case Con{h, t}:
      find.c(h, t, name)

@unsafe
def find.c(p: Prop, t: List<&2, Prop>, name: String) -> String & List<&2, Prop>:
  Prop{n, typ, v} = p
  find.d(typ, v, t, dup_s(n), dup_s(name))

@unsafe
def find.d(typ: String, v: String, t: List<&2, Prop>, ns: String & String, names: String & String) -> String & List<&2, Prop>:
  (n1, n2) = ns
  (name1, name2) = names
  find.e(typ, v, t, n2, name2, String.eq(n1, name1))

@unsafe
def find.e(typ: String, v: String, t: List<&2, Prop>, n: String, name: String, eq: Bool) -> String & List<&2, Prop>:
  match eq:
    case True{}:
      find.hit(typ, v, t, n)
    case False{}:
      find.miss(typ, v, t, n, name)

@unsafe
def find.hit(typ: String, v: String, t: List<&2, Prop>, n: String) -> String & List<&2, Prop>:
  find.hit.d(typ, t, n, dup_s(v))

@unsafe
def find.hit.d(typ: String, t: List<&2, Prop>, n: String, vs: String & String) -> String & List<&2, Prop>:
  (v1, v2) = vs
  (v1, Con{Prop{n, typ, v2}, t})

@unsafe
def find.miss(typ: String, v: String, t: List<&2, Prop>, n: String, name: String) -> String & List<&2, Prop>:
  find.miss.r(typ, v, n, find(t, name))

@unsafe
def find.miss.r(typ: String, v: String, n: String, r: String & List<&2, Prop>) -> String & List<&2, Prop>:
  (val, t2) = r
  (val, Con{Prop{n, typ, v}, t2})

@unsafe
def prefer(val: String, inline: String) -> String:
  prefer.e(inline, empty(val))

@unsafe
def prefer.e(inline: String, r: Bool & String) -> String:
  (e, val) = r
  match e:
    case True{}:
      inline
    case False{}:
      val

@unsafe
def bound(j: J.J, props: List<&2, Prop>) -> String & List<&2, Prop>:
  bound.p(props, split_obj(j))

@unsafe
def bound.p(props: List<&2, Prop>, r: Bool & J.J) -> String & List<&2, Prop>:
  (obj, j) = r
  match obj:
    case True{}:
      bound.obj(props, j)
    case False{}:
      ("lit:" ++ J.lit(j), props)

@unsafe
def bound.obj(props: List<&2, Prop>, j: J.J) -> String & List<&2, Prop>:
  bound.user(props, take(j, "user"))

@unsafe
def bound.user(props: List<&2, Prop>, r: J.J & J.J) -> String & List<&2, Prop>:
  (u, j) = r
  bound.user.k(props, j, split_str(u))

@unsafe
def bound.user.k(props: List<&2, Prop>, j: J.J, r: Bool & J.J) -> String & List<&2, Prop>:
  (yes, u) = r
  match yes:
    case True{}:
      bound.us(props, j, J.lit(u))
    case False{}:
      bound.uo(props, j, split_obj(u))

@unsafe
def bound.us(props: List<&2, Prop>, j: J.J, name: String) -> String & List<&2, Prop>:
  bound.us1(j, dup_s(name), props)

@unsafe
def bound.us1(j: J.J, ns: String & String, props: List<&2, Prop>) -> String & List<&2, Prop>:
  (n1, n2) = ns
  bound.us2(j, n2, find(props, n1))

@unsafe
def bound.us2(j: J.J, name: String, r: String & List<&2, Prop>) -> String & List<&2, Prop>:
  (val, props) = r
  bound.us3(name, props, val, take(j, "value"))

@unsafe
def bound.us3(name: String, props: List<&2, Prop>, val: String, r: J.J & J.J) -> String & List<&2, Prop>:
  (v, rest) = r
  ("user:" ++ name ++ ":" ++ prefer(val, J.lit(v)), props)

@unsafe
def bound.uo(props: List<&2, Prop>, j: J.J, r: Bool & J.J) -> String & List<&2, Prop>:
  (yes, u) = r
  match yes:
    case True{}:
      bound.un(props, j, take(u, "name"))
    case False{}:
      bound.script(props, j)

@unsafe
def bound.un(props: List<&2, Prop>, j: J.J, r: J.J & J.J) -> String & List<&2, Prop>:
  (n, rest) = r
  bound.us(props, j, J.lit(n))

@unsafe
def bound.script(props: List<&2, Prop>, j: J.J) -> String & List<&2, Prop>:
  bound.script.k(props, take(j, "script"))

@unsafe
def bound.script.k(props: List<&2, Prop>, r: J.J & J.J) -> String & List<&2, Prop>:
  (sc, j) = r
  bound.script.s(props, j, split_str(sc))

@unsafe
def bound.script.s(props: List<&2, Prop>, j: J.J, r: Bool & J.J) -> String & List<&2, Prop>:
  (yes, sc) = r
  match yes:
    case True{}:
      bound.pre(props, "script:", take(j, "value"))
    case False{}:
      bound.anim(props, j, split_obj(sc))

@unsafe
def bound.anim(props: List<&2, Prop>, j: J.J, r: Bool & J.J) -> String & List<&2, Prop>:
  (yes, a) = r
  match yes:
    case True{}:
      bound.pre(props, "anim:", take(j, "value"))
    case False{}:
      bound.pre(props, "lit:", take(j, "value"))

@unsafe
def bound.pre(props: List<&2, Prop>, pfx: String, r: J.J & J.J) -> String & List<&2, Prop>:
  (v, rest) = r
  (pfx ++ J.lit(v), props)

@unsafe
def set_name(o: Obj, v: String) -> Obj:
  Obj{name, kind, origin, size, parallax, alpha, angles, payload, effects} = o
  Obj{v, kind, origin, size, parallax, alpha, angles, payload, effects}

@unsafe
def set_origin(o: Obj, v: String) -> Obj:
  Obj{name, kind, origin, size, parallax, alpha, angles, payload, effects} = o
  Obj{name, kind, v, size, parallax, alpha, angles, payload, effects}

@unsafe
def set_size(o: Obj, v: String) -> Obj:
  Obj{name, kind, origin, size, parallax, alpha, angles, payload, effects} = o
  Obj{name, kind, origin, v, parallax, alpha, angles, payload, effects}

@unsafe
def set_parallax(o: Obj, v: String) -> Obj:
  Obj{name, kind, origin, size, parallax, alpha, angles, payload, effects} = o
  Obj{name, kind, origin, size, v, alpha, angles, payload, effects}

@unsafe
def set_alpha(o: Obj, v: String) -> Obj:
  Obj{name, kind, origin, size, parallax, alpha, angles, payload, effects} = o
  Obj{name, kind, origin, size, parallax, v, angles, payload, effects}

@unsafe
def set_angles(o: Obj, v: String) -> Obj:
  Obj{name, kind, origin, size, parallax, alpha, angles, payload, effects} = o
  Obj{name, kind, origin, size, parallax, alpha, v, payload, effects}

@unsafe
def set_effects(o: Obj, v: String) -> Obj:
  Obj{name, kind, origin, size, parallax, alpha, angles, payload, effects} = o
  Obj{name, kind, origin, size, parallax, alpha, angles, payload, v}

@unsafe
def set_kp(name: String, origin: String, size: String, parallax: String, alpha: String, angles: String, pay: String, effects: String, kindname: String, payload: String, r: Bool & String) -> Obj:
  (e, kind) = r
  match e:
    case True{}:
      Obj{name, kindname, origin, size, parallax, alpha, angles, payload, effects}
    case False{}:
      Obj{name, kind, origin, size, parallax, alpha, angles, pay, effects}

@unsafe
def set_kind_payload(o: Obj, kindname: String, payload: String) -> Obj:
  Obj{name, kind, origin, size, parallax, alpha, angles, pay, effects} = o
  set_kp(name, origin, size, parallax, alpha, angles, pay, effects, kindname, payload, empty(kind))

@unsafe
def inner_value(xs: List<&2, J.J>) -> String:
  inner_x(xs, "")

@unsafe
def inner_x(xs: List<&2, J.J>, acc: String) -> String:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      inner_x(t, inner_put(acc, h))

@unsafe
def inner_put(acc: String, j: J.J) -> String:
  J.J{k, key, s, xs} = j
  inner_put.k(acc, s, eq_keep(key, "value"))

@unsafe
def inner_put.k(acc: String, s: String, r: Bool & String) -> String:
  (eq, key) = r
  match eq:
    case True{}:
      s
    case False{}:
      acc

@unsafe
def obj_set_kind(o: Obj, props: List<&2, Prop>, kindname: String, k: U32, s: String, xs: List<&2, J.J>) -> Obj & List<&2, Prop>:
  obj_set_kind.m(o, props, kindname, s, xs, U32.is_eq(k, 6))

@unsafe
def obj_set_kind.m(o: Obj, props: List<&2, Prop>, kindname: String, s: String, xs: List<&2, J.J>, obj: Bool) -> Obj & List<&2, Prop>:
  match obj:
    case True{}:
      (set_kind_payload(o, kindname, inner_value(xs)), props)
    case False{}:
      (set_kind_payload(o, kindname, s), props)

@unsafe
def file_x(xs: List<&2, J.J>, acc: String) -> String:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      file_x(t, file_put(acc, h))

@unsafe
def file_put(acc: String, j: J.J) -> String:
  J.J{k, key, s, xs} = j
  file_put.k(acc, s, eq_keep(key, "file"))

@unsafe
def file_put.k(acc: String, s: String, r: Bool & String) -> String:
  (eq, key) = r
  match eq:
    case True{}:
      s
    case False{}:
      acc

@unsafe
def file_of(j: J.J) -> String:
  file_x(open_j(j), "")

@unsafe
def effects_str(xs: List<&2, J.J>) -> String:
  effects.go(xs, 0, "")

@unsafe
def effects.go(xs: List<&2, J.J>, n: U32, file: String) -> String:
  match xs:
    case Nil{}:
      U32.show(n) ++ ":" ++ file
    case Con{h, t}:
      effects.go(t, (n + 1 : U32), effects.keep(file, h))

@unsafe
def effects.keep(file: String, h: J.J) -> String:
  effects.keep.e(h, empty(file))

@unsafe
def effects.keep.e(h: J.J, r: Bool & String) -> String:
  (e, file) = r
  match e:
    case True{}:
      file_of(h)
    case False{}:
      file

@unsafe
def obj0() -> Obj:
  Obj{"", "", "", "", "", "", "", "", ""}

@unsafe
def obj_one(j: J.J, props: List<&2, Prop>) -> Obj & List<&2, Prop>:
  obj_xs(obj0(), props, open_j(j))

@unsafe
def obj_xs(o: Obj, props: List<&2, Prop>, xs: List<&2, J.J>) -> Obj & List<&2, Prop>:
  match xs:
    case Nil{}:
      (o, props)
    case Con{h, t}:
      obj_xs.step(obj_kv(o, props, h), t)

@unsafe
def obj_xs.step(r: Obj & List<&2, Prop>, t: List<&2, J.J>) -> Obj & List<&2, Prop>:
  (o, props) = r
  obj_xs(o, props, t)

@unsafe
def obj_kv(o: Obj, props: List<&2, Prop>, j: J.J) -> Obj & List<&2, Prop>:
  J.J{k, key, s, xs} = j
  obj_name(o, props, k, s, xs, eq_keep(key, "name"))

@unsafe
def obj_name(o: Obj, props: List<&2, Prop>, k: U32, s: String, xs: List<&2, J.J>, r: Bool & String) -> Obj & List<&2, Prop>:
  (eq, key) = r
  match eq:
    case True{}:
      (set_name(o, s), props)
    case False{}:
      obj_origin(o, props, k, s, xs, eq_keep(key, "origin"))

@unsafe
def obj_origin(o: Obj, props: List<&2, Prop>, k: U32, s: String, xs: List<&2, J.J>, r: Bool & String) -> Obj & List<&2, Prop>:
  (eq, key) = r
  match eq:
    case True{}:
      (set_origin(o, s), props)
    case False{}:
      obj_size(o, props, k, s, xs, eq_keep(key, "size"))

@unsafe
def obj_size(o: Obj, props: List<&2, Prop>, k: U32, s: String, xs: List<&2, J.J>, r: Bool & String) -> Obj & List<&2, Prop>:
  (eq, key) = r
  match eq:
    case True{}:
      (set_size(o, s), props)
    case False{}:
      obj_par(o, props, k, s, xs, eq_keep(key, "parallaxDepth"))

@unsafe
def obj_par(o: Obj, props: List<&2, Prop>, k: U32, s: String, xs: List<&2, J.J>, r: Bool & String) -> Obj & List<&2, Prop>:
  (eq, key) = r
  match eq:
    case True{}:
      (set_parallax(o, s), props)
    case False{}:
      obj_al(o, props, k, s, xs, eq_keep(key, "alpha"))

@unsafe
def obj_al(o: Obj, props: List<&2, Prop>, k: U32, s: String, xs: List<&2, J.J>, r: Bool & String) -> Obj & List<&2, Prop>:
  (eq, key) = r
  match eq:
    case True{}:
      obj_alpha(o, props, k, s, xs)
    case False{}:
      obj_ang(o, props, k, s, xs, eq_keep(key, "angles"))

@unsafe
def obj_alpha(o: Obj, props: List<&2, Prop>, k: U32, s: String, xs: List<&2, J.J>) -> Obj & List<&2, Prop>:
  obj_alpha.b(o, bound(J.J{k, "", s, xs}, props))

@unsafe
def obj_alpha.b(o: Obj, r: String & List<&2, Prop>) -> Obj & List<&2, Prop>:
  (a, props) = r
  (set_alpha(o, a), props)

@unsafe
def obj_ang(o: Obj, props: List<&2, Prop>, k: U32, s: String, xs: List<&2, J.J>, r: Bool & String) -> Obj & List<&2, Prop>:
  (eq, key) = r
  match eq:
    case True{}:
      obj_angles(o, props, k, s, xs)
    case False{}:
      obj_ef(o, props, k, s, xs, eq_keep(key, "effects"))

@unsafe
def obj_angles(o: Obj, props: List<&2, Prop>, k: U32, s: String, xs: List<&2, J.J>) -> Obj & List<&2, Prop>:
  obj_angles.b(o, bound(J.J{k, "", s, xs}, props))

@unsafe
def obj_angles.b(o: Obj, r: String & List<&2, Prop>) -> Obj & List<&2, Prop>:
  (a, props) = r
  (set_angles(o, a), props)

@unsafe
def obj_ef(o: Obj, props: List<&2, Prop>, k: U32, s: String, xs: List<&2, J.J>, r: Bool & String) -> Obj & List<&2, Prop>:
  (eq, key) = r
  match eq:
    case True{}:
      (set_effects(o, effects_str(xs)), props)
    case False{}:
      obj_image(o, props, k, s, xs, eq_keep(key, "image"))

@unsafe
def obj_image(o: Obj, props: List<&2, Prop>, k: U32, s: String, xs: List<&2, J.J>, r: Bool & String) -> Obj & List<&2, Prop>:
  (eq, key) = r
  match eq:
    case True{}:
      obj_set_kind(o, props, key, k, s, xs)
    case False{}:
      obj_particle(o, props, k, s, xs, eq_keep(key, "particle"))

@unsafe
def obj_particle(o: Obj, props: List<&2, Prop>, k: U32, s: String, xs: List<&2, J.J>, r: Bool & String) -> Obj & List<&2, Prop>:
  (eq, key) = r
  match eq:
    case True{}:
      obj_set_kind(o, props, key, k, s, xs)
    case False{}:
      obj_model(o, props, k, s, xs, eq_keep(key, "model"))

@unsafe
def obj_model(o: Obj, props: List<&2, Prop>, k: U32, s: String, xs: List<&2, J.J>, r: Bool & String) -> Obj & List<&2, Prop>:
  (eq, key) = r
  match eq:
    case True{}:
      obj_set_kind(o, props, key, k, s, xs)
    case False{}:
      obj_text(o, props, k, s, xs, eq_keep(key, "text"))

@unsafe
def obj_text(o: Obj, props: List<&2, Prop>, k: U32, s: String, xs: List<&2, J.J>, r: Bool & String) -> Obj & List<&2, Prop>:
  (eq, key) = r
  match eq:
    case True{}:
      obj_set_kind(o, props, key, k, s, xs)
    case False{}:
      obj_sound(o, props, k, s, xs, eq_keep(key, "sound"))

@unsafe
def obj_sound(o: Obj, props: List<&2, Prop>, k: U32, s: String, xs: List<&2, J.J>, r: Bool & String) -> Obj & List<&2, Prop>:
  (eq, key) = r
  match eq:
    case True{}:
      obj_set_kind(o, props, key, k, s, xs)
    case False{}:
      obj_light(o, props, k, s, xs, eq_keep(key, "light"))

@unsafe
def obj_light(o: Obj, props: List<&2, Prop>, k: U32, s: String, xs: List<&2, J.J>, r: Bool & String) -> Obj & List<&2, Prop>:
  (eq, key) = r
  match eq:
    case True{}:
      obj_set_kind(o, props, key, k, s, xs)
    case False{}:
      (o, props)

@unsafe
def obj_done(o: Obj) -> Obj:
  Obj{name, kind, origin, size, parallax, alpha, angles, payload, effects} = o
  obj_done.e(name, kind, origin, size, parallax, alpha, angles, payload, empty(effects))

@unsafe
def obj_done.e(name: String, kind: String, origin: String, size: String, parallax: String, alpha: String, angles: String, payload: String, r: Bool & String) -> Obj:
  (e, effects) = r
  match e:
    case True{}:
      Obj{name, kind, origin, size, parallax, alpha, angles, payload, "0:"}
    case False{}:
      Obj{name, kind, origin, size, parallax, alpha, angles, payload, effects}

@unsafe
def obj_line(o: Obj) -> String:
  Obj{name, kind, origin, size, parallax, alpha, angles, payload, effects} = o
  "OBJ name=" ++ name ++ " kind=" ++ kind ++ " origin=" ++ origin ++ " size=" ++ size ++ " parallax=" ++ parallax ++ " alpha=" ++ alpha ++ " angles=" ++ angles ++ " payload=" ++ payload ++ " effects=" ++ effects ++ "\n"

@unsafe
def obj_go(xs: List<&2, J.J>, props: List<&2, Prop>, acc: String) -> String & List<&2, Prop>:
  match xs:
    case Nil{}:
      (acc, props)
    case Con{h, t}:
      obj_go.step(obj_one(h, props), t, acc)

@unsafe
def obj_go.step(r: Obj & List<&2, Prop>, t: List<&2, J.J>, acc: String) -> String & List<&2, Prop>:
  (o, props) = r
  obj_go(t, props, acc ++ obj_line(obj_done(o)))

@unsafe
def obj_list(xs: List<&2, J.J>, props: List<&2, Prop>) -> String:
  obj_list.f(obj_go(xs, props, ""))

@unsafe
def obj_list.f(r: String & List<&2, Prop>) -> String:
  (s, ps) = r
  s

@unsafe
def cam_x(xs: List<&2, J.J>, acc: String) -> String:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      cam_x(t, cam_put(acc, h))

@unsafe
def cam_put(acc: String, j: J.J) -> String:
  J.J{k, key, s, xs} = j
  cam_put.k(acc, s, eq_keep(key, "center"))

@unsafe
def cam_put.k(acc: String, s: String, r: Bool & String) -> String:
  (eq, key) = r
  match eq:
    case True{}:
      s
    case False{}:
      acc

@unsafe
def cam_center(j: J.J) -> String:
  cam_x(open_j(j), "")

@unsafe
def wh(xs: List<&2, J.J>) -> String & String:
  wh.x(xs, "", "")

@unsafe
def wh.x(xs: List<&2, J.J>, w: String, h: String) -> String & String:
  match xs:
    case Nil{}:
      (w, h)
    case Con{c, t}:
      wh.step(t, wh.kv(w, h, c))

@unsafe
def wh.step(t: List<&2, J.J>, r: String & String) -> String & String:
  (w, h) = r
  wh.x(t, w, h)

@unsafe
def wh.kv(w: String, h: String, j: J.J) -> String & String:
  J.J{k, key, s, xs} = j
  wh.k(w, h, s, eq_keep(key, "width"))

@unsafe
def wh.k(w: String, h: String, s: String, r: Bool & String) -> String & String:
  (eq, key) = r
  match eq:
    case True{}:
      (s, h)
    case False{}:
      wh.h(w, h, s, eq_keep(key, "height"))

@unsafe
def wh.h(w: String, h: String, s: String, r: Bool & String) -> String & String:
  (eq, key) = r
  match eq:
    case True{}:
      (w, s)
    case False{}:
      (w, h)

@unsafe
def set_clear(sc: Sc, v: String) -> Sc:
  Sc{w, h, clear, bloom} = sc
  Sc{w, h, v, bloom}

@unsafe
def set_bloom(sc: Sc, v: String) -> Sc:
  Sc{w, h, clear, bloom} = sc
  Sc{w, h, clear, v}

@unsafe
def sc0() -> Sc:
  Sc{"", "", "", ""}

@unsafe
def gen_scene(j: J.J) -> Sc:
  gen_scene.x(open_j(j), sc0())

@unsafe
def gen_scene.x(xs: List<&2, J.J>, sc: Sc) -> Sc:
  match xs:
    case Nil{}:
      sc
    case Con{h, t}:
      gen_scene.x(t, gen_scene.kv(sc, h))

@unsafe
def gen_scene.kv(sc: Sc, j: J.J) -> Sc:
  J.J{k, key, s, xs} = j
  gen_scene.op(sc, s, xs, eq_keep(key, "orthogonalprojection"))

@unsafe
def gen_scene.op(sc: Sc, s: String, xs: List<&2, J.J>, r: Bool & String) -> Sc:
  (eq, key) = r
  match eq:
    case True{}:
      gen_scene.wh(sc, wh(xs))
    case False{}:
      gen_scene.cl(sc, s, eq_keep(key, "clearcolor"))

@unsafe
def gen_scene.wh(sc: Sc, r: String & String) -> Sc:
  Sc{ow, oh, clear, bloom} = sc
  gen_scene.wh2(clear, bloom, r)

@unsafe
def gen_scene.wh2(clear: String, bloom: String, r: String & String) -> Sc:
  (w, h) = r
  Sc{w, h, clear, bloom}

@unsafe
def gen_scene.cl(sc: Sc, s: String, r: Bool & String) -> Sc:
  (eq, key) = r
  match eq:
    case True{}:
      set_clear(sc, s)
    case False{}:
      gen_scene.bl(sc, s, eq_keep(key, "bloom"))

@unsafe
def gen_scene.bl(sc: Sc, s: String, r: Bool & String) -> Sc:
  (eq, key) = r
  match eq:
    case True{}:
      set_bloom(sc, s)
    case False{}:
      sc

@unsafe
def show_scene(j: J.J, props: List<&2, Prop>) -> String:
  show_scene.cam(props, take(j, "camera"))

@unsafe
def show_scene.cam(props: List<&2, Prop>, r: J.J & J.J) -> String:
  (c, j) = r
  show_scene.gen(props, cam_center(c), take(j, "general"))

@unsafe
def show_scene.gen(props: List<&2, Prop>, cam: String, r: J.J & J.J) -> String:
  (g, j) = r
  show_scene.ob(props, cam, gen_scene(g), take(j, "objects"))

@unsafe
def show_scene.ob(props: List<&2, Prop>, cam: String, sc: Sc, r: J.J & J.J) -> String:
  (o, j) = r
  show_scene.end(props, cam, sc, open_j(o))

@unsafe
def show_scene.end(props: List<&2, Prop>, cam: String, sc: Sc, xs: List<&2, J.J>) -> String:
  Sc{w, h, clear, bloom} = sc
  "SCN w=" ++ w ++ " h=" ++ h ++ " clear=" ++ clear ++ " bloom=" ++ bloom ++ " cam=" ++ cam ++ "\n" ++ obj_list(xs, props)

@unsafe
def kv_one(j: J.J) -> String:
  J.J{k, key, s, xs} = j
  key ++ ":" ++ s

@unsafe
def kv_join(xs: List<&2, J.J>) -> String:
  match xs:
    case Nil{}:
      ""
    case Con{h, t}:
      kv_more(kv_one(h), t)

@unsafe
def kv_more(a: String, t: List<&2, J.J>) -> String:
  kv_more.e(a, empty_list(t))

@unsafe
def kv_more.e(a: String, r: Bool & List<&2, J.J>) -> String:
  (e, t) = r
  match e:
    case True{}:
      a
    case False{}:
      a ++ "," ++ kv_join(t)

@unsafe
def tex_one(j: J.J) -> String:
  J.J{k, key, s, xs} = j
  s

@unsafe
def tex_join(xs: List<&2, J.J>) -> String:
  match xs:
    case Nil{}:
      ""
    case Con{h, t}:
      tex_more(tex_one(h), t)

@unsafe
def tex_more(a: String, t: List<&2, J.J>) -> String:
  tex_more.e(a, empty_list(t))

@unsafe
def tex_more.e(a: String, r: Bool & List<&2, J.J>) -> String:
  (e, t) = r
  match e:
    case True{}:
      a
    case False{}:
      a ++ "," ++ tex_join(t)

@unsafe
def set_shader(p: Ps, v: String) -> Ps:
  Ps{shader, blend, combo, constv, user, tex} = p
  Ps{v, blend, combo, constv, user, tex}

@unsafe
def set_blend(p: Ps, v: String) -> Ps:
  Ps{shader, blend, combo, constv, user, tex} = p
  Ps{shader, v, combo, constv, user, tex}

@unsafe
def set_combo(p: Ps, v: String) -> Ps:
  Ps{shader, blend, combo, constv, user, tex} = p
  Ps{shader, blend, v, constv, user, tex}

@unsafe
def set_const(p: Ps, v: String) -> Ps:
  Ps{shader, blend, combo, constv, user, tex} = p
  Ps{shader, blend, combo, v, user, tex}

@unsafe
def set_user(p: Ps, v: String) -> Ps:
  Ps{shader, blend, combo, constv, user, tex} = p
  Ps{shader, blend, combo, constv, v, tex}

@unsafe
def set_tex(p: Ps, v: String) -> Ps:
  Ps{shader, blend, combo, constv, user, tex} = p
  Ps{shader, blend, combo, constv, user, v}

@unsafe
def ps0() -> Ps:
  Ps{"", "", "", "", "", ""}

@unsafe
def pass_show(p: Ps) -> String:
  Ps{shader, blend, combo, constv, user, tex} = p
  " shader=" ++ shader ++ " blend=" ++ blend ++ " combo=" ++ combo ++ " const=" ++ constv ++ " user=" ++ user ++ " tex=" ++ tex

@unsafe
def pass_go(p: Ps, xs: List<&2, J.J>) -> Ps:
  match xs:
    case Nil{}:
      p
    case Con{h, t}:
      pass_go(pass_kv(p, h), t)

@unsafe
def pass_kv(p: Ps, j: J.J) -> Ps:
  J.J{k, key, s, xs} = j
  pass_sh(p, s, xs, eq_keep(key, "shader"))

@unsafe
def pass_sh(p: Ps, s: String, xs: List<&2, J.J>, r: Bool & String) -> Ps:
  (eq, key) = r
  match eq:
    case True{}:
      set_shader(p, s)
    case False{}:
      pass_bl(p, s, xs, eq_keep(key, "blending"))

@unsafe
def pass_bl(p: Ps, s: String, xs: List<&2, J.J>, r: Bool & String) -> Ps:
  (eq, key) = r
  match eq:
    case True{}:
      set_blend(p, s)
    case False{}:
      pass_co(p, s, xs, eq_keep(key, "combos"))

@unsafe
def pass_co(p: Ps, s: String, xs: List<&2, J.J>, r: Bool & String) -> Ps:
  (eq, key) = r
  match eq:
    case True{}:
      set_combo(p, kv_join(xs))
    case False{}:
      pass_cv(p, s, xs, eq_keep(key, "constantshadervalues"))

@unsafe
def pass_cv(p: Ps, s: String, xs: List<&2, J.J>, r: Bool & String) -> Ps:
  (eq, key) = r
  match eq:
    case True{}:
      set_const(p, kv_join(xs))
    case False{}:
      pass_us(p, s, xs, eq_keep(key, "usershadervalues"))

@unsafe
def pass_us(p: Ps, s: String, xs: List<&2, J.J>, r: Bool & String) -> Ps:
  (eq, key) = r
  match eq:
    case True{}:
      set_user(p, kv_join(xs))
    case False{}:
      pass_tx(p, s, xs, eq_keep(key, "textures"))

@unsafe
def pass_tx(p: Ps, s: String, xs: List<&2, J.J>, r: Bool & String) -> Ps:
  (eq, key) = r
  match eq:
    case True{}:
      set_tex(p, tex_join(xs))
    case False{}:
      p

@unsafe
def pass_one(j: J.J) -> String:
  pass_show(pass_go(ps0(), open_j(j)))

@unsafe
def pass_list(xs: List<&2, J.J>, n: U32, acc: String) -> String:
  match xs:
    case Nil{}:
      "passes=" ++ U32.show(n) ++ acc ++ "\n"
    case Con{h, t}:
      pass_list(t, (n + 1 : U32), acc ++ pass_one(h))

@unsafe
def show_mat(j: J.J) -> String:
  show_mat.t(take(j, "passes"))

@unsafe
def show_mat.t(r: J.J & J.J) -> String:
  (p, rest) = r
  "MAT " ++ pass_list(open_j(p), 0, "")

@unsafe
def proj_text(p: Proj) -> String:
  proj_text.h(split_proj(p))

@unsafe
def proj_text.h(r: String & List<&2, Prop> & List<&2, Over>) -> String:
  (h, props, overs) = r
  h ++ show_props(props)

@unsafe
def report(base: String, preset: String, user: String, notype: String, scene: String, mat: String) -> String:
  report.a(project(J.parse(base)), project(J.parse(preset)), pairs_of(J.parse(user)), project(J.parse(notype)), J.parse(scene), J.parse(mat))

@unsafe
def report.a(bp: Proj, pp: Proj, upairs: List<&2, Over>, np: Proj, sj: J.J, mj: J.J) -> String:
  report.c(split_proj(bp), split_proj(pp), upairs, split_proj(np), sj, mj)

@unsafe
def report.c(br: String & List<&2, Prop> & List<&2, Over>, pr: String & List<&2, Prop> & List<&2, Over>, upairs: List<&2, Over>, nr: String & List<&2, Prop> & List<&2, Over>, sj: J.J, mj: J.J) -> String:
  (bh, bprops, bovers) = br
  report.p(bh, bprops, pr, upairs, nr, sj, mj)

@unsafe
def report.p(bh: String, bprops: List<&2, Prop>, pr: String & List<&2, Prop> & List<&2, Over>, upairs: List<&2, Over>, nr: String & List<&2, Prop> & List<&2, Over>, sj: J.J, mj: J.J) -> String:
  (ph, pprops, povers) = pr
  report.d(bh, bprops, ph, pprops, dup_pairs(povers), upairs, nr, sj, mj)

@unsafe
def report.d(bh: String, bprops: List<&2, Prop>, ph: String, pprops: List<&2, Prop>, r: List<&2, Over> & List<&2, Over>, upairs: List<&2, Over>, nr: String & List<&2, Prop> & List<&2, Over>, sj: J.J, mj: J.J) -> String:
  (oshow, oapply) = r
  report.b(bh, ph, pprops, oshow, dup_props(bprops), oapply, upairs, nr, sj, mj)

@unsafe
def report.b(bh: String, ph: String, pprops: List<&2, Prop>, oshow: List<&2, Over>, r: List<&2, Prop> & List<&2, Prop>, oapply: List<&2, Over>, upairs: List<&2, Over>, nr: String & List<&2, Prop> & List<&2, Over>, sj: J.J, mj: J.J) -> String:
  (bshow, bapply) = r
  report.u(bh, bshow, ph, pprops, oshow, apply(apply(bapply, oapply, "preset"), upairs, "user"), nr, sj, mj)

@unsafe
def report.u(bh: String, bshow: List<&2, Prop>, ph: String, pprops: List<&2, Prop>, oshow: List<&2, Over>, resolved: List<&2, Prop>, nr: String & List<&2, Prop> & List<&2, Over>, sj: J.J, mj: J.J) -> String:
  report.ud(bh, bshow, ph, pprops, oshow, dup_props(resolved), nr, sj, mj)

@unsafe
def report.ud(bh: String, bshow: List<&2, Prop>, ph: String, pprops: List<&2, Prop>, oshow: List<&2, Over>, r: List<&2, Prop> & List<&2, Prop>, nr: String & List<&2, Prop> & List<&2, Over>, sj: J.J, mj: J.J) -> String:
  (rshow, rscene) = r
  report.out(bh, bshow, ph, pprops, oshow, rshow, rscene, nr, sj, mj)

@unsafe
def report.out(bh: String, bshow: List<&2, Prop>, ph: String, pprops: List<&2, Prop>, oshow: List<&2, Over>, rshow: List<&2, Prop>, rscene: List<&2, Prop>, nr: String & List<&2, Prop> & List<&2, Over>, sj: J.J, mj: J.J) -> String:
  "BASE\n" ++ bh ++ show_props(bshow) ++ "PRESET\n" ++ ph ++ show_props(pprops) ++ show_pairs(oshow) ++ "RESOLVED\n" ++ show_props(rshow) ++ "NOTYPE\n" ++ proj_text.nr(nr) ++ "SCENE\n" ++ show_scene(sj, rscene) ++ "MAT\n" ++ show_mat(mj)

@unsafe
def proj_text.nr(r: String & List<&2, Prop> & List<&2, Over>) -> String:
  (h, props, overs) = r
  h ++ show_props(props)

@unsafe
def bom_report(s: String) -> String:
  "BOM\n" ++ proj_text(project(J.parse(s)))

@unsafe
def codes(s: String) -> String:
  match s:
    case SNil{}:
      ""
    case SCon{Chr{c}, t}:
      codes.m(U32.show(c), t)

@unsafe
def codes.m(a: String, t: String) -> String:
  codes.e(a, empty(t))

@unsafe
def codes.e(a: String, r: Bool & String) -> String:
  (e, rest) = r
  match e:
    case True{}:
      a
    case False{}:
      a ++ "," ++ codes(rest)

@unsafe
def esc_report(s: String) -> String:
  esc_a(take(J.parse(s), "a"))

@unsafe
def esc_a(r: J.J & J.J) -> String:
  (a, j) = r
  esc_b(field_s(a), take(j, "b"))

@unsafe
def esc_b(ac: String, r: J.J & J.J) -> String:
  (b, j) = r
  "ESC a=" ++ codes(ac) ++ "\nESC b=" ++ codes(field_s(b)) ++ "\n"

import Base

# Which object kinds and effect names a scene still does not run.
# Image layers and text layers are omitted. Effect names come from effects/<name>/.
# Kind order is fixed: particle, model, sound, light. Then effect names in file order.

type Bite is Data:
  Bite{c: U32, rest: String, ok: U32}

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
def empty(s: String) -> Bool & String:
  match s:
    case SNil{}:
      (True{}, SNil{})
    case SCon{c, t}:
      (False{}, SCon{c, t})

@unsafe
def bite(s: String) -> Bite:
  match s:
    case SNil{}:
      Bite{0, SNil{}, 0}
    case SCon{Chr{c}, rest}:
      Bite{c, rest, 1}

@unsafe
def dupat(xs: List<&2, U32>) -> List<&2, U32> & List<&2, U32>:
  match xs:
    case Nil{}:
      (Nil{}, Nil{})
    case Con{h, t}:
      dupat.c(h, dupat(t))

@unsafe
def dupat.c(+h: U32, p: List<&2, U32> & List<&2, U32>) -> List<&2, U32> & List<&2, U32>:
  (a, b) = p
  (Con{h, a}, Con{h, b})

@unsafe
def plen(xs: List<&2, U32>) -> U32:
  match xs:
    case Nil{}:
      0
    case Con{_, t}:
      (plen(t) + 1 : U32)

@unsafe
def nth(xs: List<&2, U32>, +n: U32) -> U32:
  nth.m(xs, n, U32.is_zero(n))

@unsafe
def nth.m(xs: List<&2, U32>, +n: U32, z: Bool) -> U32:
  match z:
    case True{}:
      nth.h(xs)
    case False{}:
      nth.t(xs, n)

@unsafe
def nth.h(xs: List<&2, U32>) -> U32:
  match xs:
    case Nil{}:
      0
    case Con{h, _}:
      h

@unsafe
def nth.t(xs: List<&2, U32>, +n: U32) -> U32:
  match xs:
    case Nil{}:
      0
    case Con{_, t}:
      nth(t, (n - 1 : U32))

@unsafe
def adv2(+n: U32, +len: U32) -> U32:
  adv3(n, U32.is_eq(n, len))

@unsafe
def adv3(+n: U32, done: Bool) -> U32:
  match done:
    case True{}:
      100
    case False{}:
      n

@unsafe
def re_m(+c: U32, +first: U32) -> U32:
  re_b(U32.is_eq(c, first))

@unsafe
def re_b(eq: Bool) -> U32:
  match eq:
    case True{}:
      1
    case False{}:
      0

@unsafe
def step_cmp(+st: U32, +c: U32, +len: U32, +first: U32, eq: Bool) -> U32:
  match eq:
    case True{}:
      adv2((st + 1 : U32), len)
    case False{}:
      re_m(c, first)

@unsafe
def step_w(+st: U32, +c: U32, +len: U32, +first: U32, p: U32 & List<&2, U32>) -> U32:
  (want, rest) = p
  step_w2(st, c, len, first, want, dropu(rest))

@unsafe
def dropu(xs: List<&2, U32>) -> U32:
  match xs:
    case Nil{}:
      0
    case Con{_, t}:
      dropu(t)

@unsafe
def step_w2(+st: U32, +c: U32, +len: U32, +first: U32, +want: U32, +gone: U32) -> U32:
  step_cmp(st, c, len, first, U32.is_eq(c, want))

@unsafe
def step_nth(+st: U32, +c: U32, +len: U32, +first: U32, pat: List<&2, U32>) -> U32:
  step_w(st, c, len, first, nth_keep(pat, st))

@unsafe
def nth_keep(pat: List<&2, U32>, +st: U32) -> U32 & List<&2, U32>:
  nth_keep.d(st, dupat(pat))

@unsafe
def nth_keep.d(+st: U32, p: List<&2, U32> & List<&2, U32>) -> U32 & List<&2, U32>:
  (a, b) = p
  (nth(a, st), b)

@unsafe
def step_l(+st: U32, +c: U32, +first: U32, p: U32 & List<&2, U32>) -> U32:
  (len, pat) = p
  step_nth(st, c, len, first, pat)

@unsafe
def len_keep(pat: List<&2, U32>) -> U32 & List<&2, U32>:
  len_keep.d(dupat(pat))

@unsafe
def len_keep.d(p: List<&2, U32> & List<&2, U32>) -> U32 & List<&2, U32>:
  (a, b) = p
  (plen(a), b)

@unsafe
def step_at(+st: U32, +c: U32, +first: U32, pat: List<&2, U32>) -> U32:
  step_l(st, c, first, len_keep(pat))

@unsafe
def seek(s: String, pat: List<&2, U32>, +first: U32, +st: U32) -> U32:
  seek.p(bite(s), pat, first, st)

@unsafe
def seek.p(p: Bite, pat: List<&2, U32>, +first: U32, +st: U32) -> U32:
  Bite{+c, rest, +ok} = p
  seek.e(c, rest, pat, first, st, U32.is_zero(ok))

@unsafe
def seek.e(+c: U32, rest: String, pat: List<&2, U32>, +first: U32, +st: U32, end: Bool) -> U32:
  match end:
    case True{}:
      seek.done(drop_s(rest), dropu(pat))
    case False{}:
      seek.e2(c, rest, first, st, dupat(pat))

@unsafe
def seek.done(+a: U32, +b: U32) -> U32:
  0

@unsafe
def seek.e2(+c: U32, rest: String, +first: U32, +st: U32, p: List<&2, U32> & List<&2, U32>) -> U32:
  (a, b) = p
  seek.n(rest, b, first, step_at(st, c, first, a))

@unsafe
def seek.n(rest: String, pat: List<&2, U32>, +first: U32, +st: U32) -> U32:
  seek.d(rest, pat, first, st, U32.is_eq(st, 100))

@unsafe
def seek.d(rest: String, pat: List<&2, U32>, +first: U32, +st: U32, done: Bool) -> U32:
  match done:
    case True{}:
      1
    case False{}:
      seek(rest, pat, first, st)

@unsafe
def hit(+n: U32) -> Bool:
  hit.m(U32.is_zero(n))

@unsafe
def hit.m(z: Bool) -> Bool:
  match z:
    case True{}:
      False{}
    case False{}:
      True{}

@unsafe
def qkey(xs: List<&2, U32>) -> List<&2, U32>:
  Con{34, xs}

@unsafe
def colon(xs: List<&2, U32>) -> List<&2, U32>:
  colon.go(xs, Nil{})

@unsafe
def colon.go(xs: List<&2, U32>, acc: List<&2, U32>) -> List<&2, U32>:
  match xs:
    case Nil{}:
      colon.end(acc)
    case Con{h, t}:
      colon.go(t, Con{h, acc})

@unsafe
def colon.end(acc: List<&2, U32>) -> List<&2, U32>:
  colon.rev(acc, Con{34, Con{58, Nil{}}})

@unsafe
def colon.rev(xs: List<&2, U32>, acc: List<&2, U32>) -> List<&2, U32>:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      colon.rev(t, Con{h, acc})

@unsafe
def bytes(s: String) -> List<&2, U32>:
  bytes.go(s, Nil{})

@unsafe
def bytes.go(s: String, acc: List<&2, U32>) -> List<&2, U32>:
  match s:
    case SNil{}:
      revu(acc)
    case SCon{Chr{c}, t}:
      bytes.go(t, Con{c, acc})

@unsafe
def revu(xs: List<&2, U32>) -> List<&2, U32>:
  revu.go(xs, Nil{})

@unsafe
def revu.go(xs: List<&2, U32>, acc: List<&2, U32>) -> List<&2, U32>:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      revu.go(t, Con{h, acc})

@unsafe
def keypat(name: String) -> List<&2, U32>:
  qkey(colon(bytes(name)))

@unsafe
def has_key(s: String, name: String) -> Bool:
  hit(seek(s, keypat(name), 34, 0))

@unsafe
def add(acc: String, w: String) -> String:
  add.e(w, empty(acc))

@unsafe
def add.e(w: String, p: Bool & String) -> String:
  (e, acc) = p
  add.m(acc, w, e)

@unsafe
def add.m(acc: String, w: String, e: Bool) -> String:
  match e:
    case True{}:
      w
    case False{}:
      acc ++ " " ++ w

@unsafe
def kinds(s: String) -> String:
  kinds.p(pair(s))

@unsafe
def kinds.p(p: String & String) -> String:
  (a, b) = p
  kinds.particle(b, has_key(a, "particle"))

@unsafe
def kinds.particle(s: String, hitk: Bool) -> String:
  match hitk:
    case True{}:
      kinds.model(s, "particle")
    case False{}:
      kinds.model(s, "")

@unsafe
def kinds.model(s: String, acc: String) -> String:
  kinds.model2(acc, pair(s))

@unsafe
def kinds.model2(acc: String, p: String & String) -> String:
  (a, b) = p
  kinds.model3(b, acc, has_key(a, "model"))

@unsafe
def kinds.model3(s: String, acc: String, hitk: Bool) -> String:
  match hitk:
    case True{}:
      kinds.sound(s, add(acc, "model"))
    case False{}:
      kinds.sound(s, acc)

@unsafe
def kinds.sound(s: String, acc: String) -> String:
  kinds.sound2(acc, pair(s))

@unsafe
def kinds.sound2(acc: String, p: String & String) -> String:
  (a, b) = p
  kinds.sound3(b, acc, has_key(a, "sound"))

@unsafe
def kinds.sound3(s: String, acc: String, hitk: Bool) -> String:
  match hitk:
    case True{}:
      kinds.light(s, add(acc, "sound"))
    case False{}:
      kinds.light(s, acc)

@unsafe
def kinds.light(s: String, acc: String) -> String:
  kinds.light2(acc, pair(s))

@unsafe
def kinds.light2(acc: String, p: String & String) -> String:
  (a, b) = p
  kinds.light3(drop_s(b), acc, has_key(a, "light"))

@unsafe
def drop_s(s: String) -> U32:
  match s:
    case SNil{}:
      0
    case SCon{_, t}:
      drop_s(t)

@unsafe
def kinds.light3(+gone: U32, acc: String, hitk: Bool) -> String:
  match hitk:
    case True{}:
      add(acc, "light")
    case False{}:
      acc

@unsafe
def push(s: String, +c: U32) -> String:
  s ++ SCon{Chr{c}, SNil{}}

@unsafe
def word_has(acc: String, w: String) -> Bool:
  word_has.p(acc, w)

@unsafe
def word_has.p(acc: String, w: String) -> Bool:
  word_has.e(w, empty(acc))

@unsafe
def word_has.e(w: String, p: Bool & String) -> Bool:
  (e, acc) = p
  word_has.m(acc, w, e)

@unsafe
def word_has.m(acc: String, w: String, e: Bool) -> Bool:
  match e:
    case True{}:
      False{}
    case False{}:
      word_has.s(w, split(acc))

@unsafe
def split(s: String) -> String & String:
  split.go(s, SNil{})

@unsafe
def split.go(s: String, acc: String) -> String & String:
  match s:
    case SNil{}:
      (rev(acc), SNil{})
    case SCon{Chr{+c}, t}:
      split.c(t, acc, c, U32.is_eq(c, 32))

@unsafe
def split.c(t: String, acc: String, +c: U32, sp: Bool) -> String & String:
  match sp:
    case True{}:
      (rev(acc), t)
    case False{}:
      split.go(t, SCon{Chr{c}, acc})

@unsafe
def rev(s: String) -> String:
  rev.go(s, SNil{})

@unsafe
def rev.go(s: String, acc: String) -> String:
  match s:
    case SNil{}:
      acc
    case SCon{c, t}:
      rev.go(t, SCon{c, acc})

@unsafe
def word_has.s(w: String, p: String & String) -> Bool:
  (h, rest) = p
  word_has.h(rest, w, h)

@unsafe
def word_has.h(rest: String, w: String, h: String) -> Bool:
  word_has.q(rest, h, pair(w))

@unsafe
def word_has.q(rest: String, h: String, p: String & String) -> Bool:
  (w1, w2) = p
  word_has.r(rest, w2, String.eq(h, w1))

@unsafe
def word_has.r(rest: String, w: String, eq: Bool) -> Bool:
  match eq:
    case True{}:
      True{}
    case False{}:
      word_has.p(rest, w)

@unsafe
def add_new(acc: String, w: String) -> String:
  add_new.d(w, pair(acc))

@unsafe
def add_new.d(w: String, p: String & String) -> String:
  (a, b) = p
  add_new.w(a, b, pair(w))

@unsafe
def add_new.w(a: String, b: String, p: String & String) -> String:
  (w1, w2) = p
  add_new.h(b, w2, word_has(a, w1))

@unsafe
def add_new.h(acc: String, w: String, hitw: Bool) -> String:
  match hitw:
    case True{}:
      acc
    case False{}:
      add(acc, w)

@unsafe
def commit(acc: String, name: String) -> String:
  commit.e(acc, empty(name))

@unsafe
def commit.e(acc: String, p: Bool & String) -> String:
  (e, name) = p
  commit.m(acc, name, e)

@unsafe
def commit.m(acc: String, name: String, e: Bool) -> String:
  match e:
    case True{}:
      acc
    case False{}:
      add_new(acc, "effect:" ++ name)

@unsafe
def end_name(+c: U32) -> Bool:
  Bool.or(Bool.or(U32.is_eq(c, 47), U32.is_eq(c, 34)), Bool.or(U32.is_eq(c, 32), U32.is_eq(c, 10)))

@unsafe
def fx_step(+st: U32, +c: U32) -> U32:
  fx_step.z(st, c, U32.is_zero(st))

@unsafe
def fx_step.z(+st: U32, +c: U32, z: Bool) -> U32:
  match z:
    case True{}:
      fx_e(c)
    case False{}:
      fx_step.a(st, c, U32.is_eq(st, 1))

@unsafe
def fx_e(+c: U32) -> U32:
  fx_e.m(U32.is_eq(c, 101))

@unsafe
def fx_e.m(eq: Bool) -> U32:
  match eq:
    case True{}:
      1
    case False{}:
      0

@unsafe
def fx_expect(+c: U32, +want: U32, +nxt: U32) -> U32:
  fx_expect.m(c, nxt, U32.is_eq(c, want))

@unsafe
def fx_expect.m(+c: U32, +nxt: U32, eq: Bool) -> U32:
  match eq:
    case True{}:
      nxt
    case False{}:
      fx_e(c)

@unsafe
def fx_step.a(+st: U32, +c: U32, hitst: Bool) -> U32:
  match hitst:
    case True{}:
      fx_expect(c, 102, 2)
    case False{}:
      fx_step.b(st, c, U32.is_eq(st, 2))

@unsafe
def fx_step.b(+st: U32, +c: U32, hitst: Bool) -> U32:
  match hitst:
    case True{}:
      fx_expect(c, 102, 3)
    case False{}:
      fx_step.c(st, c, U32.is_eq(st, 3))

@unsafe
def fx_step.c(+st: U32, +c: U32, hitst: Bool) -> U32:
  match hitst:
    case True{}:
      fx_expect(c, 101, 4)
    case False{}:
      fx_step.d(st, c, U32.is_eq(st, 4))

@unsafe
def fx_step.d(+st: U32, +c: U32, hitst: Bool) -> U32:
  match hitst:
    case True{}:
      fx_expect(c, 99, 5)
    case False{}:
      fx_step.e(st, c, U32.is_eq(st, 5))

@unsafe
def fx_step.e(+st: U32, +c: U32, hitst: Bool) -> U32:
  match hitst:
    case True{}:
      fx_expect(c, 116, 6)
    case False{}:
      fx_step.f(st, c, U32.is_eq(st, 6))

@unsafe
def fx_step.f(+st: U32, +c: U32, hitst: Bool) -> U32:
  match hitst:
    case True{}:
      fx_expect(c, 115, 7)
    case False{}:
      fx_step.g(st, c, U32.is_eq(st, 7))

@unsafe
def fx_step.g(+st: U32, +c: U32, hitst: Bool) -> U32:
  match hitst:
    case True{}:
      fx_expect(c, 47, 8)
    case False{}:
      fx_e(c)

@unsafe
def effects(s: String) -> String:
  effects.go(s, 0, "", "")

@unsafe
def effects.go(s: String, +st: U32, acc: String, name: String) -> String:
  effects.p(bite(s), st, acc, name)

@unsafe
def effects.p(p: Bite, +st: U32, acc: String, name: String) -> String:
  Bite{+c, rest, +ok} = p
  effects.e(c, rest, st, acc, name, U32.is_zero(ok))

@unsafe
def effects.e(+c: U32, rest: String, +st: U32, acc: String, name: String, end: Bool) -> String:
  match end:
    case True{}:
      effects.end(acc, name, st)
    case False{}:
      effects.n(rest, c, st, acc, name)

@unsafe
def effects.end(acc: String, name: String, +st: U32) -> String:
  effects.end.m(acc, name, st, U32.is_eq(st, 8))

@unsafe
def effects.end.m(acc: String, name: String, +st: U32, inn: Bool) -> String:
  match inn:
    case True{}:
      commit(acc, name)
    case False{}:
      commit(acc, name)

@unsafe
def effects.n(rest: String, +c: U32, +st: U32, acc: String, name: String) -> String:
  effects.nm(rest, c, st, acc, name, U32.is_eq(st, 8))

@unsafe
def effects.nm(rest: String, +c: U32, +st: U32, acc: String, name: String, inn: Bool) -> String:
  match inn:
    case True{}:
      effects.ch(rest, c, acc, name, end_name(c))
    case False{}:
      effects.go(rest, fx_step(st, c), acc, name)

@unsafe
def effects.ch(rest: String, +c: U32, acc: String, name: String, stop: Bool) -> String:
  match stop:
    case True{}:
      effects.go(rest, 0, commit(acc, name), "")
    case False{}:
      effects.go(rest, 8, acc, push(name, c))

@unsafe
def join(a: String, b: String) -> String:
  join.e(b, empty(a))

@unsafe
def join.e(b: String, p: Bool & String) -> String:
  (e, a) = p
  join.m(a, b, e)

@unsafe
def join.m(a: String, b: String, e: Bool) -> String:
  match e:
    case True{}:
      b
    case False{}:
      join.b(a, empty(b))

@unsafe
def join.b(a: String, p: Bool & String) -> String:
  (e, b) = p
  join.x(a, b, e)

@unsafe
def join.x(a: String, b: String, e: Bool) -> String:
  match e:
    case True{}:
      a
    case False{}:
      a ++ " " ++ b

@unsafe
def showu(xs: List<&2, U32>) -> String:
  match xs:
    case Nil{}:
      ""
    case Con{h, t}:
      U32.show(h) ++ "," ++ showu(t)

@unsafe
def showpat(name: String) -> String:
  showu(keypat(name))

@unsafe
def report(src: String) -> String:
  report.d(pair(src))

@unsafe
def report.d(p: String & String) -> String:
  (a, b) = p
  join(kinds(a), effects(b))

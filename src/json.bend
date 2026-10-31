import Base

# A JSON value. Objects store each field as a child whose `key` is the name.
# Numbers and strings keep their source text. 0 null, 1 false, 2 true,
# 3 number, 4 string, 5 array, 6 object.
type J is Data:
  J{k: U32, key: String, s: String, xs: List<&2, J>}

type Peel is Data:
  Peel{c: U32, rest: String, ok: U32}

def peel(s: String) -> Peel:
  match s:
    case SNil{}:
      Peel{0, SNil{}, 0}
    case SCon{Chr{c}, rest}:
      Peel{c, rest, 1}

def is_ws(+c: U32) -> Bool:
  Bool.or(Bool.or(U32.is_eq(c, 32), U32.is_eq(c, 9)), Bool.or(U32.is_eq(c, 10), U32.is_eq(c, 13)))

def unpeel(c: U32, rest: String, ok: U32) -> String:
  match ok:
    case 0:
      SNil{}
    case _:
      SCon{Chr{c}, rest}

@unsafe
def skip(s: String) -> String:
  skip.p(peel(s))

@unsafe
def skip.p(p: Peel) -> String:
  Peel{+c, rest, +ok} = p
  skip.ws(c, rest, ok, Bool.and(U32.is_eq(ok, 1), is_ws(c)))

@unsafe
def skip.ws(c: U32, rest: String, ok: U32, ws: Bool) -> String:
  match ws:
    case True{}:
      skip(rest)
    case False{}:
      unpeel(c, rest, ok)

def rev_s(xs: String, acc: String) -> String:
  match xs:
    case SNil{}:
      acc
    case SCon{c, t}:
      rev_s(t, SCon{c, acc})

@unsafe
def revj(xs: List<&2, J>, acc: List<&2, J>) -> List<&2, J>:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      revj(t, Con{h, acc})

def null() -> J:
  J{0, SNil{}, SNil{}, Nil{}}

def with_key(j: J, key: String) -> J:
  J{k, old, s, xs} = j
  J{k, key, s, xs}

def key_s(j: J) -> String:
  J{k, key, s, xs} = j
  s

@unsafe
def hexv(+c: U32) -> U32:
  hexv.d(c, Bool.and(U32.is_ge(c, 48), U32.is_le(c, 57)))

@unsafe
def hexv.d(+c: U32, dig: Bool) -> U32:
  match dig:
    case True{}:
      (c - 48 : U32)
    case False{}:
      hexv.a(c, Bool.and(U32.is_ge(c, 97), U32.is_le(c, 102)))

@unsafe
def hexv.a(+c: U32, lo: Bool) -> U32:
  match lo:
    case True{}:
      (c - 87 : U32)
    case False{}:
      hexv.A(c, Bool.and(U32.is_ge(c, 65), U32.is_le(c, 70)))

@unsafe
def hexv.A(c: U32, up: Bool) -> U32:
  match up:
    case True{}:
      (c - 55 : U32)
    case False{}:
      0

def is_num(+c: U32) -> Bool:
  Bool.or(Bool.or(Bool.and(U32.is_ge(c, 48), U32.is_le(c, 57)), U32.is_eq(c, 45)),
    Bool.or(Bool.or(U32.is_eq(c, 43), U32.is_eq(c, 46)), Bool.or(U32.is_eq(c, 101), U32.is_eq(c, 69))))

@unsafe
def pnum(s: String, acc: String) -> String & String:
  pnum.p(peel(s), acc)

@unsafe
def pnum.p(p: Peel, acc: String) -> String & String:
  Peel{+c, rest, +ok} = p
  pnum.c(c, rest, ok, acc, Bool.and(U32.is_eq(ok, 1), is_num(c)))

@unsafe
def pnum.c(c: U32, rest: String, ok: U32, acc: String, more: Bool) -> String & String:
  match more:
    case True{}:
      pnum(rest, SCon{Chr{c}, acc})
    case False{}:
      (rev_s(acc, SNil{}), unpeel(c, rest, ok))

@unsafe
def esc_code(+c: U32) -> U32:
  esc_n(c, U32.is_eq(c, 110))

@unsafe
def esc_n(+c: U32, n: Bool) -> U32:
  match n:
    case True{}:
      10
    case False{}:
      esc_t(c, U32.is_eq(c, 116))

@unsafe
def esc_t(+c: U32, t: Bool) -> U32:
  match t:
    case True{}:
      9
    case False{}:
      esc_r(c, U32.is_eq(c, 114))

def esc_r(c: U32, r: Bool) -> U32:
  match r:
    case True{}:
      13
    case False{}:
      c

@unsafe
def pstr(s: String, acc: String) -> String & String:
  pstr.p(peel(s), acc)

@unsafe
def pstr.p(p: Peel, acc: String) -> String & String:
  Peel{+c, rest, +ok} = p
  pstr.c(c, rest, ok, acc, Bool.and(U32.is_eq(ok, 1), U32.is_eq(c, 34)), U32.is_eq(c, 92))

@unsafe
def pstr.c(c: U32, rest: String, ok: U32, acc: String, end: Bool, esc: Bool) -> String & String:
  match end:
    case True{}:
      (rev_s(acc, SNil{}), rest)
    case False{}:
      pstr.e(c, rest, ok, acc, esc)

@unsafe
def pstr.e(c: U32, rest: String, ok: U32, acc: String, esc: Bool) -> String & String:
  match esc:
    case True{}:
      pstr.esc(peel(rest), acc)
    case False{}:
      pstr.more(c, rest, ok, acc)

@unsafe
def pstr.more(c: U32, rest: String, ok: U32, acc: String) -> String & String:
  match ok:
    case 0:
      (rev_s(acc, SNil{}), SNil{})
    case _:
      pstr(rest, SCon{Chr{c}, acc})

@unsafe
def pstr.esc(p: Peel, acc: String) -> String & String:
  Peel{+c, rest, ok} = p
  pstr.u(c, rest, acc, U32.is_eq(c, 117))

@unsafe
def pstr.u(c: U32, rest: String, acc: String, uni: Bool) -> String & String:
  match uni:
    case True{}:
      pstr.hex(rest, acc, 4, 0)
    case False{}:
      pstr(rest, SCon{Chr{esc_code(c)}, acc})

@unsafe
def pstr.hex(s: String, acc: String, +n: U32, v: U32) -> String & String:
  pstr.hex2(s, acc, n, v, U32.is_zero(n))

@unsafe
def pstr.hex2(s: String, acc: String, n: U32, v: U32, done: Bool) -> String & String:
  match done:
    case True{}:
      pstr(s, SCon{Chr{v}, acc})
    case False{}:
      pstr.hexp(peel(s), acc, n, v)

@unsafe
def pstr.hexp(p: Peel, acc: String, +n: U32, v: U32) -> String & String:
  Peel{+c, rest, ok} = p
  pstr.hex(rest, acc, (n - 1 : U32), ((v << 4n : U32) + hexv(c) : U32))

@unsafe
def pobj(s: String, acc: List<&2, J>) -> J & String:
  pobj.open(peel(skip(s)), acc)

@unsafe
def pobj.open(p: Peel, acc: List<&2, J>) -> J & String:
  Peel{+c, rest, ok} = p
  pobj.close(c, rest, acc, Bool.or(U32.is_eq(ok, 0), U32.is_eq(c, 125)))

@unsafe
def pobj.close(c: U32, rest: String, acc: List<&2, J>, end: Bool) -> J & String:
  match end:
    case True{}:
      (J{6, SNil{}, SNil{}, revj(acc, Nil{})}, rest)
    case False{}:
      pobj.pair(val(SCon{Chr{c}, rest}), acc)

@unsafe
def pobj.pair(r: J & String, acc: List<&2, J>) -> J & String:
  (keyj, rest) = r
  pobj.val(key_s(keyj), val(skip_colon(rest)), acc)

@unsafe
def skip_colon(s: String) -> String:
  skip_colon.p(peel(skip(s)))

def skip_colon.p(p: Peel) -> String:
  Peel{+c, rest, ok} = p
  rest

@unsafe
def pobj.val(key: String, r: J & String, acc: List<&2, J>) -> J & String:
  (vj, rest) = r
  pobj.next(with_key(vj, key), peel(skip(rest)), acc)

@unsafe
def pobj.next(item: J, p: Peel, acc: List<&2, J>) -> J & String:
  Peel{+c, rest, ok} = p
  pobj.sep(item, rest, acc, U32.is_eq(c, 44))

@unsafe
def pobj.sep(item: J, rest: String, acc: List<&2, J>, comma: Bool) -> J & String:
  match comma:
    case True{}:
      pobj(rest, Con{item, acc})
    case False{}:
      (J{6, SNil{}, SNil{}, revj(Con{item, acc}, Nil{})}, rest)

@unsafe
def parr(s: String, acc: List<&2, J>) -> J & String:
  parr.open(peel(skip(s)), acc)

@unsafe
def parr.open(p: Peel, acc: List<&2, J>) -> J & String:
  Peel{+c, rest, ok} = p
  parr.close(c, rest, acc, Bool.or(U32.is_eq(ok, 0), U32.is_eq(c, 93)))

@unsafe
def parr.close(c: U32, rest: String, acc: List<&2, J>, end: Bool) -> J & String:
  match end:
    case True{}:
      (J{5, SNil{}, SNil{}, revj(acc, Nil{})}, rest)
    case False{}:
      parr.val(val(SCon{Chr{c}, rest}), acc)

@unsafe
def parr.val(r: J & String, acc: List<&2, J>) -> J & String:
  (vj, rest) = r
  parr.next(vj, peel(skip(rest)), acc)

@unsafe
def parr.next(item: J, p: Peel, acc: List<&2, J>) -> J & String:
  Peel{+c, rest, ok} = p
  parr.sep(item, rest, acc, U32.is_eq(c, 44))

@unsafe
def parr.sep(item: J, rest: String, acc: List<&2, J>, comma: Bool) -> J & String:
  match comma:
    case True{}:
      parr(rest, Con{item, acc})
    case False{}:
      (J{5, SNil{}, SNil{}, revj(Con{item, acc}, Nil{})}, rest)

def eat(s: String, n: Nat) -> String:
  match s n:
    case s0 0n:
      s0
    case SNil{} 1n+p:
      SNil{}
    case SCon{_, t} 1n+p:
      eat(t, p)

@unsafe
def val(s: String) -> J & String:
  val.p(peel(skip(s)))

@unsafe
def val.p(p: Peel) -> J & String:
  Peel{+c, rest, ok} = p
  val.ok(c, rest, U32.is_eq(ok, 0))

@unsafe
def val.ok(+c: U32, rest: String, empty: Bool) -> J & String:
  match empty:
    case True{}:
      (null(), SNil{})
    case False{}:
      val.q(c, rest, U32.is_eq(c, 34))

@unsafe
def val.q(+c: U32, rest: String, q: Bool) -> J & String:
  match q:
    case True{}:
      val.str(pstr(rest, SNil{}))
    case False{}:
      val.obj(c, rest, U32.is_eq(c, 123))

@unsafe
def val.str(r: String & String) -> J & String:
  (s, rest) = r
  (J{4, SNil{}, s, Nil{}}, rest)

@unsafe
def val.obj(+c: U32, rest: String, obj: Bool) -> J & String:
  match obj:
    case True{}:
      pobj(rest, Nil{})
    case False{}:
      val.arr(c, rest, U32.is_eq(c, 91))

@unsafe
def val.arr(+c: U32, rest: String, arr: Bool) -> J & String:
  match arr:
    case True{}:
      parr(rest, Nil{})
    case False{}:
      val.t(c, rest, U32.is_eq(c, 116))

@unsafe
def val.t(+c: U32, rest: String, t: Bool) -> J & String:
  match t:
    case True{}:
      (J{2, SNil{}, "true", Nil{}}, eat(rest, 3n))
    case False{}:
      val.f(c, rest, U32.is_eq(c, 102))

@unsafe
def val.f(+c: U32, rest: String, f: Bool) -> J & String:
  match f:
    case True{}:
      (J{1, SNil{}, "false", Nil{}}, eat(rest, 4n))
    case False{}:
      val.n(c, rest, U32.is_eq(c, 110))

@unsafe
def val.n(c: U32, rest: String, n: Bool) -> J & String:
  match n:
    case True{}:
      (null(), eat(rest, 3n))
    case False{}:
      val.num(pnum(SCon{Chr{c}, rest}, SNil{}))

@unsafe
def val.num(r: String & String) -> J & String:
  (s, rest) = r
  (J{3, SNil{}, s, Nil{}}, rest)

@unsafe
def parse.at(r: J & String) -> J:
  (j, rest) = r
  j

@unsafe
def bom(s: String) -> String:
  bom.p(peel(s))

@unsafe
def bom.p(p: Peel) -> String:
  Peel{+c, rest, ok} = p
  bom.u(c, rest, ok, U32.is_eq(c, 65279))

def bom.u(c: U32, rest: String, ok: U32, mark: Bool) -> String:
  match mark:
    case True{}:
      rest
    case False{}:
      unpeel(c, rest, ok)

@unsafe
def parse(s: String) -> J:
  parse.at(val(bom(s)))

@unsafe
def get.go(xs: List<&2, J>, +key: String) -> J:
  match xs:
    case Nil{}:
      null()
    case Con{h, t}:
      get.one(h, t, key)

@unsafe
def get.one(+h: J, t: List<&2, J>, +key: String) -> J:
  J{k, hk, s, xs} = h
  get.eq(h, t, key, String.eq(hk, key))

@unsafe
def get.eq(h: J, t: List<&2, J>, key: String, eq: Bool) -> J:
  match eq:
    case True{}:
      h
    case False{}:
      get.go(t, key)

@unsafe
def get(o: J, key: String) -> J:
  J{k, ok, s, xs} = o
  get.go(xs, key)

@unsafe
def lit(j: J) -> String:
  J{k, key, s, xs} = j
  lit.k(k, s)

@unsafe
def lit.k(+k: U32, s: String) -> String:
  lit.str(k, s, U32.is_eq(k, 4))

@unsafe
def lit.str(+k: U32, s: String, hit: Bool) -> String:
  match hit:
    case True{}:
      s
    case False{}:
      lit.num(k, s, Bool.or(U32.is_eq(k, 3), Bool.or(U32.is_eq(k, 2), U32.is_eq(k, 1))))

def lit.num(k: U32, s: String, hit: Bool) -> String:
  match hit:
    case True{}:
      s
    case False{}:
      SNil{}

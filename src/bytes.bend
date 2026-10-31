import Base

# A cursor is the unread tail of a file. `i` is how many bytes were consumed.
# `err` sticks at the first fault. 1 means a read would have passed the end.

type Cur is Data:
  Cur{rest: List<&2, U32>, i: U32, err: U32}

def cur(bs: List<&2, U32>) -> Cur:
  Cur{bs, 0, 0}

def ulen.inc.go(acc: U32, sat: Bool) -> U32:
  match sat:
    case True{}:
      acc
    case False{}:
      (acc + 1 : U32)

def ulen.inc(+acc: U32) -> U32:
  ulen.inc.go(acc, U32.is_eq(acc, 4294967295))

def ulen(xs: List<&2, U32>, acc: U32) -> U32:
  match xs:
    case Nil{}:
      acc
    case Con{_, t}:
      ulen(t, ulen.inc(acc))

# k bytes fit in xs. This is the bounds predicate the laws talk about.
def can(xs: List<&2, U32>, k: Nat) -> Bool:
  match xs k:
    case _ 0n:
      True{}
    case Nil{} 1n+kp:
      False{}
    case Con{_, t} 1n+kp:
      can(t, kp)

def len(xs: List<&2, U32>) -> Nat:
  match xs:
    case Nil{}:
      0n
    case Con{_, t}:
      1n+len(t)

def rev(xs: List<&2, U32>, acc: List<&2, U32>) -> List<&2, U32>:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      rev(t, Con{h, acc})

def sum(xs: List<&2, U32>, acc: U32) -> U32:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      sum(t, (acc + h : U32))

def take.go(rest: List<&2, U32>, n: Nat, acc: List<&2, U32>, i: U32) -> Cur & List<&2, U32>:
  match rest n:
    case rs 0n:
      (Cur{rs, i, 0}, rev(acc, Nil{}))
    case Nil{} 1n+p:
      (Cur{Nil{}, i, 1}, Nil{})
    case Con{h, t} 1n+p:
      take.go(t, p, Con{h, acc}, (i + 1 : U32))

def take.can(+rest: List<&2, U32>, i: U32, n: U32, ok: Bool) -> Cur & List<&2, U32>:
  match ok:
    case False{}:
      (Cur{rest, i, 1}, Nil{})
    case True{}:
      take.go(rest, U32.to_nat(n), Nil{}, i)

def take.gate(+rest: List<&2, U32>, i: U32, err: U32, +n: U32, bad: Bool) -> Cur & List<&2, U32>:
  match bad:
    case True{}:
      (Cur{rest, i, err}, Nil{})
    case False{}:
      take.can(rest, i, n, can(rest, U32.to_nat(n)))

def take(c: Cur, n: U32) -> Cur & List<&2, U32>:
  Cur{+rest, i, +err} = c
  take.gate(rest, i, err, n, U32.is_ne(err, 0))

def le4(xs: List<&2, U32>) -> U32:
  match xs:
    case Con{a, Con{b, Con{c, Con{d, _}}}}:
      (a + (b << 8n) + (c << 16n) + (d << 24n) : U32)
    case _:
      0

def u32.bad(c: Cur, xs: List<&2, U32>, bad: Bool) -> Cur & U32:
  match bad:
    case True{}:
      (c, 0)
    case False{}:
      (c, le4(xs))

def u32.got2(c: Cur, xs: List<&2, U32>) -> Cur & U32:
  Cur{rest, i, +err} = c
  u32.bad(Cur{rest, i, err}, xs, U32.is_ne(err, 0))

def u32.got(r: Cur & List<&2, U32>) -> Cur & U32:
  (c, xs) = r
  u32.got2(c, xs)

def u32(c: Cur) -> Cur & U32:
  u32.got(take(c, 4))

def f32_of(a: U32) -> F32:
  match a:
    case U32{w}:
      F32{w}

def err_of(c: Cur) -> U32:
  Cur{rest, i, err} = c
  err

def fork_err(c: Cur) -> Cur & U32:
  Cur{rest, i, +err} = c
  (Cur{rest, i, err}, err)

def fail.pick2(rest: List<&2, U32>, i: U32, err: U32, e: U32, sticky: Bool) -> Cur:
  match sticky:
    case True{}:
      Cur{rest, i, err}
    case False{}:
      Cur{rest, i, e}

def fail.pick(c: Cur, +err: U32, e: U32) -> Cur:
  Cur{rest, i, err2} = c
  fail.pick2(rest, i, err, e, U32.is_ne(err, 0))

def fail.go(r: Cur & U32, e: U32) -> Cur:
  (c, err) = r
  fail.pick(c, err, e)

def fail(c: Cur, e: U32) -> Cur:
  fail.go(fork_err(c), e)

def idx_of(c: Cur) -> U32:
  Cur{rest, i, err} = c
  i

def take_err(r: Cur & List<&2, U32>) -> U32:
  (c, xs) = r
  err_of(c)

def take_idx(r: Cur & List<&2, U32>) -> U32:
  (c, xs) = r
  idx_of(c)

def head(xs: List<&2, U32>) -> U32:
  match xs:
    case Con{h, _}:
      h
    case Nil{}:
      0

def take_head(r: Cur & List<&2, U32>) -> U32:
  (c, xs) = r
  head(xs)

def byte.bad(c: Cur, xs: List<&2, U32>, bad: Bool) -> Cur & U32:
  match bad:
    case True{}:
      (c, 0)
    case False{}:
      (c, head(xs))

def byte.got2(c: Cur, xs: List<&2, U32>) -> Cur & U32:
  Cur{rest, i, +err} = c
  byte.bad(Cur{rest, i, err}, xs, U32.is_ne(err, 0))

def byte.got(r: Cur & List<&2, U32>) -> Cur & U32:
  (c, xs) = r
  byte.got2(c, xs)

def byte(c: Cur) -> Cur & U32:
  byte.got(take(c, 1))

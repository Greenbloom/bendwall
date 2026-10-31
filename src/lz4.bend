# LZ4 block decoder in pure Bend over Array<U32> (one byte per slot),
# written as a flat state machine: one fuel loop, one micro-step per tick.
import Base

type Z is Type:
  Z{s: Array<U32>, d: Array<U32>, ip: U32, op: U32, cnt: U32, tok: U32, off: U32, n: U32, mode: U32}

def sel(c: Bool, +a: U32, +b: U32) -> U32:
  match c:
    case True{}:
      a
    case False{}:
      b

def fill(xs: List<&2, U32>, a: Array<U32>, +i: U32) -> Array<U32>:
  match xs:
    case Nil{}:
      a
    case Con{x, t}:
      fill(t, Array.set(U32, a, i, x), (i + 1 : U32))

def len(xs: List<&2, U32>, acc: U32) -> U32:
  match xs:
    case Nil{}:
      acc
    case Con{_, t}:
      len(t, (acc + 1 : U32))

# mode 0: read a token
def t0b(d: Array<U32>, +ip: U32, +op: U32, +off: U32, +n: U32, r: Array<U32> & U32) -> Z:
  (s, tok) = r
  +tok = tok
  +c = (tok >> 4n : U32)
  Z{s, d, (ip + 1 : U32), op, c, tok, off, n, sel(U32.is_eq(c, 15), 1, 2)}

def t0(s: Array<U32>, d: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32, done: Bool) -> Z:
  match done:
    case True{}:
      Z{s, d, ip, op, cnt, tok, off, n, 9}
    case False{}:
      t0b(d, ip, op, off, n, Array.get(U32, s, ip))

# mode 1: extended literal length
def t1b(d: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32, r: Array<U32> & U32) -> Z:
  (s, b) = r
  +b = b
  Z{s, d, (ip + 1 : U32), op, (cnt + b : U32), tok, off, n, sel(U32.is_eq(b, 255), 1, 2)}

def t1(s: Array<U32>, d: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32) -> Z:
  t1b(d, ip, op, cnt, tok, off, n, Array.get(U32, s, ip))

# mode 2: copy one literal
def t2b(d: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32, r: Array<U32> & U32) -> Z:
  (s, b) = r
  Z{s, Array.set(U32, d, op, b), (ip + 1 : U32), (op + 1 : U32), (cnt - 1 : U32), tok, off, n, 2}

def t2(s: Array<U32>, d: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32, zero: Bool) -> Z:
  match zero:
    case True{}:
      Z{s, d, ip, op, cnt, tok, off, n, sel(U32.is_ge(ip, n), 9, 3)}
    case False{}:
      t2b(d, ip, op, cnt, tok, off, n, Array.get(U32, s, ip))

# mode 3: match offset and match length nibble
def t3c(d: Array<U32>, +ip: U32, +op: U32, +tok: U32, +n: U32, +lo: U32, r: Array<U32> & U32) -> Z:
  (s, hi) = r
  +off = (lo + (hi << 8n) : U32)
  +c = U32.and(tok, 15)
  Z{s, d, (ip + 2 : U32), op, sel(U32.is_eq(c, 15), 15, (c + 4 : U32)), tok, off, n, sel(U32.is_eq(c, 15), 4, 5)}

def t3b(d: Array<U32>, +ip: U32, +op: U32, +tok: U32, +n: U32, r: Array<U32> & U32) -> Z:
  (s, lo) = r
  t3c(d, ip, op, tok, n, lo, Array.get(U32, s, (ip + 1 : U32)))

def t3(s: Array<U32>, d: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32) -> Z:
  t3b(d, ip, op, tok, n, Array.get(U32, s, ip))

# mode 4: extended match length
def t4b(d: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32, r: Array<U32> & U32) -> Z:
  (s, b) = r
  +b = b
  Z{s, d, (ip + 1 : U32), op, sel(U32.is_eq(b, 255), (cnt + b : U32), (cnt + b + 4 : U32)), tok, off, n, sel(U32.is_eq(b, 255), 4, 5)}

def t4(s: Array<U32>, d: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32) -> Z:
  t4b(d, ip, op, cnt, tok, off, n, Array.get(U32, s, ip))

# mode 5: copy one match byte from op - off
def t5b(s: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32, r: Array<U32> & U32) -> Z:
  (d, b) = r
  Z{s, Array.set(U32, d, op, b), ip, (op + 1 : U32), (cnt - 1 : U32), tok, off, n, 5}

def t5(s: Array<U32>, d: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32, zero: Bool) -> Z:
  match zero:
    case True{}:
      Z{s, d, ip, op, cnt, tok, off, n, 0}
    case False{}:
      t5b(s, ip, op, cnt, tok, off, n, Array.get(U32, d, (op - off : U32)))

def m5(s: Array<U32>, d: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32, +mode: U32, is5: Bool) -> Z:
  match is5:
    case True{}:
      t5(s, d, ip, op, cnt, tok, off, n, U32.is_zero(cnt))
    case False{}:
      Z{s, d, ip, op, cnt, tok, off, n, mode}

def m4(s: Array<U32>, d: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32, +mode: U32, is4: Bool) -> Z:
  match is4:
    case True{}:
      t4(s, d, ip, op, cnt, tok, off, n)
    case False{}:
      m5(s, d, ip, op, cnt, tok, off, n, mode, U32.is_eq(mode, 5))

def m3(s: Array<U32>, d: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32, +mode: U32, is3: Bool) -> Z:
  match is3:
    case True{}:
      t3(s, d, ip, op, cnt, tok, off, n)
    case False{}:
      m4(s, d, ip, op, cnt, tok, off, n, mode, U32.is_eq(mode, 4))

def m2(s: Array<U32>, d: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32, +mode: U32, is2: Bool) -> Z:
  match is2:
    case True{}:
      t2(s, d, ip, op, cnt, tok, off, n, U32.is_zero(cnt))
    case False{}:
      m3(s, d, ip, op, cnt, tok, off, n, mode, U32.is_eq(mode, 3))

def m1(s: Array<U32>, d: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32, +mode: U32, is1: Bool) -> Z:
  match is1:
    case True{}:
      t1(s, d, ip, op, cnt, tok, off, n)
    case False{}:
      m2(s, d, ip, op, cnt, tok, off, n, mode, U32.is_eq(mode, 2))

def m0(s: Array<U32>, d: Array<U32>, +ip: U32, +op: U32, +cnt: U32, +tok: U32, +off: U32, +n: U32, +mode: U32, is0: Bool) -> Z:
  match is0:
    case True{}:
      t0(s, d, ip, op, cnt, tok, off, n, U32.is_ge(ip, n))
    case False{}:
      m1(s, d, ip, op, cnt, tok, off, n, mode, U32.is_eq(mode, 1))

def tick(z: Z) -> Z:
  Z{s, d, ip, op, cnt, tok, off, n, mode} = z
  +mode = mode
  m0(s, d, ip, op, cnt, tok, off, n, mode, U32.is_eq(mode, 0))

def zmode(z: Z) -> Z & U32:
  Z{s, d, ip, op, cnt, tok, off, n, +mode} = z
  (Z{s, d, ip, op, cnt, tok, off, n, mode}, mode)

@unsafe
def loop(f: Nat, z: Z, done: Bool) -> Z:
  match done:
    case True{}:
      z
    case False{}:
      loop.more(f, z)

@unsafe
def loop.more(f: Nat, z: Z) -> Z:
  match f:
    case 0n:
      z
    case 1n+q:
      loop.step(q, tick(z))

@unsafe
def loop.step(f: Nat, z: Z) -> Z:
  loop.step2(f, zmode(z))

@unsafe
def loop.step2(f: Nat, r: Z & U32) -> Z:
  (z2, mode) = r
  loop(f, z2, U32.is_eq(mode, 9))

# 2^d slots. Depth 16 is 65536 bytes, the fixture cap.
@unsafe
def depth.go(n: U32, d: Nat, cap: U32, enough: Bool) -> Nat:
  match enough:
    case True{}:
      d
    case False{}:
      depth.next(n, d, cap)

@unsafe
def depth.next(+n: U32, +d: Nat, +cap: U32) -> Nat:
  +cap2 = (cap + cap : U32)
  depth.go(n, 1n+d, cap2, Bool.or(U32.is_ge(cap2, n), Nat.is_eq(d, 15n)))

def depth_for(+n: U32) -> Nat:
  depth.go(n, 0n, 1, U32.is_ge(1, n))

def rev(xs: List<&2, U32>, acc: List<&2, U32>) -> List<&2, U32>:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      rev(t, Con{h, acc})

@unsafe
def drain(d: Array<U32>, i: U32, n: U32, acc: List<&2, U32>, done: Bool) -> List<&2, U32>:
  match done:
    case True{}:
      rev(acc, Nil{})
    case False{}:
      drain.get(d, i, n, acc)

@unsafe
def drain.get(d: Array<U32>, +i: U32, n: U32, acc: List<&2, U32>) -> List<&2, U32>:
  drain.got(i, n, acc, Array.get(U32, d, i))

@unsafe
def drain.got(i: U32, +n: U32, acc: List<&2, U32>, r: Array<U32> & U32) -> List<&2, U32>:
  (d2, b) = r
  +j = (i + 1 : U32)
  drain(d2, j, n, Con{b, acc}, U32.is_ge(j, n))

type Dec is Data:
  Dec{ok: U32, op: U32, px: List<&2, U32>}

def fuel_of(n: U32, raw: U32) -> Nat:
  U32.to_nat((((n + raw : U32) * 16 : U32) + 64 : U32))

@unsafe
def decode.out(z: Z, +raw: U32) -> Dec:
  Z{s, d, ip, +op, cnt, tok, off, n, +mode} = z
  decode.ok(d, op, raw, mode, Bool.and(U32.is_eq(mode, 9), U32.is_eq(op, raw)))

@unsafe
def decode.ok(d: Array<U32>, +op: U32, raw: U32, mode: U32, good: Bool) -> Dec:
  match good:
    case True{}:
      Dec{1, op, drain(d, 0, op, Nil{}, U32.is_zero(op))}
    case False{}:
      Dec{0, op, Nil{}}

@unsafe
def decode.run(+xs: List<&2, U32>, +raw: U32, +n: U32) -> Dec:
  +src = fill(xs, [0 : U32^depth_for(n)], 0)
  +dst = [0 : U32^depth_for(raw)]
  decode.out(loop(fuel_of(n, raw), Z{src, dst, 0, 0, 0, 0, 0, n, 0}, False{}), raw)

@unsafe
def decode.go(xs: List<&2, U32>, raw: U32, n: U32, bad: Bool) -> Dec:
  match bad:
    case True{}:
      Dec{0, 0, Nil{}}
    case False{}:
      decode.run(xs, raw, n)

@unsafe
def decode(+xs: List<&2, U32>, +raw: U32) -> Dec:
  +n = len(xs, 0)
  decode.go(xs, raw, n, Bool.or(U32.is_gt(n, 65536), U32.is_gt(raw, 65536)))

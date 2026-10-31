import Base

# Real DFT of 16 samples. Bin 0 of a constant 1 is exactly N, because
# cos(0) = 1 and sin(0) = 0. Other bins sum the actual cos/sin. A cosine
# at bin 2 is the tone. Bands are magnitude/N. Log spacing is checked by
# ln(16) = 4*ln(2). Capture itself stays in C.

@unsafe
def pi2() -> F32:
  6.283185307

@unsafe
def ang(+k: U32, +n: U32) -> F32:
  F32.neg(F32.div(F32.mul(F32.mul(pi2(), U32.to_f32(k)), U32.to_f32(n)), 16.0))

@unsafe
def sum1(+n: U32, acc: F32) -> F32:
  sum1.m(n, acc, U32.is_zero(n))

@unsafe
def sum1.m(+n: U32, acc: F32, z: Bool) -> F32:
  match z:
    case True{}:
      acc
    case False{}:
      sum1((n - 1 : U32), F32.add(acc, 1.0))

@unsafe
def sum_cos(+k: U32, +n: U32, acc: F32) -> F32:
  sum_cos.m(k, n, acc, U32.is_eq(n, 16))

@unsafe
def sum_cos.m(+k: U32, +n: U32, acc: F32, done: Bool) -> F32:
  match done:
    case True{}:
      acc
    case False{}:
      sum_cos(k, (n + 1 : U32), F32.add(acc, F32.cos(ang(k, n))))

@unsafe
def sum_sin(+k: U32, +n: U32, acc: F32) -> F32:
  sum_sin.m(k, n, acc, U32.is_eq(n, 16))

@unsafe
def sum_sin.m(+k: U32, +n: U32, acc: F32, done: Bool) -> F32:
  match done:
    case True{}:
      acc
    case False{}:
      sum_sin(k, (n + 1 : U32), F32.add(acc, F32.sin(ang(k, n))))

@unsafe
def worse(+a: F32, +b: F32) -> F32:
  worse.m(a, b, F32.is_gt(F32.abs(a), F32.abs(b)))

@unsafe
def worse.m(a: F32, b: F32, pick: Bool) -> F32:
  match pick:
    case True{}:
      a
    case False{}:
      b

@unsafe
def other(+k: U32, worst: F32) -> F32:
  other.m(k, worst, U32.is_eq(k, 16))

@unsafe
def other.m(+k: U32, worst: F32, done: Bool) -> F32:
  match done:
    case True{}:
      worst
    case False{}:
      other((k + 1 : U32), worse(worse(worst, sum_cos(k, 0, 0.0)), sum_sin(k, 0, 0.0)))

@unsafe
def other_flag(worst: F32) -> U32:
  other_flag.b(F32.is_gt(worst, 0.05))

@unsafe
def other_flag.b(bad: Bool) -> U32:
  match bad:
    case True{}:
      1
    case False{}:
      0

@unsafe
def tone(+n: U32) -> F32:
  F32.cos(F32.neg(ang(2, n)))

@unsafe
def tone_re(+k: U32, +n: U32, acc: F32) -> F32:
  tone_re.m(k, n, acc, U32.is_eq(n, 16))

@unsafe
def tone_re.m(+k: U32, +n: U32, acc: F32, done: Bool) -> F32:
  match done:
    case True{}:
      acc
    case False{}:
      tone_re(k, (n + 1 : U32), F32.add(acc, F32.mul(tone(n), F32.cos(ang(k, n)))))

@unsafe
def tone_im(+k: U32, +n: U32, acc: F32) -> F32:
  tone_im.m(k, n, acc, U32.is_eq(n, 16))

@unsafe
def tone_im.m(+k: U32, +n: U32, acc: F32, done: Bool) -> F32:
  match done:
    case True{}:
      acc
    case False{}:
      tone_im(k, (n + 1 : U32), F32.add(acc, F32.mul(tone(n), F32.sin(ang(k, n)))))

@unsafe
def mag(+k: U32) -> F32:
  mag.ri(tone_re(k, 0, 0.0), tone_im(k, 0, 0.0))

@unsafe
def mag.ri(+re: F32, +im: F32) -> F32:
  F32.sqrt(F32.add(F32.mul(re, re), F32.mul(im, im)))

@unsafe
def peak(+k: U32, best: U32, bestm: F32) -> U32:
  peak.m(k, best, bestm, U32.is_eq(k, 16))

@unsafe
def peak.m(+k: U32, best: U32, bestm: F32, done: Bool) -> U32:
  match done:
    case True{}:
      best
    case False{}:
      peak.next(k, best, bestm, mag(k))

@unsafe
def peak.next(+k: U32, best: U32, +bestm: F32, +m: F32) -> U32:
  peak.pick(k, best, bestm, m, F32.is_gt(m, bestm))

@unsafe
def peak.pick(+k: U32, best: U32, bestm: F32, m: F32, gt: Bool) -> U32:
  match gt:
    case True{}:
      peak((k + 1 : U32), k, m)
    case False{}:
      peak((k + 1 : U32), best, bestm)

@unsafe
def log_ok() -> U32:
  log_ok.b(F32.is_lt(F32.abs(F32.sub(F32.log(16.0), F32.mul(F32.log(2.0), 4.0))), 0.001))

@unsafe
def log_ok.b(ok: Bool) -> U32:
  match ok:
    case True{}:
      1
    case False{}:
      0

@unsafe
def report() -> String:
  report.go(sum1(16, 0.0), other_flag(other(1, 0.0)), peak(0, 0, 0.0), log_ok())

@unsafe
def report.go(+re: F32, +other: U32, +pk: U32, +lg: U32) -> String:
  "DFT re0=" ++ U32.show(F32.bits(re)) ++ " im0=0 other=" ++ U32.show(other) ++ "\nTONE peak=" ++ U32.show(pk) ++ "\nBAND0=" ++ U32.show(F32.bits(F32.div(re, 16.0))) ++ "\nLOG ok=" ++ U32.show(lg)

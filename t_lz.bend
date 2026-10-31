import Base
import ./src/lz4.bend as L

def hex.nyb(n: U32, hi: Bool) -> String:
  match hi:
    case True{}:
      SCon{Chr{(n + 87 : U32)}, SNil{}}
    case False{}:
      SCon{Chr{(n + 48 : U32)}, SNil{}}

def hex2(+b: U32) -> String:
  +hi = (b >> 4n : U32)
  +lo = U32.and(b, 15)
  hex.nyb(hi, U32.is_ge(hi, 10)) ++ hex.nyb(lo, U32.is_ge(lo, 10))

@unsafe
def hexn(xs: List<&2, U32>, n: U32, acc: String, done: Bool) -> String:
  match done:
    case True{}:
      acc
    case False{}:
      hexn.step(xs, n, acc)

@unsafe
def hexn.step(xs: List<&2, U32>, n: U32, acc: String) -> String:
  match xs:
    case Nil{}:
      acc
    case Con{h, t}:
      +j = (n - 1 : U32)
      hexn(t, j, acc ++ hex2(h), U32.is_zero(j))

def show(d: L.Dec) -> String:
  L.Dec{ok, +op, px} = d
  "ok=" ++ U32.show(ok) ++ " n=" ++ U32.show(op) ++ " px=" ++ hexn(px, 32, SNil{}, U32.is_zero(op))

def main() -> IO(Unit):
  do IO<Unit>:
    IO.print("LZ4 match " ++ show(L.decode([96, 97, 98, 99, 100, 101, 102, 6, 0], 10)))
    IO.print("LZ4 lit " ++ show(L.decode([240, 1, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7], 16)))

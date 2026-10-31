import Base

# 256 leaves of 100000 F32 multiplies. `seq` is one core. `par` is a
# balanced fork-join. `par!` is the GPU path; with no CUDA it uses the
# same CPU pool. Parallel addition is not bit-identical to the left fold.

def burn(+n: Nat, +x: F32) -> F32:
  match n:
    case 0n:
      x
    case 1n+p:
      burn(p, (x * 1.000001 : F32))

def seq(+n: Nat, acc: F32) -> F32:
  match n:
    case 0n:
      acc
    case 1n+p:
      seq(p, (acc + burn(100000n, 1.0) : F32))

def par(+n: Nat) -> F32:
  match n:
    case 0n:
      burn(100000n, 1.0)
    case 1n+p:
      a b = par(p) par(p)
      (a + b : F32)

def main() -> IO(Unit):
  do IO<Unit>:
    t0 : Nat <- IO.now()
    s : F32 = seq(256n, 0.0)
    t1 : Nat <- IO.now()
    IO.print("SEQ ms=" ++ Nat.show(Nat.sub(t1, t0)))
    u0 : Nat <- IO.now()
    p : F32 = par(8n)
    u1 : Nat <- IO.now()
    IO.print("PAR ms=" ++ Nat.show(Nat.sub(u1, u0)))
    v0 : Nat <- IO.now()
    g : F32 = par!(8n)
    v1 : Nat <- IO.now()
    IO.print("GPU ms=" ++ Nat.show(Nat.sub(v1, v0)))
    IO.print(U32.show(F32.bits(s)))
    IO.print(U32.show(F32.bits(p)))
    IO.print(U32.show(F32.bits(g)))

import Base

def lower.go(c: U32, up: Bool) -> U32:
  match up:
    case True{}:
      (c + 32 : U32)
    case False{}:
      c

def lower_b(+c: U32) -> U32:
  lower.go(c, Bool.and(U32.is_ge(c, 65), U32.is_le(c, 90)))

def from_bytes(xs: List<&2, U32>) -> String:
  match xs:
    case Nil{}:
      SNil{}
    case Con{h, t}:
      SCon{Chr{h}, from_bytes(t)}

def lower_str(s: String) -> String:
  match s:
    case SNil{}:
      SNil{}
    case SCon{Chr{c}, t}:
      SCon{Chr{lower_b(c)}, lower_str(t)}

def atoi.acc.go(+acc: U32, c: U32, dig: Bool) -> U32:
  match dig:
    case False{}:
      acc
    case True{}:
      ((acc * 10 : U32) + (c - 48 : U32) : U32)

def atoi.acc(+acc: U32, +c: U32) -> U32:
  atoi.acc.go(acc, c, Bool.and(U32.is_ge(c, 48), U32.is_le(c, 57)))

def atoi(s: String, acc: U32) -> U32:
  match s:
    case SNil{}:
      acc
    case SCon{Chr{c}, t}:
      atoi(t, atoi.acc(acc, c))

def drop(n: Nat, s: String) -> String:
  match n s:
    case 0n _:
      s
    case 1n+p SNil{}:
      SNil{}
    case 1n+p SCon{_, t}:
      drop(p, t)

def ver_of.go(s: String, ok: Bool) -> U32:
  match ok:
    case False{}:
      0
    case True{}:
      atoi(drop(4n, s), 0)

def ver_of(s: String, n: U32) -> U32:
  ver_of.go(s, U32.is_ge(n, 5))

def name_drop(xs: List<&2, U32>) -> String:
  match xs:
    case Nil{}:
      SNil{}
    case Con{_, t}:
      name_drop(t)

def name_of(raw: List<&2, U32>, kept: Bool) -> String:
  match kept:
    case True{}:
      lower_str(from_bytes(raw))
    case False{}:
      name_drop(raw)

def cat(a: String, b: String) -> String:
  String.append(a, b)

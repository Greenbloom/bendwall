import Base
import ./src/json.bend as J

@unsafe
def dump(j: J.J) -> String:
  J.J{k, key, s, xs} = j
  U32.show(k) ++ "[" ++ key ++ "=" ++ s ++ "]" ++ dumpxs(xs)

@unsafe
def dumpxs(xs: List<&2, J.J>) -> String:
  match xs:
    case Nil{}:
      SNil{}
    case Con{h, t}:
      " " ++ dump(h) ++ dumpxs(t)

def main() -> IO(Unit):
  do IO<Unit>:
    IO.print(dump(J.parse("{\"title\":\"Proof\",\"n\":0.25,\"ok\":true,\"a\":[1,2],\"p\":{\"value\":1,\"user\":\"speed\"}}")))

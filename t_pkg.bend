import Base
import ./src/pkg.bend as P
import ./src/bytes.bend as B

@unsafe
def enames(es: List<&2, P.Entry>) -> String:
  match es:
    case Nil{}:
      SNil{}
    case Con{e, t}:
      ename(e) ++ " " ++ enames(t)

def ename(e: P.Entry) -> String:
  P.Entry{name, off, sz} = e
  "[" ++ name ++ " " ++ U32.show(off) ++ " " ++ U32.show(sz) ++ "]"

def show(p: P.Pkg) -> String:
  P.Pkg{ver, start, entries, err} = p
  U32.show(err) ++ " ver " ++ U32.show(ver) ++ " start " ++ U32.show(start) ++ " " ++ enames(entries)

def report(+bs: List<&2, U32>) -> String:
  "bytes " ++ U32.show(B.ulen(bs, 0)) ++ " " ++ show(P.parse(bs))

def got(m: File & Result<&1, &1, U32 & String, List<&2, U32>>) -> IO(Unit):
  (f, r) = m
  do IO<Unit>:
    bs : List<&2, U32> <- IO.pass(List<&2, U32>, r)
    IO.print(report(bs))
    File.close(f)

def arg1(xs: List<String>) -> String:
  match xs:
    case Con{_, Con{p, _}}:
      p
    case _:
      "/tmp/two.pkg"

def main() -> IO(Unit):
  do IO<Unit>:
    args : List<String> <- IO.args()
    f : File <- IO.try(File, File.open(arg1(args), "r"))
    m : File & Result<&1, &1, U32 & String, List<&2, U32>> <- File.read_bytes(f, 1000000)
    got(m)

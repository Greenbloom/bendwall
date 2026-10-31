import Base
import ./src/part.bend as Pt

@unsafe
def readf(path: String) -> IO(String):
  do IO<String>:
    f : File <- IO.try(File, File.open(path, "r"))
    m : File & Result<&1, &1, U32 & String, String> <- File.read(f, 1000000)
    s : String <- unwrap(m)
    return s

@unsafe
def unwrap(m: File & Result<&1, &1, U32 & String, String>) -> IO(String):
  (f, r) = m
  do IO<String>:
    s : String <- IO.pass(String, r)
    u : Unit <- File.close(f)
    return s

@unsafe
def main() -> IO(Unit):
  do IO<Unit>:
    src : String <- readf("fixtures/m5/spark.json")
    IO.print(Pt.run(src))

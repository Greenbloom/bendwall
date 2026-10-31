import Base
import ./src/glsl.bend as G

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
    vert : String <- readf("fixtures/m3/shaders/layer.vert")
    frag : String <- readf("fixtures/m3/shaders/layer.frag")
    inc : String <- readf("fixtures/m3/shaders/common.h")
    IO.print("VERT")
    IO.print(G.translate(vert, 0, "", "TINT", "1"))
    IO.print("END")
    fs : String & String = G.dup_s(frag)
    show(fs, inc)

@unsafe
def show(fs: String & String, inc: String) -> IO(Unit):
  (a, b) = fs
  show2(a, b, G.dup_s(inc))

@unsafe
def show2(a: String, b: String, incs: String & String) -> IO(Unit):
  (i1, i2) = incs
  do IO<Unit>:
    IO.print("FRAG0")
    IO.print(G.translate(a, 1, i1, "", ""))
    IO.print("END")
    IO.print("FRAG1")
    IO.print(G.translate(b, 1, i2, "TINT", "1"))
    IO.print("END")

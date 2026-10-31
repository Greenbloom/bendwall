import Base
import ./src/scene.bend as Scn

@unsafe
def readf(path: String) -> IO(String):
  do IO<String>:
    f : File <- IO.try(File, File.open(path, "r"))
    m : File & Result<&1, &1, U32 & String, String> <- File.read(f, 1000000)
    s : String <- unwrap(m)
    return s

def unwrap(m: File & Result<&1, &1, U32 & String, String>) -> IO(String):
  (f, r) = m
  do IO<String>:
    s : String <- IO.pass(String, r)
    u : Unit <- File.close(f)
    return s

def bom() -> String:
  SCon{Chr{65279}, "{\"title\":\"Bom\"}"}

def main() -> IO(Unit):
  do IO<Unit>:
    base : String <- readf("fixtures/m2/base/project.json")
    preset : String <- readf("fixtures/m2/preset/project.json")
    user : String <- readf("fixtures/m2/user.json")
    notype : String <- readf("fixtures/m2/notype/project.json")
    scene : String <- readf("fixtures/m2/base/scene.json")
    mat : String <- readf("fixtures/m2/base/materials/bg.json")
    esc : String <- readf("fixtures/m2/escape.json")
    IO.print(Scn.report(base, preset, user, notype, scene, mat) ++ Scn.bom_report(bom()) ++ Scn.esc_report(esc))

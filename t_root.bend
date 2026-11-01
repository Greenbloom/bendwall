import Base
import ./src/json.bend as J
import ./src/scene.bend as Scn
import ./src/vfs.bend as Vfs

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

@unsafe
def root_line(p: Scn.Proj) -> String:
  Scn.Proj{typ, file, title, dep, audio, props, overs} = p
  root_line.d(typ, file, title, Vfs.drop_s(dep ++ audio ++ Scn.show_props(props) ++ Scn.show_pairs(overs)))

@unsafe
def root_line.d(typ: String, file: String, title: String, +n: U32) -> String:
  "ROOT type=" ++ typ ++ " file=" ++ file ++ " title=" ++ title

@unsafe
def main() -> IO(Unit):
  do IO<Unit>:
    src : String <- readf("fixtures/play/nested/project.json")
    IO.print(root_line(Scn.project(J.parse(src))))

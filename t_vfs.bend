import Base
import ./src/vfs.bend as V

@unsafe
def main() -> IO(Unit):
  do IO<Unit>:
    bs : List<&2, U32> <- V.read_bin("fixtures/m1/pkg_two.pkg")
    IO.print(V.member_text(bs, "b.json"))
    IO.print("\n")
    a : String <- V.scene_text("fixtures/workshop/20", "fixtures/assets-probe", "scene.json")
    IO.print(a)
    IO.print("\n---\n")
    b : String <- V.scene_text("fixtures/workshop/21", "fixtures/assets-probe", "gifscene.json")
    IO.print(b)
    IO.print("\n---\n")
    c : String <- V.scene_text("fixtures/workshop/24", "fixtures/assets-probe", "scenes/from-assets.json")
    IO.print(c)
    IO.print("\n---\n")
    d : String <- V.scene_text("fixtures/workshop/16", "fixtures/assets-probe", "scene.json")
    IO.print(d)
    IO.print("\n---\n")
    m : String <- V.load_text("fixtures/pkgscene", "fixtures/pkgassets", "materials/layer.json", "scene.pkg")
    IO.print(m)

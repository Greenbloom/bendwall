import Base
import ./src/frame.bend as Fr
import ./src/paint.bend as Paint
import ./src/vfs.bend as Vfs

# Same frame as pkgscene, but scene.json is the third entry of a PKGV0002
# package, after two other files. The material and shaders stay in assets.

@unsafe
def main() -> IO(Unit):
  do IO<Unit>:
    base : String <- Vfs.read_text("fixtures/m2/base/project.json")
    preset : String <- Vfs.read_text("fixtures/m2/preset/project.json")
    scene : String <- Vfs.load_text("fixtures/pkgmid", "fixtures/pkgassets", "scene.json", "scene.pkg")
    mat : String <- Vfs.load_text("fixtures/pkgmid", "fixtures/pkgassets", "materials/layer.json", "scene.pkg")
    vert : String <- Vfs.load_text("fixtures/pkgmid", "fixtures/pkgassets", "shaders/layer.vert", "scene.pkg")
    frag : String <- Vfs.load_text("fixtures/pkgmid", "fixtures/pkgassets", "shaders/layer.frag", "scene.pkg")
    inc : String <- Vfs.load_text("fixtures/pkgmid", "fixtures/pkgassets", "shaders/common.h", "scene.pkg")
    bg : List<&2, U32> <- Vfs.load_bin("fixtures/pkgmid", "fixtures/pkgassets", "bg.tex", "scene.pkg")
    sp : List<&2, U32> <- Vfs.load_bin("fixtures/pkgmid", "fixtures/pkgassets", "sprite.tex", "scene.pkg")
    r : Fr.Run = Fr.assemble(base, preset, scene, mat, vert, frag, inc, bg, sp)
    Paint.draw(r, "build/pkgmid.png")

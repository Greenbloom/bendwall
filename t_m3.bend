import Base
import ./src/frame.bend as Fr
import ./src/paint.bend as Paint
import ./src/vfs.bend as Vfs

@unsafe
def main() -> IO(Unit):
  do IO<Unit>:
    base : String <- Vfs.read_text("fixtures/m2/base/project.json")
    preset : String <- Vfs.read_text("fixtures/m2/preset/project.json")
    scene : String <- Vfs.read_text("fixtures/m2/base/scene.json")
    mat : String <- Vfs.read_text("fixtures/m3/materials/layer.json")
    vert : String <- Vfs.read_text("fixtures/m3/shaders/layer.vert")
    frag : String <- Vfs.read_text("fixtures/m3/shaders/layer.frag")
    inc : String <- Vfs.read_text("fixtures/m3/shaders/common.h")
    bg : List<&2, U32> <- Vfs.read_bin("fixtures/m3/bg.tex")
    sp : List<&2, U32> <- Vfs.read_bin("fixtures/m3/sprite.tex")
    r : Fr.Run = Fr.assemble(base, preset, scene, mat, vert, frag, inc, bg, sp)
    Paint.draw(r, "build/m3.png")

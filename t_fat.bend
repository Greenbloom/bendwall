import Base
import ./src/vfs.bend as V

# Packages larger than the 2000000-byte read. scene.json is not first,
# gifscene.json is first, and one scene.json sits after a 2.1MB entry.

@unsafe
def line(label: String, s: String) -> IO(Unit):
  do IO<Unit>:
    IO.print(label ++ s)

@unsafe
def main() -> IO(Unit):
  do IO<Unit>:
    a : String <- V.scene_text("build/pkgfat/v2", "", "scene.json")
    u1 : Unit <- line("FAT ", a)
    b : String <- V.scene_text("build/pkgfat/gif", "", "gifscene.json")
    u2 : Unit <- line("GIF ", b)
    c : String <- V.scene_text("build/pkgfat/late", "", "scene.json")
    u3 : Unit <- line("LATE ", c)
    return u3

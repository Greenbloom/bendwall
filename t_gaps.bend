import Base
import ./src/gaps.bend as G

@unsafe
def main() -> IO(Unit):
  do IO<Unit>:
    IO.print(G.showpat("particle"))
    IO.print("\n")
    IO.print(G.report("{\"objects\":[{\"image\":\"a\"}]}"))
    IO.print("\n")
    IO.print(G.report("{\"particle\":\"p\",\"model\":\"m.mdl\",\"text\":{\"value\":\"hi\"},\"sound\":[],\"light\":\"lpoint\",\"file\":\"effects/tint/effect.json\",\"file2\":\"effects/pulse/effect.json\",\"file3\":\"effects/tint/effect.json\"}"))
    IO.print("\n")

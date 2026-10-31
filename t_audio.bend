import Base
import ./src/audio.bend as Au
import ./src/av.bend as Av

@unsafe
def main() -> IO(Unit):
  do IO<Unit>:
    IO.print(Au.report())
    n : U32 <- Av.av_cap()
    IO.print("AUDIO capture=" ++ U32.show(n))

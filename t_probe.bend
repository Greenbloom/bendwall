import Base
import ./src/probe.bend as Pr

@unsafe
def main() -> IO(Unit):
  do IO<Unit>:
    text : String <- Pr.run()
    IO.print(text)

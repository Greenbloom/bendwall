import Base
import ./src/gl.bend as GL
import ./src/av.bend as AV
import ./src/text.bend as TX

# Frame 0 and frame 1 of a two-frame clip, the first four audio bands as
# g_AudioSpectrum, and a bitmap text texture. Bend owns the calls.
# Codecs, GL, and the glyph bitmap stay in the C host.

@unsafe
def vs() -> String:
  "#version 330 core\nlayout(location=0) in vec3 a_Position;\nlayout(location=1) in vec2 a_TexCoord;\nout vec2 v_TexCoord;\nvoid main(){ v_TexCoord = a_TexCoord; gl_Position = vec4(a_Position, 1.0); }\n"

@unsafe
def fs() -> String:
  "#version 330 core\nin vec2 v_TexCoord;\nuniform float g_AudioSpectrum[4];\nout vec4 fragColor;\nvoid main(){ fragColor = vec4(g_AudioSpectrum[0], v_TexCoord.x * 0.0, 0.0, 1.0); }\n"

@unsafe
def white() -> List<&2, U32>:
  Con{255, Con{255, Con{255, Con{255, Nil{}}}}}

@unsafe
def bits_n() -> U32:
  F32.bits((0.0 - 1.0 : F32))

@unsafe
def bits_p() -> U32:
  F32.bits(1.0)

@unsafe
def quad_full(prog: U32, tex: U32) -> IO(U32):
  GL.gl_quad(prog, tex, bits_n(), bits_p(), bits_p(), bits_n(), 0, 0, bits_p(), bits_p())

@unsafe
def main() -> IO(Unit):
  do IO<Unit>:
    ink : U32 <- TX.text_ink("hi")
    IO.print("TEXT ink=" ++ U32.show(ink))
    i : U32 <- GL.gl_init(8, 8)
    IO.print("GL init=" ++ U32.show(i))
    tt : U32 <- TX.text_tex("hi")
    IO.print("TEXT tex=" ++ U32.show(tt))
    +t0 : U32 <- AV.av_tex("fixtures/m5/rg.mkv", 0)
    c0 : U32 <- GL.gl_px(t0, 0, 0)
    IO.print("VID0 " ++ U32.show(c0))
    +t1 : U32 <- AV.av_tex("fixtures/m5/rg.mkv", 1)
    c1 : U32 <- GL.gl_px(t1, 0, 0)
    IO.print("VID1 " ++ U32.show(c1))
    b : U32 <- GL.gl_bind(0)
    c : U32 <- GL.gl_clear()
    +prog : U32 <- GL.gl_prog(vs(), fs())
    IO.print("BAND prog=" ++ U32.show(prog))
    +dum : U32 <- GL.gl_rgba(1, 1, white())
    fv : U32 <- GL.gl_fv(prog, "g_AudioSpectrum", bits_p(), 0, 0, 0)
    IO.print("BAND fv=" ++ U32.show(fv))
    q : U32 <- quad_full(prog, dum)
    IO.print("BAND quad=" ++ U32.show(q))
    png : U32 <- GL.gl_png("build/bands.png")
    IO.print("BAND png=" ++ U32.show(png))
    log : String <- GL.gl_log()
    IO.print("LOG")
    IO.print(log)

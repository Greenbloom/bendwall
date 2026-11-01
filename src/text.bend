import Base

# Ink count and a GL texture of a text layer. The rasterizer is C because
# Bend has no font library. Layout beyond a single baseline is not done.

def text_ink(s: String) -> IO(U32):
  import "./eff_text.c"
  import "./eff_text.js"

def text_tex(s: String) -> IO(U32):
  import "./eff_text.c"
  import "./eff_text.js"

import Base

# Counts and decode flags only. Samples and pixels stay in the C host:
# Bend 2.0.39 can move a byte list across this boundary, and a full frame is too large to be worth it.

def av_cap() -> IO(U32):
  import "./eff_av.c"
  import "./eff_av.js"

def av_video(path: String) -> IO(U32):
  import "./eff_av.c"
  import "./eff_av.js"

def av_tex(path: String, index: U32) -> IO(U32):
  import "./eff_av.c"
  import "./eff_av.js"

# 1 when the player would draw a decoded picture or a text layer.
def scene_ready(dir: String, assets: String) -> IO(U32):
  import "./eff_av.c"
  import "./eff_av.js"

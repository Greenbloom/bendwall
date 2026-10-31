import Base

# PNG and JPEG blobs from a TEXB freeimage mip. Bend extracts the bytes;
# libpng and libjpeg (host/libbendwall.so) turn them into RGBA.
def Img.rgba(blob: List<&2, U32>) -> IO(List<&2, U32>):
  import "./eff_img.c"
  import "./eff_img.js"

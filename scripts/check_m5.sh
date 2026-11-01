#!/bin/sh
# Particles, DFT, SceneScript, video frames, web API, workshop probe.
set -e
cd "$(dirname "$0")/.."
export PATH="$HOME/.bend/bin:$PATH"
export BEND_NO_TELEMETRY=1
export LIBGL_ALWAYS_SOFTWARE=1
export BENDWALL_NATIVE="$PWD/host/libbendwall.so"
export BENDWALL_ASSETS="$PWD/fixtures/assets-probe"
mkdir -p build
scripts/lib.sh
scripts/check_part.sh
bend t_audio.bend -o build/audio
timeout 8 ./build/audio > build/audio.out
python3 tools/check_audio.py build/audio.out

QJS_LIB=""
for c in /usr/lib/quickjs/libquickjs.a \
         /usr/lib/libquickjs.a \
         /usr/lib64/libquickjs.a \
         /usr/lib/x86_64-linux-gnu/libquickjs.a; do
  if [ -f "$c" ]; then
    QJS_LIB="$c"
    break
  fi
done
if [ -z "$QJS_LIB" ]; then
  if pkg-config --exists quickjs 2>/dev/null; then
    QJS_LIB="$(pkg-config --libs quickjs)"
  else
    QJS_LIB="-lquickjs"
  fi
fi
# Headers: <quickjs/quickjs.h> on Arch and Debian. No -I/usr/include/quickjs,
# which would make that nested include look one directory too deep.
cc -O2 -o build/m5probe host/m5probe.c host/audio.c host/script.c host/video.c host/gl.c \
  $(pkg-config --cflags libavcodec libavformat libswscale libavutil) \
  $QJS_LIB \
  $(pkg-config --libs libavcodec libavformat libswscale libavutil) \
  -lpng -ljpeg -lEGL -lGL \
  -lpulse-simple -lpulse -lm -ldl -lpthread
timeout 4 ./build/m5probe fixtures/m5/red.mkv > build/m5probe.out
python3 tools/check_m5c.py build/m5probe.out
qjs --std tools/check_web.js
python3 tools/check_webqml.py
if [ ! -f fixtures/workshop/15/clip.mkv ]; then
  cp fixtures/m5/red.mkv fixtures/workshop/15/clip.mkv
fi
if [ ! -f fixtures/m5/rg.mkv ]; then
  ffmpeg -y -loglevel error \
    -f lavfi -i color=c=red:s=8x8:r=1:d=1 \
    -f lavfi -i color=c=0x00FF00:s=8x8:r=1:d=1 \
    -filter_complex "[0:v][1:v]concat=n=2:v=1:a=0,format=rgb24" \
    -c:v ffv1 fixtures/m5/rg.mkv
fi
scripts/probe.sh fixtures/workshop
python3 tools/check_probe.py build/probe.out
bend t_live.bend -o build/live
./build/live > build/live.out
python3 tools/check_live.py build/live.out build/bands.png
bend t_pkgscene.bend -o build/pkgscene
./build/pkgscene > build/pkgscene.out
python3 tools/check_m3.py build/pkgscene.out build/pkgscene.png
echo M5 PASS

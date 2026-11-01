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

QJS_CFLAGS=""
QJS_LIB=""
if pkg-config --exists quickjs 2>/dev/null; then
  QJS_CFLAGS="$(pkg-config --cflags quickjs)"
  QJS_LIB="$(pkg-config --libs quickjs)"
else
  for c in /usr/lib/quickjs/libquickjs.a \
           /usr/lib/libquickjs.a \
           /usr/lib64/libquickjs.a \
           /usr/lib/x86_64-linux-gnu/libquickjs.a; do
    if [ -f "$c" ]; then
      QJS_LIB="$c"
      break
    fi
  done
fi
# Headers: <quickjs/quickjs.h> on Arch and Debian. No -I/usr/include/quickjs,
# which would make that nested include look one directory too deep.
# A missing header or library compiles the stub. The probe then prints off.
cat > build/qjsprobe.c << 'EOF'
#if !defined(BW_NO_QUICKJS)
#  if defined(__has_include) && __has_include(<quickjs/quickjs.h>)
#    include <quickjs/quickjs.h>
#    define BW_HAVE_QUICKJS 1
#  elif defined(__has_include) && __has_include(<quickjs.h>)
#    include <quickjs.h>
#    define BW_HAVE_QUICKJS 1
#  endif
#endif
#ifndef BW_HAVE_QUICKJS
#error no quickjs
#endif
int main(void) { return 0; }
EOF
if [ -n "$QJS_LIB" ] && cc -O2 -o build/qjsprobe build/qjsprobe.c $QJS_CFLAGS $QJS_LIB -lm -ldl -lpthread >/tmp/qjsprobe.log 2>&1; then
  echo "quickjs linked"
else
  echo "quickjs not linked; SceneScript is off"
  QJS_CFLAGS="-DBW_NO_QUICKJS"
  QJS_LIB=""
fi
cc -O2 -o build/m5probe host/m5probe.c host/audio.c host/script.c host/video.c host/gl.c \
  $(pkg-config --cflags libavcodec libavformat libswscale libavutil) \
  $QJS_CFLAGS \
  $QJS_LIB \
  $(pkg-config --libs libavcodec libavformat libswscale libavutil) \
  -lpng -ljpeg -lEGL -lGL \
  -lpulse-simple -lpulse -lm -ldl -lpthread
timeout 4 ./build/m5probe fixtures/m5/red.mkv > build/m5probe.out
python3 tools/check_m5c.py build/m5probe.out
if command -v qjs >/dev/null 2>&1; then
  qjs --std tools/check_web.js
else
  echo "qjs not on PATH; skipping tools/check_web.js"
fi
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
python3 tools/mkpkgscene.py
bend t_fat.bend -o build/fat
./build/fat > build/fat.out
python3 tools/check_fat.py build/fat.out
bend t_fx.bend -o build/fx
./build/fx > build/fx.out
python3 tools/check_fx.py build/fx.out build
scripts/probe.sh fixtures/workshop
python3 tools/check_probe.py build/probe.out
bend t_live.bend -o build/live
./build/live > build/live.out
python3 tools/check_live.py build/live.out build/bands.png
bend t_pkgscene.bend -o build/pkgscene
./build/pkgscene > build/pkgscene.out
python3 tools/check_m3.py build/pkgscene.out build/pkgscene.png
bend t_pkgmid.bend -o build/pkgmid
./build/pkgmid > build/pkgmid.out
python3 tools/check_m3.py build/pkgmid.out build/pkgmid.png
scripts/check_play.sh
bend t_root.bend -o build/root
./build/root > build/root.out
python3 tools/check_root.py build/root.out
echo M5 PASS

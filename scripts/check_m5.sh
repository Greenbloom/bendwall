#!/bin/sh
# Particles, DFT, SceneScript, video frame, web API, workshop probe.
set -e
cd "$(dirname "$0")/.."
export PATH="$HOME/.bend/bin:$PATH"
export BEND_NO_TELEMETRY=1
export BENDWALL_NATIVE="$PWD/host/libbendwall.so"
mkdir -p build
scripts/lib.sh
scripts/check_part.sh
bend t_audio.bend -o build/audio
timeout 8 ./build/audio > build/audio.out
python3 tools/check_audio.py build/audio.out
cc -O2 -o build/m5probe host/m5probe.c host/audio.c host/script.c host/video.c \
  -I/usr/include/quickjs \
  $(pkg-config --cflags libavcodec libavformat libswscale libavutil) \
  /usr/lib/quickjs/libquickjs.a \
  $(pkg-config --libs libavcodec libavformat libswscale libavutil) \
  -lpulse-simple -lpulse -lm -ldl -lpthread
timeout 4 ./build/m5probe fixtures/m5/red.mkv > build/m5probe.out
python3 tools/check_m5c.py build/m5probe.out
qjs --std tools/check_web.js
if [ ! -f fixtures/workshop/15/clip.mkv ]; then
  cp fixtures/m5/red.mkv fixtures/workshop/15/clip.mkv
fi
scripts/probe.sh fixtures/workshop
python3 tools/check_probe.py build/probe.out

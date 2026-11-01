#!/bin/sh
# Native glue Bend cannot link itself: PNG/JPEG, EGL, Pulse, ffmpeg.
set -e
cd "$(dirname "$0")/.."
gcc -shared -fPIC -O2 -o host/libbendwall.so \
  host/native.c host/gl.c host/audio.c host/video.c host/text.c host/plan.c host/vaudio.c host/player.c \
  $(pkg-config --cflags libavcodec libavformat libswscale libswresample libavutil) \
  -lpng -ljpeg -lEGL -lGL \
  $(pkg-config --libs libavcodec libavformat libswscale libswresample libavutil) \
  -lpulse-simple -lpulse -lm -ldl -lpthread

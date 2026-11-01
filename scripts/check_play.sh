#!/bin/sh
# Live GL picture: video frames differ, text is in the image, spectrum bands.
set -e
cd "$(dirname "$0")/.."
export LIBGL_ALWAYS_SOFTWARE=1
export BENDWALL_NATIVE="$PWD/host/libbendwall.so"
scripts/lib.sh
cc -O2 -o build/playmain host/playmain.c -Ihost \
  "$PWD/host/libbendwall.so" -Wl,-rpath,"$PWD/host" -lm
mkdir -p build
./build/playmain fixtures/play > build/play.out
python3 tools/check_play.py build/play.out
echo PLAY PASS

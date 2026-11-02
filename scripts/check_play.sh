#!/bin/sh
# Live GL picture: video frames differ, text is in the image, spectrum bands.
set -e
cd "$(dirname "$0")/.."
export LIBGL_ALWAYS_SOFTWARE=1
export BENDWALL_NATIVE="$PWD/host/libbendwall.so"
scripts/lib.sh
cc -O2 -pthread -o build/playmain host/playmain.c -Ihost \
  "$PWD/host/libbendwall.so" -Wl,-rpath,"$PWD/host" -lEGL -lGL -lm
mkdir -p build
python3 tools/mktexscene.py
./build/playmain fixtures/play > build/play.out 2> build/play.err
python3 tools/check_play.py build/play.out build/play.err
echo PLAY PASS

#!/bin/sh
# Milestone 3: headless scene render. Bend plans the frame; EGL draws it.
set -e
cd "$(dirname "$0")/.."
export PATH="$HOME/.bend/bin:$PATH"
export BEND_NO_TELEMETRY=1
export LIBGL_ALWAYS_SOFTWARE=1
export BENDWALL_NATIVE="${BENDWALL_NATIVE:-$PWD/host/libbendwall.so}"
mkdir -p build
python3 tools/mkm3.py
scripts/lib.sh
bend t_m3.bend -o build/m3
./build/m3 > build/m3.out
python3 tools/check_m3.py build/m3.out build/m3.png

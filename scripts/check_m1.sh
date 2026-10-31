#!/bin/sh
# Milestone 1: pkg + tex decoders against synthetic fixtures.
set -e
cd "$(dirname "$0")/.."
export PATH="$HOME/.bend/bin:$PATH"
export BEND_NO_TELEMETRY=1
export BENDWALL_NATIVE="${BENDWALL_NATIVE:-$PWD/host/libbendwall.so}"
mkdir -p build
gcc -shared -fPIC -O2 -o host/libbendwall.so host/native.c -lpng -ljpeg
python3 tools/mkfixtures.py
bend PROOF.bend --check-only | grep -q "ALL PROOFS CHECK"
bend t_m1.bend -o build/m1
./build/m1 fixtures/m1/manifest.txt > build/m1.out
python3 tools/check_m1.py build/m1.out

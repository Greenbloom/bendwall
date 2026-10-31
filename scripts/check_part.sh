#!/bin/sh
set -e
cd "$(dirname "$0")/.."
export PATH="$HOME/.bend/bin:$PATH"
export BEND_NO_TELEMETRY=1
mkdir -p build
bend t_part.bend -o build/part
./build/part > build/part.out
python3 tools/check_part.py build/part.out

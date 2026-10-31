#!/bin/sh
# Milestone 2: JSON, scene/project/material, preset and user properties.
set -e
cd "$(dirname "$0")/.."
export PATH="$HOME/.bend/bin:$PATH"
export BEND_NO_TELEMETRY=1
mkdir -p build
bend t_m2.bend -o build/m2
./build/m2 > build/m2.out
python3 tools/check_m2.py build/m2.out

#!/bin/sh
# One command for a Workshop folder. Prints, per item:
#   <id> loaded
#   <id> partial <feature>
#   <id> unsupported <feature>
set -e
cd "$(dirname "$0")/.."
ROOT="${1:-${BENDWALL_WORKSHOP:-$HOME/.local/share/Steam/steamapps/workshop/content/431960}}"
export PATH="$HOME/.bend/bin:$PATH"
export BEND_NO_TELEMETRY=1
export BENDWALL_NATIVE="${BENDWALL_NATIVE:-$PWD/host/libbendwall.so}"
mkdir -p build
if [ ! -f host/libbendwall.so ]; then
  scripts/lib.sh
fi
python3 tools/probe_manifest.py "$ROOT" build/probe-manifest.json
if [ ! -x build/probe ] || [ src/probe.bend -nt build/probe ] || [ src/scene.bend -nt build/probe ] || [ src/av.bend -nt build/probe ]; then
  bend t_probe.bend -o build/probe
fi
./build/probe | tee build/probe.out

#!/bin/sh
# Plasma package preview: property page, workshop list, painted frame.
set -e
cd "$(dirname "$0")/.."
CXX=c++
if ! echo 'int main(){return 0;}' | c++ -x c++ -o /tmp/bendwall-cxx - >/dev/null 2>&1; then
  if echo 'int main(){return 0;}' | g++ -x c++ -o /tmp/bendwall-cxx - >/dev/null 2>&1; then
    CXX=g++
  else
    CXX=g++-13
  fi
fi
cmake -S kde -B build/kde -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX"
cmake --build build/kde -j2
./build/kde/bendwall-preview --dump-props fixtures/m2/base/project.json > build/props.out
python3 tools/check_m4.py build/props.out
./build/kde/bendwall-preview --dump-workshop fixtures/workshop > build/workshop.out
python3 tools/check_m4.py --workshop build/workshop.out
python3 tools/mkframe.py build/m4-src.png
QT_QPA_PLATFORM=offscreen ./build/kde/bendwall-preview --grab \
  "$PWD/kde/harness.qml" "$PWD/build/m4-src.png" \
  "$PWD/fixtures/m2/base/project.json" "$PWD/fixtures/workshop" \
  "$PWD/build/m4-grab.png" "$PWD/build/kde"
python3 tools/check_grab.py build/m4-grab.png
python3 tools/check_webqml.py
if [ -d /opt/cursor/artifacts ]; then
  cp -f build/m4-grab.png /opt/cursor/artifacts/m4-preview.png
fi
echo M4 PASS

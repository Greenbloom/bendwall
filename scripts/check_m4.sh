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
scripts/lib.sh
export LIBGL_ALWAYS_SOFTWARE=1
export BENDWALL_NATIVE="$PWD/host/libbendwall.so"
./build/kde/bendwall-preview --play-frames "$PWD/fixtures/play/video" 8 > build/preview-play.out
python3 tools/check_preview_play.py build/preview-play.out
python3 tools/mktexscene.py
./build/kde/bendwall-preview --play-frames "$PWD/fixtures/play/gifscene" 4 > build/preview-gif.out
python3 tools/check_preview_play.py build/preview-gif.out
QT_QPA_PLATFORM=offscreen timeout 20 ./build/kde/bendwall-preview --watch-check "$PWD/fixtures/play/web" > build/watch-web.out
python3 tools/check_watch.py build/watch-web.out web
QT_QPA_PLATFORM=offscreen timeout 20 ./build/kde/bendwall-preview --watch-check "$PWD/fixtures/play/texscene" > build/watch-scene.out
python3 tools/check_watch.py build/watch-scene.out scene
QT_QPA_PLATFORM=offscreen timeout 20 ./build/kde/bendwall-preview --watch-check "$PWD/fixtures/play/video" > build/watch-video.out
python3 tools/check_watch.py build/watch-video.out video
python3 - <<'PY'
import pathlib
xml = pathlib.Path("kde/bendwall/contents/config/main.xml").read_text()
qml = pathlib.Path("kde/bendwall/contents/ui/config.qml").read_text()
main = pathlib.Path("kde/bendwall/contents/ui/main.qml").read_text()
i = xml.find("VideoVolume")
if i < 0 or "<default>0</default>" not in xml[i:i + 240]:
    raise SystemExit("video volume default is not 0")
if "property int cfg_VideoVolume: 0" not in qml:
    raise SystemExit("config slider missing")
if "videoVolume" not in main or "return 0" not in main:
    raise SystemExit("main.qml does not default video volume to 0")
print("VOLUME PASS")
PY
QT_QPA_PLATFORM=offscreen timeout 25 ./build/kde/bendwall-preview --watch-diff "$PWD/fixtures/play/spark" > build/watch-diff.out
python3 tools/check_watch_diff.py build/watch-diff.out
if [ -d /opt/cursor/artifacts ]; then
  cp -f build/m4-grab.png /opt/cursor/artifacts/m4-preview.png
fi
echo M4 PASS

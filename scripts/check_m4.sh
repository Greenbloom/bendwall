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
if i < 0 or "<default>100</default>" not in xml[i:i + 240]:
    raise SystemExit("video volume default is not 100")
j = xml.find("VideoMute")
if j < 0 or "<default>true</default>" not in xml[j:j + 240]:
    raise SystemExit("video mute default is not true")
if "property int cfg_VideoVolume: 100" not in qml:
    raise SystemExit("config slider missing")
if "property bool cfg_VideoMute: true" not in qml:
    raise SystemExit("config mute missing")
if "videoVolume" not in main or "return 1" not in main:
    raise SystemExit("main.qml does not keep video volume at 1 once unmuted")
if "videoMuted" not in main or "return !root.videoOptIn()" not in main:
    raise SystemExit("main.qml does not mute video unless opted in")
if "wallpaperAudio" not in main:
    raise SystemExit("main.qml does not keep the watch audio opt-in")
meta = pathlib.Path("kde/bendwall/metadata.json").read_text()
if '"KPackageStructure": "Plasma/Wallpaper"' not in meta:
    raise SystemExit("metadata KPackageStructure")
if '"X-Plasma-API-Minimum-Version": "6.0"' not in meta:
    raise SystemExit("metadata api")
if "com.philip.bendwall" not in meta:
    raise SystemExit("metadata id")
if '<entry name="Item"' not in xml:
    raise SystemExit("main.xml Item")
if "property string cfg_Item" not in qml:
    raise SystemExit("cfg_Item")
if "import org.kde.plasma.plasmoid" not in main:
    raise SystemExit("main.qml does not import org.kde.plasma.plasmoid")
if "wallpaper.configuration" in main:
    raise SystemExit("main.qml still uses wallpaper.configuration")
if "configuration.Item" not in main or "configuration.VideoVolume" not in main or "configuration.VideoMute" not in main:
    raise SystemExit("main.qml does not read configuration")
import re
if not re.search(r"(?m)^WallpaperItem\s*\{", main):
    raise SystemExit("main.qml root is not WallpaperItem")
head = main.split("Frame {", 1)[0]
if "anchors.fill" in head:
    raise SystemExit("WallpaperItem root anchors itself")
stub = pathlib.Path("kde/bendwall/contents/ui/WallpaperItem.qml")
if stub.exists():
    raise SystemExit("WallpaperItem stub is inside the wallpaper package")
print("VOLUME PASS")
PY
./build/kde/bendwall-preview --wallpaper-root > build/wallroot.out
python3 - <<'PY'
import pathlib
text = pathlib.Path("build/wallroot.out").read_text()
if "WALLROOT fail" in text:
    raise SystemExit(text)
if "WALLROOT static" not in text and "WallpaperItem" not in text:
    raise SystemExit(text)
print("WALLROOT PASS")
print(text.strip())
PY
python3 - <<'PY'
import pathlib, sys
text = pathlib.Path("build/watch-video.out").read_text()
if "WATCHMUTE 1" not in text:
    raise SystemExit("watch is not muted by default\n" + text)
print("WATCHMUTE PASS default")
PY
BENDWALL_VIDEO_AUDIO=1 QT_QPA_PLATFORM=offscreen timeout 20 ./build/kde/bendwall-preview --watch-check "$PWD/fixtures/play/video" > build/watch-audio-env.out
python3 - <<'PY'
import pathlib
text = pathlib.Path("build/watch-audio-env.out").read_text()
if "WATCHMUTE 0" not in text:
    raise SystemExit("BENDWALL_VIDEO_AUDIO=1 did not enable watch audio\n" + text)
print("WATCHMUTE PASS env")
PY
QT_QPA_PLATFORM=offscreen timeout 20 ./build/kde/bendwall-preview --watch-check "$PWD/fixtures/play/video" --audio > build/watch-audio-flag.out
python3 - <<'PY'
import pathlib
text = pathlib.Path("build/watch-audio-flag.out").read_text()
if "WATCHMUTE 0" not in text:
    raise SystemExit("--audio did not enable watch audio\n" + text)
print("WATCHMUTE PASS flag")
PY
QT_QPA_PLATFORM=offscreen timeout 25 ./build/kde/bendwall-preview --watch-diff "$PWD/fixtures/play/spark" > build/watch-diff.out
python3 tools/check_watch_diff.py build/watch-diff.out
if [ -d /opt/cursor/artifacts ]; then
  cp -f build/m4-grab.png /opt/cursor/artifacts/m4-preview.png
fi
echo M4 PASS

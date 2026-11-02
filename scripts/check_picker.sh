#!/bin/sh
# Headless picker: synthetic workshop library, mock plasmashell, window grab.
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
cmake --build build/kde -j2 --target bendwall-picker

root="$PWD/build/picker-lib"
rm -rf "$root"
mkdir -p "$root/direct" "$root/steam/steamapps/workshop/content/431960" "$root/cache"

python3 - "$root" << 'PY'
import os, struct, subprocess, sys, zlib
root = sys.argv[1]

def png(path, w, h, rgba):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    raw = b""
    row = bytes(rgba) * w
    for _ in range(h):
        raw += b"\x00" + row
    def chunk(name, data):
        crc = zlib.crc32(name + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + name + data + struct.pack(">I", crc)
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    blob = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    open(path, "wb").write(blob)

def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, "w", encoding="utf-8").write(text)

d = os.path.join(root, "direct")
png(os.path.join(d, "1001", "preview.png"), 32, 18, (220, 64, 72, 255))
write(os.path.join(d, "1001", "scene.json"), '{"objects":[{"name":"label","text":{"value":"hi"}}]}')
write(os.path.join(d, "1001", "project.json"),
      '{"type":"scene","file":"scene.json","title":"Red Glass","tags":["Abstract"],"preview":"preview.png"}\n')

jpg = os.path.join(d, "1002", "preview.jpg")
os.makedirs(os.path.dirname(jpg), exist_ok=True)
subprocess.check_call([
    "ffmpeg", "-y", "-loglevel", "error", "-f", "lavfi", "-i", "color=c=0x3355aa:s=32x18",
    "-frames:v", "1", jpg,
])
mkv = os.path.join(d, "1002", "clip.mkv")
subprocess.check_call([
    "ffmpeg", "-y", "-loglevel", "error", "-f", "lavfi", "-i", "color=c=blue:s=16x16:d=0.2",
    "-c:v", "libx264", "-pix_fmt", "yuv420p", mkv,
])
write(os.path.join(d, "1002", "project.json"),
      '{"type":"video","file":"clip.mkv","title":"Night Clip","tags":["Night"],"preview":"preview.jpg"}\n')

gif = os.path.join(d, "1003", "preview.gif")
os.makedirs(os.path.dirname(gif), exist_ok=True)
subprocess.check_call([
    "ffmpeg", "-y", "-loglevel", "error",
    "-f", "lavfi", "-i", "color=c=black:s=32x18:d=0.2",
    "-f", "lavfi", "-i", "color=c=0x33cc77:s=32x18:d=0.2",
    "-filter_complex", "[0][1]concat=n=2:v=1:a=0",
    "-loop", "0", gif,
])
write(os.path.join(d, "1003", "index.html"), "<!DOCTYPE html><html><body style=\"margin:0;background:#33cc77\"></body></html>\n")
write(os.path.join(d, "1003", "project.json"),
      '{"type":"web","file":"index.html","title":"Green Page","tags":["Web"],"preview":"preview.gif"}\n')

write(os.path.join(d, "1004", "project.json"),
      '{"type":"application","file":"app.exe","title":"External"}\n')

lib = os.path.join(root, "steam", "steamapps", "workshop", "content", "431960", "1005")
png(os.path.join(lib, "preview.png"), 32, 18, (80, 200, 160, 255))
write(os.path.join(lib, "scene.json"), '{"objects":[{"name":"label","text":{"value":"lib"}}]}')
write(os.path.join(lib, "project.json"),
      '{"type":"scene","file":"scene.json","title":"From Library","tags":["Library"]}\n')

steam = os.path.join(root, "steam")
write(os.path.join(root, "libraryfolders.vdf"),
      '"libraryfolders"\n{\n\t"0"\n\t{\n\t\t"path"\t\t"%s"\n\t}\n}\n' % steam)
open(os.path.join(root, "mock.json"), "w", encoding="utf-8").write(
    '{"screens":[{"screen":0,"plugin":"com.philip.bendwall","item":"1001","mute":"true","volume":"100","autoMute":"false"},'
    '{"screen":1,"plugin":"org.kde.image","item":"","mute":"true","volume":"80","autoMute":"false"}]}\n'
)
PY

export BENDWALL_PICKER_CACHE="$root/cache"
export QT_QPA_PLATFORM=offscreen
./build/kde/bendwall-picker --check \
  --workshop "$root/direct" \
  --libraryfolders "$root/libraryfolders.vdf" \
  --mock "$root/mock.json" \
  --screenshot "$root/picker.png" > "$root/check.out"

python3 - "$root" << 'PY'
import json, subprocess, sys
root = sys.argv[1]
text = open(root + "/check.out", encoding="utf-8").read()
need = "CHECK items=5 shown_night=1 shown_web=1 shown_other=1 shown_tag=1 applied=1002 screen=0 mute=0 volume=40 auto=1"
if need not in text:
    raise SystemExit("missing check line:\n" + text)
mock = json.load(open(root + "/mock.json", encoding="utf-8"))
screens = mock["screens"]
if len(screens) != 2:
    raise SystemExit(screens)
s0, s1 = screens
if s0["screen"] != 0 or s0["plugin"] != "com.philip.bendwall" or s0["item"] != "1002":
    raise SystemExit(s0)
if s0["mute"] != "false" or s0["volume"] != "40" or s0["autoMute"] != "true":
    raise SystemExit(s0)
if s1["screen"] != 1 or s1["plugin"] != "org.kde.image" or s1["item"] != "":
    raise SystemExit(s1)
raw = subprocess.check_output([
    "ffmpeg", "-v", "error", "-i", root + "/picker.png",
    "-f", "rawvideo", "-pix_fmt", "rgb24", "-",
])
if len(raw) < 900 * 500 * 3:
    raise SystemExit("screenshot is too small")
colors = set()
step = 3000
for i in range(0, len(raw), step):
    colors.add(raw[i:i+3])
if len(colors) < 8:
    raise SystemExit("screenshot looks blank (%d colors)" % len(colors))
import os
thumbs = []
for dirpath, _, names in os.walk(root + "/cache"):
    for name in names:
        if name.endswith(".png"):
            thumbs.append(os.path.join(dirpath, name))
if not thumbs:
    raise SystemExit("no thumbnail cache")
green = 0
for path in thumbs:
    raw_png = subprocess.check_output([
        "ffmpeg", "-v", "error", "-i", path,
        "-f", "rawvideo", "-pix_fmt", "rgb24", "-",
    ])
    if not raw_png:
        raise SystemExit("empty thumb " + path)
    dark = True
    ghit = False
    step = 3
    for i in range(0, len(raw_png) - 2, step):
        r, g, b = raw_png[i], raw_png[i + 1], raw_png[i + 2]
        if r + g + b > 24:
            dark = False
        if g > 150 and r < 80 and b < 180:
            ghit = True
    if dark:
        raise SystemExit("black thumbnail " + path)
    if ghit:
        green += 1
if green < 1:
    raise SystemExit("gif thumb did not keep the green frame")
print("PICKER PASS")
PY

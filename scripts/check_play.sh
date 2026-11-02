#!/bin/sh
# Live GL picture: video frames differ, text is in the image, spectrum bands.
set -e
cd "$(dirname "$0")/.."
export LIBGL_ALWAYS_SOFTWARE=1
export BENDWALL_NATIVE="$PWD/host/libbendwall.so"
scripts/lib.sh
mkdir -p build
if ! pactl info >/dev/null 2>&1; then
  echo "pactl info failed. Refusing to play on the default sink." >&2
  exit 1
fi
mod=$(pactl load-module module-null-sink sink_name=bendwall_null)
trap 'pactl unload-module "$mod" >/dev/null 2>&1 || true' EXIT
export PULSE_SINK=bendwall_null
cc -O2 -pthread -o build/playmain host/playmain.c -Ihost \
  "$PWD/host/libbendwall.so" -Wl,-rpath,"$PWD/host" -lEGL -lGL -lm
python3 tools/mktexscene.py
./build/playmain fixtures/play > build/play.out 2> build/play.err
python3 tools/check_play.py build/play.out build/play.err
if [ "$PULSE_SINK" != "bendwall_null" ]; then
  echo "PULSE_SINK is not bendwall_null. Refusing the sink-input check." >&2
  exit 1
fi
cc -O2 -pthread -o build/sinkcheck host/sinkcheck.c -Ihost \
  "$PWD/host/libbendwall.so" -Wl,-rpath,"$PWD/host" -lpulse -lpulse-simple -lm -lpthread
./build/sinkcheck "$PWD/fixtures/play/vidaud" > build/sinkcheck.out 2> build/sinkcheck.err &
pid=$!
i=0
while [ "$i" -lt 300 ]; do
  if grep -q "HOLD 1" build/sinkcheck.out 2>/dev/null; then
    break
  fi
  if ! kill -0 "$pid" 2>/dev/null; then
    echo "sinkcheck exited before HOLD" >&2
    wait "$pid" || true
    cat build/sinkcheck.out build/sinkcheck.err >&2 || true
    exit 1
  fi
  sleep 0.1
  i=$((i + 1))
done
if ! grep -q "HOLD 1" build/sinkcheck.out 2>/dev/null; then
  echo "HOLD did not appear" >&2
  kill "$pid" 2>/dev/null || true
  wait "$pid" || true
  cat build/sinkcheck.out build/sinkcheck.err >&2 || true
  exit 1
fi
pactl list sink-inputs > build/sink-inputs.txt
set +e
wait "$pid"
rc=$?
set -e
python3 - <<'PY'
import pathlib, re, sys
out = pathlib.Path("build/sinkcheck.out").read_text()
err = pathlib.Path("build/sinkcheck.err").read_text()
text = pathlib.Path("build/sink-inputs.txt").read_text()
sys.stdout.write(out)
if err.strip():
    sys.stderr.write(err)
moff = re.search(r"PEAKOFF\s+([0-9.]+)", out)
mon = re.search(r"PEAKON\s+([0-9.]+)", out)
if not moff or not mon or "HOLD 1" not in out:
    raise SystemExit("sinkcheck output missing PEAKOFF, PEAKON, or HOLD\n" + out)
off = float(moff.group(1))
on = float(mon.group(1))
if not off > 0.2:
    raise SystemExit("PEAKOFF %.3f is not above 0.2" % off)
if not on < 0.02:
    raise SystemExit("PEAKON %.3f is not under 0.02" % on)
names = re.findall(r'(?m)^\s*application\.name = "(.*)"\s*$', text)
if names.count("Bendwall") != 1:
    raise SystemExit("expected one application.name Bendwall, got %s\n%s" % (names, text))
if "OtherApp" not in names:
    raise SystemExit("OtherApp sink-input missing\n" + text)

def blocks(raw):
    parts = re.split(r"(?m)^Sink Input #", raw)
    return ["Sink Input #" + p for p in parts[1:]]

def prop(block, key):
    prefix = key + " = "
    for line in block.splitlines():
        s = line.strip()
        if s.startswith(prefix):
            return s[len(prefix):].strip().strip('"')
    return None

def field(block, label):
    prefix = label + ":"
    for line in block.splitlines():
        s = line.strip()
        if s.startswith(prefix):
            return s.split(":", 1)[1].strip()
    return None

bend = [b for b in blocks(text) if prop(b, "application.name") == "Bendwall"]
if len(bend) != 1:
    raise SystemExit("expected one Bendwall sink-input block\n" + text)
b = bend[0]
if prop(b, "application.id") != "com.philip.bendwall":
    raise SystemExit("application.id\n" + b)
if prop(b, "application.icon_name") != "preferences-desktop-wallpaper":
    raise SystemExit("application.icon_name\n" + b)
if prop(b, "media.role") != "music":
    raise SystemExit("media.role\n" + b)
mute = field(b, "Mute")
if mute is not None and mute != "no":
    raise SystemExit("Bendwall Mute is %s\n%s" % (mute, b))
corked = field(b, "Corked")
if corked is not None and corked != "no":
    raise SystemExit("Bendwall Corked is %s\n%s" % (corked, b))
print("SINK PASS PEAKOFF %.3f PEAKON %.3f" % (off, on))
for line in b.splitlines():
    s = line.strip()
    if s.startswith("Mute:") or s.startswith("Corked:") or "application." in s or s.startswith("media."):
        print(s)
for line in text.splitlines():
    if 'application.name = "OtherApp"' in line:
        print(line.strip())
PY
if [ "$rc" -ne 0 ]; then
  echo "sinkcheck exited $rc" >&2
  exit "$rc"
fi
echo PLAY PASS

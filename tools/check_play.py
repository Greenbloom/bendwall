#!/usr/bin/env python3
"""Live scene image: two video frames, text ink, tint, pulse, spectrum."""
import pathlib, re, sys

text = pathlib.Path(sys.argv[1]).read_text()

def grab(pat):
    m = re.search(pat, text)
    if not m:
        raise SystemExit("missing " + pat + "\n" + text)
    return m

v0 = [int(x) for x in grab(r"VIDEO0 (\d+) (\d+) (\d+) (\d+)").groups()]
v1 = [int(x) for x in grab(r"VIDEO1 (\d+) (\d+) (\d+) (\d+)").groups()]
if v0[0] < 180 or v0[1] > 80 or v0[2] > 80:
    raise SystemExit(f"frame0 {v0}")
if v1[1] < 180 or v1[0] > 80 or v1[2] > 80:
    raise SystemExit(f"frame1 {v1}")
if "DIFF 1" not in text:
    raise SystemExit(text)
spec = [float(x) for x in grab(r"SPECTRUM ([0-9.]+) ([0-9.]+) ([0-9.]+) ([0-9.]+)").groups()]
if spec[0] < 0.9 or spec[1] < 0.9 or spec[2] < 0.9 or spec[3] > 0.05:
    raise SystemExit(f"spectrum {spec}")
web = [float(x) for x in grab(r"WEB ([0-9.]+) ([0-9.]+)").groups()]
if web[0] < 0.9 or web[1] > 0.05:
    raise SystemExit(f"web {web}")
tone = [float(x) for x in grab(r"TONE ([0-9.]+) ([0-9.]+)").groups()]
if tone[0] < 0.8 or tone[1] < 0.8:
    raise SystemExit(f"tone {tone}")
if "uni=6" not in text:
    raise SystemExit(text)
white = int(grab(r"LABEL white=(\d+)").group(1))
if white < 8:
    raise SystemExit(f"white {white}")
pulse = [int(x) for x in grab(r"PULSE (\d+) (\d+)").groups()]
if pulse[1] < pulse[0] + 40:
    raise SystemExit(f"pulse {pulse}")
tint = [int(x) for x in grab(r"TINT (\d+) (\d+) (\d+)").groups()]
if tint[0] > 40 or not (50 <= tint[1] <= 140) or tint[2] > 40:
    raise SystemExit(f"tint {tint}")
pack = grab(r"PACK (.*)").group(1)
if "tint=1" not in pack or "pulse=1" not in pack:
    raise SystemExit(pack)
if "PLAYMAIN ok" not in text:
    raise SystemExit(text)
print("PLAY PASS")

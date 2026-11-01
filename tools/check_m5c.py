#!/usr/bin/env python3
"""C probe: SceneScript, Pulse capture count, first video frame."""
import pathlib, sys
text = pathlib.Path(sys.argv[1]).read_text()
lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
ok = True
script_off = "SCRIPT fn=off" in lines and "SCRIPT body=off" in lines
script_on = "SCRIPT fn=hi!" in lines and "SCRIPT body=1" in lines
if not script_off and not script_on:
    ok = False
caps = [ln for ln in lines if ln.startswith("AUDIO capture=")]
vids = [ln for ln in lines if ln.startswith("VIDEO ")]
if len(caps) != 1 or int(caps[0].split("=", 1)[1]) < 0:
    ok = False
if len(vids) != 1:
    ok = False
else:
    parts = vids[0].split()
    # VIDEO 8x8 253 0 0 255
    wh = parts[1].split("x")
    px = [int(x) for x in parts[2:6]]
    if wh != ["8", "8"] or px[0] < 200 or px[1] > 30 or px[2] > 30 or px[3] < 250:
        print("video", vids[0])
        ok = False
if not ok:
    print(text)
    sys.exit(1)
print("M5C PASS")

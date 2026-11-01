#!/usr/bin/env python3
"""--watch-diff: the Plasma timer steps, and the picture changes over 1s."""
import pathlib, re, sys

text = pathlib.Path(sys.argv[1]).read_text()
m = re.search(r"WATCHDIFF steps=(\d+) (\d+) diff=(\d+) size=(\d+)x(\d+)", text)
if not m:
    raise SystemExit(text)
s0, s1, diff, w, h = (int(m.group(i)) for i in range(1, 6))
if s0 < 1 or s1 < s0 + 8:
    raise SystemExit(f"clock stalled {m.group(0)}")
if diff != 1:
    raise SystemExit(f"picture did not move\n{m.group(0)}")
if w < 8 or h < 8:
    raise SystemExit(m.group(0))
print(f"WATCH DIFF PASS steps={s0}..{s1} {w}x{h}")

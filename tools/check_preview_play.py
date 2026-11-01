#!/usr/bin/env python3
"""The preview tool prints fps and cpu, and two video frames differ."""
import pathlib, re, sys

text = pathlib.Path(sys.argv[1]).read_text()
if "DIFF 1" not in text:
    raise SystemExit(text)
m = re.search(r"PLAY fps=([0-9]+[.,][0-9]+) cpu=([0-9]+[.,][0-9]+)", text)
if not m:
    raise SystemExit(text)
fps = float(m.group(1).replace(",", "."))
if fps <= 0:
    raise SystemExit(text)
print(f"PREVIEW PLAY PASS fps={m.group(1)} cpu={m.group(2)}")

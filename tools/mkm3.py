#!/usr/bin/env python3
"""Synthetic scene textures for the headless render proof."""
import importlib.util
import os
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("mkfixtures", root / "tools" / "mkfixtures.py")
mk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mk)

out = root / "fixtures" / "m3"
bg = bytes([255, 0, 0, 255, 0, 0, 255, 255])
pix = bytearray()
for y in range(4):
    for x in range(8):
        if x < 4:
            pix += bytes([0, 255, 0, 255])
        else:
            pix += bytes([255, 255, 0, 255])
frames = [
    {"img": 0, "t": 0.5, "x": 0, "y": 0, "w": 4, "wy": 0, "hx": 0, "h": 4},
    {"img": 0, "t": 0.5, "x": 4, "y": 0, "w": 4, "wy": 0, "hx": 0, "h": 4},
]
(out / "bg.tex").write_bytes(mk.tex(fmt=0, flags=0, tw=2, th=1, iw=2, ih=1, images=[[mk.mip(2, 1, bg)]]))
(out / "sprite.tex").write_bytes(mk.tex(
    fmt=0, flags=7, tw=8, th=4, iw=8, ih=4,
    images=[[mk.mip(8, 4, bytes(pix))]],
    sprites={"sv": 3, "sw": 8, "sh": 4, "frames": frames},
))
print("m3 tex", (out / "bg.tex").stat().st_size, (out / "sprite.tex").stat().st_size)

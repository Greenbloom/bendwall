#!/usr/bin/env python3
"""Scroll, spin, opacity, and colorkey on a red|green texture."""
import pathlib, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import check_m3

def pix(path):
    w, h, rows = check_m3.png_rows(path)
    if (w, h) != (8, 8):
        raise SystemExit(f"{path} {w}x{h}")
    return tuple(rows[0][0:4])

text = pathlib.Path(sys.argv[1]).read_text()
root = pathlib.Path(sys.argv[2])
for name in ("scroll", "spin", "opacity", "colorkey", "tint", "pulse"):
    if f"FX {name}=1" not in text:
        raise SystemExit(text)
if "FX init=1" not in text:
    raise SystemExit(text)
sc, sp = pix(root / "fx-scroll.png"), pix(root / "fx-spin.png")
for name, p in (("scroll", sc), ("spin", sp)):
    if p[1] < 200 or p[0] > 40 or p[2] > 40 or p[3] < 250:
        raise SystemExit(f"{name} {p}")
op = pix(root / "fx-opacity.png")
if op[0] < 200 or op[1] > 40 or op[2] > 40 or not (96 <= op[3] <= 160):
    raise SystemExit(f"opacity {op}")
ck = pix(root / "fx-colorkey.png")
if ck[3] > 10:
    raise SystemExit(f"colorkey {ck}")
w, h, rows = check_m3.png_rows(root / "fx-tint.png")
tint = tuple(rows[0][7 * 4:7 * 4 + 4])
if tint[0] > 40 or not (50 <= tint[1] <= 140) or tint[2] > 40 or tint[3] < 200:
    raise SystemExit(f"tint {tint}")
pulse = pix(root / "fx-pulse.png")
if not (100 <= pulse[0] <= 170) or pulse[1] > 40 or pulse[3] < 200:
    raise SystemExit(f"pulse {pulse}")
print("FX PASS")

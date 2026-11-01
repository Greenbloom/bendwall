#!/usr/bin/env python3
"""Two video frames, g_AudioSpectrum[0] == 1, and a text texture."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_m3


def chans(n):
    n = int(n)
    return n & 255, (n >> 8) & 255, (n >> 16) & 255, (n >> 24) & 255


def main():
    text = Path(sys.argv[1]).read_text()
    png = sys.argv[2]
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    ink = tex = None
    vid = {}
    fv = quad = wrote = prog = None
    for ln in lines:
        if ln.startswith("TEXT ink="):
            ink = int(ln.split("=", 1)[1])
        elif ln.startswith("TEXT tex="):
            tex = int(ln.split("=", 1)[1])
        elif ln.startswith("VID0 "):
            vid[0] = chans(ln.split()[1])
        elif ln.startswith("VID1 "):
            vid[1] = chans(ln.split()[1])
        elif ln.startswith("BAND prog="):
            prog = int(ln.split("=", 1)[1])
        elif ln.startswith("BAND fv="):
            fv = int(ln.split("=", 1)[1])
        elif ln.startswith("BAND quad="):
            quad = int(ln.split("=", 1)[1])
        elif ln.startswith("BAND png="):
            wrote = int(ln.split("=", 1)[1])
    if not ink or ink < 1:
        raise SystemExit("text ink " + text)
    if not tex or tex < 1:
        raise SystemExit("text tex " + text)
    if 0 not in vid or 1 not in vid:
        raise SystemExit("video lines\n" + text)
    r, g, b, a = vid[0]
    if r < 200 or g > 40 or b > 40 or a < 250:
        raise SystemExit(f"frame0 {vid[0]}")
    r, g, b, a = vid[1]
    if g < 200 or r > 40 or b > 40 or a < 250:
        raise SystemExit(f"frame1 {vid[1]}")
    if not prog or not fv or not quad or not wrote:
        raise SystemExit("bands\n" + text)
    w, h, rows = check_m3.png_rows(png)
    if (w, h) != (8, 8):
        raise SystemExit(f"bands size {w}x{h}")
    px = tuple(rows[0][0:4])
    if px[0] < 200 or px[1] > 40 or px[2] > 40 or px[3] < 250:
        raise SystemExit(f"spectrum pixel {px}")
    print("LIVE PASS")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""DFT of 16 ones, cosine peak, log identity, capture count."""
import pathlib, sys
lines = [ln.strip() for ln in pathlib.Path(sys.argv[1]).read_text().splitlines() if ln.strip()]
need = {
    "DFT re0=1098907648 im0=0 other=0",
    "BAND0=1065353216",
    "LOG ok=1",
}
have = set(lines)
missing = [n for n in need if n not in have]
tone = [ln for ln in lines if ln.startswith("TONE peak=")]
cap = [ln for ln in lines if ln.startswith("AUDIO capture=")]
bad = False
if missing:
    print("missing", missing)
    bad = True
if len(tone) != 1 or tone[0] not in ("TONE peak=2", "TONE peak=14"):
    print("tone", tone)
    bad = True
if len(cap) != 1:
    print("capture", cap)
    bad = True
else:
    n = int(cap[0].split("=", 1)[1])
    if n < 0:
        bad = True
if bad:
    print("\n".join(lines))
    sys.exit(1)
print("AUDIO PASS", tone[0], cap[0])

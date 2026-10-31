#!/usr/bin/env python3
"""Synthetic workshop: one status line per item."""
import pathlib, sys
got = [ln.strip() for ln in pathlib.Path(sys.argv[1]).read_text().splitlines() if ln.strip()]
want = [
    "10 loaded",
    "11 partial particle",
    "12 unsupported application",
    "13 partial web",
    "14 unsupported dependency",
    "15 partial playback",
    "16 unsupported scene",
    "18 unsupported video",
    "19 partial model",
]
if got != want:
    print("GOT")
    print("\n".join(got))
    print("WANT")
    print("\n".join(want))
    sys.exit(1)
print("PROBE PASS")

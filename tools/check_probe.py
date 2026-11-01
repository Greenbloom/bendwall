#!/usr/bin/env python3
"""Synthetic workshop: one status line per item."""
import pathlib, sys
got = [ln.strip() for ln in pathlib.Path(sys.argv[1]).read_text().splitlines() if ln.strip()]
want = [
    "10 partial texture",
    "11 partial particle",
    "12 unsupported application",
    "13 loaded",
    "14 unsupported dependency",
    "15 loaded",
    "16 unsupported scene",
    "18 unsupported video",
    "19 partial model",
    "20 partial texture",
    "21 partial texture",
    "22 loaded",
    "23 partial particle",
    "24 partial texture",
    "26 loaded",
    "30 partial texture",
    "31 partial texture",
    "32 partial texture",
    "33 partial texture",
]
if got != want:
    print("GOT")
    print("\n".join(got))
    print("WANT")
    print("\n".join(want))
    sys.exit(1)
print("PROBE PASS")

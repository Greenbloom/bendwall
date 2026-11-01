#!/usr/bin/env python3
"""Scene JSON from packages bigger than the 2MB read."""
import pathlib, sys
text = pathlib.Path(sys.argv[1]).read_text()
want = [
    'FAT {"mark":"city-scene"}',
    'GIF {"mark":"gif-scene"}',
    'LATE {"mark":"after-tex"}',
]
got = [ln.strip() for ln in text.splitlines() if ln.strip()]
if got != want:
    print("\n".join(got))
    sys.exit(1)
print("FAT PASS")

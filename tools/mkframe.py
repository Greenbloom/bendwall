#!/usr/bin/env python3
"""64x64 PNG: yellow left, cyan right. No Pillow."""
import struct, sys, zlib
w, h = 64, 64
rows = []
for y in range(h):
    row = bytearray([0])
    for x in range(w):
        if x < w // 2:
            row += bytes((255, 255, 0, 255))
        else:
            row += bytes((0, 255, 255, 255))
    rows.append(bytes(row))
raw = b"".join(rows)

def chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

png = b"\x89PNG\r\n\x1a\n"
png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(raw))
png += chunk(b"IEND", b"")
open(sys.argv[1], "wb").write(png)
print("png", sys.argv[1], len(png))

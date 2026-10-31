#!/usr/bin/env python3
"""The grabbed window must contain both the yellow and the cyan of the frame."""
import struct, sys, zlib
data = open(sys.argv[1], "rb").read()
assert data.startswith(b"\x89PNG")
pos = 8
w = h = depth = color = None
raw = b""
while pos < len(data):
    n = struct.unpack(">I", data[pos:pos + 4])[0]
    tag = data[pos + 4:pos + 8]
    chunk = data[pos + 8:pos + 8 + n]
    if tag == b"IHDR":
        w, h, depth, color = struct.unpack(">IIBB", chunk[:10])
    elif tag == b"IDAT":
        raw += chunk
    elif tag == b"IEND":
        break
    pos += 12 + n
def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c

inflated = zlib.decompress(raw)
channels = 4 if color == 6 else 3
stride = w * channels + 1
rows = []
prev = bytearray(w * channels)
for y in range(h):
    row = inflated[y * stride:(y + 1) * stride]
    filt, cur = row[0], bytearray(row[1:])
    if filt == 1:
        for i in range(len(cur)):
            left = cur[i - channels] if i >= channels else 0
            cur[i] = (cur[i] + left) & 255
    elif filt == 2:
        for i in range(len(cur)):
            cur[i] = (cur[i] + prev[i]) & 255
    elif filt == 3:
        for i in range(len(cur)):
            left = cur[i - channels] if i >= channels else 0
            cur[i] = (cur[i] + ((left + prev[i]) // 2)) & 255
    elif filt == 4:
        for i in range(len(cur)):
            left = cur[i - channels] if i >= channels else 0
            up = prev[i]
            ul = prev[i - channels] if i >= channels else 0
            cur[i] = (cur[i] + paeth(left, up, ul)) & 255
    prev = cur
    rows.append(cur)
yellow = cyan = 0
for row in rows:
    for x in range(w):
        r, g, b = row[x * channels:x * channels + 3]
        if r > 200 and g > 200 and b < 40:
            yellow += 1
        if r < 40 and g > 200 and b > 200:
            cyan += 1
print(f"GRAB yellow={yellow} cyan={cyan} {w}x{h}")
if yellow < 20 or cyan < 20:
    sys.exit(1)
print("M4 GRAB PASS")

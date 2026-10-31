#!/usr/bin/env python3
"""Synthetic Wallpaper Engine pkg/tex fixtures. No Workshop bytes.

Layouts follow REA-FINDINGS §3 and §4. Expected pixels are the bytes we put in,
not the output of the Bend parser.
"""
import os, struct, subprocess, zlib

ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "fixtures", "m1")

def u32(n):
    return struct.pack("<I", n & 0xFFFFFFFF)

def i32(n):
    return struct.pack("<i", n)

def f32(n):
    return struct.pack("<f", n)

def tag(s):
    assert len(s) == 8
    return s.encode("ascii") + b"\x00"

def lz4_literals(data: bytes) -> bytes:
    out = bytearray()
    n = len(data)
    if n < 15:
        out.append(n << 4)
        out += data
        return bytes(out)
    out.append(0xF0)
    left = n - 15
    while left >= 255:
        out.append(255)
        left -= 255
    out.append(left)
    out += data
    return bytes(out)

def png_rgba(w, h, rgba: bytes) -> bytes:
    assert len(rgba) == w * h * 4
    raw = b""
    for y in range(h):
        raw += b"\x00" + rgba[y * w * 4:(y + 1) * w * 4]
    def chunk(name, data):
        crc = zlib.crc32(name + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + name + data + struct.pack(">I", crc)
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")

def jpeg_rgba(w, h, rgba: bytes) -> bytes:
    rgb = bytearray()
    for i in range(0, len(rgba), 4):
        rgb += rgba[i:i + 3]
    path = "/tmp/bendwall_fix.jpg"
    subprocess.run(
        ["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24",
         "-s", f"{w}x{h}", "-i", "pipe:0", "-frames:v", "1", "-q:v", "1", path],
        input=bytes(rgb),
        check=True,
    )
    return open(path, "rb").read()

def tex(ver="TEXV0005", texi="TEXI0001", fmt=0, flags=0, tw=2, th=2, iw=2, ih=2,
        depth=None, unk=0x11223344, bv=2, fif=None, cond=None, images=None, sprites=None,
        stop_after_header=False):
    images = images or []
    b = tag(ver) + tag(texi)
    b += u32(fmt) + u32(flags) + u32(tw) + u32(th) + u32(iw) + u32(ih)
    if depth is not None:
        b += u32(depth)
    if int(texi[4:]) > 0:
        b += u32(unk)
    if stop_after_header:
        return b
    b += tag(f"TEXB{bv:04d}")
    b += u32(len(images))
    if bv >= 3:
        b += i32(-1 if fif is None else fif)
    if bv >= 4:
        b += u32(0 if cond is None else cond)
    if cond:
        return b
    for im in images:
        b += u32(len(im))
        for m in im:
            b += u32(m["w"]) + u32(m["h"])
            if depth is not None:
                b += u32(m.get("d", depth))
            data = m["data"]
            if bv >= 2:
                b += u32(m.get("lz4", 0)) + u32(m.get("raw", len(data)))
            b += u32(len(data)) + data
    if sprites:
        sv = sprites["sv"]
        frames = sprites["frames"]
        b += tag(f"TEXS{sv:04d}")
        b += u32(len(frames))
        if sv >= 3:
            b += u32(sprites["sw"]) + u32(sprites["sh"])
        for fr in frames:
            b += u32(fr["img"]) + f32(fr["t"])
            for k in ("x", "y", "w", "wy", "hx", "h"):
                b += u32(fr[k]) if sv == 1 else f32(float(fr[k]))
    return b

def mip(w, h, data, lz4=0, raw=None, d=None):
    m = {"w": w, "h": h, "data": data, "lz4": lz4, "raw": len(data) if raw is None else raw}
    if d is not None:
        m["d"] = d
    return m

def pkg(entries, ver=1):
    magic = f"PKGV{ver:04d}".encode("ascii")
    assert len(magic) == 8
    parts = [u32(8), magic, u32(len(entries))]
    blob = bytearray()
    off = 0
    for name, data in entries:
        nb = name.encode("ascii")
        parts.append(u32(len(nb)))
        parts.append(nb)
        parts.append(u32(off))
        parts.append(u32(len(data)))
        blob += data
        off += len(data)
    return b"".join(parts) + bytes(blob)

RGBA = bytes([
    255, 0, 0, 255, 0, 255, 0, 255,
    0, 0, 255, 255, 255, 255, 0, 255,
])
RG = bytes([10, 20, 30, 40, 50, 60, 70, 80])
R8 = bytes([1, 2, 3, 4])
DXT1 = bytes([0x00, 0xF8, 0x00, 0xF8, 0x00, 0x00, 0x00, 0x00])
DXT5 = bytes([
    255, 0, 0, 0, 0, 0, 0, 0,
    0x00, 0xF8, 0x00, 0xF8, 0x00, 0x00, 0x00, 0x00,
])

def main():
    os.makedirs(ROOT, exist_ok=True)
    files = []

    def put(name, data):
        path = os.path.join(ROOT, name)
        open(path, "wb").write(data)
        files.append(name)
        return path

    put("pkg_one.pkg", pkg([("Hello.TXT", b"hi")], 1))
    put("pkg_two.pkg", pkg([("a.txt", b"one"), ("B.JSON", b'{"x":1}')], 9))
    put("pkg_badver.pkg", pkg([], 25))
    # name longer than 0x800 is kept in the file but the engine stores it empty
    long = bytearray(u32(8) + b"PKGV0002" + u32(1))
    long += u32(2049) + (b"A" * 2049) + u32(0) + u32(1) + b"Z"
    put("pkg_longname.pkg", bytes(long))
    put("pkg_trunc.pkg", u32(8) + b"PKGV0001" + u32(1) + b"\x01\x02")
    put("pkg_huge.pkg", u32(8) + b"PKGV0001" + u32(400000000))
    put("pkg_magic.pkg", u32(9) + b"PKGV0001!")

    put("tex_rgba.tex", tex(fmt=0, images=[[mip(2, 2, RGBA)]]))
    comp = lz4_literals(RGBA)
    put("tex_lz4.tex", tex(fmt=0, images=[[mip(2, 2, comp, lz4=1, raw=len(RGBA))]]))
    put("tex_rg88.tex", tex(fmt=8, images=[[mip(2, 2, RG)]]))
    put("tex_r8.tex", tex(fmt=9, images=[[mip(2, 2, R8)]]))
    put("tex_dxt1.tex", tex(fmt=7, tw=4, th=4, iw=4, ih=4, images=[[mip(4, 4, DXT1)]]))
    put("tex_dxt5.tex", tex(fmt=4, tw=4, th=4, iw=4, ih=4, images=[[mip(4, 4, DXT5)]]))
    png = png_rgba(2, 2, RGBA)
    open(os.path.join(ROOT, "tex_png.rgba"), "wb").write(struct.pack("<II", 2, 2) + RGBA)
    put("tex_png.tex", tex(fmt=0, bv=3, fif=13, images=[[mip(2, 2, png, raw=len(png))]]))
    jpg_src = bytes([255, 0, 0, 255]) * 16
    jpg = jpeg_rgba(4, 4, jpg_src)
    open(os.path.join(ROOT, "tex_jpg.rgba"), "wb").write(struct.pack("<II", 4, 4) + jpg_src)
    put("tex_jpg.tex", tex(fmt=0, tw=4, th=4, iw=4, ih=4, bv=3, fif=2,
                           images=[[mip(4, 4, jpg, raw=len(jpg))]]))
    frames3 = [
        {"img": 0, "t": 0.5, "x": 0, "y": 0, "w": 4, "wy": 0, "hx": 0, "h": 4},
        {"img": 0, "t": 0.25, "x": 4, "y": 0, "w": 4, "wy": 0, "hx": 0, "h": 4},
    ]
    put("tex_sprite.tex", tex(fmt=0, flags=7, tw=8, th=4, iw=8, ih=4,
                              images=[[mip(8, 4, bytes(range(8 * 4 * 4)))]],
                              sprites={"sv": 3, "sw": 8, "sh": 4, "frames": frames3}))
    frames1 = [
        {"img": 1, "t": 0.5, "x": 0, "y": 1, "w": 4, "wy": 0, "hx": 0, "h": 4},
    ]
    put("tex_sprite1.tex", tex(fmt=0, flags=4, images=[[mip(2, 2, RGBA)]],
                               sprites={"sv": 1, "frames": frames1}))
    put("tex_vol.tex", tex(fmt=9, flags=0x40, depth=2, images=[[mip(2, 2, R8 + R8, d=2)]]))
    put("tex_badres.tex", tex(tw=8193, th=1, iw=1, ih=1, stop_after_header=True))
    put("tex_depth.tex", tex(flags=0x40, depth=129, images=[[mip(2, 2, RGBA, d=129)]]))
    put("tex_cond.tex", tex(bv=4, cond=1, images=[]))
    put("tex_v4.tex", tex(ver="TEXV0004", images=[[mip(2, 2, RGBA)]]))
    put("tex_texb1.tex", tex(bv=1, images=[[mip(2, 2, RGBA)]]))
    put("tex_nounk.tex", tex(texi="TEXI0000", images=[[mip(2, 2, RGBA)]]))
    put("tex_lz4bad.tex", tex(fmt=0, images=[[mip(2, 2, b"\xff\xff\xff\xff", lz4=1, raw=16)]]))
    put("tex_nosprite.tex", tex(flags=4, images=[[mip(2, 2, RGBA)]]))
    put("tex_badver.tex", tex(ver="TEXV0006", stop_after_header=True))
    put("tex_two.tex", tex(fmt=0, images=[
        [mip(2, 2, RGBA), mip(1, 1, bytes([1, 2, 3, 4]))],
        [mip(2, 2, bytes([9, 9, 9, 9, 8, 8, 8, 8, 7, 7, 7, 7, 6, 6, 6, 6]))],
    ], sprites={"sv": 3, "sw": 2, "sh": 2, "frames": [
        {"img": 1, "t": 1.0, "x": 0, "y": 0, "w": 2, "wy": 0, "hx": 0, "h": 2},
    ]}))

    man = os.path.join(ROOT, "manifest.txt")
    lines = ["L"]
    for name in files:
        kind = "P" if name.endswith(".pkg") else "T"
        if name in ("tex_png.tex", "tex_jpg.tex"):
            kind = "I"
        lines.append(f"{kind} fixtures/m1/{name}")
    open(man, "w").write("\n".join(lines) + "\n")
    print(f"wrote {len(files)} fixtures")

if __name__ == "__main__":
    main()

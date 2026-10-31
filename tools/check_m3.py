#!/usr/bin/env python3
"""Independent check of the milestone-3 frame.

The shader oracle follows the same dialect rules as src/glsl.bend:
combo #define, one-level #include splice, whole-identifier rewrite.
Pixel values are the locked 32x32 proof, not a copy of the Bend output.
"""
import json
import re
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MACROS = (
    "#define saturate(x) clamp((x), 0.0, 1.0)\n"
    "#define mul(a, b) ((a) * (b))\n"
    "#define texSample2D(s, uv) texture((s), (uv))\n"
    "#define texSample2DLod(s, uv, l) textureLod((s), (uv), (l))\n"
    "#define texLoad2D(s, p) texelFetch((s), (p), 0)\n"
    "#define CAST2(x) vec2(x)\n"
    "#define CAST3(x) vec3(x)\n"
    "#define CAST4(x) vec4(x)\n"
)
IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
WORDS = {
    "float4x4": "mat4",
    "float3x3": "mat3",
    "float2x2": "mat2",
    "float4": "vec4",
    "float3": "vec3",
    "float2": "vec2",
    "attribute": "in",
    "lerp": "mix",
    "frac": "fract",
    "ddx": "dFdx",
    "ddy": "dFdy",
}


def rewrite(src, stage):
    def one(m):
        w = m.group(0)
        if w == "varying":
            return "out" if stage == 0 else "in"
        return WORDS.get(w, w)

    return IDENT.sub(one, src)


def splice(src, inc):
    if inc == "":
        return src
    i = src.find("#include")
    if i < 0:
        return src
    j = src.find("\n", i)
    return src[:i] + inc + src[j + 1 :]


def combo_of(src):
    i = src.find("[COMBO]")
    if i < 0:
        return "", ""
    rest = src[i + len("[COMBO]") :]
    j = 0
    while j < len(rest) and rest[j] in " \t\r\n":
        j += 1
    if j >= len(rest) or rest[j] != "{":
        return "", ""
    k = rest.find("}", j)
    blob = rest[j : k + 1]
    obj = json.loads(blob)
    name = obj.get("combo", "")
    raw = re.search(r'"default"\s*:\s*([^,}\s]+)', blob)
    default = raw.group(1) if raw else ""
    if default.startswith('"') and default.endswith('"'):
        default = default[1:-1]
    return str(name), default


def translate(src, stage, inc, oname, oval):
    name, default = combo_of(src)
    if oname == name and oval != "":
        val = oval
    else:
        val = default
    define = ""
    if name:
        define = f"#define {name} {val}\n"
    frag = ""
    if stage == 1:
        frag = "out vec4 fragColor;\n#define gl_FragColor fragColor\n"
    head = "#version 330 core\n" + define + MACROS + frag
    return head + rewrite(splice(src, inc), stage)


def section(text, label):
    a = text.index(label + "\n") + len(label) + 1
    b = text.index("\nEND\n", a)
    return text[a:b]


def bits(f):
    return struct.unpack("<I", struct.pack("<f", float(f)))[0]


def png_rows(path):
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("not a png")
    pos = 8
    w = h = 0
    idat = b""
    while pos < len(data):
        n = struct.unpack(">I", data[pos : pos + 4])[0]
        typ = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + n]
        pos += 12 + n
        if typ == b"IHDR":
            w, h = struct.unpack(">II", chunk[:8])
        elif typ == b"IDAT":
            idat += chunk
        elif typ == b"IEND":
            break
    raw = zlib.decompress(idat)
    stride = w * 4
    rows = []
    prev = bytearray(stride)
    i = 0
    for _y in range(h):
        filt = raw[i]
        i += 1
        row = bytearray(raw[i : i + stride])
        i += stride
        if filt == 1:
            for x in range(stride):
                left = row[x - 4] if x >= 4 else 0
                row[x] = (row[x] + left) & 255
        elif filt == 2:
            for x in range(stride):
                row[x] = (row[x] + prev[x]) & 255
        elif filt == 3:
            for x in range(stride):
                left = row[x - 4] if x >= 4 else 0
                row[x] = (row[x] + ((left + prev[x]) // 2)) & 255
        elif filt == 4:
            def paeth(a, b, c):
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                if pa <= pb and pa <= pc:
                    return a
                return b if pb <= pc else c

            for x in range(stride):
                a = row[x - 4] if x >= 4 else 0
                b = prev[x]
                c = prev[x - 4] if x >= 4 else 0
                row[x] = (row[x] + paeth(a, b, c)) & 255
        elif filt != 0:
            raise SystemExit(f"png filter {filt}")
        rows.append(bytes(row))
        prev = row
    return w, h, rows


def main():
    out = Path(sys.argv[1]).read_text()
    png = sys.argv[2]
    vert = (ROOT / "fixtures/m3/shaders/layer.vert").read_text()
    frag = (ROOT / "fixtures/m3/shaders/layer.frag").read_text()
    inc = (ROOT / "fixtures/m3/shaders/common.h").read_text()
    expect = {
        "VERT": translate(vert, 0, "", "TINT", "1"),
        "FRAG0": translate(frag, 1, inc, "", ""),
        "FRAG1": translate(frag, 1, inc, "TINT", "1"),
    }
    for label, want in expect.items():
        got = section(out, label)
        if got != want:
            print(label, "mismatch")
            print("--- got ---")
            print(got)
            print("--- want ---")
            print(want)
            raise SystemExit(1)
    w = 32.0
    plan = (
        f"PLAN fx={bits(4.0)} x0={bits((2*26-w)/w)} y0={bits((w-2*8)/w)} "
        f"x1={bits((2*30-w)/w)} y1={bits((w-2*12)/w)} "
        f"u0={bits(0.5)} v0={bits(0.0)} u1={bits(1.0)} v1={bits(1.0)} boost=0.5"
    )
    full = (
        f"FULL x0={bits(-1.0)} y0={bits(1.0)} x1={bits(1.0)} y1={bits(-1.0)} "
        f"u0={bits(0.0)} v0={bits(0.0)} u1={bits(1.0)} v1={bits(1.0)}"
    )
    if plan not in out:
        raise SystemExit("plan mismatch:\n" + plan)
    if full not in out:
        raise SystemExit("full mismatch:\n" + full)
    for key in ("GL init=1", "GL png=1", "TEX bg=0 sp=0", "GL plain eye=1", "GL tint boost=1", "GL sprite par=1", "GL qbg=1", "GL qfx=1", "GL qsp=1"):
        if key not in out:
            raise SystemExit("missing " + key)
    plain = re.search(r"GL plain=(\d+)", out)
    tint = re.search(r"GL tint=(\d+)", out)
    if not plain or plain.group(1) == "0" or not tint or tint.group(1) == "0":
        raise SystemExit("shader program failed")
    pw, ph, rows = png_rows(png)
    if (pw, ph) != (32, 32):
        raise SystemExit(f"size {pw}x{ph}")

    def px(x, y):
        return tuple(rows[y][x * 4 : x * 4 + 4])

    yellow = (255, 255, 0, 255)
    cyan = (0, 255, 255, 255)
    for p in ((0, 0), (15, 0), (26, 8), (29, 11)):
        if px(*p) != yellow:
            raise SystemExit(f"{p} {px(*p)} != yellow")
    for p in ((16, 0), (31, 0), (22, 8), (30, 8), (26, 7), (26, 12)):
        if px(*p) != cyan:
            raise SystemExit(f"{p} {px(*p)} != cyan")
    print("M3 PASS")


if __name__ == "__main__":
    main()

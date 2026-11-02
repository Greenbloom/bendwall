#!/usr/bin/env python3
"""Check Bend pkg/tex output against an independent reader."""
import os, struct, sys, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wetex, wepkg

def u32(b, o):
    return struct.unpack_from("<I", b, o)[0], o + 4

def i32(b, o):
    return struct.unpack_from("<i", b, o)[0], o + 4

def fbits(b, o):
    return struct.unpack_from("<I", b, o)[0], o + 4

def tag(b, o):
    s = b[o:o + 8].decode("ascii")
    assert b[o + 8] == 0
    return s, o + 9

def lz4_decompress(src: bytes, raw: int) -> bytes:
    out = bytearray()
    i = 0
    n = len(src)
    while i < n:
        tok = src[i]
        i += 1
        lit = tok >> 4
        if lit == 15:
            while True:
                c = src[i]
                i += 1
                lit += c
                if c != 255:
                    break
        out += src[i:i + lit]
        i += lit
        if i >= n:
            break
        off = src[i] | (src[i + 1] << 8)
        i += 2
        m = tok & 15
        if m == 15:
            while True:
                c = src[i]
                i += 1
                m += c
                if c != 255:
                    break
        m += 4
        p = len(out) - off
        for k in range(m):
            out.append(out[p + k])
    if len(out) != raw:
        raise AssertionError(f"lz4 len {len(out)} != {raw}")
    return bytes(out)

def expand(fmt, px):
    if fmt == 8:
        o = bytearray()
        for i in range(0, len(px), 2):
            v, a = px[i], px[i + 1]
            o += bytes((v, v, v, a))
        return bytes(o)
    if fmt == 9:
        o = bytearray()
        for v in px:
            o += bytes((v, v, v, 255))
        return bytes(o)
    return px

def parse_tex(b):
    o = 0
    ver_s, o = tag(b, o)
    assert ver_s.startswith("TEXV")
    ver = int(ver_s[4:])
    texi, o = tag(b, o)
    assert texi.startswith("TEXI")
    iv = int(texi[4:])
    fmt, o = u32(b, o)
    flags, o = u32(b, o)
    tw, o = u32(b, o)
    th, o = u32(b, o)
    iw, o = u32(b, o)
    ih, o = u32(b, o)
    depth = 1
    if flags & 0x40:
        depth, o = u32(b, o)
    unk = 0
    if iv > 0:
        unk, o = u32(b, o)
    tb, o = tag(b, o)
    assert tb.startswith("TEXB")
    bv = int(tb[4:])
    ni, o = u32(b, o)
    fif = -1
    if bv >= 3:
        fif, o = i32(b, o)
    cond = 0
    if bv >= 4:
        cond, o = u32(b, o)
    images = []
    if cond == 0:
        for _ in range(ni):
            nm, o = u32(b, o)
            mips = []
            for _ in range(nm):
                w, o = u32(b, o)
                h, o = u32(b, o)
                d = 1
                if flags & 0x40:
                    d, o = u32(b, o)
                lz = raw = 0
                if bv >= 2:
                    lz, o = u32(b, o)
                    raw, o = u32(b, o)
                csz, o = u32(b, o)
                data = b[o:o + csz]
                o += csz
                px = lz4_decompress(data, raw) if (lz & 1) else data
                mips.append(dict(w=w, h=h, d=d, lz=lz, raw=raw, csz=csz, px=px))
            images.append(mips)
    frames = []
    sw = sh = sv = 0
    if o < len(b) and cond == 0:
        ts, o = tag(b, o)
        assert ts.startswith("TEXS")
        sv = int(ts[4:])
        nf, o = u32(b, o)
        if sv >= 3:
            sw, o = u32(b, o)
            sh, o = u32(b, o)
        else:
            sw, sh = iw, ih
        for _ in range(nf):
            img, o = u32(b, o)
            tb_, o = fbits(b, o)
            g = []
            for _ in range(6):
                g.append(u32(b, o)[0] if sv == 1 else fbits(b, o)[0])
                o += 4
            frames.append((img, tb_, *g))
    if flags & 4 and not frames and cond == 0:
        err = 1
    else:
        err = 0
    return dict(err=err, ver=ver, fmt=fmt, flags=flags, tw=tw, th=th, iw=iw, ih=ih,
                dep=depth, unk=unk, bv=bv, fif=fif, cond=cond, ni=len(images),
                images=images, sw=sw, sh=sh, sv=sv, frames=frames)

def kv(line):
    out = {}
    if line.startswith("TEX ") or line.startswith("PKG ") or line.startswith("IMG ") or line.startswith("LZ4 "):
        body = line.split(" ", 1)[1]
    else:
        body = line
    for part in body.split(" "):
        if "=" in part:
            k, v = part.split("=", 1)
            out[k] = v
    return out

def hx(b):
    return b.hex()

def main():
    log = open(sys.argv[1]).read().splitlines()
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    fails = []
    def bad(msg):
        fails.append(msg)
        print("FAIL", msg)

    lz = [kv(l) for l in log if l.startswith("LZ4 ")]
    if lz[0]["name"] != "match" or lz[0]["px"] != "61626364656661626364" or lz[0]["ok"] != "1":
        bad(f"lz4 match {lz[0]}")
    if lz[1]["px"] != "07" * 16:
        bad(f"lz4 lit {lz[1]['px']}")
    # independent
    if lz4_decompress(bytes([96, 97, 98, 99, 100, 101, 102, 6, 0]), 10) != b"abcdefabcd":
        bad("python lz4 match")

    by_path = {}
    for line in log:
        if line.startswith(("PKG ", "TEX ", "IMG ")):
            d = kv(line)
            by_path.setdefault(d["path"], []).append((line[:3], d))

    for path, rows in by_path.items():
        full = os.path.join(root, path)
        blob = open(full, "rb").read()
        kind = rows[0][0]
        if kind == "PKG":
            got = rows[0][1]
            if path.endswith("pkg_badver.pkg"):
                if got["err"] != "3" or got["ver"] != "25":
                    bad(path)
                continue
            if path.endswith("pkg_trunc.pkg") or path.endswith("pkg_huge.pkg"):
                if got["err"] != "1":
                    bad(path + " err " + got["err"])
                continue
            if path.endswith("pkg_magic.pkg"):
                if got["err"] != "2":
                    bad(path)
                continue
            if path.endswith("pkg_longname.pkg"):
                if got["err"] != "0" or got["ents"] != "@0@1":
                    bad("longname " + got["ents"])
                continue
            p = wepkg.parse(blob)
            ents = ",".join(f"{e['name'].lower()}@{e['offset']}@{e['size']}" for e in p["entries"])
            if got["err"] != "0" or got["ver"] != str(int(p["magic"][4:])) or got["start"] != str(p["data_start"]) or got["ents"] != ents:
                bad(f"pkg {path} bend={got['ents']} oracle={ents} start {got['start']} vs {p['data_start']}")
            continue
        got = rows[0][1]
        if path.endswith("tex_badres.tex"):
            if got["err"] != "3":
                bad(path)
            continue
        if path.endswith("tex_depth.tex"):
            if got["err"] != "4":
                bad(path)
            continue
        if path.endswith("tex_badver.tex"):
            if got["err"] != "6" or got["ver"] != "6":
                bad(path)
            continue
        if path.endswith("tex_lz4bad.tex"):
            if got["err"] != "7":
                bad(path)
            continue
        if path.endswith("tex_cond.tex"):
            if got["err"] != "5" or got["cond"] != "1" or got["bv"] != "4":
                bad("cond " + str(got))
            continue
        exp = parse_tex(blob)
        if path.endswith("tex_nosprite.tex") and exp["err"] != 1:
            bad("oracle nosprite")
        keys = ["err", "ver", "fmt", "flags", "tw", "th", "iw", "ih", "dep", "unk", "bv", "fif", "cond", "ni", "sw", "sh", "sv"]
        for k in keys:
            if got[k] != str(exp[k]):
                bad(f"{path} {k} bend={got[k]} spec={exp[k]}")
        if exp["images"]:
            m = exp["images"][0][0]
            if got["nm"] != str(len(exp["images"][0])) or got["mw"] != str(m["w"]) or got["plen"] != str(len(m["px"])):
                bad(f"{path} mip {got['nm']} {got['plen']} vs {len(m['px'])}")
            if got["px"] != hx(m["px"])[:len(got["px"])]:
                bad(f"{path} pixels")
            if got["lz"] != str(m["lz"]) or got["raw"] != str(m["raw"]) or got["csz"] != str(m["csz"]):
                bad(f"{path} lz fields")
            ex = expand(exp["fmt"], m["px"])
            if got["exp"] and got["exp"] != hx(ex)[:len(got["exp"])]:
                bad(f"{path} expand {got['exp']} vs {hx(ex)[:len(got['exp'])]}")
        fr = "|".join(",".join(str(x) for x in f) for f in exp["frames"])
        if got["fr"] != fr:
            bad(f"{path} frames bend={got['fr']} spec={fr}")
        if "TEXI0000" not in open(full, "rb").read()[:20].decode("latin1") and exp["err"] == 0 and exp["cond"] == 0:
            h, _imgs = wetex.parse(blob, keep_data=False)
            if int(h["texv"][4:]) != exp["ver"] or h["format"] != exp["fmt"] or h["flags"] != exp["flags"]:
                bad(f"wetex header {path}")
            if h.get("frame_count", 0) != len(exp["frames"]):
                bad(f"wetex frames {path}")
        if len(rows) > 1:
            img = rows[1][1]
            side = open(full.replace(".tex", ".rgba"), "rb").read()
            sw, sh = struct.unpack_from("<II", side, 0)
            rgba = side[8:]
            if img["w"] != str(sw) or img["h"] != str(sh):
                bad(f"img size {path} {img['w']}x{img['h']}")
            pix = bytes.fromhex(img["px"])
            if path.endswith("tex_png.tex"):
                if pix != rgba[:len(pix)]:
                    bad("png pixels " + img["px"])
            else:
                for i in range(0, len(pix), 4):
                    if abs(pix[i] - rgba[i]) > 2 or pix[i + 1] > 2 or pix[i + 2] > 2 or pix[i + 3] != 255:
                        bad(f"jpeg pixel {pix[i:i+4]} vs {rgba[i:i+4]}")
                        break
    # png file really is the blob inside the tex
    png = parse_tex(open(os.path.join(root, "fixtures/m1/tex_png.tex"), "rb").read())
    blob = png["images"][0][0]["px"]
    assert blob.startswith(b"\x89PNG")
    print(f"checked {len(by_path)} files, {len(fails)} failures")
    if fails:
        sys.exit(1)
    print("M1 PASS")

if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Parse Wallpaper Engine .tex files (TEXV0005/TEXI0001/TEXB000n/TEXS000n).
Usage: wetex.py FILE [--png OUT.png]   (prints header JSON; optionally decodes mip 0 of image 0)"""
import struct, sys, json

class R:
    def __init__(s, b, o=0): s.b, s.o = b, o
    def u32(s): v = struct.unpack_from('<I', s.b, s.o)[0]; s.o += 4; return v
    def i32(s): v = struct.unpack_from('<i', s.b, s.o)[0]; s.o += 4; return v
    def f32(s): v = struct.unpack_from('<f', s.b, s.o)[0]; s.o += 4; return v
    def tag(s):
        v = s.b[s.o:s.o+8].decode('ascii'); assert s.b[s.o+8] == 0, 'tag not NUL-terminated'; s.o += 9; return v
    def take(s, n): v = s.b[s.o:s.o+n]; s.o += n; return v

def parse(b, keep_data=False):
    r = R(b)
    h = {'texv': r.tag(), 'texi': r.tag()}
    h.update(format=r.u32(), flags=r.u32(), tex_w=r.u32(), tex_h=r.u32())
    vol = bool(h['flags'] & 0x40)  # REA FUN_14015c760: flag 0x40 adds a depth field after img_h (volume/3D LUT)
    h.update(img_w=r.u32(), img_h=r.u32())
    if vol: h['depth'] = r.u32()
    h['unk_color'] = hex(r.u32())   # REA: read when TEXI version > 0; meaning unknown (looks ARGB)
    h['texb'] = r.tag()
    bv = int(h['texb'][4:])
    nimg = r.u32()
    if bv >= 3: h['image_format'] = r.i32()   # FreeImage FREE_IMAGE_FORMAT, -1 = raw
    if bv >= 4: h['texb4_cond'] = r.u32()      # REA FUN_14015c8d0: nonzero -> condition/variant table (inferred); 0 in all samples
    imgs = []
    for _ in range(nimg):
        mips = []
        for _ in range(r.u32()):
            m = {'w': r.u32(), 'h': r.u32()}
            if vol: m['d'] = r.u32()
            if bv >= 2:
                m['lz4'] = r.u32(); m['raw_size'] = r.u32()
            m['size'] = r.u32(); m['offset'] = r.o
            d = r.take(m['size'])
            if keep_data: m['data'] = d
            mips.append(m)
        imgs.append(mips)
    h['images'] = [[{k: v for k, v in m.items() if k != 'data'} for m in im] for im in imgs]
    if r.o < len(b):
        h['texs'] = r.tag(); sv = int(h['texs'][4:])
        n = r.u32()
        if sv >= 3: h['sheet_w'], h['sheet_h'] = r.u32(), r.u32()
        fr = []
        for _ in range(n):
            img = r.u32(); t = r.f32()
            # TEXS0001 stores geometry as u32, later versions as f32 (inferred from sample values)
            g = [r.u32() for _ in range(6)] if sv == 1 else [r.f32() for _ in range(6)]
            fr.append({'image': img, 'frametime': round(t, 5), 'x': g[0], 'y': g[1], 'w': g[2], 'w_y': g[3], 'h_x': g[4], 'h': g[5]})
        h['frame_count'] = n; h['frames_head'] = fr[:3]
    h['trailing_bytes'] = len(b) - r.o
    return h, imgs

def decode(h, mip):
    import lz4.block, numpy as np
    d = mip['data']
    if mip.get('lz4'): d = lz4.block.decompress(d, uncompressed_size=mip['raw_size'])
    if h.get('image_format', -1) != -1:
        from PIL import Image; import io
        return Image.open(io.BytesIO(d)).convert('RGBA')
    from PIL import Image
    w, hh, f = mip['w'], mip['h'], h['format']
    if f == 0: return Image.frombytes('RGBA', (w, hh), d[:w*hh*4])
    if f == 8: a = np.frombuffer(d[:w*hh*2], np.uint8).reshape(hh, w, 2); return Image.fromarray(np.dstack([a[..., 0], a[..., 1], np.zeros_like(a[..., 0]), np.full_like(a[..., 0], 255)]))
    if f == 9: return Image.frombytes('L', (w, hh), d[:w*hh])
    if f in (4, 6, 7): return Image.fromarray(dxt(d, w, hh, {7: 1, 6: 3, 4: 5}[f]))
    raise SystemExit('format %d not handled' % f)

def dxt(d, w, h, kind):
    import numpy as np
    out = np.zeros((h, w, 4), np.uint8)
    bs = 8 if kind == 1 else 16
    o = 0
    def c565(c): return [((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31]
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            blk = d[o:o+bs]; o += bs
            alpha = [255] * 16
            if kind == 5:
                a0, a1 = blk[0], blk[1]; bits = int.from_bytes(blk[2:8], 'little')
                pal = [a0, a1] + ([((6-i)*a0 + (i+1)*a1)//7 for i in range(6)] if a0 > a1 else [((4-i)*a0 + (i+1)*a1)//5 for i in range(4)] + [0, 255])
                alpha = [pal[(bits >> (3*i)) & 7] for i in range(16)]
            elif kind == 3:
                bits = int.from_bytes(blk[0:8], 'little'); alpha = [((bits >> (4*i)) & 15) * 17 for i in range(16)]
            cb = blk[bs-8:]
            c0, c1 = struct.unpack_from('<HH', cb, 0); idx = struct.unpack_from('<I', cb, 4)[0]
            p0, p1 = c565(c0), c565(c1)
            if c0 > c1 or kind != 1:
                pal = [p0, p1, [(2*a+b)//3 for a, b in zip(p0, p1)], [(a+2*b)//3 for a, b in zip(p0, p1)]]
            else:
                pal = [p0, p1, [(a+b)//2 for a, b in zip(p0, p1)], [0, 0, 0]]
            for i in range(16):
                y, x = by + i // 4, bx + i % 4
                if y < h and x < w:
                    out[y, x, :3] = pal[(idx >> (2*i)) & 3]; out[y, x, 3] = alpha[i]
    return out

if __name__ == '__main__':
    b = open(sys.argv[1], 'rb').read()
    h, imgs = parse(b, keep_data='--png' in sys.argv)
    print(json.dumps(h if len(json.dumps(h)) < 3000 else {k: v for k, v in h.items() if k != 'images'} | {'mip0': h['images'][0][0], 'n_images': len(h['images']), 'mips_per_image': [len(i) for i in h['images']]}))
    if '--png' in sys.argv:
        img = decode(h, imgs[0][0]); img.save(sys.argv[sys.argv.index('--png') + 1])

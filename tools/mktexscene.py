#!/usr/bin/env python3
"""Synthetic scene package, nested project.json, and tex decodes.

No Workshop bytes. The scene pkg is object → model → material → an LZ4
RGBA .tex. A nested "type" sits inside general.properties so a scanner
that takes the first key in the file would call the video a color.
"""
import os, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mkfixtures import lz4_literals, mip, pkg, png_rgba, tex

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLAY = os.path.join(ROOT, "fixtures", "play")

CYAN = bytes([0, 255, 255, 255])
MAGENTA = bytes([255, 0, 255, 255])
RED = bytes([255, 0, 0, 255])
YELLOW = bytes([255, 255, 0, 255])

# DXT5 4x4, alpha index 0, color 0x07E0 (exact green), color index 0.
DXT5_GREEN = bytes([
    255, 0, 0, 0, 0, 0, 0, 0,
    0xE0, 0x07, 0xE0, 0x07, 0, 0, 0, 0,
])


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def write_text(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def sky_tex():
    row = CYAN * 4 + MAGENTA * 4
    rgba = row * 4
    comp = lz4_literals(rgba)
    return tex(fmt=0, tw=8, th=4, iw=8, ih=4, bv=2,
               images=[[mip(8, 4, comp, lz4=1, raw=len(rgba))]])


def scene_pkg():
    scene = """{
  "general": {"orthogonalprojection": {"width": 160, "height": 90}},
  "preview": "decoy.png",
  "objects": [{
    "name": "bg",
    "image": "models/bg.json",
    "origin": "40 22.5 0",
    "size": "80 45",
    "scale": "1 1 1",
    "angles": "0 0 0",
    "alpha": 1
  }]
}
"""
    model = """{"material": "materials/bg.json", "fullscreen": false, "width": 80, "height": 45}
"""
    material = """{"passes": [{"textures": [null, "_rt_fullframe", "materials/sky"]}]}
"""
    return pkg([
        ("scene.json", scene.encode("utf-8")),
        ("models/bg.json", model.encode("utf-8")),
        ("materials/bg.json", material.encode("utf-8")),
        ("materials/sky.tex", sky_tex()),
    ], 1)


def oriented_clip(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    subprocess.run([
        "ffmpeg", "-y", "-loglevel", "error",
        "-f", "lavfi", "-i", "color=c=red:s=16x8:r=1:d=1",
        "-f", "lavfi", "-i", "color=c=0x00FF00:s=16x8:r=1:d=1",
        "-f", "lavfi", "-i", "color=c=0x00FF00:s=16x8:r=1:d=1",
        "-f", "lavfi", "-i", "color=c=red:s=16x8:r=1:d=1",
        "-filter_complex",
        "[0:v][1:v]vstack[a];[2:v][3:v]vstack[b];[a][b]concat=n=2:v=1:a=0,format=rgb24",
        "-c:v", "ffv1", path,
    ], check=True)


def half_rgba(w, h):
    left = w // 2
    row = CYAN * left + MAGENTA * (w - left)
    return row * h


def solid_rgba(w, h, px):
    return px * (w * h)


def dxt5_solid(w, h, block):
    if w % 4 or h % 4:
        raise SystemExit("dxt size")
    return block * ((w // 4) * (h // 4))


def named_scene_pkg(tex_name, tex_bytes, ver=9):
    scene = (
        '{"general":{"orthogonalprojection":{"width":160,"height":90}},'
        '"objects":[{"name":"bg","image":"models/bg.json","origin":"80 45 0",'
        '"size":"160 90","alpha":1}]}'
    )
    model = '{"material":"materials/bg.json","fullscreen":true,"width":160,"height":90}\n'
    material = '{"passes":[{"textures":["%s"]}]}\n' % tex_name
    return pkg([
        ("scene.json", scene.encode("utf-8")),
        ("models/bg.json", model.encode("utf-8")),
        ("materials/bg.json", material.encode("utf-8")),
        ("materials/%s.tex" % tex_name, tex_bytes),
    ], ver)


def bare_scene_pkg(tex_name, ver=9):
    """Scene whose texture is not in the package. Lookup must leave the pkg."""
    scene = (
        '{"general":{"orthogonalprojection":{"width":160,"height":90}},'
        '"objects":[{"name":"bg","image":"models/bg.json","alpha":1}]}'
    )
    model = '{"material":"materials/bg.json","fullscreen":true}\n'
    material = '{"passes":[{"textures":["%s"]}]}\n' % tex_name
    return pkg([
        ("scene.json", scene.encode("utf-8")),
        ("models/bg.json", model.encode("utf-8")),
        ("materials/bg.json", material.encode("utf-8")),
    ], ver)


def project(dirpath):
    write_text(os.path.join(dirpath, "project.json"),
               '{ "type": "scene", "file": "scene.json" }\n')


def main():
    write(os.path.join(PLAY, "texscene", "scene.pkg"), scene_pkg())
    write(os.path.join(PLAY, "texscene", "decoy.png"), png_rgba(8, 8, YELLOW * 64))
    write_text(os.path.join(PLAY, "texscene", "project.json"), """{
  "general": {"properties": {"schemecolor": {"type": "color", "value": "1 0 0"}}},
  "file": "not-the-scene.json",
  "type": "scene"
}
""")
    write_text(os.path.join(PLAY, "nested", "project.json"), """{
  "general": {"properties": {"schemecolor": {"type": "color", "file": "nested-not-a-video.mp4", "title": "Nested title", "value": "1 0 0"}}},
  "file": "clip.mkv",
  "title": "Night drive",
  "type": "video"
}
""")
    clip = os.path.join(PLAY, "nested", "clip.mkv")
    if not os.path.exists(clip):
        oriented_clip(clip)
    write(os.path.join(PLAY, "dxt.tex"), tex(
        fmt=4, tw=4, th=4, iw=4, ih=4, bv=2,
        images=[[mip(4, 4, DXT5_GREEN)]],
    ))
    png = png_rgba(2, 2, RED * 4)
    write(os.path.join(PLAY, "png.tex"), tex(
        fmt=0, tw=2, th=2, iw=2, ih=2, bv=3, fif=13,
        images=[[mip(2, 2, png, raw=len(png))]],
    ))
    write_text(os.path.join(PLAY, "missing", "project.json"), """{
  "type": "scene",
  "title": "Gone",
  "file": "scene.json"
}
""")
    write_text(os.path.join(PLAY, "missing", "scene.json"), """{
  "objects": [{"name": "bg", "image": "models/gone.json"}]
}
""")
    write_text(os.path.join(PLAY, "web", "project.json"), """{
  "general": {"properties": {"schemecolor": {"type": "color", "value": "1 0 0"}}},
  "file": "index.html",
  "title": "Colorful Matrix",
  "type": "web"
}
""")
    write_text(os.path.join(PLAY, "web", "index.html"),
               "<!DOCTYPE html><html><body style=\"margin:0;background:#00ff00\"></body></html>\n")

    # Short texture name inside PKGV0009. The member is materials/ztyz.tex.
    ztyz = half_rgba(8, 4)
    zcomp = lz4_literals(ztyz)
    write(os.path.join(PLAY, "named", "scene.pkg"), named_scene_pkg(
        "ztyz",
        tex(fmt=0, tw=8, th=4, iw=8, ih=4, bv=2,
            images=[[mip(8, 4, zcomp, lz4=1, raw=len(ztyz))]]),
        ver=9,
    ))
    project(os.path.join(PLAY, "named"))

    # Stored entries past the old 2MB read. PKGV0009 again.
    lw, lh = 720, 720
    lraw = half_rgba(lw, lh)
    if len(lraw) <= 2000000:
        raise SystemExit("lz4 fixture is not big")
    lcomp = lz4_literals(lraw)
    write(os.path.join(PLAY, "pkg9big", "lz", "scene.pkg"), named_scene_pkg(
        "biglz",
        tex(fmt=0, tw=lw, th=lh, iw=lw, ih=lh, bv=2,
            images=[[mip(lw, lh, lcomp, lz4=1, raw=len(lraw))]]),
        ver=9,
    ))
    project(os.path.join(PLAY, "pkg9big", "lz"))
    dw, dh = 1664, 1280
    draw = dxt5_solid(dw, dh, DXT5_GREEN)
    if len(draw) <= 2000000:
        raise SystemExit("dxt fixture is not big")
    write(os.path.join(PLAY, "pkg9big", "dxt", "scene.pkg"), named_scene_pkg(
        "bigdxt",
        tex(fmt=4, tw=dw, th=dh, iw=dw, ih=dh, bv=2,
            images=[[mip(dw, dh, draw)]]),
        ver=9,
    ))
    project(os.path.join(PLAY, "pkg9big", "dxt"))

    blue = bytes([10, 40, 240, 255])
    write(os.path.join(PLAY, "assetroot", "materials", "fromassets.tex"), tex(
        fmt=0, images=[[mip(2, 2, blue * 4)]],
    ))
    write(os.path.join(PLAY, "fromassets", "scene.pkg"), bare_scene_pkg("fromassets", ver=9))
    project(os.path.join(PLAY, "fromassets"))

    write(os.path.join(PLAY, "badtex", "scene.pkg"), named_scene_pkg(
        "bad", b"TEXV0005\x00this is not a texture", ver=9,
    ))
    project(os.path.join(PLAY, "badtex"))
    print("mktexscene ok")


if __name__ == "__main__":
    main()

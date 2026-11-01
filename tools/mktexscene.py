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
    oriented_clip(os.path.join(PLAY, "nested", "clip.mkv"))
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
    print("mktexscene ok")


if __name__ == "__main__":
    main()

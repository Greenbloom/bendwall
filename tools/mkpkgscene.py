#!/usr/bin/env python3
"""Synthetic scene.pkg / gifscene.pkg fixtures. No Workshop bytes."""
import os, shutil, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mkfixtures import pkg

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SCENE_IMAGE = """{
  "objects": [
    {"name": "bg", "image": "models/bg.json"}
  ]
}
"""

SCENE_GIF = """{
  "objects": [
    {"name": "bg", "image": "models/gif.json"}
  ]
}
"""

SCENE_FX = """{
  "objects": [
    {"name": "bg", "image": "models/bg.json", "effects": [
      {"file": "effects/tint/effect.json"},
      {"file": "effects/pulse/effect.json"}
    ]},
    {"name": "sparks", "particle": "particles/sparks.json"}
  ]
}
"""

ASSETS_SCENE = """{
  "objects": [
    {"name": "bg", "image": "models/from-assets.json"}
  ]
}
"""


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def text(path, s):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(s)


def main():
    write(os.path.join(ROOT, "fixtures/workshop/20/scene.pkg"), pkg([("scene.json", SCENE_IMAGE.encode())], 1))
    write(os.path.join(ROOT, "fixtures/workshop/21/gifscene.pkg"), pkg([("gifscene.json", SCENE_GIF.encode())], 2))
    write(os.path.join(ROOT, "fixtures/workshop/23/scene.pkg"), pkg([("scene.json", SCENE_FX.encode())], 1))
    text(os.path.join(ROOT, "fixtures/assets-probe/scenes/from-assets.json"), ASSETS_SCENE)

    scene = open(os.path.join(ROOT, "fixtures/m2/base/scene.json"), "rb").read()
    bg = open(os.path.join(ROOT, "fixtures/m3/bg.tex"), "rb").read()
    sp = open(os.path.join(ROOT, "fixtures/m3/sprite.tex"), "rb").read()
    write(os.path.join(ROOT, "fixtures/pkgscene/scene.pkg"), pkg([
        ("scene.json", scene),
        ("bg.tex", bg),
        ("sprite.tex", sp),
    ], 1))
    assets = os.path.join(ROOT, "fixtures/pkgassets")
    os.makedirs(os.path.join(assets, "shaders"), exist_ok=True)
    os.makedirs(os.path.join(assets, "materials"), exist_ok=True)
    for name in ("layer.vert", "layer.frag", "common.h"):
        shutil.copy(os.path.join(ROOT, "fixtures/m3/shaders", name), os.path.join(assets, "shaders", name))
    shutil.copy(os.path.join(ROOT, "fixtures/m3/materials/layer.json"), os.path.join(assets, "materials/layer.json"))
    print("pkg fixtures ok")


if __name__ == "__main__":
    main()

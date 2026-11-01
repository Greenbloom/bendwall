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
    # PKGV0002, scene.json is the third entry and its offset is not 0.
    write(os.path.join(ROOT, "fixtures/workshop/30/scene.pkg"), pkg([
        ("particles/presets/leaves5.json", b'{"k":"leaves"}'),
        ("materials/anime-cityscape-anime-girls-landscape.json", b'{"passes":[]}'),
        ("scene.json", SCENE_IMAGE.encode()),
        ("models/girl.json", b'{"material":"materials/x"}'),
        ("materials/city.tex", b"\x11" * 32),
        ("materials/presets/leaves5.json", b'{"k":"preset"}'),
    ], 2))
    # PKGV0001 gifscene: gifscene.json first, then material, model, tex.
    write(os.path.join(ROOT, "fixtures/workshop/31/gifscene.pkg"), pkg([
        ("gifscene.json", SCENE_GIF.encode()),
        ("materials/background.json", b'{"passes":[]}'),
        ("models/background.json", b'{"material":"materials/background"}'),
        ("materials/background.tex", b"\x22" * 32),
    ], 1))
    # PKGV0006, scene.json first, several following entries.
    v6 = [("scene.json", SCENE_IMAGE.encode())]
    for i in range(7):
        v6.append((f"materials/e{i}.json", b'{"n":%d}' % i))
    write(os.path.join(ROOT, "fixtures/workshop/32/scene.pkg"), pkg(v6, 6))
    text(os.path.join(ROOT, "fixtures/assets-probe/scenes/from-assets.json"), ASSETS_SCENE)

    scene = open(os.path.join(ROOT, "fixtures/m2/base/scene.json"), "rb").read()
    bg = open(os.path.join(ROOT, "fixtures/m3/bg.tex"), "rb").read()
    sp = open(os.path.join(ROOT, "fixtures/m3/sprite.tex"), "rb").read()
    write(os.path.join(ROOT, "fixtures/pkgscene/scene.pkg"), pkg([
        ("scene.json", scene),
        ("bg.tex", bg),
        ("sprite.tex", sp),
    ], 1))
    # Same pixels, but scene.json is not the first entry (PKGV0002 shape).
    write(os.path.join(ROOT, "fixtures/pkgmid/scene.pkg"), pkg([
        ("particles/presets/leaves5.json", b'{"k":"leaves"}'),
        ("materials/anime-cityscape-anime-girls-landscape.json", b'{"passes":[]}'),
        ("scene.json", scene),
        ("models/girl.json", b'{"material":"materials/x"}'),
        ("bg.tex", bg),
        ("sprite.tex", sp),
    ], 2))
    fat = os.path.join(ROOT, "build/pkgfat")
    mark = b'{"mark":"city-scene"}'
    gifm = b'{"mark":"gif-scene"}'
    late = b'{"mark":"after-tex"}'
    write(os.path.join(fat, "v2/scene.pkg"), pkg([
        ("particles/presets/leaves5.json", b'{"k":"leaves"}'),
        ("materials/anime-cityscape-anime-girls-landscape.json", b'{"passes":[]}'),
        ("scene.json", mark),
        ("models/girl.json", b'{"material":"materials/x"}'),
        ("materials/big.tex", b"\x11" * 2100000),
        ("materials/presets/leaves5.json", b'{"k":"preset"}'),
    ], 2))
    write(os.path.join(fat, "gif/gifscene.pkg"), pkg([
        ("gifscene.json", gifm),
        ("materials/background.json", b'{"passes":[]}'),
        ("models/background.json", b'{"material":"materials/background"}'),
        ("materials/background.tex", b"\x22" * 2100000),
    ], 1))
    write(os.path.join(fat, "late/scene.pkg"), pkg([
        ("materials/big.tex", b"\x33" * 2100000),
        ("scene.json", late),
    ], 2))
    assets = os.path.join(ROOT, "fixtures/pkgassets")
    os.makedirs(os.path.join(assets, "shaders"), exist_ok=True)
    os.makedirs(os.path.join(assets, "materials"), exist_ok=True)
    for name in ("layer.vert", "layer.frag", "common.h"):
        shutil.copy(os.path.join(ROOT, "fixtures/m3/shaders", name), os.path.join(assets, "shaders", name))
    shutil.copy(os.path.join(ROOT, "fixtures/m3/materials/layer.json"), os.path.join(assets, "materials/layer.json"))
    print("pkg fixtures ok")


if __name__ == "__main__":
    main()

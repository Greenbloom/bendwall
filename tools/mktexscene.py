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
GREEN = bytes([0, 255, 0, 255])
BLUE = bytes([0, 40, 255, 255])
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


def write_quad(dirpath, image_json):
    """Half cyan, half magenta, placed like the texscene quad."""
    project(dirpath)
    write_text(os.path.join(dirpath, "scene.json"), """{
  "general": {"orthogonalprojection": {"width": 160, "height": 90}},
  "objects": [{
    "name": "bg",
    "image": %s,
    "origin": "40 22.5 0",
    "size": "80 45",
    "alpha": 1
  }]
}
""" % image_json)
    write_text(os.path.join(dirpath, "models", "bg.json"),
               '{"material": "materials/bg.json", "fullscreen": false, "width": 80, "height": 45}\n')
    write_text(os.path.join(dirpath, "materials", "bg.json"),
               '{"passes": [{"textures": ["sky"]}]}\n')
    write(os.path.join(dirpath, "materials", "sky.tex"), sky_tex())


def write_mat(dirpath):
    """The image object is a material reference, not a model path."""
    project(dirpath)
    write_text(os.path.join(dirpath, "scene.json"), """{
  "general": {"orthogonalprojection": {"width": 160, "height": 90}},
  "objects": [{
    "name": "bg",
    "image": {"material": "bg"},
    "origin": "40 22.5 0",
    "size": "80 45",
    "alpha": 1
  }]
}
""")
    write_text(os.path.join(dirpath, "materials", "bg.json"),
               '{"passes": [{"textures": ["sky"]}]}\n')
    write(os.path.join(dirpath, "materials", "sky.tex"), sky_tex())


def write_mixed(dirpath):
    """One missing image and one decoded image. The probe must say partial."""
    project(dirpath)
    write_text(os.path.join(dirpath, "scene.json"), """{
  "general": {"orthogonalprojection": {"width": 160, "height": 90}},
  "objects": [
    {"name": "gone", "image": {"file": "models/missing-face.json"}},
    {"name": "bg", "image": "models/bg.json", "origin": "80 45 0", "size": "160 90", "alpha": 1}
  ]
}
""")
    write_text(os.path.join(dirpath, "models", "bg.json"),
               '{"material": "materials/bg.json", "fullscreen": true, "width": 160, "height": 90}\n')
    write_text(os.path.join(dirpath, "materials", "bg.json"),
               '{"passes": [{"textures": ["sky"]}]}\n')
    write(os.path.join(dirpath, "materials", "sky.tex"), sky_tex())


def gif_tex():
    """Two TEXB images and a TEXS0001 sheet. Frame 0 is red, frame 1 is green."""
    red = RED * (8 * 8)
    green = GREEN * (8 * 8)
    return tex(
        fmt=0, flags=0x4, tw=8, th=8, iw=8, ih=8, bv=2,
        images=[
            [mip(8, 8, lz4_literals(red), lz4=1, raw=len(red))],
            [mip(8, 8, lz4_literals(green), lz4=1, raw=len(green))],
        ],
        sprites={
            "sv": 1,
            "frames": [
                {"img": 0, "t": 0.5, "x": 0, "y": 0, "w": 8, "wy": 0, "hx": 0, "h": 8},
                {"img": 1, "t": 2.0, "x": 0, "y": 0, "w": 8, "wy": 0, "hx": 0, "h": 8},
            ],
        },
    )


def gif_pkg():
    scene = (
        '{"general":{"orthogonalprojection":{"width":160,"height":90}},'
        '"objects":[{"name":"bg","image":"models/bg.json","alpha":1}]}'
    )
    model = '{"material":"materials/bg.json","fullscreen":true,"width":160,"height":90}\n'
    material = '{"passes":[{"textures":["background"]}]}\n'
    return pkg([
        ("gifscene.json", scene.encode("utf-8")),
        ("models/bg.json", model.encode("utf-8")),
        ("materials/bg.json", material.encode("utf-8")),
        ("materials/background.tex", gif_tex()),
    ], 9)


def sheet_tex():
    """One atlas, TEXS0003 float rects. Left is red, right is blue."""
    rgba = (RED * 4 + BLUE * 4) * 4
    return tex(
        fmt=0, flags=0x4, tw=8, th=4, iw=8, ih=4, bv=2,
        images=[[mip(8, 4, rgba)]],
        sprites={
            "sv": 3,
            "sw": 8,
            "sh": 4,
            "frames": [
                {"img": 0, "t": 0.5, "x": 0, "y": 0, "w": 4, "wy": 0, "hx": 0, "h": 4},
                {"img": 0, "t": 2.0, "x": 4, "y": 0, "w": 4, "wy": 0, "hx": 0, "h": 4},
            ],
        },
    )


def write_spark():
    """A dark plate and one additive sprite that travels +x at 70 px/s."""
    bg = bytes([12, 16, 28, 255]) * 16
    white = bytes([255, 255, 255, 255]) * 16
    part = """{
  "material": "materials/spark.json",
  "maxcount": 8,
  "starttime": 0,
  "emitter": [{
    "name": "sphererandom",
    "rate": 1,
    "origin": "0 0 0",
    "directions": "1 0 0",
    "distancemin": 0,
    "distancemax": 0,
    "speedmin": 70,
    "speedmax": 70
  }],
  "initializer": [
    {"name": "lifetimerandom", "min": 3, "max": 3},
    {"name": "sizerandom", "min": 16, "max": 16},
    {"name": "alpharandom", "min": 1, "max": 1},
    {"name": "colorrandom", "min": "1 0.47 0.12", "max": "1 0.47 0.12"},
    {"name": "rotationrandom", "min": 0, "max": 0}
  ],
  "operator": [
    {"name": "movement", "gravity": "0 0 0", "drag": 0},
    {"name": "alphafade", "fadeintime": 0, "fadeouttime": 0.4},
    {"name": "sizechange", "startvalue": 1, "endvalue": 0.5},
    {"name": "colorchange", "startvalue": "1 0.47 0.12", "endvalue": "1 0.9 0.2"},
    {"name": "angularmovement", "speed": 0}
  ],
  "renderer": [{"name": "sprite"}]
}
"""
    scene = """{
  "general": {"orthogonalprojection": {"width": 160, "height": 90}},
  "objects": [
    {"name": "bg", "image": "textures/bg.png", "origin": "80 45 0", "size": "160 90 0"},
    {
      "name": "spark",
      "particle": "particles/spark.json",
      "origin": "24 45 0",
      "angles": "0 0 0",
      "scale": "1 1 1",
      "alpha": 1,
      "instanceoverride": {
        "alpha": 1, "count": 1, "lifetime": 1, "rate": 1,
        "size": 1, "speed": 1, "brightness": 1
      }
    }
  ]
}
"""
    project = '{ "type": "scene", "file": "scene.json" }\n'
    local_mat = """{"passes": [{"blending": "additive", "textures": ["textures/spark.png"]}]}
"""
    asset_mat = """{"passes": [{"blending": "additive", "textures": ["materials/particle/spark.png"]}]}
"""
    for name, mat in (("spark", local_mat), ("sparkasset", asset_mat)):
        root = os.path.join(PLAY, name)
        write(os.path.join(root, "textures", "bg.png"), png_rgba(4, 4, bg))
        write_text(os.path.join(root, "particles", "spark.json"), part)
        write_text(os.path.join(root, "materials", "spark.json"), mat)
        write_text(os.path.join(root, "scene.json"), scene)
        write_text(os.path.join(root, "project.json"), project)
    write(os.path.join(PLAY, "spark", "textures", "spark.png"), png_rgba(4, 4, white))
    write(os.path.join(PLAY, "spark-assets", "materials", "particle", "spark.png"), png_rgba(4, 4, white))


def tone_clip(path, acodec, vcodec, extra):
    # This ffmpeg's sine source cannot set amplitude and peaks near 1/8.
    # aevalsrc is a full-scale stereo 440 Hz tone, and the mp4 is AAC.
    cmd = [
        "ffmpeg", "-y", "-loglevel", "error",
        "-f", "lavfi", "-i",
        "aevalsrc=0.9*sin(440*2*PI*t)|0.9*sin(440*2*PI*t):s=44100:d=8",
        "-f", "lavfi", "-i", "color=c=red:s=64x36:r=10:d=8",
        "-map", "1:v", "-map", "0:a", "-shortest",
        "-c:v", vcodec, "-c:a", acodec,
    ] + extra + [path]
    subprocess.run(cmd, check=True)


def write_vidaud():
    base = os.path.join(PLAY, "vidaud")
    os.makedirs(base, exist_ok=True)
    tone_clip(os.path.join(base, "tone.mkv"), "pcm_s16le", "mpeg4", [])
    mp4 = os.path.join(base, "tone.mp4")
    try:
        tone_clip(mp4, "aac", "libx264", ["-b:a", "160k", "-pix_fmt", "yuv420p"])
    except subprocess.CalledProcessError:
        tone_clip(mp4, "aac", "mpeg4", ["-b:a", "160k"])
    write_text(os.path.join(base, "project.json"),
               '{ "type": "video", "file": "tone.mkv", "title": "Tone",'
               ' "general": {"volume": 1, "mute": false} }\n')
    write_text(os.path.join(PLAY, "vidaudmute", "project.json"),
               '{ "type": "video", "file": "../vidaud/tone.mkv", "title": "Muted",'
               ' "general": {"volume": 1, "mute": true} }\n')
    write_text(os.path.join(PLAY, "vidaudhalf", "project.json"),
               '{ "type": "video", "file": "../vidaud/tone.mkv", "title": "Half",'
               ' "general": {"properties": [{"name": "volume", "type": "slider", "value": 50}]} }\n')
    write_text(os.path.join(PLAY, "vidaudmap", "project.json"),
               '{ "type": "video", "file": "../vidaud/tone.mkv", "title": "Map",'
               ' "general": {"volume": 1, "properties": {"volume": {"type": "slider", "value": 25}}} }\n')
    write_text(os.path.join(PLAY, "vidaudmp4", "project.json"),
               '{ "type": "video", "file": "../vidaud/tone.mp4", "title": "Aac",'
               ' "general": {"volume": 100} }\n')


def solid_tex(w, h, px):
    rgba = px * (w * h)
    return tex(fmt=0, tw=w, th=h, iw=w, ih=h, bv=2, images=[[mip(w, h, rgba)]])


def blank_gif_tex():
    """Frame 0 is fully transparent. Frame 1 is opaque red."""
    clear = bytes([0, 0, 0, 0]) * (8 * 8)
    red = RED * (8 * 8)
    return tex(
        fmt=0, flags=0x4, tw=8, th=8, iw=8, ih=8, bv=2,
        images=[
            [mip(8, 8, clear)],
            [mip(8, 8, red)],
        ],
        sprites={
            "sv": 1,
            "frames": [
                {"img": 0, "t": 0.5, "x": 0, "y": 0, "w": 8, "wy": 0, "hx": 0, "h": 8},
                {"img": 1, "t": 2.0, "x": 0, "y": 0, "w": 8, "wy": 0, "hx": 0, "h": 8},
            ],
        },
    )


def part_body(rate, count, speed, size, life, burst, texture):
    tex_json = "null" if texture is None else '"%s"' % texture
    inst = 1 if burst else 0
    return """{
  "material": "materials/p.json",
  "maxcount": %d,
  "starttime": 0,
  "emitter": [{
    "name": "sphererandom",
    "rate": %s,
    "instantaneous": %d,
    "origin": "0 0 0",
    "directions": "1 0 0",
    "distancemin": 0,
    "distancemax": 0,
    "speedmin": %s,
    "speedmax": %s
  }],
  "initializer": [
    {"name": "lifetimerandom", "min": %s, "max": %s},
    {"name": "sizerandom", "min": %s, "max": %s},
    {"name": "alpharandom", "min": 1, "max": 1},
    {"name": "colorrandom", "min": "1 1 1", "max": "1 1 1"},
    {"name": "rotationrandom", "min": 0, "max": 0}
  ],
  "operator": [
    {"name": "movement", "gravity": "0 0 0", "drag": 0}
  ],
  "renderer": [{"name": "sprite"}]
}
""" % (count, rate, inst, speed, speed, life, life, size, size), tex_json


def write_one_part(root, origin, rate, count, speed, size, life, burst, texture, blending):
    body, tex_json = part_body(rate, count, speed, size, life, burst, texture)
    write_text(os.path.join(root, "particles", "p.json"), body)
    write_text(os.path.join(root, "materials", "p.json"),
               '{"passes": [{"blending": "%s", "textures": [%s]}]}\n' % (blending, tex_json))
    scene = """{
  "general": {"orthogonalprojection": {"width": 160, "height": 90}},
  "objects": [
    {"name": "bg", "image": "textures/bg.png", "origin": "80 45 0", "size": "160 90 0"},
    {
      "name": "p",
      "particle": "particles/p.json",
      "origin": "%s 0",
      "angles": "0 0 0",
      "scale": "1 1 1",
      "alpha": 1
    }
  ]
}
""" % origin
    write_text(os.path.join(root, "scene.json"), scene)
    write_text(os.path.join(root, "project.json"),
               '{ "type": "scene", "file": "scene.json", "title": "Particles" }\n')
    write(os.path.join(root, "textures", "bg.png"), png_rgba(4, 4, bytes([12, 16, 28, 255]) * 16))


def write_resolve():
    """Jpg slots, null images, user properties, instance textures, blank gif."""
    plate = bytes([12, 16, 28, 255])
    yellow = bytes([240, 200, 16, 255])
    city = bytes([10, 80, 240, 255])
    poster = bytes([0, 200, 60, 255])
    black = bytes([0, 0, 0, 255])
    write(os.path.join(PLAY, "halo-assets", "materials", "particle", "halo_4.tex"),
          solid_tex(4, 4, yellow))
    halo = os.path.join(PLAY, "halo")
    write_one_part(halo, "24 45", "1", 8, "70", "16", "3", 0, "particle/halo_4.jpg", "translucent")
    soft = os.path.join(PLAY, "softpart")
    write_one_part(soft, "80 45", "1", 4, "0", "24", "3", 1, None, "translucent")
    nope = os.path.join(PLAY, "nope")
    write_one_part(nope, "80 45", "1", 4, "0", "24", "3", 1, "particle/gone.jpg", "translucent")

    city_dir = os.path.join(PLAY, "city")
    project(city_dir)
    write_text(os.path.join(city_dir, "scene.json"), """{
  "general": {"orthogonalprojection": {"width": 160, "height": 90}},
  "objects": [
    {"name": "bg", "image": "models/bg.json", "origin": "80 45 0", "size": "160 90", "alpha": 1},
    {"name": "decoy", "image": null}
  ]
}
""")
    write_text(os.path.join(city_dir, "models", "bg.json"),
               '{"material": "materials/bg.json", "fullscreen": true, "width": 160, "height": 90}\n')
    write_text(os.path.join(city_dir, "materials", "bg.json"),
               '{"passes": [{"textures": [{"name": "backgrounds/city.jpg"}]}]}\n')
    write(os.path.join(city_dir, "materials", "backgrounds", "city.tex"), solid_tex(4, 4, city))

    user = os.path.join(PLAY, "userpic")
    write_text(os.path.join(user, "project.json"),
               '{ "type": "scene", "file": "scene.json", "title": "User",'
               ' "general": {"properties": {"picture": {"type": "file",'
               ' "value": "backgrounds/poster.jpg"}}} }\n')
    write_text(os.path.join(user, "scene.json"), """{
  "general": {"orthogonalprojection": {"width": 160, "height": 90}},
  "objects": [{
    "name": "bg",
    "image": {"user": "picture"},
    "origin": "80 45 0",
    "size": "160 90",
    "alpha": 1
  }]
}
""")
    write(os.path.join(user, "materials", "backgrounds", "poster.tex"), solid_tex(4, 4, poster))

    inst = os.path.join(PLAY, "instpic")
    project(inst)
    write_text(os.path.join(inst, "scene.json"), """{
  "general": {"orthogonalprojection": {"width": 160, "height": 90}},
  "objects": [{
    "name": "bg",
    "image": "models/bg.json",
    "origin": "80 45 0",
    "size": "160 90",
    "alpha": 1,
    "instance": {"textures": ["backgrounds/poster.jpg"]}
  }]
}
""")
    write_text(os.path.join(inst, "models", "bg.json"),
               '{"material": "materials/bg.json", "fullscreen": true}\n')
    write_text(os.path.join(inst, "materials", "bg.json"),
               '{"passes": [{"textures": ["black"]}]}\n')
    write(os.path.join(inst, "materials", "black.tex"), solid_tex(2, 2, black))
    write(os.path.join(inst, "materials", "backgrounds", "poster.tex"), solid_tex(4, 4, poster))

    titled = os.path.join(PLAY, "titled")
    write_text(os.path.join(titled, "project.json"),
               '{ "type": "scene", "file": "scene.json",'
               ' "title": "Tokyo Night [2560x1440p]" }\n')
    write_text(os.path.join(titled, "scene.json"), """{
  "general": {"orthogonalprojection": {"width": 160, "height": 90}},
  "objects": [{
    "name": "bg", "image": "textures/bg.png",
    "origin": "80 45 0", "size": "160 90 0", "alpha": 1
  }]
}
""")
    write(os.path.join(titled, "textures", "bg.png"), png_rgba(4, 4, plate * 16))

    blank = os.path.join(PLAY, "blankgif")
    write(os.path.join(blank, "gifscene.pkg"), pkg([
        ("gifscene.json",
         ('{"general":{"orthogonalprojection":{"width":160,"height":90}},'
          '"objects":[{"name":"bg","image":"models/bg.json","alpha":1}]}').encode("utf-8")),
        ("models/bg.json",
         b'{"material":"materials/bg.json","fullscreen":true,"width":160,"height":90}\n'),
        ("materials/bg.json", b'{"passes":[{"textures":["background"]}]}\n'),
        ("materials/background.tex", blank_gif_tex()),
    ], 9))
    write_text(os.path.join(blank, "project.json"),
               '{ "type": "scene", "file": "gifscene.json", "title": "Blank frame" }\n')

    swarm = os.path.join(PLAY, "swarm")
    project(swarm)
    write(os.path.join(swarm, "textures", "dot.png"), png_rgba(2, 2, bytes([255, 255, 255, 255]) * 4))
    write(os.path.join(swarm, "textures", "bg.png"), png_rgba(2, 2, plate * 4))
    write_text(os.path.join(swarm, "particles", "p.json"), """{
  "material": "materials/p.json",
  "maxcount": 128,
  "starttime": 0,
  "emitter": [{
    "name": "sphererandom", "rate": 8, "instantaneous": 1,
    "origin": "0 0 0", "directions": "1 0 0",
    "distancemin": 0, "distancemax": 0, "speedmin": 10, "speedmax": 10
  }],
  "initializer": [
    {"name": "lifetimerandom", "min": 4, "max": 4},
    {"name": "sizerandom", "min": 4, "max": 4},
    {"name": "alpharandom", "min": 1, "max": 1},
    {"name": "colorrandom", "min": "1 1 1", "max": "1 1 1"}
  ],
  "operator": [{"name": "movement", "gravity": "0 0 0", "drag": 0}]
}
""")
    write_text(os.path.join(swarm, "materials", "p.json"),
               '{"passes": [{"blending": "translucent", "textures": ["textures/dot.png"]}]}\n')
    objs = []
    for i in range(8):
        objs.append(
            '{"name":"e%d","image":null,"particle":"particles/p.json","origin":"%d 45 0","alpha":1}'
            % (i, 10 + i * 16)
        )
    write_text(os.path.join(swarm, "scene.json"),
               '{"general":{"orthogonalprojection":{"width":640,"height":360}},'
               '"objects":[{"name":"bg","image":"textures/bg.png","origin":"320 180 0","size":"640 360 0"},'
               + ",".join(objs) + "]}\n")


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

    # image is an object whose file is the model, not the word "image".
    write_quad(os.path.join(PLAY, "objimg"), '{"file": "models/bg.json"}')
    # image is a material reference, then textures[0].
    write_mat(os.path.join(PLAY, "matref"))
    write_text(os.path.join(PLAY, "objmiss", "project.json"),
               '{ "type": "scene", "file": "scene.json", "title": "Missing face" }\n')
    write_text(os.path.join(PLAY, "objmiss", "scene.json"),
               '{ "objects": [{ "name": "bg", "image": {"file": "models/missing-face.json"} }] }\n')
    write_mixed(os.path.join(PLAY, "mixed"))
    write(os.path.join(PLAY, "gifscene", "gifscene.pkg"), gif_pkg())
    write_text(os.path.join(PLAY, "gifscene", "project.json"),
               '{ "type": "scene", "file": "gifscene.json", "title": "Hack page" }\n')
    write(os.path.join(PLAY, "sheet.tex"), sheet_tex())
    write_spark()
    write_vidaud()
    write_resolve()
    print("mktexscene ok")


if __name__ == "__main__":
    main()

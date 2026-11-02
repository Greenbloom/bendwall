#!/usr/bin/env python3
"""Independent oracle for the Bend scene model.

Rules checked against the project files rather than the Bend output:
- type match is case-insensitive; a missing type is scene
- preset overlays dependency properties by name; unknown names are appended
- user overrides win over preset; existing property types are kept
- numbers and bools are their JSON source literals (0.25 stays 0.25)
- a bound value is lit, user (string or {name}), script, or animation
- object kind is the first of image/particle/model/text/sound/light
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
KINDS = ("image", "particle", "model", "text", "sound", "light")


def lit(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, int):
        return str(v)
    if isinstance(v, float):
        return format(v, ".12g")
    if v is None:
        return ""
    return str(v)


def fold_type(t):
    if t is None or t == "":
        return "scene"
    return str(t).lower()


def header(proj):
    gen = proj.get("general") or {}
    audio = lit(gen["supportsaudioprocessing"]) if "supportsaudioprocessing" in gen else ""
    return (
        "PROJ type=%s file=%s title=%s dep=%s audio=%s\n"
        % (
            fold_type(proj.get("type")),
            proj.get("file") or "",
            proj.get("title") or "",
            proj.get("dependency") or "",
            audio,
        )
    )


def load_props(proj):
    gen = proj.get("general") or {}
    out = []
    for p in gen.get("properties") or []:
        out.append({"name": p["name"], "type": p["type"], "value": lit(p["value"])})
    return out


def show_props(ps):
    return "".join(
        "PROP name=%s type=%s value=%s\n" % (p["name"], p["type"], p["value"]) for p in ps
    )


def overlay(props, overs, newtyp):
    props = [dict(p) for p in props]
    for name, raw in overs.items():
        val = lit(raw)
        hit = False
        for p in props:
            if p["name"] == name:
                p["value"] = val
                hit = True
                break
        if not hit:
            props.append({"name": name, "type": newtyp, "value": val})
    return props


def find(props, name):
    for p in props:
        if p["name"] == name:
            return p["value"]
    return ""


def bound(node, props):
    if not isinstance(node, dict):
        return "lit:" + lit(node)
    if "user" in node:
        u = node["user"]
        name = u.get("name", "") if isinstance(u, dict) else lit(u) if not isinstance(u, str) else u
        val = find(props, name)
        if val == "":
            val = lit(node.get("value", ""))
        return "user:%s:%s" % (name, val)
    if "script" in node:
        return "script:" + lit(node.get("value", ""))
    if "animation" in node:
        return "anim:" + lit(node.get("value", ""))
    return "lit:" + lit(node.get("value", ""))


def kind_payload(obj):
    for k, v in obj.items():
        if k in KINDS:
            if isinstance(v, dict):
                return k, lit(v.get("value", ""))
            return k, v if isinstance(v, str) else lit(v)
    return "", ""


def effects(obj):
    if "effects" not in obj:
        return "0:"
    xs = obj["effects"] or []
    if not xs:
        return "0:"
    return "%d:%s" % (len(xs), xs[0].get("file", ""))


def scene_text(scene, props):
    gen = scene.get("general") or {}
    op = gen.get("orthogonalprojection") or {}
    w = lit(op["width"]) if "width" in op else ""
    h = lit(op["height"]) if "height" in op else ""
    clear = gen["clearcolor"] if "clearcolor" in gen else ""
    if not isinstance(clear, str):
        clear = lit(clear)
    bloom = lit(gen["bloom"]) if "bloom" in gen else ""
    cam = ""
    camera = scene.get("camera") or {}
    if "center" in camera:
        c = camera["center"]
        cam = c if isinstance(c, str) else lit(c)
    lines = "SCN w=%s h=%s clear=%s bloom=%s cam=%s\n" % (w, h, clear, bloom, cam)
    for obj in scene.get("objects") or []:
        kind, payload = kind_payload(obj)
        alpha = bound(obj["alpha"], props) if "alpha" in obj else ""
        angles = bound(obj["angles"], props) if "angles" in obj else ""
        lines += (
            "OBJ name=%s kind=%s origin=%s size=%s parallax=%s alpha=%s angles=%s payload=%s effects=%s\n"
            % (
                obj.get("name", ""),
                kind,
                obj.get("origin", ""),
                obj.get("size", ""),
                obj.get("parallaxDepth", ""),
                alpha,
                angles,
                payload,
                effects(obj),
            )
        )
    return lines


def kv(obj):
    return ",".join("%s:%s" % (k, lit(v)) for k, v in obj.items())


def mat_text(mat):
    acc = ""
    passes = mat.get("passes") or []
    for p in passes:
        tex = ",".join("" if t is None else (t if isinstance(t, str) else lit(t)) for t in (p.get("textures") or []))
        acc += " shader=%s blend=%s combo=%s const=%s user=%s tex=%s" % (
            p.get("shader", ""),
            p.get("blending", ""),
            kv(p.get("combos") or {}),
            kv(p.get("constantshadervalues") or {}),
            kv(p.get("usershadervalues") or {}),
            tex,
        )
    return "MAT passes=%d%s\n" % (len(passes), acc)


def codes(s):
    return ",".join(str(ord(c)) for c in s)


def expect():
    base = json.loads((ROOT / "fixtures/m2/base/project.json").read_text())
    preset = json.loads((ROOT / "fixtures/m2/preset/project.json").read_text())
    user = json.loads((ROOT / "fixtures/m2/user.json").read_text())
    notype = json.loads((ROOT / "fixtures/m2/notype/project.json").read_text())
    scene = json.loads((ROOT / "fixtures/m2/base/scene.json").read_text())
    mat = json.loads((ROOT / "fixtures/m2/base/materials/bg.json").read_text())
    esc = json.loads((ROOT / "fixtures/m2/escape.json").read_text())
    props = load_props(base)
    resolved = overlay(overlay(props, preset.get("preset") or {}, "preset"), user, "user")
    pairs = "".join("PAIR %s=%s\n" % (k, lit(v)) for k, v in (preset.get("preset") or {}).items())
    return (
        "BASE\n"
        + header(base)
        + show_props(props)
        + "PRESET\n"
        + header(preset)
        + pairs
        + "RESOLVED\n"
        + show_props(resolved)
        + "NOTYPE\n"
        + header(notype)
        + "SCENE\n"
        + scene_text(scene, resolved)
        + "MAT\n"
        + mat_text(mat)
        + "BOM\n"
        + "PROJ type=scene file= title=Bom dep= audio=\n"
        + "ESC a=%s\nESC b=%s\n" % (codes(esc["a"]), codes(esc["b"]))
    )


def main():
    got = Path(sys.argv[1]).read_text() if len(sys.argv) > 1 else sys.stdin.read()
    if got.endswith("\n\n"):
        got = got[:-1]
    exp = expect()
    src = (ROOT / "fixtures/m2/base/project.json").read_text()
    if "0.25" not in src:
        print("fixture lost the 0.25 literal")
        sys.exit(1)
    if "value=0.25" not in got.split("PRESET\n", 1)[0]:
        print("BASE did not keep the source literal 0.25")
        sys.exit(1)
    if "value=0.5\n" not in got.split("RESOLVED\n", 1)[1].split("NOTYPE\n", 1)[0]:
        print("preset speed was not the source literal 0.5")
        sys.exit(1)
    if got != exp:
        print("M2 MISMATCH")
        import difflib
        for line in difflib.unified_diff(exp.splitlines(), got.splitlines(), lineterm="", n=1):
            print(line)
        sys.exit(1)
    print("M2 PASS")
    print(got, end="" if got.endswith("\n") else "\n")


if __name__ == "__main__":
    main()

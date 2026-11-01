#!/usr/bin/env python3
"""Live scene image: two video frames, text ink, tint, pulse, spectrum."""
import pathlib, re, sys

text = pathlib.Path(sys.argv[1]).read_text()

def grab(pat):
    m = re.search(pat, text)
    if not m:
        raise SystemExit("missing " + pat + "\n" + text)
    return m

v0 = [int(x) for x in grab(r"VIDEO0 (\d+) (\d+) (\d+) (\d+)").groups()]
v1 = [int(x) for x in grab(r"VIDEO1 (\d+) (\d+) (\d+) (\d+)").groups()]
if v0[0] < 180 or v0[1] > 80 or v0[2] > 80:
    raise SystemExit(f"frame0 {v0}")
if v1[1] < 180 or v1[0] > 80 or v1[2] > 80:
    raise SystemExit(f"frame1 {v1}")
if "DIFF 1" not in text:
    raise SystemExit(text)
spec = [float(x) for x in grab(r"SPECTRUM ([0-9.]+) ([0-9.]+) ([0-9.]+) ([0-9.]+)").groups()]
if spec[0] < 0.9 or spec[1] < 0.9 or spec[2] < 0.9 or spec[3] > 0.05:
    raise SystemExit(f"spectrum {spec}")
web = [float(x) for x in grab(r"WEB ([0-9.]+) ([0-9.]+)").groups()]
if web[0] < 0.9 or web[1] > 0.05:
    raise SystemExit(f"web {web}")
tone = [float(x) for x in grab(r"TONE ([0-9.]+) ([0-9.]+)").groups()]
if tone[0] < 0.8 or tone[1] < 0.8:
    raise SystemExit(f"tone {tone}")
if "uni=6" not in text:
    raise SystemExit(text)
white = int(grab(r"LABEL white=(\d+)").group(1))
if white < 8:
    raise SystemExit(f"white {white}")
pulse = [int(x) for x in grab(r"PULSE (\d+) (\d+)").groups()]
if pulse[1] < pulse[0] + 40:
    raise SystemExit(f"pulse {pulse}")
tint = [int(x) for x in grab(r"TINT (\d+) (\d+) (\d+)").groups()]
if tint[0] > 40 or not (50 <= tint[1] <= 140) or tint[2] > 40:
    raise SystemExit(f"tint {tint}")
pack = grab(r"PACK (.*)").group(1)
if "tint=1" not in pack or "pulse=1" not in pack:
    raise SystemExit(pack)
orient = grab(r"ORIENT kind=(\S+) top=(\d+) (\d+) (\d+) bot=(\d+) (\d+) (\d+)")
if orient.group(1) != "video":
    raise SystemExit(orient.group(0))
top = [int(orient.group(i)) for i in (2, 3, 4)]
bot = [int(orient.group(i)) for i in (5, 6, 7)]
if top[0] < 180 or top[1] > 80 or bot[1] < 180 or bot[0] > 80:
    raise SystemExit(orient.group(0))
orient2 = grab(r"ORIENT2 top=(\d+) (\d+) (\d+) bot=(\d+) (\d+) (\d+)")
top2 = [int(orient2.group(i)) for i in (1, 2, 3)]
bot2 = [int(orient2.group(i)) for i in (4, 5, 6)]
if top2[1] < 180 or top2[0] > 80 or bot2[0] < 180 or bot2[1] > 80:
    raise SystemExit(orient2.group(0))
tex = grab(r"TEX c=(\d+) (\d+) (\d+) m=(\d+) (\d+) (\d+) far=(\d+) (\d+) (\d+) low=(\d+) (\d+) (\d+)")
cyan = [int(tex.group(i)) for i in (1, 2, 3)]
mag = [int(tex.group(i)) for i in (4, 5, 6)]
far = [int(tex.group(i)) for i in (7, 8, 9)]
low = [int(tex.group(i)) for i in (10, 11, 12)]
if cyan[0] > 40 or cyan[1] < 200 or cyan[2] < 200:
    raise SystemExit("cyan " + str(cyan))
if mag[0] < 200 or mag[1] > 40 or mag[2] < 200:
    raise SystemExit("magenta " + str(mag))
if any(c > 20 for c in far + low):
    raise SystemExit("outside " + str(far) + " " + str(low))
dxt = [int(x) for x in grab(r"DXT (\d+) (\d+) (\d+)").groups()]
if dxt[0] > 40 or dxt[1] < 200 or dxt[2] > 40:
    raise SystemExit(f"dxt {dxt}")
png = [int(x) for x in grab(r"PNG (\d+) (\d+) (\d+)").groups()]
if png[0] < 200 or png[1] > 40 or png[2] > 40:
    raise SystemExit(f"png {png}")
miss = grab(r"MISS miss=(\d+) varied=(\d+)")
if int(miss.group(1)) < 1 or miss.group(2) != "1":
    raise SystemExit(miss.group(0))
if len(sys.argv) < 3:
    raise SystemExit("need stderr")
err = pathlib.Path(sys.argv[2]).read_text()
if "bendwall: texture not found" not in err:
    raise SystemExit(err)
if "bendwall: texture decode failed" not in err:
    raise SystemExit(err)
name = grab(r"NAME c=(\d+) (\d+) (\d+) m=(\d+) (\d+) (\d+)")
nc = [int(name.group(i)) for i in (1, 2, 3)]
nm = [int(name.group(i)) for i in (4, 5, 6)]
if nc[0] > 40 or nc[1] < 200 or nc[2] < 200 or nm[0] < 200 or nm[1] > 40 or nm[2] < 200:
    raise SystemExit(name.group(0))
big = grab(r"BIGLZ c=(\d+) (\d+) (\d+) m=(\d+) (\d+) (\d+)")
bc = [int(big.group(i)) for i in (1, 2, 3)]
bm = [int(big.group(i)) for i in (4, 5, 6)]
if bc[0] > 40 or bc[1] < 200 or bc[2] < 200 or bm[0] < 200 or bm[1] > 40 or bm[2] < 200:
    raise SystemExit(big.group(0))
dxtb = [int(x) for x in grab(r"BIGDXT (\d+) (\d+) (\d+)").groups()]
if dxtb[0] > 40 or dxtb[1] < 200 or dxtb[2] > 40:
    raise SystemExit(f"bigdxt {dxtb}")
asset = [int(x) for x in grab(r"ASSET (\d+) (\d+) (\d+)").groups()]
if asset[2] < 200 or asset[0] > 40 or asset[1] > 80:
    raise SystemExit(f"asset {asset}")
root = pathlib.Path(sys.argv[1]).resolve().parents[1]
for rel in ("fixtures/play/pkg9big/lz/scene.pkg", "fixtures/play/pkg9big/dxt/scene.pkg"):
    sz = (root / rel).stat().st_size
    if sz <= 2000000:
        raise SystemExit(f"{rel} is {sz} bytes")
magic_pkg = (root / "fixtures/play/named/scene.pkg").read_bytes()
if b"PKGV0009" not in magic_pkg[:24]:
    raise SystemExit("named package is not PKGV0009")
if "PLAYMAIN ok" not in text:
    raise SystemExit(text)
print("PLAY PASS")

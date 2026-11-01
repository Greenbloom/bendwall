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
obj = grab(r"OBJ c=(\d+) (\d+) (\d+) m=(\d+) (\d+) (\d+)")
oc = [int(obj.group(i)) for i in (1, 2, 3)]
om = [int(obj.group(i)) for i in (4, 5, 6)]
if oc[0] > 40 or oc[1] < 200 or oc[2] < 200 or om[0] < 200 or om[1] > 40 or om[2] < 200:
    raise SystemExit(obj.group(0))
mat = grab(r"MAT c=(\d+) (\d+) (\d+) m=(\d+) (\d+) (\d+)")
mc = [int(mat.group(i)) for i in (1, 2, 3)]
mm = [int(mat.group(i)) for i in (4, 5, 6)]
if mc[0] > 40 or mc[1] < 200 or mc[2] < 200 or mm[0] < 200 or mm[1] > 40 or mm[2] < 200:
    raise SystemExit(mat.group(0))
mix = grab(r"MIX c=(\d+) (\d+) (\d+) miss=(\d+)")
mx = [int(mix.group(i)) for i in (1, 2, 3)]
if mx[0] > 40 or mx[1] < 200 or mx[2] < 200 or int(mix.group(4)) < 1:
    raise SystemExit(mix.group(0))
gif0 = [int(x) for x in grab(r"GIF0 (\d+) (\d+) (\d+)").groups()]
gif1 = [int(x) for x in grab(r"GIF1 (\d+) (\d+) (\d+)").groups()]
if gif0[0] < 200 or gif0[1] > 40 or gif0[2] > 40:
    raise SystemExit(f"gif0 {gif0}")
if gif1[1] < 200 or gif1[0] > 40 or gif1[2] > 40:
    raise SystemExit(f"gif1 {gif1}")
sheet = [int(x) for x in grab(r"SHEET (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)").groups()]
if sheet[0] < 200 or sheet[1] > 40 or sheet[2] > 40:
    raise SystemExit(f"sheet0 {sheet}")
if sheet[5] < 200 or sheet[3] > 40 or sheet[4] > 80:
    raise SystemExit(f"sheet1 {sheet}")
ready = grab(r"READY mixed=(\d+) gif=(\d+) label=(\d+)")
if ready.group(1) != "0" or ready.group(2) != "1" or ready.group(3) != "1":
    raise SystemExit(ready.group(0))
if re.search(r"texture not found image$", err, re.M):
    raise SystemExit("logged the word image\n" + err)
if "models/missing-face.json" not in err:
    raise SystemExit(err)
vol = [float(x) for x in grab(r"VOL ([0-9.]+) ([0-9.]+) ([0-9.]+)").groups()]
if vol != [1.0, 1.0, 0.0]:
    raise SystemExit(f"vol {vol}")
vaud = grab(r"VAUD peak=([0-9.]+) power=([0-9.]+)")
if float(vaud.group(1)) < 0.5 or float(vaud.group(2)) < 1000:
    raise SystemExit(vaud.group(0))
for label in ("VAUD0", "VAUDUSER", "VAUDDUCK", "VAUDPROJ"):
    p = float(grab(label + r" peak=([0-9.]+)").group(1))
    if p > 0.02:
        raise SystemExit(f"{label} peak {p}")
iso = grab(r"VAUDISO band=([0-9.]+) peak=([0-9.]+)")
if float(iso.group(1)) > 0.05 or float(iso.group(2)) < 0.5:
    raise SystemExit(iso.group(0))
half = grab(r"VAUDHALF peak=([0-9.]+) ratio=([0-9.]+)")
if not (0.15 <= float(half.group(1)) <= 0.75) or not (0.12 <= float(half.group(2)) <= 0.45):
    raise SystemExit(half.group(0))
mapped = grab(r"VAUDMAP peak=([0-9.]+) ratio=([0-9.]+)")
if not (0.08 <= float(mapped.group(1)) <= 0.45) or not (0.02 <= float(mapped.group(2)) <= 0.15):
    raise SystemExit(mapped.group(0))
aac = grab(r"VAUDAAC peak=([0-9.]+) ratio=([0-9.]+)")
if float(aac.group(1)) < 0.2 or float(aac.group(2)) < 0.05:
    raise SystemExit(aac.group(0))
s0 = grab(r"SPARK0 .*parts=(\d+) alive=(\d+) vol=([0-9.]+) t=(\d+)")
s1 = grab(r"SPARK1 .*parts=(\d+) alive=(\d+) vol=([0-9.]+) t=(\d+)")
if s0.group(1) != "1" or s0.group(2) != "0" or s0.group(4) != "0":
    raise SystemExit(s0.group(0))
if s1.group(1) != "1" or s1.group(2) != "1" or s1.group(4) != "1000":
    raise SystemExit(s1.group(0))
spark = grab(r"SPARK home=(\d+) (\d+) (\d+) away=(\d+) (\d+) (\d+)")
home = [int(spark.group(i)) for i in (1, 2, 3)]
away = [int(spark.group(i)) for i in (4, 5, 6)]
if home[0] > 40 or home[1] > 40 or home[2] > 50:
    raise SystemExit("spark home " + str(home))
# One additive sprite, colour lerped a third of the way from (1, 0.47, 0.12)
# toward (1, 0.9, 0.2), on the dark plate.
if away[0] < 240 or not (140 <= away[1] <= 210) or not (40 <= away[2] <= 100):
    raise SystemExit("spark away " + str(away))
assetp = [int(x) for x in grab(r"SPARKASSET (\d+) (\d+) (\d+)").groups()]
if assetp[0] < 240 or not (140 <= assetp[1] <= 210) or not (40 <= assetp[2] <= 100):
    raise SystemExit(f"sparkasset {assetp}")
if "PLAYMAIN ok" not in text:
    raise SystemExit(text)
print("PLAY PASS")

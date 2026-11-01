#!/usr/bin/env python3
"""--watch-check loads the Plasma page. Web tries the loader; a scene does not."""
import pathlib, re, sys

kind_want = sys.argv[2]
text = pathlib.Path(sys.argv[1]).read_text()
m = re.search(r"WATCH kind=(\S*) loader=(-?\d+) page=(.*)", text)
if not m:
    raise SystemExit(text)
kind, loader, page = m.group(1), int(m.group(2)), m.group(3).strip()
if kind != kind_want:
    raise SystemExit(f"kind {kind} want {kind_want}\n" + text)
if kind_want == "web":
    if "index.html" not in page:
        raise SystemExit(text)
    if loader == 0:
        raise SystemExit("web loader never started\n" + text)
    if loader != 1:
        blob = text
        if "QtWebEngine" not in blob:
            raise SystemExit(text)
elif kind_want == "scene" or kind_want == "video":
    if loader != 0:
        raise SystemExit(text)
else:
    raise SystemExit("unknown kind " + kind_want)
print(f"WATCH PASS {kind} loader={loader}")

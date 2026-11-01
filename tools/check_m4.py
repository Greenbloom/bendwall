#!/usr/bin/env python3
"""Settings lines and workshop ids from the Qt preview."""
import pathlib, sys
args = [a for a in sys.argv[1:] if not a.startswith("--")]
text = pathlib.Path(args[0]).read_text()
lines = [ln.rstrip() for ln in text.splitlines() if ln.strip()]
if "--workshop" in sys.argv:
    want = [
        "ITEM 10", "ITEM 11", "ITEM 12", "ITEM 13", "ITEM 14",
        "ITEM 15", "ITEM 16", "ITEM 18", "ITEM 19",
        "ITEM 20", "ITEM 21", "ITEM 22", "ITEM 23", "ITEM 24", "ITEM 26",
    ]
else:
    want = [
        "color1\tcolor\t1 0 0\ttext=Color",
        "speed\tslider\t0.25\tmin=0 max=1",
        "spin\tbool\tfalse",
        "mode\tcombo\ta\toptions=a|b",
    ]
if lines != want:
    print("GOT")
    print("\n".join(lines))
    sys.exit(1)
print("M4 DUMP PASS", len(lines))

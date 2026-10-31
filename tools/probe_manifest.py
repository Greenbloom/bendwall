#!/usr/bin/env python3
"""List Workshop item directories. Bend classifies each project.json."""
import json, os, sys
root = os.path.abspath(sys.argv[1])
out = sys.argv[2]
items = []
if os.path.isdir(root):
    for name in sorted(os.listdir(root)):
        d = os.path.join(root, name)
        if os.path.isfile(os.path.join(d, "project.json")):
            items.append({"id": name, "dir": d, "root": root})
os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
with open(out, "w", encoding="utf-8") as f:
    json.dump({"items": items}, f)
print(f"manifest {len(items)}")

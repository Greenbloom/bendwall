#!/usr/bin/env python3
"""Top-level project keys. A nested type/file/title must not win."""
import pathlib, sys
text = pathlib.Path(sys.argv[1]).read_text()
if "ROOT type=video file=clip.mkv title=Night drive" not in text:
    raise SystemExit(text)
if "color" in text or "Nested" in text or "nested-not-a-video" in text:
    raise SystemExit(text)
print("ROOT PASS")

#!/usr/bin/env python3
"""Particle step: origin, speed 2, gravity (0,-1), two steps of dt 1."""
import pathlib, sys
text = pathlib.Path(sys.argv[1]).read_text()
want = "PART x=1082130432 y=3212836864 a=1056964608 n=1"
if want not in text:
    print(text)
    sys.exit("part mismatch")
print("PART PASS")

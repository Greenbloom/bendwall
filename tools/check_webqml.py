#!/usr/bin/env python3
"""The Plasma package has a web view, and the harness does not import it."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
ui = root / "kde/bendwall/contents/ui"
web = (ui / "WebLayer.qml").read_text()
main = (ui / "main.qml").read_text()
harness = (root / "kde/harness.qml").read_text()
shim = (ui / "webshim.js").read_text()
host = (root / "host/webshim.js").read_text()
blank = (ui / "blank.html").read_text()

need = [
    "import QtWebEngine",
    "WebEngineView",
    "WebEngineScript",
    "WebEngineScript.DocumentCreation",
    "WebEngineScript.MainWorld",
    "webshim.js",
    "bendwallApplyProperties",
    "bendwallPushAudio",
    "for (i = 0; i < 128; i++)",
    "wallpaperRegisterAudioListener",
]
for s in need:
    if s not in web and s not in shim and s not in blank:
        raise SystemExit("missing " + s)
if "try {" not in main or "wallpaperKind" not in main or "WebLayer.qml" not in main:
    raise SystemExit("main.qml does not switch on wallpaperKind")
if "QtWebEngine" in harness:
    raise SystemExit("harness imports QtWebEngine")
if shim != host:
    raise SystemExit("package webshim.js drifted from host/webshim.js")
if "applyUserProperties" not in shim or "wallpaperRegisterAudioListener" not in shim:
    raise SystemExit("shim api")
print("WEBQML PASS")

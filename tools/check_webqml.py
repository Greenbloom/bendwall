#!/usr/bin/env python3
"""The Plasma package has a web view, and the harness does not import it."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
ui = root / "kde/bendwall/contents/ui"
web = (ui / "WebLayer.qml").read_text()
main = (ui / "main.qml").read_text()
harness = (root / "kde/harness.qml").read_text()
watch = (root / "kde/watch.qml").read_text()
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
if "wallpaperDir" not in main or "bendwallFrame" not in main or "bendwallWeb" not in main:
    raise SystemExit("main.qml does not pin wallpaperDir")
if "bendwall/contents/ui/main.qml" not in watch:
    raise SystemExit("watch.qml does not load main.qml")
if "QtWebEngine" in harness or "QtWebEngine" in watch:
    raise SystemExit("harness or watch imports QtWebEngine")
if "WebEngineProfilePrototype" in web or "profileProto" in web:
    raise SystemExit("web profile still instantiates WebEngineProfilePrototype")
if "WebEngineProfile {" in web:
    raise SystemExit("WebEngineProfile is created directly")
if "WebEngineScript {" in web:
    raise SystemExit("WebEngineScript element is not creatable on Qt 6.11")
if "WebEngine.script()" not in web:
    raise SystemExit("shim is not built with WebEngine.script()")
script_at = web.find("WebEngine.script()")
url_at = web.find("view.url = host.pageUrl()")
if script_at < 0 or url_at < 0 or script_at > url_at:
    raise SystemExit("user script is not inserted before the url")
if "userScripts.insert" not in web or "view.userScripts" not in web:
    raise SystemExit("shim is not inserted on the view userScripts collection")
if shim != host:
    raise SystemExit("package webshim.js drifted from host/webshim.js")
if "applyUserProperties" not in shim or "wallpaperRegisterAudioListener" not in shim:
    raise SystemExit("shim api")
print("WEBQML PASS")

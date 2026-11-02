#!/bin/sh
# Remove the user-local Plasma package, the QML module, and the env script.
set -e
rm -rf "${HOME}/.local/share/plasma/wallpapers/com.philip.bendwall"
rm -rf "${HOME}/.local/lib/qt6/qml/com/philip/bendwall"
rm -f "${HOME}/.config/plasma-workspace/env/bendwall.sh"
rm -f "${HOME}/.local/bin/bendwall-picker"
rm -f "${HOME}/.local/share/applications/bendwall-picker.desktop"
rm -f "${HOME}/.local/share/icons/hicolor/scalable/apps/bendwall.svg"
echo "Removed Bendwall."
echo "If it was the current wallpaper, pick another one, then log out and back in."

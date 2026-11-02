#!/bin/sh
# User-local Plasma 6 wallpaper. Log out and back in afterwards.
set -e
cd "$(dirname "$0")/.."
scripts/lib.sh
CXX=c++
if ! echo 'int main(){return 0;}' | c++ -x c++ -o /tmp/bendwall-cxx - >/dev/null 2>&1; then
  if echo 'int main(){return 0;}' | g++ -x c++ -o /tmp/bendwall-cxx - >/dev/null 2>&1; then
    CXX=g++
  else
    CXX=g++-13
  fi
fi
cmake -S kde -B build/kde -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX"
cmake --build build/kde -j2

wp="${HOME}/.local/share/plasma/wallpapers/com.philip.bendwall"
qml="${HOME}/.local/lib/qt6/qml/com/philip/bendwall"
rm -rf "$wp"
mkdir -p "$wp" "$qml" "${HOME}/.config/plasma-workspace/env"
cp -a kde/bendwall/. "$wp/"
cp build/kde/com/philip/bendwall/libbendwallplugin.so "$qml/"
cp kde/module/qmldir "$qml/"
cp host/libbendwall.so "$qml/"
bindir="${HOME}/.local/bin"
mkdir -p "$bindir" "${HOME}/.local/share/applications" "${HOME}/.local/share/icons/hicolor/scalable/apps"
cp build/kde/bendwall-picker "$bindir/bendwall-picker"
chmod 755 "$bindir/bendwall-picker"
cp kde/bendwall.svg "${HOME}/.local/share/icons/hicolor/scalable/apps/bendwall.svg"
sed "s|^Exec=.*|Exec=${bindir}/bendwall-picker|; s|^Icon=.*|Icon=${HOME}/.local/share/icons/hicolor/scalable/apps/bendwall.svg|" \
  kde/bendwall-picker.desktop > "${HOME}/.local/share/applications/bendwall-picker.desktop"
cat > "${HOME}/.config/plasma-workspace/env/bendwall.sh" << 'EOF'
export QML_IMPORT_PATH="$HOME/.local/lib/qt6/qml${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}"
export QML2_IMPORT_PATH="$QML_IMPORT_PATH"
export BENDWALL_NATIVE="$HOME/.local/lib/qt6/qml/com/philip/bendwall/libbendwall.so"
export BENDWALL_ASSETS="${BENDWALL_ASSETS:-$HOME/.local/share/Steam/steamapps/common/wallpaper_engine/assets}"
export BENDWALL_WORKSHOP="${BENDWALL_WORKSHOP:-$HOME/.local/share/Steam/steamapps/workshop/content/431960}"
EOF
echo "Installed Bendwall."
echo "The picker is ${bindir}/bendwall-picker."
echo "Log out and back in, then choose Bendwall as the desktop wallpaper."

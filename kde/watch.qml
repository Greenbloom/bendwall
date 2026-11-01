import QtQuick
import QtQuick.Window

/* Same wallpaper page Plasma loads. wallpaperDir and wallpaperKind are
   set by bendwall-preview; Plasma leaves them unset. */
Window {
    id: win
    width: 960
    height: 540
    visible: true
    title: "Bendwall"
    color: "#121418"

    Loader {
        anchors.fill: parent
        source: Qt.resolvedUrl("bendwall/contents/ui/main.qml")
    }
}

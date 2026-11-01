import QtQuick
import com.philip.bendwall

Item {
    id: root
    anchors.fill: parent

    function wallpaperIsWeb() {
        try {
            return wallpaperKind === "web"
        } catch (e) {
            return false
        }
    }

    Frame {
        id: frame
        anchors.fill: parent
        visible: !root.wallpaperIsWeb()
        framePath: wallpaperFrame
    }

    Loader {
        anchors.fill: parent
        active: root.wallpaperIsWeb()
        source: "WebLayer.qml"
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        enabled: !root.wallpaperIsWeb()
        onPositionChanged: function (mouse) {
            frame.pointerX = mouse.x
            frame.pointerY = mouse.y
        }
    }
}

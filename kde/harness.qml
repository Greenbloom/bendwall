import QtQuick
import QtQuick.Controls
import com.philip.bendwall

Window {
    id: win
    width: 520
    height: 280
    visible: true
    color: "#121418"
    title: "Bendwall"

    Row {
        anchors.fill: parent

        Frame {
            id: frame
            width: 240
            height: 280
            framePath: wallpaperFrame
        }

        Loader {
            width: 280
            height: 280
            source: Qt.resolvedUrl("bendwall/contents/ui/config.qml")
        }
    }

    MouseArea {
        x: 0
        y: 0
        width: 240
        height: 280
        hoverEnabled: true
        onPositionChanged: function (mouse) {
            frame.pointerX = mouse.x
            frame.pointerY = mouse.y
        }
    }
}

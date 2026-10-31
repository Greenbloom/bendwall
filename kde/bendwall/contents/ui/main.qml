import QtQuick
import com.philip.bendwall

Item {
    id: root
    anchors.fill: parent

    Frame {
        id: frame
        anchors.fill: parent
        framePath: wallpaperFrame
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPositionChanged: function (mouse) {
            frame.pointerX = mouse.x
            frame.pointerY = mouse.y
        }
    }
}

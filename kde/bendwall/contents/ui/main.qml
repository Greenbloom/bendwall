import QtQuick
import com.philip.bendwall

Item {
    id: root
    anchors.fill: parent

    function wallpaperIsWeb() {
        try {
            if (wallpaperKind === "web")
                return true
        } catch (e) {
        }
        return frame.kind === "web"
    }

    function currentId() {
        try {
            var configured = wallpaper.configuration.Item
            if (configured && String(configured).length > 0)
                return String(configured)
        } catch (e) {
        }
        try {
            if (wallpaperItem && String(wallpaperItem).length > 0)
                return String(wallpaperItem)
        } catch (e) {
        }
        var ids = Catalog.ids
        return ids.length ? ids[0] : ""
    }

    function refresh() {
        try {
            if (wallpaperDir && String(wallpaperDir).length > 0) {
                frame.mediaDir = String(wallpaperDir)
                return
            }
        } catch (e) {
        }
        var id = currentId()
        if (id !== frame.itemId)
            frame.itemId = id
    }

    Frame {
        id: frame
        objectName: "bendwallFrame"
        anchors.fill: parent
        live: true
        videoVolume: {
            try {
                var v = wallpaper.configuration.VideoVolume
                if (v === undefined || v === null || v === "")
                    return 0
                var n = Number(v)
                if (isNaN(n))
                    return 0
                return n / 100
            } catch (e) {
                return 0
            }
        }
        visible: !root.wallpaperIsWeb() || webLoader.status !== Loader.Ready
        framePath: {
            try {
                return wallpaperFrame
            } catch (e) {
                return ""
            }
        }
    }

    Loader {
        id: webLoader
        objectName: "bendwallWeb"
        anchors.fill: parent
        active: root.wallpaperIsWeb()
        source: "WebLayer.qml"
        onLoaded: {
            item.liveAudio = Qt.binding(function () { return frame.audioSamples })
            item.livePage = Qt.binding(function () { return frame.pageUrl })
        }
    }

    Rectangle {
        visible: root.wallpaperIsWeb() && !frame.visible
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        width: webBadge.implicitWidth + 16
        height: webBadge.implicitHeight + 8
        radius: 4
        color: "#a0000000"
        Text {
            id: webBadge
            anchors.centerIn: parent
            text: frame.statsText
            color: "#e8e6e3"
            font.pixelSize: 13
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        enabled: frame.visible
        onPositionChanged: function (mouse) {
            frame.pointerX = mouse.x
            frame.pointerY = mouse.y
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: root.refresh()
    }

    Component.onCompleted: root.refresh()
}

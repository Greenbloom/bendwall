import QtQuick
import QtWebEngine

Item {
    property string pageUrl

    WebEngineView {
        anchors.fill: parent
        url: pageUrl === "" ? "" : ("file:" + pageUrl)
        audioMuted: true
        backgroundColor: "#1a1b22"
    }
}

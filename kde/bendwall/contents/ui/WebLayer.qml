import QtQuick
import QtWebEngine

/* Web wallpapers. QtWebEngine is loaded only when main.qml activates this
   file, so image wallpapers and the offscreen harness do not need it.
   The shim is the page-side API from REA §12. The host pushes user
   properties and a 128-float spectrum (left 0..63, right 64..127). */
Item {
    id: host
    anchors.fill: parent

    function pageUrl() {
        try {
            if (wallpaperWebUrl && String(wallpaperWebUrl).length > 0)
                return wallpaperWebUrl
        } catch (e) {
        }
        return Qt.resolvedUrl("blank.html")
    }

    function userProps() {
        try {
            if (wallpaperPropsJson && String(wallpaperPropsJson).length > 0)
                return JSON.parse(wallpaperPropsJson)
        } catch (e) {
        }
        return {}
    }

    function samples() {
        var src = null
        try {
            src = wallpaperAudio
        } catch (e) {
            src = null
        }
        var out = []
        var i
        for (i = 0; i < 128; i++) {
            var v = 0
            if (src && i < src.length)
                v = src[i]
            out.push(v)
        }
        return out
    }

    function pushAll() {
        if (!pageReady)
            return
        view.runJavaScript("bendwallApplyProperties(" + JSON.stringify(userProps()) + ");")
        view.runJavaScript("bendwallPushAudio(" + JSON.stringify(samples()) + ");")
    }

    property bool pageReady: false

    WebEngineProfile {
        id: profile
        storageName: "bendwall"
        offTheRecord: true
        userScripts: [
            WebEngineScript {
                name: "bendwall-host"
                worldId: WebEngineScript.MainWorld
                injectionPoint: WebEngineScript.DocumentCreation
                sourceUrl: Qt.resolvedUrl("webshim.js")
            }
        ]
    }

    WebEngineView {
        id: view
        anchors.fill: parent
        profile: profile
        url: host.pageUrl()
        onLoadingChanged: function (load) {
            if (load.status === WebEngineView.LoadSucceededStatus) {
                host.pageReady = true
                host.pushAll()
            }
        }
    }

    Timer {
        interval: 33
        repeat: true
        running: host.pageReady
        onTriggered: host.pushAll()
    }
}

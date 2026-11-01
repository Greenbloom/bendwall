import QtQuick
import QtWebEngine

/* Web wallpapers. QtWebEngine is loaded only when main.qml activates this
   file, so image wallpapers and the offscreen harness do not need it.
   The shim is the page-side API from REA §12. The host pushes user
   properties and a 128-float spectrum (left 0..63, right 64..127). */
Item {
    id: host
    anchors.fill: parent

    property var liveAudio: []
    property string livePage: ""
    onLivePageChanged: view.url = pageUrl()

    function pageUrl() {
        try {
            if (wallpaperWebUrl && String(wallpaperWebUrl).length > 0)
                return wallpaperWebUrl
        } catch (e) {
        }
        if (livePage && livePage.length > 0)
            return livePage
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
        var src = liveAudio
        try {
            if (wallpaperAudio && wallpaperAudio.length)
                src = wallpaperAudio
        } catch (e) {
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

    WebEngineScript {
        id: shim
        name: "bendwall-host"
        worldId: WebEngineScript.MainWorld
        injectionPoint: WebEngineScript.DocumentCreation
        sourceUrl: Qt.resolvedUrl("webshim.js")
    }

    /* Qt 6.11 will not create a WebEngineProfile element from QML.
       WebEngineProfilePrototype (6.9+) does. Leaving storageName unset
       makes the instance off-the-record. The shim is attached to that
       instance, which is where userScripts live. */
    WebEngineProfilePrototype {
        id: profileProto
        Component.onCompleted: {
            if (instance)
                instance.userScripts.insert(shim)
        }
    }

    WebEngineView {
        id: view
        anchors.fill: parent
        profile: profileProto.instance
        settings.localContentCanAccessFileUrls: true
        settings.localContentCanAccessRemoteUrls: true
        Component.onCompleted: url = host.pageUrl()
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

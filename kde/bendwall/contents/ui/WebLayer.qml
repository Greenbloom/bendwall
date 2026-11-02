import QtQuick
import QtWebEngine

/* Web wallpapers. QtWebEngine is loaded only when main.qml activates this
   file, so image wallpapers and the offscreen harness do not need it.
   The shim is the page-side wallpaper API. The host pushes user
   properties and a 128-float spectrum (left 0..63, right 64..127). */
Item {
    id: host
    anchors.fill: parent

    property var liveAudio: []
    property string livePage: ""
    onLivePageChanged: if (shimReady) view.url = pageUrl()

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
    property bool shimReady: false

    /* WebEngineScript is a value type on Qt 6.11, so it is not a QML
       element. WebEngine.script() builds the value, and the view's
       userScripts collection takes it before the first url. */
    function attachShim() {
        var script = WebEngine.script()
        script.name = "bendwall-host"
        script.worldId = WebEngineScript.MainWorld
        script.injectionPoint = WebEngineScript.DocumentCreation
        script.sourceUrl = Qt.resolvedUrl("webshim.js")
        if (view.userScripts)
            view.userScripts.insert(script)
        else if (view.profile && view.profile.userScripts)
            view.profile.userScripts.insert(script)
        host.shimReady = true
        view.url = host.pageUrl()
    }

    WebEngineView {
        id: view
        anchors.fill: parent
        settings.localContentCanAccessFileUrls: true
        settings.localContentCanAccessRemoteUrls: true
        Component.onCompleted: host.attachShim()
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

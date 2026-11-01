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

    /* Qt 6.11.3 reports a profile element as not creatable, and older
       Qt has no separate profile type we can build here. The view's
       default profile accepts userScripts on every version we run,
       including 6.11. bendwallQtVersion is the runtime Qt string. */
    function runtimeQt() {
        var major = 0
        var minor = 0
        try {
            if (typeof bendwallQtVersion === "string") {
                var parts = bendwallQtVersion.split(".")
                major = parseInt(parts[0])
                minor = parseInt(parts[1])
            }
        } catch (e) {
        }
        if (isNaN(major))
            major = 0
        if (isNaN(minor))
            minor = 0
        return { major: major, minor: minor }
    }

    function attachShim() {
        var q = runtimeQt()
        var profile = view.profile
        /* 6.11 and anything older, and a missing version, stay on the
           default profile. A newer runtime uses that same profile: an
           explicit profile element is not creatable on 6.11. */
        if (q.major < 6 || (q.major === 6 && q.minor <= 11) || q.major === 0)
            profile = view.profile
        else
            profile = view.profile
        if (profile)
            profile.userScripts.insert(shim)
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

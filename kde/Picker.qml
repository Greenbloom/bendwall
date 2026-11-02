import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BendwallPicker 1.0

ApplicationWindow {
    id: win
    objectName: "pickerRoot"
    width: 1120
    height: 740
    minimumWidth: 880
    minimumHeight: 560
    visible: true
    title: "Bendwall"
    color: "#121318"

    property string currentId: ""
    property string currentTitle: ""
    property string currentKind: ""
    property string currentDir: ""
    property string currentPreview: ""
    property string currentNote: ""
    property bool currentGif: false
    property bool currentLive: false
    property bool liveGaveUp: false

    function selectMap(row) {
        currentId = row.id
        currentTitle = row.title
        currentKind = row.kind
        currentDir = row.dir
        currentPreview = row.preview
        currentNote = row.note
        currentGif = row.gif
        currentLive = row.liveOk
        liveGaveUp = false
        if (currentLive)
            liveTimer.restart()
        else
            liveTimer.stop()
    }

    function selectRow(index) {
        var item = wallpaperGrid.itemAtIndex(index)
        if (!item)
            return
        selectMap({
            id: item.itemId,
            title: item.cardTitle,
            kind: item.cardKind,
            dir: item.cardDir,
            preview: item.cardPreview,
            note: item.cardNote,
            gif: item.cardGif,
            liveOk: item.cardLive
        })
    }

    function missCount(line) {
        if (!line)
            return 0
        var match = /miss=(\d+)/.exec(line)
        if (!match)
            return 0
        return parseInt(match[1], 10)
    }

    Timer {
        id: liveTimer
        interval: 1500
        repeat: false
        onTriggered: {
            if (liveFrame.steps < 1)
                liveGaveUp = true
        }
    }

    Connections {
        target: library
        function onShownChanged() {
            if (win.currentId === "" && library.shown > 0)
                Qt.callLater(function () { win.selectRow(0) })
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#121318"
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 16

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 22
            color: Qt.rgba(1, 1, 1, 0.045)
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    ColumnLayout {
                        spacing: 2
                        Text {
                            text: "Bendwall"
                            color: "#eceaf4"
                            font.pixelSize: 22
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: library.statusLine
                            color: "#9a96a8"
                            font.pixelSize: 12
                        }
                    }
                    Item { Layout.fillWidth: true }
                    TextField {
                        id: search
                        objectName: "searchField"
                        Layout.preferredWidth: 260
                        Layout.preferredHeight: 36
                        placeholderText: "Search titles and tags"
                        color: "#eceaf4"
                        placeholderTextColor: "#7c7a88"
                        leftPadding: 16
                        rightPadding: 16
                        font.pixelSize: 13
                        background: Rectangle {
                            radius: height / 2
                            color: "#1a1b22"
                            border.color: search.activeFocus ? "#c4b5fd" : "#34363f"
                            border.width: 1
                        }
                        onTextChanged: library.query = text
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: [
                            { label: "All", value: "" },
                            { label: "Scene", value: "scene" },
                            { label: "Video", value: "video" },
                            { label: "Web", value: "web" },
                            { label: "Other", value: "other" }
                        ]
                        delegate: Rectangle {
                            required property var modelData
                            Layout.preferredHeight: 32
                            Layout.preferredWidth: pill.implicitWidth + 28
                            radius: height / 2
                            color: library.typeFilter === modelData.value ? "#c4b5fd" : "#1a1b22"
                            border.color: library.typeFilter === modelData.value ? "#c4b5fd" : "#34363f"
                            Text {
                                id: pill
                                anchors.centerIn: parent
                                text: modelData.label
                                color: library.typeFilter === modelData.value ? "#1a1028" : "#d8d4e4"
                                font.pixelSize: 13
                                font.weight: Font.Medium
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: library.typeFilter = modelData.value
                            }
                        }
                    }
                    Item { Layout.fillWidth: true }
                    ComboBox {
                        id: tagBox
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 32
                        model: ["Any tag"].concat(library.tags)
                        font.pixelSize: 13
                        contentItem: Text {
                            text: tagBox.displayText
                            color: "#eceaf4"
                            font.pixelSize: 13
                            leftPadding: 12
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        background: Rectangle {
                            radius: height / 2
                            color: "#1a1b22"
                            border.color: "#34363f"
                        }
                        onActivated: library.tagFilter = index === 0 ? "" : currentText
                    }
                }

                GridView {
                    id: wallpaperGrid
                    objectName: "wallpaperGrid"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    cellWidth: 214
                    cellHeight: 168
                    boundsBehavior: Flickable.StopAtBounds
                    model: library

                    delegate: Item {
                        id: card
                        width: wallpaperGrid.cellWidth
                        height: wallpaperGrid.cellHeight
                        required property string itemId
                        required property string title
                        required property string kind
                        required property string dir
                        required property string preview
                        required property string thumb
                        required property string note
                        required property string tags
                        required property bool gif
                        required property bool liveOk
                        required property string activeText
                        property string cardTitle: title
                        property string cardKind: kind
                        property string cardDir: dir
                        property string cardPreview: preview
                        property string cardNote: note
                        property bool cardGif: gif
                        property bool cardLive: liveOk
                        property bool hovered: false

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 6
                            radius: 18
                            color: win.currentId === card.itemId ? Qt.rgba(0.77, 0.71, 0.99, 0.16) : Qt.rgba(1, 1, 1, 0.04)
                            border.width: win.currentId === card.itemId ? 2 : 1
                            border.color: win.currentId === card.itemId ? "#c4b5fd" : Qt.rgba(1, 1, 1, 0.08)
                            scale: card.hovered ? 1.02 : 1
                            Behavior on scale {
                                NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: caption.top
                                anchors.margins: 8
                                anchors.bottomMargin: 0
                                radius: 12
                                color: "#1a1b22"
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    source: card.thumb ? ("file:" + card.thumb) : ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    visible: source !== "" && !(card.hovered && card.gif)
                                }
                                AnimatedImage {
                                    anchors.fill: parent
                                    source: card.hovered && card.gif && card.preview ? ("file:" + card.preview) : ""
                                    playing: card.hovered && card.gif
                                    fillMode: Image.PreserveAspectCrop
                                    visible: card.hovered && card.gif && card.preview !== ""
                                }
                                Text {
                                    anchors.centerIn: parent
                                    width: parent.width - 16
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
                                    visible: !card.thumb
                                    text: card.kind
                                    color: "#6e6a78"
                                    font.pixelSize: 13
                                }
                            }

                            RowLayout {
                                id: caption
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 10
                                height: 36
                                spacing: 6
                                Text {
                                    Layout.fillWidth: true
                                    text: card.title
                                    color: "#eceaf4"
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }
                                Rectangle {
                                    Layout.preferredHeight: 20
                                    Layout.preferredWidth: kindText.implicitWidth + 12
                                    radius: height / 2
                                    color: card.kind === "video" ? "#3b2433" : (card.kind === "web" ? "#1d3330" : (card.kind === "scene" ? "#26243b" : "#2a2a30"))
                                    Text {
                                        id: kindText
                                        anchors.centerIn: parent
                                        text: card.kind
                                        color: "#e4def8"
                                        font.pixelSize: 10
                                    }
                                }
                            }

                            Rectangle {
                                visible: card.activeText !== ""
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.margins: 14
                                radius: height / 2
                                height: 20
                                width: activeLabel.implicitWidth + 12
                                color: "#c4b5fd"
                                Text {
                                    id: activeLabel
                                    anchors.centerIn: parent
                                    text: card.activeText
                                    color: "#1a1028"
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onContainsMouseChanged: card.hovered = containsMouse
                                onClicked: win.selectMap({
                                    id: card.itemId,
                                    title: card.title,
                                    kind: card.kind,
                                    dir: card.dir,
                                    preview: card.preview,
                                    note: card.note,
                                    gif: card.gif,
                                    liveOk: card.liveOk
                                })
                            }
                        }
                    }
                }

                Text {
                    visible: !library.scanning && library.shown === 0
                    text: library.total === 0
                          ? "No wallpapers in the Steam workshop folders."
                          : "Nothing matches this search."
                    color: "#9a96a8"
                    font.pixelSize: 13
                }
            }
        }

        Rectangle {
            id: livePane
            objectName: "livePane"
            Layout.preferredWidth: 360
            Layout.fillHeight: true
            radius: 22
            color: Qt.rgba(1, 1, 1, 0.045)
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 200
                    radius: 16
                    color: "#1a1b22"
                    clip: true

                    Image {
                        anchors.fill: parent
                        source: win.currentPreview ? ("file:" + win.currentPreview) : ""
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: false
                        opacity: liveFrame.steps > 0 && win.currentLive && !win.liveGaveUp ? 0 : 1
                        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    }
                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 24
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        visible: win.currentId !== "" && !win.currentPreview && liveFrame.steps < 1
                        text: win.currentKind
                        color: "#6e6a78"
                    }
                    Frame {
                        id: liveFrame
                        anchors.fill: parent
                        live: win.currentLive && !win.liveGaveUp
                        videoMuted: true
                        videoVolume: 0
                        mediaDir: win.currentLive ? win.currentDir : ""
                        opacity: win.currentLive && !win.liveGaveUp && steps > 0 ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    }
                }

                Text {
                    text: win.currentTitle === "" ? "Choose a wallpaper" : win.currentTitle
                    color: "#eceaf4"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: "#b7b3c4"
                    font.pixelSize: 12
                    text: {
                        if (win.currentId === "")
                            return "The grid lists items from your Steam library folders for app 431960."
                        if (!win.currentLive)
                            return win.currentNote !== "" ? win.currentNote : "Showing the preview image."
                        var missing = win.missCount(liveFrame.playerLine)
                        if (missing > 0)
                            return "Partial. " + missing + (missing === 1 ? " texture did not load. " : " textures did not load. ") + "The pane shows only what Bendwall could draw."
                        if (win.liveGaveUp)
                            return "Live preview did not start. Showing the preview image."
                              + (liveFrame.statsText ? " " + liveFrame.statsText : "")
                        if (liveFrame.steps > 0)
                            return "Live preview, muted."
                        return "Starting the live preview. Sound stays off."
                    }
                }

                ComboBox {
                    id: screenBox
                    objectName: "screenBox"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    model: shell.screenChoices
                    font.pixelSize: 13
                    contentItem: Text {
                        text: screenBox.displayText
                        color: "#eceaf4"
                        font.pixelSize: 13
                        leftPadding: 14
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: height / 2
                        color: "#1a1b22"
                        border.color: "#34363f"
                    }
                    onActivated: shell.loadEditor(shell.screenChoiceValue(currentIndex))
                }

                CheckBox {
                    id: muteBox
                    objectName: "muteBox"
                    text: "Mute video"
                    checked: shell.mute
                    onToggled: shell.mute = checked
                    contentItem: Text {
                        text: muteBox.text
                        color: "#eceaf4"
                        font.pixelSize: 13
                        leftPadding: muteBox.indicator.width + 8
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Volume"
                        color: "#9a96a8"
                        font.pixelSize: 12
                    }
                    Text {
                        text: String(Math.round(volumeSlider.value))
                        color: "#eceaf4"
                        font.pixelSize: 12
                    }
                }
                Slider {
                    id: volumeSlider
                    objectName: "volumeSlider"
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    stepSize: 1
                    value: shell.volume
                    onMoved: shell.volume = Math.round(value)
                    background: Rectangle {
                        x: volumeSlider.leftPadding
                        y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                        width: volumeSlider.availableWidth
                        height: 4
                        radius: 2
                        color: "#2c2e38"
                        Rectangle {
                            width: volumeSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 2
                            color: "#c4b5fd"
                        }
                    }
                    handle: Rectangle {
                        x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                        y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                        width: 16
                        height: 16
                        radius: 8
                        color: "#c4b5fd"
                    }
                }

                CheckBox {
                    id: autoMuteBox
                    objectName: "autoMuteBox"
                    text: "Mute when other apps play audio or go fullscreen"
                    checked: shell.autoMute
                    onToggled: shell.autoMute = checked
                    Layout.fillWidth: true
                    contentItem: Text {
                        text: autoMuteBox.text
                        color: "#eceaf4"
                        font.pixelSize: 13
                        wrapMode: Text.Wrap
                        width: autoMuteBox.width - autoMuteBox.indicator.width - 8
                        leftPadding: autoMuteBox.indicator.width + 8
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: "When this is on, the track also mutes while another app is fullscreen or playing audio."
                    color: "#7c7a88"
                    font.pixelSize: 11
                }

                Button {
                    id: applyButton
                    objectName: "applyButton"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    text: "Apply"
                    enabled: win.currentId !== ""
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    onClicked: {
                        var screen = shell.screenChoiceValue(screenBox.currentIndex)
                        if (shell.apply(win.currentId, screen))
                            applyNote.text = "Applied " + win.currentTitle + "."
                        else
                            applyNote.text = shell.error
                    }
                    contentItem: Text {
                        text: applyButton.text
                        color: applyButton.enabled ? "#1a1028" : "#6e6a78"
                        font: applyButton.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: height / 2
                        color: !applyButton.enabled ? "#2a2c36" : (applyButton.down ? "#a78bfa" : "#c4b5fd")
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }
                }
                Text {
                    id: applyNote
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: shell.error ? "#f0abfc" : "#9a96a8"
                    font.pixelSize: 12
                    text: shell.error ? shell.error : shell.status
                }
            }
        }
    }
}

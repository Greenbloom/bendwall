import QtQuick
import QtQuick.Controls
import com.philip.bendwall

Column {
    id: column
    spacing: 8
    width: 260
    padding: 12

    property string cfg_Item: ""
    property int cfg_VideoVolume: 100
    property bool cfg_VideoMute: true
    property bool cfg_AutoMuteOthers: false

    function idList() {
        try {
            if (typeof workshopIds !== "undefined" && workshopIds && workshopIds.length > 0)
                return workshopIds
        } catch (e) {
        }
        return Catalog.ids
    }

    function applyCfg() {
        if (!cfg_Item || !cfg_Item.length)
            return
        for (var i = 0; i < picker.count; i++) {
            if (picker.textAt(i) === cfg_Item) {
                picker.currentIndex = i
                return
            }
        }
    }

    Text {
        text: "Wallpaper"
        color: "#e8e6e3"
        font.pixelSize: 13
    }

    Text {
        text: "Compatible with Wallpaper Engine wallpapers. Wallpaper Engine, Steam, and Workshop are trademarks of their respective owners. Bendwall is not affiliated with or endorsed by those owners."
        color: "#9a958c"
        font.pixelSize: 11
        width: 236
        wrapMode: Text.Wrap
    }

    ComboBox {
        id: picker
        model: column.idList()
        implicitWidth: 236
        Component.onCompleted: column.applyCfg()
        onActivated: column.cfg_Item = currentText
    }

    Text {
        text: "Properties"
        color: "#e8e6e3"
        font.pixelSize: 13
    }

    Repeater {
        model: {
            try {
                if (typeof propLines !== "undefined" && propLines)
                    return propLines
            } catch (e) {
            }
            return []
        }
        delegate: Text {
            required property string modelData
            text: modelData
            color: "#c8c4bc"
            font.pixelSize: 12
            width: 236
            wrapMode: Text.Wrap
        }
    }

    Text {
        text: "Video volume"
        color: "#e8e6e3"
        font.pixelSize: 13
    }

    Text {
        text: "Off until Mute video is unticked. 100 is then the video's own level, and 0 mutes it as well."
        color: "#9a958c"
        font.pixelSize: 11
        width: 236
        wrapMode: Text.Wrap
    }

    Slider {
        id: volume
        from: 0
        to: 100
        stepSize: 1
        implicitWidth: 236
        value: column.cfg_VideoVolume
        onMoved: column.cfg_VideoVolume = Math.round(value)
    }

    Text {
        text: Math.round(volume.value) + " / 100"
        color: "#c8c4bc"
        font.pixelSize: 12
    }

    CheckBox {
        id: muteBox
        text: "Mute video"
        checked: column.cfg_VideoMute
        onClicked: column.cfg_VideoMute = checked
    }

    CheckBox {
        id: autoMuteBox
        text: "Mute when other apps play audio or go fullscreen"
        checked: column.cfg_AutoMuteOthers
        onClicked: column.cfg_AutoMuteOthers = checked
    }

    Text {
        text: "When this is on, the track also mutes while another app is fullscreen or playing audio."
        color: "#9a958c"
        font.pixelSize: 11
        width: 236
        wrapMode: Text.Wrap
    }

    onCfg_ItemChanged: applyCfg()
    onCfg_VideoVolumeChanged: {
        if (Math.round(volume.value) !== cfg_VideoVolume)
            volume.value = cfg_VideoVolume
    }
    onCfg_VideoMuteChanged: muteBox.checked = cfg_VideoMute
    onCfg_AutoMuteOthersChanged: autoMuteBox.checked = cfg_AutoMuteOthers
}

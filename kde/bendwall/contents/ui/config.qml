import QtQuick
import QtQuick.Controls
import com.philip.bendwall

Column {
    id: column
    spacing: 8
    width: 260
    padding: 12

    property string cfg_Item: ""

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
        text: "Workshop"
        color: "#e8e6e3"
        font.pixelSize: 13
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

    onCfg_ItemChanged: applyCfg()
}

import QtQuick
import QtQuick.Controls

Column {
    id: column
    spacing: 8
    width: 260
    padding: 12

    Text {
        text: "Workshop"
        color: "#e8e6e3"
        font.pixelSize: 13
    }

    ComboBox {
        id: picker
        model: workshopIds
        implicitWidth: 236
    }

    Text {
        text: "Properties"
        color: "#e8e6e3"
        font.pixelSize: 13
    }

    Repeater {
        model: propLines
        delegate: Text {
            required property string modelData
            text: modelData
            color: "#c8c4bc"
            font.pixelSize: 12
            width: 236
            wrapMode: Text.Wrap
        }
    }
}

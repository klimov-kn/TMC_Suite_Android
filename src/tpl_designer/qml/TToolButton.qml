// Кнопка инструмента: значок и подпись, не меньше 48×48 dp. Выбранная — фон accent.
import QtQuick
import TamicTplTheme

Rectangle {
    id: tb

    property string icon: "select"
    property string text: ""
    property bool checked: false
    signal clicked()

    implicitWidth: Theme.touchMin
    implicitHeight: text.length > 0 ? Theme.touchMin + 12 : Theme.touchMin
    radius: Theme.radiusTool
    color: checked ? Theme.accent : (area.pressed ? Theme.surface2 : "transparent")
    opacity: enabled ? 1.0 : 0.4

    Column {
        anchors.centerIn: parent
        spacing: 2
        TIcon {
            anchors.horizontalCenter: parent.horizontalCenter
            kind: tb.icon
            color: tb.checked ? Theme.textOnAccent : Theme.text
        }
        Text {
            visible: tb.text.length > 0
            anchors.horizontalCenter: parent.horizontalCenter
            text: tb.text
            color: tb.checked ? Theme.textOnAccent : Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeCaption
            font.weight: tb.checked ? Font.DemiBold : Font.Normal
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        onClicked: tb.clicked()
    }
}

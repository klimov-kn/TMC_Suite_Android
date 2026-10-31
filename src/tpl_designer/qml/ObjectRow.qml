// Строка списка объектов: цветная метка материала, имя и описание. Выбранная —
// фон line, имя accent.
import QtQuick
import TamicTplTheme

Rectangle {
    id: row

    property string title: ""
    property string subtitle: ""
    property color swatch: Theme.text
    property bool selected: false
    signal clicked()

    width: parent ? parent.width : 300
    height: Math.max(Theme.touchMin + 8, texts.implicitHeight + 16)
    radius: Theme.radiusSm
    color: selected ? Theme.line : area.pressed ? Theme.surface2 : Theme.bg

    Rectangle {
        id: mark
        anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
        width: 12
        height: 12
        radius: 3
        color: row.swatch
    }
    Column {
        id: texts
        anchors {
            left: mark.right; leftMargin: 12
            right: parent.right; rightMargin: 12
            verticalCenter: parent.verticalCenter
        }
        Text {
            width: parent.width
            text: row.title
            color: row.selected ? Theme.accent : Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeBody
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: row.subtitle
            color: Theme.textMute
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.Wrap
        }
    }
    MouseArea {
        id: area
        anchors.fill: parent
        onClicked: row.clicked()
    }
}

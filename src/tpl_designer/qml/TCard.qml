// Карточка: фон surface, рамка line, скругление 14;
// необязательный заголовок группы — цвет accent, как QGroupBox у TPL Designer.
import QtQuick
import TamicTplTheme

Rectangle {
    id: card

    default property alias content: column.data
    property string title: ""
    property int padding: Theme.gutter
    property alias spacing: column.spacing

    width: parent ? parent.width : 300
    color: Theme.surface
    radius: Theme.radius
    border.width: Theme.borderW
    border.color: Theme.line
    implicitHeight: column.implicitHeight + padding * 2
    height: implicitHeight

    Column {
        id: column
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: card.padding
        }
        spacing: Theme.spacing

        Text {
            visible: card.title.length > 0
            width: parent.width
            text: card.title
            color: Theme.accent
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.Bold
            wrapMode: Text.Wrap
        }
    }
}

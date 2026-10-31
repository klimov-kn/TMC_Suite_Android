// Карточка: фон surface, рамка line, скругление 14.
// Содержимое кладётся в колонку с отступом padding.
// https://www.tamic.ru/docs/platforms/android.html#app
import QtQuick

Rectangle {
    id: card

    default property alias content: column.data
    property int padding: Theme.gutter
    property alias spacing: column.spacing
    property bool highlighted: false

    color: highlighted ? Theme.surface2 : Theme.surface
    radius: Theme.radius
    border.width: Theme.borderW
    border.color: Theme.line
    implicitHeight: column.implicitHeight + padding * 2

    Column {
        id: column
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: card.padding
        }
        spacing: Theme.spacing / 2
    }
}

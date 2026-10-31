// Лист поверх страницы: затемнение, панель surface со скруглением, заголовок,
// прокручиваемое содержимое и кнопки внизу. Касание по затемнению — закрыть.
import QtQuick
import TamicTplTheme

Item {
    id: sheet

    property string title: ""
    property bool open: false
    default property alias content: body.data
    property alias buttons: buttonRow.data
    signal closed()

    anchors.fill: parent
    visible: open
    z: 100

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
        opacity: 0.8
        MouseArea { anchors.fill: parent; onClicked: { sheet.open = false; sheet.closed() } }
    }

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.gutter, 560)
        height: Math.min(parent.height - 2 * Theme.gutter,
                         header.height + flick.contentHeight + buttonRow.height + 3 * Theme.spacing + 2 * Theme.gutter)
        color: Theme.surface
        radius: Theme.radius
        border.width: Theme.borderW
        border.color: Theme.line
        MouseArea { anchors.fill: parent }   // касания внутри панели не закрывают лист

        Text {
            id: header
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: Theme.gutter }
            text: sheet.title
            color: Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeH2
            font.weight: Font.Bold
            wrapMode: Text.Wrap
        }
        Flickable {
            id: flick
            anchors {
                left: parent.left; right: parent.right
                top: header.bottom; bottom: buttonRow.top
                leftMargin: Theme.gutter; rightMargin: Theme.gutter
                topMargin: Theme.spacing; bottomMargin: Theme.spacing
            }
            clip: true
            contentWidth: width
            contentHeight: body.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            Column {
                id: body
                width: flick.width
                spacing: Theme.spacing / 2
            }
        }
        Row {
            id: buttonRow
            anchors { right: parent.right; bottom: parent.bottom; margins: Theme.gutter }
            spacing: Theme.spacing / 2
            height: childrenRect.height
        }
    }
}

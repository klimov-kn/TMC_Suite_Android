// Строка списка: заголовок, подпись, справа — статус или действие, «›».
// Нажатие — фон surface-2, рамка accent (как у кнопок).
// https://www.tamic.ru/docs/platforms/android.html#app
import QtQuick

Rectangle {
    id: row

    property string title
    property string subtitle
    property bool mono: false            // заголовок моноширинным (имена файлов)
    property string actionText: ""       // кнопка справа («Поделиться», «Убрать»)
    property bool actionDanger: false
    property bool chevron: true
    property bool highlighted: false     // выбранная строка: фон line, рамка accent (style-guide)
    default property alias trailing: trailingSlot.data
    signal clicked()
    signal pressAndHold()               // долгое нажатие — меню строки
    signal actionClicked()

    implicitHeight: Math.max(Theme.touchMin + 16, textColumn.implicitHeight + 20)
    radius: Theme.radiusSm
    color: highlighted ? Theme.line : area.pressed ? Theme.surface2 : Theme.surface
    border.width: Theme.borderW
    border.color: area.pressed || highlighted ? Theme.accent : Theme.line

    MouseArea {
        id: area
        anchors.fill: parent
        onClicked: row.clicked()
        onPressAndHold: row.pressAndHold()
    }

    Column {
        id: textColumn
        anchors {
            left: parent.left
            leftMargin: Theme.gutter
            right: rightSide.left
            rightMargin: Theme.spacing
            verticalCenter: parent.verticalCenter
        }
        spacing: 2
        Text {
            width: parent.width
            text: row.title
            color: row.highlighted ? Theme.accent : Theme.text
            font.family: row.mono ? Theme.fontMono : Theme.fontUi
            font.pixelSize: Theme.fontSizeBody
            font.weight: row.mono ? Font.Bold : Font.DemiBold
            elide: Text.ElideMiddle
        }
        Text {
            width: parent.width
            visible: text.length > 0
            text: row.subtitle
            color: Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideRight
        }
    }

    Row {
        id: rightSide
        anchors {
            right: parent.right
            rightMargin: Theme.spacing
            verticalCenter: parent.verticalCenter
        }
        spacing: Theme.spacing / 2

        Item {
            id: trailingSlot
            anchors.verticalCenter: parent.verticalCenter
            width: childrenRect.width
            height: childrenRect.height
        }

        Rectangle {
            visible: row.actionText.length > 0
            anchors.verticalCenter: parent.verticalCenter
            width: actionLabel.implicitWidth + 20
            height: Theme.touchMin - 8
            radius: Theme.radiusTool
            color: actionArea.pressed ? Theme.surface2 : Theme.bg2
            border.width: Theme.borderW
            border.color: row.actionDanger ? Theme.danger
                        : actionArea.pressed ? Theme.accent : Theme.line
            Text {
                id: actionLabel
                anchors.centerIn: parent
                text: row.actionText
                color: row.actionDanger ? Theme.danger : Theme.accent
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
                font.weight: Font.DemiBold
            }
            MouseArea {
                id: actionArea
                anchors.fill: parent
                anchors.margins: -4
                onClicked: row.actionClicked()
            }
        }

        Text {
            visible: row.chevron
            anchors.verticalCenter: parent.verticalCenter
            text: "›"
            color: Theme.textMute
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeH2
        }
    }
}

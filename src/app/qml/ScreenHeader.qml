// Шапка экрана: необязательная ссылка «назад», заголовок (Inter 800), подзаголовок.
// compact (низкий экран, горизонтальное положение) — одна строка: «‹ назад» и заголовок
// поменьше, без подзаголовка.
import QtQuick

Item {
    id: header

    property string title
    property string subtitle
    property string backText: ""          // пусто — без кнопки «назад»
    property bool compact: false
    signal backClicked()

    implicitHeight: compact ? Theme.touchMin : column.implicitHeight + Theme.spacing

    // --- Обычная шапка -------------------------------------------------------
    Column {
        id: column
        visible: !header.compact
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            topMargin: header.backText.length > 0 ? 0 : Theme.spacing
        }
        spacing: 2

        Item {
            visible: header.backText.length > 0
            width: Math.min(parent.width, backLabel.implicitWidth + Theme.spacing)
            height: Theme.touchMin

            Text {
                id: backLabel
                anchors.verticalCenter: parent.verticalCenter
                text: "‹ " + header.backText
                color: backArea.pressed ? Theme.accentPressed : Theme.accent
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                width: Math.min(implicitWidth, header.width)
            }
            MouseArea {
                id: backArea
                anchors.fill: parent
                onClicked: header.backClicked()
            }
        }

        Text {
            width: parent.width
            text: header.title
            color: Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeScreen
            font.weight: Theme.fontWeightTitle
            elide: Text.ElideMiddle
        }
        Text {
            width: parent.width
            visible: text.length > 0
            text: header.subtitle
            color: Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }
    }

    // --- Компактная шапка (одна строка) ---------------------------------------
    Row {
        visible: header.compact
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        spacing: Theme.spacing

        Item {
            visible: header.backText.length > 0
            width: Math.min(compactBack.implicitWidth, header.width / 3) + Theme.spacing / 2
            height: Theme.touchMin
            Text {
                id: compactBack
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, header.width / 3)
                text: "‹ " + header.backText
                color: compactBackArea.pressed ? Theme.accentPressed : Theme.accent
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            MouseArea {
                id: compactBackArea
                anchors.fill: parent
                onClicked: header.backClicked()
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: header.width - x
            text: header.title
            color: Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeH2 + 2
            font.weight: Theme.fontWeightTitle
            elide: Text.ElideMiddle
        }
    }
}

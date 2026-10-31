// Небольшая кнопка-переключатель (вид оси графика, координаты): скругление 8 как у
// кнопок-инструментов (style-guide), выбранная — фон accent, текст textOnAccent.
// https://www.tamic.ru/docs/platforms/android.html#app
import QtQuick

Rectangle {
    id: chip

    property string text
    property bool active: false
    property bool accentText: false
    signal clicked()

    implicitWidth: label.implicitWidth + 24
    implicitHeight: Theme.touchMin - 6
    radius: Theme.radiusTool
    opacity: enabled ? 1 : 0.45
    color: active ? Theme.accent : area.pressed ? Theme.surface2 : Theme.surface
    border.width: active ? 0 : Theme.borderW
    border.color: area.pressed ? Theme.accent : Theme.line

    Text {
        id: label
        anchors.centerIn: parent
        text: chip.text
        color: chip.active ? Theme.textOnAccent : chip.accentText ? Theme.accent : Theme.text
        font.family: Theme.fontUi
        font.pixelSize: Theme.fontSizeSmall
        font.weight: chip.active || chip.accentText ? Font.DemiBold : Font.Normal
    }
    MouseArea {
        id: area
        anchors.fill: parent
        anchors.margins: -3
        onClicked: chip.clicked()
    }
}

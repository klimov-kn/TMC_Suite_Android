// Статус расчёта: не считалась — text-mute; идёт — accent;
// готово — ok; ошибка — danger; остановлено — warn.
import QtQuick

Rectangle {
    id: chip

    property int code: -1          // -1 нет расчёта, 0..3 — код ядра
    property bool running: false
    readonly property color tone: running ? Theme.accent
                                : code === 0 ? Theme.ok
                                : code === 1 || code === 2 ? Theme.danger
                                : code === 3 ? Theme.warn
                                : Theme.textMute
    readonly property string label: running ? qsTr("идёт")
                                  : code === 0 ? qsTr("готово")
                                  : code === 1 ? qsTr("ошибка в задании")
                                  : code === 2 ? qsTr("ошибка расчёта")
                                  : code === 3 ? qsTr("остановлено")
                                  : qsTr("не считалась")

    implicitWidth: label.implicitWidth + 16
    implicitHeight: label.implicitHeight + 6
    radius: height / 2
    color: "transparent"
    border.width: Theme.borderW
    border.color: tone

    Text {
        id: label
        anchors.centerIn: parent
        text: chip.label
        color: chip.tone
        font.family: Theme.fontUi
        font.pixelSize: Theme.fontSizeCaption
        font.weight: Font.DemiBold
    }
}

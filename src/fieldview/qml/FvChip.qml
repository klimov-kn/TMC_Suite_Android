// Переключатель-«чип» панели «Поле»: включён — рамка и текст accent, фон surface-2.
import QtQuick
import TamicTplTheme

Rectangle {
    id: chip
    property alias text: label.text
    property bool checked: false
    signal clicked()

    height: Math.max(36, Theme.touchMin - 8)
    width: label.implicitWidth + 28
    radius: Theme.radiusSm
    color: checked ? Theme.surface2 : Theme.surface
    border.width: Theme.borderW
    border.color: checked || tap.pressed ? Theme.accent : Theme.line

    Text {
        id: label
        anchors.centerIn: parent
        color: chip.checked ? Theme.accent : Theme.text
        font.family: Theme.fontUi
        font.pixelSize: Theme.fontSizeSmall
        font.weight: chip.checked ? Font.DemiBold : Font.Normal
    }

    TapHandler {
        id: tap
        // Касание по краю чипа тоже считается (палец шире рамки).
        margin: 4
        onTapped: chip.clicked()
    }
}

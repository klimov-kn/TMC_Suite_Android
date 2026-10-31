// Счётчик замечаний проверки: ошибки — danger, предупреждения — warn, чисто — ok.
// Касание ведёт на шаг «Проверка». Ошибку показываем там, где её сделали, —
// на любом шаге, как список замечаний под страницей у настольного TPL Designer.
// https://www.tamic.ru/docs/platforms/android.html#designer
import QtQuick
import TamicTplTheme

Rectangle {
    id: chip

    required property DesignerModel model
    signal clicked()

    readonly property color tone: model.errorCount > 0 ? Theme.danger
                                : model.warningCount > 0 ? Theme.warn : Theme.ok
    width: label.implicitWidth + 24
    height: Theme.touchMin - 8
    radius: height / 2
    color: Theme.surface
    border.width: Theme.borderW
    border.color: tone

    Text {
        id: label
        anchors.centerIn: parent
        text: chip.model.errorCount > 0 ? qsTr("Ошибок: %1").arg(chip.model.errorCount)
            : chip.model.warningCount > 0 ? qsTr("Замечаний: %1").arg(chip.model.warningCount)
            : qsTr("Без ошибок")
        color: chip.tone
        font.family: Theme.fontUi
        font.pixelSize: Theme.fontSizeSmall
        font.weight: Font.DemiBold
    }
    MouseArea {
        anchors.fill: parent
        anchors.margins: -4          // касание 48 dp при видимой высоте 40
        onClicked: chip.clicked()
    }
}

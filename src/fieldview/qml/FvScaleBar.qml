// Цветовая шкала поверх сцены: полоса цветов шкалы («Волна» или «Свечение») снизу вверх,
// подписи максимума, середины и минимума (числа — JetBrains Mono).
// https://www.tamic.ru/docs/platforms/android.html#field
import QtQuick
import TamicTplTheme

Item {
    id: bar
    property var colors: []
    property real minValue: 0
    property real maxValue: 1
    property string unit: ""

    width: 14 + labels.width + 6

    function fmt(v) {
        if (v === 0)
            return "0"
        const a = Math.abs(v)
        return (a >= 1e4 || a < 1e-2) ? v.toExponential(2) : v.toPrecision(3)
    }

    Rectangle {
        id: strip
        anchors.right: parent.right
        width: 14
        height: parent.height
        radius: 4
        border.width: Theme.borderW
        border.color: Theme.line
        color: Theme.bg
        clip: true
        Column {
            anchors.fill: parent
            anchors.margins: 1
            Repeater {
                model: bar.colors.length
                delegate: Rectangle {
                    required property int index
                    width: strip.width - 2
                    height: (strip.height - 2) / bar.colors.length
                    // Сверху — максимум.
                    color: bar.colors[bar.colors.length - 1 - index]
                }
            }
        }
    }

    Column {
        id: labels
        anchors.right: strip.left
        anchors.rightMargin: 6
        height: parent.height
        width: Math.max(maxText.implicitWidth, midText.implicitWidth, minText.implicitWidth, unitText.implicitWidth)
        Text {
            id: unitText
            anchors.right: parent.right
            text: bar.unit
            color: Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeCaption
        }
        Text {
            id: maxText
            anchors.right: parent.right
            text: bar.fmt(bar.maxValue)
            color: Theme.textDim
            font.family: Theme.fontMono
            font.pixelSize: Theme.fontSizeCaption
        }
        Item {
            width: 1
            height: Math.max(0, labels.height / 2 - unitText.height - maxText.height - midText.height / 2)
        }
        Text {
            id: midText
            anchors.right: parent.right
            text: bar.fmt((bar.maxValue + bar.minValue) / 2)
            color: Theme.textMute
            font.family: Theme.fontMono
            font.pixelSize: Theme.fontSizeCaption
        }
        Item {
            width: 1
            height: Math.max(0, labels.height / 2 - midText.height / 2 - minText.height)
        }
        Text {
            id: minText
            anchors.right: parent.right
            text: bar.fmt(bar.minValue)
            color: Theme.textDim
            font.family: Theme.fontMono
            font.pixelSize: Theme.fontSizeCaption
        }
    }
}

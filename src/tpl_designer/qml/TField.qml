// Поле ввода с подписью и единицей (фон bg, рамка line, в
// фокусе — accent; выделение — фон accent, текст bg). Значение уходит в модель
// по окончании ввода (edited), а не на каждую букву: иначе проверка задания
// дёргалась бы посреди набора числа.
import QtQuick
import TamicTplTheme
import QtQuick.Templates as T

Column {
    id: field

    property string label: ""
    property string value: ""          // что показывать (из модели)
    property string unit: ""
    property bool numeric: true
    property bool mono: numeric
    property bool multiline: false
    property string placeholder: ""
    property bool readOnly: false
    signal edited(string text)

    width: parent ? parent.width : 300
    spacing: 4

    Text {
        visible: field.label.length > 0
        width: parent.width
        text: field.label
        color: Theme.textDim
        font.family: Theme.fontUi
        font.pixelSize: Theme.fontSizeSmall
        elide: Text.ElideRight
    }

    Rectangle {
        id: box
        width: parent.width
        height: field.multiline ? Math.max(Theme.touchMin * 2, area.implicitHeight + 16) : Theme.touchMin
        radius: Theme.radiusSm
        color: Theme.bg
        border.width: Theme.borderW
        border.color: (input.activeFocus || area.activeFocus) ? Theme.accent : Theme.line

        T.TextField {
            id: input
            visible: !field.multiline
            anchors {
                left: parent.left
                right: unitText.left
                verticalCenter: parent.verticalCenter
                leftMargin: 12
                rightMargin: 6
            }
            height: parent.height
            text: field.value
            readOnly: field.readOnly
            verticalAlignment: TextInput.AlignVCenter
            color: Theme.text
            selectionColor: Theme.accent
            selectedTextColor: Theme.bg
            placeholderText: field.placeholder
            placeholderTextColor: Theme.textMute
            font.family: field.mono ? Theme.fontMono : Theme.fontUi
            font.pixelSize: Theme.fontSizeBody
            inputMethodHints: field.numeric ? (Qt.ImhFormattedNumbersOnly | Qt.ImhNoPredictiveText)
                                            : Qt.ImhNoPredictiveText
            onEditingFinished: if (text !== field.value) field.edited(text)
        }

        T.TextArea {
            id: area
            visible: field.multiline
            anchors.fill: parent
            anchors.margins: 8
            text: field.value
            readOnly: field.readOnly
            wrapMode: TextEdit.Wrap
            color: Theme.text
            selectionColor: Theme.accent
            selectedTextColor: Theme.bg
            placeholderText: field.placeholder
            placeholderTextColor: Theme.textMute
            font.family: field.mono ? Theme.fontMono : Theme.fontUi
            font.pixelSize: Theme.fontSizeBody
            inputMethodHints: Qt.ImhNoPredictiveText
            onEditingFinished: if (text !== field.value) field.edited(text)
        }

        Text {
            id: unitText
            anchors {
                right: parent.right
                rightMargin: field.unit.length > 0 ? 12 : 0
                verticalCenter: parent.verticalCenter
            }
            visible: !field.multiline
            text: field.unit
            width: field.unit.length > 0 ? implicitWidth : 0
            color: Theme.textMute
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    // Модель могла вернуть то же значение в другом виде («2,0» -> «2») или отвергнуть
    // ввод — показываем то, что в модели, как только поле отпущено.
    onValueChanged: {
        if (!input.activeFocus)
            input.text = value
        if (!area.activeFocus)
            area.text = value
    }
}

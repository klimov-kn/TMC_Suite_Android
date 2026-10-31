// Строка-переключатель во всю ширину: подпись, пояснение и флажок справа.
// Касание по всей строке (не меньше 48 dp); включённый флажок — accent (style-guide).
// https://www.tamic.ru/docs/platforms/android.html#designer
import QtQuick
import TamicTplTheme

Item {
    id: row

    property string text: ""
    property string hint: ""
    property bool checked: false
    signal toggled(bool on)

    width: parent ? parent.width : 300
    implicitHeight: Math.max(Theme.touchMin, labels.implicitHeight + 12)
    height: implicitHeight
    opacity: enabled ? 1.0 : 0.5

    Column {
        id: labels
        anchors {
            left: parent.left
            right: track.left
            rightMargin: Theme.spacing
            verticalCenter: parent.verticalCenter
        }
        spacing: 2
        Text {
            width: parent.width
            text: row.text
            color: Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeBody
            wrapMode: Text.Wrap
        }
        Text {
            width: parent.width
            visible: row.hint.length > 0
            text: row.hint
            color: Theme.textMute
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.Wrap
        }
    }

    Rectangle {
        id: track
        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
        width: 48
        height: 28
        radius: 14
        color: row.checked ? Theme.accent : Theme.bg
        border.width: Theme.borderW
        border.color: row.checked ? Theme.accent : Theme.line
        Behavior on color {
            ColorAnimation { duration: Theme.durMs; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeCurve }
        }
        Rectangle {
            width: 20
            height: 20
            radius: 10
            y: 4
            x: row.checked ? parent.width - width - 4 : 4
            color: row.checked ? Theme.textOnAccent : Theme.textDim
            Behavior on x {
                NumberAnimation { duration: Theme.durMs; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeCurve }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: row.toggled(!row.checked)
    }
}

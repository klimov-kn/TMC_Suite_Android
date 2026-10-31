// Выбор одного из нескольких (как переключатель H/X оболочки): выбранный — accent.
// options: [{key, label}]; columns — сколько в ряд (по умолчанию все в один ряд).
// https://www.tamic.ru/docs/platforms/android.html#designer
pragma ComponentBehavior: Bound
import QtQuick
import TamicTplTheme

Rectangle {
    id: seg

    property var options: []
    property string current: ""
    property int columns: options.length
    signal picked(string key)

    readonly property int cols: Math.max(1, columns)
    readonly property int rows: Math.max(1, Math.ceil(options.length / cols))
    width: parent ? parent.width : 300
    implicitHeight: rows * Theme.touchMin + 8
    height: implicitHeight
    radius: Theme.radiusSm
    color: Theme.bg
    border.width: Theme.borderW
    border.color: Theme.line
    opacity: enabled ? 1.0 : 0.5

    Grid {
        anchors.fill: parent
        anchors.margins: 4
        columns: seg.cols
        Repeater {
            model: seg.options
            delegate: Rectangle {
                id: cell
                required property var modelData
                readonly property bool active: seg.current === modelData.key
                width: (seg.width - 8) / seg.cols
                height: Theme.touchMin
                radius: Theme.radiusSm - 3
                color: active ? Theme.accent : (area.pressed ? Theme.surface2 : "transparent")
                Behavior on color {
                    ColorAnimation { duration: Theme.durMs; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeCurve }
                }
                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 4
                    anchors.rightMargin: 4
                    text: cell.modelData.label
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    color: cell.active ? Theme.textOnAccent : Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: cell.active ? Font.Bold : Font.Normal
                }
                MouseArea {
                    id: area
                    anchors.fill: parent
                    onClicked: seg.picked(cell.modelData.key)
                }
            }
        }
    }
}

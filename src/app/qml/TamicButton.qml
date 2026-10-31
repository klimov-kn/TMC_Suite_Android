// Кнопка в стиле проекта:
//  - обычная: фон surface, рамка line, при нажатии рамка accent;
//  - главная (primary: true, одна на экран): фон accent, текст textOnAccent, жирный;
//  - опасная (danger: true): текст и рамка danger.
// Построена на QtQuick.Templates — без чужого стиля (Material и т.п.).
// https://www.tamic.ru/docs/platforms/android.html#app
import QtQuick
import QtQuick.Templates as T

T.Button {
    id: control

    property bool primary: false
    property bool danger: false

    implicitWidth: Math.max(implicitContentWidth + leftPadding + rightPadding, 160)
    implicitHeight: Math.max(implicitContentHeight + topPadding + bottomPadding, Theme.touchMin + 8)
    padding: Theme.spacing
    leftPadding: Theme.gutter + 4
    rightPadding: Theme.gutter + 4

    font.family: Theme.fontUi
    font.pixelSize: Theme.fontSizeBody + 1
    font.weight: control.primary ? Font.Bold : Font.DemiBold

    contentItem: Text {
        text: control.text
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        color: control.primary ? Theme.textOnAccent
             : control.danger ? Theme.danger
             : control.enabled ? Theme.text : Theme.textMute
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.primary ? (control.down ? Theme.accentPressed : Theme.accent)
             : control.down ? Theme.surface2 : Theme.surface
        border.width: control.primary ? 0 : Theme.borderW
        border.color: control.danger ? Theme.danger
                    : (control.down || control.visualFocus) ? Theme.accent : Theme.line

        Behavior on border.color {
            ColorAnimation { duration: Theme.durMs; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeCurve }
        }
        Behavior on color {
            ColorAnimation { duration: Theme.durMs; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeCurve }
        }
    }
}

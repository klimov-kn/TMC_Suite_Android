// Кнопка в стиле проекта, как
// TamicButton оболочки: обычная — фон surface, рамка line, нажатие — рамка accent;
// главная (primary) — фон accent, текст textOnAccent; опасная (danger) — текст и рамка danger.
import QtQuick
import TamicTplTheme
import QtQuick.Templates as T

T.Button {
    id: control

    property bool primary: false
    property bool danger: false
    property bool compact: false     // узкая (в ряд по 2–3), высота та же — касание 48 dp

    implicitWidth: Math.max(implicitContentWidth + leftPadding + rightPadding,
                            compact ? Theme.touchMin : 120)
    implicitHeight: Math.max(implicitContentHeight + topPadding + bottomPadding, Theme.touchMin)
    padding: compact ? 8 : Theme.spacing
    leftPadding: compact ? 10 : Theme.gutter
    rightPadding: compact ? 10 : Theme.gutter

    font.family: Theme.fontUi
    font.pixelSize: compact ? Theme.fontSizeSmall : Theme.fontSizeBody
    font.weight: control.primary ? Font.Bold : Font.DemiBold

    contentItem: Text {
        text: control.text
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        color: !control.enabled ? Theme.textMute
             : control.primary ? Theme.textOnAccent
             : control.danger ? Theme.danger : Theme.text
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.primary ? (!control.enabled ? Theme.surface2
                                  : control.down ? Theme.accentPressed : Theme.accent)
             : control.down ? Theme.surface2 : Theme.surface
        border.width: control.primary && control.enabled ? 0 : Theme.borderW
        border.color: control.danger ? Theme.danger
                    : (control.down || control.visualFocus) ? Theme.accent : Theme.line
        Behavior on color {
            ColorAnimation { duration: Theme.durMs; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeCurve }
        }
    }
}

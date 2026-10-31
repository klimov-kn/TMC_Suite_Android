// Кнопка-значок панели «Поле»: главная (accent) или обычная (surface + рамка line).
// Значки рисуются фигурами, без шрифтов: play, pause, prev, next.
import QtQuick
import TamicTplTheme

Rectangle {
    id: btn
    property string icon: "play"
    property bool primary: false
    signal clicked()

    width: Theme.touchMin
    height: Theme.touchMin
    radius: Theme.radiusSm
    opacity: enabled ? 1.0 : 0.4
    color: primary ? (tap.pressed ? Theme.accentPressed : Theme.accent) : Theme.surface
    border.width: primary ? 0 : Theme.borderW
    border.color: tap.pressed ? Theme.accent : Theme.line

    readonly property color glyph: primary ? Theme.textOnAccent : Theme.text

    Canvas {
        id: canvas
        anchors.centerIn: parent
        width: 20
        height: 20
        property color c: btn.glyph
        property string kind: btn.icon
        onCChanged: requestPaint()
        onKindChanged: requestPaint()
        onPaint: {
            const g = getContext("2d")
            g.reset()
            g.fillStyle = c
            const tri = (x0, dir) => {
                g.beginPath()
                if (dir > 0) { g.moveTo(x0, 3); g.lineTo(x0 + 11, 10); g.lineTo(x0, 17) }
                else { g.moveTo(x0 + 11, 3); g.lineTo(x0, 10); g.lineTo(x0 + 11, 17) }
                g.closePath()
                g.fill()
            }
            if (kind === "play") {
                tri(5, 1)
            } else if (kind === "pause") {
                g.fillRect(5, 3, 4, 14)
                g.fillRect(11, 3, 4, 14)
            } else if (kind === "prev") {
                g.fillRect(3, 3, 3, 14)
                tri(7, -1)
            } else if (kind === "next") {
                tri(2, 1)
                g.fillRect(14, 3, 3, 14)
            }
        }
    }

    TapHandler {
        id: tap
        onTapped: btn.clicked()
    }
}

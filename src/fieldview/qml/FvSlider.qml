// Ползунок тактов: дорожка line, пройденная часть и ручка accent. Без Qt Quick Controls —
// чтобы не зависеть от стиля оболочки.
import QtQuick
import TamicTplTheme

Item {
    id: s
    property real from: 0
    property real to: 1
    property real value: 0
    signal moved(real v)

    height: Theme.touchMin
    opacity: enabled ? 1.0 : 0.4

    readonly property real frac: to > from ? Math.max(0, Math.min(1, (value - from) / (to - from))) : 0
    readonly property real knob: 22

    Rectangle {
        id: track
        x: s.knob / 2
        width: s.width - s.knob
        height: 4
        radius: 2
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.line
        Rectangle {
            width: parent.width * s.frac
            height: parent.height
            radius: 2
            color: Theme.accent
        }
    }
    Rectangle {
        width: s.knob
        height: s.knob
        radius: s.knob / 2
        x: track.x + track.width * s.frac - s.knob / 2
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.accent
        border.width: 3
        border.color: Theme.bg2
    }

    function valueAt(px) {
        const f = Math.max(0, Math.min(1, (px - track.x) / Math.max(1, track.width)))
        return from + f * (to - from)
    }
    TapHandler {
        onTapped: (point) => s.moved(s.valueAt(point.position.x))
    }
    DragHandler {
        target: null
        xAxis.enabled: true
        yAxis.enabled: false
        onCentroidChanged: if (active) s.moved(s.valueAt(centroid.position.x))
    }
}

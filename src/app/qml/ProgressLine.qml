// Полоса хода расчёта: дорожка line, заполнение accent (или цвет итога).
// indeterminate — идёт работа без процентов (подготовка задания): по дорожке бежит отрезок,
// чтобы было видно, что приложение не зависло.
import QtQuick

Rectangle {
    id: line

    property real value: 0          // 0..100
    property color fill: Theme.accent
    property bool indeterminate: false
    // Плавное заполнение. Выключать там, где окно перерисовывается не по таймеру анимации
    // (экспорт анимации снимает окно на каждом кадре — анимация ширины сбивалась туда-обратно).
    property bool animated: true

    implicitHeight: 8
    radius: height / 2
    color: Theme.line

    Rectangle {
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        width: Math.max(parent.height, parent.width * Math.min(100, Math.max(0, line.value)) / 100)
        visible: line.value > 0
        radius: parent.radius
        color: line.fill
        Behavior on width {
            enabled: line.animated
            NumberAnimation { duration: Theme.durMs; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeCurve }
        }
    }

    Item {
        anchors.fill: parent
        clip: true
        visible: line.indeterminate
        Rectangle {
            id: runner
            width: Math.max(line.height * 4, line.width * 0.22)
            height: parent.height
            radius: line.radius
            color: line.fill
            opacity: 0.85
            SequentialAnimation on x {
                running: line.indeterminate && line.visible
                loops: Animation.Infinite
                NumberAnimation { from: -runner.width; to: line.width; duration: 1400
                                  easing.type: Easing.InOutQuad }
                PauseAnimation { duration: 150 }
            }
        }
    }
}

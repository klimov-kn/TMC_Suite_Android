// Значки инструментов холста — линии, как значки вкладок оболочки (TabIcon.qml);
// рисунки повторяют значки TPL Designer (mainwindow.cpp, toolIcon).
// https://www.tamic.ru/docs/platforms/android.html#designer
import QtQuick
import TamicTplTheme
import QtQuick.Shapes

Shape {
    id: icon

    property string kind: "select"
    property color color: Theme.text

    width: Theme.iconSize
    height: Theme.iconSize
    preferredRendererType: Shape.CurveRenderer

    readonly property var paths: ({
        "pan":     "M12 3 L12 21 M3 12 L21 12 M12 3 L9 6 M12 3 L15 6 M12 21 L9 18 M12 21 L15 18 M3 12 L6 9 M3 12 L6 15 M21 12 L18 9 M21 12 L18 15",
        "multi":   "M4 4 L14 4 L14 14 L4 14 Z M10 10 L20 10 L20 20 L10 20 Z",
        "select":  "M7 3 L7 19 L11 15 L14 21 L16.5 20 L13.5 14 L18 13.5 Z",
        "rect":    "M3.5 6 L20.5 6 L20.5 18 L3.5 18 Z",
        "circle":  "M12 4 A8 8 0 1 1 11.99 4 Z",
        "line":    "M3.5 19 L10 7.5 L20.5 14",
        "contour": "M4 16 L8.5 5 L19.5 8.5 L16 19.5 Z",
        "arc":     "M3.5 18 C6 4.5 18 4.5 20.5 18",
        "port":    "M6 4 L6 20 M9 12 L20 12 M16 8 L20 12 L16 16",
        "fit":     "M4 9 L4 4 L9 4 M15 4 L20 4 L20 9 M20 15 L20 20 L15 20 M9 20 L4 20 L4 15",
        "grid":    "M4 4 L20 4 L20 20 L4 20 Z M9.3 4 L9.3 20 M14.7 4 L14.7 20 M4 9.3 L20 9.3 M4 14.7 L20 14.7",
        "snap":    "M7 4 L7 13 A5 5 0 0 0 17 13 L17 4 M4 4 L10 4 M14 4 L20 4",
        "undo":    "M9 6 L4 11 L9 16 M4 11 L14 11 A5 5 0 0 1 14 21",
        "redo":    "M15 6 L20 11 L15 16 M20 11 L10 11 A5 5 0 0 0 10 21",
        "trash":   "M4 7 L20 7 M9 7 L9 4 L15 4 L15 7 M6 7 L7 20 L17 20 L18 7",
        "copy":    "M8 8 L20 8 L20 20 L8 20 Z M16 8 L16 4 L4 4 L4 16 L8 16",
        "plus":    "M12 4 L12 20 M4 12 L20 12",
        "edit":    "M4 20 L4 16 L15 5 L19 9 L8 20 Z M13 7 L17 11",
        "check":   "M4 12 L9.5 17.5 L20 7",
        "close":   "M6 6 L18 18 M18 6 L6 18",
        "back":    "M15 5 L8 12 L15 19"
    })

    ShapePath {
        strokeColor: icon.color
        strokeWidth: 2
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin
        PathSvg { path: icon.paths[icon.kind] || "" }
    }
}

// Значки нижних вкладок — линии; цвет задаёт вкладка (accent / text-mute).
// https://www.tamic.ru/docs/platforms/android.html#app
import QtQuick
import QtQuick.Shapes

Shape {
    id: icon

    property string kind: "tasks"     // tasks · designer · results · about
    property color color: Theme.textMute

    width: Theme.iconSize
    height: Theme.iconSize
    preferredRendererType: Shape.CurveRenderer

    ShapePath {
        strokeColor: icon.color
        strokeWidth: 2
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin
        PathSvg {
            path: icon.kind === "designer"
                  ? "M4 20 L4 16 L15 5 L19 9 L8 20 Z M13 7 L17 11"
                  : icon.kind === "results"
                    ? "M3 20 L21 20 M3 4 L3 20 M6 15 C9 5 11 5 13 12 C15 18 17 16 20 7"
                  : icon.kind === "about"
                    ? "M12 3 A9 9 0 1 1 11.99 3 Z M12 11 L12 17 M12 7.5 L12 7.6"
                    : "M3 6 L3 19 L21 19 L21 8 L11 8 L9 5 L3 5 Z"
        }
    }
}

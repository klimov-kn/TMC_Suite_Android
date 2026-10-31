// Прокручиваемая страница шага. Вертикально — карточки одной колонкой; горизонтально
// (wide) — двумя: колонки заполняются по очереди, карточка задаёт ширину colWidth.
import QtQuick
import TamicTplTheme

Flickable {
    id: page

    property bool wide: false
    default property alias content: flow.data
    readonly property real colWidth: wide ? (width - 2 * Theme.gutter - Theme.spacing) / 2
                                          : width - 2 * Theme.gutter

    clip: true
    contentWidth: width
    contentHeight: flow.height + 2 * Theme.gutter
    boundsBehavior: Flickable.StopAtBounds

    Flow {
        id: flow
        x: Theme.gutter
        y: Theme.gutter
        width: page.width - 2 * Theme.gutter
        spacing: Theme.spacing
    }
}

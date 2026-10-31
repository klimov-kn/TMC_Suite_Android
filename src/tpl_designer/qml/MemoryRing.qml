// Кольцо памяти телефона (настольный MemoryDonut): всё кольцо — вся память, сплошной сектор — занято
// системой и приложениями, следом — расчёт: сетка и накопление ДН. Не помещается — кольцо краснеет.
// Это только картинка: сохранить файл можно в любом случае.
// https://www.tamic.ru/docs/code/tpl-designer.html#gui-memory
import QtQuick
import TamicTplTheme

Canvas {
    id: ring

    /// Карта DesignerModel.memory.
    property var data: ({})

    implicitWidth: 132
    implicitHeight: 132
    onDataChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const r = Math.min(width, height) / 2 - 9
        const cx = width / 2, cy = height / 2
        const arc = (from, part, color) => {
            if (part <= 0) return from
            ctx.beginPath()
            ctx.strokeStyle = color
            ctx.lineWidth = 14
            ctx.arc(cx, cy, r, from, from + part * 2 * Math.PI, false)
            ctx.stroke()
            return from + part * 2 * Math.PI
        }
        const d = ring.data
        const start = -Math.PI / 2
        arc(start, 1, Theme.surface2)
        if (!d.known || !(d.total > 0)) return
        let a = arc(start, d.used / d.total, Theme.textMute)
        const room = Math.max(0, 1 - d.used / d.total)
        const grid = Math.min(room, d.grid / d.total)
        a = arc(a, grid, d.fits ? Theme.accent : Theme.danger)
        arc(a, Math.min(room - grid, d.pattern / d.total), d.fits ? Theme.accent2 : Theme.danger)
    }
}

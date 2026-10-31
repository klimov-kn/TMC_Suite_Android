// График файла результата: S-матрица (.s), ДН (.DAT), сигналы (.t).
// Рисует C++ (модуль Tamic.Viewers, QQuickPaintedItem); здесь — раскладка и жесты:
//   * щипок — масштаб, перетаскивание — сдвиг, двойное касание — сброс, касание — значение
//     точки; колесо мыши на ПК — масштаб;
//   * вертикально — график сверху, ряды снизу; горизонтально — график на весь экран,
//     справа узкая панель рядов. Элемент графика один на всё время показа,
//     поэтому поворот не теряет ни масштаба, ни выбора рядов.
// https://www.tamic.ru/docs/platforms/android.html#app
import QtQuick
import Tamic.Viewers

Item {
    id: screen

    property string folder
    property string file
    property var files: []            // все файлы этого вида из расчёта (их ряды — в списке)
    property string task: ""          // задание расчёта (подпись и исключение из сравнения)
    property var compare: []          // файлы других расчётов: [{ folder, file, label }]
    property bool compact: false
    property string options: ""       // параметры запуска для снимков: db, polar, mode=N, pick, all,
                                      // cmp (сравнить со всеми), cmpsheet (лист сравнения)
    signal back()

    readonly property string kind: ViewerTools.chartKind(file)
    readonly property bool landscape: width > height
    readonly property var chart: chartLoader.item
    readonly property real panelWidth: Math.min(260, Math.max(200, width * 0.27))

    // Назад (кнопка Android): закрыть лист сравнения. true — обработано.
    function goBack() {
        if (compareSheet.open) { compareSheet.close(); return true }
        return false
    }

    // Файлы того же вида из всех посчитанных задач (кроме открытого расчёта).
    function candidates() {
        const out = []
        const runs = TaskStore.allRuns()
        for (let i = 0; i < runs.length; ++i) {
            const r = runs[i]
            for (let k = 0; k < r.files.length; ++k) {
                const f = r.files[k]
                if (ViewerTools.chartKind(f) !== kind)
                    continue
                if (r.folder === folder && files.indexOf(f) >= 0)
                    continue
                out.push({ folder: r.folder, file: f, task: r.task, folderTitle: r.folderTitle, date: r.date })
            }
        }
        return out
    }
    function compareIndex(c) {
        for (let i = 0; i < compare.length; ++i)
            if (compare[i].folder === c.folder && compare[i].file === c.file)
                return i
        return -1
    }
    function toggleCompare(c) {
        const list = compare.slice()
        const i = compareIndex(c)
        if (i >= 0)
            list.splice(i, 1)
        else
            list.push({ folder: c.folder, file: c.file, label: c.task + " · " + c.file })
        compare = list
    }

    function saveImage() {
        if (!chart)
            return
        const stamp = Qt.formatDateTime(new Date(), "yyyyMMdd-HHmmss")
        const base = file.replace(/\.[^.]*$/, "")
        const name = base + "_" + (kind === "pattern" ? "DN" : kind === "signal" ? "signal" : "S") + "_" + stamp + ".png"
        const path = chart.saveImage(name)
        const ok = path.length > 0 && TaskStore.saveFile(path, folder, name)
        toast.show(ok ? qsTr("Сохранено в папку задачи: %1").arg(name)
                      : qsTr("Не удалось сохранить картинку"), !ok)
    }

    function applyOptions() {
        if (!chart || !options)
            return
        const list = options.split(",")
        for (let i = 0; i < list.length; ++i) {
            const o = list[i].trim()
            if (o === "db") chart.mode = 1
            else if (o === "polar") chart.polar = true
            else if (o.indexOf("mode=") === 0) chart.mode = parseInt(o.substring(5))
            else if (o === "all") { for (let k = 0; k < chart.series.length; ++k) chart.setSeriesVisible(k, true) }
            else if (o === "pick") chart.pickMax()
            else if (o === "cmp") { const c = candidates(); for (let k = 0; k < c.length; ++k) toggleCompare(c[k]) }
            else if (o === "cmpsheet") { compareSheet.items = candidates(); compareSheet.open = true }
        }
    }

    ScreenHeader {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        compact: screen.compact || screen.landscape
        backText: qsTr("Результаты")
        title: (screen.chart ? screen.chart.kindTitle + " · " : "") + screen.file
        subtitle: screen.chart && screen.chart.info ? screen.chart.info : ""
        onBackClicked: screen.back()
    }

    // --- График -----------------------------------------------------------------
    Rectangle {
        id: plotFrame
        x: 0
        y: header.height + (screen.compact || screen.landscape ? 4 : Theme.spacing / 2)
        width: screen.landscape ? screen.width - screen.panelWidth - Theme.spacing : screen.width
        height: screen.landscape ? screen.height - y
                                 : Math.max(220, (screen.height - y) * 0.62)
        radius: Theme.radius
        color: Theme.bg
        border.width: Theme.borderW
        border.color: Theme.line
        clip: true

        Loader {
            id: chartLoader
            anchors.fill: parent
            anchors.margins: 6
            sourceComponent: screen.kind === "smatrix" ? sComp
                           : screen.kind === "pattern" ? dnComp
                           : screen.kind === "signal" ? sigComp : null
            onLoaded: optionsTimer.restart()
        }
        Component { id: sComp; SMatrixChart { folder: screen.folder; file: screen.file; files: screen.files; compare: screen.compare } }
        Component { id: dnComp; PatternChart { folder: screen.folder; file: screen.file; files: screen.files; compare: screen.compare } }
        Component { id: sigComp; SignalChart { folder: screen.folder; file: screen.file; files: screen.files; compare: screen.compare } }

        // Параметры для снимков применяются, когда файл прочитан.
        Timer { id: optionsTimer; interval: 50; onTriggered: { if (screen.chart && screen.chart.loading) restart(); else screen.applyOptions() } }

        // Мышь на ПК. Касания разбирает сам график (ChartBase::touchEvent, src/ui/touchgesture.h).
        DragHandler {
            id: drag
            target: null
            acceptedDevices: PointerDevice.Mouse
            property real lastX: 0
            property real lastY: 0
            onActiveChanged: { lastX = 0; lastY = 0 }
            onActiveTranslationChanged: {
                if (!screen.chart || !active) return
                screen.chart.panBy(activeTranslation.x - lastX, activeTranslation.y - lastY)
                lastX = activeTranslation.x
                lastY = activeTranslation.y
            }
        }
        TapHandler {
            acceptedDevices: PointerDevice.Mouse
            onTapped: (point) => { if (screen.chart) screen.chart.pick(point.position.x - chartLoader.x, point.position.y - chartLoader.y) }
            onDoubleTapped: { if (screen.chart) { screen.chart.clearPick(); screen.chart.resetView() } }
        }
        WheelHandler {
            onWheel: (event) => {
                if (screen.chart)
                    screen.chart.zoomAt(Math.pow(1.0015, event.angleDelta.y), point.position.x - chartLoader.x,
                                        point.position.y - chartLoader.y)
            }
        }
    }

    // --- Панель: вид оси, кнопки, ряды --------------------------------------------
    Item {
        id: panel
        x: screen.landscape ? plotFrame.width + Theme.spacing : 0
        y: screen.landscape ? plotFrame.y : plotFrame.y + plotFrame.height + Theme.spacing / 2
        width: screen.landscape ? screen.panelWidth : screen.width
        height: screen.height - y

        // Кнопки: горизонтально — в несколько строк по ширине панели; вертикально — одной
        // строкой с прокруткой (больше места спискам рядов).
        Flickable {
            id: controls
            width: parent.width
            height: controlsFlow.childrenRect.height
            contentWidth: controlsFlow.childrenRect.width
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            Flow {
                id: controlsFlow
                width: screen.landscape ? controls.width : 4000
                spacing: 6
                // Вид оси Y (как в Windows-вьюверах) и координаты ДН.
                Repeater {
                    model: screen.chart ? screen.chart.modes : []
                    delegate: Chip {
                        required property string modelData
                        required property int index
                        text: modelData
                        active: screen.chart && screen.chart.mode === index
                        onClicked: screen.chart.mode = index
                    }
                }
                Chip {
                    visible: screen.chart && screen.chart.polarAvailable
                    text: qsTr("Декартовы"); active: screen.chart && !screen.chart.polar; onClicked: screen.chart.polar = false
                }
                Chip {
                    visible: screen.chart && screen.chart.polarAvailable
                    text: qsTr("Полярные"); active: screen.chart && screen.chart.polar; onClicked: screen.chart.polar = true
                }
                Chip {
                    text: qsTr("Сброс")
                    enabled: screen.chart && (screen.chart.zoomed || screen.chart.pickText.length > 0)
                    onClicked: { screen.chart.clearPick(); screen.chart.resetView() }
                }
                Chip {
                    text: screen.compare.length > 0 ? qsTr("Сравнить (%1)").arg(screen.compare.length) : qsTr("+ Сравнить")
                    accentText: true
                    active: screen.compare.length > 0
                    onClicked: { compareSheet.items = screen.candidates(); compareSheet.open = true }
                }
                Chip { text: qsTr("Сохранить PNG"); accentText: true; onClicked: screen.saveImage() }
            }
        }

        ListView {
            id: seriesList
            anchors { left: parent.left; right: parent.right; top: controls.bottom; topMargin: Theme.spacing / 2; bottom: parent.bottom }
            clip: true
            spacing: 2
            model: screen.chart ? screen.chart.series : []
            delegate: Column {
                id: rowItem
                required property var modelData
                required property int index
                width: ListView.view.width
                readonly property bool newGroup: index === 0
                    || (screen.chart && screen.chart.series[index - 1].group !== modelData.group)
                Text {
                    visible: rowItem.newGroup && (screen.files.length > 1 || screen.compare.length > 0)
                    height: visible ? implicitHeight + 6 : 0
                    verticalAlignment: Text.AlignBottom
                    text: rowItem.modelData.group
                    color: Theme.accent
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontSizeCaption
                    font.weight: Font.Bold
                }
                Rectangle {
                    width: parent.width
                    height: Theme.touchMin
                    radius: Theme.radiusTool
                    color: rowArea.pressed ? Theme.surface2 : rowItem.modelData.visible ? Theme.surface : "transparent"
                    border.width: rowItem.modelData.visible ? Theme.borderW : 0
                    border.color: Theme.line
                    Row {
                        anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                        spacing: 10
                        Rectangle {   // флажок (style-guide: скругление 4, accent)
                            anchors.verticalCenter: parent.verticalCenter
                            width: 20; height: 20
                            radius: Theme.radiusCheck
                            color: rowItem.modelData.visible ? Theme.accent : Theme.bg
                            border.width: Theme.borderW
                            border.color: rowItem.modelData.visible ? Theme.accent : Theme.line
                            Text {
                                anchors.centerIn: parent
                                visible: rowItem.modelData.visible
                                text: "✓"
                                color: Theme.textOnAccent
                                font.pixelSize: 14
                                font.weight: Font.Bold
                            }
                        }
                        Item {   // цвет ряда; ряд файла сравнения — пунктиром, как на графике
                            anchors.verticalCenter: parent.verticalCenter
                            width: 16; height: 4
                            Rectangle { width: rowItem.modelData.compared ? 6 : 16; height: 4; radius: 2; color: rowItem.modelData.color }
                            Rectangle { visible: rowItem.modelData.compared; x: 10; width: 6; height: 4; radius: 2; color: rowItem.modelData.color }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: seriesList.width - 80
                            text: rowItem.modelData.name
                            elide: Text.ElideRight
                            color: rowItem.modelData.visible ? Theme.text : Theme.textDim
                            font.family: Theme.fontMono
                            font.pixelSize: Theme.fontSizeMono
                        }
                    }
                    MouseArea {
                        id: rowArea
                        anchors.fill: parent
                        onClicked: screen.chart.setSeriesVisible(rowItem.index, !rowItem.modelData.visible)
                    }
                }
            }
        }
    }

    // Выбор файлов для сравнения: те же графики других расчётов (пересчитанный рупор и т.п.).
    Sheet {
        id: compareSheet
        property var items: []
        title: qsTr("Сравнить с другим расчётом")
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: compareSheet.items.length > 0
                  ? qsTr("Отметьте файлы — их кривые лягут на этот график пунктиром. Повторное касание убирает файл.")
                  : qsTr("Других посчитанных файлов этого вида нет. Посчитайте ещё одно задание — оно появится здесь.")
            color: Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
        }
        Repeater {
            model: compareSheet.items
            delegate: ListRow {
                id: cand
                required property var modelData
                width: parent.width
                mono: true
                chevron: false
                title: modelData.file
                subtitle: modelData.task + " · " + modelData.folderTitle + (modelData.date ? " · " + modelData.date : "")
                highlighted: { screen.compare; return screen.compareIndex(cand.modelData) >= 0 }
                onClicked: screen.toggleCompare(cand.modelData)
            }
        }
        buttons: [
            TamicButton { implicitWidth: 120; visible: screen.compare.length > 0; text: qsTr("Очистить")
                          onClicked: screen.compare = [] },
            TamicButton { implicitWidth: 120; primary: true; text: qsTr("Готово"); onClicked: compareSheet.close() }
        ]
    }

    // Короткое сообщение внизу (сохранение картинки).
    Rectangle {
        id: toast
        property bool bad: false
        function show(msg, isBad) { toastText.text = msg; bad = isBad; opacity = 1; toastTimer.restart() }
        anchors { horizontalCenter: plotFrame.horizontalCenter; bottom: plotFrame.bottom; bottomMargin: Theme.spacing }
        width: Math.min(plotFrame.width - 2 * Theme.spacing, toastText.implicitWidth + 2 * Theme.gutter)
        height: toastText.implicitHeight + Theme.spacing
        radius: Theme.radiusSm
        color: Theme.surface2
        border.width: Theme.borderW
        border.color: bad ? Theme.danger : Theme.accent
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.durMs } }
        Text {
            id: toastText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, plotFrame.width - 2 * Theme.spacing - 2 * Theme.gutter)
            wrapMode: Text.Wrap
            color: toast.bad ? Theme.danger : Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
        }
        Timer { id: toastTimer; interval: 3500; onTriggered: toast.opacity = 0 }
    }
}

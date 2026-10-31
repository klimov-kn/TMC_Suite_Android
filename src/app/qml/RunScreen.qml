// «Расчёт»: ход (шаг, такт, %), время, журнал ядра, «Остановить».
// Данные — только из Calc (CalcSession) и Calc.runner (KernelRunner): они живут в C++,
// поэтому поворот экрана и пересоздание экрана ход расчёта не теряют.
// Широкий экран (горизонтальное положение): слева ход и кнопки, справа журнал.
// https://www.tamic.ru/docs/platforms/android.html#app
import QtQuick

Item {
    id: screen

    property bool compact: false
    signal back()
    signal showResults()

    readonly property bool twoColumn: width > height && width >= 560
    readonly property real leftWidth: twoColumn ? Math.min(420, (width - Theme.spacing) * 0.5) : width
    readonly property var runner: Calc.runner
    readonly property bool busy: Calc.busy
    readonly property bool done: Calc.phase === "done"
    readonly property int code: Calc.resultCode

    function clock(ms) {
        const s = Math.floor(ms / 1000)
        const h = Math.floor(s / 3600)
        const m = Math.floor((s % 3600) / 60)
        const sec = s % 60
        const two = (v) => (v < 10 ? "0" : "") + v
        return h > 0 ? h + ":" + two(m) + ":" + two(sec) : two(m) + ":" + two(sec)
    }
    readonly property string remaining: {
        const p = runner.progress
        if (!busy || p < 1)
            return "—"
        return "≈ " + clock(runner.elapsedMs * (100 - p) / p)
    }
    readonly property string stage: Calc.phase === "running" ? runner.stage : ""
    readonly property bool preparing: stage === "prepr" || stage === "read" || stage === "grid" || stage === "post"
    readonly property string stageText: stage === "prepr" ? qsTr("Препроцессор разворачивает задание")
                                      : stage === "read" ? qsTr("Шаг %1: чтение топологии, ε и плазмы на сетке").arg(runner.step)
                                      : stage === "grid" ? qsTr("Шаг %1: подготовка сетки к счёту").arg(runner.step)
                                      : stage === "post" ? qsTr("Шаг %1: обработка результатов (S-матрица, ДН, файлы)").arg(runner.step)
                                      : ""
    readonly property string statusText: Calc.phase === "copyIn" ? qsTr("Копирование задания в рабочую папку")
                                       : Calc.phase === "running" ? (preparing ? qsTr("Подготовка") : qsTr("Идёт расчёт"))
                                       : Calc.phase === "copyOut" ? qsTr("Копирование результатов в папку задачи")
                                       : code === 0 ? qsTr("Расчёт закончен")
                                       : code === 1 ? qsTr("Ошибка в задании")
                                       : code === 2 ? qsTr("Ошибка при расчёте")
                                       : code === 3 ? qsTr("Расчёт остановлен")
                                       : qsTr("Расчёта не было")
    readonly property color tone: busy ? Theme.accent
                                : code === 0 ? Theme.ok
                                : code === 3 ? Theme.warn
                                : code > 0 ? Theme.danger : Theme.textMute

    ScreenHeader {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        compact: screen.compact
        backText: qsTr("Задание")
        title: screen.compact && Calc.task ? qsTr("Расчёт · %1 · %2").arg(Calc.task).arg(Calc.mode)
                                           : qsTr("Расчёт")
        subtitle: Calc.task ? qsTr("%1 · ядро %2 · %3").arg(Calc.task).arg(Calc.mode)
                                                       .arg(TaskStore.folderTitle(Calc.folder))
                            : ""
        onBackClicked: screen.back()
    }

    Card {
        id: progressCard
        x: 0
        y: header.height + (screen.compact ? 4 : Theme.spacing / 2)
        width: screen.leftWidth
        padding: screen.compact ? Theme.spacing : Theme.gutter
        spacing: screen.compact ? Theme.spacing / 2 : Theme.spacing

        Item {
            width: parent.width
            height: percent.implicitHeight
            Text {
                id: percent
                anchors.left: parent.left
                text: Math.floor(screen.runner.progress) + " %"
                color: screen.tone
                font.family: Theme.fontMono
                font.pixelSize: screen.compact ? Theme.fontSizeScreen : Theme.fontSizeBig
                font.weight: Font.Bold
            }
            Text {
                anchors { left: percent.right; leftMargin: Theme.spacing; right: parent.right
                          verticalCenter: percent.verticalCenter }
                text: screen.statusText
                color: screen.tone
                font.family: Theme.fontUi
                font.pixelSize: screen.compact ? Theme.fontSizeSmall : Theme.fontSizeBody
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight
            }
        }

        ProgressLine {
            width: parent.width
            value: screen.done && screen.code === 0 ? 100 : screen.runner.progress
            fill: screen.tone
            indeterminate: screen.preparing || Calc.phase === "copyIn" || Calc.phase === "copyOut"
        }

        // Стадия подготовки: проценты стоят, но работа идёт — сказать об этом прямо.
        Rectangle {
            visible: screen.preparing
            width: parent.width
            height: stageColumn.implicitHeight + Theme.spacing
            radius: Theme.radiusSm
            color: Theme.bg2
            border.width: Theme.borderW
            border.color: Theme.line
            Column {
                id: stageColumn
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter
                          leftMargin: Theme.spacing; rightMargin: Theme.spacing }
                spacing: 2
                Text {
                    width: parent.width
                    text: screen.stageText
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                }
                Text {
                    width: parent.width
                    text: (screen.stage === "post"
                           ? qsTr("Такты посчитаны, ядро обрабатывает и записывает результаты.")
                           : qsTr("Проценты пойдут, когда начнутся такты. Большие задания готовятся долго — это нормально, расчёт идёт."))
                          + " " + qsTr("Этап идёт %1.").arg(screen.clock(screen.runner.stageMs))
                    color: Theme.textDim
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeCaption
                    wrapMode: Text.Wrap
                }
            }
        }

        Grid {
            id: grid
            width: parent.width
            columns: screen.compact ? 3 : 2
            rowSpacing: screen.compact ? 4 : Theme.spacing / 2
            columnSpacing: Theme.spacing
            Repeater {
                model: [
                    { k: qsTr("Шаг"),   v: screen.runner.stepCount > 0 ? qsTr("%1 из %2").arg(screen.runner.step).arg(screen.runner.stepCount) : "—" },
                    { k: qsTr("Такт"),  v: screen.runner.tickMax > 0 ? qsTr("%1 из %2").arg(screen.runner.tick).arg(screen.runner.tickMax) : "—" },
                    { k: qsTr("Модельное время"), v: screen.runner.modelTime || "—" },
                    { k: qsTr("Прошло"), v: screen.clock(screen.runner.elapsedMs) },
                    { k: qsTr("Осталось"), v: screen.remaining },
                    { k: qsTr("Результатов"), v: screen.done ? String(Calc.resultCount) : "—" }
                ]
                delegate: Column {
                    id: cell
                    required property var modelData
                    width: (grid.width - grid.columnSpacing * (grid.columns - 1)) / grid.columns
                    spacing: 0
                    Text {
                        width: parent.width
                        text: cell.modelData.k
                        color: Theme.textMute
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fontSizeCaption
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: cell.modelData.v
                        color: Theme.text
                        font.family: Theme.fontMono
                        font.pixelSize: screen.compact ? Theme.fontSizeSmall : Theme.fontSizeBody
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    Rectangle {
        id: logBox
        x: screen.twoColumn ? screen.leftWidth + Theme.spacing : 0
        y: screen.twoColumn ? progressCard.y : progressCard.y + progressCard.height + Theme.spacing
        width: screen.twoColumn ? screen.width - x : screen.width
        height: (screen.twoColumn ? screen.height : actions.y - Theme.spacing) - y
        color: Theme.bg2
        radius: Theme.radius
        border.width: Theme.borderW
        border.color: Theme.line
        clip: true

        Text {
            id: logTitle
            anchors { left: parent.left; top: parent.top; margins: Theme.spacing }
            text: qsTr("Журнал ядра")
            color: Theme.accent
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
            font.weight: Font.Bold
        }
        Flickable {
            id: logFlick
            anchors {
                left: parent.left
                right: parent.right
                top: logTitle.bottom
                bottom: parent.bottom
                margins: Theme.spacing
                topMargin: Theme.spacing / 2
            }
            clip: true
            contentWidth: width
            contentHeight: logText.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            Text {
                id: logText
                width: logFlick.width
                text: screen.runner.logText
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                color: Theme.textDim
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSizeCaption
                onImplicitHeightChanged: logFlick.contentY = Math.max(0, implicitHeight - logFlick.height)
            }
        }
    }

    Row {
        id: actions
        x: 0
        y: screen.height - height
        width: screen.leftWidth
        spacing: Theme.spacing

        TamicButton {
            visible: screen.busy
            width: parent.width
            danger: true
            enabled: Calc.phase === "running"
            text: qsTr("Остановить")
            onClicked: Calc.stop()
        }
        TamicButton {
            visible: !screen.busy
            width: (parent.width - Theme.spacing) / 2
            text: qsTr("К заданию")
            onClicked: screen.back()
        }
        TamicButton {
            visible: !screen.busy
            width: (parent.width - Theme.spacing) / 2
            primary: true
            enabled: Calc.resultCount > 0
            text: qsTr("Результаты")
            onClicked: screen.showResults()
        }
    }
}

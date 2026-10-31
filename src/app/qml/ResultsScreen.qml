// «Результаты»: файлы последнего расчёта задачи в её папке, «Поделиться» и «Удалить».
// Касание файла .s / .DAT / .t открывает график (ChartScreen, этап О3), файла поля
// (.ex, .ex.frames, .AMP, .FAZ, .tt) — поле (FieldScreen, этап О4); оба — поверх списка на
// весь экран. Долгое нажатие на файл — меню: поделиться или удалить один файл.
// «Удалить» внизу — все результаты расчёта (задание и входные файлы остаются).
// В горизонтальном положении правая часть — подсказка, список — слева.
import QtQuick
import Tamic.Viewers

Item {
    id: screen

    property string folder       // пусто — последний расчёт
    property string task
    property bool compact: false
    property string startChart: ""        // параметры запуска (снимки): файл и вид графика
    property string startChartOpts: ""

    // Открытый график: папка, файл и все файлы того же вида из расчёта.
    property string chartFolder: ""
    property string chartFile: ""
    property var chartFiles: []
    property string fieldFile: ""         // открытый файл поля

    function openChart(name) {
        const kind = ViewerTools.chartKind(name)
        if (kind === "field") {
            fieldFile = name
            return
        }
        if (!kind)
            return
        const same = []
        for (let i = 0; i < files.length; ++i)
            if (ViewerTools.chartKind(files[i].name) === kind)
                same.push(files[i].name)
        chartFolder = runFolder
        chartFiles = same
        chartFile = name
    }
    // Назад (кнопка Android): закрыть лист, график или поле. true — обработано.
    function closeChart() {
        if (fileSheet.open) { fileSheet.close(); return true }
        if (deleteSheet.open) { deleteSheet.close(); return true }
        if (chartLoader.item && chartLoader.item.goBack()) return true
        if (fieldFile) { fieldFile = ""; return true }
        if (!chartFile)
            return false
        chartFile = ""
        return true
    }

    function fieldNames() {
        const out = []
        for (let i = 0; i < files.length; ++i)
            if (ViewerTools.chartKind(files[i].name) === "field")
                out.push(files[i].name)
        return out
    }
    function totalSize() {
        let n = 0
        for (let i = 0; i < files.length; ++i)
            n += files[i].size
        return n
    }
    function remove(names) {
        const r = TaskStore.deleteResults(runFolder, runTask, names)
        if (r.failed > 0)
            toast.show(qsTr("Удалено файлов: %1, не удалось: %2").arg(r.deleted).arg(r.failed), true)
        else
            toast.show(qsTr("Удалено файлов: %1 (%2)").arg(r.deleted).arg(TaskStore.formatSize(r.bytes)), false)
    }

    Component.onCompleted: {
        // Снимки руководства: лист удаления или меню файла без касаний.
        if (startChartOpts === "delete")
            deleteSheet.open = true
        else if (startChartOpts === "deletenow")   // проверка удаления без касаний
            remove(names())
        else if (startChartOpts.indexOf("filemenu=") === 0) {
            fileSheet.name = startChartOpts.substring(9)
            fileSheet.sizeText = ""
            fileSheet.open = true
        }
        if (!startChart)
            return
        const slash = Math.max(startChart.lastIndexOf("/"), startChart.lastIndexOf("\\"))
        if (slash >= 0) {   // полный путь (ПК): папка и файл
            chartFolder = startChart.substring(0, slash)
            chartFile = startChart.substring(slash + 1)
            chartFiles = [chartFile]
        } else {
            openChart(startChart)
        }
    }

    readonly property bool twoColumn: width > height && width >= 560
    readonly property real leftWidth: twoColumn ? Math.min(400, (width - Theme.spacing) * 0.45) : width
    readonly property var run: { TaskStore.revision; return TaskStore.lastRun(screen.folder, screen.task) }
    readonly property string runFolder: run.folder || ""
    readonly property string runTask: run.task || ""
    readonly property var files: { TaskStore.revision; return runTask ? TaskStore.results(runFolder, runTask) : [] }
    readonly property bool runBusy: Calc.busy && Calc.folder === screen.runFolder && Calc.task === screen.runTask

    function names() {
        const out = []
        for (let i = 0; i < files.length; ++i)
            out.push(files[i].name)
        return out
    }

    ScreenHeader {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        compact: screen.compact
        title: screen.compact && screen.runTask ? qsTr("Результаты · %1").arg(screen.runTask)
                                                : qsTr("Результаты")
        subtitle: screen.runTask ? qsTr("%1 · %2").arg(screen.runTask).arg(screen.run.folderTitle)
                                 : qsTr("Посчитайте задание — здесь появятся файлы результатов")
    }

    Card {
        id: summary
        visible: screen.runTask.length > 0
        x: 0
        y: header.height + (screen.compact ? 4 : Theme.spacing / 2)
        width: screen.leftWidth
        padding: screen.compact ? Theme.spacing / 2 + 2 : Theme.spacing

        Row {
            spacing: Theme.spacing
            StatusChip {
                anchors.verticalCenter: parent.verticalCenter
                code: screen.run.code !== undefined ? screen.run.code : -1
                running: screen.runBusy
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: screen.run.mode
                      ? qsTr("ядро %1 · %2 с · %3").arg(screen.run.mode)
                            .arg(Lang.number(screen.run.seconds, 1))
                            .arg(screen.run.date)
                      : ""
                color: Theme.textDim
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }

    ListView {
        id: list
        x: 0
        y: (summary.visible ? summary.y + summary.height : header.height) + Theme.spacing
        width: screen.leftWidth
        height: (screen.twoColumn ? buttons.y : viewers.y) - Theme.spacing - y
        clip: true
        spacing: Theme.spacing / 2
        model: screen.files
        delegate: ListRow {
            id: rowItem
            required property var modelData
            readonly property string kind: ViewerTools.chartKind(modelData.name)
            width: ListView.view.width
            mono: true
            chevron: kind.length > 0
            title: modelData.name
            subtitle: kind === "smatrix" ? qsTr("%1 · S-матрица").arg(modelData.sizeText)
                    : kind === "pattern" ? qsTr("%1 · ДН").arg(modelData.sizeText)
                    : kind === "signal" ? qsTr("%1 · сигналы").arg(modelData.sizeText)
                    : kind === "field" ? qsTr("%1 · поле").arg(modelData.sizeText)
                    : modelData.sizeText
            actionText: qsTr("Поделиться")
            onActionClicked: TaskStore.share(screen.runFolder, [rowItem.modelData.name])
            onClicked: screen.openChart(rowItem.modelData.name)
            onPressAndHold: {
                fileSheet.name = rowItem.modelData.name
                fileSheet.sizeText = rowItem.modelData.sizeText
                fileSheet.open = true
            }
        }

        Text {
            anchors.centerIn: parent
            width: parent.width - Theme.gutter * 2
            visible: list.count === 0 && screen.runTask.length > 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("Файлов результатов нет")
            color: Theme.textMute
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeBody
        }
    }

    // Подсказка: что можно сделать с файлами.
    Rectangle {
        id: viewers
        x: screen.twoColumn ? screen.leftWidth + Theme.spacing : 0
        y: screen.twoColumn ? header.height + (screen.compact ? 4 : Theme.spacing / 2)
                            : buttons.y - Theme.spacing - height
        width: screen.twoColumn ? screen.width - x : screen.width
        height: screen.twoColumn ? screen.height - y : viewersText.implicitHeight + Theme.spacing * 2
        color: Theme.surface
        radius: Theme.radius
        border.width: Theme.borderW
        border.color: Theme.line

        Column {
            id: viewersText
            anchors { left: parent.left; right: parent.right; margins: Theme.spacing }
            anchors.verticalCenter: screen.twoColumn ? parent.verticalCenter : undefined
            y: Theme.spacing
            spacing: 4
            Text {
                width: parent.width
                horizontalAlignment: screen.twoColumn ? Text.AlignHCenter : Text.AlignLeft
                text: qsTr("Просмотр")
                color: Theme.accent
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
                font.weight: Font.Bold
            }
            Text {
                width: parent.width
                horizontalAlignment: screen.twoColumn ? Text.AlignHCenter : Text.AlignLeft
                wrapMode: Text.Wrap
                text: qsTr("Нажмите файл: .s — S-матрица, .DAT — ДН, .t — сигналы, .ex / .AMP — поле. Долгое нажатие — поделиться или удалить файл.")
                color: Theme.textMute
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }

    Row {
        id: buttons
        x: 0
        y: screen.height - height
        width: screen.leftWidth
        spacing: Theme.spacing / 2
        TamicButton {
            id: shareAll
            width: parent.width - deleteAll.width - parent.spacing
            implicitWidth: 120
            primary: true
            enabled: screen.files.length > 0
            text: qsTr("Поделиться (%1)").arg(screen.files.length)
            onClicked: TaskStore.share(screen.runFolder, screen.names())
        }
        TamicButton {
            id: deleteAll
            implicitWidth: Math.max(120, implicitContentWidth + leftPadding + rightPadding)
            danger: true
            enabled: screen.files.length > 0 && !screen.runBusy
            text: qsTr("Удалить")
            onClicked: deleteSheet.open = true
        }
    }

    // --- Листы ---------------------------------------------------------------------
    Sheet {
        id: deleteSheet
        title: qsTr("Удалить результаты?")
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr("С телефона будут удалены все файлы результатов расчёта %1: %2 шт., %3. Задание .tpl и его входные файлы останутся — задачу можно посчитать заново.")
                  .arg(screen.runTask).arg(screen.files.length).arg(TaskStore.formatSize(screen.totalSize()))
            color: Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeBody
        }
        buttons: [
            TamicButton { implicitWidth: 130; text: qsTr("Отмена"); onClicked: deleteSheet.close() },
            TamicButton { implicitWidth: 130; danger: true; text: qsTr("Удалить")
                          onClicked: { deleteSheet.close(); screen.remove(screen.names()) } }
        ]
    }

    Sheet {
        id: fileSheet
        property string name: ""
        property string sizeText: ""
        property bool confirm: false
        title: name
        onOpenChanged: if (open) confirm = false
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: fileSheet.confirm ? qsTr("Удалить файл %1 (%2) с телефона?").arg(fileSheet.name).arg(fileSheet.sizeText)
                                    : fileSheet.sizeText
            color: fileSheet.confirm ? Theme.text : Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeBody
        }
        buttons: [
            TamicButton { text: fileSheet.confirm ? qsTr("Отмена") : qsTr("Поделиться")
                          onClicked: {
                              if (fileSheet.confirm) { fileSheet.close(); return }
                              fileSheet.close()
                              TaskStore.share(screen.runFolder, [fileSheet.name])
                          } },
            TamicButton { danger: true; text: qsTr("Удалить")
                          enabled: !screen.runBusy
                          onClicked: {
                              if (!fileSheet.confirm) { fileSheet.confirm = true; return }
                              fileSheet.close()
                              screen.remove([fileSheet.name])
                          } }
        ]
    }

    // Короткое сообщение внизу (удаление).
    Rectangle {
        id: toast
        property bool bad: false
        function show(msg, isBad) { toastText.text = msg; bad = isBad; opacity = 1; toastTimer.restart() }
        anchors { horizontalCenter: list.horizontalCenter; bottom: buttons.top; bottomMargin: Theme.spacing }
        width: Math.min(list.width - 2 * Theme.spacing, toastText.implicitWidth + 2 * Theme.gutter)
        height: toastText.implicitHeight + Theme.spacing
        radius: Theme.radiusSm
        color: Theme.surface2
        border.width: Theme.borderW
        border.color: bad ? Theme.danger : Theme.accent
        opacity: 0
        visible: opacity > 0
        z: 20
        Behavior on opacity { NumberAnimation { duration: Theme.durMs } }
        Text {
            id: toastText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, list.width - 2 * Theme.spacing - 2 * Theme.gutter)
            wrapMode: Text.Wrap
            color: toast.bad ? Theme.danger : Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
        }
        Timer { id: toastTimer; interval: 3500; onTriggered: toast.opacity = 0 }
    }

    // График поверх списка (один элемент на всё время показа: поворот его не пересоздаёт).
    Rectangle {
        anchors.fill: parent
        visible: screen.chartFile.length > 0
        color: Theme.bg
        z: 30
        MouseArea { anchors.fill: parent }   // касания не проходят к списку под графиком
        Loader {
            id: chartLoader
            anchors.fill: parent
            active: screen.chartFile.length > 0
            sourceComponent: ChartScreen {
                folder: screen.chartFolder
                file: screen.chartFile
                files: screen.chartFiles
                task: screen.runTask
                compact: screen.compact
                options: screen.startChartOpts
                onBack: screen.closeChart()
            }
        }
    }

    // Поле поверх списка.
    Rectangle {
        anchors.fill: parent
        visible: screen.fieldFile.length > 0
        color: Theme.bg
        z: 30
        MouseArea { anchors.fill: parent }
        Loader {
            anchors.fill: parent
            active: screen.fieldFile.length > 0
            sourceComponent: FieldScreen {
                folder: screen.runFolder
                task: screen.runTask
                file: screen.fieldFile
                names: screen.fieldNames()
                compact: screen.compact
                options: screen.startChartOpts
                onBack: screen.fieldFile = ""
            }
        }
    }
}

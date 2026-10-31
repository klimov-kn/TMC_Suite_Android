// Главное окно TAMIC: экраны и вкладки (Задачи · TPL Designer · Результаты · Справка).
// Вкладка «Задачи» — цепочка: папки -> задания папки -> задание -> расчёт.
//
// Поворот экрана (ориентация в манифесте не фиксирована):
//   * вертикально — вкладки внизу, экраны по одному;
//   * горизонтально — вкладки боковой полосой слева; на «Задачах» список (папки или задания)
//     слева, задание справа; «Расчёт» и «Результаты» — в две колонки; шапки компактные.
// Состояние (открытая папка и задание, вкладка) — свойства этого окна, ход расчёта — в C++
// (Calc), поэтому поворот ничего не теряет (Android не пересоздаёт окно: configChanges).
// Цвета, шрифты и размеры — только через Theme.
// https://www.tamic.ru/docs/platforms/android.html#app
import QtQuick

Window {
    id: root

    // Параметры запуска (main.cpp): экран и пример для снимков и самопроверки.
    property string startScreen: ""
    property string startTask: ""
    property string startChart: ""        // график для снимков: файл и вид (ResultsScreen)
    property string startChartOpts: ""

    property int tab: 0                   // 0 — Задачи, 1 — TPL Designer, 2 — Результаты, 3 — Справка
    property string page: "folders"       // folders · tasks · task · run
    property string curFolder: ""
    property string curTask: ""
    property string resFolder: ""         // задача на вкладке «Результаты» (пусто — последний расчёт)
    property string resTask: ""
    property string designerFolder: ""    // папка, куда TPL Designer пишет задание

    readonly property bool landscape: safe.width > safe.height
    readonly property bool compact: safe.height < Theme.compactHeight

    width: 412          // логическая ширина типичного телефона — для настольной отладки
    height: 860
    visible: true
    title: "TMC Suite"
    color: Theme.bg

    function exampleTpl(name) {
        return name === "voln" ? "VOLN.TPL"
             : name === "horn" ? "horn.tpl"
             : name === "plasma_nby" ? "plasma_nby.tpl"
             : name === "voln_field" ? "VFIELD.TPL"
             : name === "eps4" ? "eps4.tpl"
             : name.indexOf("xr_") === 0 || name.indexOf("lens") === 0 ? name + ".tpl" : ""
    }

    function openFolder(folder) { curFolder = folder; curTask = ""; page = "tasks"; tab = 0 }
    function openTask(name) { curTask = name; page = "task"; tab = 0 }
    function showRun() { tab = 0; page = "run" }
    function showResults() { resFolder = Calc.folder; resTask = Calc.task; tab = 2 }

    // Назад (кнопка «назад» Android): true — обработано внутри приложения.
    function goBack() {
        if (tab === 1 && designerLoader.item && designerLoader.item.goBack()) return true
        if (tab === 2 && resultsLoader.item && resultsLoader.item.closeChart()) return true
        if (tab === 3 && aboutLoader.item && aboutLoader.item.goBack()) return true
        if (tab !== 0) { tab = 0; return true }
        if (page === "run") { page = curTask ? "task" : "folders"; return true }
        if (page === "task") { page = "tasks"; return true }
        if (page === "tasks") { page = "folders"; return true }
        return false
    }

    // График из параметров запуска открывается один раз.
    onTabChanged: if (tab !== 2) startChart = ""

    Component.onCompleted: {
        const ex = exampleTpl(startTask)
        if (ex) {
            curFolder = TaskStore.examplesFolder
            curTask = ex
        }
        switch (startScreen) {
        case "tasks":    page = curFolder ? "tasks" : "folders"; break
        case "task":     page = curTask ? "task" : "folders"; break
        case "run":      page = "run"; break
        case "results":  resFolder = curFolder; resTask = curTask; tab = 2; break
        case "designer": tab = 1; break
        case "about":    tab = 3; break
        case "manual":   startManual = true; tab = 3; break
        default:         page = "folders"
        }
    }
    property bool startManual: false

    // Начался расчёт (в том числе самопроверка) — показать его ход.
    Connections {
        target: Calc
        function onStateChanged() {
            if (Calc.phase === "copyIn") {
                root.curFolder = Calc.folder
                root.curTask = Calc.task
                root.page = "run"
                if (root.startScreen !== "results" && root.startScreen !== "designer"
                    && root.startScreen !== "about" && root.startScreen !== "manual")
                    root.tab = 0
            }
        }
    }

    Connections {
        target: TaskStore
        function onFolderAdded(folder) { root.openFolder(folder) }
    }

    Loader {
        id: folderPicker
        active: !TaskStore.nativeFolderPicker
        source: "FolderPicker.qml"
    }

    Item {
        id: safe
        anchors {
            fill: parent
            topMargin: root.contentItem.SafeArea.margins.top
            bottomMargin: root.contentItem.SafeArea.margins.bottom
            leftMargin: root.contentItem.SafeArea.margins.left
            rightMargin: root.contentItem.SafeArea.margins.right
        }
        focus: true
        Keys.onReleased: (event) => {
            if (event.key === Qt.Key_Back && root.goBack())
                event.accepted = true
        }

        BottomTabs {
            id: tabs
            vertical: root.landscape
            x: 0
            y: root.landscape ? 0 : safe.height - height
            width: root.landscape ? Theme.railWidth : safe.width
            height: root.landscape ? safe.height : Theme.tabBarHeight
            current: root.tab
            onSelected: (index) => {
                if (index === 2 && root.tab !== 2) {
                    // «Результаты» — задачи, открытой на вкладке «Задачи», если она
                    // считалась; иначе последнего расчёта.
                    const open = root.page === "task" || root.page === "run"
                    root.resFolder = open ? root.curFolder : ""
                    root.resTask = open ? root.curTask : ""
                }
                root.tab = index
            }
        }

        Item {
            id: content
            readonly property int gap: root.compact ? Theme.spacing : Theme.gutter
            x: (root.landscape ? tabs.width : 0) + Theme.gutter
            y: root.compact ? Theme.spacing / 2 : 0
            width: safe.width - x - Theme.gutter
            height: (root.landscape ? safe.height : tabs.y) - y - gap

            // Ширина списка слева в горизонтальном положении.
            readonly property real masterWidth: Math.min(Theme.masterWidth, width * 0.42)
            readonly property bool split: root.landscape && root.page !== "run"
            readonly property bool showMaster: root.tab === 0 && root.page !== "run"
                                               && (split || root.page !== "task")
            readonly property bool showDetail: root.tab === 0 && root.page !== "run"
                                               && (split || root.page === "task")

            // --- Список: папки или задания папки ---------------------------------
            Item {
                id: masterPane
                visible: content.showMaster
                width: content.split ? content.masterWidth : content.width
                height: content.height

                FoldersScreen {
                    anchors.fill: parent
                    visible: root.page === "folders"
                    compact: root.compact
                    onOpenFolder: (folder) => root.openFolder(folder)
                    onPickFolder: {
                        if (TaskStore.nativeFolderPicker)
                            TaskStore.pickFolder()
                        else if (folderPicker.item)
                            folderPicker.item.open()
                    }
                }
                Loader {
                    anchors.fill: parent
                    active: root.page === "tasks" || root.page === "task"
                    sourceComponent: TasksScreen {
                        folder: root.curFolder
                        currentTask: content.split ? root.curTask : ""
                        compact: root.compact
                        onBack: { root.curTask = ""; root.page = "folders" }
                        onOpenTask: (name) => root.openTask(name)
                    }
                }
            }

            // --- Детали: задание (или подсказка в горизонтальном положении) ---------
            Item {
                id: detailPane
                visible: content.showDetail
                x: content.split ? content.masterWidth + Theme.gutter : 0
                width: content.width - x
                height: content.height

                Loader {
                    anchors.fill: parent
                    active: root.page === "task"
                    sourceComponent: TaskScreen {
                        folder: root.curFolder
                        task: root.curTask
                        compact: root.compact
                        showBack: !content.split
                        onBack: root.page = "tasks"
                        onShowRun: root.showRun()
                    }
                }
                Card {
                    visible: root.page !== "task"
                    anchors.centerIn: parent
                    width: Math.min(parent.width, 420)
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        text: root.page === "folders" ? qsTr("Выберите папку с заданиями слева")
                                                      : qsTr("Выберите задание слева")
                        color: Theme.textMute
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fontSizeBody
                    }
                }
            }

            // Экран расчёта один на всё время работы: поворот и переходы его не пересоздают.
            RunScreen {
                anchors.fill: parent
                visible: root.tab === 0 && root.page === "run"
                compact: root.compact
                onBack: root.page = root.curTask ? "task" : "folders"
                onShowResults: root.showResults()
            }
            Loader {
                id: designerLoader
                anchors.fill: parent
                active: root.tab === 1
                sourceComponent: DesignerScreen {
                    compact: root.compact
                    startOpts: root.startChartOpts
                    // Папка: выбранная в конструкторе, иначе открытая на «Задачах», иначе «Примеры».
                    folder: root.designerFolder || root.curFolder || TaskStore.examplesFolder
                    onFolderPicked: (folder) => root.designerFolder = folder
                    onOpenTask: (folder, name) => { root.openFolder(folder); root.openTask(name) }
                }
            }
            Loader {
                id: aboutLoader
                anchors.fill: parent
                active: root.tab === 3
                sourceComponent: AboutScreen {
                    compact: root.compact
                    startManual: root.startManual
                    startSection: root.startChartOpts.indexOf("section=") === 0
                                  ? parseInt(root.startChartOpts.substring(8)) : -1
                }
            }
            Loader {
                id: resultsLoader
                anchors.fill: parent
                active: root.tab === 2
                sourceComponent: ResultsScreen {
                    folder: root.resFolder
                    task: root.resTask
                    compact: root.compact
                    startChart: root.startChart
                    startChartOpts: root.startChartOpts
                }
            }
        }
    }
}

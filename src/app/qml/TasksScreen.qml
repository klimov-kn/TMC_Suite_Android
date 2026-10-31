// «Задачи», второй уровень: задания .tpl выбранной папки, статус последнего расчёта.
import QtQuick

Item {
    id: screen

    property string folder
    property string currentTask: ""      // открытое задание (подсветка в горизонтальном положении)
    property bool compact: false
    signal back()
    signal openTask(string name)

    readonly property var taskList: { TaskStore.revision; return TaskStore.tasks(screen.folder) }

    ScreenHeader {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        compact: screen.compact
        backText: qsTr("Задачи")
        title: TaskStore.folderTitle(screen.folder)
        subtitle: screen.taskList.length === 0 ? qsTr("В папке нет файлов .tpl")
                                               : qsTr("Заданий: %1").arg(screen.taskList.length)
        onBackClicked: screen.back()
    }

    ListView {
        anchors {
            left: parent.left
            right: parent.right
            top: header.bottom
            topMargin: Theme.spacing
            bottom: parent.bottom
        }
        clip: true
        spacing: Theme.spacing / 2
        model: screen.taskList
        delegate: ListRow {
            id: rowItem
            required property var modelData
            width: ListView.view.width
            mono: true
            highlighted: screen.currentTask === modelData.name
            title: modelData.name
            subtitle: modelData.sizeText
                      + (modelData.lastMode ? qsTr(" · считалась ядром %1").arg(modelData.lastMode) : "")
            onClicked: screen.openTask(rowItem.modelData.name)
            StatusChip {
                code: rowItem.modelData.lastCode
                running: Calc.busy && Calc.folder === screen.folder && Calc.task === rowItem.modelData.name
            }
        }
    }
}

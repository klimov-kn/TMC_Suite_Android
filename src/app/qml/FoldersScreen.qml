// «Задачи», первый уровень: папки с заданиями (решение M16).
// «Примеры» — встроенные задания; ниже — папки, выбранные пользователем.
import QtQuick

Item {
    id: screen

    property bool compact: false
    signal openFolder(string folder)
    signal pickFolder()

    ScreenHeader {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        compact: screen.compact
        title: qsTr("Задачи")
        subtitle: qsTr("Папки с заданиями .tpl. Результаты расчёта записываются в ту же папку.")
    }

    ListView {
        id: list
        anchors {
            left: parent.left
            right: parent.right
            top: header.bottom
            topMargin: Theme.spacing
            bottom: pickButton.top
            bottomMargin: Theme.spacing
        }
        clip: true
        spacing: Theme.spacing / 2
        model: TaskStore.folders
        delegate: ListRow {
            id: rowItem
            required property var modelData
            width: ListView.view.width
            title: modelData.title
            subtitle: modelData.available ? modelData.subtitle
                                          : qsTr("Нет доступа — выберите папку заново")
            actionText: modelData.kind === "examples" ? "" : qsTr("Убрать")
            actionDanger: true
            onClicked: if (rowItem.modelData.available) screen.openFolder(rowItem.modelData.id)
            onActionClicked: TaskStore.forgetFolder(rowItem.modelData.id)
        }
    }

    TamicButton {
        id: pickButton
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        primary: true
        text: qsTr("Выбрать папку")
        onClicked: screen.pickFolder()
    }
}

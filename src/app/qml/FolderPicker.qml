// Выбор папки задач на настольной сборке (на Android — системное окно SAF,
// TaskStore.pickFolder()).
import QtQuick
import QtQuick.Dialogs

FolderDialog {
    title: qsTr("Папка с заданиями .tpl")
    onAccepted: TaskStore.addLocalFolder(selectedFolder)
}

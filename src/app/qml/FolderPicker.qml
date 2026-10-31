// Выбор папки задач на настольной сборке (на Android — системное окно SAF,
// TaskStore.pickFolder()).
// https://www.tamic.ru/docs/platforms/android.html#app
import QtQuick
import QtQuick.Dialogs

FolderDialog {
    title: qsTr("Папка с заданиями .tpl")
    onAccepted: TaskStore.addLocalFolder(selectedFolder)
}

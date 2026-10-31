// «TPL Designer» — встроенный конструктор заданий (решение M17, модуль TamicTplDesigner).
// Сверху — папка задачи, куда пишется задание (её можно сменить); ниже — страница
// конструктора. После записи можно сразу перейти к заданию на вкладке «Задачи».
import QtQuick
import TamicTplDesigner

Item {
    id: screen

    property bool compact: false
    property string folder: ""             // папка задачи (M16)
    property string startOpts: ""          // параметры запуска (снимки): sample=<id>, step=<0..4>
    signal folderPicked(string folder)
    signal openTask(string folder, string name)

    // Назад (кнопка Android): листы и шаги конструктора. true — обработано.
    function goBack() {
        if (folderSheet.open) { folderSheet.close(); return true }
        if (savedBar.visible) { savedBar.name = ""; return true }
        return page.goBack()
    }

    // Снимки руководства: открыть образец и шаг без касаний.
    Timer {
        interval: 200
        running: screen.startOpts.length > 0
        onTriggered: {
            const list = screen.startOpts.split(",")
            for (let i = 0; i < list.length; ++i) {
                const o = list[i].trim()
                if (o.indexOf("sample=") === 0) { page.designer.openSample(o.substring(7)); page.geometry.fit() }
                else if (o.indexOf("step=") === 0) page.step = parseInt(o.substring(5))
            }
        }
    }

    // --- Папка задачи ----------------------------------------------------------
    Rectangle {
        id: folderBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        anchors.topMargin: screen.compact ? 2 : Theme.spacing / 2
        height: Theme.touchMin
        radius: Theme.radiusSm
        color: folderArea.pressed ? Theme.surface2 : Theme.surface
        border.width: Theme.borderW
        border.color: folderArea.pressed ? Theme.accent : Theme.line
        Text {
            anchors { left: parent.left; leftMargin: Theme.spacing; right: change.left; rightMargin: Theme.spacing
                      verticalCenter: parent.verticalCenter }
            text: qsTr("Папка: %1").arg(screen.folder ? TaskStore.folderTitle(screen.folder) : qsTr("не выбрана"))
            color: screen.folder ? Theme.text : Theme.warn
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
            font.weight: Font.DemiBold
            elide: Text.ElideMiddle
        }
        Text {
            id: change
            anchors { right: parent.right; rightMargin: Theme.spacing; verticalCenter: parent.verticalCenter }
            text: qsTr("Сменить ›")
            color: Theme.accent
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
            font.weight: Font.DemiBold
        }
        MouseArea {
            id: folderArea
            anchors.fill: parent
            onClicked: folderSheet.open = true
        }
    }

    TplDesignerPage {
        id: page
        anchors { left: parent.left; right: parent.right; top: folderBar.bottom; bottom: parent.bottom }
        anchors.topMargin: Theme.spacing / 2
        taskFolder: screen.folder
        exporter: TaskStore
        onSavedTask: (folder, name) => {
            TaskStore.refresh()
            savedBar.folder = folder
            savedBar.name = name
        }
    }

    // Задание записано — перейти к нему.
    Rectangle {
        id: savedBar
        property string folder: ""
        property string name: ""
        visible: name.length > 0
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: Theme.touchMin + Theme.spacing
        radius: Theme.radiusSm
        color: Theme.surface2
        border.width: Theme.borderW
        border.color: Theme.accent
        z: 50
        Text {
            anchors { left: parent.left; leftMargin: Theme.spacing; right: openTask.left; rightMargin: Theme.spacing
                      verticalCenter: parent.verticalCenter }
            text: qsTr("Создано: %1").arg(savedBar.name)
            color: Theme.text
            font.family: Theme.fontMono
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideMiddle
        }
        TamicButton {
            id: openTask
            anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
            implicitWidth: 140
            height: Theme.touchMin
            primary: true
            text: qsTr("Открыть")
            onClicked: {
                const f = savedBar.folder, n = savedBar.name
                savedBar.name = ""
                screen.openTask(f, n)
            }
        }
    }

    Sheet {
        id: folderSheet
        title: qsTr("Папка для задания")
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr("Задание и его файлы (.h, .sig, .eps) запишутся в выбранную папку. Новую папку добавляют на вкладке «Задачи».")
            color: Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
        }
        Repeater {
            model: TaskStore.folders
            delegate: ListRow {
                required property var modelData
                width: parent.width
                title: modelData.title
                subtitle: modelData.subtitle
                chevron: false
                highlighted: modelData.id === screen.folder
                enabled: modelData.available
                onClicked: {
                    screen.folderPicked(modelData.id)
                    folderSheet.close()
                }
            }
        }
        buttons: [
            TamicButton { text: qsTr("Закрыть"); onClicked: folderSheet.close() }
        ]
    }
}

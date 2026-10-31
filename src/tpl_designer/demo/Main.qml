// Окно настольной проверки: страница TplDesignerPage во весь размер «телефона».
import QtQuick
import TamicTplDesigner

Window {
    id: win

    property string folder: ""
    readonly property alias page: page

    width: 360
    height: 780
    visible: true
    title: qsTr("TPL Designer — проверка")

    TplDesignerPage {
        id: page
        anchors.fill: parent
        taskFolder: win.folder
        onSaved: (tplPath) => console.log("saved:", tplPath)
    }
}

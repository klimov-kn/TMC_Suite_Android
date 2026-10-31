// Шаг 3 «Объекты»: фигуры (материалы, eps, плазма) и порты (источники) списком.
// Вертикально: список, касание открывает свойства отдельным экраном. Горизонтально:
// список слева, свойства справа (список — подробность).
// https://www.tamic.ru/docs/platforms/android.html#designer
pragma ComponentBehavior: Bound
import QtQuick
import TamicTplTheme

Item {
    id: page

    required property DesignerModel model
    property bool landscape: false
    property bool editorOpen: false
    signal next()

    function openItem(id) {
        page.model.select(id)
        if (!page.landscape)
            page.editorOpen = true
    }

    Flickable {
        id: listFlick
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        width: page.landscape ? page.width * 0.45 : page.width
        clip: true
        contentHeight: listColumn.implicitHeight + 2 * Theme.gutter
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: listColumn
            x: Theme.gutter
            y: Theme.gutter
            width: listFlick.width - 2 * Theme.gutter
            spacing: Theme.spacing

            TCard {
                title: qsTr("Фигуры (%1)").arg(page.model.shapes.length)
                spacing: 6
                TNote {
                    visible: page.model.shapes.length === 0
                    text: qsTr("Фигур пока нет — нарисуйте их на шаге «Геометрия».")
                }
                Repeater {
                    model: page.model.shapes
                    delegate: ObjectRow {
                        required property var modelData
                        title: modelData.name
                        subtitle: modelData.what
                        swatch: modelData.plasma ? Theme.accent3
                              : modelData.material === "magnetic" ? Theme.accent2
                              : modelData.material === "absorber" ? Theme.warn
                              : modelData.material === "dielectric" ? Theme.accent : Theme.text
                        selected: page.model.selectedId === modelData.id
                        onClicked: page.openItem(modelData.id)
                    }
                }
            }

            TCard {
                title: qsTr("Порты — источники и приёмники (%1)").arg(page.model.ports.length)
                spacing: 6
                TNote {
                    visible: page.model.ports.length === 0
                    text: qsTr("Портов нет. Протяните порт вдоль границы на холсте или добавьте кнопкой ниже.")
                }
                Repeater {
                    model: page.model.ports
                    delegate: ObjectRow {
                        required property var modelData
                        title: modelData.name
                        subtitle: modelData.what
                        swatch: modelData.active ? Theme.ok : Theme.danger
                        selected: page.model.selectedId === modelData.id
                        onClicked: page.openItem(modelData.id)
                    }
                }
                Grid {
                    width: parent.width
                    columns: 2
                    columnSpacing: Theme.spacing / 2
                    rowSpacing: Theme.spacing / 2
                    TButton { width: (parent.width - Theme.spacing / 2) / 2; compact: true; text: qsTr("+ Порт слева")
                              onClicked: page.openItem(page.model.addPort("left")) }
                    TButton { width: (parent.width - Theme.spacing / 2) / 2; compact: true; text: qsTr("+ Порт справа")
                              onClicked: page.openItem(page.model.addPort("right")) }
                }
            }

            TButton {
                width: parent.width
                primary: true
                text: qsTr("Далее: вывод")
                onClicked: page.next()
            }
        }
    }

    // --- свойства: справа (горизонтально) или отдельным экраном (вертикально) ---
    Rectangle {
        id: detail
        visible: page.landscape || page.editorOpen
        anchors {
            left: page.landscape ? listFlick.right : parent.left
            right: parent.right; top: parent.top; bottom: parent.bottom
        }
        color: page.landscape ? Theme.surface : Theme.bg
        z: 5
        MouseArea { anchors.fill: parent }
        Rectangle { visible: page.landscape; anchors { left: parent.left; top: parent.top; bottom: parent.bottom } width: Theme.borderW; color: Theme.line }

        Item {
            id: detailHeader
            visible: !page.landscape
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: visible ? Theme.touchMin + 8 : 0
            Row {
                anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
                TToolButton { icon: "back"; onClicked: page.editorOpen = false }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Объекты")
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeH2
                    font.weight: Font.Bold
                }
            }
        }
        Flickable {
            anchors { left: parent.left; right: parent.right; top: detailHeader.bottom; bottom: parent.bottom }
            anchors.margins: Theme.gutter
            clip: true
            contentHeight: detailEditor.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            SelectionEditor {
                id: detailEditor
                width: parent.width
                model: page.model
            }
        }
    }
}

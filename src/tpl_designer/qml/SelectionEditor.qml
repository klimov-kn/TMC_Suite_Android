// Свойства выделенного объекта: фигуры или порта, с действиями «Копия» и «Удалить».
// Один и тот же компонент живёт в боковой панели (горизонтально) и на отдельном
// экране (вертикально): состояние — в модели, поворот телефона его не теряет.
import QtQuick
import TamicTplTheme

Column {
    id: sel

    required property DesignerModel model
    readonly property string type: model.selected.type || ""

    width: parent ? parent.width : 300
    spacing: Theme.spacing

    Text {
        visible: sel.type.length === 0
        width: parent.width
        wrapMode: Text.Wrap
        text: qsTr("Ничего не выбрано. Коснитесь фигуры или порта на холсте либо в списке объектов.")
        color: Theme.textMute
        font.family: Theme.fontUi
        font.pixelSize: Theme.fontSizeBody
    }

    Row {
        visible: sel.type.length > 0
        width: parent.width
        spacing: Theme.spacing / 2
        TButton {
            compact: true
            width: (parent.width - Theme.spacing / 2) / 2
            text: qsTr("Копия")
            onClicked: sel.model.duplicateSelected()
        }
        TButton {
            compact: true
            danger: true
            width: (parent.width - Theme.spacing / 2) / 2
            text: qsTr("Удалить")
            onClicked: sel.model.deleteSelected()
        }
    }

    ShapeEditor {
        visible: sel.type === "shape"
        model: sel.model
    }
    PortEditor {
        visible: sel.type === "port"
        model: sel.model
    }
}

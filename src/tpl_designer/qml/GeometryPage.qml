// Шаг 2 «Геометрия»: холст во весь экран, инструменты, свойства выделенного.
//
// Вертикально: холст сверху, под ним подсказка и ряд инструментов; свойства —
// отдельным экраном («Свойства»). Горизонтально: инструменты колонкой слева, холст —
// большая часть экрана, свойства — боковой панелью справа. Холст один и тот же при
// любом положении: поворот не сбрасывает ни масштаб, ни выделение.
// https://www.tamic.ru/docs/platforms/android.html#designer
pragma ComponentBehavior: Bound
import QtQuick
import TamicTplTheme

Item {
    id: page

    required property DesignerModel model
    property bool landscape: false
    property bool editorOpen: false
    readonly property alias canvas: cv
    readonly property string selType: model.selected.type || ""
    readonly property int groupSize: model.selectedIds.length
    signal next()

    function fit() { cv.zoomToFit() }

    readonly property var tools: [
        { key: "pan",     icon: "pan",     label: qsTr("Обзор") },
        { key: "select",  icon: "select",  label: qsTr("Выбор") },
        { key: "rect",    icon: "rect",    label: qsTr("Прям.") },
        { key: "circle",  icon: "circle",  label: qsTr("Круг") },
        { key: "line",    icon: "line",    label: qsTr("Линия") },
        { key: "contour", icon: "contour", label: qsTr("Контур") },
        { key: "arc",     icon: "arc",     label: qsTr("Дуга") },
        { key: "port",    icon: "port",    label: qsTr("Порт") }
    ]

    function addByNumbers(kind) {
        addSheet.open = false
        const c = cv.viewCenterMm()
        if (kind.startsWith("port:"))
            page.model.addPort(kind.substring(5))
        else
            page.model.addShape(kind, c.x, c.y, cv.viewSizeMm())
        cv.tool = "select"
        if (!page.landscape)
            page.editorOpen = true
    }

    // --- колонка инструментов (горизонтально) ---
    Rectangle {
        id: toolColumn
        visible: page.landscape
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        width: visible ? 2 * Theme.touchMin + 20 : 0
        color: Theme.surface
        Flickable {
            anchors.fill: parent
            contentHeight: toolCol.height + 16
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            // Горизонтально высоты мало: значки без подписей, в две колонки.
            Grid {
                id: toolCol
                x: 8
                y: 8
                columns: 2
                spacing: 4
                Repeater {
                    model: page.tools
                    delegate: TToolButton {
                        required property var modelData
                        width: Theme.touchMin
                        icon: modelData.icon
                        checked: cv.tool === modelData.key
                        onClicked: cv.tool = modelData.key
                    }
                }
            }
        }
    }

    // --- верхняя панель над холстом ---
    Rectangle {
        id: topBar
        anchors {
            left: toolColumn.right
            right: page.landscape ? side.left : parent.right
            top: parent.top
        }
        height: Theme.touchMin + 8
        color: Theme.bg
        Row {
            anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
            spacing: 2
            TToolButton { icon: "undo"; enabled: page.model.canUndo; onClicked: page.model.undo() }
            TToolButton { icon: "redo"; enabled: page.model.canRedo; onClicked: page.model.redo() }
            TToolButton { icon: "fit"; onClicked: cv.zoomToFit() }
            TToolButton { icon: "grid"; checked: cv.gridVisible; onClicked: cv.gridVisible = !cv.gridVisible }
            TToolButton { icon: "snap"; checked: cv.snapEnabled; onClicked: cv.snapEnabled = !cv.snapEnabled }
            TToolButton { icon: "multi"; checked: cv.multiSelect; onClicked: cv.multiSelect = !cv.multiSelect }
        }
        TButton {
            anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            compact: true
            text: qsTr("+ Числами")
            onClicked: addSheet.open = true
        }
    }

    // --- холст ---
    GeometryCanvas {
        id: cv
        anchors {
            left: toolColumn.right
            right: page.landscape ? side.left : parent.right
            top: topBar.bottom
            bottom: hintBar.top
        }
        model: page.model
        colors: ({
            bg: Theme.bg, grid: Theme.line, box: Theme.textMute, boxFill: Theme.surface,
            text: Theme.text, metal: Theme.text, magnetic: Theme.accent2, absorber: Theme.warn,
            dielectric: Theme.accent, plasma: Theme.accent3, plasmaB: Theme.accent2,
            plasmaY: Theme.textDim, active: Theme.ok, passive: Theme.danger,
            select: Theme.accent, snap: Theme.accent3
        })
        onEditRequested: if (!page.landscape) page.editorOpen = true
    }

    // --- подсказка и кнопки ломаной (в горизонтальном — под холстом) ---
    Rectangle {
        id: hintBar
        anchors {
            left: toolColumn.right
            right: page.landscape ? side.left : parent.right
            bottom: page.landscape ? parent.bottom : bottomArea.top
        }
        height: Math.max(Theme.touchMin, hintColumn.implicitHeight + 8)
        color: Theme.bg
        Column {
            id: hintColumn
            anchors {
                left: parent.left; leftMargin: Theme.gutter
                right: chainButtons.left; rightMargin: 8
                verticalCenter: parent.verticalCenter
            }
            Text {
                width: parent.width
                text: cv.cursorText.length > 0 ? cv.cursorText : " "
                color: Theme.accent
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSizeCaption
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: cv.hint
                color: Theme.textMute
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeCaption
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }
        Row {
            id: chainButtons
            anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            spacing: 4
            width: implicitWidth
            TButton { compact: true; visible: cv.chainCount > 0; text: qsTr("Отмена"); onClicked: cv.cancelDrawing() }
            TButton { compact: true; primary: true; text: qsTr("Готово"); visible: cv.chainCount > 0 && cv.tool !== "arc"; onClicked: cv.finishChain() }
            TButton { compact: true; primary: true; text: qsTr("Далее"); visible: cv.chainCount === 0; onClicked: page.next() }
        }
    }

    // --- низ (вертикально): инструменты и выделенное ---
    Column {
        id: bottomArea
        visible: !page.landscape
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: visible ? implicitHeight : 0

        Rectangle {
            id: toolRow
            width: parent.width
            height: Theme.touchMin + 20
            color: Theme.surface
            Row {
                anchors.centerIn: parent
                spacing: 0
                Repeater {
                    model: page.tools
                    delegate: TToolButton {
                        required property var modelData
                        width: Math.max(Theme.touchMin, (page.width - 16) / page.tools.length)
                        icon: modelData.icon
                        text: modelData.label
                        checked: cv.tool === modelData.key
                        onClicked: cv.tool = modelData.key
                    }
                }
            }
        }

        Rectangle {
            width: parent.width
            height: visible ? Theme.touchMin + 16 : 0
            visible: page.selType.length > 0 || page.groupSize > 1
            color: Theme.surface2
            Text {
                anchors {
                    left: parent.left; leftMargin: Theme.gutter
                    right: selButtons.left; rightMargin: 8
                    verticalCenter: parent.verticalCenter
                }
                text: page.groupSize > 1 ? qsTr("Выбрано: %1").arg(page.groupSize)
                      : (page.model.selected.name || "") + "  ·  " + (page.model.selected.what || page.model.selected.kindText || "")
                color: Theme.text
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideRight
            }
            Row {
                id: selButtons
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                spacing: 4
                TButton { compact: true; danger: true; text: qsTr("Удалить"); onClicked: page.model.deleteSelected() }
                TButton { compact: true; visible: page.groupSize > 1; text: qsTr("Копия"); onClicked: page.model.duplicateSelected() }
                TButton { compact: true; visible: page.groupSize > 1; text: qsTr("Соединить"); onClicked: mergeNote.text = page.model.mergeSelected() }
                TButton { compact: true; visible: page.groupSize <= 1; text: qsTr("Свойства"); onClicked: page.editorOpen = true }
            }
        }
    }

    // --- боковая панель (горизонтально): свойства выделенного ---
    Rectangle {
        id: side
        visible: page.landscape
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        width: visible ? Math.min(400, Math.max(300, page.width * 0.38)) : 0
        color: Theme.surface
        Rectangle { anchors { left: parent.left; top: parent.top; bottom: parent.bottom } width: Theme.borderW; color: Theme.line }
        Flickable {
            anchors.fill: parent
            anchors.margins: Theme.gutter
            clip: true
            contentHeight: sideColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            Column {
                id: sideColumn
                width: parent.width
                spacing: Theme.spacing
                Text {
                    width: parent.width
                    text: page.selType === "shape" ? qsTr("Фигура") : page.selType === "port" ? qsTr("Порт") : qsTr("Свойства")
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeH2
                    font.weight: Font.Bold
                }
                SelectionEditor { model: page.model }
            }
        }
    }

    // --- свойства отдельным экраном (вертикально) ---
    Rectangle {
        id: editor
        anchors.fill: parent
        visible: page.editorOpen && !page.landscape
        color: Theme.bg
        z: 10
        MouseArea { anchors.fill: parent }
        Item {
            id: editorHeader
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: Theme.touchMin + 8
            Row {
                anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
                TToolButton { icon: "back"; onClicked: page.editorOpen = false }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.selType === "port" ? qsTr("Порт") : page.selType === "shape" ? qsTr("Фигура") : qsTr("Свойства")
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeH2
                    font.weight: Font.Bold
                }
            }
            TButton {
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                compact: true
                text: qsTr("К холсту")
                onClicked: page.editorOpen = false
            }
        }
        Flickable {
            anchors { left: parent.left; right: parent.right; top: editorHeader.bottom; bottom: parent.bottom }
            anchors.leftMargin: Theme.gutter
            anchors.rightMargin: Theme.gutter
            clip: true
            contentHeight: editorColumn.implicitHeight + Theme.gutter
            boundsBehavior: Flickable.StopAtBounds
            SelectionEditor {
                id: editorColumn
                width: parent.width
                model: page.model
            }
        }
    }

    TSheet {
        id: mergeSheet
        title: qsTr("Геометрия")
        open: mergeNote.text.length > 0
        TNote { id: mergeNote; text: "" }
        buttons: [ TButton { compact: true; text: qsTr("Понятно"); onClicked: mergeNote.text = "" } ]
        onClosed: mergeNote.text = ""
    }

    // --- импорт среза 3D-модели (STL, OBJ) ---
    TSheet {
        id: importSheet
        property var files: []
        property string plane: "xy"
        title: qsTr("Импорт из 3D")
        TNote {
            text: importSheet.files.length === 0
                  ? qsTr("В папке задачи нет файлов .stl и .obj. Положите модель в папку задачи. STEP на телефоне не читается: сохраните модель в STL.")
                  : qsTr("Модель режется плоскостью, срез становится обычными фигурами.")
        }
        TSegmented {
            width: parent.width
            options: [ { key: "xy", label: "XY" }, { key: "xz", label: "XZ" }, { key: "yz", label: "YZ" } ]
            current: importSheet.plane
            onPicked: (k) => importSheet.plane = k
        }
        TField { id: importOffset; width: parent.width; label: qsTr("Положение плоскости"); unit: qsTr("мм"); value: "0" }
        Repeater {
            model: importSheet.files
            delegate: TButton {
                required property string modelData
                width: parent ? parent.width : 300
                compact: true
                text: modelData
                onClicked: {
                    importSheet.open = false
                    mergeNote.text = page.model.importMesh(modelData, importSheet.plane, importOffset.value)
                    cv.zoomToFit()
                }
            }
        }
        buttons: [ TButton { compact: true; text: qsTr("Закрыть"); onClicked: importSheet.open = false } ]
    }

    TSheet {
        id: filletSheet
        title: qsTr("Скругление углов")
        TField { id: filletRadius; width: parent.width; label: qsTr("Радиус"); unit: qsTr("мм"); value: "5" }
        buttons: [
            TButton { compact: true; text: qsTr("Отмена"); onClicked: filletSheet.open = false },
            TButton { compact: true; primary: true; text: qsTr("Скруглить")
                      onClicked: { filletSheet.open = false; mergeNote.text = page.model.filletSelected(filletRadius.value) } }
        ]
    }

    // --- добавить фигуру или порт числами ---
    TSheet {
        id: addSheet
        title: qsTr("Добавить числами")
        TNote { text: qsTr("Фигура появится в центре видимой части; размеры и материал — в свойствах.") }
        Grid {
            width: parent.width
            columns: 2
            columnSpacing: Theme.spacing / 2
            rowSpacing: Theme.spacing / 2
            TButton { width: (parent.width - Theme.spacing / 2) / 2; compact: true; text: qsTr("Прямоугольник"); onClicked: page.addByNumbers("rect") }
            TButton { width: (parent.width - Theme.spacing / 2) / 2; compact: true; text: qsTr("Круг"); onClicked: page.addByNumbers("circle") }
            TButton { width: (parent.width - Theme.spacing / 2) / 2; compact: true; text: qsTr("Линия"); onClicked: page.addByNumbers("line") }
            TButton { width: (parent.width - Theme.spacing / 2) / 2; compact: true; text: qsTr("Контур"); onClicked: page.addByNumbers("contour") }
            TButton { width: (parent.width - Theme.spacing / 2) / 2; compact: true; text: qsTr("Порт слева"); onClicked: page.addByNumbers("port:left") }
            TButton { width: (parent.width - Theme.spacing / 2) / 2; compact: true; text: qsTr("Порт справа"); onClicked: page.addByNumbers("port:right") }
            TButton { width: (parent.width - Theme.spacing / 2) / 2; compact: true; text: qsTr("Порт снизу"); onClicked: page.addByNumbers("port:bottom") }
            TButton { width: (parent.width - Theme.spacing / 2) / 2; compact: true; text: qsTr("Импорт 3D"); onClicked: { addSheet.open = false; importSheet.files = page.model.meshFiles(); importSheet.open = true } }
            TButton { width: (parent.width - Theme.spacing / 2) / 2; compact: true; text: qsTr("Скруглить углы"); enabled: page.selType === "shape"; onClicked: { addSheet.open = false; filletSheet.open = true } }
            TButton { width: (parent.width - Theme.spacing / 2) / 2; compact: true; text: qsTr("Порт сверху"); onClicked: page.addByNumbers("port:top") }
        }
        buttons: [ TButton { compact: true; text: qsTr("Закрыть"); onClicked: addSheet.open = false } ]
    }
}

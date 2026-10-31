// Встроенный TPL Designer (решение M17): задание .tpl собирается в приложении.
//
// Шаги — как в настольном TPL Designer: «Задание» (расчёт) → «Геометрия» → «Объекты»
// (материалы и источники) → «Вывод» → «Проверка» с кнопкой «Сгенерировать».
// Шаги — вкладками сверху (M4), в любом порядке; замечания проверки видны на всех шагах.
//
// Работает и вертикально, и горизонтально (landscape = ширина больше высоты): в
// горизонтальном положении холст занимает большую часть экрана, свойства — в боковой
// панели; страницы с полями раскладываются в две колонки. Всё состояние живёт в
// DesignerModel и холсте, поэтому поворот не теряет ни геометрию, ни масштаб, ни
// выделение.
//
// Подключение в оболочке:   import TamicTplDesigner
//     TplDesignerPage { taskFolder: …; onSaved: (tplPath) => … }
pragma ComponentBehavior: Bound
import QtQuick
import TamicTplTheme
import QtQuick.Layouts

Item {
    id: root

    /// Папка задачи (M16): сюда «Сгенерировать» кладёт .tpl и сопутствующие файлы.
    property string taskFolder: ""
    /// Для папки content:// (Android SAF): объект с методом copyOut(localPath, folder, name).
    property QtObject exporter: null
    /// Задание записано: путь к .tpl в папке задачи.
    signal saved(string tplPath)
    /// Задание записано: папка задачи и имя .tpl (переход к заданию в оболочке).
    signal savedTask(string folder, string tplName)

    property int step: 0
    readonly property bool landscape: width > height
    readonly property alias designer: dm
    readonly property alias geometry: geometryPage

    /// Кнопка «назад» телефона: true — обработано внутри страницы.
    function goBack() {
        if (samplesPage.visible) {
            if (!samplesPage.goBack())
                samplesPage.visible = false
            return true
        }
        if (openSheet.open) { openSheet.open = false; return true }
        if (messageSheet.open) { messageSheet.open = false; return true }
        if (calcSheet.open) { calcSheet.open = false; return true }
        if (saveSampleSheet.open) { saveSampleSheet.open = false; return true }
        if (confirmSheet.open) { confirmSheet.open = false; return true }
        if (geometryPage.editorOpen && step === 1) { geometryPage.editorOpen = false; return true }
        if (objectsPage.editorOpen && step === 2) { objectsPage.editorOpen = false; return true }
        if (step > 0) { step = step - 1; return true }
        return false
    }

    function openSamples() {
        samplesPage.reload()
        samplesPage.visible = true
    }

    function openCard(key) {
        dm.openSampleCard(key)
        samplesPage.cardOpen = true
    }

    function showMessage(title, text) {
        messageSheet.title = title
        messageText.text = text
        messageSheet.open = true
    }

    function generate() {
        if (!dm.canGenerate) {
            const r = dm.generate()
            showMessage(qsTr("Нельзя создать файл"), r.message)
            return
        }
        const existing = dm.existingFiles()
        if (existing.length > 0) {
            confirmText.text = qsTr("В папке задачи уже есть:\n  ") + existing.join("\n  ")
                               + qsTr("\n\nОни будут перезаписаны.")
            confirmSheet.open = true
            return
        }
        doGenerate()
    }

    function doGenerate() {
        const r = dm.generate()
        showMessage(r.ok ? qsTr("Задание создано") : qsTr("Не удалось записать"), r.message)
    }

    DesignerModel {
        id: dm
        taskFolder: root.taskFolder
        exporter: root.exporter
        onSaved: (tplPath) => root.saved(tplPath)
        onSavedTask: (folder, tplName) => root.savedTask(folder, tplName)
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    // ---------------------------------------------------------------- шапка
    Rectangle {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: headerColumn.implicitHeight
        color: Theme.bg
        z: 2

        Column {
            id: headerColumn
            anchors { left: parent.left; right: parent.right; top: parent.top }

            // Заголовок — только вертикально: горизонтально высоту бережём для холста.
            Item {
                visible: !root.landscape
                width: parent.width
                height: visible ? Theme.touchMin + 4 : 0
                Text {
                    id: title
                    anchors { left: parent.left; leftMargin: Theme.gutter; verticalCenter: parent.verticalCenter }
                    text: qsTr("TPL Designer")
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeH2
                    font.weight: Theme.fontWeightTitle
                }
                Text {
                    anchors {
                        left: title.right; leftMargin: Theme.spacing
                        right: chipPortrait.left; rightMargin: Theme.spacing
                        verticalCenter: parent.verticalCenter
                    }
                    text: (dm.settings.name || "") + ".tpl"
                    color: Theme.textDim
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontSizeSmall
                    elide: Text.ElideMiddle
                }
                IssueChip {
                    id: chipPortrait
                    anchors { right: parent.right; rightMargin: Theme.gutter; verticalCenter: parent.verticalCenter }
                    model: dm
                    onClicked: root.step = 5
                }
            }

            Item {
                width: parent.width
                height: 56
                StepTabs {
                    anchors {
                        left: parent.left; leftMargin: 8
                        right: root.landscape ? chipLandscape.left : parent.right
                        rightMargin: 8
                        top: parent.top; bottom: parent.bottom
                    }
                    current: root.step
                    labels: [qsTr("Задание"), qsTr("Геометрия"), qsTr("Объекты"), qsTr("Считать"), qsTr("Вывод"), qsTr("Проверка")]
                    onPicked: (i) => root.step = i
                }
                IssueChip {
                    id: chipLandscape
                    visible: root.landscape
                    anchors { right: parent.right; rightMargin: Theme.gutter; verticalCenter: parent.verticalCenter }
                    model: dm
                    onClicked: root.step = 5
                }
            }
            Rectangle { width: parent.width; height: Theme.borderW; color: Theme.line }
        }
    }

    // ---------------------------------------------------------------- шаги
    StackLayout {
        id: stack
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom }
        currentIndex: root.step

        SetupPage {
            model: dm
            wide: root.landscape
            onNext: root.step = 1
            onOpenRequested: { openSheet.files = dm.tplFiles(); openSheet.open = true }
            onSamplesRequested: root.openSamples()
            onNewRequested: { dm.newProject(); geometryPage.fit() }
            onCalculatorRequested: { calcSheet.recalc(); calcSheet.open = true }
        }
        GeometryPage {
            id: geometryPage
            model: dm
            landscape: root.landscape
            onNext: root.step = 2
        }
        ObjectsPage {
            id: objectsPage
            model: dm
            landscape: root.landscape
            onNext: root.step = 3
        }
        RunPlanPage {
            model: dm
            wide: root.landscape
            onNext: root.step = 4
        }
        OutputPage {
            model: dm
            wide: root.landscape
            onNext: root.step = 5
        }
        CheckPage {
            model: dm
            wide: root.landscape
            taskFolder: root.taskFolder
            active: root.step === 5
            onGenerateRequested: root.generate()
            onSaveSampleRequested: saveSampleSheet.open = true
        }
    }

    // ---------------------------------------------------------------- листы
    TSheet {
        id: messageSheet
        Text {
            id: messageText
            width: parent.width
            wrapMode: Text.Wrap
            color: Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeBody
        }
        buttons: [
            TButton { text: qsTr("Понятно"); primary: true; onClicked: messageSheet.open = false }
        ]
    }

    TSheet {
        id: confirmSheet
        title: qsTr("Перезаписать файлы?")
        Text {
            id: confirmText
            width: parent.width
            wrapMode: Text.Wrap
            color: Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeBody
        }
        buttons: [
            TButton { text: qsTr("Отмена"); compact: true; onClicked: confirmSheet.open = false },
            TButton { text: qsTr("Перезаписать"); primary: true; compact: true
                      onClicked: { confirmSheet.open = false; root.doGenerate() } }
        ]
    }

    TSheet {
        id: openSheet
        property var files: []
        title: qsTr("Открыть задание из папки")
        TNote {
            text: openSheet.files.length === 0
                  ? (root.taskFolder.length === 0 ? qsTr("Папка задачи не выбрана.")
                                                  : qsTr("В папке задачи нет файлов .tpl."))
                  : qsTr("Задание раскладывается по шагам; правите геометрию, а не текст.")
        }
        Repeater {
            model: openSheet.files
            delegate: TButton {
                required property string modelData
                width: parent.width
                text: modelData
                onClicked: {
                    openSheet.open = false
                    const msg = dm.openTpl(modelData)
                    geometryPage.fit()
                    root.showMessage(qsTr("Файл прочитан"), msg)
                }
            }
        }
        buttons: [ TButton { text: qsTr("Закрыть"); compact: true; onClicked: openSheet.open = false } ]
    }

    SamplesPage {
        id: samplesPage
        anchors.fill: parent
        visible: false
        model: dm
        onCloseRequested: visible = false
        onOpened: (message) => {
            visible = false
            geometryPage.fit()
            root.step = 1
            root.showMessage(qsTr("Образец"), message)
        }
    }

    TSheet {
        id: calcSheet
        property var out: ({})
        function recalc() {
            out = dm.calculate({ a: calcA.value, wgEps: calcWgEps.value, eps: calcEps.value, freq: calcFreq.value,
                                 n: calcN.value, b: calcB.value })
        }
        title: qsTr("Калькулятор")
        TField { id: calcA; width: parent.width; label: qsTr("Ширина волновода a"); unit: qsTr("мм"); value: "72"; onEdited: calcSheet.recalc() }
        TField { id: calcWgEps; width: parent.width; label: qsTr("Заполнение ε"); value: "1"; onEdited: calcSheet.recalc() }
        TNote { text: qsTr("Отсечка H10: %1, H20: %2").arg(calcSheet.out.fc10 || "").arg(calcSheet.out.fc20 || "") }
        TField { id: calcEps; width: parent.width; label: qsTr("Диэлектрик ε"); value: "4"; onEdited: calcSheet.recalc() }
        TField { id: calcFreq; width: parent.width; label: qsTr("Частота"); unit: qsTr("ГГц"); value: "4"; onEdited: calcSheet.recalc() }
        TNote { text: qsTr("Длина волны в среде: %1").arg(calcSheet.out.lambda || "") }
        TField { id: calcN; width: parent.width; label: qsTr("Концентрация N, м⁻³"); value: "1.24e18"; onEdited: calcSheet.recalc() }
        TField { id: calcB; width: parent.width; label: qsTr("Подмагничивание B, Тл"); value: "0"; onEdited: calcSheet.recalc() }
        TNote { text: calcSheet.out.plasma || "" }
        buttons: [ TButton { compact: true; text: qsTr("Закрыть"); onClicked: calcSheet.open = false } ]
    }

    TSheet {
        id: saveSampleSheet
        title: qsTr("Сохранить как образец")
        TField { id: sampleTitle; width: parent.width; numeric: false; label: qsTr("Название") }
        TField { id: sampleSummary; width: parent.width; numeric: false; label: qsTr("Описание") }
        buttons: [
            TButton { text: qsTr("Отмена"); compact: true; onClicked: saveSampleSheet.open = false },
            TButton { text: qsTr("Сохранить"); primary: true; compact: true
                      onClicked: {
                          const r = dm.saveAsSample(sampleTitle.value, sampleSummary.value)
                          saveSampleSheet.open = false
                          root.showMessage(qsTr("Образец"), r.message)
                      } }
        ]
    }
}

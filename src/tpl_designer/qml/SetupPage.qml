// Шаг 1 «Задание»: имя, ядро H/X, счётная область, шаг сетки, время, частоты.
// Поля — страница «Расчёт» настольного TPL Designer (mainwindow.cpp, buildSetupPage).
import QtQuick
import TamicTplTheme

TPage {
    id: page

    required property DesignerModel model
    readonly property var st: model.settings
    signal next()
    signal openRequested()
    signal samplesRequested()
    signal newRequested()

    function put(key, value) { page.model.setSetting(key, value) }

    TCard {
        width: page.colWidth
        title: qsTr("Задание")
        TField {
            label: qsTr("Имя файла (.tpl)")
            numeric: false
            mono: true
            value: page.st.name || ""
            onEdited: (t) => page.put("name", t)
        }
        Text {
            text: qsTr("Ядро расчёта")
            color: Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
        }
        TSegmented {
            options: [ { key: "H", label: qsTr("H — tmc_rth") }, { key: "X", label: qsTr("X — с плазмой") } ]
            current: page.st.kernel || "H"
            onPicked: (k) => page.put("kernel", k)
        }
        TNote { text: qsTr("Заголовок файла у обоих ядер один — #TMC_RT_H. Плазму (N, B, Y) понимает только X.") }
        Grid {
            width: parent.width
            columns: 3
            columnSpacing: Theme.spacing / 2
            TButton { compact: true; width: (parent.width - Theme.spacing) / 3; text: qsTr("Новое"); onClicked: page.newRequested() }
            TButton { compact: true; width: (parent.width - Theme.spacing) / 3; text: qsTr("Открыть"); onClicked: page.openRequested() }
            TButton { compact: true; width: (parent.width - Theme.spacing) / 3; text: qsTr("Образец"); onClicked: page.samplesRequested() }
        }
    }

    TCard {
        width: page.colWidth
        title: qsTr("Счётная область")
        Grid {
            width: parent.width
            columns: 2
            columnSpacing: Theme.spacing
            rowSpacing: Theme.spacing / 2
            TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("X от"); unit: qsTr("мм"); value: page.st.xMin || ""; onEdited: (t) => page.put("xMin", t) }
            TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("X до"); unit: qsTr("мм"); value: page.st.xMax || ""; onEdited: (t) => page.put("xMax", t) }
            TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Y от"); unit: qsTr("мм"); value: page.st.yMin || ""; onEdited: (t) => page.put("yMin", t) }
            TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Y до"); unit: qsTr("мм"); value: page.st.yMax || ""; onEdited: (t) => page.put("yMax", t) }
            TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Шаг сетки DELTA"); unit: qsTr("мм"); value: page.st.delta || ""; onEdited: (t) => page.put("delta", t) }
            TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Время счёта"); unit: qsTr("нс"); value: page.st.timeMax || ""; onEdited: (t) => page.put("timeMax", t) }
        }
        TNote { text: qsTr("Время счёта растёт примерно кубически при уменьшении шага сетки. К этому шагу привязывается палец на холсте.") }
        TNote { text: qsTr("Узлов сетки: %1, памяти ядру около %2.").arg(page.st.nodes || "0").arg(page.st.memory || "") ; level: "dim" }
    }

    TCard {
        width: page.colWidth
        title: qsTr("Частота")
        TSegmented {
            options: [ { key: "one", label: qsTr("Одна частота") }, { key: "range", label: qsTr("Диапазон") } ]
            current: page.st.freqRange ? "range" : "one"
            onPicked: (k) => page.put("freqRange", k === "range")
        }
        TField {
            visible: !page.st.freqRange
            label: qsTr("Частота")
            unit: qsTr("ГГц")
            value: page.st.freqSingle || ""
            onEdited: (t) => page.put("freqSingle", t)
        }
        Grid {
            visible: page.st.freqRange || false
            width: parent.width
            columns: 3
            columnSpacing: Theme.spacing / 2
            TField { width: (parent.width - Theme.spacing) / 3; label: qsTr("От, ГГц"); value: page.st.freqStart || ""; onEdited: (t) => page.put("freqStart", t) }
            TField { width: (parent.width - Theme.spacing) / 3; label: qsTr("До, ГГц"); value: page.st.freqEnd || ""; onEdited: (t) => page.put("freqEnd", t) }
            TField { width: (parent.width - Theme.spacing) / 3; label: qsTr("Шаг, ГГц"); value: page.st.freqStep || ""; onEdited: (t) => page.put("freqStep", t) }
        }
        TNote {
            text: page.st.freqInfo || ""
            level: page.st.freqTooMany ? "danger" : ""
        }
        TButton {
            visible: page.st.freqTooMany || false
            width: parent.width
            text: qsTr("Взять шаг %1 ГГц").arg(page.st.maxStep || "")
            onClicked: page.put("useMaxStep", true)
        }
        TSwitchRow {
            visible: page.st.freqRange || false
            enabled: (page.st.freqCount || 0) > 1
            text: qsTr("Разложить по нескольким заданиям")
            hint: qsTr("Конвейер: у каждого задания свой .s. В одно задание больше 99 частот не помещается.")
            checked: page.st.chunkOn || false
            onToggled: (on) => page.put("chunkOn", on)
        }
        TField {
            visible: (page.st.freqRange && page.st.chunkOn) || false
            label: qsTr("Частот в одном задании")
            value: page.st.chunkSize || ""
            onEdited: (t) => page.put("chunkSize", t)
        }
    }

    TButton {
        width: page.colWidth
        primary: true
        text: qsTr("Далее: геометрия")
        onClicked: page.next()
    }
}

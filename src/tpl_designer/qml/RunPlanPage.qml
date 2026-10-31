// Шаг «Что считать»: какие порты возбуждать (столбцы S-матрицы), варианты геометрии, поворот объекта.
// Страница настольного TPL Designer (RunPlanPage): один файл задания вместо нескольких, потому что
// каждый #STEP самодостаточен. Правила — в designerextras.cpp.
// https://www.tamic.ru/docs/code/tpl-designer.html#gui-run
pragma ComponentBehavior: Bound
import QtQuick
import TamicTplTheme

TPage {
    id: page

    required property DesignerModel model
    readonly property var plan: model.runPlan
    readonly property var sweep: plan.sweep || ({})
    property int openVariant: -1
    signal next()

    TCard {
        width: page.colWidth
        title: qsTr("Какие S-параметры считать")
        TNote { text: qsTr("Один прогон возбуждает один вход и даёт один столбец S-матрицы. Чтобы получить, например, S11 и S21, отметьте вход 1; чтобы получить всю матрицу устройства с двумя портами, отметьте оба входа.") }
        TNote {
            visible: page.model.ports.length === 0
            level: "warn"
            text: qsTr("Портов нет: добавьте их на шаге «Геометрия».")
        }
        Repeater {
            model: page.plan.ports || []
            delegate: TSwitchRow {
                required property var modelData
                width: parent ? parent.width : 300
                text: modelData.gives
                hint: modelData.text
                checked: modelData.checked
                onToggled: (on) => page.model.setExciter(modelData.id, on)
            }
        }
        TNote { text: page.plan.hint || ""; level: page.plan.hintLevel === "warn" ? "warn" : "" }
    }

    TCard {
        width: page.colWidth
        title: qsTr("Поворот объекта по шагам")
        TSwitchRow {
            text: qsTr("Поворачивать объект")
            hint: qsTr("Объект поворачивается на заданные углы, каждый угол — свой шаг расчёта.")
            checked: page.sweep.on || false
            onToggled: (on) => page.model.setSweep("on", on)
        }
        Column {
            visible: page.sweep.on || false
            width: parent.width
            spacing: Theme.spacing / 2
            Repeater {
                model: page.sweep.shapes || []
                delegate: TSwitchRow {
                    required property var modelData
                    width: parent ? parent.width : 300
                    text: modelData.text
                    checked: modelData.on
                    onToggled: (on) => page.model.setSweepShape(modelData.id, on)
                }
            }
            Grid {
                width: parent.width
                columns: 3
                columnSpacing: Theme.spacing / 2
                TField { width: (parent.width - Theme.spacing) / 3; label: qsTr("Угол от"); unit: "°"; value: page.sweep.from || ""; onEdited: (t) => page.model.setSweep("from", t) }
                TField { width: (parent.width - Theme.spacing) / 3; label: qsTr("Угол до"); unit: "°"; value: page.sweep.to || ""; onEdited: (t) => page.model.setSweep("to", t) }
                TField { width: (parent.width - Theme.spacing) / 3; label: qsTr("Шаг"); unit: "°"; value: page.sweep.step || ""; onEdited: (t) => page.model.setSweep("step", t) }
            }
            TSwitchRow {
                text: qsTr("Ось в центре объекта")
                checked: page.sweep.center || false
                onToggled: (on) => page.model.setSweep("center", on)
            }
            Grid {
                visible: !(page.sweep.center || false)
                width: parent.width
                columns: 2
                columnSpacing: Theme.spacing
                TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Ось X"); unit: qsTr("мм"); value: page.sweep.pivotX || ""; onEdited: (t) => page.model.setSweep("pivotX", t) }
                TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Ось Y"); unit: qsTr("мм"); value: page.sweep.pivotY || ""; onEdited: (t) => page.model.setSweep("pivotY", t) }
            }
            TSwitchRow {
                text: qsTr("Опорный шаг без объекта")
                checked: page.sweep.reference || false
                onToggled: (on) => page.model.setSweep("reference", on)
            }
        }
        TNote { text: page.sweep.hint || ""; level: page.sweep.hintLevel === "warn" ? "warn" : "" }
    }

    TCard {
        width: page.colWidth
        title: qsTr("Варианты геометрии")
        enabled: !(page.sweep.on || false)
        opacity: enabled ? 1.0 : 0.5
        TNote { text: (page.plan.variants || []).length === 0
                      ? qsTr("Вариантов нет: задание посчитается один раз, со всей нарисованной геометрией.")
                      : qsTr("Снятый переключатель значит, что фигуры в этом варианте нет. Порты остаются во всех вариантах.") }
        Repeater {
            model: page.plan.variants || []
            delegate: Column {
                id: variantBox
                required property var modelData
                width: parent ? parent.width : 300
                spacing: Theme.spacing / 2
                ObjectRow {
                    title: variantBox.modelData.name + "  [" + variantBox.modelData.tag + "]"
                    subtitle: variantBox.modelData.what
                    swatch: variantBox.modelData.empty ? Theme.danger : Theme.accent
                    selected: page.openVariant === variantBox.modelData.index
                    onClicked: page.openVariant = page.openVariant === variantBox.modelData.index ? -1 : variantBox.modelData.index
                }
                Column {
                    visible: page.openVariant === variantBox.modelData.index
                    width: parent.width
                    spacing: Theme.spacing / 2
                    Grid {
                        width: parent.width
                        columns: 2
                        columnSpacing: Theme.spacing
                        TField { width: (parent.width - Theme.spacing) / 2; numeric: false; label: qsTr("Название"); value: variantBox.modelData.name; onEdited: (t) => page.model.setVariant(variantBox.modelData.index, "name", t) }
                        TField { width: (parent.width - Theme.spacing) / 2; numeric: false; mono: true; label: qsTr("Тег файлов"); value: variantBox.modelData.tag; onEdited: (t) => page.model.setVariant(variantBox.modelData.index, "tag", t) }
                    }
                    Repeater {
                        model: variantBox.modelData.shapes
                        delegate: TSwitchRow {
                            required property var modelData
                            width: parent ? parent.width : 300
                            text: modelData.text
                            checked: modelData.on
                            onToggled: (on) => page.model.setVariantShape(variantBox.modelData.index, modelData.id, on)
                        }
                    }
                    TButton {
                        compact: true
                        danger: true
                        text: qsTr("Удалить вариант")
                        onClicked: { page.openVariant = -1; page.model.removeVariant(variantBox.modelData.index) }
                    }
                }
            }
        }
        Grid {
            width: parent.width
            columns: 2
            columnSpacing: Theme.spacing / 2
            TButton { width: (parent.width - Theme.spacing / 2) / 2; compact: true; text: qsTr("Добавить"); onClicked: page.model.addVariant() }
            TButton { width: (parent.width - Theme.spacing / 2) / 2; compact: true; text: qsTr("Пустой + заполненный"); onClicked: page.model.presetVariants() }
        }
    }

    TCard {
        width: page.colWidth
        title: qsTr("Итог")
        TNote { text: page.plan.summary || ""; level: page.plan.summaryHeavy ? "warn" : "" }
        TButton {
            primary: true
            text: qsTr("Далее: вывод")
            onClicked: page.next()
        }
    }
}

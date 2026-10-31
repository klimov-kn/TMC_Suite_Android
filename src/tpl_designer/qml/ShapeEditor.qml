// Свойства фигуры: имя, материал, eps, плазма (только X), размеры числами, стороны.
// Поля — как в диалоге фигуры TPL Designer (shapedialog.cpp); правка уходит в модель
// сразу (model.setShape), отмена — общей кнопкой «Отменить».
pragma ComponentBehavior: Bound
import QtQuick
import TamicTplTheme

Column {
    id: ed

    required property DesignerModel model
    readonly property var s: model.selected
    readonly property bool xKernel: model.settings.kernel === "X"

    width: parent ? parent.width : 300
    spacing: Theme.spacing

    function put(key, value) { ed.model.setShape(ed.s.id, key, value) }

    TField {
        label: qsTr("Имя (в файл уходит комментарием)")
        numeric: false
        value: ed.s.name || ""
        onEdited: (t) => ed.put("name", t)
    }
    TNote { text: ed.s.kindText || ""; level: "dim" }

    Text {
        text: qsTr("Материал")
        color: Theme.textDim
        font.family: Theme.fontUi
        font.pixelSize: Theme.fontSizeSmall
    }
    TSegmented {
        columns: 2
        options: [
            { key: "metal", label: qsTr("Металл") },
            { key: "magnetic", label: qsTr("Магн. стенка") },
            { key: "absorber", label: qsTr("Поглотитель") },
            { key: "dielectric", label: qsTr("Диэлектрик") }
        ]
        current: ed.s.material || ""
        onPicked: (k) => ed.put("material", k)
    }
    TNote {
        text: ed.s.contour
              ? qsTr("Металл, магнитная стенка и поглотитель — это КОНТУР: ядро ставит условие только на периметре.")
              : qsTr("Диэлектрик и плазма — это ЗАЛИВКА: ядро задаёт среду по всей площади.")
    }

    TField {
        visible: ed.s.material === "dielectric" && !ed.s.epsFormulaOn
        label: qsTr("Диэлектрическая проницаемость ε (сама ε, а не добавка)")
        value: ed.s.eps || ""
        onEdited: (t) => ed.put("eps", t)
    }
    TNote {
        visible: ed.s.material === "dielectric" && !ed.s.epsFormulaOn
        text: qsTr("В файл уйдёт добавка ε − 1 — её ставит программа.")
    }
    TSwitchRow {
        visible: ed.s.material === "dielectric" || (ed.s.epsFormulaOn || false)
        text: qsTr("ε по формуле от координат")
        hint: qsTr("x, y — мм от центра фигуры, r и f (угол). Пишется сама ε.")
        checked: ed.s.epsFormulaOn || false
        onToggled: (on) => ed.put("epsFormulaOn", on)
    }
    TField {
        visible: ed.s.epsFormulaOn || false
        label: qsTr("ε(x, y)")
        numeric: false
        mono: true
        value: ed.s.epsFormula || ""
        placeholder: "1+3*exp(-(x^2+y^2)/100)"
        onEdited: (t) => ed.put("epsFormula", t)
    }

    // --- плазма: только X-мода (tmc_rtx) ---
    TCard {
        visible: ed.xKernel
        title: qsTr("Плазма (X-мода)")
        TNote { text: qsTr("N — м⁻³ (СИ), B — Тесла, Y — безразмерный. Можно число или формулу от x, y.") }
        TSwitchRow {
            text: qsTr("Концентрация N")
            checked: ed.s.plasmaN || false
            onToggled: (on) => ed.put("plasmaN", on)
        }
        TField {
            visible: ed.s.plasmaN || false
            numeric: false
            mono: true
            unit: qsTr("м⁻³")
            value: ed.s.plasmaNValue || ""
            onEdited: (t) => ed.put("plasmaNValue", t)
        }
        TSwitchRow {
            text: qsTr("Подмагничивание B")
            hint: qsTr("Работает только вместе с N.")
            checked: ed.s.plasmaB || false
            onToggled: (on) => ed.put("plasmaB", on)
        }
        TField {
            visible: ed.s.plasmaB || false
            numeric: false
            mono: true
            unit: qsTr("Тл")
            value: ed.s.plasmaBValue || ""
            onEdited: (t) => ed.put("plasmaBValue", t)
        }
        TSwitchRow {
            text: qsTr("Потери: проводимость σ")
            checked: ed.s.plasmaS || false
            onToggled: (on) => ed.put("plasmaS", on)
        }
        TField {
            visible: ed.s.plasmaS || false
            width: parent.width
            label: qsTr("σ, См/м")
            value: ed.s.plasmaSValue || ""
            onEdited: (t) => ed.put("plasmaSValue", t)
        }
        TField { width: parent.width; numeric: false; mono: true; label: qsTr("N из файла"); value: ed.s.plasmaNFile || ""; onEdited: (t) => ed.put("plasmaNFile", t) }
        TField { width: parent.width; numeric: false; mono: true; label: qsTr("B из файла"); value: ed.s.plasmaBFile || ""; onEdited: (t) => ed.put("plasmaBFile", t) }
        TField { width: parent.width; numeric: false; mono: true; label: qsTr("Y из файла"); value: ed.s.plasmaYFile || ""; onEdited: (t) => ed.put("plasmaYFile", t) }
        TSwitchRow {
            text: qsTr("Потери Y")
            checked: ed.s.plasmaY || false
            onToggled: (on) => ed.put("plasmaY", on)
        }
        TField {
            visible: ed.s.plasmaY || false
            numeric: false
            mono: true
            value: ed.s.plasmaYValue || ""
            onEdited: (t) => ed.put("plasmaYValue", t)
        }
    }

    // --- размеры числами ---
    Text {
        text: qsTr("Размеры, мм")
        color: Theme.accent
        font.family: Theme.fontUi
        font.pixelSize: Theme.fontSizeBody
        font.weight: Font.Bold
    }
    Grid {
        visible: ed.s.kind === "rect"
        width: parent.width
        columns: 2
        columnSpacing: Theme.spacing
        rowSpacing: Theme.spacing / 2
        TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("X от"); value: ed.s.xMin || ""; onEdited: (t) => ed.put("xMin", t) }
        TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("X до"); value: ed.s.xMax || ""; onEdited: (t) => ed.put("xMax", t) }
        TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Y от"); value: ed.s.yMin || ""; onEdited: (t) => ed.put("yMin", t) }
        TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Y до"); value: ed.s.yMax || ""; onEdited: (t) => ed.put("yMax", t) }
    }
    Grid {
        visible: ed.s.kind === "circle"
        width: parent.width
        columns: 2
        columnSpacing: Theme.spacing
        rowSpacing: Theme.spacing / 2
        TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Центр X"); value: ed.s.cx || ""; onEdited: (t) => ed.put("cx", t) }
        TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Центр Y"); value: ed.s.cy || ""; onEdited: (t) => ed.put("cy", t) }
        TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("R внутр."); value: ed.s.rMin || ""; onEdited: (t) => ed.put("rMin", t) }
        TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("R наружн."); value: ed.s.rMax || ""; onEdited: (t) => ed.put("rMax", t) }
        TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Угол от"); unit: "°"; value: ed.s.fiMin || ""; onEdited: (t) => ed.put("fiMin", t) }
        TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Угол до"); unit: "°"; value: ed.s.fiMax || ""; onEdited: (t) => ed.put("fiMax", t) }
    }
    Grid {
        width: parent.width
        columns: 2
        columnSpacing: Theme.spacing
        TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Скорость Vx"); value: ed.s.vx || ""; onEdited: (t) => ed.put("vx", t) }
        TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Скорость Vy"); value: ed.s.vy || ""; onEdited: (t) => ed.put("vy", t) }
    }
    TField {
        visible: ed.s.kind === "polygon" || ed.s.kind === "polyline"
        label: qsTr("Вершины: по одной на строке «x; y»")
        multiline: true
        numeric: false
        mono: true
        value: ed.s.points || ""
        onEdited: (t) => ed.put("points", t)
    }

    // --- стороны прямоугольного контура ---
    Column {
        visible: ed.s.kind === "rect" && (ed.s.contour || false)
        width: parent.width
        spacing: 0
        Text {
            text: qsTr("Стороны")
            color: Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
        }
        TNote { text: qsTr("Снятая сторона — открытый торец. Разные стороны программа запишет отдельными блоками.") }
        Repeater {
            model: [qsTr("Низ"), qsTr("Право"), qsTr("Верх"), qsTr("Лево")]
            delegate: TSwitchRow {
                required property int index
                required property string modelData
                text: modelData
                checked: ed.s.edgePresent ? ed.s.edgePresent[index] : true
                onToggled: (on) => ed.put("edgePresent" + index, on)
            }
        }
    }
}

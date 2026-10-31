// Свойства порта (источника): сторона и апертура, активный/пассивный, мода, ε во
// входе, сигнал формулой или файлом .sig. Поля — как в диалоге порта TPL Designer
// (portdialog.cpp). INPUT_X / INPUT_Y и добавку ε − 1 выбирает программа, не человек.
// https://www.tamic.ru/docs/platforms/android.html#designer
pragma ComponentBehavior: Bound
import QtQuick
import TamicTplTheme

Column {
    id: ed

    required property DesignerModel model
    readonly property var p: model.selected
    readonly property var wf: p.waveFields || ({})

    width: parent ? parent.width : 300
    spacing: Theme.spacing

    function put(key, value) { ed.model.setPort(ed.p.id, key, value) }

    TField {
        label: qsTr("Имя")
        numeric: false
        value: ed.p.name || ""
        onEdited: (t) => ed.put("name", t)
    }
    TSwitchRow {
        text: qsTr("Активный — возбуждает волну")
        hint: ed.p.active ? qsTr("Возбуждение порта даёт столбец S-матрицы.")
                          : qsTr("Пассивный — только принимает.")
        checked: ed.p.active || false
        onToggled: (on) => ed.put("active", on)
    }

    Text {
        text: qsTr("Сторона (куда смотрит вход)")
        color: Theme.textDim
        font.family: Theme.fontUi
        font.pixelSize: Theme.fontSizeSmall
    }
    TSegmented {
        options: [
            { key: "left", label: qsTr("Слева") },
            { key: "right", label: qsTr("Справа") },
            { key: "bottom", label: qsTr("Снизу") },
            { key: "top", label: qsTr("Сверху") }
        ]
        current: ed.p.side || ""
        onPicked: (k) => ed.put("side", k)
    }
    TNote {
        text: ed.p.alongY ? qsTr("Апертура идёт вдоль оси Y — это INPUT_X: программа запишет его сама.")
                          : qsTr("Апертура идёт вдоль оси X — это INPUT_Y: программа запишет его сама.")
    }
    Grid {
        width: parent.width
        columns: 2
        columnSpacing: Theme.spacing
        rowSpacing: Theme.spacing / 2
        TField {
            width: (parent.width - Theme.spacing) / 2
            label: ed.p.alongY ? qsTr("Апертура Y от") : qsTr("Апертура X от")
            unit: qsTr("мм")
            value: ed.p.from || ""
            onEdited: (t) => ed.put("from", t)
        }
        TField {
            width: (parent.width - Theme.spacing) / 2
            label: ed.p.alongY ? qsTr("Апертура Y до") : qsTr("Апертура X до")
            unit: qsTr("мм")
            value: ed.p.to || ""
            onEdited: (t) => ed.put("to", t)
        }
    }
    TField {
        label: ed.p.alongY ? qsTr("Плоскость входа, X") : qsTr("Плоскость входа, Y")
        unit: qsTr("мм")
        value: ed.p.plane || ""
        onEdited: (t) => ed.put("plane", t)
    }
    TNote { text: qsTr("Вход может стоять и внутри области. Направление волны программа держит в согласии с половиной области — иначе ядро молча развернуло бы её.") }

    Text {
        text: qsTr("Мода во входе")
        color: Theme.textDim
        font.family: Theme.fontUi
        font.pixelSize: Theme.fontSizeSmall
    }
    TSegmented {
        options: [ { key: "h10", label: qsTr("Волна H10") }, { key: "plane", label: qsTr("Плоская волна") } ]
        current: ed.p.mode || ""
        onPicked: (k) => ed.put("mode", k)
    }
    TField {
        label: qsTr("ε среды в сечении входа (сама ε)")
        value: ed.p.eps || ""
        onEdited: (t) => ed.put("eps", t)
    }

    // --- сигнал ---
    TCard {
        visible: ed.p.active || false
        title: qsTr("Падающая волна")
        TSegmented {
            options: [ { key: "formula", label: qsTr("Формулой") }, { key: "file", label: qsTr("Из файла .sig") } ]
            current: ed.p.source || ""
            onPicked: (k) => ed.put("source", k)
        }

        // формулой: Amp(t,f)*sin(2pi*FREQ*t + Faza)
        TSegmented {
            visible: ed.p.source === "formula"
            options: [
                { key: "cont", label: qsTr("Непрерывный") },
                { key: "gauss", label: qsTr("Гаусс") },
                { key: "rect", label: qsTr("Импульс") }
            ]
            current: ed.p.shape || ""
            onPicked: (k) => ed.put("shape", k)
        }
        Grid {
            visible: ed.p.source === "formula"
            width: parent.width
            columns: 2
            columnSpacing: Theme.spacing
            rowSpacing: Theme.spacing / 2
            TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Амплитуда"); value: ed.p.amplitude || ""; onEdited: (t) => ed.put("amplitude", t) }
            TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Фаза"); unit: "°"; value: ed.p.phase || ""; onEdited: (t) => ed.put("phase", t) }
            TField { visible: ed.p.shape === "gauss"; width: (parent.width - Theme.spacing) / 2; label: qsTr("Центр t0"); unit: qsTr("нс"); value: ed.p.center || ""; onEdited: (t) => ed.put("center", t) }
            TField { visible: ed.p.shape !== "cont"; width: (parent.width - Theme.spacing) / 2; label: qsTr("Длительность"); unit: qsTr("нс"); value: ed.p.duration || ""; onEdited: (t) => ed.put("duration", t) }
        }
        TNote {
            visible: ed.p.source === "formula"
            text: qsTr("Amp(t, f) в задании: ") + (ed.p.ampExpression || "")
        }

        // из файла: форма волны, файл пишет программа
        Column {
            visible: ed.p.source === "file"
            width: parent.width
            spacing: Theme.spacing / 2
            Text {
                text: qsTr("Форма: ") + (ed.p.waveName || "")
                color: Theme.text
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.DemiBold
            }
            Flow {
                width: parent.width
                spacing: 6
                Repeater {
                    model: ed.model.waveforms
                    delegate: Rectangle {
                        id: chip
                        required property var modelData
                        readonly property bool active: ed.p.waveKind === modelData.kind
                        width: chipText.implicitWidth + 24
                        height: Theme.touchMin
                        radius: Theme.radiusSm
                        color: active ? Theme.accent : Theme.bg
                        border.width: Theme.borderW
                        border.color: active ? Theme.accent : Theme.line
                        Text {
                            id: chipText
                            anchors.centerIn: parent
                            text: chip.modelData.name
                            color: chip.active ? Theme.textOnAccent : Theme.text
                            font.family: Theme.fontUi
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        MouseArea { anchors.fill: parent; onClicked: ed.put("waveKind", chip.modelData.kind) }
                    }
                }
            }
            TNote { text: ed.p.waveSummary || ""; level: "dim" }
        }
        Grid {
            visible: ed.p.source === "file"
            width: parent.width
            columns: 2
            columnSpacing: Theme.spacing
            rowSpacing: Theme.spacing / 2
            TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Амплитуда"); value: ed.p.wAmplitude || ""; onEdited: (t) => ed.put("wAmplitude", t) }
            TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Начало"); unit: qsTr("нс"); value: ed.p.wStart || ""; onEdited: (t) => ed.put("wStart", t) }
            TField { visible: ed.wf.band || ed.wf.carrier || ed.wf.rate || false; width: (parent.width - Theme.spacing) / 2
                     label: ed.wf.band ? qsTr("Полоса от") : ed.wf.rate ? qsTr("Повторение") : qsTr("Несущая")
                     unit: qsTr("ГГц"); value: ed.p.wF1 || ""; onEdited: (t) => ed.put("wF1", t) }
            TField { visible: ed.wf.band || false; width: (parent.width - Theme.spacing) / 2; label: qsTr("Полоса до"); unit: qsTr("ГГц"); value: ed.p.wF2 || ""; onEdited: (t) => ed.put("wF2", t) }
            TField { visible: ed.wf.tones || false; width: (parent.width - Theme.spacing) / 2; label: qsTr("Тонов"); value: ed.p.wTones || ""; onEdited: (t) => ed.put("wTones", t) }
            TField { visible: ed.wf.periods || false; width: (parent.width - Theme.spacing) / 2; label: qsTr("Периодов"); value: ed.p.wPeriods || ""; onEdited: (t) => ed.put("wPeriods", t) }
            TField { visible: ed.wf.duration || false; width: (parent.width - Theme.spacing) / 2; label: qsTr("Длительность"); unit: qsTr("нс"); value: ed.p.wDuration || ""; onEdited: (t) => ed.put("wDuration", t) }
            TField { visible: ed.wf.width || false; width: (parent.width - Theme.spacing) / 2; label: qsTr("Ширина τ"); unit: qsTr("нс"); value: ed.p.wWidth || ""; onEdited: (t) => ed.put("wWidth", t) }
            TField { visible: ed.wf.edges || false; width: (parent.width - Theme.spacing) / 2; label: qsTr("Фронт"); unit: qsTr("нс"); value: ed.p.wRise || ""; onEdited: (t) => ed.put("wRise", t) }
            TField { visible: ed.wf.edges || false; width: (parent.width - Theme.spacing) / 2; label: qsTr("Спад"); unit: qsTr("нс"); value: ed.p.wFall || ""; onEdited: (t) => ed.put("wFall", t) }
        }
        TField {
            visible: ed.p.source === "file" && (ed.wf.expression || false)
            label: qsTr("a(t), t — нс")
            numeric: false
            mono: true
            value: ed.p.wExpression || ""
            onEdited: (t) => ed.put("wExpression", t)
        }
        TField {
            visible: ed.p.source === "file"
            label: qsTr("Имя файла .sig (без точки; пусто — по имени порта)")
            numeric: false
            mono: true
            value: ed.p.file || ""
            placeholder: ed.p.sigName || ""
            onEdited: (t) => ed.put("file", t)
        }
        TNote {
            visible: ed.p.source === "file"
            text: qsTr("Файл .sig программа пишет рядом с заданием сама. Частоту задания она ставит в частоту согласования сигнала: ")
                  + (ed.p.suggestedFreq || "") + qsTr(" ГГц.")
        }
    }
}

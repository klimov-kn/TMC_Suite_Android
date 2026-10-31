// Шаг 4 «Вывод»: что выгружает ядро — сигналы, топология, поля, S-матрица, ДН.
// Поля — страница «Вывод данных» настольного TPL Designer (buildOutputPage). Окно
// снятия само едет за временем счёта (followTimeWindow в модели).
import QtQuick
import TamicTplTheme

TPage {
    id: page

    required property DesignerModel model
    readonly property var st: model.settings
    signal next()

    function put(key, value) { page.model.setSetting(key, value) }

    TCard {
        width: page.colWidth
        title: qsTr("Сигналы на портах")
        TField {
            label: qsTr("Имя файла (.t пишется всегда)")
            numeric: false
            mono: true
            value: page.st.signalFile || ""
            onEdited: (t) => page.put("signalFile", t)
        }
        TNote { text: qsTr("Расширение ядро добавит само — точку в имени не ставьте.") }
        TSwitchRow {
            text: qsTr("Топология (.tt)")
            hint: qsTr("Пишется один раз и почти ничего не стоит.")
            checked: page.st.topology || false
            onToggled: (on) => page.put("topology", on)
        }
    }

    TCard {
        width: page.colWidth
        title: qsTr("Мгновенные поля (.ex, .ey, .hz)")
        TSegmented {
            options: [
                { key: "0", label: qsTr("Нет") },
                { key: "1", label: qsTr("1-й шаг") },
                { key: "2", label: qsTr("Все шаги") }
            ]
            current: String(page.st.fields || 0)
            onPicked: (k) => page.put("fields", parseInt(k))
        }
        TNote { text: page.st.fieldsHint || ""; level: page.st.fieldsSlow ? "warn" : "" }
    }

    TCard {
        width: page.colWidth
        title: qsTr("Матрица рассеяния (.s)")
        TSwitchRow {
            text: qsTr("Снимать S-матрицу")
            checked: page.st.sMatrix || false
            onToggled: (on) => page.put("sMatrix", on)
        }
        TField {
            visible: page.st.sMatrix || false
            label: qsTr("Имя файла")
            numeric: false
            mono: true
            value: page.st.sMatrixFile || ""
            onEdited: (t) => page.put("sMatrixFile", t)
        }
        TSwitchRow {
            visible: page.st.sMatrix || false
            text: qsTr("Ограничить окно у входа с файлом сигнала")
            hint: qsTr("Без ограничения ядро считает S-матрицу по всему окну счёта.")
            checked: page.st.sWide || false
            onToggled: (on) => page.put("sWide", on)
        }
        Grid {
            visible: page.st.sMatrix || false
            width: parent.width
            columns: 2
            columnSpacing: Theme.spacing
            TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Окно от"); unit: qsTr("нс"); value: page.st.sMin || ""; onEdited: (t) => page.put("sMin", t) }
            TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Окно до"); unit: qsTr("нс"); value: page.st.sMax || ""; onEdited: (t) => page.put("sMax", t) }
        }
        TNote {
            visible: page.st.sMatrix || false
            text: qsTr("Ядро ДОПИСЫВАЕТ .s, а не перезаписывает. Приложение считает в чистой рабочей папке, поэтому старые точки не подмешаются.")
        }
    }

    TCard {
        width: page.colWidth
        title: qsTr("Диаграмма направленности")
        TSwitchRow {
            text: qsTr("Считать ДН (.AMP, .FAZ, .DAT)")
            checked: page.st.pattern || false
            onToggled: (on) => page.put("pattern", on)
        }
        TField {
            visible: page.st.pattern || false
            label: qsTr("Имя файла")
            numeric: false
            mono: true
            value: page.st.patternFile || ""
            onEdited: (t) => page.put("patternFile", t)
        }
        Grid {
            visible: page.st.pattern || false
            width: parent.width
            columns: 2
            columnSpacing: Theme.spacing
            TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Накопление от"); unit: qsTr("нс"); value: page.st.pMin || ""; onEdited: (t) => page.put("pMin", t) }
            TField { width: (parent.width - Theme.spacing) / 2; label: qsTr("Накопление до"); unit: qsTr("нс"); value: page.st.pMax || ""; onEdited: (t) => page.put("pMax", t) }
        }
        TNote { text: page.st.patternInfo || ""; level: page.st.patternHeavy ? "warn" : "" }
    }

    TButton {
        width: page.colWidth
        primary: true
        text: qsTr("Далее: проверка")
        onClicked: page.next()
    }
}

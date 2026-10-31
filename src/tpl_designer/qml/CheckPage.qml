// Шаг 5 «Проверка»: замечания, сводка «что будет посчитано», текст файла .tpl и
// главная кнопка «Сгенерировать». При ошибках файл не пишется — как у оригинала.
// https://www.tamic.ru/docs/platforms/android.html#designer
pragma ComponentBehavior: Bound
import QtQuick
import TamicTplTheme

TPage {
    id: page

    required property DesignerModel model
    property string taskFolder: ""
    /// Страница на экране: память обновляется только тогда.
    property bool active: false
    signal generateRequested()
    signal saveSampleRequested()

    Timer {
        interval: 3000
        repeat: true
        running: page.active
        triggeredOnStart: true
        onTriggered: page.model.updateMemory()
    }

    TCard {
        width: page.colWidth
        title: qsTr("Проверка")
        Repeater {
            model: page.model.issues
            delegate: Row {
                id: issueRow
                required property var modelData
                width: parent.width
                spacing: 8
                Rectangle {
                    y: 7
                    width: 8
                    height: 8
                    radius: 4
                    color: issueRow.modelData.level === "error" ? Theme.danger
                         : issueRow.modelData.level === "warning" ? Theme.warn : Theme.ok
                }
                Text {
                    width: parent.width - 16
                    text: (issueRow.modelData.level === "error" ? qsTr("Ошибка: ")
                          : issueRow.modelData.level === "warning" ? qsTr("Внимание: ") : "") + issueRow.modelData.text
                    color: issueRow.modelData.level === "error" ? Theme.danger
                         : issueRow.modelData.level === "warning" ? Theme.warn : Theme.ok
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.Wrap
                }
            }
        }
        TButton {
            width: parent.width
            primary: true
            enabled: page.model.canGenerate
            text: qsTr("Сгенерировать")
            onClicked: page.generateRequested()
        }
        TButton {
            compact: true
            enabled: page.model.canGenerate
            text: qsTr("Сохранить как образец")
            onClicked: page.saveSampleRequested()
        }
        TNote {
            text: page.taskFolder.length > 0 ? qsTr("Папка задачи: ") + page.taskFolder
                                             : qsTr("Папка задачи не выбрана — выберите её на вкладке «Задачи».")
            level: page.taskFolder.length > 0 ? "" : "warn"
        }
    }

    TCard {
        width: page.colWidth
        title: qsTr("Память телефона")
        Row {
            width: parent.width
            spacing: Theme.spacing
            MemoryRing {
                id: memoryRing
                data: page.model.memory
            }
            Column {
                width: parent.width - memoryRing.width - Theme.spacing
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                TNote { width: parent.width; visible: page.model.memory.known || false; text: qsTr("Занято: %1").arg(page.model.memory.usedText || "") }
                TNote { width: parent.width; text: qsTr("Сетка расчёта: %1").arg(page.model.memory.gridText || "") }
                TNote { width: parent.width; visible: page.model.memory.patternOn || false
                        text: (page.model.memory.patternBorder ? qsTr("ДН по краю области: %1") : qsTr("ДН: %1")).arg(page.model.memory.patternText || "") }
                TNote { width: parent.width; visible: page.model.memory.known || false; text: qsTr("Свободно: %1").arg(page.model.memory.freeText || "") }
            }
        }
        TNote {
            text: page.model.memory.text || ""
            level: page.model.memory.level === "danger" ? "danger" : page.model.memory.level === "warn" ? "warn" : ""
        }
    }

    TCard {
        width: page.colWidth
        title: qsTr("Файл задания")
        Rectangle {
            width: parent.width
            height: Math.min(previewText.implicitHeight + 16, 420)
            radius: Theme.radiusSm
            color: Theme.bg
            border.width: Theme.borderW
            border.color: Theme.line
            Flickable {
                anchors.fill: parent
                anchors.margins: 8
                clip: true
                contentWidth: previewText.implicitWidth
                contentHeight: previewText.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                Text {
                    id: previewText
                    text: page.model.preview
                    color: Theme.text
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontSizeMono - 1
                    textFormat: Text.PlainText
                }
            }
        }
        TNote { text: qsTr("Файл пишется в CP1251 с переводами строк CRLF — как ждут ядра. Прокрутка — в обе стороны.") }
    }

    TCard {
        width: page.colWidth
        title: qsTr("Что будет посчитано")
        Text {
            width: parent.width
            text: page.model.summary
            color: Theme.textDim
            font.family: Theme.fontMono
            font.pixelSize: Theme.fontSizeCaption
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }
    }
}

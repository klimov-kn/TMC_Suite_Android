// Шаг 5 «Проверка»: замечания, сводка «что будет посчитано», текст файла .tpl и
// главная кнопка «Сгенерировать». При ошибках файл не пишется — как у оригинала.
pragma ComponentBehavior: Bound
import QtQuick
import TamicTplTheme

TPage {
    id: page

    required property DesignerModel model
    property string taskFolder: ""
    signal generateRequested()

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
        TNote {
            text: page.taskFolder.length > 0 ? qsTr("Папка задачи: ") + page.taskFolder
                                             : qsTr("Папка задачи не выбрана — выберите её на вкладке «Задачи».")
            level: page.taskFolder.length > 0 ? "" : "warn"
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

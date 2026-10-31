// «Справка»: о программе, язык интерфейса, руководство пользователя, память телефона,
// лицензии. Руководство (ManualScreen) открывается поверх на весь экран.
// https://www.tamic.ru/docs/platforms/android.html#app
import QtQuick

Item {
    id: screen

    property bool compact: false
    property bool startManual: false      // параметры запуска (снимки): сразу руководство
    property int startSection: -1         // … и его раздел
    property bool manualOpen: false
    property bool licensesOpen: false
    property real cache: -1               // служебные копии во внутренней памяти, байт

    // Назад (кнопка Android). true — обработано.
    function goBack() {
        if (manualLoader.item && manualLoader.item.goBack()) return true
        if (manualOpen) { manualOpen = false; return true }
        if (licensesOpen) { licensesOpen = false; return true }
        return false
    }

    Component.onCompleted: {
        cache = TaskStore.cacheSize()
        if (startManual)
            manualOpen = true
    }

    readonly property bool twoColumn: width > height && width >= 560

    ScreenHeader {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        compact: screen.compact
        title: qsTr("Справка")
        subtitle: qsTr("О программе, язык, руководство пользователя")
    }

    Flickable {
        id: flick
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom }
        anchors.topMargin: screen.compact ? 4 : Theme.spacing / 2
        clip: true
        contentWidth: width
        contentHeight: grid.height
        boundsBehavior: Flickable.StopAtBounds

        Grid {
            id: grid
            width: flick.width
            columns: screen.twoColumn ? 2 : 1
            columnSpacing: Theme.spacing
            rowSpacing: Theme.spacing
            readonly property real cellWidth: (width - columnSpacing * (columns - 1)) / columns

            // --- О программе ---------------------------------------------------
            Card {
                width: grid.cellWidth
                Row {
                    spacing: Theme.spacing
                    Image {
                        width: 56; height: 56
                        source: "qrc:/icon/tamic_icon_160.png"
                        sourceSize: Qt.size(112, 112)
                        smooth: true
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            text: "TMC Suite"
                            color: Theme.text
                            font.family: Theme.fontUi
                            font.pixelSize: Theme.fontSizeH2 + 4
                            font.weight: Theme.fontWeightTitle
                        }
                        Text {
                            text: qsTr("Версия %1").arg(Qt.application.version)
                            color: Theme.textDim
                            font.family: Theme.fontMono
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: qsTr("Мобильная версия TMC Suite: расчёт электромагнитных задач по веткам H и X прямо на телефоне и просмотр результатов — S-матрицы, диаграммы направленности, сигналы на портах и поле по тактам. Задания .tpl — те же, что у настольной версии; встроенный TPL Designer собирает их без набора текста.")
                    color: Theme.textDim
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeSmall
                }
                // Авторство и сайт.
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: qsTr("© 2026 К. Н. Климов и коллектив TAMIC")
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "<a href=\"https://www.tamic.ru\">www.tamic.ru</a>"
                    textFormat: Text.StyledText
                    linkColor: Theme.accent
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeSmall
                    MouseArea { anchors.fill: parent; acceptedButtons: Qt.NoButton; cursorShape: Qt.PointingHandCursor }
                }
                // Авторы в научных сетях и все репозитории (как в установщике Windows-версии).
                LinkIcons {
                    width: parent.width
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: qsTr("Исходный код открыт. Версии TMC Suite: Windows, Linux, macOS, Android — www.tamic.ru.")
                    color: Theme.textDim
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeSmall
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: qsTr("Используете результаты в статье, отчёте или диссертации — пожалуйста, процитируйте авторов и проект www.tamic.ru. Расчёт идёт на телефоне, без интернета; приложение не собирает и не передаёт данные.")
                    color: Theme.textMute
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeCaption
                }
            }

            // --- Язык ------------------------------------------------------------
            Card {
                width: grid.cellWidth
                Text {
                    text: qsTr("Язык интерфейса")
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                }
                Row {
                    spacing: 6
                    Chip { text: "Русский"; active: Lang.code === "ru"; onClicked: Lang.setCode("ru") }
                    Chip { text: "English"; active: Lang.code === "en"; onClicked: Lang.setCode("en") }
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: qsTr("Выбор запоминается. При первом запуске язык берётся из настроек телефона: русский — для русского, английский — для остальных.")
                    color: Theme.textMute
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeCaption
                }
            }

            // --- Руководство -------------------------------------------------------
            ListRow {
                width: grid.cellWidth
                title: qsTr("Руководство пользователя")
                subtitle: qsTr("Как считать задания, смотреть и сравнивать результаты — со снимками экрана")
                onClicked: screen.manualOpen = true
            }

            // --- Память ------------------------------------------------------------
            Card {
                width: grid.cellWidth
                Text {
                    text: qsTr("Память телефона")
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: qsTr("Служебные копии расчётов (рабочие папки и файлы поля для просмотра): %1. Результаты в папках задач удаляются на экране «Результаты».")
                          .arg(screen.cache >= 0 ? TaskStore.formatSize(screen.cache) : "…")
                    color: Theme.textDim
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeSmall
                }
                TamicButton {
                    danger: true
                    enabled: screen.cache > 0 && !Calc.busy
                    text: qsTr("Очистить")
                    onClicked: { TaskStore.clearCache(); screen.cache = TaskStore.cacheSize() }
                }
            }

            // --- Лицензии ----------------------------------------------------------
            ListRow {
                width: grid.cellWidth
                title: qsTr("Лицензии")
                subtitle: qsTr("TMC Suite — Apache 2.0; Qt — LGPLv3; шрифты — SIL OFL 1.1")
                onClicked: screen.licensesOpen = true
            }
        }
    }

    Sheet {
        id: licenses
        open: screen.licensesOpen
        onClosed: screen.licensesOpen = false
        onOpenChanged: if (!open) screen.licensesOpen = false
        title: qsTr("Лицензии")
        // Текст лицензии TMC Suite — без изменений (файл LICENSE).
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            text: AppInfo.license()
            color: Theme.text
            font.family: Theme.fontMono
            font.pixelSize: Theme.fontSizeCaption
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            topPadding: Theme.spacing
            text: qsTr("Qt %1 — библиотека интерфейса, GNU LGPL v3. Исходный код Qt: https://download.qt.io/; приложение собрано с Qt без изменений.\n\nInter (Rasmus Andersson) и JetBrains Mono (JetBrains) — шрифты, SIL Open Font License 1.1. Полные тексты лицензий шрифтов — ниже.").arg(AppInfo.qtVersion)
            color: Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            text: AppInfo.fontLicenses
            color: Theme.textDim
            font.family: Theme.fontMono
            font.pixelSize: Theme.fontSizeCaption
        }
        buttons: [ TamicButton { implicitWidth: 130; text: qsTr("Закрыть"); onClicked: licenses.close() } ]
    }

    // Руководство поверх экрана.
    Rectangle {
        anchors.fill: parent
        visible: screen.manualOpen
        color: Theme.bg
        z: 30
        MouseArea { anchors.fill: parent }
        Loader {
            id: manualLoader
            anchors.fill: parent
            active: screen.manualOpen
            sourceComponent: ManualScreen {
                compact: screen.compact
                startSection: screen.startSection
                onBack: screen.manualOpen = false
            }
        }
    }
}

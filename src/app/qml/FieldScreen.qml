// «Поле» результата (этап О4, модуль TamicFieldView на Qt RHI, решение M6): рельеф поля
// по тактам, топология, слои плазмы. Открывается из «Результатов» поверх списка.
// Просмотр читает файлы с диска: для папки SAF (Android) файлы поля берутся из рабочей
// папки расчёта или копируются во внутреннюю память (TaskStore.viewFolder).
import QtQuick
import TamicFieldView

Item {
    id: screen

    property string folder          // папка задачи (путь или content://)
    property string task            // задание (.tpl)
    property string file            // открытый файл поля (.ex, .ex.frames, .AMP, .FAZ, .tt)
    property var names: []          // файлы поля и топологии этого расчёта
    property bool compact: false
    property string options: ""     // параметры запуска для снимков: frame=<такт>, flat,
                                    // png=<папка> или gif=<файл> — выгрузить анимацию и выйти,
                                    // pngfull=<папка> — кадры всего экрана приложения (видео),
                                    // 2d (вид сверху), present (поле на весь экран), notopo,
                                    // nowire, zoom=<x>, gain=<контраст>,
                                    // gifnow — как кнопка «Экспорт анимации»
    signal back()

    readonly property bool landscape: width > height
    // Локальная папка готовится один раз при открытии (копирование может занять время).
    property string localFolder: ""
    property bool failed: false

    // --- Экспорт анимации -----------------------------------------------------------
    property var page: null          // FieldViewPage (когда загружена)
    property string exportName: ""
    property bool quitAfterExport: false

    function startExport(path, maxFrames, width) {
        if (!page || exporter.running)
            return
        exporter.start(page.sceneArea, page.scene, path, maxFrames, page.speed, width)
    }
    function exportGif() {
        const stamp = Qt.formatDateTime(new Date(), "yyyyMMdd-HHmmss")
        exportName = screen.sourceName(screen.file).replace(/\.[^.]*$/, "") + "_" + stamp + ".gif"
        startExport(TaskStore.tempPath(exportName), 160, 480)
    }

    FieldExporter {
        id: exporter
        onFinished: (ok, path) => {
            if (screen.quitAfterExport) {
                Qt.exit(ok ? 0 : 5)
                return
            }
            if (!ok) {
                toast.show(qsTr("Экспорт не удался: %1").arg(exporter.error), true)
                return
            }
            const saved = TaskStore.saveFile(path, screen.folder, screen.exportName)
            toast.show(saved ? qsTr("Анимация сохранена в папку задачи: %1").arg(screen.exportName)
                             : qsTr("Не удалось сохранить анимацию"), !saved)
            if (saved)
                shareBar.visible = true
        }
    }

    // Имя источника в FieldScene: серия тактов показывается под именем .ex.
    function sourceName(name) {
        return name.toLowerCase().endsWith(".frames") ? name.substring(0, name.length - 7) : name
    }

    Timer {
        id: prepare
        interval: 30
        running: true
        onTriggered: {
            screen.localFolder = TaskStore.viewFolder(screen.folder, screen.task, screen.names)
            screen.failed = screen.localFolder.length === 0
        }
    }

    ScreenHeader {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        compact: true
        backText: qsTr("Результаты")
        title: qsTr("Поле · %1").arg(screen.sourceName(screen.file))
        onBackClicked: screen.back()
    }

    Item {
        id: body
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom }
        anchors.topMargin: 4
        clip: true

        Loader {
            anchors.fill: parent
            active: screen.localFolder.length > 0
            sourceComponent: FieldViewPage {
                id: fieldPage
                resultFolder: screen.localFolder
                onExportRequested: screen.exportGif()
                showStats: !exporter.running
                Component.onDestruction: screen.page = null
                Connections {
                    target: fieldPage.scene
                    // Открыть тот файл, который нажали в списке.
                    function onSourcesChanged() {
                        const want = screen.sourceName(screen.file).toLowerCase()
                        const list = fieldPage.scene.sources
                        for (let i = 0; i < list.length; ++i)
                            if (list[i].toLowerCase() === want) { fieldPage.scene.sourceIndex = i; break }
                    }
                }
                // Снимки руководства: нужный такт без касаний.
                Timer {
                    interval: 600
                    running: screen.options.length > 0
                    onTriggered: {
                        const list = screen.options.split(",")
                        for (let i = 0; i < list.length; ++i) {
                            const o = list[i].trim()
                            if (o.indexOf("frame=") === 0) fieldPage.scene.frame = parseInt(o.substring(6))
                            else if (o === "flat") fieldPage.view.topology3D = false
                            else if (o === "gifnow") screen.exportGif()   // как кнопка «Экспорт»
                            else if (o === "2d") { fieldPage.view.relief = false; fieldPage.view.resetView() }
                            else if (o === "present") fieldPage.presentation = true
                            else if (o === "notopo") fieldPage.scene.topologyVisible = false
                            else if (o === "nowire") fieldPage.view.showWire = false
                            else if (o.indexOf("zoom=") === 0) fieldPage.view.zoom = parseFloat(o.substring(5))
                            else if (o.indexOf("gain=") === 0) fieldPage.scene.colorGain = parseFloat(o.substring(5))
                            else if (o.indexOf("png=") === 0 || o.indexOf("gif=") === 0) {
                                screen.quitAfterExport = true
                                exportDelay.path = o.substring(4)
                                exportDelay.start()
                            } else if (o.indexOf("pngfull=") === 0) {   // весь экран приложения (видео)
                                screen.quitAfterExport = true
                                exportDelay.full = true
                                exportDelay.path = o.substring(8)
                                exportDelay.start()
                            }
                        }
                    }
                }
                // Выгрузка для видео (ПК): после того как серия прочитана и вид установлен.
                Timer {
                    id: exportDelay
                    property string path: ""
                    property bool full: false
                    interval: 1500
                    onTriggered: {
                        if (full)
                            exporter.start(screen.Window.contentItem, fieldPage.scene, path, 100000,
                                           fieldPage.speed, 1280)
                        else
                            screen.startExport(path, 100000, 1280)
                    }
                }
                Component.onCompleted: {
                    screen.page = fieldPage
                    const want = screen.sourceName(screen.file).toLowerCase()
                    const list = fieldPage.scene.sources
                    for (let i = 0; i < list.length; ++i)
                        if (list[i].toLowerCase() === want) { fieldPage.scene.sourceIndex = i; break }
                }
            }
        }

        // Ход выгрузки анимации (при съёмке видео — не показывать, он попал бы в кадр).
        Rectangle {
            visible: exporter.running && !screen.quitAfterExport
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: Theme.spacing }
            height: exportCol.implicitHeight + Theme.spacing * 2
            radius: Theme.radiusSm
            color: Theme.surface2
            border.width: Theme.borderW
            border.color: Theme.accent
            z: 20
            Column {
                id: exportCol
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: Theme.spacing }
                spacing: Theme.spacing / 2
                Text {
                    width: parent.width
                    text: qsTr("Экспорт анимации: %1 %").arg(Math.round(exporter.progress * 100))
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: Font.DemiBold
                }
                ProgressLine { width: parent.width; value: exporter.progress * 100; animated: false }
                TamicButton { text: qsTr("Отмена"); onClicked: exporter.cancel() }
            }
        }

        // Готовая анимация — поделиться.
        Rectangle {
            id: shareBar
            visible: false
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: Theme.spacing }
            height: Theme.touchMin + Theme.spacing
            radius: Theme.radiusSm
            color: Theme.surface2
            border.width: Theme.borderW
            border.color: Theme.accent
            z: 20
            Text {
                anchors { left: parent.left; leftMargin: Theme.spacing; right: shareBtn.left; rightMargin: Theme.spacing
                          verticalCenter: parent.verticalCenter }
                text: screen.exportName
                color: Theme.text
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSizeCaption
                elide: Text.ElideMiddle
            }
            TamicButton {
                id: shareBtn
                anchors { right: closeShare.left; rightMargin: 4; verticalCenter: parent.verticalCenter }
                height: Theme.touchMin
                primary: true
                text: qsTr("Поделиться")
                onClicked: TaskStore.share(screen.folder, [screen.exportName])
            }
            Text {
                id: closeShare
                anchors { right: parent.right; rightMargin: Theme.spacing; verticalCenter: parent.verticalCenter }
                text: "×"
                color: Theme.textMute
                font.pixelSize: Theme.fontSizeH2
                MouseArea { anchors.fill: parent; anchors.margins: -10; onClicked: shareBar.visible = false }
            }
        }

        Rectangle {
            id: toast
            property bool bad: false
            function show(msg, isBad) { toastText.text = msg; bad = isBad; opacity = 1; toastTimer.restart() }
            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: Theme.spacing }
            width: Math.min(parent.width - 2 * Theme.spacing, toastText.implicitWidth + 2 * Theme.gutter)
            height: toastText.implicitHeight + Theme.spacing
            radius: Theme.radiusSm
            color: Theme.surface2
            border.width: Theme.borderW
            border.color: bad ? Theme.danger : Theme.accent
            opacity: 0
            visible: opacity > 0
            z: 21
            Behavior on opacity { NumberAnimation { duration: Theme.durMs } }
            Text {
                id: toastText
                anchors.centerIn: parent
                width: Math.min(implicitWidth, toast.parent.width - 2 * Theme.spacing - 2 * Theme.gutter)
                wrapMode: Text.Wrap
                color: toast.bad ? Theme.danger : Theme.text
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
            }
            Timer { id: toastTimer; interval: 4000; onTriggered: toast.opacity = 0 }
        }

        Column {
            anchors.centerIn: parent
            width: parent.width - 2 * Theme.gutter
            spacing: Theme.spacing
            visible: screen.localFolder.length === 0
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: screen.failed ? qsTr("Не удалось прочитать файлы поля из папки задачи")
                                    : qsTr("Подготовка файлов поля…")
                color: screen.failed ? Theme.danger : Theme.textDim
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeBody
            }
        }
    }
}

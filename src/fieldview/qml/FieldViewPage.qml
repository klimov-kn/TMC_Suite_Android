// Экран «Поле»: рельеф поля по тактам, топология, плазменные слои ε/N/Y/B.
//
// Подключение (README.md): import TamicFieldView
//     FieldViewPage { resultFolder: "<папка результатов задачи>" }   // или fieldFile: ".../voln.ex"
//
// Сцена — FieldRhiItem (Qt RHI); надписи, шкала и кнопки — Qt Quick поверх.
// Вертикально — сцена сверху, управление снизу; горизонтально — сцена на всю высоту,
// панель сбоку. Сцена одна на оба положения: вид и такт при повороте сохраняются.
// Жесты: один палец — поворот, щипок — масштаб, два пальца — сдвиг, двойное касание —
// исходный вид. На компьютере: правая кнопка — сдвиг, колесо — масштаб.
// https://www.tamic.ru/docs/platforms/android.html#field
import QtQuick
import TamicTplTheme
import TamicFieldView

Item {
    id: root

    /// Папка результатов расчёта (локальный путь). Берётся первый файл поля в ней.
    property string resultFolder: ""
    /// Конкретный файл поля (.ex, .ex.frames, .AMP, .FAZ) — вместо папки.
    property string fieldFile: ""
    /// Следить за файлом и перечитывать при изменении (живой расчёт, как OnTimer Windows).
    property alias watch: scene.watch

    property alias scene: scene
    property alias view: field
    /// Сцена с надписями и шкалой — её снимает экспорт анимации (FieldExporter).
    property alias sceneArea: sceneArea
    /// Кнопка «Экспорт» в панели: обработчик — у того, кто встроил страницу.
    signal exportRequested()
    /// Служебная строка (видеоинтерфейс, кадр/с) — прячется на время экспорта анимации.
    property bool showStats: true
    /// Показ (съёмка видео): поле на всю страницу, без панели управления.
    property bool presentation: false
    property bool playing: false
    property int speed: 8           // кадр/с: 1…30 (ползунок); Windows-FieldView — таймер 50 мс

    readonly property bool landscape: width > height
    readonly property bool compact: height < Theme.compactHeight

    FieldScene {
        id: scene
        folder: root.resultFolder
        file: root.fieldFile
    }

    Timer {
        interval: Math.max(10, Math.round(1000 / root.speed))
        repeat: true
        running: root.playing && scene.frameCount > 1
        onTriggered: scene.stepFrame(1)
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    Item {
        id: sceneArea
        x: 0
        y: 0
        width: root.presentation ? root.width : root.landscape ? root.width - panel.width : root.width
        height: root.presentation ? root.height : root.landscape ? root.height : Math.round(root.height * 0.56)
        clip: true

        FieldRhiItem {
            id: field
            anchors.fill: parent
            scene: scene
            visible: scene.loaded
        }

        // --- мышь на ПК ---
        // Касания разбирает сам FieldRhiItem (src/ui/touchgesture.h).
        DragHandler {
            id: rotateDrag
            target: null
            acceptedDevices: PointerDevice.Mouse
            acceptedButtons: Qt.LeftButton
            property vector2d last: Qt.vector2d(0, 0)
            onActiveChanged: last = Qt.vector2d(0, 0)
            onActiveTranslationChanged: {
                field.rotateBy(activeTranslation.x - last.x, activeTranslation.y - last.y)
                last = activeTranslation
            }
        }
        DragHandler {
            // Сдвиг правой кнопкой мыши (компьютер).
            target: null
            acceptedDevices: PointerDevice.Mouse
            acceptedButtons: Qt.RightButton
            property vector2d last: Qt.vector2d(0, 0)
            onActiveChanged: last = Qt.vector2d(0, 0)
            onActiveTranslationChanged: {
                field.panBy(activeTranslation.x - last.x, activeTranslation.y - last.y)
                last = activeTranslation
            }
        }
        WheelHandler {
            target: null
            onWheel: (event) => field.zoomBy(Math.pow(1.0015, event.angleDelta.y))
        }
        TapHandler {
            acceptedDevices: PointerDevice.Mouse
            onDoubleTapped: field.resetView()
        }

        // --- надписи поверх сцены ---
        Repeater {
            model: field.portLabels
            delegate: Text {
                required property var modelData
                x: modelData.x + 4
                y: modelData.y - height / 2
                text: modelData.text
                color: Theme.accent
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSizeCaption
                visible: scene.topologyVisible && scene.loaded
            }
        }

        Column {
            id: info
            x: Theme.gutter
            y: Theme.gutter / 2
            spacing: 2
            visible: scene.loaded
            Text {
                text: scene.fileName + (scene.layer === 0 ? "" : "  ·  " + ["", "ε", "N", "Y", "B"][scene.layer])
                color: Theme.text
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
                font.weight: Font.DemiBold
            }
            Text {
                text: "nT = " + scene.nT + "   t = " + scene.timeText
                color: Theme.textDim
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSizeCaption
            }
            Text {
                text: scene.gridText
                color: Theme.textMute
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSizeCaption
                visible: !root.compact || root.landscape
            }
        }

        FvScaleBar {
            anchors.right: parent.right
            anchors.rightMargin: Theme.gutter / 2
            anchors.verticalCenter: parent.verticalCenter
            height: Math.min(parent.height * 0.62, 260)
            colors: scene.scaleColors
            minValue: scene.scaleMin
            maxValue: scene.scaleMax
            unit: scene.scaleUnit
            visible: scene.loaded
        }

        Text {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: Theme.gutter / 2
            text: field.backendName + (field.fps > 0 ? "  ·  " + qsTr("%1 к/с").arg(field.fps.toFixed(0)) : "")
                  + (scene.indexProgress < 1 ? "  ·  " + qsTr("размах тактов %1 %").arg(Math.round(scene.indexProgress * 100)) : "")
            color: Theme.textMute
            font.family: Theme.fontMono
            font.pixelSize: Theme.fontSizeCaption
            visible: scene.loaded && root.showStats
        }

        Column {
            anchors.centerIn: parent
            width: parent.width - 2 * Theme.gutter
            spacing: Theme.spacing
            visible: !scene.loaded
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Нет данных поля")
                color: Theme.text
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeH2
                font.weight: Font.Bold
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: scene.error.length > 0 ? scene.error
                                             : qsTr("Откройте результаты расчёта с включёнными FIELDS и TOPOLOGY.")
                color: scene.error.length > 0 ? Theme.warn : Theme.textDim
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }

    Rectangle {
        // Разделитель сцены и панели.
        visible: !root.presentation
        color: Theme.line
        x: root.landscape ? sceneArea.width : 0
        y: root.landscape ? 0 : sceneArea.height
        width: root.landscape ? Theme.borderW : root.width
        height: root.landscape ? root.height : Theme.borderW
    }

    FvControls {
        id: panel
        visible: !root.presentation
        x: root.landscape ? root.width - width : 0
        y: root.landscape ? 0 : sceneArea.height + Theme.borderW
        width: root.landscape ? Math.min(380, Math.round(root.width * 0.4)) : root.width
        height: root.landscape ? root.height : root.height - sceneArea.height - Theme.borderW
        scene: scene
        field: field
        page: root
    }
}

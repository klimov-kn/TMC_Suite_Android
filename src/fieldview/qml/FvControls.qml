// Панель управления экрана «Поле»: такты (пуск/пауза, шаг, ползунок, скорость),
// слой (поле / ε / N / Y / B), вид (рельеф, каркас, топология, модуль, шкала), файл.
import QtQuick
import TamicTplTheme

Rectangle {
    id: panel
    property var scene
    property var field
    property var page

    color: Theme.bg2

    Flickable {
        anchors.fill: parent
        contentHeight: col.height + 2 * Theme.gutter
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: col
            x: Theme.gutter
            y: Theme.gutter
            width: parent.width - 2 * Theme.gutter
            spacing: Theme.spacing

            // --- такты ---
            Row {
                spacing: Theme.spacing
                FvButton {
                    icon: "prev"
                    enabled: panel.scene.frameCount > 1
                    onClicked: { panel.page.playing = false; panel.scene.stepFrame(-1) }
                }
                FvButton {
                    icon: panel.page.playing ? "pause" : "play"
                    primary: true
                    enabled: panel.scene.frameCount > 1
                    onClicked: panel.page.playing = !panel.page.playing
                }
                FvButton {
                    icon: "next"
                    enabled: panel.scene.frameCount > 1
                    onClicked: { panel.page.playing = false; panel.scene.stepFrame(1) }
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: panel.scene.frameCount > 0
                              ? qsTr("Такт %1 из %2").arg(panel.scene.frame + 1).arg(panel.scene.frameCount)
                              : qsTr("Тактов нет")
                        color: Theme.text
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fontSizeSmall
                    }
                    Text {
                        text: "nT " + panel.scene.nT
                        color: Theme.textDim
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontSizeCaption
                    }
                }
            }

            FvSlider {
                width: parent.width
                from: 0
                to: Math.max(0, panel.scene.frameCount - 1)
                value: panel.scene.frame
                enabled: panel.scene.frameCount > 1
                onMoved: (v) => { panel.page.playing = false; panel.scene.frame = Math.round(v) }
            }

            // Скорость проигрывания: плавно от 1 до 30 кадров в секунду.
            Text {
                text: qsTr("Скорость: %1 кадр/с").arg(panel.page.speed)
                color: Theme.textDim
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeCaption
            }
            FvSlider {
                width: parent.width
                from: 1
                to: 30
                value: panel.page.speed
                onMoved: (v) => panel.page.speed = Math.round(v)
            }

            // --- слой ---
            Text {
                text: qsTr("Слой")
                color: Theme.textDim
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeCaption
                visible: panel.scene.hasTopology
            }
            Flow {
                width: parent.width
                spacing: 8
                visible: panel.scene.hasTopology
                Repeater {
                    model: [
                        { name: qsTr("Поле"), layer: 0, plasma: false },
                        { name: "ε", layer: 1, plasma: false },
                        { name: "N", layer: 2, plasma: true },
                        { name: "Y", layer: 3, plasma: true },
                        { name: "B", layer: 4, plasma: true }
                    ]
                    delegate: FvChip {
                        required property var modelData
                        text: modelData.name
                        visible: !modelData.plasma || panel.scene.hasPlasma
                        checked: panel.scene.layer === modelData.layer
                        onClicked: panel.scene.layer = modelData.layer
                    }
                }
            }

            // --- вид ---
            Text {
                text: qsTr("Вид")
                color: Theme.textDim
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeCaption
            }
            Flow {
                width: parent.width
                spacing: 8
                FvChip {
                    text: qsTr("Рельеф")
                    checked: panel.field.relief
                    onClicked: { panel.field.relief = !panel.field.relief; panel.field.resetView() }
                }
                FvChip {
                    text: qsTr("Заливка")
                    checked: panel.field.showSurface
                    onClicked: panel.field.showSurface = !panel.field.showSurface
                }
                FvChip {
                    text: qsTr("Каркас")
                    checked: panel.field.showWire
                    onClicked: panel.field.showWire = !panel.field.showWire
                }
                FvChip {
                    text: qsTr("Топология")
                    checked: panel.scene.topologyVisible
                    visible: panel.scene.hasTopology
                    onClicked: panel.scene.topologyVisible = !panel.scene.topologyVisible
                }
                FvChip {
                    // Как кнопка «3D» Windows-FieldView: блоки геометрии с высотой или плоские.
                    text: qsTr("Геометрия 3D")
                    checked: panel.field.topology3D
                    visible: panel.scene.hasTopology && panel.scene.topologyVisible && panel.field.relief
                    onClicked: panel.field.topology3D = !panel.field.topology3D
                }
                FvChip {
                    text: "|E|"
                    checked: panel.scene.modulus
                    visible: panel.scene.layer === 0
                    onClicked: panel.scene.modulus = !panel.scene.modulus
                }
                FvChip {
                    text: qsTr("20 уровней")
                    checked: panel.field.levels === 20
                    onClicked: panel.field.levels = panel.field.levels === 20 ? 0 : 20
                }
                FvChip {
                    text: panel.scene.globalScale ? qsTr("Шкала: все такты") : qsTr("Шкала: такт")
                    checked: panel.scene.globalScale
                    visible: panel.scene.layer === 0 && panel.scene.frameCount > 1
                    onClicked: panel.scene.globalScale = !panel.scene.globalScale
                }
                FvChip {
                    text: qsTr("Сброс вида")
                    onClicked: panel.field.resetView()
                }
                FvChip {
                    text: qsTr("Экспорт анимации (GIF)")
                    visible: panel.scene.frameCount > 1
                    onClicked: panel.page.exportRequested()
                }
            }

            // --- файл ---
            Text {
                text: qsTr("Файл поля")
                color: Theme.textDim
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeCaption
                visible: panel.scene.sources.length > 1
            }
            Flow {
                width: parent.width
                spacing: 8
                visible: panel.scene.sources.length > 1
                Repeater {
                    model: panel.scene.sources
                    delegate: FvChip {
                        required property string modelData
                        required property int index
                        text: modelData
                        checked: panel.scene.sourceIndex === index
                        onClicked: panel.scene.sourceIndex = index
                    }
                }
            }
        }
    }
}

// Переключатель ветки ядра H / X (решение M22): две половины, выбранная — accent.
import QtQuick

Rectangle {
    id: sw

    property string mode: "H"
    property bool enabledSwitch: true
    signal picked(string mode)

    implicitHeight: Theme.touchMin + 8
    radius: Theme.radiusSm
    color: Theme.bg
    border.width: Theme.borderW
    border.color: Theme.line

    Row {
        anchors.fill: parent
        anchors.margins: 4
        Repeater {
            model: [
                { key: "H", hint: qsTr("ядро tmc_rth") },
                { key: "X", hint: qsTr("tmc_rtx, плазма") }
            ]
            delegate: Rectangle {
                id: seg
                required property var modelData
                readonly property bool active: sw.mode === modelData.key
                width: (sw.width - 8) / 2
                height: sw.height - 8
                radius: Theme.radiusSm - 3
                color: active ? Theme.accent : (segArea.pressed ? Theme.surface2 : "transparent")
                Behavior on color {
                    ColorAnimation { duration: Theme.durMs; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeCurve }
                }
                Row {
                    anchors.centerIn: parent
                    spacing: 8
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: seg.modelData.key
                        color: seg.active ? Theme.textOnAccent : Theme.text
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontSizeH2
                        font.weight: Font.Bold
                    }
                    Text {
                        // узкий переключатель (горизонтальное положение) — только буква
                        visible: seg.width >= 150
                        anchors.verticalCenter: parent.verticalCenter
                        text: seg.modelData.hint
                        color: seg.active ? Theme.textOnAccent : Theme.textDim
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fontSizeCaption
                        font.weight: seg.active ? Font.DemiBold : Font.Normal
                    }
                }
                MouseArea {
                    id: segArea
                    anchors.fill: parent
                    enabled: sw.enabledSwitch
                    onClicked: sw.picked(seg.modelData.key)
                }
            }
        }
    }
}

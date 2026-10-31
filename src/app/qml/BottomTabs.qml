// Вкладки: фон surface, активная вкладка — значок и подпись accent.
// Вертикальное положение телефона — панель внизу; горизонтальное (vertical: true) —
// боковая полоса слева, чтобы не отнимать и без того малую высоту экрана.
// https://www.tamic.ru/docs/platforms/android.html#app
import QtQuick

Rectangle {
    id: bar

    property int current: 0
    property bool vertical: false
    property var tabs: [
        { kind: "tasks",    label: qsTr("Задачи") },
        { kind: "designer", label: qsTr("TPL Designer") },
        { kind: "results",  label: qsTr("Результаты") },
        { kind: "about",    label: qsTr("Справка") }
    ]
    signal selected(int index)

    implicitHeight: Theme.tabBarHeight
    implicitWidth: Theme.railWidth
    color: Theme.surface

    // Разделитель: сверху у нижней панели, справа у боковой.
    Rectangle {
        x: bar.vertical ? bar.width - width : 0
        width: bar.vertical ? Theme.borderW : bar.width
        height: bar.vertical ? bar.height : Theme.borderW
        color: Theme.line
    }

    Grid {
        anchors.fill: parent
        anchors.topMargin: bar.vertical ? Theme.spacing : 0
        columns: bar.vertical ? 1 : bar.tabs.length
        Repeater {
            model: bar.tabs
            delegate: Item {
                id: tab
                required property var modelData
                required property int index
                readonly property bool active: bar.current === index
                width: bar.vertical ? bar.width - Theme.borderW : bar.width / bar.tabs.length
                height: bar.vertical ? Theme.tabBarHeight + Theme.spacing : bar.height

                Column {
                    anchors.centerIn: parent
                    spacing: 4
                    TabIcon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        kind: tab.modelData.kind
                        color: tab.active ? Theme.accent : Theme.textMute
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.min(implicitWidth, tab.width - 8)
                        horizontalAlignment: Text.AlignHCenter
                        text: tab.modelData.label
                        color: tab.active ? Theme.accent : Theme.textMute
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fontSizeCaption
                        font.weight: tab.active ? Font.DemiBold : Font.Normal
                        wrapMode: bar.vertical ? Text.Wrap : Text.NoWrap
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: bar.selected(tab.index)
                }
            }
        }
    }
}

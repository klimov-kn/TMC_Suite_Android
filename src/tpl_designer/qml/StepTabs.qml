// Вкладки шагов: номер и подпись; активная — accent (как активная вкладка оболочки).
// Ширина делится поровну, у каждой вкладки касание не меньше 48 dp.
pragma ComponentBehavior: Bound
import QtQuick
import TamicTplTheme

Row {
    id: tabs

    property var labels: []
    property int current: 0
    signal picked(int index)

    Repeater {
        model: tabs.labels
        delegate: Item {
            id: tab
            required property int index
            required property string modelData
            readonly property bool active: tabs.current === index
            width: tabs.width / Math.max(1, tabs.labels.length)
            height: tabs.height

            Column {
                anchors.centerIn: parent
                spacing: 2
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 22
                    height: 22
                    radius: 11
                    color: tab.active ? Theme.accent : "transparent"
                    border.width: Theme.borderW
                    border.color: tab.active ? Theme.accent : Theme.line
                    Text {
                        anchors.centerIn: parent
                        text: tab.index + 1
                        color: tab.active ? Theme.textOnAccent : Theme.textDim
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontSizeCaption
                        font.weight: Font.Bold
                    }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(implicitWidth, tab.width - 4)
                    horizontalAlignment: Text.AlignHCenter
                    text: tab.modelData
                    color: tab.active ? Theme.accent : Theme.textDim
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeCaption
                    font.weight: Font.Normal
                    elide: Text.ElideRight
                }
            }
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 10; rightMargin: 10 }
                height: 2
                radius: 1
                color: Theme.accent
                visible: tab.active
            }
            MouseArea {
                anchors.fill: parent
                onClicked: tabs.picked(tab.index)
            }
        }
    }
}

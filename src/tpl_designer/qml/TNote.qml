// Пояснение под полем: по умолчанию text-mute; level: "warn" | "danger" | "ok" | "dim".
// https://www.tamic.ru/docs/platforms/android.html#designer
import QtQuick
import TamicTplTheme

Text {
    property string level: ""
    width: parent ? parent.width : implicitWidth
    wrapMode: Text.Wrap
    color: level === "danger" ? Theme.danger
         : level === "warn" ? Theme.warn
         : level === "ok" ? Theme.ok
         : level === "dim" ? Theme.textDim : Theme.textMute
    font.family: Theme.fontUi
    font.pixelSize: Theme.fontSizeSmall
    visible: text.length > 0
}

// Руководство пользователя внутри приложения: оглавление → раздел (текст и снимки
// экрана). Текст и снимки — на языке интерфейса (AppInfo.manual, ресурсы :/manual).
// Касание снимка — снимок на весь экран.
// https://www.tamic.ru/docs/platforms/android.html#app
import QtQuick

Item {
    id: screen

    property bool compact: false
    property int startSection: -1          // параметры запуска (снимки): открыть раздел
    signal back()

    Component.onCompleted: if (startSection >= 0) current = startSection

    readonly property var sections: { Lang.code; return AppInfo.manual(Lang.code) }
    property int current: -1               // открытый раздел; −1 — оглавление
    property string zoomSrc: ""

    function goBack() {
        if (zoomSrc) { zoomSrc = ""; return true }
        if (current >= 0) { current = -1; return true }
        return false
    }
    function imageUrl(src) { return "qrc:/manual/" + Lang.code + "/" + src }

    ScreenHeader {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        compact: true
        backText: screen.current >= 0 ? qsTr("Оглавление") : qsTr("Справка")
        title: screen.current >= 0 ? screen.sections[screen.current].title : qsTr("Руководство")
        onBackClicked: if (!screen.goBack()) screen.back()
    }

    // --- Оглавление --------------------------------------------------------------
    ListView {
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom }
        anchors.topMargin: Theme.spacing / 2
        visible: screen.current < 0
        clip: true
        spacing: Theme.spacing / 2
        model: screen.sections
        delegate: ListRow {
            required property var modelData
            required property int index
            width: ListView.view.width
            title: (index + 1) + ". " + modelData.title
            subtitle: modelData.summary || ""
            onClicked: { screen.current = index; page.contentY = 0 }
        }
    }

    // --- Раздел --------------------------------------------------------------------
    Flickable {
        id: page
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom }
        anchors.topMargin: Theme.spacing / 2
        visible: screen.current >= 0
        clip: true
        contentWidth: width
        contentHeight: body.implicitHeight + Theme.gutter
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: body
            width: Math.min(page.width, 720)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing

            Repeater {
                model: screen.current >= 0 ? screen.sections[screen.current].blocks : []
                delegate: Loader {
                    id: block
                    required property var modelData
                    width: body.width
                    sourceComponent: modelData.type === "img" ? imgBlock
                                   : modelData.type === "h" ? headBlock
                                   : modelData.type === "list" ? listBlock
                                   : modelData.type === "note" ? noteBlock : textBlock
                    property var data_: modelData
                }
            }

            // Следующий раздел.
            ListRow {
                visible: screen.current >= 0 && screen.current < screen.sections.length - 1
                width: body.width
                title: visible ? qsTr("Дальше: %1").arg(screen.sections[screen.current + 1].title) : ""
                onClicked: { screen.current = screen.current + 1; page.contentY = 0 }
            }
        }
    }

    Component {
        id: textBlock
        Text {
            text: parent ? parent.data_.text : ""
            wrapMode: Text.Wrap
            textFormat: Text.StyledText
            color: Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeBody
            lineHeight: 1.15
        }
    }
    Component {
        id: headBlock
        Text {
            text: parent ? parent.data_.text : ""
            wrapMode: Text.Wrap
            topPadding: Theme.spacing / 2
            color: Theme.accent
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeH2
            font.weight: Font.Bold
        }
    }
    Component {
        id: noteBlock
        Rectangle {
            implicitHeight: noteText.implicitHeight + Theme.spacing * 2
            radius: Theme.radiusSm
            color: Theme.surface
            border.width: Theme.borderW
            border.color: Theme.accent
            Text {
                id: noteText
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: Theme.spacing }
                text: parent.parent ? parent.parent.data_.text : ""
                wrapMode: Text.Wrap
                textFormat: Text.StyledText
                color: Theme.textDim
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }
    Component {
        id: listBlock
        Column {
            spacing: 6
            Repeater {
                model: parent && parent.parent ? parent.parent.data_.items : []
                delegate: Row {
                    required property string modelData
                    spacing: 8
                    Text { text: "•"; color: Theme.accent; font.pixelSize: Theme.fontSizeBody; font.family: Theme.fontUi }
                    Text {
                        width: body.width - 24
                        text: modelData
                        wrapMode: Text.Wrap
                        textFormat: Text.StyledText
                        color: Theme.text
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fontSizeBody
                    }
                }
            }
        }
    }
    Component {
        id: imgBlock
        Column {
            spacing: 4
            readonly property var d: parent ? parent.data_ : ({})
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: shot.paintedWidth + 2
                height: shot.paintedHeight + 2
                color: "transparent"
                radius: Theme.radiusSm
                border.width: Theme.borderW
                border.color: Theme.line
                Image {
                    id: shot
                    anchors.centerIn: parent
                    // Вертикальный снимок — не шире 300, горизонтальный — во всю ширину.
                    width: Math.min(body.width - 2, (sourceSize.width > sourceSize.height ? body.width : 300))
                    height: sourceSize.width > 0 ? width * sourceSize.height / sourceSize.width : 0
                    fillMode: Image.PreserveAspectFit
                    source: d.src ? screen.imageUrl(d.src) : ""
                    asynchronous: true
                    smooth: true
                    mipmap: true
                }
                MouseArea { anchors.fill: parent; onClicked: screen.zoomSrc = d.src }
            }
            Text {
                visible: (d.text || "").length > 0
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: d.text || ""
                wrapMode: Text.Wrap
                color: Theme.textMute
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeCaption
            }
        }
    }

    // Снимок на весь экран.
    Rectangle {
        anchors.fill: parent
        visible: screen.zoomSrc.length > 0
        color: Theme.bg
        z: 10
        Image {
            anchors.fill: parent
            anchors.margins: Theme.spacing
            fillMode: Image.PreserveAspectFit
            source: screen.zoomSrc ? screen.imageUrl(screen.zoomSrc) : ""
            smooth: true
            mipmap: true
        }
        MouseArea { anchors.fill: parent; onClicked: screen.zoomSrc = "" }
    }
}

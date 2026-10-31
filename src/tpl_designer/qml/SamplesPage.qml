// Библиотека образцов на весь экран (настольный SamplesPage): разделы и поиск, плитки с картинками,
// карточка образца с параметрами, проверкой, паспортом и числами ядра.
// https://www.tamic.ru/docs/code/tpl-designer.html#gui-samples
pragma ComponentBehavior: Bound
import QtQuick
import TamicTplTheme

Rectangle {
    id: page

    required property DesignerModel model
    property var sections: []
    property string section: "all"
    property string search: ""
    property bool cardOpen: false
    readonly property var card: model.sampleCard
    readonly property var canvasColors: ({
        bg: Theme.bg, grid: Theme.line, box: Theme.textMute, boxFill: Theme.surface,
        text: Theme.text, metal: Theme.text, magnetic: Theme.accent2, absorber: Theme.warn,
        dielectric: Theme.accent, plasma: Theme.accent3, plasmaB: Theme.accent2,
        plasmaY: Theme.textDim, active: Theme.ok, passive: Theme.danger,
        select: Theme.accent, snap: Theme.accent3
    })
    signal closeRequested()
    signal opened(string message)

    color: Theme.bg
    z: 90

    function reload() {
        sections = model.sampleSections()
        cardOpen = false
    }

    function goBack() {
        if (cardOpen) { cardOpen = false; return true }
        return false
    }

    readonly property var tiles: {
        const out = []
        const q = search.trim().toLowerCase()
        for (const sec of sections) {
            if (section !== "all" && sec.id !== section) continue
            for (const it of sec.items) {
                if (q.length > 0 && (it.title + " " + it.summary + " " + it.standard).toLowerCase().indexOf(q) < 0)
                    continue
                out.push(it)
            }
        }
        return out
    }

    // --- шапка ---------------------------------------------------------------
    Item {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: Theme.touchMin + 8
        TToolButton {
            anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
            icon: "back"
            onClicked: { if (!page.goBack()) page.closeRequested() }
        }
        Text {
            anchors { left: parent.left; leftMargin: Theme.touchMin + 8; verticalCenter: parent.verticalCenter }
            text: page.cardOpen ? qsTr("Образец") : qsTr("Библиотека образцов")
            color: Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeH2
            font.weight: Font.Bold
        }
    }

    // --- каталог -------------------------------------------------------------
    Column {
        id: filters
        visible: !page.cardOpen
        anchors { left: parent.left; right: parent.right; top: header.bottom; leftMargin: Theme.gutter; rightMargin: Theme.gutter }
        spacing: Theme.spacing / 2
        TField {
            width: parent.width
            numeric: false
            label: qsTr("Поиск")
            value: page.search
            onEdited: (t) => page.search = t
        }
        Flickable {
            width: parent.width
            height: 40
            contentWidth: chips.implicitWidth
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            Row {
                id: chips
                spacing: 6
                Repeater {
                    model: [{ id: "all", title: qsTr("Все") }].concat(page.sections)
                    delegate: Rectangle {
                        id: chip
                        required property var modelData
                        readonly property bool active: page.section === modelData.id
                        width: chipText.implicitWidth + 24
                        height: 34
                        radius: Theme.radiusSm
                        color: active ? Theme.accent : Theme.surface
                        border.width: Theme.borderW
                        border.color: active ? Theme.accent : Theme.line
                        Text {
                            id: chipText
                            anchors.centerIn: parent
                            text: chip.modelData.title
                            color: chip.active ? Theme.textOnAccent : Theme.text
                            font.family: Theme.fontUi
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        MouseArea { anchors.fill: parent; onClicked: page.section = chip.modelData.id }
                    }
                }
            }
        }
    }

    GridView {
        id: grid
        visible: !page.cardOpen
        anchors { left: parent.left; right: parent.right; top: filters.bottom; bottom: parent.bottom
                  leftMargin: Theme.gutter; rightMargin: Theme.gutter; topMargin: Theme.spacing / 2 }
        clip: true
        cellWidth: width / Math.max(1, Math.floor(width / 150))
        cellHeight: 186
        cacheBuffer: 0
        boundsBehavior: Flickable.StopAtBounds
        model: page.tiles
        delegate: Item {
            id: tile
            required property var modelData
            width: grid.cellWidth
            height: grid.cellHeight
            Rectangle {
                anchors { fill: parent; margins: 4 }
                radius: Theme.radiusSm
                color: tileArea.pressed ? Theme.surface2 : Theme.surface
                border.width: Theme.borderW
                border.color: Theme.line
                clip: true
                SamplePreview {
                    id: thumb
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 4 }
                    height: 104
                    colors: page.canvasColors
                    source: page.model
                    sampleKey: tile.modelData.key
                }
                Column {
                    anchors { left: parent.left; right: parent.right; top: thumb.bottom; margins: 8 }
                    spacing: 2
                    Text {
                        width: parent.width
                        text: tile.modelData.title + (tile.modelData.kernel === "X" ? "  · X" : "")
                        color: Theme.text
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fontSizeSmall
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: tile.modelData.summary
                        color: Theme.textMute
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fontSizeCaption
                        maximumLineCount: 1
                        elide: Text.ElideRight
                    }
                }
                MouseArea {
                    id: tileArea
                    anchors.fill: parent
                    onClicked: { page.model.openSampleCard(tile.modelData.key); page.cardOpen = true }
                }
            }
        }
    }

    // --- карточка ------------------------------------------------------------
    TPage {
        visible: page.cardOpen
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom }

        TCard {
            width: parent ? parent.width : 300
            title: page.card.title || ""
            SamplePreview {
                width: parent.width
                height: Math.max(130, Math.min(260, width * 0.6))
                colors: page.canvasColors
                source: page.model
            }
            TNote { visible: (page.card.meta || "").length > 0; text: page.card.meta || "" }
            TNote { visible: (page.card.summary || "").length > 0; text: page.card.summary || "" }
            TNote { text: page.card.check || ""; level: page.card.ok ? "" : "danger" }
            Repeater {
                model: page.card.errors || []
                delegate: TNote { required property string modelData; text: "• " + modelData; level: "danger" }
            }
            Repeater {
                model: page.card.warnings || []
                delegate: TNote { required property string modelData; text: "• " + modelData; level: "warn" }
            }
            TButton {
                primary: true
                enabled: page.card.canOpen || false
                text: qsTr("Открыть в редакторе")
                onClicked: page.opened(page.model.openCardInEditor())
            }
            TButton {
                visible: page.card.user || false
                danger: true
                compact: true
                text: qsTr("Удалить образец")
                onClicked: { page.model.deleteUserSample(); page.reload() }
            }
        }

        TCard {
            width: parent ? parent.width : 300
            visible: (page.card.params || []).length > 0
            title: qsTr("Параметры")
            Repeater {
                model: page.card.params || []
                delegate: Column {
                    id: param
                    required property var modelData
                    width: parent ? parent.width : 300
                    TField {
                        visible: param.modelData.kind === "number"
                        width: parent.width
                        label: param.modelData.label
                        unit: param.modelData.unit
                        value: param.modelData.value
                        onEdited: (t) => page.model.setSampleParam(param.modelData.key, t)
                    }
                    TSwitchRow {
                        visible: param.modelData.kind === "flag"
                        text: param.modelData.label
                        hint: param.modelData.hint
                        checked: param.modelData.checked
                        onToggled: (on) => page.model.setSampleParam(param.modelData.key, on)
                    }
                    Column {
                        visible: param.modelData.kind === "choice"
                        width: parent.width
                        spacing: 4
                        Text {
                            width: parent.width
                            text: param.modelData.label
                            color: Theme.textDim
                            font.family: Theme.fontUi
                            font.pixelSize: Theme.fontSizeSmall
                            wrapMode: Text.Wrap
                        }
                        Repeater {
                            model: param.modelData.choices
                            delegate: ObjectRow {
                                required property int index
                                required property string modelData
                                title: modelData
                                swatch: index === param.modelData.index ? Theme.accent : Theme.line
                                selected: index === param.modelData.index
                                onClicked: page.model.setSampleParam(param.modelData.key, String(index))
                            }
                        }
                    }
                }
            }
            TButton {
                compact: true
                enabled: page.card.canReset || false
                text: qsTr("Вернуть значения по умолчанию")
                onClicked: page.model.resetSampleParams()
            }
        }

        TCard {
            width: parent ? parent.width : 300
            visible: (page.card.theory || []).length > 0
            title: qsTr("Паспорт")
            Repeater {
                model: page.card.theory || []
                delegate: TNote { required property var modelData; text: modelData.label + ": " + modelData.value }
            }
        }

        TCard {
            width: parent ? parent.width : 300
            title: qsTr("Числа ядра")
            Repeater {
                model: page.card.kernelRows || []
                delegate: TNote { required property var modelData; text: modelData.label + ": " + modelData.value }
            }
            TNote { text: page.card.kernelNote || "" }
        }

        TCard {
            width: parent ? parent.width : 300
            visible: (page.card.notes || []).length > 0 || (page.card.help || "").length > 0
            title: qsTr("Справка")
            Repeater {
                model: page.card.notes || []
                delegate: TNote { required property string modelData; text: modelData }
            }
            TNote { visible: (page.card.help || "").length > 0; text: page.card.help || "" }
        }
    }
}

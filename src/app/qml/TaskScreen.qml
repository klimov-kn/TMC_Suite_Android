// «Задание»: текст .tpl (CP1251 -> Unicode, только просмотр), выбор ядра H/X
// (: подсказка по тексту, выбор запоминается для задачи), «Считать».
// compact (низкий экран — горизонтальное положение): переключатель и «Считать» в одну
// строку над текстом, подсказка — одной строкой.
// https://www.tamic.ru/docs/platforms/android.html#app
import QtQuick

Item {
    id: screen

    property string folder
    property string task
    property bool compact: false
    property bool showBack: true
    signal back()
    signal showRun()

    readonly property var info: { TaskStore.revision; return TaskStore.modeInfo(screen.folder, screen.task) }
    readonly property string mode: info.mode
    readonly property string fullText: TaskStore.tplText(screen.folder, screen.task)
    readonly property int maxShown: 200000
    readonly property bool busyOther: Calc.busy && !(Calc.folder === folder && Calc.task === task)
    readonly property bool busyThis: Calc.busy && Calc.folder === folder && Calc.task === task
    readonly property string hintText: busyOther
        ? qsTr("Идёт расчёт задачи %1 — дождитесь окончания").arg(Calc.task)
        : (info.saved ? qsTr("Выбрано для этой задачи. Подсказка: %1 — %2.")
                      : qsTr("Подсказка: %1 — %2. Признака моды в задании нет, проверьте выбор."))
              .arg(info.suggested).arg(info.reason)
    readonly property string runText: busyThis ? (compact ? qsTr("Ход расчёта") : qsTr("Открыть ход расчёта"))
                                               : (compact ? qsTr("Считать · %1").arg(mode)
                                                          : qsTr("Считать · ядро %1").arg(mode))

    function run() {
        if (!busyThis)
            Calc.start(folder, task, mode, true)
        showRun()
    }

    ScreenHeader {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        compact: screen.compact
        backText: screen.showBack ? TaskStore.folderTitle(screen.folder) : ""
        title: screen.task
        subtitle: qsTr("Задание · %1 строк").arg(screen.fullText.split("\n").length)
        onBackClicked: screen.back()
    }

    // --- Обычная раскладка: карточка ядра сверху, кнопка внизу ------------------
    Card {
        id: modeCard
        visible: !screen.compact
        anchors { left: parent.left; right: parent.right; top: header.bottom; topMargin: Theme.spacing / 2 }
        padding: Theme.spacing
        spacing: Theme.spacing / 2

        Text {
            text: qsTr("Ядро расчёта")
            color: Theme.accent
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
            font.weight: Font.Bold
        }
        ModeSwitch {
            width: parent.width
            mode: screen.mode
            enabledSwitch: !screen.busyThis
            onPicked: (m) => TaskStore.setMode(screen.folder, screen.task, m)
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            color: screen.busyOther ? Theme.warn : Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeCaption
            text: screen.hintText
        }
    }

    // --- Компактная раскладка: всё управление в одну строку ---------------------
    Column {
        id: compactBar
        visible: screen.compact
        anchors { left: parent.left; right: parent.right; top: header.bottom; topMargin: 4 }
        spacing: 4

        Row {
            width: parent.width
            spacing: Theme.spacing
            ModeSwitch {
                width: parent.width - runCompact.width - Theme.spacing
                mode: screen.mode
                enabledSwitch: !screen.busyThis
                onPicked: (m) => TaskStore.setMode(screen.folder, screen.task, m)
            }
            TamicButton {
                id: runCompact
                width: Math.min(220, parent.width * 0.42)
                primary: !screen.busyThis
                enabled: !screen.busyOther
                text: screen.runText
                onClicked: screen.run()
            }
        }
        Text {
            width: parent.width
            elide: Text.ElideRight
            color: screen.busyOther ? Theme.warn : Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeCaption
            text: screen.hintText
        }
    }

    Rectangle {
        id: textBox
        anchors {
            left: parent.left
            right: parent.right
            top: screen.compact ? compactBar.bottom : modeCard.bottom
            topMargin: screen.compact ? Theme.spacing / 2 : Theme.spacing
            bottom: screen.compact ? parent.bottom : actions.top
            bottomMargin: screen.compact ? 0 : Theme.spacing
        }
        color: Theme.bg2
        radius: Theme.radius
        border.width: Theme.borderW
        border.color: Theme.line
        clip: true

        Flickable {
            id: flick
            anchors.fill: parent
            anchors.margins: Theme.spacing
            contentWidth: Math.max(width, tplText.implicitWidth)
            contentHeight: tplText.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Text {
                id: tplText
                text: screen.fullText.length > screen.maxShown
                      ? screen.fullText.substring(0, screen.maxShown) + "\n…"
                      : screen.fullText
                textFormat: Text.PlainText
                color: Theme.text
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSizeMono
                lineHeight: 1.15
            }
        }
    }

    TamicButton {
        id: actions
        visible: !screen.compact
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        primary: !screen.busyThis
        enabled: !screen.busyOther
        text: screen.runText
        onClicked: screen.run()
    }
}

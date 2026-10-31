// Ссылки на авторов в «Справке»: GitHub (все репозитории автора), ResearchGate, LinkedIn,
// eLibrary — те же, что в окне установщика Windows-версии (TMC_Suite/installer/TMC_Suite.iss).
// Значки — простые знаки в палитре проекта: круг surface, рамка line,
// знак text; нажатие — рамка accent. Касание открывает страницу в браузере.
import QtQuick
import QtQuick.Shapes

Row {
    id: links

    spacing: Theme.spacing

    readonly property var items: [
        { kind: "github",       name: "GitHub",       url: "https://github.com/klimov-kn" },
        { kind: "researchgate", name: "ResearchGate", url: "https://www.researchgate.net/profile/Konstantin-Klimov" },
        { kind: "linkedin",     name: "LinkedIn",     url: "https://www.linkedin.com/in/konstantinklimov" },
        { kind: "elibrary",     name: "eLibrary",     url: "https://www.elibrary.ru/author_items.asp?authorid=121878" }
    ]

    Repeater {
        model: links.items
        delegate: Column {
            id: link
            required property var modelData
            spacing: 4
            width: Math.max(Theme.touchMin + 8, caption.implicitWidth)

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Theme.touchMin + 4
                height: width
                radius: width / 2
                color: area.pressed ? Theme.surface2 : Theme.bg2
                border.width: Theme.borderW
                border.color: area.pressed ? Theme.accent : Theme.line

                // GitHub — знак «кот» (octicon mark-github, 16×16).
                Shape {
                    visible: link.modelData.kind === "github"
                    anchors.centerIn: parent
                    width: 24; height: 24
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        scale: Qt.size(1.5, 1.5)
                        strokeWidth: -1
                        fillColor: Theme.text
                        PathSvg {
                            path: "M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0 0 16 8c0-4.42-3.58-8-8-8z"
                        }
                    }
                }
                // ResearchGate — «R» и маленькая «G», LinkedIn — «in», eLibrary — «e».
                Row {
                    visible: link.modelData.kind !== "github"
                    anchors.centerIn: parent
                    Text {
                        text: link.modelData.kind === "researchgate" ? "R"
                            : link.modelData.kind === "linkedin" ? "in" : "e"
                        color: Theme.text
                        font.family: link.modelData.kind === "researchgate" ? "serif" : Theme.fontUi
                        font.pixelSize: link.modelData.kind === "elibrary" ? 26 : 21
                        font.weight: link.modelData.kind === "linkedin" || link.modelData.kind === "elibrary"
                                     ? Font.ExtraBold : Font.Normal
                    }
                    Text {
                        visible: link.modelData.kind === "researchgate"
                        text: "G"
                        color: Theme.text
                        font.family: "serif"
                        font.pixelSize: 12
                    }
                }
                MouseArea {
                    id: area
                    anchors.fill: parent
                    onClicked: Qt.openUrlExternally(link.modelData.url)
                }
            }
            Text {
                id: caption
                anchors.horizontalCenter: parent.horizontalCenter
                text: link.modelData.name
                color: Theme.textDim
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeCaption
            }
        }
    }
}

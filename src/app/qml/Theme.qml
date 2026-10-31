// Токены оформления TAMIC — единственное место, где в QML записаны цвета,
// шрифты и размеры. Значения — строго по стилю проекта (эталон:
// website/assets/css/main.css и TPL_Designer/src/gui/theme.h).
// В экранах — только имена токенов (Theme.accent и т.п.), никаких "#…".
// План (style-guide, «Как это устроено в коде»): на этапе О2 значения переедут
// в src/ui/theme.h, а этот синглтон будет брать их из C++.
// https://www.tamic.ru/docs/platforms/android.html#app
pragma Singleton
import QtQuick

QtObject {
    // --- Палитра (тёмная, единственная) ---
    readonly property color bg:          "#0a0e16"
    readonly property color bg2:         "#0e1320"
    readonly property color surface:     "#131a28"
    readonly property color surface2:    "#1a2335"
    readonly property color line:        "#243049"
    readonly property color text:        "#e7eef7"
    readonly property color textDim:     "#9fb0c8"
    readonly property color textMute:    "#6c7d97"
    readonly property color accent:      "#34e3c4"
    readonly property color accentPressed: "#5cebd1"
    readonly property color textOnAccent: "#06231e"   // текст на кнопке accent
    readonly property color accent2:     "#b07cff"
    readonly property color accent3:     "#ffcf5c"
    readonly property color warn:        "#e3b341"
    readonly property color danger:      "#ff6b6b"
    readonly property color ok:          "#5ce08a"

    // --- Шрифты ---
    // Inter и JetBrains Mono встроены в приложение (src/app/fonts, SIL OFL 1.1;
    // регистрация — loadFonts() в main.cpp, по образцу appfonts::load() из
    // src/gui_common/appfonts.cpp версии для macOS). Начертания: Inter 400/600/700/800,
    // JetBrains Mono 400/700.
    readonly property string fontUi:   "Inter"
    readonly property string fontMono: "JetBrains Mono"

    readonly property int fontSizeBody:    16
    readonly property int fontSizeSmall:   14   // подписи, вторичный текст
    readonly property int fontSizeCaption: 12   // подписи вкладок, метки
    readonly property int fontSizeMono:    13   // текст задания, журнал ядра
    readonly property int fontSizeH2:      20   // заголовок карточки
    readonly property int fontSizeScreen:  28   // заголовок экрана
    readonly property int fontSizeTitle:   34
    readonly property int fontSizeBig:     44   // крупное число (ход расчёта, %)
    readonly property int fontWeightTitle: Font.ExtraBold   // 800, как h1 сайта

    // --- Нижняя панель вкладок ---
    readonly property int tabBarHeight: 64
    readonly property int railWidth:    88   // боковая полоса вкладок (горизонтальное положение)
    readonly property int iconSize:     24

    // --- Раскладка под поворот экрана ---
    readonly property int masterWidth:   320  // список слева в горизонтальном положении
    readonly property int compactHeight: 560  // ниже — компактные шапки и раскладки

    // --- Размеры и форма (логические единицы: dp / pt) ---
    readonly property int radius:      14   // карточки, панели
    readonly property int radiusSm:    10   // кнопки, поля ввода
    readonly property int radiusTool:   8   // мелкие кнопки-инструменты
    readonly property int radiusCheck:  4   // флажок
    readonly property int borderW:      1
    readonly property int touchMin: Qt.platform.os === "ios" ? 44 : 48
    readonly property int gutter:      16   // отступ от края экрана
    readonly property int spacing:     12

    // --- Анимации: 0,28 с, cubic-bezier(.2,.7,.3,1) ---
    readonly property int durMs: 280
    readonly property var easeCurve: [0.2, 0.7, 0.3, 1.0, 1.0, 1.0]
}

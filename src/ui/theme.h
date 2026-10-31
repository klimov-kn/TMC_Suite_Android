/// @file theme.h
/// @brief Оформление TAMIC для C++ (графики QPainter и т.п.).
///
/// Значения НЕ записаны здесь: они берутся из src/app/qml/Theme.qml (тот же файл, что
/// у экранов QML и у модуля TPL Designer). CMake при настройке читает Theme.qml и пишет
/// tamic_theme_tokens.h (src/ui/theme_tokens.cmake) — источник правды один.
/// Имена функций — как в TPL_Designer/src/gui/theme.h (theme::bg(), theme::accent() …).
/// @see https://www.tamic.ru/docs/platforms/android.html#app

#pragma once

#include "tamic_theme_tokens.h"

#include <QColor>
#include <QFont>
#include <QString>

namespace theme {

inline QColor token(const char *hex) { return QColor(QString::fromLatin1(hex)); }

inline QColor bg()          { return token(theme_tokens::bg); }
inline QColor bg2()         { return token(theme_tokens::bg2); }
inline QColor surface()     { return token(theme_tokens::surface); }
inline QColor surface2()    { return token(theme_tokens::surface2); }
inline QColor line()        { return token(theme_tokens::line); }
inline QColor text()        { return token(theme_tokens::text); }
inline QColor textDim()     { return token(theme_tokens::textDim); }
inline QColor textMute()    { return token(theme_tokens::textMute); }
inline QColor accent()      { return token(theme_tokens::accent); }
inline QColor accent2()     { return token(theme_tokens::accent2); }
inline QColor accent3()     { return token(theme_tokens::accent3); }
inline QColor warn()        { return token(theme_tokens::warn); }
inline QColor danger()      { return token(theme_tokens::danger); }
inline QColor ok()          { return token(theme_tokens::ok); }

// Ряды данных по порядку (style-guide, «Графики»): accent, accent-2, accent-3, ok,
// danger, далее text-dim.
inline QColor series(int index)
{
    switch (index < 0 ? 0 : index % 6) {
    case 0:  return accent();
    case 1:  return accent2();
    case 2:  return accent3();
    case 3:  return ok();
    case 4:  return danger();
    default: return textDim();
    }
}

inline QString fontUi()   { return QString::fromLatin1(theme_tokens::fontUi); }
inline QString fontMono() { return QString::fromLatin1(theme_tokens::fontMono); }

// Шрифт заданного размера в логических пикселях (как font.pixelSize в QML).
inline QFont uiFont(int pixelSize, int weight = QFont::Normal)
{
    QFont f(fontUi());
    f.setPixelSize(pixelSize);
    f.setWeight(QFont::Weight(weight));
    return f;
}
inline QFont monoFont(int pixelSize, int weight = QFont::Normal)
{
    QFont f(fontMono());
    f.setPixelSize(pixelSize);
    f.setWeight(QFont::Weight(weight));
    return f;
}

} // namespace theme

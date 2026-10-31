/// @file viewers_import.cpp
/// @brief Импорт статического QML-модуля Tamic.Viewers в приложение (цель tamic_viewers_module):
///
/// без этой ссылки компоновщик выбросит плагин, и «import Tamic.Viewers» не найдётся.
/// @see https://www.tamic.ru/docs/platforms/android.html#charts

#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(Tamic_ViewersPlugin)

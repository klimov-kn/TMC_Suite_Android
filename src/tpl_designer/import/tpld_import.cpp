/// @file tpld_import.cpp
/// @brief Импорт статических QML-плагинов встроенного TPL Designer.
///
///
/// Файл — INTERFACE-исходник цели tamic_tpl_designer_module: он компилируется в
/// составе того, кто подключает модуль (приложение tamic, tpl_designer_demo), поэтому
/// оболочке не нужно ничего дописывать в свой main.cpp — достаточно
/// target_link_libraries(... tamic_tpl_designer_module).
/// @see https://www.tamic.ru/docs/platforms/android.html#designer

#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(TamicTplDesignerPlugin)
Q_IMPORT_QML_PLUGIN(TamicTplThemePlugin)

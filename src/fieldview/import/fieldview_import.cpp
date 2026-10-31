/// @file fieldview_import.cpp
/// @brief Импорт статических QML-плагинов модуля «Поле» (TamicFieldView и тема TamicTplTheme).
///
/// Файл — INTERFACE-исходник цели tamic_fieldview_module: он компилируется в составе
/// того, кто подключает модуль (приложение tamic, fieldview_demo), поэтому в main.cpp
/// оболочки ничего дописывать не нужно — достаточно
/// target_link_libraries(... tamic_fieldview_module).
/// @see https://www.tamic.ru/docs/platforms/android.html#field

#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(TamicFieldViewPlugin)
Q_IMPORT_QML_PLUGIN(TamicTplThemePlugin)

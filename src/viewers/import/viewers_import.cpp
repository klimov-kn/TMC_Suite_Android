// Импорт статического QML-модуля Tamic.Viewers в приложение (цель tamic_viewers_module):
// без этой ссылки компоновщик выбросит плагин, и «import Tamic.Viewers» не найдётся.
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(Tamic_ViewersPlugin)

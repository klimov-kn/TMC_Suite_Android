# Приложение TAMIC: оболочка на Qt Quick.

set(TAMIC_APP_SOURCES
    main.cpp
    appinfo.h appinfo.cpp
    autotest.h autotest.cpp
    calcsession.h calcsession.cpp
    kernelrunner.h kernelrunner.cpp
    language.h language.cpp
    platform.h
    taskstore.h taskstore.cpp
)
if(ANDROID)
    list(APPEND TAMIC_APP_SOURCES platform_android.cpp)
else()
    list(APPEND TAMIC_APP_SOURCES platform_desktop.cpp)
endif()

qt_add_executable(tamic ${TAMIC_APP_SOURCES})

# Выбор папки на ПК — FolderDialog (QtQuick.Dialogs). На Android папку выбирает
# системное окно SAF (TamicBridge.java), и QtQuick.Dialogs с его стилями Controls в
# APK не попадает (Main.qml грузит FolderPicker.qml через Loader только на ПК).
if(ANDROID)
    set(TAMIC_DESKTOP_QML "")
else()
    set(TAMIC_DESKTOP_QML ../desktop_qml/FolderPicker.qml)
    set_source_files_properties(../desktop_qml/FolderPicker.qml PROPERTIES QT_RESOURCE_ALIAS FolderPicker.qml)
endif()

# Theme.qml — синглтон с токенами оформления.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(tamic
    URI Tamic
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/TamicButton.qml
        qml/Card.qml
        qml/ScreenHeader.qml
        qml/BottomTabs.qml
        qml/TabIcon.qml
        qml/StatusChip.qml
        qml/ModeSwitch.qml
        qml/ListRow.qml
        qml/ProgressLine.qml
        qml/FoldersScreen.qml
        qml/TasksScreen.qml
        qml/TaskScreen.qml
        qml/RunScreen.qml
        qml/ResultsScreen.qml
        qml/DesignerScreen.qml
        qml/ChartScreen.qml
        qml/FieldScreen.qml
        qml/AboutScreen.qml
        qml/ManualScreen.qml
        qml/Sheet.qml
        qml/LinkIcons.qml
        qml/Chip.qml
        ${TAMIC_DESKTOP_QML}
)

# Тексты лицензий — не исходники: lupdate их не читает.
set_property(TARGET tamic PROPERTY QT_EXCLUDE_SOURCES_FROM_TRANSLATION "fonts/*.txt")
# Шрифты (SIL OFL 1.1, лицензии — fonts/*-OFL.txt) и встроенные примеры
# (папка examples; копируются в «Примеры» при первом запуске).
qt_add_resources(tamic tamic_fonts
    PREFIX /fonts
    BASE fonts
    FILES
        fonts/Inter-Regular.ttf
        fonts/Inter-SemiBold.ttf
        fonts/Inter-Bold.ttf
        fonts/Inter-ExtraBold.ttf
        fonts/JetBrainsMono-Regular.ttf
        fonts/JetBrainsMono-Bold.ttf
        fonts/Inter-OFL.txt
        fonts/JetBrainsMono-OFL.txt
)
# Значок для «Справки» и руководство пользователя (src/app/manual: тексты <язык>.json и
# снимки экрана <язык>/*.png).
qt_add_resources(tamic tamic_icon
    PREFIX /icon
    BASE icon
    FILES icon/tamic_icon_160.png
)
# Лицензия TMC Suite (файл LICENSE) — копия в «Справке».
qt_add_resources(tamic tamic_legal
    PREFIX /legal
    BASE ${PROJECT_SOURCE_DIR}
    FILES ${PROJECT_SOURCE_DIR}/LICENSE
)
set_property(TARGET tamic APPEND PROPERTY QT_EXCLUDE_SOURCES_FROM_TRANSLATION "../../LICENSE")
file(GLOB _tamic_manual CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/manual/*.json
    ${CMAKE_CURRENT_SOURCE_DIR}/manual/ru/*.png
    ${CMAKE_CURRENT_SOURCE_DIR}/manual/en/*.png)
qt_add_resources(tamic tamic_manual
    PREFIX /manual
    BASE manual
    FILES ${_tamic_manual}
)
set_property(TARGET tamic APPEND PROPERTY QT_EXCLUDE_SOURCES_FROM_TRANSLATION "manual/*")

set(_tamic_examples ${CMAKE_CURRENT_SOURCE_DIR}/examples)
qt_add_resources(tamic tamic_examples
    PREFIX /examples
    BASE ${_tamic_examples}
    FILES
        ${_tamic_examples}/voln/VOLN.TPL
        ${_tamic_examples}/voln/VOLNSTEP.H
        ${_tamic_examples}/horn/horn.tpl
        ${_tamic_examples}/plasma_nby/plasma_nby.tpl
        ${_tamic_examples}/voln_field/VFIELD.TPL
        ${_tamic_examples}/voln_field/VFIELD.H
        ${_tamic_examples}/eps4/eps4.tpl
        ${_tamic_examples}/xr/xr_up.tpl
        ${_tamic_examples}/xr/xr_down.tpl
        ${_tamic_examples}/xr/xr_off.tpl
        ${_tamic_examples}/lens/lens1.tpl
        ${_tamic_examples}/lens/lens3.tpl
)

# androiddeployqt ищет QML-модули в <префикс>/qml: каталог сборки — один из префиксов.
if(ANDROID)
    list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_BINARY_DIR})
endif()

set_target_properties(tamic PROPERTIES
    OUTPUT_NAME tamic
    WIN32_EXECUTABLE TRUE
    # Android: свой манифест, Java-помощник и ресурсы — в platforms/android
    # (только arm64-v8a, API 28+).
    QT_ANDROID_PACKAGE_SOURCE_DIR "${PROJECT_SOURCE_DIR}/platforms/android"
    QT_ANDROID_PACKAGE_NAME "ru.tamic.app"
    QT_ANDROID_MIN_SDK_VERSION 28
    QT_ANDROID_TARGET_SDK_VERSION 36
    QT_ANDROID_VERSION_NAME "${TAMIC_VERSION_NAME}"
    QT_ANDROID_VERSION_CODE ${TAMIC_VERSION_CODE}
    # Нативные библиотеки распаковываются при установке (useLegacyPackaging): ядра
    # libtmc_rt?.so должны лежать файлами в nativeLibraryDir, чтобы их можно было
    # запустить процессом.
    QT_ANDROID_LEGACY_PACKAGING TRUE
    # Сканер импортов QML смотрит src/app; FolderPicker.qml с модулем QtQuick.Dialogs лежит вне этой
    # папки: он нужен только настольной сборке и потащил бы в APK стили Controls.
    QT_QML_IMPORT_PATH "${CMAKE_BINARY_DIR}/qml"
)

# Точный состав Qt в APK: без отладочных модулей qmltooling (один из них тянет Qt6Quick3DUtils
# под GPL), без TLS и сведений о сети, которые приложению без доступа к сети не нужны.
# QML-плагины перечислены тоже: при заданном списке androiddeployqt их сам не добавляет.
if(ANDROID)
    set(_tamic_qt_deps
        jar/Qt6Android.jar
        jar/Qt6AndroidNetwork.jar
        jar/Qt6AndroidQuick.jar
        lib/libQt6Core_arm64-v8a.so
        lib/libQt6Gui_arm64-v8a.so
        lib/libQt6Network_arm64-v8a.so
        lib/libQt6OpenGL_arm64-v8a.so
        lib/libQt6Qml_arm64-v8a.so
        lib/libQt6QmlModels_arm64-v8a.so
        lib/libQt6QmlWorkerScript_arm64-v8a.so
        lib/libQt6QmlMeta_arm64-v8a.so
        lib/libQt6Quick_arm64-v8a.so
        lib/libQt6Svg_arm64-v8a.so
        lib/libQt6QuickShapes_arm64-v8a.so
        lib/libQt6QuickTemplates2_arm64-v8a.so
        lib/libQt6QuickLayouts_arm64-v8a.so
        plugins/platforms/libplugins_platforms_qtforandroid_arm64-v8a.so
        plugins/imageformats/libplugins_imageformats_qgif_arm64-v8a.so
        plugins/imageformats/libplugins_imageformats_qico_arm64-v8a.so
        plugins/imageformats/libplugins_imageformats_qjpeg_arm64-v8a.so
        plugins/imageformats/libplugins_imageformats_qsvg_arm64-v8a.so
        plugins/iconengines/libplugins_iconengines_qsvgicon_arm64-v8a.so
        qml/QtQml/qmldir
        qml/QtQml/libqml_QtQml_qmlplugin_arm64-v8a.so
        qml/QtQml/Models/qmldir
        qml/QtQml/Models/libqml_QtQml_Models_modelsplugin_arm64-v8a.so
        qml/QtQml/WorkerScript/qmldir
        qml/QtQml/WorkerScript/libqml_QtQml_WorkerScript_workerscriptplugin_arm64-v8a.so
        qml/QtQuick/qmldir
        qml/QtQuick/libqml_QtQuick_qtquick2plugin_arm64-v8a.so
        qml/QtQuick/Layouts/qmldir
        qml/QtQuick/Layouts/libqml_QtQuick_Layouts_qquicklayoutsplugin_arm64-v8a.so
        qml/QtQuick/Shapes/qmldir
        qml/QtQuick/Shapes/libqml_QtQuick_Shapes_qmlshapesplugin_arm64-v8a.so
        qml/QtQuick/Templates/qmldir
        qml/QtQuick/Templates/libqml_QtQuick_Templates_qtquicktemplates2plugin_arm64-v8a.so
    )
    set_target_properties(tamic PROPERTIES QT_ANDROID_DEPLOYMENT_DEPENDENCIES "${_tamic_qt_deps}")
endif()

target_compile_definitions(tamic PRIVATE TAMIC_VERSION_STRING="${TAMIC_VERSION_NAME}")
target_link_libraries(tamic PRIVATE Qt6::Quick)
if(ANDROID)
    target_link_libraries(tamic PRIVATE Qt6::CorePrivate log)
    # Сегменты ELF по 16 КБ (Google Play, Android 15+) — как у библиотек Qt.
    target_link_options(tamic PRIVATE -Wl,-z,max-page-size=16384)
endif()
tamic_set_warnings(tamic)

# Ядра — рядом с приложением (корневой CMakeLists.txt, tamic_kernels).
add_dependencies(tamic tamic_kernels)
if(ANDROID)
    # В APK как нативные библиотеки lib*.so: Android 10+ запускает программы только
    # из nativeLibraryDir; QT_ANDROID_LEGACY_PACKAGING (выше) кладёт их туда
    # файлами при установке.
    set_property(TARGET tamic APPEND PROPERTY QT_ANDROID_EXTRA_LIBS
        ${TAMIC_KERNEL_OUT}/${TAMIC_KERNEL_FILE_H}
        ${TAMIC_KERNEL_OUT}/${TAMIC_KERNEL_FILE_X})
else()
    add_custom_command(TARGET tamic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${TAMIC_KERNEL_OUT}/${TAMIC_KERNEL_FILE_H} ${TAMIC_KERNEL_OUT}/${TAMIC_KERNEL_FILE_X}
            $<TARGET_FILE_DIR:tamic>
        COMMENT "Ядра tmc_rth/tmc_rtx — рядом с tamic")
endif()

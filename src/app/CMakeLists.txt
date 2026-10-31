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
    set(TAMIC_DESKTOP_QML qml/FolderPicker.qml)
endif()

# Theme.qml — синглтон с токенами оформления.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(tamic
    URI Tamic
    VERSION 1.0
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

# Тексты лицензий — не исходники: lupdate их не читает (M28).
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
    # запустить процессом (решения A3, A13).
    QT_ANDROID_LEGACY_PACKAGING TRUE
)

target_compile_definitions(tamic PRIVATE TAMIC_VERSION_STRING="${TAMIC_VERSION_NAME}")
target_link_libraries(tamic PRIVATE Qt6::Quick Qt6::QuickControls2)
if(ANDROID)
    target_link_libraries(tamic PRIVATE Qt6::CorePrivate log)
    # Сегменты ELF по 16 КБ (Google Play, Android 15+; решение M33) — как у библиотек Qt.
    target_link_options(tamic PRIVATE -Wl,-z,max-page-size=16384)
endif()
tamic_set_warnings(tamic)

# Ядра — рядом с приложением (корневой CMakeLists.txt, tamic_kernels).
add_dependencies(tamic tamic_kernels)
if(ANDROID)
    # В APK как нативные библиотеки lib*.so: Android 10+ запускает программы только
    # из nativeLibraryDir; QT_ANDROID_LEGACY_PACKAGING (выше) кладёт их туда
    # файлами при установке (решения A3, A13).
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

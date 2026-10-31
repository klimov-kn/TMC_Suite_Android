# Просмотр поля (FieldView) на Qt RHI.
#
#   tamic_fieldview_core   — статическая библиотека (Qt Core): чтение .ex / .ex.frames /
#                            .tt, границы топологии, запись тактов (FieldRecorder).
#                            Логика чтения — из Windows-FieldView без изменений.
#   tamic_fieldview        — QML-модуль TamicFieldView: FieldViewPage.qml (свойства
#                            resultFolder / fieldFile), типы FieldScene и FieldRhiItem.
#   fieldview_demo         — настольная проверка, снимки экрана, замер кадр/с (только desktop).
#   fieldview_capture      — запуск ядра с записью тактов в .ex.frames (только desktop).
#
# Подключение в корневом CMakeLists.txt (после add_subdirectory(src/app)):
#     add_subdirectory(src/fieldview)
#     target_link_libraries(tamic PRIVATE tamic_fieldview_module)
# и в QML: import TamicFieldView / FieldViewPage { resultFolder: … }.
# Подробности — README.md рядом.
#
# Каталог собирается и самостоятельно (проверка без оболочки):
#     cmake -S src/fieldview -B build/fieldview-desktop -G Ninja …
# тогда по умолчанию включаются демо и инструменты.

cmake_minimum_required(VERSION 3.21)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(TamicFieldView VERSION 0.1.0 LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(_fv_standalone ON)
else()
    set(_fv_standalone OFF)
endif()

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui GuiPrivate Qml Quick ShaderTools)
if(_fv_standalone)
    qt_standard_project_setup(REQUIRES 6.7)
    qt_policy(SET QTP0004 NEW)
endif()

# Ноль предупреждений — как в корневом CMakeLists.txt.
if(NOT COMMAND tamic_set_warnings)
    function(tamic_set_warnings target)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /WX)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
        endif()
    endfunction()
endif()


# --- Ядро: файлы поля и топологии ------------------------------------------
add_library(tamic_fieldview_core STATIC
    core/textscan.h
    core/fieldfile.h core/fieldfile.cpp
    core/topology.h core/topology.cpp
    core/fieldrecorder.h core/fieldrecorder.cpp
    core/fieldscales.h core/fieldscales.cpp
    core/gifwriter.h core/gifwriter.cpp
)
set_target_properties(tamic_fieldview_core PROPERTIES AUTOMOC ON)
target_include_directories(tamic_fieldview_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core)
target_link_libraries(tamic_fieldview_core PUBLIC Qt6::Core Qt6::Gui)
# Цвета — токены оформления из src/app/qml/Theme.qml через src/ui/theme.h (один источник).
include(${CMAKE_CURRENT_SOURCE_DIR}/../ui/theme_tokens.cmake)
tamic_generate_theme_tokens(tamic_fieldview_core)
tamic_set_warnings(tamic_fieldview_core)

# --- Тема QML: тот же модуль TamicTplTheme, что у TPL Designer ---------------
# Синглтон Theme из ТОГО ЖЕ файла src/app/qml/Theme.qml. Если TPL Designer уже
# подключён (add_subdirectory(src/tpl_designer) раньше), его цель используется как есть;
# иначе она создаётся здесь тем же способом (самостоятельная сборка модуля).
if(NOT TARGET tamic_tpl_theme)
    qt_add_library(tamic_tpl_theme STATIC)
    set(_fv_theme ${CMAKE_CURRENT_BINARY_DIR}/theme/Theme.qml)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../app/qml/Theme.qml ${_fv_theme} COPYONLY)
    set_source_files_properties(${_fv_theme} PROPERTIES
        QT_QML_SINGLETON_TYPE TRUE
        QT_RESOURCE_ALIAS Theme.qml)
    qt_add_qml_module(tamic_tpl_theme
        URI TamicTplTheme
        VERSION 1.0
        RESOURCE_PREFIX /qt/qml
        QML_FILES ${_fv_theme}
    )
    target_link_libraries(tamic_tpl_theme PUBLIC Qt6::Qml Qt6::Quick)
endif()

# --- QML-модуль TamicFieldView ---------------------------------------------
qt_add_library(tamic_fieldview STATIC)
set(_fv_internal
    qml/FvButton.qml qml/FvChip.qml qml/FvSlider.qml qml/FvScaleBar.qml qml/FvControls.qml)
set_source_files_properties(${_fv_internal} PROPERTIES QT_QML_INTERNAL_TYPE TRUE)
foreach(_f ${_fv_internal} qml/FieldViewPage.qml)
    get_filename_component(_n ${_f} NAME)
    set_source_files_properties(${_f} PROPERTIES QT_RESOURCE_ALIAS ${_n})
endforeach()
qt_add_qml_module(tamic_fieldview
    URI TamicFieldView
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    SOURCES
        rhi/fieldscene.h rhi/fieldscene.cpp
        rhi/fieldrhiitem.h rhi/fieldrhiitem.cpp
        rhi/fieldexporter.h rhi/fieldexporter.cpp
    IMPORTS TamicTplTheme
    QML_FILES
        qml/FieldViewPage.qml
        ${_fv_internal}
)
# Шейдеры пишутся один раз (GLSL 440); qsb переводит их для каждой платформы:
# SPIR-V (Vulkan), GLSL ES 300 / GLSL 330 (OpenGL ES, OpenGL), HLSL (D3D), MSL (Metal).
qt_add_shaders(tamic_fieldview "tamic_fieldview_shaders"
    PREFIX "/tamic/fieldview"
    GLSL "300es,330"
    HLSL 50
    MSL 12
    FILES
        shaders/surface.vert
        shaders/surface.frag
        shaders/color.vert
        shaders/color.frag
)
target_include_directories(tamic_fieldview PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rhi ${CMAKE_CURRENT_SOURCE_DIR}/../ui)
target_link_libraries(tamic_fieldview PUBLIC
    tamic_fieldview_core tamic_tpl_theme
    # QRhi (rhi/qrhi.h) в Qt 6.11 — среди закрытых заголовков QtGui.
    Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::Qml Qt6::Quick)
tamic_set_warnings(tamic_fieldview)

# Подключение одной целью: статические плагины модулей и их импорт
# (import/fieldview_import.cpp компилируется в составе подключающего приложения).
add_library(tamic_fieldview_module INTERFACE)
target_sources(tamic_fieldview_module INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/import/fieldview_import.cpp)
target_link_libraries(tamic_fieldview_module INTERFACE
    tamic_fieldviewplugin tamic_tpl_themeplugin tamic_fieldview)



# Встроенный TPL Designer (решение M17, этап 2B): создание задания .tpl в приложении.
#
#   tamic_tpl_core       — статическая библиотека, ядро генератора из
#                          ..\TPL_Designer\src\core (Qt Core, без Widgets). Логика
#                          порождения текста .tpl НЕ меняется.
#   tamic_tpl_designer   — QML-модуль TamicTplDesigner: страница TplDesignerPage
#                          (свойство taskFolder, сигнал saved(string tplPath)).
#
# Подключение в корневом CMakeLists.txt (после add_subdirectory(src/app)):
#     add_subdirectory(src/tpl_designer)
#     target_link_libraries(tamic PRIVATE tamic_tpl_designer_module)
# и в QML: import TamicTplDesigner / TplDesignerPage { taskFolder: … }.
# Подробности — README.md рядом.
#
# Каталог собирается и самостоятельно (проверка без оболочки):
#     cmake -S src/tpl_designer -B build/tpl-designer-desktop -G Ninja …
# тогда по умолчанию включается демо.

cmake_minimum_required(VERSION 3.21)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(TamicTplDesigner VERSION 0.1.0 LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(_tpld_standalone ON)
else()
    set(_tpld_standalone OFF)
endif()

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Qml Quick)
if(_tpld_standalone)
    qt_standard_project_setup(REQUIRES 6.7)
    qt_policy(SET QTP0004 NEW)
endif()

# Ноль предупреждений (M14). В составе приложения функция задана корневым
# CMakeLists.txt; при самостоятельной сборке — здесь, тем же содержимым.
if(NOT COMMAND tamic_set_warnings)
    function(tamic_set_warnings target)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /WX)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
        endif()
    endfunction()
endif()


# --- Ядро генератора --------------------------------------------------------
# Перенесено из TPL_Designer/src/core без изменений логики.
# STEP через OpenCASCADE на телефоне не нужен — stepio_none.cpp (STL и OBJ читаются).
add_library(tamic_tpl_core STATIC
    core/lang.cpp
    core/cp1251.cpp
    core/project.cpp
    core/emitter.cpp
    core/placement.cpp
    core/expr.cpp
    core/epsfile.cpp
    core/sigfile.cpp
    core/ramestimate.cpp
    core/tplreader.cpp
    core/taskfiles.cpp
    core/mesh3d.cpp
    core/meshio.cpp
    core/meshslice.cpp
    core/convexsplit.cpp
    core/materialmap.cpp
    core/meshimport.cpp
    core/meshgen.cpp
    core/stepio_none.cpp
    core/samples/catalog.cpp
    core/samples/standards.cpp
    core/samples/rf.cpp
    core/samples/kit.cpp
    core/samples/waveguides.cpp
    core/samples/antennas.cpp
    core/samples/microstrip.cpp
    core/samples/microstrip2.cpp
    core/samples/media.cpp
    core/samples/lessons.cpp
    core/samples/baked.cpp
    core/samples/usersamples.cpp
    # Авторские учебные задания (CP1251, байт в байт): Q_INIT_RESOURCE(lessons).
    core/samples/lessons.qrc
)
set_target_properties(tamic_tpl_core PROPERTIES AUTORCC ON AUTOMOC OFF AUTOUIC OFF)
target_include_directories(tamic_tpl_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/samples)
target_link_libraries(tamic_tpl_core PUBLIC Qt6::Core)
tamic_set_warnings(tamic_tpl_core)
if(NOT MSVC)
    # Как во всём проекте (корневой CMakeLists.txt): без слияния умножения со сложением
    # (FMA на arm64), чтобы числа в .tpl совпадали с настольной сборкой оригинала.
    target_compile_options(tamic_tpl_core PRIVATE -ffp-contract=off)
endif()

# --- QML-модуль TamicTplDesigner -------------------------------------------
# Статическая библиотека с типами DesignerModel и GeometryCanvas и страницей
# TplDesignerPage. Оформление — токены Theme оболочки: тот же файл
# src/app/qml/Theme.qml подключён в модуль под своим именем (QT_RESOURCE_ALIAS),
# своих цветов здесь нет (README.md, «Оформление»).
#
# Тонкий модуль-прослойка TamicTplTheme: синглтон Theme из ТОГО ЖЕ файла, что у
# оболочки (src/app/qml/Theme.qml, под своим именем через QT_RESOURCE_ALIAS). Его
# импортируют только файлы дизайнера, поэтому тип Theme не попадает в пространство
# имён оболочки при «import TamicTplDesigner».
qt_add_library(tamic_tpl_theme STATIC)
# Файл копируется в папку сборки при настройке (configure_file следит за ним: правка
# Theme.qml оболочки перенастраивает сборку) — qmlcachegen не принимает пути с «..».
set(_tpld_theme ${CMAKE_CURRENT_BINARY_DIR}/theme/Theme.qml)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../app/qml/Theme.qml ${_tpld_theme} COPYONLY)
set_source_files_properties(${_tpld_theme} PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
    QT_RESOURCE_ALIAS Theme.qml)
qt_add_qml_module(tamic_tpl_theme
    URI TamicTplTheme
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml/TamicTplTheme
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${_tpld_theme}
)
target_link_libraries(tamic_tpl_theme PUBLIC Qt6::Qml Qt6::Quick)

qt_add_library(tamic_tpl_designer STATIC)
# Наружу видны только TplDesignerPage, DesignerModel и GeometryCanvas; остальное —
# внутреннее (internal в qmldir): имена Theme, TButton… не спорят с типами оболочки.
set(_tpld_internal
    qml/StepTabs.qml qml/IssueChip.qml qml/SetupPage.qml qml/GeometryPage.qml
    qml/ObjectsPage.qml qml/ObjectRow.qml qml/OutputPage.qml qml/CheckPage.qml
    qml/RunPlanPage.qml qml/MemoryRing.qml qml/SamplesPage.qml
    qml/SelectionEditor.qml qml/ShapeEditor.qml qml/PortEditor.qml qml/TPage.qml
    qml/TCard.qml qml/TButton.qml qml/TField.qml qml/TNote.qml qml/TSwitchRow.qml
    qml/TSegmented.qml qml/TSheet.qml qml/TIcon.qml qml/TToolButton.qml)
set_source_files_properties(${_tpld_internal} PROPERTIES QT_QML_INTERNAL_TYPE TRUE)
# Файлы лежат в qml/, а в модуле — в корне (QT_RESOURCE_ALIAS): иначе у файлов в
# подпапке своя неявная папка-импорт, и внутренние типы модуля им не видны.
foreach(_f ${_tpld_internal} qml/TplDesignerPage.qml)
    get_filename_component(_n ${_f} NAME)
    set_source_files_properties(${_f} PROPERTIES QT_RESOURCE_ALIAS ${_n})
endforeach()
qt_add_qml_module(tamic_tpl_designer
    URI TamicTplDesigner
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml/TamicTplDesigner
    RESOURCE_PREFIX /qt/qml
    SOURCES
        backend/designermodel.h backend/designermodel.cpp
        backend/geometrycanvas.h backend/geometrycanvas.cpp
        backend/designerextras.cpp
        backend/samplepreview.h backend/samplepreview.cpp
        backend/sysmemory.h backend/sysmemory.cpp
    IMPORTS TamicTplTheme
    QML_FILES
        qml/TplDesignerPage.qml
        qml/StepTabs.qml
        qml/IssueChip.qml
        qml/SetupPage.qml
        qml/GeometryPage.qml
        qml/ObjectsPage.qml
        qml/ObjectRow.qml
        qml/OutputPage.qml
        qml/CheckPage.qml
        qml/RunPlanPage.qml
        qml/MemoryRing.qml
        qml/SamplesPage.qml
        qml/SelectionEditor.qml
        qml/ShapeEditor.qml
        qml/PortEditor.qml
        qml/TPage.qml
        qml/TCard.qml
        qml/TButton.qml
        qml/TField.qml
        qml/TNote.qml
        qml/TSwitchRow.qml
        qml/TSegmented.qml
        qml/TSheet.qml
        qml/TIcon.qml
        qml/TToolButton.qml
)
target_include_directories(tamic_tpl_designer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/backend)
target_link_libraries(tamic_tpl_designer PUBLIC tamic_tpl_core tamic_tpl_theme Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
tamic_set_warnings(tamic_tpl_designer)

# Подключение одной целью: статические плагины обоих модулей и их импорт
# (import/tpld_import.cpp компилируется в составе подключающего приложения).
add_library(tamic_tpl_designer_module INTERFACE)
target_sources(tamic_tpl_designer_module INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/import/tpld_import.cpp)
target_link_libraries(tamic_tpl_designer_module INTERFACE
    tamic_tpl_designerplugin tamic_tpl_themeplugin tamic_tpl_designer)



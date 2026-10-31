# Просмотр результатов (этап О3): S-матрица (TMCGROUT), ДН (TMC_DN), сигналы (TMCROS).
#
#   tamic_viewers_data   — чтение файлов .s / .t / .DAT / .AMP / .FAZ и обработка для
#                          графиков (только Qt Core): перенос из Windows-вьюверов без изменения
#                          математики.
#   tamic_viewers        — QML-модуль Tamic.Viewers: SMatrixChart, PatternChart, SignalChart
#                          (QQuickPaintedItem + QPainter, решение M5), ViewerTools.
#   tamic_viewers_module — подключение одной целью (статический плагин и его импорт,
#                          import/viewers_import.cpp компилируется в приложении).
# Оформление — src/ui/theme.h (значения из src/app/qml/Theme.qml, один источник).

include(${CMAKE_CURRENT_SOURCE_DIR}/../ui/theme_tokens.cmake)

add_library(tamic_viewers_data STATIC
    data/textscan.h
    data/tmccomplex.h
    data/smatrixfile.h data/smatrixfile.cpp
    data/signalfile.h data/signalfile.cpp
    data/dnpattern.h data/dnpattern.cpp
    data/fieldfile.h data/fieldfile.cpp
)
target_include_directories(tamic_viewers_data PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/data)
target_link_libraries(tamic_viewers_data PUBLIC Qt6::Core)
if(NOT MSVC)
    # Как во всём проекте: без FMA — числа ДН и S-параметров как у Windows-версии.
    target_compile_options(tamic_viewers_data PRIVATE -ffp-contract=off)
endif()
tamic_set_warnings(tamic_viewers_data)

qt_add_library(tamic_viewers STATIC)
qt_add_qml_module(tamic_viewers
    URI Tamic.Viewers
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml/Tamic/Viewers
    RESOURCE_PREFIX /qt/qml
    SOURCES
        viewers.h viewers.cpp
        chart/chartbase.h chart/chartbase.cpp
        chart/charts.h chart/charts.cpp
)
target_include_directories(tamic_viewers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/chart)
target_link_libraries(tamic_viewers PUBLIC tamic_viewers_data Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
tamic_generate_theme_tokens(tamic_viewers)
tamic_set_warnings(tamic_viewers)

add_library(tamic_viewers_module INTERFACE)
target_sources(tamic_viewers_module INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/import/viewers_import.cpp)
target_link_libraries(tamic_viewers_module INTERFACE tamic_viewersplugin tamic_viewers)

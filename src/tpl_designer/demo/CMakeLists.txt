# tpl_designer_demo — настольная проверка страницы TplDesignerPage (только desktop):
#   tpl_designer_demo [--folder <папка задачи>]
#   tpl_designer_demo --screenshot <папка>   — снимки шагов в размере телефона
#                                              (1080x2340 и 2340x1080, dpr 3)
#   tpl_designer_demo --make-voln <папка>    — собрать задание «волновод со вставкой»
#                                              через модель страницы и записать
qt_add_executable(tpl_designer_demo main.cpp)
qt_add_qml_module(tpl_designer_demo
    URI TplDesignerDemo
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../qml/TplDesignerDemo
    QML_FILES Main.qml
)
# Шрифты проекта (Inter, JetBrains Mono, SIL OFL 1.1) — те же файлы, что у оболочки.
set(_fonts ${CMAKE_CURRENT_SOURCE_DIR}/../../app/fonts)
qt_add_resources(tpl_designer_demo tpld_demo_fonts
    PREFIX /fonts
    BASE ${_fonts}
    FILES
        ${_fonts}/Inter-Regular.ttf
        ${_fonts}/Inter-SemiBold.ttf
        ${_fonts}/Inter-Bold.ttf
        ${_fonts}/Inter-ExtraBold.ttf
        ${_fonts}/JetBrainsMono-Regular.ttf
        ${_fonts}/JetBrainsMono-Bold.ttf
)
target_link_libraries(tpl_designer_demo PRIVATE tamic_tpl_designer_module Qt6::Quick)
set_target_properties(tpl_designer_demo PROPERTIES WIN32_EXECUTABLE FALSE)
tamic_set_warnings(tpl_designer_demo)

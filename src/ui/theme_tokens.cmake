# Токены оформления для C++ — из того же файла, что у QML (один источник правды).
#
# Источник — src/app/qml/Theme.qml. Его же берёт модуль
# TPL Designer (src/tpl_designer, TamicTplTheme), поэтому Theme.qml остаётся чистым QML
# без ссылок на C++. Функция читает строки вида
#     readonly property color accent:      "#34e3c4"
#     readonly property string fontMono: "JetBrains Mono"
#     readonly property int fontSizeSmall:   14
# и пишет заголовок <OUT_DIR>/tamic_theme_tokens.h с константами theme_tokens::<имя>.
# Правка Theme.qml перенастраивает сборку (CMAKE_CONFIGURE_DEPENDS) — разойтись
# значения C++ и QML не могут. Доступ из C++ — src/ui/theme.h.
#
#   tamic_generate_theme_tokens(<цель>)  — генерирует заголовок и добавляет каталог
#                                          с ним и src/ui в пути включения цели.

set(TAMIC_THEME_QML ${CMAKE_CURRENT_LIST_DIR}/../app/qml/Theme.qml)
get_filename_component(TAMIC_THEME_QML ${TAMIC_THEME_QML} ABSOLUTE)
set(TAMIC_UI_DIR ${CMAKE_CURRENT_LIST_DIR})

function(tamic_generate_theme_tokens target)
    set(out_dir ${CMAKE_CURRENT_BINARY_DIR}/theme_tokens)
    set(out ${out_dir}/tamic_theme_tokens.h)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${TAMIC_THEME_QML})

    file(STRINGS ${TAMIC_THEME_QML} lines REGEX "readonly property (color|string|int) ")
    set(body "")
    set(count 0)
    foreach(line IN LISTS lines)
        if(line MATCHES "readonly property color ([A-Za-z0-9_]+):[ \t]*\"(#[0-9a-fA-F]+)\"")
            string(APPEND body "inline constexpr const char *${CMAKE_MATCH_1} = \"${CMAKE_MATCH_2}\";\n")
            math(EXPR count "${count} + 1")
        elseif(line MATCHES "readonly property string ([A-Za-z0-9_]+):[ \t]*\"([^\"]*)\"")
            string(APPEND body "inline constexpr const char *${CMAKE_MATCH_1} = \"${CMAKE_MATCH_2}\";\n")
        elseif(line MATCHES "readonly property int ([A-Za-z0-9_]+):[ \t]*([0-9]+)[ \t]*(//.*)?$")
            string(APPEND body "inline constexpr int ${CMAKE_MATCH_1} = ${CMAKE_MATCH_2};\n")
        endif()
    endforeach()
    if(count LESS 16)
        message(FATAL_ERROR "Theme.qml: найдено только ${count} цветов — формат строк изменился?")
    endif()

    set(text "// Порождён CMake (src/ui/theme_tokens.cmake) из src/app/qml/Theme.qml — не править.\n#pragma once\n\nnamespace theme_tokens {\n${body}} // namespace theme_tokens\n")
    # Перезапись только при изменении — иначе пересборка всего, что включает theme.h.
    set(old "")
    if(EXISTS ${out})
        file(READ ${out} old)
    endif()
    if(NOT old STREQUAL text)
        file(WRITE ${out} "${text}")
    endif()
    target_include_directories(${target} PUBLIC ${out_dir} ${TAMIC_UI_DIR})
endfunction()

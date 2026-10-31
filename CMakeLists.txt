# TAMIC — мобильное приложение для Android и настольная
# отладочной сборки (пресет desktop). Пресеты — в CMakePresets.json.
cmake_minimum_required(VERSION 3.21)

project(TAMIC
    VERSION 0.1.0
    DESCRIPTION "TAMIC: расчёт по веткам H/X и просмотр результатов"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Арифметика с плавающей точкой (как в версиях для Linux и macOS) -------
# gcc/clang по умолчанию вправе объединять умножение со сложением (FMA); MSVC в
# Windows-версии этого не делает. Нужны совпадающие с Windows числа, поэтому
# объединение запрещено для всего проекта. Формулы и порядок операций не меняются.
if(NOT MSVC)
    add_compile_options(-ffp-contract=off)
endif()

# --- Только консольные ядра (пресет kernels-android-arm64, этап П1) ----------
# Ядра tmc_rth / tmc_rtx — самостоятельный подпроект src/kernels без Qt. Пресеты
# CMake не умеют задавать свой каталог исходников, поэтому пресет ядер собирает
# корневой проект с этим ключом: подключается только src/kernels, Qt не ищется.
option(TAMIC_KERNELS_ONLY "Собрать только консольные ядра tmc_rth/tmc_rtx (без Qt)" OFF)
if(TAMIC_KERNELS_ONLY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    add_subdirectory(src/kernels)
    return()
endif()

find_package(Qt6 6.7 REQUIRED COMPONENTS Quick QuickControls2)
if(ANDROID)
    # QtAndroidPrivate::startActivity (выбор папки SAF с ответом) — закрытый API QtCore.
    find_package(Qt6 REQUIRED COMPONENTS CorePrivate)
endif()
# Версия для людей (имя APK, «О программе»): этап и номер сборки.
set(TAMIC_VERSION_NAME "0.1.0-alpha4")
set(TAMIC_VERSION_CODE 4)
qt_standard_project_setup(REQUIRES 6.7)
# QML-файлы в подпапках (src/app/qml) получают свой qmldir — поведение Qt 6.8+.
qt_policy(SET QTP0004 NEW)

# Ноль предупреждений (M14): предупреждения исправляются, а не глушатся.
# Флаги задаются на каждую нашу цель функцией tamic_set_warnings(), чтобы не
# навязывать их коду, который генерирует Qt.
function(tamic_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
endfunction()

# --- Ядра расчёта (H и X) -------------------------------------------------
# Подпроект ядер src/kernels собирается ОТДЕЛЬНОЙ сборкой (ExternalProject), а не
# add_subdirectory:
#   * всегда Release (-O3), даже в отладочной сборке оболочки — иначе счёт в разы
#     медленнее (замеры П1 — на Release);
#   * свои ключи (_PREC_DOUBLE, -ffp-contract=off, -fsigned-char …) и свой журнал
#     предупреждений (этап О1), не смешиваются с -Werror оболочки;
#   * на Android — NDK напрямую и c++_static: ядро — самостоятельная программа без
#     libc++_shared.so (решение A3), как в пресете kernels-android-arm64.
# H и X — две разные программы, вместе не линкуются. Оболочка
# запускает их процессом (KernelRunner, M9).
#
# TAMIC_KERNELS_PREBUILT — каталог с уже собранными tmc_rth/tmc_rtx (например,
# build/kernels-android/bin): тогда src/kernels не собирается, файлы берутся как есть.
include(ExternalProject)
set(TAMIC_KERNELS_PREBUILT "" CACHE PATH
    "Каталог с готовыми ядрами tmc_rth/tmc_rtx (пусто — собрать из src/kernels)")
set(TAMIC_KERNEL_OUT ${CMAKE_BINARY_DIR}/kernels-out)
if(ANDROID)
    # Android 10+ разрешает запуск программ только из каталога нативных библиотек
    # приложения, поэтому ядра кладутся в APK под именами lib*.so (A3).
    set(TAMIC_KERNEL_FILE_H libtmc_rth.so)
    set(TAMIC_KERNEL_FILE_X libtmc_rtx.so)
    set(_tamic_exe_suffix "")
    set(_tamic_kernel_args
        -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake
        -DANDROID_ABI=${ANDROID_ABI}
        -DANDROID_PLATFORM=android-28
        -DANDROID_STL=c++_static
        # Сегменты ELF по 16 КБ: Google Play требует это для Android 15+ (M33);
        # иначе на телефонах со страницами 16 КБ программа ядра не загрузится.
        -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON)
else()
    set(_tamic_exe_suffix ${CMAKE_EXECUTABLE_SUFFIX})
    set(TAMIC_KERNEL_FILE_H tmc_rth${_tamic_exe_suffix})
    set(TAMIC_KERNEL_FILE_X tmc_rtx${_tamic_exe_suffix})
    set(_tamic_kernel_args -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER})
endif()
if(TAMIC_KERNELS_PREBUILT)
    add_custom_target(tamic_kernels ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TAMIC_KERNEL_OUT}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${TAMIC_KERNELS_PREBUILT}/tmc_rth${_tamic_exe_suffix} ${TAMIC_KERNEL_OUT}/${TAMIC_KERNEL_FILE_H}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${TAMIC_KERNELS_PREBUILT}/tmc_rtx${_tamic_exe_suffix} ${TAMIC_KERNEL_OUT}/${TAMIC_KERNEL_FILE_X}
        BYPRODUCTS ${TAMIC_KERNEL_OUT}/${TAMIC_KERNEL_FILE_H} ${TAMIC_KERNEL_OUT}/${TAMIC_KERNEL_FILE_X}
        COMMENT "Ядра TAMIC: готовые из ${TAMIC_KERNELS_PREBUILT}")
else()
    ExternalProject_Add(tamic_kernels
        SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/kernels
        BINARY_DIR ${CMAKE_BINARY_DIR}/kernels
        CMAKE_GENERATOR Ninja
        CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
            ${_tamic_kernel_args}
        BUILD_ALWAYS ON
        INSTALL_COMMAND
            ${CMAKE_COMMAND} -E make_directory ${TAMIC_KERNEL_OUT}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                <BINARY_DIR>/bin/tmc_rth${_tamic_exe_suffix} ${TAMIC_KERNEL_OUT}/${TAMIC_KERNEL_FILE_H}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                <BINARY_DIR>/bin/tmc_rtx${_tamic_exe_suffix} ${TAMIC_KERNEL_OUT}/${TAMIC_KERNEL_FILE_X}
        BUILD_BYPRODUCTS ${TAMIC_KERNEL_OUT}/${TAMIC_KERNEL_FILE_H} ${TAMIC_KERNEL_OUT}/${TAMIC_KERNEL_FILE_X})
endif()

# --- Приложение -------------------------------------------------------------
add_subdirectory(src/app)

# --- Графики результатов (этап О3): QML-модуль Tamic.Viewers ------------------
add_subdirectory(src/viewers)
target_link_libraries(tamic PRIVATE tamic_viewers_module)

# --- Встроенный TPL Designer (M17, этап 2B): QML-модуль TamicTplDesigner -------
add_subdirectory(src/tpl_designer)
target_link_libraries(tamic PRIVATE tamic_tpl_designer_module)

# --- Поле на Qt RHI (M6, этап О4): QML-модуль TamicFieldView -------------------
# После TPL Designer: общий модуль темы TamicTplTheme создаётся там один раз.
add_subdirectory(src/fieldview)
target_link_libraries(tamic PRIVATE tamic_fieldview_module tamic_fieldview_core)

# --- Второй язык (M28): тексты в коде по-русски, английский — перевод Qt Linguist --
# i18n/tamic_en.ts обновляется целью update_translations (lupdate по исходникам и QML
# всех модулей), tamic_en.qm собирается lrelease и кладётся в ресурсы (:/i18n).
# В ядре генератора TPL Designer (tamic_tpl_core) переводятся только замечания проверки
# (контекст TplCheck); текст самого задания .tpl не переводится.
find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(tamic
    TS_FILES ${PROJECT_SOURCE_DIR}/i18n/tamic_en.ts
    SOURCE_TARGETS tamic tamic_viewers tamic_viewers_data tamic_tpl_designer tamic_tpl_core
                   tamic_fieldview tamic_fieldview_core
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -source-language ru_RU -target-language en -no-obsolete -locations none)


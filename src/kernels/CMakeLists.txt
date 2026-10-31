# Консольные счётные ядра tmc_rth (H) и tmc_rtx (X) из общего снимка src/win_src.
#
# Самостоятельный подпроект (собирается и без корневого CMakeLists.txt):
#   cmake -S src/kernels -B build/kernels-desktop -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/kernels-desktop
#
# Образец — версия для Linux (src/CMakeLists.txt, src/cli, src/compat): состав
# библиотек и ядер — ровно как в .vcxproj Windows-проекта, GUI-обвязка MFC
# заменена консольной точкой входа (cli/) и тонким слоем совместимости (compat/).
#
# МАТЕМАТИКУ НЕ ТРОГАЕМ: CMake лишь компилирует те же файлы. Требования:
#   * _PREC_DOUBLE всегда (Баг #11). Он же определён в Include/Typerth.h пустым
#     макросом; здесь задаётся тем же пустым телом (-D_PREC_DOUBLE=), поэтому
#     переопределения и предупреждения нет;
#   * -ffp-contract=off — никакого слияния a*b+c в FMA (как MSVC);
#   * шесть макросов X-моды (ELECTRON_Q___ …) НЕ задаются: они уже определены
#     в Include/Typerth.h и Include/Tmcgrviw.h;
#   * H и X — отдельные исполняемые файлы, вместе не линкуются.
#
# Ноль предупреждений (решение M14, этап 1B, 2026-10-16): -Wall -Wextra -Werror для
# всех библиотек и обоих ядер; предупреждения исправлены в исходниках
# а не заглушены.

cmake_minimum_required(VERSION 3.22)

project(TMC_Kernels
        VERSION 0.1.0
        DESCRIPTION "TAMIC: консольные счётные ядра tmc_rth / tmc_rtx"
        LANGUAGES CXX)

# Когда подпроект подключён из корневого CMakeLists.txt, не переопределяем
# общие настройки; когда собирается сам — задаём их здесь.
if(PROJECT_IS_TOP_LEVEL)
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Тип сборки" FORCE)
  endif()
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# gnu++17, а не c++17: строгий режим (__STRICT_ANSI__) прячет в заголовках MinGW
# расширения CRT Microsoft (_complex, stricmp, _finite …), которыми пользуется
# Windows-код. На Android/iOS это тоже безвредно.
set(CMAKE_CXX_EXTENSIONS ON)

set(TMC_WIN_SRC  ${CMAKE_CURRENT_SOURCE_DIR}/../win_src)
get_filename_component(TMC_WIN_SRC ${TMC_WIN_SRC} ABSOLUTE)
set(TMC_INCLUDE  ${TMC_WIN_SRC}/Include)
set(TMC_COMPAT   ${CMAKE_CURRENT_SOURCE_DIR}/compat/include)
set(TMC_CLI      ${CMAKE_CURRENT_SOURCE_DIR}/cli)

# --- Общие флаги для всего кода ядер ------------------------------------------
# Определения и флаги — через INTERFACE-цель, чтобы не менять глобальное
# состояние, если подпроект подключён из корня.
add_library(tmc_kernel_flags INTERFACE)
target_compile_definitions(tmc_kernel_flags INTERFACE
    _PREC_DOUBLE=          # Баг #11: только double (тем же пустым телом, что в Typerth.h)
    _MBCS)                 # как Common.props Windows-проекта (не Unicode)
if(WIN32)
  target_compile_definitions(tmc_kernel_flags INTERFACE WIN32)
endif()
# _WINDOWS / _LIB задаются ПО ЦЕЛЯМ ровно как в .vcxproj: от _WINDOWS зависит
# подпись в заголовке файла S-матрицы (Include/LABEL/RTH.H, MAIN_LABEL). SFILE95
# собирается БЕЗ _WINDOWS -> "TAMIC-RTH V 2.00", как у Windows-версии. Это
# требование формата файла, поэтому определения одинаковы на всех платформах.
target_compile_options(tmc_kernel_flags INTERFACE
    -ffp-contract=off      # без FMA: числа как у MSVC
    -Wall -Wextra -Werror)
# -fpermissive (gcc) больше не нужен: вольности MSVC (лишняя квалификация имени в
# классе, (int)указатель, строковый литерал -> char*) исправлены в исходниках (О1.4).
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Расширения Microsoft, которыми пользуется Windows-код (как в версии для macOS).
  # Предупреждений -Wmicrosoft-* в коде ядер не осталось.
  target_compile_options(tmc_kernel_flags INTERFACE -fms-extensions)
endif()
if(NOT WIN32)
  # Android / iOS: то, что MSVC давал неявно (_complex, __max/__min, _stricmp),
  # и подмена fopen на tmc_fopen (CRLF, регистр имён, обратная косая) —
  # преамбула подставляется в начало КАЖДОГО файла (образец — версия для macOS).
  target_compile_options(tmc_kernel_flags INTERFACE
      -include ${TMC_COMPAT}/tmc_win_prelude.h)
  # Заглушки системных заголовков Windows (windows.h, psapi.h) — отдельный
  # каталог, чтобы на Windows (MinGW) они не заслонили настоящие.
  target_include_directories(tmc_kernel_flags INTERFACE
      ${CMAKE_CURRENT_SOURCE_DIR}/compat/posix)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  # char со знаком, как в MSVC и на x86: на arm64 (Android) char по умолчанию
  # БЕЗ знака, а код сравнивает символы с EOF и отрицательными значениями
  # (CP1251 > 127). Смысл кода — как в Windows; на x86 ключ ничего не меняет.
  target_compile_options(tmc_kernel_flags INTERFACE -fsigned-char)
endif()
target_include_directories(tmc_kernel_flags INTERFACE ${TMC_COMPAT} ${TMC_INCLUDE})

# -----------------------------------------------------------------------------
# tmc_add_library(<name> DIRS <каталог>... FILES <имя.cpp>...)
# Только файлы из FILES (состав .vcxproj), имена сравниваются без учёта регистра.
# -----------------------------------------------------------------------------
function(tmc_add_library name)
  cmake_parse_arguments(A "" "" "DIRS;FILES;DEFS" ${ARGN})
  set(allow "")
  foreach(f ${A_FILES})
    string(TOLOWER "${f}" lf)
    list(APPEND allow "${lf}")
  endforeach()
  set(srcs "")
  foreach(dir ${A_DIRS})
    file(GLOB part CONFIGURE_DEPENDS "${TMC_WIN_SRC}/${dir}/*.cpp" "${TMC_WIN_SRC}/${dir}/*.CPP")
    list(REMOVE_DUPLICATES part)
    foreach(p ${part})
      get_filename_component(bn "${p}" NAME)
      string(TOLOWER "${bn}" lbn)
      if(lbn IN_LIST allow)
        list(APPEND srcs "${p}")
      endif()
    endforeach()
  endforeach()
  list(REMOVE_DUPLICATES srcs)
  list(LENGTH srcs n_found)
  list(LENGTH allow n_want)
  if(NOT n_found EQUAL n_want)
    message(FATAL_ERROR "${name}: найдено ${n_found} из ${n_want} файлов .vcxproj")
  endif()
  add_library(${name} STATIC ${srcs})
  foreach(dir ${A_DIRS})
    target_include_directories(${name} PRIVATE ${TMC_WIN_SRC}/${dir})
  endforeach()
  target_link_libraries(${name} PUBLIC tmc_kernel_flags)
  target_compile_definitions(${name} PRIVATE ${A_DEFS})
endfunction()

# --- Шесть библиотек (состав — из .vcxproj снимка) ----------------------------
tmc_add_library(complex     DEFS _LIB     DIRS libs/complex     FILES CMPLXALL.CPP)
tmc_add_library(sfile95     DEFS _LIB     DIRS libs/SFILE95     FILES
    Errors.cpp Frqdata.cpp Read_s.cpp S_file.cpp S_memf.cpp S_remt.cpp
    S_table.cpp S_wemt.cpp Sadd.cpp Sofftbl.cpp Unite_s.cpp Write_s.cpp Wrrd_s.cpp)
tmc_add_library(exprint     DEFS _LIB     DIRS libs/Expr libs/Inter FILES
    Expr_com.cpp Expr_err.cpp Expr_in1.cpp Expr_per.cpp Expremsg.cpp Get_err.cpp
    Inte_in1.cpp Inte_ope.cpp Inte_val.cpp)
tmc_add_library(TMCLibError DEFS _LIB     DIRS libs/TMCLibError FILES TmcLibError.cpp)
tmc_add_library(prepr       DEFS _WINDOWS DIRS libs/PREPR       FILES
    Def_dest.cpp Def_er.cpp Def_erd.cpp Def_macr.cpp Def_str.cpp Defmis.cpp
    Prep_ld.cpp Prep_mn.cpp Read_buf.cpp)
tmc_add_library(TMCIndan    DEFS _LIB     DIRS libs/TMCIndan    FILES
    FieldIntegrated.cpp TmcRTH_BolckList.cpp TmcRTH_Indan.cpp TmcRTH_IndanOutput.cpp
    TmcRTH_IndanParam.cpp TmcRTH_IndanTopology.cpp TmcRTH_Input.cpp TmcRTH_InputNode.cpp)

# Библиотеки связаны взаимно (prepr <-> sfile95, TMCIndan <-> S-матрица ядра),
# поэтому для однопроходного GNU ld — группа архивов.
set(TMC_KERNEL_LIBS sfile95 prepr exprint TMCIndan TMCLibError complex)

# ДИАГНОСТИКА, по умолчанию выключена: sin/cos/atan2/exp … берутся из ucrtbase.dll
# (как у Windows-версии, MSVC), а не из MinGW. Нужна только для проверки, что
# расхождения в младших разрядах вызваны библиотекой математических функций.
option(TMC_DIAG_UCRT_LIBM "Диагностика: математические функции из ucrtbase.dll (MinGW)" OFF)

# -----------------------------------------------------------------------------
# tmc_add_kernel(<target> <каталог ядра> <название>)
# Состав — только вычислительные модули .vcxproj ядра; GUI-файлы (MainFrm,
# ChildFrm, *Doc, *View, Dialog*, Sound*, StdAfx, PL_GLFUN, Pl_iofor) заменены
# консольными cli/tmc_kernel_console.cpp и cli/tmc_kernel_main.cpp.
# -----------------------------------------------------------------------------
function(tmc_add_kernel target kdir title)
  set(K ${TMC_WIN_SRC}/kernels/${kdir})
  set(V ${TMC_WIN_SRC}/viewers/Tmcrtout)
  add_executable(${target}
      ${K}/TmcRTHNodeDiel.cpp
      ${K}/TmcRTHRectNode.cpp
      ${V}/TmcSMatrix.cpp
      ${V}/TmcTtoS.cpp
      ${V}/TmcWideband.cpp
      ${TMC_CLI}/tmc_kernel_main.cpp
      ${TMC_CLI}/tmc_kernel_console.cpp)
  # Каталог ядра — ПЕРЕД общим Include: PlanRT_H.h, Pl_iofor.h, resource.h и
  # stdafx.h берутся из версии своего ядра (H или X).
  target_include_directories(${target} BEFORE PRIVATE ${K})
  target_compile_definitions(${target} PRIVATE _WINDOWS
      TMC_KERNEL_NAME="${target}" TMC_KERNEL_TITLE="${title}")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT APPLE))
    target_link_libraries(${target} PRIVATE
        -Wl,--start-group ${TMC_KERNEL_LIBS} -Wl,--end-group tmc_kernel_flags)
  else()
    target_link_libraries(${target} PRIVATE ${TMC_KERNEL_LIBS} tmc_kernel_flags)
  endif()
  if(NOT WIN32)
    # Обёртка fopen (текстовый режим и поиск имени, compat/tmc_textmode.cpp).
    target_sources(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compat/tmc_textmode.cpp)
  endif()
  # #pragma comment(lib, "psapi.lib") в Include/TmcCalcTiming.h теперь только для
  # MSVC (#ifdef _MSC_VER), поэтому
  # -Wl,--no-dependent-libraries для Android больше не нужен.
  if(MINGW)
    # Без зависимости от DLL MinGW: исполняемый файл переносим как есть.
    target_link_options(${target} PRIVATE -static)
    if(TMC_DIAG_UCRT_LIBM)
      target_sources(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compat/ucrt_libm_probe.cpp)
    endif()
  endif()
endfunction()



tmc_add_kernel(tmc_rth PlanarRT_H "PlanarRT_H (H-mode)")
tmc_add_kernel(tmc_rtx PlanarRT_X "PlanarRT_X (X-mode)")

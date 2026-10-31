# Счётные ядра tmc_rth (H) и tmc_rtx (X) для Android.
#
# Самостоятельный подпроект: корневой CMakeLists.txt собирает его отдельно, всегда
# как Release, своими ключами и с c++_static, потому что ядро — самостоятельная программа
# в каталоге нативных библиотек приложения.
# Сборка и устройство: https://www.tamic.ru/docs/platforms/android.html#kernels
#
# Математика — в ../win_src, общая с версиями для Linux и Windows. Требования:
#   * -ffp-contract=off — никакого слияния a*b+c в FMA, как у MSVC: числа совпадают с Windows;
#   * -fsigned-char — на arm64 char по умолчанию без знака, а код сравнивает символы с EOF;
#   * H и X — отдельные программы: одинаковые имена классов не линкуются вместе.

cmake_minimum_required(VERSION 3.22)

project(TMC_Kernels
        DESCRIPTION "TMC Suite: счётные ядра tmc_rth / tmc_rtx"
        LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Тип сборки" FORCE)
  endif()
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)   # расширения CRT и strcasecmp в заголовках Bionic и glibc

get_filename_component(WIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../win_src ABSOLUTE)
set(TMC_INCLUDE ${WIN_SRC}/include)
set(TMC_COMPAT  ${CMAKE_CURRENT_SOURCE_DIR}/compat)
set(TMC_CLI     ${CMAKE_CURRENT_SOURCE_DIR}/cli)

# Общие ключи всего кода ядер.
add_library(tmc_kernel_flags INTERFACE)
target_compile_options(tmc_kernel_flags INTERFACE
    -ffp-contract=off
    -fsigned-char
    -include ${TMC_COMPAT}/include/tmc_win_prelude.h)
target_include_directories(tmc_kernel_flags INTERFACE ${TMC_COMPAT}/include ${TMC_INCLUDE})
find_package(Threads REQUIRED)
target_link_libraries(tmc_kernel_flags INTERFACE Threads::Threads)

# tmc_add_library(<имя> <каталог> [<каталог>...] FILES <файл>...)
function(tmc_add_library name)
  cmake_parse_arguments(A "" "" "FILES" ${ARGN})
  set(dirs ${A_UNPARSED_ARGUMENTS})
  list(GET dirs 0 first)
  set(srcs "")
  foreach(f ${A_FILES})
    if(f MATCHES "/")
      list(APPEND srcs ${WIN_SRC}/libs/${f})
    else()
      list(APPEND srcs ${WIN_SRC}/libs/${first}/${f})
    endif()
  endforeach()
  add_library(${name} STATIC ${srcs})
  foreach(d ${dirs})
    target_include_directories(${name} PUBLIC ${WIN_SRC}/libs/${d})
  endforeach()
  target_link_libraries(${name} PUBLIC tmc_kernel_flags)
endfunction()

tmc_add_library(complex complex FILES cmplxall.cpp)
tmc_add_library(sfile95 sfile95 FILES
    errors.cpp frqdata.cpp read_s.cpp s_file.cpp s_memf.cpp s_remt.cpp
    s_table.cpp s_wemt.cpp sadd.cpp sofftbl.cpp unite_s.cpp write_s.cpp wrrd_s.cpp)
tmc_add_library(exprint expr inter FILES
    expr_com.cpp expr_err.cpp expr_in1.cpp expr_per.cpp expremsg.cpp get_err.cpp
    inter/inte_in1.cpp inter/inte_ope.cpp inter/inte_val.cpp)
tmc_add_library(TMCLibError tmcliberror FILES tmcliberror.cpp)
tmc_add_library(prepr prepr FILES
    def_dest.cpp def_er.cpp def_erd.cpp def_macr.cpp def_str.cpp defmis.cpp
    prep_ld.cpp prep_mn.cpp read_buf.cpp)
tmc_add_library(TMCIndan tmcindan FILES
    fieldintegrated.cpp tmcrth_bolcklist.cpp tmcrth_indan.cpp tmcrth_indanoutput.cpp
    tmcrth_indanparam.cpp tmcrth_indantopology.cpp)
target_link_libraries(prepr    PUBLIC TMCLibError)
target_link_libraries(TMCIndan PUBLIC prepr exprint TMCLibError)

# Библиотеки ссылаются друг на друга, поэтому набор оборачивается в группу архивов.
set(KERNEL_LIBS sfile95 prepr exprint TMCIndan TMCLibError complex)

# tmc_add_kernel(<цель> <каталог ядра в win_src> <H|X>)
function(tmc_add_kernel name kernel_dir mode)
  set(KDIR ${WIN_SRC}/${kernel_dir})
  add_executable(${name}
      ${KDIR}/tmcrthnodediel.cpp
      ${KDIR}/tmcrthrectnode.cpp
      ${WIN_SRC}/viewers/tmcrtout/tmcttos.cpp       # время -> S-параметры
      ${WIN_SRC}/viewers/tmcrtout/tmcsmatrix.cpp    # S-матрица -> файл .s
      ${WIN_SRC}/viewers/tmcrtout/tmcwideband.cpp   # многочастотный расчёт
      ${TMC_CLI}/tmc_kernel_main.cpp
      ${TMC_CLI}/tmc_kernel_console.cpp
      ${TMC_COMPAT}/tmc_textmode.cpp)
  # Каталог ядра идёт перед общим: у H и X свои pl_iofor.h и tmc*.h.
  target_include_directories(${name} PRIVATE ${KDIR} ${TMC_INCLUDE} ${WIN_SRC}/viewers/tmcrtout)
  target_compile_definitions(${name} PRIVATE TMC_KERNEL_MODE="${mode}" TMC_KERNEL_EXE="${name}")
  target_link_libraries(${name} PRIVATE
      -Wl,--start-group ${KERNEL_LIBS} -Wl,--end-group tmc_kernel_flags)
endfunction()

tmc_add_kernel(tmc_rth kernels/planarrt_h H)
tmc_add_kernel(tmc_rtx kernels/planarrt_x X)

// tmc_win_prelude.h — преамбула, которую CMake подставляет (-include) в начало
// каждого файла ядер при сборке ВНЕ Windows (Android, далее iOS).
//
// Образец — src/compat/include/tmc_win_prelude.h версии для macOS (проект только
// читался). Исходники писались под MSVC, и часть вещей компилятор давал им «сам
// собой»; на clang/bionic их нет. Недостающее объявляется здесь, снаружи, без
// правки снимка src/win_src:
//   1. слой MFC/win (CString, BOOL, far/near …) — tmc_mfc.h;
//   2. тип _complex (И-3) — дословно как C-ветка typedef.h (struct tag_complex
//      с полями x, y типа _real): раскладка в памяти та же, что в Windows;
//   3. __max/__min старого CRT MSVC (И-3) — тернарное сравнение;
//   4. _stricmp/_strnicmp — strcasecmp/strncasecmp;
//   5. подмена fopen на tmc_fopen (И-5): текстовый режим CRLF, как в Windows,
//      поиск имени без учёта регистра (И-1) и обратная косая в пути (И-6) —
//      см. compat/tmc_textmode.cpp.
// Ни одна формула и ни одна константа здесь не затрагиваются.

#pragma once

#ifndef _WIN32

#include "tmc_mfc.h"

// Ради типа _real (typedef.h защищён #ifndef, повторное включение безвредно).
#include <Typedef.h>

#ifndef __COMPLEX_DEFINED
  #define __COMPLEX_DEFINED
  typedef struct tag_complex { _real x;
                               _real y; } _complex;   /* real and imaginary parts */
#endif

#ifndef __max
  #define __max(a,b) ( (a) > (b) ? (a) : (b) )
#endif
#ifndef __min
  #define __min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

#include <strings.h>
#ifndef _stricmp
  #define _stricmp  strcasecmp
#endif
#ifndef _strnicmp
  #define _strnicmp strncasecmp
#endif

// --- Открытие файлов ----------------------------------------------------------
// <stdio.h> — ДО макроса, иначе подмена испортила бы объявление самой функции.
// <cstdio> здесь намеренно не подключается: его `using ::fopen;` пройдёт уже
// через макрос, и std::fopen тоже попадёт в обёртку. Сам tmc_textmode.cpp
// снимает подмену первой строкой (#undef fopen).
#include <stdio.h>

extern "C" FILE* tmc_fopen(const char* path, const char* mode);
// remove: ядро удаляет старые файлы поля и топологии перед записью, а потом
// дописывает их ("a"); путь у него с обратной косой — его надо привести так же,
// как в tmc_fopen, иначе при повторном расчёте в той же папке файл не удалится.
extern "C" int tmc_remove(const char* path);

#ifndef fopen
  #define fopen tmc_fopen
#endif
#ifndef remove
  #define remove tmc_remove
#endif

#endif // !_WIN32

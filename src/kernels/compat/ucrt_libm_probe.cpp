// ucrt_libm_probe.cpp — ДИАГНОСТИКА (по умолчанию не собирается, опция
// TMC_DIAG_UCRT_LIBM в src/kernels/CMakeLists.txt; только Windows + MinGW).
//
// Зачем: проверить гипотезу, что расхождения с Windows-версией в младших
// разрядах (.AMP/.FAZ, .s, часть строк .t у X) вызваны ТОЛЬКО библиотекой
// математических функций: MinGW (msvcrt + mingwex) и UCRT, с которой собрана
// Windows-версия (MSVC), по-разному округляют sin/cos/atan2 … в последнем бите.
// Здесь эти функции перенаправляются в ucrtbase.dll — ту же реализацию, что у
// Windows-версии. Формулы ядра и порядок вычислений не меняются.
//
// В поставку не входит: на Android/iOS UCRT нет, решение о единой библиотеке
// математических функций — за пользователем.

#if defined(_WIN32) && defined(__MINGW32__)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef double (*TmcFn1)(double);
typedef double (*TmcFn2)(double, double);

static FARPROC TmcUcrt( const char *name )
{
	static HMODULE h = LoadLibraryA( "ucrtbase.dll" );
	FARPROC p = h ? GetProcAddress( h, name ) : NULL;
	if( p == NULL ) ExitProcess( 99 );
	return p;
}

#define TMC_UCRT1(fn) \
	extern "C" double fn( double x ) \
	{ static TmcFn1 p = (TmcFn1)(void*)TmcUcrt( #fn ); return p( x ); }
#define TMC_UCRT2(fn) \
	extern "C" double fn( double x, double y ) \
	{ static TmcFn2 p = (TmcFn2)(void*)TmcUcrt( #fn ); return p( x, y ); }

TMC_UCRT1(sin)  TMC_UCRT1(cos)  TMC_UCRT1(tan)
TMC_UCRT1(asin) TMC_UCRT1(acos) TMC_UCRT1(atan)
TMC_UCRT1(sinh) TMC_UCRT1(cosh) TMC_UCRT1(tanh)
TMC_UCRT1(exp)  TMC_UCRT1(log)  TMC_UCRT1(log10)
TMC_UCRT2(atan2) TMC_UCRT2(pow) TMC_UCRT2(fmod)

// g++ объединяет sin(x) и cos(x) одного аргумента в один вызов sincos (MinGW):
// направляем и его в те же функции UCRT.
extern "C" void sincos( double x, double *s, double *c )
{
	*s = sin( x );
	*c = cos( x );
}

#endif

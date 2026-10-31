// tmc_kernel_console.cpp — консольные замены функций, которые в Windows-сборке
// жили в GUI-файлах ядра (PL_GLFUN.CPP, Pl_iofor.cpp) PlanarRT_H и PlanarRT_X.
//
// Общий для обоих ядер: у H и X эти файлы и заголовки совпадают; нужные заголовки
// (PlanRT_H.h, Pl_iofor.h, Typerth.h) берутся из каталога своего ядра по пути
// заголовков цели. Образец — src/cli/tmc_kernel_console.cpp версии для Linux.
//
// Что здесь:
//   * PutTrace / PutStatistics — строка состояния окна -> stderr (только при
//     TMC_KERNEL_TRACE=1 в окружении; на выходные файлы не влияет);
//   * PutModel / PutSinchronizFlag / Beep* — индикаторы окна и звук -> ничего;
//   * IsAutoRun / IsDemoRun / IsBatchRun — режимы окна -> FALSE: окна нет, а ветки
//     IsAutoRun() в ядре касаются только закрытия окна после последнего шага;
//   * Get*Format — форматы вывода чисел: значения по умолчанию из Pl_iofor.h
//     (в окне они меняются только диалогом, которого в консоли нет);
//   * коды возврата и строки хода в stdout (как /Ar, /Arb Windows-версии);
//   * IsPointInTr — ЕДИНСТВЕННАЯ вычислительная функция: перенесена БАЙТ В БАЙТ из
//     PL_GLFUN.CPP снимка (строки 120–176), закомментированная отладка опущена.
//     Формула, eps и порядок операций не изменены.

#include <afxwin.h>
#include <PlanRT_H.h>
#include <Pl_iofor.h>
#include <Typerth.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

static bool TraceOn( void )
{
	static int n = -1;
	if( n < 0 )
	{
		const char *p = std::getenv( "TMC_KERNEL_TRACE" );
		n = ( p != NULL && p[0] == '1' ) ? 1 : 0;
	};
	return n == 1;
}

// --- Строка состояния окна -> stderr -----------------------------------------
void PutTrace( CString csTrace )
{
	if( TraceOn() ) std::fprintf( stderr, "%s\n", (const char*)csTrace );
}

void PutTrace( const char *szTrace )
{
	if( TraceOn() ) std::fprintf( stderr, "%s\n", szTrace ? szTrace : "" );
}

void PutStatistics( CString csTrace )
{
	if( TraceOn() ) std::fprintf( stderr, "%s\n", (const char*)csTrace );
}

void PutStatistics( const char *szTrace )
{
	if( TraceOn() ) std::fprintf( stderr, "%s\n", szTrace ? szTrace : "" );
}

void PutModel( void ) {}
void PutSinchronizFlag( BOOL, BOOL, BOOL ) {}

// --- Звук окончания шага и расчёта -> ничего ---------------------------------
void BeepStepEnd( int ) {}
void BeepAllEnd( void ) {}

// --- Режимы окна: в консоли окна нет ------------------------------------------
BOOL IsAutoRun( void )  { return FALSE; }
BOOL IsDemoRun( void )  { return FALSE; }
BOOL IsBatchRun( void ) { return FALSE; }
CMainFrame *GetMainFramePointer__( void ) { return NULL; }
void RunOnViewField1( void ) {}

// --- Ключ /Time: отчёт о времени расчёта рядом с .tpl ------------------------
static BOOL bTimeReportFlag = FALSE;
void SetTimeReportOn( void )  { bTimeReportFlag = TRUE; }
void SetTimeReportOff( void ) { bTimeReportFlag = FALSE; }
BOOL IsTimeReport( void )     { return bTimeReportFlag; }

// --- Форматы вывода чисел: значения по умолчанию (Pl_iofor.h) ----------------
static char szNTFormat[MAX_LEN_FORMAT_OUT_FILE]        = { FORMAT_OUT_FILE_NTFORMAT };
static char szDTCurrentFormat[MAX_LEN_FORMAT_OUT_FILE] = { FORMAT_OUT_FILE_DTCurrentFORMAT };
static char szNBFormat[MAX_LEN_FORMAT_OUT_FILE]        = { FORMAT_OUT_FILE_NBFORMAT };
static char szDInpFormat[MAX_LEN_FORMAT_OUT_FILE]      = { FORMAT_OUT_FILE_DINPFORMAT };
static char szDOutFormat[MAX_LEN_FORMAT_OUT_FILE]      = { FORMAT_OUT_FILE_DOUTFORMAT };
static char szDFieldOutFormat[MAX_LEN_FORMAT_OUT_FILE] = { FORMAT_OUT_FILE_DFLDOUTFORMAT };

char *GetnTFormat( void )        { return szNTFormat; }
char *GetdTcurrentFormat( void ) { return szDTCurrentFormat; }
char *GetnBFormat( void )        { return szNBFormat; }
char *GetdInpFormat( void )      { return szDInpFormat; }
char *GetdOutFormat( void )      { return szDOutFormat; }
char *GetdFieldOutFormat( void ) { return szDFieldOutFormat; }

// --- Код возврата и строки хода (как консольный режим /Ar, /Arb) ------------
static int nRunExitCode = TMC_EXIT_OK;

void SetRunExitCode( int nCode ) { nRunExitCode = nCode; }
int  GetRunExitCode( void )      { return nRunExitCode; }
void SetRunMainWnd( HWND ) {}
void SetRunClosing( void ) {}
void PostRunClose( void ) {}

void ConsolePrint( const char *pszFormat, ... )
{
	va_list args;
	std::fprintf( stdout, "[%s] ", TMC_KERNEL_NAME );
	va_start( args, pszFormat );
	std::vfprintf( stdout, pszFormat, args );
	va_end( args );
	std::fprintf( stdout, "\n" );
	std::fflush( stdout );
}

void ConsoleReportResult( void )
{
	switch( GetRunExitCode() )
	{
		case TMC_EXIT_OK:
			ConsolePrint( "RESULT %d: OK - all steps are calculated", TMC_EXIT_OK );
			break;
		case TMC_EXIT_TPL_ERROR:
			ConsolePrint( "RESULT %d: ERROR in .tpl file - calculation is not done", TMC_EXIT_TPL_ERROR );
			break;
		case TMC_EXIT_CALC_ERROR:
			ConsolePrint( "RESULT %d: ERROR during calculation", TMC_EXIT_CALC_ERROR );
			break;
		default:
			ConsolePrint( "RESULT %d: STOPPED - calculation is not finished", GetRunExitCode() );
			break;
	};
}

// --- Вычислительный предикат: БАЙТ В БАЙТ из PL_GLFUN.CPP (строки 120–176) ---
BOOL IsPointInTr( double x1, double y1, double x2, double y2, double x3, double y3, double x0, double y0 )
{
	double s120, s230, s310, s123, eps;

	s123 = fabs( (x2-x1)*(y3-y1) - (x3-x1)*(y2-y1) );
	s120 = fabs( (x1-x0)*(y2-y0) - (x2-x0)*(y1-y0) );
	s230 = fabs( (x2-x0)*(y3-y0) - (x3-x0)*(y2-y0) );
	s310 = fabs( (x3-x0)*(y1-y0) - (x1-x0)*(y3-y0) );

	eps = s123*(1.e-10);
	if( (s120 + s230 + s310) > (s123 + eps) )
	{
		return FALSE;
	};

	return TRUE;
}

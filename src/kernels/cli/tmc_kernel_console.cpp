/// @file tmc_kernel_console.cpp
/// @brief Консольные реализации функций, объявленных в `typerth.h` и `pl_iofor.h`: ход расчёта,
///        код завершения, сообщения пакетного режима, форматы чисел.
///
/// Общий для обоих ядер: нужные заголовки находятся в каталоге своего ядра. Строка состояния окна
/// Windows-версии становится выводом в поток ошибок. Его читает `KernelRunner` приложения, поэтому
/// на Android такты идут все, а не раз в 10 с, как в журнале Linux-версии. Форматы чисел — значения
/// по умолчанию из `pl_iofor.h`: в окне их менял диалог, которого здесь нет. Единственная
/// вычислительная функция — `IsPointInTr`, перенесена без изменений.
/// @see https://www.tamic.ru/docs/platforms/android.html#kernels

#include <pl_iofor.h>   // объявления формат-геттеров, Is*Run, ConsolePrint, TMC_EXIT_*
#include <typerth.h>    // объявления PutTrace/PutStatistics/PutModel/Beep*/IsPointInTr

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

/// @brief Включает вывод тактов в поток ошибок: задаёт оболочка переменной `TMC_KERNEL_TRACE=1`.
/// @return true — строки хода нужны.
static bool TraceOn()
{
    static const bool on = [] {
        const char *p = std::getenv( "TMC_KERNEL_TRACE" );
        return p != nullptr && p[0] == '1';
    }();
    return on;
}

// --- Строка состояния -> поток ошибок: её читает KernelRunner ---------------
void PutTrace( CString csTrace )          { if( TraceOn() ) std::fprintf( stderr, "%s\n", (const char*)csTrace ); }
void PutTrace( const char *szTrace )      { if( TraceOn() ) std::fprintf( stderr, "%s\n", szTrace ? szTrace : "" ); }
void PutStatistics( CString csTrace )     { if( TraceOn() ) std::fprintf( stderr, "%s\n", (const char*)csTrace ); }
void PutStatistics( const char *szTrace ) { if( TraceOn() ) std::fprintf( stderr, "%s\n", szTrace ? szTrace : "" ); }

// --- Код завершения и сообщения пакетного режима (как pl_iofor.cpp 5.0) -----
/// Код завершения процесса (`TMC_EXIT_*`).
static volatile int nRunExitCode = TMC_EXIT_OK;

void SetRunExitCode( int nCode ) { nRunExitCode = nCode; }
int  GetRunExitCode( void )      { return nRunExitCode; }

/// Последнее предупреждение расчёта: повторяется в конце, как в строке состояния окна.
static char szRunWarning[1024] = "";

void SetRunWarning( const char *pszText )
{
    std::snprintf( szRunWarning, sizeof( szRunWarning ), "%s", pszText ? pszText : "" );
}

const char *GetRunWarning( void ) { return szRunWarning; }

void ConsolePrint( const char *pszFormat, ... )
{
    char szBuf[4096];
    va_list args;
    va_start( args, pszFormat );
    std::vsnprintf( szBuf, sizeof( szBuf ), pszFormat, args );
    va_end( args );

    std::fprintf( stdout, "[%s] %s\n", TMC_KERNEL_EXE, szBuf );
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

// --- Форматы вывода: дефолты из pl_iofor.h (FORMAT_OUT_FILE_*) ---------------
char *GetnTFormat( void )
{
    static char buf[] = FORMAT_OUT_FILE_NTFORMAT;
    return buf;
}
char *GetdTcurrentFormat( void )
{
    static char buf[] = FORMAT_OUT_FILE_DTCurrentFORMAT;
    return buf;
}
char *GetnBFormat( void )
{
    static char buf[] = FORMAT_OUT_FILE_NBFORMAT;
    return buf;
}
char *GetdInpFormat( void )
{
    static char buf[] = FORMAT_OUT_FILE_DINPFORMAT;
    return buf;
}
char *GetdOutFormat( void )
{
    static char buf[] = FORMAT_OUT_FILE_DOUTFORMAT;
    return buf;
}
char *GetdFieldOutFormat( void )
{
    static char buf[] = FORMAT_OUT_FILE_DFLDOUTFORMAT;
    return buf;
}

// --- Вычислительный предикат: ПЕРЕНЕСЁН БАЙТ-В-БАЙТ из pl_glfun.cpp ----------
// (математика не изменена; см. описание файла).
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

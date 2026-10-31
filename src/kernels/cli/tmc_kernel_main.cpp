/// @file tmc_kernel_main.cpp
/// @brief Точка входа счётных ядер `tmc_rth` и `tmc_rtx` на Android: файл задания → расчёт → файлы результатов.
///
/// Один файл на оба ядра: заголовки находятся в каталоге своего ядра, а имя и режим задают
/// макросы `TMC_KERNEL_MODE` ("H"/"X") и `TMC_KERNEL_EXE` из CMake. Цикл повторяет
/// `CPlanRT_HDoc::RunAll()` и `RunAll1()` Windows-версии: те же сообщения и те же коды завершения.
///
/// Только в версии для Android: ядро — отдельный процесс приложения. Его запускает служба
/// расчёта через `KernelRunner`, а ход читает из потоков вывода. Поэтому перед чтением шага
/// выводится строка `Step N of M: read` (стадия подготовки на экране «Расчёт»), а SIGTERM от
/// кнопки «Остановить» завершает расчёт с кодом 3.
/// @see https://www.tamic.ru/docs/platforms/android.html#kernels
/// @see https://www.tamic.ru/docs/user-manual/console-mode.html

#include <tmcrthrectnode.h>    // CTmcRTHRectNode — вычислительный класс ядра
#include <pl_iofor.h>          // TMC_EXIT_*, ConsolePrint

#include <climits>
#include <clocale>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include <unistd.h>

namespace {

/// Признак остановки: выставляется обработчиком сигнала.
volatile std::sig_atomic_t g_bStop = 0;
/// Ядро, которому обработчик сигнала передаёт остановку; `nullptr` до начала расчёта.
CTmcRTHRectNode *g_pNode = nullptr;
/// Текущий шаг задания: для сообщений об исключениях.
int nStep = 1;

/// @brief Обработчик SIGINT и SIGTERM: останавливает расчёт, как кнопка «Остановить».
extern "C" void OnStopSignal( int )
{
    g_bStop = 1;
    if( g_pNode ) g_pNode->Stop();
}

/// @brief Проверяет ключ: «-имя» или «--имя» без учёта регистра.
/// @param[in] arg довод командной строки
/// @param[in] name имя ключа без дефиса
/// @return true — довод и есть этот ключ.
bool IsKey( const char *arg, const char *name )
{
    if( arg[0] != '-' ) return false;
    const char *k = ( arg[1] == '-' ) ? arg + 2 : arg + 1;
    return strcasecmp( k, name ) == 0;
}

/// @brief Печатает справку по ключам.
/// @param[in] exe имя программы
void PrintUsage( const char *exe )
{
    std::printf(
        "Usage:  %s [keys] <task.tpl>\n"
        "  -Time   calculation time report <task>.timing_%s_android64.txt\n"
        "  -DN     write the directional pattern files after the calculation\n"
        "Exit code: 0 - all steps are calculated, 1 - error in the task,\n"
        "           2 - error during the calculation, 3 - stopped.\n",
        exe, TMC_KERNEL_MODE );
}

/// @brief Полный путь к заданию: каталог задания нужен ядру для `#include` и выходных файлов.
/// @param[in] path путь из командной строки
/// @return Полный путь; если его нельзя получить — путь как есть.
std::string FullPath( const char *path )
{
    char buf[PATH_MAX];
    if( ::realpath( path, buf ) ) return std::string( buf );
    return std::string( path );
}

/// @brief Считает все шаги задания: тело `CPlanRT_HDoc::RunAll1()` Windows-версии.
///
/// Ошибка шага не обрывает свип частот: S-матрица этого шага записывается нулями, а итог
/// сообщает первые десять таких шагов.
/// @param[in,out] cRectNode счётное ядро
/// @param[in] csPath полный путь к заданию
void RunAll1( CTmcRTHRectNode &cRectNode, const CString &csPath )
{
    CString csBuf;
    int i, n;
    int nErrStep = 0;           // шаги, посчитанные с ошибкой
    nStep = 1;
    CString csErrFirst, csErrSteps;

    SetRunWarning( "" );        // предупреждения этого расчёта

    __int64 qTmPr = TmcTimingNow();                                 // TIMING
    n = cRectNode.GetNStepMax( csPath ) + 1;
    cRectNode.GetTiming().qRunPrepr += TmcTimingNow() - qTmPr;      // TIMING

    // ошибка препроцессора или нет #STEP в .tpl — считать нечего
    if( cRectNode.IsError() )
    {
        ConsolePrint( "ERROR in .tpl: %s", (LPCTSTR)cRectNode.GetErrorMessage() );
        SetRunExitCode( TMC_EXIT_TPL_ERROR );
        return;
    };
    if( n - 1 < nStep )
    {
        ConsolePrint( "ERROR in .tpl: no #STEP to calculate (steps found: %d)", n - 1 );
        SetRunExitCode( TMC_EXIT_TPL_ERROR );
        return;
    };
    ConsolePrint( "Steps: %d", n - 1 );

    for( i = nStep; i < n; i++ )
    {
        nStep = i;

        if( g_bStop )
        {
            ConsolePrint( "STOPPED before step %d of %d", nStep, n - 1 );
            SetRunExitCode( TMC_EXIT_ABORTED );
            return;
        };
        // Чтение шага у больших заданий идёт долго: экран «Расчёт» показывает эту стадию.
        ConsolePrint( "Step %d of %d: read", nStep, n - 1 );
        cRectNode.ReadData( csPath, nStep );
        if( cRectNode.IsError() )
        {
            ConsolePrint( "ERROR in .tpl (step %d of %d): %s", nStep, n - 1,
                (LPCTSTR)cRectNode.GetErrorMessage() );
            SetRunExitCode( TMC_EXIT_TPL_ERROR );
            return;
        };

        ConsolePrint( "Step %d of %d: start", nStep, n - 1 );

        if( g_bStop )
        {
            ConsolePrint( "STOPPED at step %d of %d", nStep, n - 1 );
            SetRunExitCode( TMC_EXIT_ABORTED );
            return;
        };
        cRectNode.RunStep();
        if( cRectNode.IsError() )
        {
            // Свип не обрывается: S-матрица этого шага в .s нулевая, ошибка
            // идёт в журнал, считается следующий шаг.
            const int nZero = cRectNode.GetOutput()->PutSmatrixZero( cRectNode.GetTopology().GetBlockList() );
            const double rFreq = cRectNode.GetParam().rFreq / cRectNode.GetParam().rFreqUnit;
            const char *pszUnit = (LPCTSTR)cRectNode.GetParam().csFreqUnit;
            ConsolePrint( "ERROR in calculation (step %d of %d, %lg %s): %s%s", nStep, n - 1,
                rFreq, pszUnit, (LPCTSTR)cRectNode.GetErrorMessage(),
                ( nZero > 0 ) ? " - the S-matrix of this step is zeros in the .s" :
                ( ( nZero < 0 ) ? " - zeros could not be written to the .s" : "" ) );
            csBuf.Format( "Error at step %d (%lg %s): %s", nStep, rFreq, pszUnit,
                (LPCTSTR)cRectNode.GetErrorMessage() );
            PutTrace( csBuf );
            if( nErrStep == 0 ) csErrFirst = csBuf;
            if( nErrStep < 10 )
            {
                CString csS;
                csS.Format( "%s%d (%lg %s)", ( nErrStep > 0 ) ? ", " : "", nStep, rFreq, pszUnit );
                csErrSteps += csS;
            };
            nErrStep++;
            if( g_bStop )
            {
                ConsolePrint( "STOPPED at step %d of %d", nStep, n - 1 );
                SetRunExitCode( TMC_EXIT_ABORTED );
                return;
            };
            continue;
        };
        if( g_bStop )
        {
            ConsolePrint( "STOPPED at step %d of %d", nStep, n - 1 );
            SetRunExitCode( TMC_EXIT_ABORTED );
            return;
        };

        ConsolePrint( "Step %d of %d: done", nStep, n - 1 );
    };

    // Итог: были шаги с ошибкой — код 2 и перечень первых 10 таких шагов.
    if( nErrStep > 0 )
    {
        csBuf.Format( "Calculated with errors at %d step(s): %s%s - the S-matrix there is zeros. First: %s",
            nErrStep, (LPCTSTR)csErrSteps, ( nErrStep > 10 ) ? ", ..." : "", (LPCTSTR)csErrFirst );
        ConsolePrint( "%s", (LPCTSTR)csBuf );
        PutTrace( csBuf );
        SetRunExitCode( TMC_EXIT_CALC_ERROR );
        return;
    };
    if( GetRunWarning()[0] != '\0' ) PutTrace( GetRunWarning() );   // предупреждение ещё раз в конце журнала
    SetRunExitCode( TMC_EXIT_OK );
}

} // namespace

/// @brief Точка входа: ключи, проверка задания, расчёт, отчёт о времени, код завершения.
/// @param[in] argc число доводов
/// @param[in] argv задание `.tpl` и ключи (см. `PrintUsage`)
/// @return 0 — все шаги посчитаны, 1 — ошибка задания, 2 — ошибка расчёта, 3 — остановлено.
int main( int argc, char **argv )
{
    // Числа в файлах TMC пишутся и читаются только с точкой, как в Windows-версии.
    // Локаль телефона с запятой не должна влиять на выходные файлы.
    std::setlocale( LC_NUMERIC, "C" );

    const char *exe = TMC_KERNEL_EXE;
    const char *pszTask = nullptr;
    bool bTime = false;

    for( int i = 1; i < argc; i++ )
    {
        const char *a = argv[i];
        if( IsKey( a, "h" ) || IsKey( a, "help" ) )
        {
            PrintUsage( exe );
            return 0;
        }
        if( IsKey( a, "Time" ) ) { bTime = true; continue; }
        if( IsKey( a, "DN" ) )   { ::setenv( "TMC_AUTO_DN", "1", 1 ); continue; }
        if( a[0] == '-' && a[1] != '\0' )
        {
            std::fprintf( stderr, "%s: unknown key %s\n", exe, a );
            return TMC_EXIT_TPL_ERROR;
        }
        if( pszTask )
        {
            std::fprintf( stderr, "%s: only one task file can be given\n", exe );
            return TMC_EXIT_TPL_ERROR;
        }
        pszTask = a;
    }

    if( !pszTask )
    {
        PrintUsage( exe );
        ConsolePrint( "ERROR: no .tpl file in the command line" );
        SetRunExitCode( TMC_EXIT_TPL_ERROR );
        ConsoleReportResult();
        return GetRunExitCode();
    }
    if( FILE *fin = fopen( pszTask, "rb" ) )
    {
        fclose( fin );
    }
    else
    {
        ConsolePrint( "ERROR: can't open file %s", pszTask );
        SetRunExitCode( TMC_EXIT_TPL_ERROR );
        ConsoleReportResult();
        return GetRunExitCode();
    }

    const std::string full = FullPath( pszTask );
    const CString csPath( full.c_str() );
    ConsolePrint( "File: %s", pszTask );

    struct sigaction sa;
    std::memset( &sa, 0, sizeof( sa ) );
    sa.sa_handler = OnStopSignal;
    sigemptyset( &sa.sa_mask );
    ::sigaction( SIGINT, &sa, nullptr );
    ::sigaction( SIGTERM, &sa, nullptr );

    // Тело CPlanRT_HDoc::RunAll(): замер всего расчёта, исключения превращаются в код 2.
    // До конца расчёта стоит код «остановлено», как в пакетном режиме Windows-версии.
    SetRunExitCode( TMC_EXIT_ABORTED );
    static CTmcRTHRectNode cRectNode;   // большой объект — не на стеке
    g_pNode = &cRectNode;
    cRectNode.GetTiming().BeginRunAll();
    try
    {
        RunAll1( cRectNode, csPath );
    }
    catch( std::bad_alloc & )
    {
        ConsolePrint( "ERROR in calculation (step %d): not enough memory", nStep );
        PutTrace( "Error: not enough memory" );
        SetRunExitCode( TMC_EXIT_CALC_ERROR );
    }
    catch( ... )
    {
        ConsolePrint( "ERROR in calculation (step %d): unknown exception", nStep );
        PutTrace( "Error: exception in calculation" );
        SetRunExitCode( TMC_EXIT_CALC_ERROR );
    };
    cRectNode.GetTiming().EndRunAll();
    if( bTime )
    {
        const CString csReport = cRectNode.GetTiming().DefaultReportName();
        if( cRectNode.GetTiming().SaveReport( csReport ) )
            ConsolePrint( "Time report: %s", (LPCTSTR)csReport );
        else
            ConsolePrint( "WARNING: can't write the time report %s", (LPCTSTR)csReport );
    }
    g_pNode = nullptr;

    ConsoleReportResult();
    return GetRunExitCode();
}

// tmc_kernel_main.cpp — консольная точка входа ядер tmc_rth (H) и tmc_rtx (X).
//
// Один файл на оба ядра: имя и подпись задаются при сборке (TMC_KERNEL_NAME,
// TMC_KERNEL_TITLE в src/kernels/CMakeLists.txt), класс расчёта CTmcRTHRectNode
// берётся из каталога своего ядра по пути заголовков цели.
//
// Логика повторяет CPlanRT_HDoc::RunAll()/RunAll1() снимка Windows-кода
// (kernels/PlanarRT_H/PlanRT_HDoc.cpp) без MFC Doc/View: GetNStepMax ->
// для каждого шага ReadData / RunStep с проверкой IsError; коды возврата и
// строки хода — как в консольном режиме /Ar, /Arb Windows-версии:
//   0 — все шаги посчитаны; 1 — ошибка в .tpl; 2 — ошибка при расчёте шага;
//   3 — остановлено (на этом шаге не используется).
// Математика не затронута.
//
// Использование:  tmc_rth <задание.tpl> [/Time] [/SavePrepr]
// /SavePrepr (отладка): сохранить развёрнутое задание в <задание>~$Prepr$~, как
// прежний временный файл. Без ключа препроцессор работает только в памяти (M15).

#include <afxwin.h>
#include <PlanRT_H.h>          // resource.h ядра (нужен заголовкам окон из Pl_iofor.h)
#include <TmcRTHRectNode.h>
#include <Pl_iofor.h>

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#ifdef _WIN32
#include <stdlib.h>   // _fullpath
#else
#include <climits>    // PATH_MAX
#endif

// Полный путь к заданию: Windows-версия получает его от MFC (GetPathName()),
// от него ядро берёт каталог задания для #include и выходных файлов.
static std::string FullPath( const char *pszPath )
{
#ifdef _WIN32
	char buf[_MAX_PATH * 4];
	if( _fullpath( buf, pszPath, sizeof(buf) ) != NULL ) return std::string( buf );
#else
	char buf[PATH_MAX];
	if( realpath( pszPath, buf ) != NULL ) return std::string( buf );
#endif
	return std::string( pszPath );
}

static void PrintUsage( void )
{
	std::fprintf( stderr,
		"%s - TAMIC counting kernel %s, console version\n"
		"usage: %s <task.tpl> [/Time] [/SavePrepr]\n"
		"exit codes: 0 - done, 1 - error in .tpl, 2 - calculation error\n",
		TMC_KERNEL_NAME, TMC_KERNEL_TITLE, TMC_KERNEL_NAME );
}

int main( int argc, char **argv )
{
	// Числа в файлах TMC — только с точкой.
	std::setlocale( LC_NUMERIC, "C" );

	const char *pszTpl = NULL;
	for( int i = 1; i < argc; i++ )
	{
		if( ( argv[i][0] == '/' || argv[i][0] == '-' ) && argv[i][1] != '\0' &&
			strchr( argv[i] + 1, '.' ) == NULL && strchr( argv[i] + 1, '/' ) == NULL &&
			strchr( argv[i] + 1, '\\' ) == NULL )
		{
			if( _stricmp( argv[i] + 1, "Time" ) == 0 ) SetTimeReportOn();
			else if( _stricmp( argv[i] + 1, "SavePrepr" ) == 0 ) SetPreprSaveFile( TRUE );
			else if( _stricmp( argv[i] + 1, "Ar" ) == 0 || _stricmp( argv[i] + 1, "Arb" ) == 0 ) {}
			else if( _stricmp( argv[i] + 1, "h" ) == 0 || _stricmp( argv[i] + 1, "-help" ) == 0 )
			{
				PrintUsage();
				return 0;
			}
			else
			{
				std::fprintf( stderr, "[%s] unknown key: %s\n", TMC_KERNEL_NAME, argv[i] );
				return TMC_EXIT_TPL_ERROR;
			};
			continue;
		};
		pszTpl = argv[i];
	};

	if( pszTpl == NULL )
	{
		PrintUsage();
		return TMC_EXIT_TPL_ERROR;
	};

	std::string sPath = FullPath( pszTpl );
	// fopen без std:: — вне Windows это обёртка tmc_fopen (макрос преамбулы).
	if( FILE *f = fopen( sPath.c_str(), "rb" ) ) std::fclose( f );
	else
	{
		ConsolePrint( "ERROR: .tpl file not found: %s", sPath.c_str() );
		ConsolePrint( "RESULT 1: ERROR in .tpl file - calculation is not done" );
		return TMC_EXIT_TPL_ERROR;
	};
	ConsolePrint( "File: %s", sPath.c_str() );

#ifndef _WIN32
	// Android / iOS: код ядра ищет каталог задания по обратной
	// косой (CTmcRTH_Indan::SetTplFileName) и собирает пути выходных и включаемых
	// файлов как "%s\\%s". Ядру передаётся путь в записи Windows
	// (\data\local\tmp\...\voln.tpl); настоящий путь с прямой косой восстанавливает
	// обёртка tmc_fopen (compat/tmc_textmode.cpp) при каждом открытии. Так снимок
	// не правится, а имя файла внутри результатов записано, как в Windows.
	for( std::string::size_type i = 0; i < sPath.size(); i++ )
		if( sPath[i] == '/' ) sPath[i] = '\\';
#endif

	static CTmcRTHRectNode cRectNode;
	const char *pszPath = sPath.c_str();
	int nStep = 1;
	int nCode = TMC_EXIT_OK;

	cRectNode.GetTiming().BeginRunAll();
	try
	{
		__int64 qTmPr = TmcTimingNow();
		int n = cRectNode.GetNStepMax( pszPath ) + 1;
		cRectNode.GetTiming().qRunPrepr += TmcTimingNow() - qTmPr;

		if( cRectNode.IsError() )
		{
			ConsolePrint( "ERROR in .tpl: %s", (LPCTSTR)cRectNode.GetErrorMessage() );
			nCode = TMC_EXIT_TPL_ERROR;
		}
		else if( n - 1 < nStep )
		{
			ConsolePrint( "ERROR in .tpl: no #STEP to calculate (steps found: %d)", n - 1 );
			nCode = TMC_EXIT_TPL_ERROR;
		}
		else
		{
			ConsolePrint( "Steps: %d", n - 1 );
			for( nStep = 1; nStep < n; nStep++ )
			{
				// Чтение шага (топология, ε и плазма на сетке) у больших заданий идёт
				// долго — оболочка показывает эту стадию (KernelProgress::stage).
				ConsolePrint( "Step %d of %d: read", nStep, n - 1 );
				cRectNode.ReadData( pszPath, nStep );
				if( cRectNode.IsError() )
				{
					ConsolePrint( "ERROR in .tpl (step %d of %d): %s", nStep, n - 1,
						(LPCTSTR)cRectNode.GetErrorMessage() );
					nCode = TMC_EXIT_TPL_ERROR;
					break;
				};
				ConsolePrint( "Step %d of %d: start", nStep, n - 1 );
				cRectNode.RunStep();
				if( cRectNode.IsError() )
				{
					ConsolePrint( "ERROR in calculation (step %d of %d): %s", nStep, n - 1,
						(LPCTSTR)cRectNode.GetErrorMessage() );
					nCode = TMC_EXIT_CALC_ERROR;
					break;
				};
				ConsolePrint( "Step %d of %d: done", nStep, n - 1 );
			};
		};
	}
	catch( std::bad_alloc & )
	{
		ConsolePrint( "ERROR in calculation (step %d): not enough memory", nStep );
		nCode = TMC_EXIT_CALC_ERROR;
	}
	catch( ... )
	{
		ConsolePrint( "ERROR in calculation (step %d): unknown exception", nStep );
		nCode = TMC_EXIT_CALC_ERROR;
	};
	cRectNode.GetTiming().EndRunAll();
	if( IsTimeReport() ) cRectNode.GetTiming().SaveReport( cRectNode.GetTiming().DefaultReportName() );

	SetRunExitCode( nCode );
	ConsoleReportResult();
	return nCode;
}

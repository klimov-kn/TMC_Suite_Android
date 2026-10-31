// tmc_win_posix.h — функции Win32, которыми пользуются ядра вне расчёта, для
// Android / iOS (POSIX). Подключается из tmc_mfc.h только вне _WIN32.
//
// Кому нужно:
//   * Include/TmcCalcTiming.h (замер времени этапов, ключ /Time): счётчик
//     QueryPerformanceCounter, местное время, критическая секция, время ЦП
//     потока, пик памяти процесса, сведения о машине;
//   * libs/TMCIndan/FieldIntegrated.cpp: GlobalMemoryStatusEx — сколько памяти
//     свободно (выбор полосового режима ДН, BORDER);
//   * kernels/*/TmcRTHNodeDiel.cpp: GetTickCount (интервал строк хода).
// Это НЕ математика и НЕ формат файлов: только время, память и имя машины.
//
// Типы-«слова» Win32 (LowPart, HighPart …) заданы точной ширины (uint32_t),
// потому что DWORD здесь — unsigned long (так его определяет typedef.h снимка),
// а на LP64 (arm64) это 64 бита.

#pragma once

#ifndef _WIN32

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>

static_assert(sizeof(__int64) == 8, "__int64 must be 64-bit (clang -fms-extensions)");

// --- Счётчик высокого разрешения: CLOCK_MONOTONIC, единица — 1 нс -----------
typedef union _LARGE_INTEGER {
    struct { uint32_t LowPart; int32_t HighPart; };
    long long QuadPart;
} LARGE_INTEGER;
typedef union _ULARGE_INTEGER {
    struct { uint32_t LowPart; uint32_t HighPart; };
    unsigned long long QuadPart;
} ULARGE_INTEGER;

inline BOOL QueryPerformanceCounter(LARGE_INTEGER* p)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    p->QuadPart = (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
    return TRUE;
}
inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* p)
{
    p->QuadPart = 1000000000LL;
    return TRUE;
}
inline DWORD GetTickCount(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)(uint32_t)((unsigned long long)ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000);
}

// --- Местное время ------------------------------------------------------------
typedef struct _SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME;
inline void GetLocalTime(SYSTEMTIME* st)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    time_t t = ts.tv_sec;
    struct tm tmv;
    localtime_r(&t, &tmv);
    st->wYear = (WORD)(tmv.tm_year + 1900);
    st->wMonth = (WORD)(tmv.tm_mon + 1);
    st->wDayOfWeek = (WORD)tmv.tm_wday;
    st->wDay = (WORD)tmv.tm_mday;
    st->wHour = (WORD)tmv.tm_hour;
    st->wMinute = (WORD)tmv.tm_min;
    st->wSecond = (WORD)tmv.tm_sec;
    st->wMilliseconds = (WORD)(ts.tv_nsec / 1000000);
}

// --- Критическая секция: рекурсивный мьютекс (как в Win32) -------------------
typedef struct _CRITICAL_SECTION { pthread_mutex_t m; } CRITICAL_SECTION;
inline void InitializeCriticalSection(CRITICAL_SECTION* c)
{
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&c->m, &a);
    pthread_mutexattr_destroy(&a);
}
inline void DeleteCriticalSection(CRITICAL_SECTION* c) { pthread_mutex_destroy(&c->m); }
inline void EnterCriticalSection(CRITICAL_SECTION* c)  { pthread_mutex_lock(&c->m); }
inline void LeaveCriticalSection(CRITICAL_SECTION* c)  { pthread_mutex_unlock(&c->m); }

// --- Процесс и поток: пик памяти и время ЦП ----------------------------------
inline HANDLE GetCurrentProcess(void) { return (HANDLE)(intptr_t)-1; }
inline HANDLE GetCurrentThread(void)  { return (HANDLE)(intptr_t)-2; }

typedef struct _PROCESS_MEMORY_COUNTERS {
    DWORD  cb;
    DWORD  PageFaultCount;
    size_t PeakWorkingSetSize;
    size_t WorkingSetSize;
    size_t QuotaPeakPagedPoolUsage, QuotaPagedPoolUsage;
    size_t QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage;
    size_t PagefileUsage, PeakPagefileUsage;
} PROCESS_MEMORY_COUNTERS;
// Пик резидентной памяти процесса (ru_maxrss, КБ) — аналог PeakWorkingSetSize.
inline BOOL GetProcessMemoryInfo(HANDLE, PROCESS_MEMORY_COUNTERS* p, DWORD)
{
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) != 0) return FALSE;
    p->PeakWorkingSetSize = (size_t)ru.ru_maxrss * 1024u;
    return TRUE;
}
// Время ЦП вызывающего потока (RUSAGE_THREAD), в единицах FILETIME (100 нс).
inline BOOL GetThreadTimes(HANDLE, FILETIME* pC, FILETIME* pE, FILETIME* pK, FILETIME* pU)
{
    struct rusage ru;
    if (getrusage(RUSAGE_THREAD, &ru) != 0) return FALSE;
    unsigned long long u = (unsigned long long)ru.ru_utime.tv_sec * 10000000ULL + ru.ru_utime.tv_usec * 10ULL;
    unsigned long long k = (unsigned long long)ru.ru_stime.tv_sec * 10000000ULL + ru.ru_stime.tv_usec * 10ULL;
    pC->dwLowDateTime = pC->dwHighDateTime = 0;
    pE->dwLowDateTime = pE->dwHighDateTime = 0;
    pU->dwLowDateTime = (DWORD)(uint32_t)u;  pU->dwHighDateTime = (DWORD)(uint32_t)(u >> 32);
    pK->dwLowDateTime = (DWORD)(uint32_t)k;  pK->dwHighDateTime = (DWORD)(uint32_t)(k >> 32);
    return TRUE;
}

// --- Ожидание потоков ---------------------------------------------------------
// Единственное место — заполнение eps несколькими потоками (EPSFAST,
// kernels/*/TmcRTHNodeDiel.cpp): пока потоки std::thread работают, раз в 250 мс
// выводится строка хода, затем всем делается join(). Дескриптор потока POSIX
// (pthread_t) нельзя ждать с тайм-аутом, поэтому сразу возвращается «готово»:
// строки хода во время заполнения eps не выводятся, а ожидание выполняет join().
// На результат это не влияет.
#ifndef WAIT_OBJECT_0
#define WAIT_OBJECT_0 0x00000000L
#endif
#ifndef WAIT_TIMEOUT
#define WAIT_TIMEOUT  0x00000102L
#endif
inline DWORD WaitForMultipleObjects(DWORD, const HANDLE*, BOOL, DWORD) { return WAIT_OBJECT_0; }

// --- Сведения о машине --------------------------------------------------------
inline BOOL GetComputerNameA(char* name, DWORD* n)
{
    if (!name || !n || *n == 0) return FALSE;
    if (gethostname(name, (size_t)*n) != 0) return FALSE;
    name[*n - 1] = '\0';
    *n = (DWORD)std::strlen(name);
    return TRUE;
}

// Реестра нет: имя процессора не читается (в отчёте — «?»).
typedef void* HKEY;
typedef BYTE* LPBYTE;
typedef long  LSTATUS;
#define HKEY_LOCAL_MACHINE ((HKEY)(uintptr_t)0x80000002u)
#ifndef KEY_READ
#define KEY_READ 0x20019
#endif
#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS 0L
#endif
#ifndef ERROR_FILE_NOT_FOUND
#define ERROR_FILE_NOT_FOUND 2L
#endif
inline LSTATUS RegOpenKeyExA(HKEY, const char*, DWORD, DWORD, HKEY*) { return ERROR_FILE_NOT_FOUND; }
inline LSTATUS RegQueryValueExA(HKEY, const char*, DWORD*, DWORD*, LPBYTE, DWORD*) { return ERROR_FILE_NOT_FOUND; }
inline LSTATUS RegCloseKey(HKEY) { return ERROR_SUCCESS; }

typedef struct _SYSTEM_INFO { DWORD dwNumberOfProcessors; DWORD dwPageSize; } SYSTEM_INFO;
inline void GetSystemInfo(SYSTEM_INFO* si)
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    long p = sysconf(_SC_PAGESIZE);
    si->dwNumberOfProcessors = (DWORD)(n > 0 ? n : 1);
    si->dwPageSize = (DWORD)(p > 0 ? p : 4096);
}

// --- Память: всего и доступно (MemAvailable из /proc/meminfo) ----------------
typedef struct _MEMORYSTATUSEX {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    unsigned long long ullTotalPhys, ullAvailPhys;
    unsigned long long ullTotalPageFile, ullAvailPageFile;
    unsigned long long ullTotalVirtual, ullAvailVirtual, ullAvailExtendedVirtual;
} MEMORYSTATUSEX;
inline BOOL GlobalMemoryStatusEx(MEMORYSTATUSEX* ms)
{
    struct sysinfo si;
    if (sysinfo(&si) != 0) return FALSE;
    const unsigned long long unit = si.mem_unit ? si.mem_unit : 1;
    ms->ullTotalPhys = (unsigned long long)si.totalram * unit;
    // «Доступно» в смысле Windows (с учётом кэша) — MemAvailable ядра Linux;
    // freeram без кэша на телефоне сильно занижено.
    unsigned long long avail = ((unsigned long long)si.freeram + si.bufferram) * unit;
    if (FILE* f = std::fopen("/proc/meminfo", "rb")) {
        char line[128];
        unsigned long long kb = 0;
        while (std::fgets(line, sizeof(line), f))
            if (std::sscanf(line, "MemAvailable: %llu kB", &kb) == 1) { avail = kb * 1024ULL; break; }
        std::fclose(f);
    }
    ms->ullAvailPhys = avail;
    ms->ullTotalPageFile = ms->ullAvailPageFile = 0;
    ms->ullTotalVirtual = ms->ullAvailVirtual = ms->ullAvailExtendedVirtual = 0;
    ms->dwMemoryLoad = ms->ullTotalPhys ? (DWORD)(100ULL - 100ULL * avail / ms->ullTotalPhys) : 0;
    return TRUE;
}

#endif // !_WIN32

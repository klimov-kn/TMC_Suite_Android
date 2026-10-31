// tmc_mfc.h — минимальный слой совместимости MFC для консольных ядер tmc_rth / tmc_rtx.
//
// Образец — src/compat/include/tmc_mfc.h версии для Linux (консольный порт ядер).
// Отличие: слой работает и на Windows (настольная сборка desktop, MinGW), где MFC
// нет, но есть настоящий <windows.h>. Поэтому:
//   * на _WIN32 базовые win-типы и функции (BOOL, DWORD, Sleep, GetFileAttributes,
//     SetFileAttributes, FILETIME, HWND …) берутся из системного <windows.h>;
//   * вне _WIN32 (Android, iOS — будущие этапы) — минимальные определения ниже.
//
// ВАЖНО:
//  * Это НЕ математика — только типы и строки. Формулы и числа не затрагиваются.
//  * CString — ТРИВИАЛЬНО КОПИРУЕМЫЙ тип с единственным членом char* (как у MFC,
//    где внутри один указатель на буфер). Исходники ядра передают CString в
//    printf/Format через "%s" (varargs): объект должен передаваться как указатель.
//    Буферы не освобождаются (тривиальная копируемость запрещает деструктор);
//    для режима «файл -> расчёт -> файл» утечка ограничена одним прогоном.

#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <type_traits>
#include <ctime>       // time(): в MSVC приходил транзитивно через заголовки MFC

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#else  // !_WIN32 — Android / iOS (будущие этапы)

typedef int            BOOL;
typedef char           TCHAR;
typedef const char*    LPCTSTR;
typedef char*          LPTSTR;
typedef const char*    LPCSTR;
typedef char*          LPSTR;
typedef unsigned int   UINT;
typedef void*          LPVOID;
// DWORD — как в typedef.h снимка (typedef unsigned long DWORD): определения
// должны совпадать, иначе «typedef redefinition». На LP64 (arm64) это 64 бита;
// в форматы файлов DWORD не пишется (проверено сверкой результатов, П1).
typedef unsigned long  DWORD;
typedef unsigned short WORD;
typedef unsigned char  BYTE;
typedef unsigned int   ULONG;
typedef int            LONG;
typedef void*          HANDLE;
typedef void*          HWND;
typedef unsigned long  WPARAM;
typedef long           LPARAM;
typedef long           LRESULT;
typedef unsigned int   COLORREF;

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define far
#define _far
#define near
#define _near
#define huge
#define _huge
#define _pascal
#define _cdecl

struct FILETIME { DWORD dwLowDateTime; DWORD dwHighDateTime; };

struct PROCESS_INFORMATION { HANDLE hProcess; HANDLE hThread; DWORD dwProcessId; DWORD dwThreadId; };

#include <thread>
#include <chrono>
inline void Sleep(unsigned long ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

#ifndef FILE_ATTRIBUTE_NORMAL
#define FILE_ATTRIBUTE_NORMAL 0x80
#endif
#ifndef FILE_ATTRIBUTE_ARCHIVE
#define FILE_ATTRIBUTE_ARCHIVE 0x20
#endif
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFu
#endif
// Атрибуты файла: синхронизация с внешним FieldView (В-15). Атрибуты не хранятся;
// GetFileAttributes разрешает путь, как tmc_fopen (обратная косая, регистр), —
// compat/tmc_textmode.cpp. «Файл есть» = FILE_ATTRIBUTE_ARCHIVE, как у нового файла в Windows.
inline BOOL SetFileAttributes(const char*, DWORD) { return TRUE; }
extern "C" unsigned long tmc_get_file_attributes(const char* path);
inline DWORD GetFileAttributes(const char* path)
{
    return (DWORD)tmc_get_file_attributes(path);
}
inline int _heapmin() { return 0; }

// Время, память, критическая секция, имя машины (TmcCalcTiming.h и др.).
#include "tmc_win_posix.h"

#endif // _WIN32

// fortran / _fortran — соглашение о вызовах DOS-эпохи; в <windows.h> его нет.
#ifndef fortran
#define fortran
#endif
#ifndef _fortran
#define _fortran
#endif

// --- AfxMessageBox: окна нет — сообщение в stderr --------------------------
inline int AfxMessageBox(const char* msg, unsigned int = 0, unsigned int = 0)
{
    std::fprintf(stderr, "%s\n", msg ? msg : "");
    return 1;  // IDOK
}

// --- CString (тривиально копируемый; внутри один char*) ---------------------
class CString
{
public:
    CString() : m_p(tmc_cstr_empty()) {}
    CString(const char* s) : m_p(tmc_cstr_dup(s ? s : "")) {}

    CString& operator=(const char* s) { m_p = tmc_cstr_dup(s ? s : ""); return *this; }

    void Format(const char* fmt, ...)
    {
#ifndef _WIN32
        // %I64d / %I64u / %I64x — расширение MSVC (TmcCalcTiming.h); bionic и
        // libc Darwin его не знают. Заменяем на стандартное %lld (то же значение).
        char fmtBuf[512];
        if (std::strstr(fmt, "%I64") && std::strlen(fmt) < sizeof(fmtBuf) / 2)
        {
            char* w = fmtBuf;
            for (const char* q = fmt; *q; )
            {
                if (q[0] == '%' && q[1] == 'I' && q[2] == '6' && q[3] == '4')
                { *w++ = '%'; *w++ = 'l'; *w++ = 'l'; q += 4; }
                else *w++ = *q++;
            }
            *w = '\0';
            fmt = fmtBuf;
        }
#endif
        va_list ap;  va_start(ap, fmt);
        va_list ap2; va_copy(ap2, ap);
        int n = std::vsnprintf(nullptr, 0, fmt, ap);
        va_end(ap);
        if (n < 0) { m_p = tmc_cstr_empty(); va_end(ap2); return; }
        char* buf = static_cast<char*>(std::malloc(static_cast<size_t>(n) + 1));
        std::vsnprintf(buf, static_cast<size_t>(n) + 1, fmt, ap2);
        va_end(ap2);
        m_p = buf;
    }

    int  GetLength() const { return static_cast<int>(std::strlen(m_p)); }
    BOOL IsEmpty()   const { return (m_p[0] == '\0') ? TRUE : FALSE; }
    void Empty()           { m_p = tmc_cstr_empty(); }
    char GetAt(int i) const { return m_p[i]; }
    char operator[](int i) const { return m_p[i]; }

    // GetBuffer/ReleaseBuffer — буфер нужного размера; запись в него на месте.
    char* GetBuffer(int nMin)
    {
        int n = GetLength();
        if (nMin < n) nMin = n;
        char* p = static_cast<char*>(std::malloc(static_cast<size_t>(nMin) + 1));
        std::memcpy(p, m_p, static_cast<size_t>(n) + 1);
        m_p = p;
        return m_p;
    }
    void ReleaseBuffer(int nNew = -1) { if (nNew >= 0) m_p[nNew] = '\0'; }

    int Find(char c, int start = 0) const
    {
        if (start < 0 || start > GetLength()) return -1;
        const char* r = std::strchr(m_p + start, c);
        return r ? static_cast<int>(r - m_p) : -1;
    }
    int Find(const char* s, int start = 0) const
    {
        if (start < 0 || start > GetLength()) return -1;
        const char* r = std::strstr(m_p + start, s ? s : "");
        return r ? static_cast<int>(r - m_p) : -1;
    }
    int FindOneOf(const char* set) const
    {
        size_t k = std::strcspn(m_p, set ? set : "");
        return (m_p[k] != '\0') ? static_cast<int>(k) : -1;
    }
    int Replace(char cOld, char cNew)
    {
        int n = 0;
        for (const char* q = m_p; *q; ++q) if (*q == cOld) ++n;
        if (n == 0) return 0;
        m_p = tmc_cstr_dup(m_p);
        for (char* q = m_p; *q; ++q) if (*q == cOld) *q = cNew;
        return n;
    }
    int Replace(const char* sOld, const char* sNew)
    {
        if (!sOld || !*sOld) return 0;
        if (!sNew) sNew = "";
        size_t lo = std::strlen(sOld), ln = std::strlen(sNew);
        int n = 0;
        for (const char* q = std::strstr(m_p, sOld); q; q = std::strstr(q + lo, sOld)) ++n;
        if (n == 0) return 0;
        size_t len = std::strlen(m_p) + static_cast<size_t>(n) * ln - static_cast<size_t>(n) * lo;
        char* r = static_cast<char*>(std::malloc(len + 1));
        char* w = r;
        const char* q = m_p;
        for (const char* f = std::strstr(q, sOld); f; f = std::strstr(q, sOld))
        {
            std::memcpy(w, q, static_cast<size_t>(f - q)); w += f - q;
            std::memcpy(w, sNew, ln); w += ln;
            q = f + lo;
        }
        std::strcpy(w, q);
        m_p = r;
        return n;
    }
    int ReverseFind(char c) const
    {
        const char* r = std::strrchr(m_p, c);
        return r ? static_cast<int>(r - m_p) : -1;
    }
    CString Left(int n) const
    {
        int len = GetLength();
        if (n < 0) n = 0;
        if (n > len) n = len;
        return Sub(0, n);
    }
    CString Right(int n) const
    {
        int len = GetLength();
        if (n < 0) n = 0;
        if (n > len) n = len;
        return Sub(len - n, n);
    }
    CString Mid(int first) const { return Mid(first, GetLength() - first); }
    CString Mid(int first, int n) const
    {
        int len = GetLength();
        if (first < 0) first = 0;
        if (first > len) first = len;
        if (n < 0) n = 0;
        if (first + n > len) n = len - first;
        return Sub(first, n);
    }

    const char* GetString() const { return m_p; }
    operator const char*()  const { return m_p; }

    CString& operator+=(const char* s)    { m_p = tmc_cstr_cat(m_p, s ? s : ""); return *this; }
    CString& operator+=(const CString& o) { m_p = tmc_cstr_cat(m_p, o.m_p);      return *this; }
    CString& operator+=(char c)           { char b[2] = { c, 0 }; m_p = tmc_cstr_cat(m_p, b); return *this; }

    friend CString operator+(const CString& a, const char* b)    { CString r(a); r += b; return r; }
    friend CString operator+(const char* a, const CString& b)    { CString r(a); r += b; return r; }
    friend CString operator+(const CString& a, const CString& b) { CString r(a); r += b; return r; }
    friend CString operator+(const CString& a, char b)           { CString r(a); r += b; return r; }

    bool operator==(const CString& o) const { return std::strcmp(m_p, o.m_p) == 0; }
    bool operator!=(const CString& o) const { return std::strcmp(m_p, o.m_p) != 0; }
    bool operator==(const char* s) const { return std::strcmp(m_p, s ? s : "") == 0; }
    bool operator!=(const char* s) const { return std::strcmp(m_p, s ? s : "") != 0; }

private:
    CString Sub(int first, int n) const
    {
        char* p = static_cast<char*>(std::malloc(static_cast<size_t>(n) + 1));
        std::memcpy(p, m_p + first, static_cast<size_t>(n));
        p[n] = '\0';
        CString r; r.m_p = p; return r;
    }
    static char* tmc_cstr_empty()
    {
        static char e[1] = { '\0' };
        return e;
    }
    static char* tmc_cstr_dup(const char* s)
    {
        size_t n = std::strlen(s) + 1;
        char* p = static_cast<char*>(std::malloc(n));
        std::memcpy(p, s, n);
        return p;
    }
    static char* tmc_cstr_cat(const char* a, const char* b)
    {
        size_t na = std::strlen(a), nb = std::strlen(b);
        char* p = static_cast<char*>(std::malloc(na + nb + 1));
        std::memcpy(p, a, na);
        std::memcpy(p + na, b, nb + 1);
        return p;
    }

    char* m_p;   // единственный член: объект ABI-эквивалентен указателю
};

static_assert(std::is_trivially_copyable<CString>::value,
              "CString must be trivially copyable (varargs %s relies on it)");
static_assert(sizeof(CString) == sizeof(char*), "CString must be one pointer");

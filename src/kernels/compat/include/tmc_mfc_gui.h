// tmc_mfc_gui.h — «болванки» GUI-классов MFC для консольных ядер tmc_rth / tmc_rtx.
//
// Образец — src/compat/include/tmc_mfc_gui.h версии для Linux.
// Счётные модули ядра (TmcRTHRectNode.cpp, TmcRTHNodeDiel.cpp) транзитивно включают
// GUI-заголовки (PlanRT_H.h -> CWinApp; Pl_iofor.h -> MainFrm.h/PlanRT_HDoc.h/
// PlanRT_HView.h). Сами GUI-файлы .cpp в сборку не входят, но их заголовки должны
// разбираться компилятором. Здесь — пустые классы и нейтральные макросы.
// Ни одно окно не создаётся: точка входа — консольный main (src/kernels/cli).
//
// Это НЕ математика и НЕ формат файлов.

#pragma once

#include "tmc_mfc.h"
#include <climits>
#include <new>

#ifndef __AFXWIN_H__
#define __AFXWIN_H__
#endif

#ifndef WM_CLOSE
#define WM_CLOSE 0x0010
#endif

#ifndef _WIN32
struct CREATESTRUCT { int unused; };
typedef CREATESTRUCT* LPCREATESTRUCT;
#endif

// --- Макросы ClassWizard: карты сообщений и динамическое создание не нужны ----
#define afx_msg
#define DECLARE_MESSAGE_MAP()
#define BEGIN_MESSAGE_MAP(theClass, baseClass)
#define END_MESSAGE_MAP()
#define DECLARE_DYNAMIC(class_name)
#define DECLARE_DYNCREATE(class_name)
#define DECLARE_SERIAL(class_name)
#define IMPLEMENT_DYNAMIC(class_name, base_class_name)
#define IMPLEMENT_DYNCREATE(class_name, base_class_name)
#define IMPLEMENT_SERIAL(class_name, base_class_name, wSchema)
#ifndef TRACE
#define TRACE(...) ((void)0)
#endif
#ifndef ASSERT
#define ASSERT(f) ((void)0)
#endif
#ifndef ASSERT_VALID
#define ASSERT_VALID(p) ((void)0)
#endif
#ifndef VERIFY
#define VERIFY(f) ((void)(f))
#endif

class CDC;
class CPrintInfo;
class CArchive;
class CDumpContext;
class CDataExchange;
class CMenu;
class CFont;
class CBitmap;
class CRect;
class CPoint;
class CSize;

class CObject
{
public:
    CObject() {}
    virtual ~CObject() {}
};

class CException : public CObject
{
public:
    virtual BOOL GetErrorMessage(char* buf, UINT n, UINT* = nullptr)
    { if (buf && n) buf[0] = '\0'; return FALSE; }
    void Delete() {}
};

class CCmdTarget : public CObject {};

class CCmdUI
{
public:
    void Enable(BOOL = TRUE) {}
    void SetCheck(int = 1) {}
    void SetText(const char*) {}
};

class CWnd : public CCmdTarget
{
public:
    CWnd() {}
    virtual ~CWnd() {}
    HWND GetSafeHwnd() const { return nullptr; }
    BOOL PostMessage(UINT, WPARAM = 0, LPARAM = 0) { return TRUE; }
    BOOL RedrawWindow() { return TRUE; }
};

class CControlBar : public CWnd {};
class CStatusBar  : public CControlBar {};
class CToolBar    : public CControlBar {};
class CReBar      : public CControlBar {};
class CDialogBar  : public CControlBar {};

class CFrameWnd    : public CWnd {};
class CMDIFrameWnd : public CFrameWnd {};
class CMDIChildWnd : public CFrameWnd {};

class CDocument : public CCmdTarget {};

class CView : public CWnd
{
public:
    CDocument* m_pDocument = nullptr;
};
class CScrollView : public CView {};

class CDialog        : public CWnd {};
class CPropertyPage  : public CDialog {};
class CPropertySheet : public CWnd {};

class CEvent
{
public:
    BOOL SetEvent() { return TRUE; }
    BOOL ResetEvent() { return TRUE; }
};

class CWinThread : public CCmdTarget {};
class CWinApp : public CWinThread
{
public:
    CWnd* m_pMainWnd = nullptr;
    virtual BOOL InitInstance() { return TRUE; }
    virtual int ExitInstance() { return 0; }
};

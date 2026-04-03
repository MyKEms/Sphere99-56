#ifndef _INC_CWINDOW_H
#define _INC_CWINDOW_H

#ifdef _WIN32
#include "stdafx.h"
#include "cstring.h"
#include <RICHEDIT.H>	// CRichEditCtrl

class CWindow    // similar to Std MFC class CWnd
{
public:
	HWND m_hWnd;

public:
	operator HWND () const       // cast as a HWND
	{
		return(m_hWnd);
	}
	CWindow()
	{
		m_hWnd = NULL;
	}
	~CWindow()
	{
		DestroyWindow();
	}

public:
	BOOL SetWindowText(LPCSTR lpszText)
	{
		ASSERT(m_hWnd);
		return ::SetWindowText(m_hWnd, lpszText);
	}
	int GetWindowText(LPSTR lpszText, int iLen)
	{
		ASSERT(m_hWnd);
		return ::GetWindowText(m_hWnd, lpszText, iLen);
	}

	BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct = NULL)
	{
		m_hWnd = hwnd;
		return(TRUE);
	}
	void OnDestroy()
	{
		m_hWnd = NULL;
	}
	void OnDestroy(HWND hwnd)
	{
		m_hWnd = NULL;
	}
	BOOL IsWindow(void) const
	{
		if (this == NULL)
			return(false);
		if (m_hWnd == NULL)
			return(false);
		return(::IsWindow(m_hWnd));
	}
	LRESULT SendMessage(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0) const
	{
		ASSERT(m_hWnd);
		return(::SendMessage(m_hWnd, uMsg, wParam, lParam));
	}
	BOOL PostMessage(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0) const
	{
		ASSERT(m_hWnd);
		return(::PostMessage(m_hWnd, uMsg, wParam, lParam));
	}
	BOOL ShowWindow(int nCmdShow)
	{
		// SW_SHOW
		return(::ShowWindow(m_hWnd, nCmdShow));
	}
	HWND GetDlgItem(int id) const
	{
		ASSERT(m_hWnd);
		return(::GetDlgItem(m_hWnd, id));
	}
	BOOL SetDlgItemText(int nIDDlgItem, LPCSTR lpString)
	{
		ASSERT(m_hWnd);
		return(::SetDlgItemText(m_hWnd, nIDDlgItem, lpString));
	}
	void DestroyWindow(void)
	{
		if (m_hWnd == NULL)
			return;
		::DestroyWindow(m_hWnd);
		ASSERT(m_hWnd == NULL);
	}
	void GetClientRect(LPRECT pRect) const
	{
		ASSERT(m_hWnd);
		::GetClientRect(m_hWnd, pRect);
	}
	BOOL MoveWindow(int X, int Y, int nWidth, int nHeight, BOOL bRepaint = TRUE)
	{
		return(::MoveWindow(m_hWnd, X, Y, nWidth, nHeight, bRepaint));
	}
	BOOL SetForegroundWindow()
	{
		ASSERT(m_hWnd);
		return(::SetForegroundWindow(m_hWnd));
	}
	HWND SetFocus()
	{
		ASSERT(m_hWnd);
		return(::SetFocus(m_hWnd));
	}
	HFONT GetFont() const
	{
		return((HFONT)SendMessage(WM_GETFONT));
	}
	void SetFont(HFONT hFont, BOOL fRedraw = false)
	{
		SendMessage(WM_SETFONT, (WPARAM)hFont, MAKELPARAM(fRedraw, 0));
	}
	UINT SetTimer(UINT uTimerID, UINT uWaitmSec)
	{
		ASSERT(m_hWnd);
		return(::SetTimer(m_hWnd, uTimerID, uWaitmSec, NULL));
	}
	BOOL KillTimer(UINT uTimerID)
	{
		ASSERT(m_hWnd);
		return(::KillTimer(m_hWnd, uTimerID));
	}
	int MessageBox(LPCSTR lpszText, LPCSTR lpszTitle, UINT fuStyle = MB_OK) const
	{
		// ASSERT( m_hWnd ); ok for this to be NULL !
		return(::MessageBox(m_hWnd, lpszText, lpszTitle, fuStyle));
	}
};

class CDialogBase : public CWindow
{
public:
	static BOOL CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	virtual BOOL DefDialogProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
		return FALSE;
	}
};

class CWindowBase : public CWindow
{
public:
	static ATOM RegisterClass(WNDCLASS* wc) { throw "not implemented"; }
	virtual LRESULT DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
	{
		return ::DefWindowProc(m_hWnd, message, wParam, lParam);
	}
};

class CTabCtrl : public CWindow
{

};

class CListbox : public CWindow
{
public:
	void ResetContent()
	{
		ASSERT(IsWindow());
		SendMessage(LB_RESETCONTENT);
	}
	int GetCount() const
	{
		return((int)(DWORD)SendMessage(LB_GETCOUNT));
	}
	int AddString(LPCTSTR lpsz) const
	{
		return((int)(DWORD)SendMessage(LB_ADDSTRING, 0L, (LPARAM)(lpsz)));
	}
};

class CComboBox : public CWindow
{
};

class CEdit : public CWindow
{
public:
	void SetSel(DWORD dwSelection, BOOL bNoScroll = FALSE)
	{
		ASSERT(IsWindow());
		SendMessage(EM_SETSEL, (WPARAM)dwSelection, (LPARAM)dwSelection);
	}
	DWORD GetSel() const
	{
		ASSERT(IsWindow());
		return((DWORD)SendMessage(EM_GETSEL));
	}
	void GetSel(int& nStartChar, int& nEndChar) const
	{
		ASSERT(IsWindow());
		DWORD dwSel = GetSel();
		nStartChar = LOWORD(dwSel);
		nEndChar = HIWORD(dwSel);
	}
	int GetSelText(LPCTSTR szLine);
	void ReplaceSel(LPCTSTR lpszNewText, BOOL bCanUndo = FALSE)
	{
		ASSERT(IsWindow());
		SendMessage(EM_REPLACESEL, (WPARAM)bCanUndo, (LPARAM)lpszNewText);
	}
	int GetFirstVisibleLine() const
	{
		return((int)(DWORD)SendMessage(EM_GETFIRSTVISIBLELINE));
	}
	int LineIndex(int line)
	{
		return (int)(DWORD)SendMessage(EM_LINEINDEX, (WPARAM)(int)(line), 0L);
	}
	int LineLength(int line)
	{
		return ((int)(DWORD)SendMessage(EM_LINELENGTH, (WPARAM)(int)(line), 0L));
	}

};

class CRichEditCtrl : public CEdit
{
public:
	COLORREF SetBackgroundColor(BOOL bSysColor, COLORREF cr)
	{
		return((COLORREF)(DWORD)SendMessage(EM_SETBKGNDCOLOR, (WPARAM)bSysColor, (LPARAM)cr));
	}
	void SetAutoUrlDetect(BOOL fActive)
	{
		SendMessage(EM_AUTOURLDETECT, (WPARAM)fActive);
	}

	// Formatting.
	BOOL SetDefaultCharFormat(CHARFORMAT& cf)
	{
		return((BOOL)(DWORD)SendMessage(EM_SETCHARFORMAT, (WPARAM)SCF_DEFAULT, (LPARAM)&cf));
	}
	BOOL SetSelectionCharFormat(CHARFORMAT& cf)
	{
		return((BOOL)(DWORD)SendMessage(EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&cf));
	}
	DWORD SetEventMask(DWORD dwEventMask = ENM_NONE)
	{
		// ENM_NONE = default.
		return((DWORD)SendMessage(EM_SETEVENTMASK, 0, (LPARAM)dwEventMask));
	}
	DWORD Scroll(int iAction = SB_PAGEDOWN)
	{
		return((DWORD)SendMessage(EM_SCROLL, (WPARAM)iAction));
	}
};

class CWinApp	// Similar to MFC type
{
public:
	LPCSTR	 	m_pszAppName;	// Specifies the name of the application. (display freindly)
	HINSTANCE 	m_hInstance;	// Identifies the current instance of the application.

public:
	void InitInstance(LPCTSTR pszAppName, HINSTANCE hInstance, LPSTR lpszCmdLine) { throw "not implemented"; }
	HICON LoadIcon(int id) const
	{
		return(::LoadIcon(m_hInstance, MAKEINTRESOURCE(id)));
	}
	HMENU LoadMenu(int id) const
	{
		return(::LoadMenu(m_hInstance, MAKEINTRESOURCE(id)));
	}
};

class CCommandLog
{
public:
	void Add(LPCTSTR pszCmd) { throw "not implemented"; }
	LPCTSTR ScrollCmd(DWORD dwParam) { throw "not implemented"; }
};

#endif // _WIN32

#endif // _INC_CWINDOW_H

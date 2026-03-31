//
// CServConsoleW.CPP
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
// Put up a window for data (other than the console)
//

#include "stdafx.h"	// predef header.

#if defined(_WIN32) && ! defined(_MFC_VER) && ! defined(_CONSOLE)

#include <windowsx.h>
#include <commctrl.h>	// NM_RCLICK
#include "resource.h"
#include "cwindow.h"

#define WM_USER_POST_MSG		(WM_USER+10)
#define WM_USER_TRAY_NOTIFY		(WM_USER+12)

#define IDC_M_TABCTL	10
#define IDC_M_LOG		11
#define IDC_M_CLIENTS	12
#define IDC_M_STATS		13
#define IDC_M_INPUT		14

#define IDT_WAKEUP		100
#define IDT_UPDATE		200	// Periodically update the display ?

class CDlgAbout : public CDialogBase
{
private:
	bool OnInitDialog();
	bool OnCommand( WORD wNotifyCode, int wID, HWND hwndCtl );
public:
	virtual BOOL DefDialogProc( UINT message, WPARAM wParam, LPARAM lParam );
};

class CDlgOptions : public CDialogBase
{
private:
	bool OnInitDialog();
	bool OnCommand( WORD wNotifyCode, int wID, HWND hwndCtl );
public:
	virtual BOOL DefDialogProc( UINT message, WPARAM wParam, LPARAM lParam );
};

class CListTextConsole : public CScriptConsole
{
	// Target the response from a script to a listbox.
	CListbox m_wndList;
public:
	virtual int GetPrivLevel() const
	{
		return PLEVEL_Admin;
	}
	virtual CString GetName() const
	{
		return( _TEXT("Stats"));
	}
	virtual bool WriteString( LPCTSTR pszMessage )
	{
		if ( pszMessage == NULL || ISINTRESOURCE(pszMessage))
			return false;
		// Strip the new lines off !
		int iLen = Str_GetEndWhitespace( pszMessage, strlen(pszMessage));
		char ch = pszMessage[iLen];
		if ( ch )
		{
			const_cast<char*>(pszMessage)[iLen] = '\0';
		}
		m_wndList.AddString( pszMessage );
		if ( ch )
		{
			const_cast<char*>(pszMessage)[iLen] = ch;
		}
		return( true );
	}
	CListTextConsole( HWND hWndList )
	{
		m_wndList.m_hWnd = hWndList;
	}
	~CListTextConsole()
	{
		m_wndList.OnDestroy();
	}
};

class CWndMain : public CWindowBase
{
	// Create a dialog to do stuff.
public:
	COLORREF m_dwColorNew;	// set the color for the next block written.
	COLORREF m_dwColorPrv;

	int m_iTabActive;	// 0-3 Which tab is active ?
	CTabCtrl m_wndTabCtrl;

	CRichEditCtrl m_wndLog;		// IDC_M_LOG
	int m_iLogTextLen;

	CListbox m_wndListClients;	// CListCtrl?
	CListbox m_wndListStats;	// CListCtrl?

	CEdit m_wndInput;		// the text input portion at the bottom.
	int m_iInputFontHeight;	// Used for the height of the edit input window. not display.
	//HFONT m_hLogFontFixed;
	HFONT m_hLogFont;
	int m_iLogFontHeight;	// Used for the height of the edit input window. not display.

	bool m_fLogScrollLock;	// lock with the rolling text ?

	CCommandLog m_CommandLog;

private:
	int OnCreate( LPCREATESTRUCT lParam );
	void OnShowWindow( bool fShown );
	bool OnSysCommand( UINT uCmdType, int xPos, int yPos );
	void OnSize( UINT nType, int cx, int cy );
	void OnDestroy();
	void OnSetFocus( HWND hWndLoss );
	bool OnClose();
	void OnUserPostMessage( COLORREF color, char* pszMsg );
	LRESULT OnUserTrayNotify( WPARAM wID, LPARAM lEvent );
	LRESULT OnNotify( int idCtrl, NMHDR* pnmh );
	bool OnOK();
	void OnTimer( int idTimer );

public:
	bool OnCommand( WORD wNotifyCode, int wID, HWND hwndCtl );

	static bool RegisterClass();
	LRESULT DefWindowProc( UINT message, WPARAM wParam, LPARAM lParam );

	void List_Clear();
	void List_Add( COLORREF color, LPCTSTR pszText );
	bool AttachToShell();

	void FillStatusClients();
	void FillStatusStats();
	void UpdateActiveTab();

	CWndMain();
	virtual ~CWndMain();
};

class CWinAppMain : public CWinApp
{
public:
	CWndMain m_wndMain;	// m_pMainWnd
	CDlgOptions m_dlgOptions;
};

CWinAppMain theApp;

//************************************
// -CDlgAbout

bool CDlgAbout::OnInitDialog()
{
	SetDlgItemText( IDC_ABOUT_VERSION, SPHERE_TITLE " Version " SPHERE_VERSION );
	return( false );
}

bool CDlgAbout::OnCommand( WORD wNotifyCode, int wID, HWND hwndCtl )
{
	// WM_COMMAND
	switch ( wID )
	{
	case IDOK:
	case IDCANCEL:
		EndDialog( m_hWnd, wID );
		break;
	}
	return( true );
}

BOOL CDlgAbout::DefDialogProc( UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message )
	{
	case WM_INITDIALOG:
		return( OnInitDialog());
	case WM_COMMAND:
		return( OnCommand(  HIWORD(wParam), LOWORD(wParam), (HWND) lParam ));
	case WM_DESTROY:
		OnDestroy();
		return( true );
	}
	return( false );
}

//************************************
// -CDlgOptions

bool CDlgOptions::OnInitDialog()
{
	return( false );
}

bool CDlgOptions::OnCommand( WORD wNotifyCode, int wID, HWND hwndCtl)
{
	// WM_COMMAND
	switch ( wID )
	{
	case IDOK:
	case IDCANCEL:
		DestroyWindow();
		break;
	}
	return( false );
}

BOOL CDlgOptions::DefDialogProc( UINT message, WPARAM wParam, LPARAM lParam )
{
	// IDM_OPTIONS
	switch ( message )
	{
	case WM_INITDIALOG:
		return( OnInitDialog());
	case WM_COMMAND:
		return( OnCommand(  HIWORD(wParam), LOWORD(wParam), (HWND) lParam ));
	case WM_DESTROY:
		OnDestroy();
		return( true );
	}
	return( false );
}

//************************************
// -CWndMain

CWndMain::CWndMain()
{
	m_iLogTextLen = 0;
	m_iInputFontHeight = 0;
	m_iLogFontHeight = 0;
	//m_hLogFontFixed = NULL;
	m_hLogFont = NULL;
	m_fLogScrollLock = false;
	m_dwColorNew = RGB( 0xaf,0xaf,0xaf );
	m_dwColorPrv = RGB( 0xaf,0xaf,0xaf );
}

CWndMain::~CWndMain()
{
	DestroyWindow();
}

void CWndMain::List_Clear()
{
	m_wndLog.SetWindowText( "");
	m_wndLog.SetSel( 0, 0 );
	m_iLogTextLen = 0;
}

void CWndMain::List_Add( COLORREF color, LPCTSTR pszText )
{
	int iTextLen = strlen( pszText );
	int iNewLen = m_iLogTextLen + iTextLen;

#define LOG_MAX_LEN  (256*1024)
	if ( iNewLen > LOG_MAX_LEN )
	{
		int iCut = iNewLen - LOG_MAX_LEN;
		m_wndLog.SetSel( 0, iCut );
		m_wndLog.ReplaceSel( "" );
		m_iLogTextLen = LOG_MAX_LEN;
	}

	m_wndLog.SetSel( m_iLogTextLen, m_iLogTextLen );

	// set the blocks color.
	CHARFORMAT cf;
	memset( &cf, 0, sizeof(cf));
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_COLOR;
	cf.crTextColor = color;
	BOOL fRet = m_wndLog.SetSelectionCharFormat( cf );

	m_wndLog.ReplaceSel( pszText );

	m_iLogTextLen += iTextLen;
	m_wndLog.SetSel( m_iLogTextLen, m_iLogTextLen );

	int iSelBegin;
	int iSelEnd;
	m_wndLog.GetSel( iSelBegin, iSelEnd );
	// ASSERT( iSelEnd == iSelBegin );
	// ASSERT( m_iLogTextLen == iSelBegin );
	m_iLogTextLen = iSelBegin;	// make sure it's correct.

	// If the select is on screen then keep scrolling.
	if ( ! m_fLogScrollLock && ! GetCapture())
	{
		if ( CGSystemInfo::IsNt())
		{
			m_wndLog.Scroll();
		}
	}
}

void CWndMain::FillStatusClients()
{
	// update this when number of clients changes.
	if ( m_wndListClients.m_hWnd == NULL )
		return;
	m_wndListClients.ResetContent();
	CListTextConsole capture( m_wndListClients.m_hWnd );
	g_Serv.ListClients( &capture );
	int iCount = m_wndListClients.GetCount();
	iCount++;
}

void CWndMain::FillStatusStats()
{
	// update this periodically?
	if ( m_wndListStats.m_hWnd == NULL )
		return;
	m_wndListStats.ResetContent();

	CListTextConsole capture( m_wndListStats.m_hWnd );
	for ( int i=0; i < PROFILE_QTY; i++ )
	{
		capture.Printf( "'%s' = %s\n", 
			g_ProfileProps[i].m_pszName, 
			g_Serv.m_Profile.GetTaskStatusDesc( i ));
	}

	for ( int i=0; i<SERV_STAT_QTY; i++ )
	{
		capture.Printf( "'%s' = %d\n", 
			g_Serv.CServerDef::sm_Props[ CServerDef::P_StatAccounts + i].m_pszName,
			g_Serv.StatGet( (SERV_STAT_TYPE) i ));
	}
}

bool CWndMain::AttachToShell()
{
	NOTIFYICONDATA pnid;
	memset(&pnid,0,sizeof(pnid));
	pnid.cbSize = sizeof(NOTIFYICONDATA);
	pnid.hWnd   = m_hWnd;
	pnid.uFlags = NIF_TIP | NIF_ICON | NIF_MESSAGE;
	pnid.uCallbackMessage = WM_USER_TRAY_NOTIFY;
	pnid.hIcon  = theApp.LoadIcon( IDR_MAINFRAME );
	strcpy( pnid.szTip, theApp.m_pszAppName );
	return Shell_NotifyIcon(NIM_ADD, &pnid);
}

bool CWndMain::RegisterClass()	// static
{
	WNDCLASS wc;
	memset( &wc, 0, sizeof(wc));

	wc.style = CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW;
	wc.hInstance = theApp.m_hInstance;
	wc.hIcon = theApp.LoadIcon( IDR_MAINFRAME );
	wc.hCursor = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH) NULL;
	wc.lpszMenuName = NULL; // MAKEINTRESOURCE( 1 );
	wc.lpszClassName = SPHERE_TITLE "Svr";

	ATOM frc = CWindowBase::RegisterClass( &wc );
	if ( !frc )
	{
		return( false );
	}

	InitCommonControls();

	HMODULE hMod = LoadLibrary("Riched20.dll"); // Load the RichEdit DLL to activate the class
	DEBUG_CHECK(hMod);
	return( true );
}

void CWndMain::UpdateActiveTab()
{
	OnTimer(IDT_UPDATE);
	RECT rect;
	GetClientRect(&rect);
	OnSize(SIZE_RESTORED,rect.right,rect.bottom);
}

//****************************

int CWndMain::OnCreate( LPCREATESTRUCT lParam )
{
	// Create the tab control to be the parent.

	m_iTabActive = 0;
	m_wndTabCtrl.m_hWnd = ::CreateWindow( WC_TABCONTROL, NULL,
		WS_CHILD|WS_VISIBLE,
		0, 0, 10, 10,
		m_hWnd,
		(HMENU)(UINT) IDC_M_TABCTL, theApp.m_hInstance,
		NULL );
	ASSERT( m_wndTabCtrl.m_hWnd );

	m_wndListClients.m_hWnd = ::CreateWindow( "LISTBOX", NULL,
		WS_CHILD|WS_VISIBLE|WS_VSCROLL,
		0, 0, 10, 10,
		m_hWnd,
		(HMENU)(UINT) IDC_M_CLIENTS, theApp.m_hInstance,
		NULL );
	m_wndListStats.m_hWnd = ::CreateWindow( "LISTBOX", NULL,
		WS_CHILD|WS_VISIBLE|WS_VSCROLL,
		0, 0, 10, 10,
		m_hWnd,
		(HMENU)(UINT) IDC_M_STATS, theApp.m_hInstance,
		NULL );
	m_wndLog.m_hWnd = ::CreateWindow( RICHEDIT_CLASS, NULL,
		WS_CHILD|WS_VISIBLE|WS_VSCROLL |
		ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY, // | ES_OEMCONVERT
		0, 0, 10, 10,
		m_hWnd,
		(HMENU)(UINT) IDC_M_LOG, theApp.m_hInstance,
		NULL );
	ASSERT( m_wndLog.m_hWnd );
	m_wndInput.m_hWnd = ::CreateWindow( _TEXT( "EDIT" ), NULL,
		ES_LEFT | ES_AUTOHSCROLL |	// | ES_OEMCONVERT
		WS_CHILD|WS_VISIBLE|WS_BORDER|WS_TABSTOP,
		0, 0, 10, 10,
		m_hWnd,
		(HMENU)(UINT) IDC_M_INPUT,
		theApp.m_hInstance, 
		NULL );
	ASSERT( m_wndInput.m_hWnd );

	// Set the font to a proportional font (Courier)

	int iRet;
	LOGFONT logfont;
	memset( &logfont, 0, sizeof(logfont));

#if 0
	iRet = ::GetObject( (HFONT) GetStockObject(SYSTEM_FONT), sizeof(logfont),&logfont );
	ASSERT(iRet==sizeof(logfont));
	strcpy( logfont.lfFaceName, "Courier" );

	m_hLogFont = CreateFontIndirect(&logfont);
#else
	m_hLogFont = (HFONT) GetStockObject(OEM_FIXED_FONT);
#endif

	ASSERT(m_hLogFont);

	iRet = ::GetObject(m_hLogFont, sizeof(logfont),&logfont );
	ASSERT(iRet==sizeof(logfont));
	m_iLogFontHeight = ABS( logfont.lfHeight );
	ASSERT(m_iLogFontHeight);

	// m_wndInput.SetFont( m_hLogFont );
	m_wndLog.SetFont( m_hLogFont );
	m_wndListStats.SetFont( m_hLogFont );
	m_wndListClients.SetFont( m_hLogFont );

	// TEXTMODE
	// m_wndLog.SetTextMode( TM_RICHTEXT );	// |TM_MULTICODEPAGE..
	m_wndLog.SetBackgroundColor( false, RGB(0,0,0));
	CHARFORMAT cf;
	memset( &cf, 0, sizeof(cf));
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_COLOR;
	cf.crTextColor = m_dwColorPrv;
	m_wndLog.SetDefaultCharFormat( cf );
	m_wndLog.SetAutoUrlDetect( true );
	m_wndLog.SetEventMask( ENM_LINK | ENM_MOUSEEVENTS | ENM_KEYEVENTS );

	//*******************

	TCITEM tie;
    tie.mask = TCIF_TEXT | TCIF_IMAGE; 
    tie.iImage = -1; 

static const char* pszTabs[] = 
{
	"Log",			// IDC_M_LOG
	"Clients",		// IDC_M_CLIENTS
	"Stats",		// IDC_M_STATS
};

    for ( int i = 0; i < COUNTOF(pszTabs); i++) 
	{ 
	    tie.pszText = (char*) pszTabs[i]; 
        if ( TabCtrl_InsertItem(m_wndTabCtrl, i, &tie) == -1)
		{ 
            return -1; 
        } 
    } 

	AttachToShell();

	g_ServConsole.OnTriggerEvent( SERVTRIG_ModeChange, 0, 0);

	// ??? Add About to the sys menu ?

	return( 0 );
}

void CWndMain::OnDestroy()
{
	NOTIFYICONDATA pnid;
	memset(&pnid,0,sizeof(pnid));
	pnid.cbSize = sizeof(NOTIFYICONDATA);
	pnid.hWnd   = m_hWnd;
	Shell_NotifyIcon(NIM_DELETE, &pnid);

	m_wndLog.OnDestroy();	// these are automatic.
	m_wndInput.OnDestroy();
	m_wndListClients.OnDestroy();
	m_wndListStats.OnDestroy();
	m_wndTabCtrl.OnDestroy();

	CWindow::OnDestroy();
}

void CWndMain::OnSetFocus( HWND hWndLoss )
{
	m_wndInput.SetFocus();
}

LRESULT CWndMain::OnUserTrayNotify( WPARAM wID, LPARAM lEvent )
{
	// WM_USER_TRAY_NOTIFY
	switch ( lEvent )
	{
	case WM_MOUSEMOVE:
		return 0;
	case WM_RBUTTONDOWN:
		// Context menu ?
		{
			HMENU hMenu = theApp.LoadMenu( IDM_POP_TRAY );
			if ( hMenu == NULL )
				break;
			HMENU hMenuPop = GetSubMenu(hMenu,0);
			if ( hMenuPop )
			{
				POINT point;
				if ( GetCursorPos( &point ))
				{
					TrackPopupMenu( hMenuPop, TPM_RIGHTBUTTON, point.x, point.y, 0, m_hWnd, NULL );
				}
			}
			DestroyMenu( hMenu );
		}
		return 1;
	case WM_LBUTTONDBLCLK:
		ShowWindow(SW_NORMAL);
		SetForegroundWindow();
		return 1; // handled
	}
	return 0;	// not handled.
}

void CWndMain::OnUserPostMessage( COLORREF color, char* pszMsg )
{
	// WM_USER_POST_MSG
	ASSERT(pszMsg);
	List_Add( color, pszMsg );
	free(pszMsg);
}

void CWndMain::OnSize( UINT nType, int cx, int cy )
{
	if ( nType != SIZE_MINIMIZED && nType != SIZE_MAXHIDE && m_wndTabCtrl.m_hWnd )
	{
		// Calculate the display rectangle, assuming the 
        // tab control is the size of the client area. 

		if ( m_iInputFontHeight == 0 )
		{
			HFONT hFont = m_wndInput.GetFont();
			if ( hFont == NULL )
			{
				hFont = (HFONT) GetStockObject(SYSTEM_FONT);
			}
			LOGFONT logfont;
			int iRet = ::GetObject(hFont, sizeof(logfont),&logfont );
			ASSERT(iRet==sizeof(logfont));
			m_iInputFontHeight = ABS( logfont.lfHeight );
			ASSERT(m_iInputFontHeight);
		}

		cy -= m_iInputFontHeight;

		RECT rc; 
		SetRect(&rc, 0, 0, cx, cy ); 
		TabCtrl_AdjustRect( m_wndTabCtrl, false, &rc); 

		// Size the tab control to fit the client area. 
        HDWP hdwp = BeginDeferWindowPos(4); 
        DeferWindowPos( hdwp, m_wndTabCtrl,
			HWND_BOTTOM, 0, 0, 
			cx, cy, SWP_NOMOVE ); 

		// Position and size the static control to fit the 
        // tab control's display area, and make sure the 
        // static control is in front of the tab control. 
		for ( int i=0; i<3; i++ )
		{
	        DeferWindowPos(hdwp, GetDlgItem(i + IDC_M_LOG),
				HWND_TOP, rc.left, rc.top, 
			    rc.right - rc.left, rc.bottom - rc.top, 
				(i==m_iTabActive) ? SWP_SHOWWINDOW : SWP_HIDEWINDOW ); 
		}

		EndDeferWindowPos(hdwp); 

		m_wndInput.MoveWindow( 0, cy, cx, m_iInputFontHeight, true );
	}
}

bool CWndMain::OnClose()
{
	// WM_CLOSE
	ASSERT( CThread::GetCurrentThreadId() == g_Serv.m_dwParentThread );

	if ( g_Serv.m_iExitFlag == 0 )
	{
		int iRet = theApp.m_wndMain.MessageBox( _TEXT("Are you sure you want to close the server?"),
			theApp.m_pszAppName, MB_YESNO|MB_ICONQUESTION );
		if ( iRet == IDNO )
			return( false );
	}

	PostQuitMessage(0);
	g_Serv.SetExitFlag( SPHEREERR_WINDOW_CLOSE );
	return( true );	// ok to close.
}

bool CWndMain::OnOK()
{
	if ( g_ServConsole.IsCommandReady())
		return false;

	TCHAR szTmp[ MAX_TALK_BUFFER ];
	m_wndInput.GetWindowText( szTmp, sizeof(szTmp));
	m_wndInput.SetWindowText("");

	// Switch to the console view.
	if ( m_iTabActive != 0 )
	{
		m_iTabActive = 0;
		TabCtrl_SetCurSel(m_wndTabCtrl,0);
		UpdateActiveTab();
	}

	// Store to the command log.
	m_CommandLog.Add(szTmp);
	g_ServConsole.SendCommand( szTmp );
	return( true );
}

bool CWndMain::OnCommand( WORD wNotifyCode, int wID, HWND hwndCtl )
{
	// WM_COMMAND

	switch ( wID )
	{
	case IDC_M_INPUT:
		ASSERT( hwndCtl == m_wndInput.m_hWnd );
		break;
	case IDC_M_LOG:
		break;

	case IDM_OPTIONS:
#if 0
		if ( theApp.m_dlgOptions.m_hWnd )
		{
			theApp.m_dlgOptions.SetFocus();
		}
		else
		{
			theApp.m_dlgOptions.m_hWnd = ::CreateDialog( theApp.m_hInstance,
				MAKEINTRESOURCE(IDM_OPTIONS), HWND_DESKTOP, CDlgOptions::DefWindowProc );
		}
#endif
		break;
	case IDR_ABOUT_BOX:
		{
			CDlgAbout wndAbout;
			int iRet = DialogBoxParam(
				theApp.m_hInstance,  // handle to application instance
				MAKEINTRESOURCE(IDR_ABOUT_BOX),   // identifies dialog box template
				m_hWnd,      // handle to owner window
				CDialogBase::DialogProc,
				(DWORD) STATIC_CAST(CDialogBase,&wndAbout));  // pointer to dialog box procedure
		}
		break;

	case IDM_MINIMIZE:
		// SC_MINIMIZE
		ShowWindow(SW_HIDE);
		break;
	case IDM_RESTORE:
		// SC_RESTORE
		ShowWindow(SW_NORMAL);
		SetForegroundWindow();
		break;
	case IDM_EXIT:
		PostMessage( WM_CLOSE );
		break;

	case IDM_RESYNC_PAUSE:
		return( g_ServConsole.SendCommand( "R" ));

	case IDM_EDIT_COPY:
		m_wndLog.SendMessage( WM_COPY );
		break;

	case IDOK:
		// We just entered the text.
		return( OnOK());
	}
	return( true );
}

bool CWndMain::OnSysCommand( UINT uCmdType, int xPos, int yPos )
{
	// WM_SYSCOMMAND
	// return : 1 = i processed this.
	switch ( uCmdType )
	{
	case SC_MINIMIZE:
		ShowWindow(SW_HIDE);
		return( true );
	}
	return( false );
}

void CWndMain::OnTimer( int idTimer )
{
	// WM_TIMER
	// Update if necessary.
	if ( idTimer != IDT_UPDATE )
		return;

	switch ( m_iTabActive + IDC_M_LOG )
	{
	case IDC_M_LOG:
		break;
	case IDC_M_CLIENTS:
		FillStatusClients();
		break;
	case IDC_M_STATS:
		FillStatusStats();
		break;
	}
}

LRESULT CWndMain::OnNotify( int idCtrl, NMHDR* pnmh )
{
	// WM_NOTIFY
	ASSERT(pnmh);

	if ( idCtrl == IDC_M_TABCTL )
	{
	    if ( pnmh->code == TCN_SELCHANGE )
		{
			m_iTabActive = TabCtrl_GetCurSel(m_wndTabCtrl);
			UpdateActiveTab();
		}
		return 0;
	} 

	if ( idCtrl == IDC_M_LOG )
	{
		if ( pnmh->code == EN_LINK )	// ENM_LINK
		{
			ENLINK* pLink = (ENLINK *)(pnmh);
			if ( pLink->msg == WM_LBUTTONDOWN )
			{
				// Open the web page.
				// ShellExecute()
				SendMessage( WM_COMMAND, IDR_ABOUT_BOX );
				return 1;
			}
		}
		else if ( pnmh->code == EN_MSGFILTER )	// ENM_MOUSEEVENTS
		{
			MSGFILTER* pMsg = (MSGFILTER *)(pnmh);
			ASSERT(pMsg);
			if ( pMsg->msg == WM_MOUSEMOVE )
				return 0;
			if ( pMsg->msg == WM_RBUTTONDOWN )
			{
				HMENU hMenu = theApp.LoadMenu( IDM_POP_LOG );
				if ( hMenu == NULL )
					return 0;
				HMENU hMenuPop = GetSubMenu(hMenu,0);
				if ( hMenuPop )
				{
					POINT point;
					if ( GetCursorPos( &point ))	// pMsg->lParam is client coords.
					{
						TrackPopupMenu( hMenuPop, TPM_RIGHTBUTTON, point.x, point.y, 0, m_hWnd, NULL );
					}
				}
				DestroyMenu( hMenu );
				return 1;
			}
			if ( pMsg->msg == WM_CHAR )	// ENM_KEYEVENTS
			{
				// We normally have no business typing into this window.
				// Should we allow CTL C etc ?
				if ( _ISSET( pMsg->lParam, 29))	// ALT
					return 0;
				SHORT sState = GetKeyState( VK_CONTROL );
				if ( sState & 0xff00 )
					return 0;
				m_wndInput.SetFocus();
				m_wndInput.PostMessage( WM_CHAR, pMsg->wParam, pMsg->lParam );
				return 1;	// mostly ignored.
			}
			if ( pMsg->msg == WM_LBUTTONDBLCLK )	// ENM_KEYEVENTS
			{
				// Select the whole line and check it out. CRichEditCtrl
				// if it's an error then open the script editor.
				// int xPos = GET_X_LPARAM(pMsg->lParam);
				int yPos = GET_Y_LPARAM(pMsg->lParam);

				// What line is selected?
				int iLine = m_wndLog.GetFirstVisibleLine();
				iLine += yPos / m_iLogFontHeight;

				int index = m_wndLog.LineIndex(iLine);
				int iLen = m_wndLog.LineLength(index);
				m_wndLog.SetSel( index, index + iLen );

				// Get the text of the line.
				TCHAR szLine[256];
				if ( iLen >= COUNTOF(szLine))
				{
					return 1;	// mostly ignored.
				}

				int iRet = m_wndLog.GetSelText(szLine);
				szLine[iLen] = '\0';

				// Ussually in the format. "23:30:WARNING:(sphere_d_events_human.scp,36)Undefined symbols ')'" )
				TCHAR* pszStart = strchr( szLine, '(' );
				if ( pszStart )
				{
					TCHAR* pszEnd = strchr( szLine, ',' );
					if ( pszEnd )
					{
						*pszEnd = '\0';
						CGString sFileName;
						CResourceFilePtr pResFile = g_Cfg.FindResourceFile(pszStart+1);
						if ( pResFile )
						{
							sFileName = pResFile->GetFilePath();
						}
						else
						{
							sFileName = pszStart+1;
						}
						// execute the script editor.
					    HINSTANCE result = ShellExecute( NULL, TEXT("open"), sFileName, NULL,NULL, SW_SHOW );
					}
				}

				// m_wndLog
				pMsg->msg = WM_NULL;
				return 0;	// mostly ignored.
			}
		}
	}

	return 0;	// mostly ignored.
}

void CWndMain::OnShowWindow( bool fShown )
{
	// WM_SHOWWINDOW
	if ( fShown )
	{
		int iSeconds = g_Serv.m_Profile.GetSampleWindowLen();
		if ( iSeconds<=0 || iSeconds>5 )
			iSeconds = 5;
		SetTimer( IDT_UPDATE, iSeconds*1000 );
	}
	else
	{
		KillTimer(IDT_UPDATE);
	}
}

LRESULT CWndMain::DefWindowProc( UINT message, WPARAM wParam, LPARAM lParam )
{
	try
	{
		switch( message )
		{
		case WM_CREATE:
			return( OnCreate( (LPCREATESTRUCT) lParam ));
		case WM_SYSCOMMAND:
			if ( OnSysCommand( wParam&~0x0f, LOWORD(lParam), HIWORD(lParam)))
				return( 0 );
			break;
		case WM_COMMAND:
			if ( OnCommand( HIWORD(wParam), LOWORD(wParam), (HWND) lParam ))
				return( 0 );
			break;
		case WM_SHOWWINDOW:
			OnShowWindow( (bool) wParam);
			break;
		case WM_CLOSE:
			if ( ! OnClose())
				return( false );
			break;
		case WM_ERASEBKGND:	// don't bother with this.
			return 1;
		case WM_SIZE:	// get the client rectangle
			OnSize( wParam, LOWORD(lParam), HIWORD(lParam));
			return 0;
		case WM_DESTROY:
			OnDestroy();
			return 0;
		case WM_TIMER:
			OnTimer( wParam );
			return 0;
		case WM_SETFOCUS:
			OnSetFocus( (HWND) wParam );
			return 0;
		case WM_NOTIFY:
			OnNotify( (int) wParam, (NMHDR *) lParam );
			return 0;
		case WM_USER_POST_MSG:
			OnUserPostMessage( (COLORREF) wParam, (char*) lParam );
			return 1;
		case WM_USER_TRAY_NOTIFY:
			return OnUserTrayNotify( wParam, lParam );
		}
	}
	SPHERE_LOG_TRY_CATCH( "Window" )

	return CWindowBase::DefWindowProc( message, wParam, lParam );
}

//************************************

void CServConsole::Exit()
{
	// Unattach the window.
	DEBUG_CHECK( CThread::GetCurrentThreadId() == g_Serv.m_dwParentThread );

	if ( g_Serv.m_iExitFlag < 0 )
	{
		CGString sMsg;
		sMsg.Format( _TEXT("Server terminated by error %d!"), g_Serv.m_iExitFlag );
		int iRet = theApp.m_wndMain.MessageBox( sMsg, theApp.m_pszAppName, MB_OK|MB_ICONEXCLAMATION );
		// just sit here for a bit til the user wants to close the window.
		while ( CServConsole::OnTick( 500 ))
		{
		}
	}

	theApp.m_wndMain.DestroyWindow();
}

void CServConsole::OnTriggerEvent( SERVTRIG_TYPE type, DWORD dwArg1, DWORD dwArg2 )
{
	// set the title to reflect mode.
	if ( theApp.m_wndMain.m_hWnd == NULL )
		return;

	CGString sStatus;
	switch ( type)
	{
	case SERVTRIG_ServerMsg:
	case SERVTRIG_Startup:
		SetMessageColorType( dwArg2 );
		WriteString( (LPCSTR) dwArg1 );
		return;
	case SERVTRIG_ClientAttach:
	case SERVTRIG_ClientChange:
	case SERVTRIG_ClientDetach:
		// Number of connections ?
		if ( theApp.m_wndMain.m_iTabActive + IDC_M_LOG == IDC_M_CLIENTS )
		{
			theApp.m_wndMain.FillStatusClients();
		}
		return;
	case SERVTRIG_LoadStatus:
	case SERVTRIG_GarbageStatus:
	case SERVTRIG_SaveStatus:
	case SERVTRIG_TestStatus:
		sStatus.Format( "%d%%", MulDiv( dwArg1, 100, dwArg2 ));
		// fall thru...

	case SERVTRIG_ModeChange:
		{
		CGString sTitle;
		sTitle.Format( _TEXT("%s - %s (%s) %s"), 
			theApp.m_pszAppName, 
			(LPCSTR) g_Serv.GetName(), 
			(LPCSTR) g_Serv.GetModeDescription(), 
			sStatus );
		theApp.m_wndMain.SetWindowText( sTitle );

		NOTIFYICONDATA pnid;
		memset(&pnid,0,sizeof(pnid));
		pnid.cbSize = sizeof(NOTIFYICONDATA);
		pnid.hWnd   = theApp.m_wndMain.m_hWnd;
		pnid.uFlags = NIF_TIP;
		strcpy( pnid.szTip, sTitle );
		if ( ! Shell_NotifyIcon(NIM_MODIFY, &pnid))
		{
			// The Desktop has a habit of crashing.
			theApp.m_wndMain.AttachToShell();
			Shell_NotifyIcon(NIM_MODIFY, &pnid);
		}
		}
		return;
	}
}

void CServConsole::SetMessageColorType( int iColorType )
{
	// Set the color for the next text.
	if ( theApp.m_wndMain.m_hWnd == NULL )
		return;

	static const COLORREF sm_Colors[] =
	{
		0,	// body
		RGB( 127,127,0 ), // time,
		RGB( 255,0,0 ),  // error label.
		RGB( 0,127,255 ), // context (Scripts)
	};
	if ( ! iColorType || iColorType >= COUNTOF(sm_Colors))
	{
		// set to default color.
		theApp.m_wndMain.m_dwColorNew = theApp.m_wndMain.m_dwColorPrv;
	}
	else
	{
		theApp.m_wndMain.m_dwColorNew = sm_Colors[iColorType];
	}
}

bool CServConsole::WriteString( LPCTSTR pszMsg )
{
	// Post a message to print out on the main display.
	// If we use AttachThreadInput we don't need to post ?
	// RETURN:
	//  false = post did not work.
	// NOTE: 
	//  We may not be called from our own thread!

	if ( theApp.m_wndMain.m_hWnd == NULL )
		return false;

	COLORREF color = theApp.m_wndMain.m_dwColorNew;

	if ( g_Serv.m_dwParentThread != CThread::GetCurrentThreadId())
	{
		// A thread safe way to do text. strdup() sort of thing.
		char* pszMsgTmp = _strdup( pszMsg );
		ASSERT(pszMsgTmp);
		if ( ! theApp.m_wndMain.PostMessage( WM_USER_POST_MSG, (WPARAM) color, (LPARAM)pszMsgTmp ))
		{
			free( pszMsgTmp );
			return false;
		}
	}
	else
	{
		// just add it now.
		theApp.m_wndMain.List_Add( color, pszMsg );
	}
	return true;
}

bool CServConsole::OnTick( int iWaitmSec )
{
	// Periodically we get a time slice.
	// ARGS:
	//  iWaitmSec = how long to sit here.
	// RETURN: 
	//  false = exit the app.
	// NOTE:
	//  We should not do this if it is not our thread !!

	if ( CThread::GetCurrentThreadId() != g_Serv.m_dwParentThread )
		return true;	// Not supposed to be here !

	// Use MsgWaitForMultipleObjects( QS_ALLINPUT ) ?
	if ( iWaitmSec )
	{
		if ( theApp.m_wndMain.IsWindow())
		{
			UINT uRet = theApp.m_wndMain.SetTimer( IDT_WAKEUP, iWaitmSec );
			if ( ! uRet )
			{
				iWaitmSec = 0;
			}
		}
		else
		{
			::Sleep( iWaitmSec );	// might be nt service ?
			iWaitmSec = 0;
		}
	}

	// Give the windows message loops a tick.
	for(;;)
	{
		if ( g_Serv.m_iExitFlag )
			return false;

		MSG msg;

		// any windows messages ? (blocks until a message arrives)
		if ( iWaitmSec )
		{
			if ( ! GetMessage( &msg, NULL, 0, 0 ))
			{
				g_Serv.SetExitFlag( SPHEREERR_WINDOW_QUIT );
				return( false );
			}
			if ( msg.hwnd == theApp.m_wndMain.m_hWnd &&
				msg.message == WM_TIMER &&
				msg.wParam == IDT_WAKEUP )
			{
				theApp.m_wndMain.KillTimer( IDT_WAKEUP );
				iWaitmSec = 0;	// empty the queue and bail out.
				continue;
			}
		}
		else
		{
			if (! PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ))
				return true;
			if ( msg.message == WM_QUIT )
			{
				g_Serv.SetExitFlag( SPHEREERR_WINDOW_QUIT );
				return( false );
			}
		}

		if ( theApp.m_wndMain.m_wndInput.m_hWnd &&
			msg.hwnd == theApp.m_wndMain.m_wndInput.m_hWnd )
		{
			// IDC_M_INPUT
			if ( msg.message == WM_CHAR && msg.wParam == '\r' )
			{
				if ( theApp.m_wndMain.OnCommand( 0, IDOK, msg.hwnd ))
					return( true );
			}
			if ( msg.message == WM_KEYDOWN && 
				( msg.wParam == VK_UP || 
				msg.wParam == VK_DOWN ||
				msg.wParam == VK_ESCAPE ))
			{
				theApp.m_wndMain.m_wndInput.SetWindowText( theApp.m_wndMain.m_CommandLog.ScrollCmd( msg.wParam ));
			}
		}
		if ( theApp.m_dlgOptions.m_hWnd &&
			IsDialogMessage( theApp.m_dlgOptions.m_hWnd, &msg ))
		{
			return( true );
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return true;
}

bool CServConsole::Init( HINSTANCE hInstance, LPSTR lpCmdLine, int nCmdShow )
{
	// Decide if we want to create a window or not.
	theApp.InitInstance( SPHERE_TITLE " Server V" SPHERE_VERSION, hInstance, lpCmdLine );

	// Look for -UnInstallApp ???
	//
	// remove from the uninstall section of the registry. delete all my files.
	//

	if ( ! theApp.m_wndMain.IsWindow())
	{
		CWndMain::RegisterClass();

		theApp.m_wndMain.m_hWnd = ::CreateWindow(
			SPHERE_TITLE "Svr", // registered class name
			SPHERE_TITLE " V" SPHERE_VERSION, // window name
			WS_OVERLAPPEDWINDOW,   // window style
			CW_USEDEFAULT,  // horizontal position of window
			CW_USEDEFAULT,  // vertical position of window
			CW_USEDEFAULT,  // window width
			CW_USEDEFAULT,	// window height
			HWND_DESKTOP,      // handle to parent or owner window
			NULL,          // menu handle or child identifier
			theApp.m_hInstance,  // handle to application instance
			(CWindowBase*) &theApp.m_wndMain // window-creation data
			);
		if ( theApp.m_wndMain.m_hWnd == NULL )
			return false;
		theApp.m_wndMain.ShowWindow( nCmdShow );
	}

	return( true );
}

#endif // _WIN32

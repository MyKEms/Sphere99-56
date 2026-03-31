//
// SPHERESRV.CPP.
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
// game/login server for uo client
// http://www.menasoft.com for more details.
// I'm liking http://www.cs.umd.edu/users/cml/cstyle/CppCodingStandard.html as a coding standard.
//
// [menace@unforgettable.com] Dennis Robinson, http://www.menasoft.com
// [nixon@mediaone.net] Nix, Scripts
// [petewest@mindspring.com] Westy, /INFO, Maps
// [johnr@cyberstreet.com] John Davey (Garion)(The Answer Guy) docs.web pages.
// [bytor@mindspring.com] Philip Esterle (PAE)(Altiara Dragon) tailoring,weather,moons,LINUX
//
// Current:
//  Casting penalties for plate
//	Unconscious state.
//  vendor economy.
//  NPC AI
//  Interserver registration.
//
// Top Issues:
//  new mining/smelting system. more than 1 ingot for smelting 1 ore.
//  weapon table for speed and damage. DEX for speed.
//
// Wish list:
//  chop sound.
//  inscribe magic weapons, wands from menu
//  Magery skill degradation for plate and weapons in hand.
//  Skill sticking. set a skill so it will stick at a value and not degrade.
//  NPC smith repair.
//  GrandMaster Created items are better and labled with name of the creator.
//  Web page Portrait of PC
//  Dex as part of AR ?
//  Stable limits are per player/account based.
//
// Protocol Issues:
// 2. placing an item like a forge or any other deeded item. (xcept houses and ships, i know how to do those.)
// 9. grave stone and sign gumps ?
// 4. Watching someone else drag an item from a container on the ground to their pack. and visa versa.
// 5. Watching someone else drag an item from a pack to the ground. and visa versa.
// 6. Adding a person to the party system.
// x. Looking through all the gump menus for the house.
// x. casting a gate spell. ok to watch someone else do it.
// x. Chop a tree. (i want to get the proper sound)
//
// BUGS:
//   npc sheild item. block npc's from walking here .
//   bless > 100 will lose stats
//   account update before savbe ?
//
// 2. Hiding does not work right.
//	You are still attacked by monsters when hidden.
// 3. Fletching does not work right.
//	If you have 100 shafts and 100 feathers and you fail to make arrow/bolts you lose everything.
// 4. Combat is EXTREMELY SLOW.
// 5. There are some truly Physotic Monsters out there.
//	For example: There are these archers that look like Orcish Lords that shot 4 - 5 arrows at a time.
//	So by the time you swing at them once you have 15 - 20 arrows in ya and your TOAST.
// 6. Targeting after casting.
//	If you are attacking a monster with a sword and cast a spell you lose your target. You just stand
//	there while the monster rips you a new asshole until you 2x click and attack him again.
//
// ADDNPC from scripts, "CONT" keyword

#include "stdafx.h"	// predef header.
#include <cstdlib>	// malloc/free for custom operator new/delete
#include <new>		// std::bad_alloc
#ifdef _WIN32
#include "eh.h"
#include <crtdbg.h>
#endif

extern "C"
{
	void globalendsymbol()	// put this here as just the ending offset.
	{
	}
	const BYTE globalenddata = 0xff;
}

const CScriptPropX g_ProfileProps[PROFILE_QTY+1] =
{
#define CPROFILEPROP(a,b,c) CSCRIPT_PROPX_IMP(a,b,c)
#include "cprofileprops.tbl"
#undef CPROFILEPROP
	NULL,
};

const char* const g_Stat_Name[STAT_QTY+1] =	// not sorted obviously.
{
	"STR",
	"INT",
	"DEX",
	"HITS",
	"MANA",
	"STAM",
	"FOOD",
	"FAME",
	"KARMA",
	"MAXHITS",
	"MAXMANA",
	"MAXSTAM",
	NULL,
};

const char* g_szServerDescription =
SPHERE_TITLE " TEST Version " SPHERE_VERSION " "
#ifdef _WIN32
"[WIN32]"
#else
#ifdef _BSD
"[FreeBSD]"
#else
"[LINUX]"
#endif
#endif
#ifdef _DEBUG
"[DEBUG]"
#endif
" by " SPHERE_URL;

// game servers stuff.
CWorld		g_World;	// the world. (we save this stuff)
CServer		g_Serv;	// current state stuff not saved.
CServerDef*	g_pServ = &g_Serv;
CSphereResourceMgr g_Cfg;
CResourceMgr* g_pCfg = &g_Cfg;
CServTask	g_ServTask;
CBackTask	g_BackTask;
CMainTask	g_MainTask;
CLog		g_Log;
CLogBase*	g_pLog = &g_Log;
CAccountMgr	g_Accounts;	// All the player accounts. name sorted CAccount
CServConsole g_ServConsole;

#if defined(_WIN32) && ! defined(_LIB)
CSphereService g_NTService;
#endif

const CPointMap g_pntLBThrone(1323,1624,0); // This is origin for degree, sextant minute coordinates

//////////////////////////////////////////////////////////////////
// util type stuff.

//
// Provide these functions globally.
//

class CSphereAssert : public CGException
{
protected:
	const char* const m_pExp;
	const char* const m_pFile;
	const unsigned m_uLine;
public:
	virtual BOOL GetErrorMessage( LPTSTR lpszError, UINT nMaxError,	UINT* pnHelpContext )
	{
		sprintf( lpszError, "Assert pri=%d:'%s' file '%s', line %d",
			m_eSeverity, m_pExp, m_pFile, m_uLine );
		return( true );
	}
	CSphereAssert( LOGL_TYPE eSeverity, const char* pExp,
		const char* pFile, unsigned uLine ) :
	CGException( eSeverity, 0, _TEXT("Assert")),
		m_pExp(pExp), m_pFile(pFile), m_uLine(uLine)
	{
	}
	virtual ~CSphereAssert()
	{
	}
};

#ifdef _DEBUG

void Debug_CheckPoint()
{
	// Check the continuity of the server periodically.
	// Validate the heap?

#ifdef _WIN32
	ASSERT(_CrtCheckMemory());
#endif

	if ( g_MulInstall.GetMulFile(VERFILE_MAP0)->IsFileOpen())
	{
		CGFile* pFile = g_MulInstall.GetMulFile(VERFILE_MAP0);
		long lRet = pFile->Seek( 50000, SEEK_SET );
		if ( lRet != 50000 )
		{
			g_Serv.Printf("Odd!");
		}
	}
}

void Debug_CheckFail( const char* pExp, const char* pFile, unsigned uLine )
{
	g_Log.Event( LOG_GROUP_DEBUG, LOGL_ERROR, "Check Fail:'%s' file '%s', line %d" LOG_CR, pExp, pFile, uLine );
}

void Debug_Assert_CheckFail( const char* pExp, const char* pFile, unsigned uLine )
{
	throw CSphereAssert( LOGL_CRIT, pExp, pFile, uLine );
}
#endif	// _DEBUG

void Assert_CheckFail( const char* pExp, const char* pFile, unsigned uLine )
{
	// These get left in release code .
	// This is _assert() is MS code.
	throw CSphereAssert( LOGL_CRIT, pExp, pFile, uLine );
}

#ifdef _WIN32

class CSphereException : public CGException
{
	// Catch and get details on the system exceptions.
	// NULL pointer access etc.
public:
	const DWORD m_dwAddress;
public:
	CSphereException( unsigned int uCode, DWORD dwAddress ) :
		m_dwAddress( dwAddress ),
	CGException( LOGL_CRIT, uCode, _TEXT("Exception"))
	{
	}
	virtual ~CSphereException()
	{
	}
	virtual BOOL GetErrorMessage( LPTSTR lpszError, UINT nMaxError,	UINT* pnHelpContext )
	{
		sprintf( lpszError, "Exception code=0%0x, addr=0%0x",
			m_hError, m_dwAddress );
		return( true );
	}
};

extern "C"
{

	void _cdecl Sphere_Exception_Win32( unsigned int id, struct _EXCEPTION_POINTERS* pData )
	{
		// WIN32 gets an exception. press '@' to test this.
		// id = 0xc0000094 for divide by zero.
		// STATUS_ACCESS_VIOLATION is 0xC0000005.

		DWORD dwCodeStart = (DWORD)(BYTE *) &globalstartsymbol;	// sync up to my MAP file.
#ifdef _DEBUG
		// NOTE: This value is not accurate for some stupid reason. (only in debug versions)
		//dwCodeStart += 0x06d40;	// no idea why i have to do this.
#endif
		//	_asm mov dwCodeStart, CODE

		DWORD dwAddr = (DWORD)( pData->ExceptionRecord->ExceptionAddress );
		dwAddr -= dwCodeStart;
#ifdef _DEBUG
		dwAddr += 0x0a450;	// so it will match the most recent map file. DEBUG only !
#endif

		throw( CSphereException( id, dwAddr ));
	}

#ifndef _LIB
	void _cdecl _assert( void *pExp, void *pFile, unsigned uLine )
	{
		// trap the system version of this just in case.
		Assert_CheckFail((const char*) pExp, (const char*) pFile, uLine );
	}
#endif

	int _cdecl _purecall()
	{
		// catch this special type of C++ exception as well.
		Assert_CheckFail( "purecall", "unknown", 1 );
		return 0;
	}

#if 0
	void _cdecl _amsg_exit( int iArg )
	{
		// try to trap some of the other strange exit conditions !!!
		// Some strange stdlib calls use this.
		Assert_CheckFail( "_amsg_exit", "unknown", 1 );
	}
#endif

}	// extern "C"

#endif	// _WIN32

#if !defined(_MFC_VER) && ! defined(_LIB)

// Flag to track whether g_Serv is constructed and safe to use for stats.
// Must be set AFTER g_Serv construction completes.
static bool s_fServReady = false;

void* _cdecl operator new( size_t stAllocateBlock )
{
	// Use malloc to avoid infinite recursion (CMemBlockBase ctor calls new).
	void* pData = malloc( stAllocateBlock );
	if ( pData == NULL && stAllocateBlock > 0 )
	{
		throw std::bad_alloc();
	}
	if ( s_fServReady )
	{
		g_Serv.StatInc(SERV_STAT_ALLOCS);
	}
	return pData;
}

void* _cdecl operator new[]( size_t stAllocateBlock )
{
	void* pData = malloc( stAllocateBlock );
	if ( pData == NULL && stAllocateBlock > 0 )
	{
		throw std::bad_alloc();
	}
	if ( s_fServReady )
	{
		g_Serv.StatInc(SERV_STAT_ALLOCS);
	}
	return pData;
}

void _cdecl operator delete( void* pThis ) noexcept
{
	if ( pThis == NULL )
		return;
	free( pThis );
	if ( s_fServReady )
	{
		g_Serv.StatDec(SERV_STAT_ALLOCS);
	}
}

void _cdecl operator delete[]( void* pThis ) noexcept
{
	if ( pThis == NULL )
		return;
	free( pThis );
	if ( s_fServReady )
	{
		g_Serv.StatDec(SERV_STAT_ALLOCS);
	}
}

#endif	// _MFC_VER

#if defined(_WIN32) && ! defined(_LIB)
const char* CSphereService::GetServiceName( int iForm ) const
{
	switch ( iForm )
	{
	case 0: // Event Source.Name
	case 1: // Unique name suitable for a file or pipe name.
	case 2:	// Service name
		return( SPHERE_TITLE "Svr" );
	case 3: // Descriptive name.
		return( SPHERE_TITLE " V" SPHERE_VERSION );
	}
	ASSERT(0);
	return NULL;
}

int CSphereService::MainEntryPoint( int argc, char* argv[] )
{
	return Sphere_MainEntryPoint(argc, argv);
}

#endif // _WIN32

//*******************************************************************
// -CServTask

THREAD_ENTRY_RET _cdecl CServTask::EntryProc( void* lpThreadParameter ) // static
{
	// The main message loop.
	g_ServTask.InitInstance();
#ifdef _WIN32
	_set_se_translator( Sphere_Exception_Win32 );	// must be called for each thread.
#endif
	while ( ! Sphere_OnTick())
	{
#ifdef _DEBUG
		DEBUG_CHECK( g_ServTask.SetScriptContext(NULL) == NULL );
		DEBUG_CHECK( g_ServTask.SetExecContext(NULL) == NULL );
#endif
	}
	g_ServTask.ExitInstance();
}

void CServTask::CreateThread()
{
	// AttachInputThread to us if needed ?
	CThread::CreateThread( EntryProc );
}

void CServTask::CheckStuckThread()
{
	// Called from another thread.
	// Periodically called to check if the tread is stuck.

	static CGTime sm_timeRealPrev;

	// Has real time changed ?
	CGTime timeRealCur;
	timeRealCur.InitTimeCurrent();
	int iTimeDiff = timeRealCur.GetTime() - sm_timeRealPrev.GetTime();
	iTimeDiff = ABS( iTimeDiff );
	if ( iTimeDiff < g_Cfg.m_iFreezeRestartTime )
		return;
	sm_timeRealPrev = timeRealCur;

	// Has server time changed ?
	CServTime timeCur = CServTime::GetCurrentTime();
	if ( timeCur != m_timePrev )	// Seems ok.
	{
		m_timePrev = timeCur;
		return;
	}

	if ( g_Serv.IsValidBusy())	// Server is just busy.
		return;

	if ( m_timeRestart == timeCur )
	{
		g_Log.Event( LOG_GROUP_DEBUG, LOGL_FATAL, "Main loop freeze RESTART FAILED!" LOG_CR );
		// g_Serv.SetExitFlag( SPHEREERR_INTERNAL );
		// return;
		//_asm int 3;
	}

	if ( m_timeWarn == timeCur )
	{
		// Kill and revive the main process
		g_Log.Event( LOG_GROUP_DEBUG, LOGL_CRIT, "Main loop freeze RESTART!" LOG_CR );

#ifndef _DEBUG
#ifdef _WIN32
		TerminateThread( 0xDEAD );

		// try to restart it.
		g_Log.Event( LOG_GROUP_DEBUG, LOGL_EVENT, "Trying to restart the main loop thread" LOG_CR );
		CreateThread();
#endif
#endif
		m_timeRestart = timeCur;
	}
	else
	{
		g_Log.Event( LOG_GROUP_DEBUG, LOGL_WARN, "Main loop frozen ?" LOG_CR );
		m_timeWarn = timeCur;
	}
}

//*****************************************************

THREAD_ENTRY_RET _cdecl CMainTask::EntryProc( void* lpThreadParameter ) // static
{
	// Just make sure the main loop is alive every so often.
	// This should be the parent thread.
	// try to restart it if it is not.

	g_MainTask.InitInstance();

	// ASSERT( CThread::GetCurrentThreadId() == g_Serv.m_dwParentThread ); -- skip, thread IDs may differ
	while ( ! g_Serv.m_iExitFlag )
	{
		if ( g_Cfg.m_iFreezeRestartTime <= 0 )
		{
			DEBUG_ERR(( "Freeze Restart Time cannot be cleared at run time" LOG_CR ));
			g_Cfg.m_iFreezeRestartTime = 10;
		}

		g_ServConsole.OnTick( g_Cfg.m_iFreezeRestartTime * 1000 );

		// Don't look for freezing when doing certain things.
		if ( g_Serv.IsLoading() || ! g_Cfg.m_fSecure )
			continue;

		g_ServTask.CheckStuckThread();
		g_BackTask.CheckStuckThread();
	}

	g_MainTask.ExitInstance(); // ?? main task runs til process ends.
}

void CMainTask::CreateThread()
{
	// AttachInputThread to us if needed ?
	CThread::CreateThread( EntryProc );
}

CSphereThread* CSphereThread::GetCurrentThread() // static
{
	DWORD dwThreadID = CThread::GetCurrentThreadId();
	if ( g_ServTask.GetThreadID() == dwThreadID )
	{
		return( &g_ServTask );
	}
	if ( g_BackTask.GetThreadID() == dwThreadID )
	{
		return( &g_BackTask );
	}
	if ( g_MainTask.GetThreadID() == dwThreadID )
	{
		return( &g_MainTask );
	}
	//DEBUG_ASSERT(0);
	return( &g_MainTask );	// ! NEVER RETURN NULL !
}

//*******************************************************************

SPHEREERR_TYPE Sphere_InitServer( int argc, char *argv[] )
{
	// Do some sanity checks right off.
	ASSERT( UO_MAX_EVENT_BUFFER >= sizeof( CUOCommand ));
	ASSERT( UO_MAX_EVENT_BUFFER >= sizeof( CUOEvent ));
	ASSERT( sizeof( int ) == sizeof( DWORD ));	// make this assumption often.
	ASSERT( sizeof( ITEMID_TYPE ) == sizeof( DWORD ));
	ASSERT( sizeof( WORD ) == 2 );
	ASSERT( sizeof( DWORD ) == 4 );
	ASSERT( sizeof( NWORD ) == 2 );
	ASSERT( sizeof( NDWORD ) == 4 );
	//ASSERT( sizeof(CUOItemTypeRec) == 37 );	// byte pack working ? - may differ on GCC

#ifdef _WIN32
	_set_se_translator( Sphere_Exception_Win32 );
#endif
#ifdef _DEBUG
	// _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|(16*_CRTDBG_CHECK_EVERY_1024_DF));
#endif
	Debug_CheckPoint();

	// g_ScriptClassMgr.InitClasses();	// make sure all scripting ability is setup. (not available)
	CSphereExpArgs::InitFunctions();
#ifdef USE_JSCRIPT
	g_JScriptEngine.Init(NULL);
#endif

	// The caller of this function is now the main task.
	g_MainTask.InitInstance();

	g_Log.WriteString( LOG_CR );		// blank space in log.
	g_Log.Event( LOG_GROUP_INIT, LOGL_TRACE, "%s" LOG_CR
		"Compiled on " __DATE__ " (" __TIME__ ")" LOG_CR
		// "NOTE: All saves made with ver .53 are unstable and not supported in the future" LOG_CR
		LOG_CR,
		g_szServerDescription );

#ifdef _WIN32
	g_Serv.m_Profile.InitTasks( SPHERE_FILE, PROFILE_QTY, g_ProfileProps );
#else
	g_Serv.m_Profile.InitTasks( PROFILE_QTY );
#endif

	if ( argc >= 2 && toupper(argv[1][1]) == 'I' )	// must go before other stuff.
	{
		g_Serv.CommandLineArg(argv[1]);
	}

	Debug_CheckPoint();
	if ( ! g_Serv.Load())
	{
		g_Log.Event( LOG_GROUP_INIT, LOGL_FATAL, "The " SPHERE_FILE ".INI file is corrupt or missing" LOG_CR );
		return( SPHEREERR_BAD_INI );
	}

	Debug_CheckPoint();
	for ( int argn=1; argn<argc; argn++ )
	{
		if ( g_Serv.CommandLineArg( argv[argn] ))
		{
			return( SPHEREERR_COMMANDLINE );
		}
	}

	Debug_CheckPoint();
	if ( ! g_World.LoadAll())
	{
		if ( g_Cfg.m_sMainLogServerDir.IsEmpty())
		{
			return( SPHEREERR_BAD_WORLD );
		}
	}

	Debug_CheckPoint();
	if ( ! g_Serv.SocketsInit())
	{
		return( SPHEREERR_BAD_SOCKET );
	}

	Debug_CheckPoint();

	g_Log.Event( LOG_GROUP_INIT, LOGL_TRACE, g_Serv.GetStatusString( 0x24 ));
	g_Log.Event( LOG_GROUP_INIT, LOGL_TRACE, _TEXT("Startup complete. items=%d, chars=%d" LOG_CR), g_Serv.StatGet(SERV_STAT_ITEMS), g_Serv.StatGet(SERV_STAT_CHARS));

	g_Serv.OnTriggerEvent( SERVTRIG_Startup, (DWORD) ( "Press '?' for console commands" LOG_CR ), 0 );

	return( SPHEREERR_OK );
}

void Sphere_ExitServer()
{
	// NOTE: Try to close things down in proper order. (so ref counts work)
	// ASSERT( g_MainTask.GetThreadID() == GetCurrentThreadId());
	g_Serv.SetServerMode( SERVMODE_Exiting );

	g_BackTask.WaitForClose( 15 );
	g_ServTask.WaitForClose( 15 );

	g_Serv.SocketsClose();
	g_World.Close(true);

	g_Cfg.Unload(false);
	g_Accounts.Empty();

	if ( g_Serv.m_iExitFlag < 0 )
	{
		g_Log.Event( LOG_GROUP_INIT, LOGL_FATAL, "Server terminated by error %d!" LOG_CR, g_Serv.m_iExitFlag );
	}
	else
	{
		g_Log.Event( LOG_GROUP_INIT, LOGL_EVENT, "Server shutdown (code %d) complete!" LOG_CR, g_Serv.m_iExitFlag );
	}

	g_Serv.OnTriggerEvent( SERVTRIG_Shutdown );
	g_Log.Close();

#if 0 // def _AFXDLL
	AfxSocketTerm();
#else
#ifdef _WIN32
	WSACleanup();
#endif
#endif
}

SPHEREERR_TYPE Sphere_OnTick()
{
	// Give the world (CServTask) a single tick.
	// RETURN: 0 = everything is fine.

	static int s_nTick = 0;
	s_nTick++;

	// ASSERT( g_ServTask.GetThreadID() == GetCurrentThreadId());
#ifndef _DEBUG
// I put the try stuff in the debug section so I could isolate some of the exceptions we are getting
	try
	{
#endif
		g_World.OnTick();
		g_Serv.OnTick();

#ifndef _DEBUG
	}
	SPHERE_LOG_TRY_CATCH( "Main Loop" )
#endif

	return( g_Serv.m_iExitFlag );
}

//******************************************************

SPHEREERR_TYPE Sphere_MainEntryPoint( int argc, char *argv[] )
{
	// Enable memory stats tracking now that g_Serv is fully constructed.
	s_fServReady = true;

	g_Serv.m_iExitFlag = Sphere_InitServer( argc, argv );
	// ASSERT( g_MainTask.GetThreadID() == GetCurrentThreadId());

	if ( ! g_Serv.m_iExitFlag )
	{
#ifdef _WIN32
		if ( CGSystemInfo::IsNt() && g_Cfg.m_iFreezeRestartTime )
		{
			// Create a thread to do server stuff on, just monitor with this one.
			g_ServTask.CreateThread();
			g_MainTask.EntryProc( 0 );
		}
		else
#endif
		{
			// Linux: run single-threaded to avoid threading issues under QEMU user-mode.
			// The monitoring thread (CMainTask) and separate CServTask thread are
			// not needed -- run the tick loop directly in the main thread.
			g_MainTask.ExitInstance();	// not used anymore.
			g_ServTask.EntryProc( 0 );	// This is the main task now.
		}
	}

	Sphere_ExitServer();
	return( g_Serv.m_iExitFlag );
}

/////////////////////////////////////////////////////////////////////////////////////
//
//	FUNCTION: main()
//
//	PURPOSE:  This is the main function in the application.
//		It parses the command line arguments and attempts to start the service.
//
//	PARAMETERS:
//		argc - Argument count
//		argv - Program arguments
//
//	RETURN VALUE:
//		none
//
//	COMMENTS:
//
/////////////////////////////////////////////////////////////////////////////////////

#ifndef _LIB
#ifndef _WIN32
int _cdecl main( int argc, char* argv[] )
{
	return Sphere_MainEntryPoint(argc, argv);
}
#endif // _WIN32

#ifdef _WIN32
#if defined(_CONSOLE)
int _cdecl main( int argc, char* argv[] )
{
	HINSTANCE hInstance = NULL;
	int nCmdShow = SW_SHOWNORMAL; // show state = 1
	LPSTR lpCmdLine = argv[0];
#else
int WINAPI WinMain(
	HINSTANCE hInstance,      // handle to current instance
	HINSTANCE hPrevInstance,  // handle to previous instance
	LPSTR lpCmdLine,          // command line
	int nCmdShow              // show state, SW_SHOWNORMAL
	)
{
	TCHAR* argv[32];
	argv[0] = NULL;
	int argc = Str_ParseCmds( lpCmdLine, &argv[1], COUNTOF(argv)-1, " \t" ) + 1;
#endif // _CONSOLE

	g_MainTask.InitInstance();

	if ( ! CGSystemInfo::IsNt())
	{
		// We are running Win9x - So we are clearly not an NT service.
do_not_nt_service:
		g_ServConsole.Init( hInstance, lpCmdLine, nCmdShow );
		int iRet = Sphere_MainEntryPoint(argc, argv);
		g_ServConsole.Exit();
		return( iRet );
	}

#ifdef _WIN32
	if ( ! g_NTService.Init( argc, argv ))	// Tell NT we might be a service.
		goto do_not_nt_service;

	// Open the temporary log file.
	{
	CString sTmp;
	sTmp.Format( "\\%ssvc.log", g_NTService.GetServiceName(0));
	g_Log.Open( sTmp, OF_WRITE | OF_TEXT | OF_SHARE_DENY_WRITE );
	}

	int iRet = g_NTService.Start( argc, argv );
	if ( iRet == SPHEREERR_MULTI_INST )
	{
		goto do_not_nt_service;
	}

	Sphere_ExitServer();
	return iRet;
#else	// _WIN32
	return -1;
#endif

}

#endif // _WIN32
#endif // _LIB

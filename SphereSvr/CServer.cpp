//
// CServer.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.
#include "spherelog.h"
#ifndef _WIN32
#include <signal.h>
#include <cstring>
#include <poll.h>
#define strcmpi strcasecmp
static DWORD GetTickCount()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (DWORD)((unsigned long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
static int MulDiv(int a, int b, int c) { return (int)(((long long)a * b) / c); }
#endif

////////////////////////////////////////////////////////////////////////////////////////
// -CServer

#ifdef USE_JSCRIPT
#define CSERVERMETHOD(a,b,c) JSCRIPT_METHOD_IMP(CServer,a)
#include "cservermethods.tbl"
#undef CSERVERMETHOD
#endif

const CScriptMethod CServer::sm_Methods[CServer::M_QTY+1] =
{
#define CSERVERMETHOD(a,b,c) CSCRIPT_METHOD_IMP(a,b,c)
#include "cservermethods.tbl"
#undef CSERVERMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP2(Server,NULL,CServer::sm_Methods,NULL,ServerDef);

CServer::CServer() : CServerDef( UID_INDEX_CLEAR, SPHERE_TITLE, CSocketAddressIP( SOCKET_LOCAL_ADDRESS ))
{
	IncRefCount();	// static singleton

	m_iExitFlag = SPHEREERR_OK;
	m_fResyncPause = false;
	m_uSizeMsgMax = 0;

	m_nClientsAreAdminTelnets = 0;
	m_nClientsAreGuests = 0;

	m_sServVersion = SPHERE_VERSION;

	// we are in start up mode. // IsLoading()
	SetServerMode( SERVMODE_Loading );
}

CServer::~CServer()
{
	StaticDestruct();	// static singleton
}

template<>
void CScriptClassTemplate<CServer>::InitScriptClass()
{
	if ( IsInit())
		return;
	AddSubClass(&g_World.sm_ScriptClass);
	AddSubClass(&g_Cfg.sm_ScriptClass);
	CScriptClass::InitScriptClass();
}

#ifndef _WIN32
void _cdecl Signal_Terminate(int x=0) // If shutdown is initialized
{
	// LINUX specific: set exit flag instead of throwing from a signal handler.
	// Throwing C++ exceptions from signal handlers is undefined behavior.
	g_Serv.SetExitFlag( (x == SIGTERM) ? SPHEREERR_TIMED_CLOSE : SPHEREERR_CTRLC );
}

static volatile int s_nSEGV = 0;
#ifndef _WIN32
#include <setjmp.h>
volatile sig_atomic_t g_fSEGV_catch = 0;  // 1 = longjmp recovery enabled
sigjmp_buf g_SEGV_jmpbuf;
#endif

void _cdecl Signal_Illegal_Instruction(int x=0)
{
	s_nSEGV++;
	fprintf(stderr, "SEGV/ILL signal %d caught (count=%d)\n", x, s_nSEGV);
	fflush(stderr);

#ifndef _WIN32
	if ( g_fSEGV_catch )
	{
		// Jump back to the recovery point (skips the faulting code).
		signal(x, &Signal_Illegal_Instruction);
		siglongjmp(g_SEGV_jmpbuf, 1);
		return; // not reached
	}
#endif

	if ( s_nSEGV > 10 )
	{
		signal(x, SIG_DFL);
		raise(x);
		return;
	}
	signal(x, &Signal_Illegal_Instruction);
}
#endif

void CServer::SetSignals()
{
	// We have just started or we changed Secure mode.

#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN); // This appears when we try to write to a broken network connection

	if ( g_Cfg.m_fSecure )
	{
		signal(SIGQUIT, &Signal_Terminate);
		signal(SIGTERM, &Signal_Terminate);
		signal(SIGINT, &Signal_Terminate);
		signal(SIGILL, &Signal_Terminate);
		signal(SIGSEGV, &Signal_Illegal_Instruction);
	}
	else
	{
		signal(SIGTERM, SIG_DFL );
		signal(SIGQUIT, SIG_DFL );
		signal(SIGINT, SIG_DFL );
		signal(SIGILL, SIG_DFL);
		signal(SIGSEGV, SIG_DFL);
	}
#endif

	if ( ! IsLoading())
	{
		g_World.Broadcast( g_Cfg.m_fSecure ?
			"The world is now running in SECURE MODE" :
			"WARNING: The world is NOT running in SECURE MODE" );
	}
}

SERVMODE_TYPE CServer::SetServerMode( SERVMODE_TYPE mode )
{
	// SERVTRIG_QTY
	SERVMODE_TYPE iModePrv = m_iModeCode;
	if ( iModePrv != mode )
	{
		m_iModeCode = mode;
		OnTriggerEvent( SERVTRIG_ModeChange, mode );
	}
	return iModePrv;
}

bool CServer::IsValidBusy() const
{
	// We might appear to be stopped but it's really ok ?
	// ?
	switch ( m_iModeCode )
	{
	case SERVMODE_Saving:
		if ( g_World.IsSaving())
			return true;
		break;
	case SERVMODE_Loading:
		return( true );
	case SERVMODE_RestockAll:	// these may look stuck but are not.
	case SERVMODE_GarbageCollect:
		return( true );
	}
	return( false );
}

bool CServer::OnTick_Busy()
{
	// We are busy doing stuff but give the pseudo background stuff a tick.
	// RETURN: true = keep going.
#if defined(_WIN32) && ! defined(_LIB)
	if ( ! g_NTService.OnTick())
	{
		SetExitFlag( SPHEREERR_NTSERVICE_CLOSE );
	}
#endif
	g_ServConsole.OnTick(0);
	return( ! m_iExitFlag );
}

void CServer::SetExitFlag( SPHEREERR_TYPE iFlag )
{
	if ( m_iExitFlag )	// already set.
		return;
	m_iExitFlag = iFlag;
}

void CServer::SetShutdownTime( int iMinutes ) // If shutdown is initialized
{
	if ( iMinutes == 0 )
	{
		if ( ! m_timeShutdown.IsTimeValid())
			return;
		m_timeShutdown.InitTime();
		g_World.Broadcast("Shutdown has been interrupted.");
		return;
	}

	SetPollTime();	// record the fact that we checked.

	if ( iMinutes < 0 )
	{
		// Just re-display the amount of time.
		iMinutes = m_timeShutdown.GetTimeDiff() / ( 60* TICKS_PER_SEC );
	}
	else
	{
		m_timeShutdown.InitTimeCurrent( iMinutes* 60* TICKS_PER_SEC );
	}

	CGString sTmp;
	sTmp.Format( "Server going down in %i minute(s).", iMinutes );
	g_World.Broadcast( sTmp );
}

bool CServer::TestServerIntegrity()
{
	// set a checksum for the server code and constant data.
	// if it changes then exit.
	// globalstartdata
	// globalstopdata
	// void * pBase = &__ImageBase;

#ifdef _DEBUG
	static const DWORD sm_dwCheckSum = 0;
#else
	static const DWORD sm_dwCheckSum = 0;
#endif

	DWORD* pdwCodeStart = (DWORD *)(void*) globalstartsymbol;
	DWORD* pdwCodeStop = (DWORD *)(void*) globalendsymbol;
	DWORD dwCheckSum = 0;

	ASSERT( pdwCodeStart < pdwCodeStop );
	for ( ; pdwCodeStart < pdwCodeStop; pdwCodeStart++ )
	{
		dwCheckSum += *pdwCodeStart;
	}

	// SetExitFlag( SPHEREERR_BAD_CODECRC );

	return dwCheckSum;
}

bool CServer::WriteString( LPCTSTR pszMsg )
{
	// Print just to the main console.
	if ( pszMsg == NULL || ISINTRESOURCE(pszMsg))
		return false;
	g_ServConsole.WriteString( pszMsg );
	return true;
}

void CServer::Event_PrintClient( LPCTSTR pszMsg ) const
{
	// Tell just the telnet clients about system events.
	if ( ! m_nClientsAreAdminTelnets )
		return;

	for ( CClientPtr pClient = GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
	{
		if ( pClient->m_ConnectType != CONNECT_TELNET )
			continue;
		if ( pClient->GetPrivLevel() < PLEVEL_Admin )
			continue;
		pClient->WriteString( pszMsg );
	}
}

void CServer::Event_PrintPercent( SERVTRIG_TYPE type, long iCount, long iTotal )
{
	// These vals can get very large. so use MulDiv to prevent overflow. (not IMULDIV)

	DEBUG_CHECK( iCount >= 0 );
	DEBUG_CHECK( iTotal >= 0 );

	if ( iTotal <= 0 )
		return;

#if 1
	int iPercent = MulDiv( iCount, 100, iTotal );

	TCHAR szPercent[32];
	int iLen = sprintf( szPercent, "%d%%", iPercent );
	TCHAR szBack[32];
	memset( szBack, 0x08, iLen );
	szBack[iLen] = '\0';

	Event_PrintClient( szPercent );
	Event_PrintClient( szBack );	// backspace over it.
#endif

	OnTriggerEvent( type, iCount, iTotal );
	OnTick_Busy();
}

int CServer::GetAgeHours() const
{
	return( CServTime::GetCurrentTime().GetTimeRaw() / (60*60*TICKS_PER_SEC));
}

CString CServer::GetStatusStringRegister( bool fFirst ) const
{
	// we are registering ourselves.
	CString sMsg;
	if ( fFirst )
	{
		TCHAR szVersion[128];
		sMsg.Format( SPHERE_TITLE ", Name=\"%s\", RegPass=\"%s\", Port=%d, Ver=\"%s\", TZ=%d, EMail=\"%s\", URL=\"%s\", Lang=\"%s\", CliVer=\"%s\", AccApp=%d" LOG_CR,
			(LPCTSTR) GetName(),
			(LPCTSTR) m_sRegisterPassword,
			m_ip.GetPort(),
			(LPCTSTR) SPHERE_VERSION,
			m_TimeZone,
			(LPCTSTR) m_sEMail,
			(LPCTSTR) m_sURL,
			(LPCTSTR) m_sLang,
			(LPCTSTR) m_ClientVersion.WriteCryptVer(szVersion),
			m_eAccApp
			);
	}
	else
	{
		sMsg.Format( SPHERE_TITLE ", Name=\"%s\", RegPass=\"%s\", Age=%i, Accounts=%d, Clients=%i, Items=%i, Chars=%i, Mem=%iK, Notes=\"%s\"" LOG_CR,
			(LPCTSTR) GetName(),
			(LPCTSTR) m_sRegisterPassword,
			GetAgeHours()/24,
			StatGet(SERV_STAT_ACCOUNTS),
			StatGet(SERV_STAT_CLIENTS),
			StatGet(SERV_STAT_ITEMS),
			StatGet(SERV_STAT_CHARS),
			StatGet(SERV_STAT_MEMORY)/1024,
			(LPCTSTR) m_sNotes
			);
	}
	return( sMsg );
}

CString CServer::GetStatusString( BYTE iIndex ) const
{
	// NOTE: The key names should match those in CServerDef::s_PropSet
	// A ping will return this as well.
	// 0 or 0x21 = main status.

	CString sMsg;

	switch ( iIndex )
	{
	case 0x21:	// '!'
		// typical (first time) poll response.
		{
			TCHAR szVersion[128];
			sMsg.Format( SPHERE_TITLE ", Name=\"%s\", Port=%d, Ver=%s, TZ=%d, EMail=%s, URL=%s, Lang=%s, CliVer=%s" LOG_CR,
				(LPCTSTR) GetName(),
				m_ip.GetPort(),
				SPHERE_VERSION,
				m_TimeZone,
				(LPCTSTR) m_sEMail,
				(LPCTSTR) m_sURL,
				(LPCTSTR) m_sLang,
				m_ClientVersion.WriteCryptVer(szVersion)
				);
		}
		break;
	case 0x22: // '"'
		// shown in the INFO page in game.
		sMsg.Format( SPHERE_TITLE ", Name=\"%s\", Age=%i, Clients=%i, Items=%i, Chars=%i, Mem=%iK" LOG_CR,
			(LPCTSTR) GetName(),
			GetAgeHours()/24,
			StatGet(SERV_STAT_CLIENTS),
			StatGet(SERV_STAT_ITEMS),
			StatGet(SERV_STAT_CHARS),
			StatGet(SERV_STAT_MEMORY)/1024
			);
		break;
	case 0x23:
	default:	// default response to ping.
		sMsg.Format( SPHERE_TITLE ", Name=%s, Age=%i, Ver=%s, TZ=%d, EMail=%s, URL=%s, Clients=%i" LOG_CR,
			(LPCTSTR) GetName(),
			GetAgeHours()/24,
			SPHERE_VERSION,
			m_TimeZone,
			(LPCTSTR) m_sEMail,
			(LPCTSTR) m_sURL,
			StatGet(SERV_STAT_CLIENTS)
			);
		break;
	case 0x24: // '$'
		// show at startup.
		sMsg.Format( "Admin=%s, URL=%s, Lang=%s, TZ=%d" LOG_CR,
			(LPCTSTR) m_sEMail,
			(LPCTSTR) m_sURL,
			(LPCTSTR) m_sLang,
			m_TimeZone
			);
		break;
	}

	return( sMsg );
}

//*********************************************************

void CServer::ListServers( CStreamText* pSrc ) const
{
	ASSERT( pSrc );

	for ( int i=0; true; i++ )
	{
		CServerLock pServ = g_Cfg.Server_GetDef(i);
		if ( pServ == NULL )
			break;
		pSrc->Printf( "%d:NAME=%s, STATUS=%s" LOG_CR, i,
			(LPCTSTR) pServ->GetName(), (LPCTSTR) pServ->GetStatus());
	}
}

void CServer::ListClients( CScriptConsole* pSrc ) const
{
	// Mask which clients we want ?
	// Give a format of what info we want to SHOW ?

	ASSERT( pSrc );
	CCharPtr pCharSrc = GET_ATTACHED_CCHAR(pSrc);

	for ( CClientPtr pClient = GetClientHead(); pClient; pClient = pClient->GetNext())
	{
		CSocketAddress PeerName = pClient->m_Socket.GetPeerName();

		CCharPtr pChar = pClient->GetChar();
		if ( pChar )
		{
			if ( pCharSrc &&
				! pCharSrc->CanDisturb( pChar ))
			{
				continue;
			}

			TCHAR chRank = '=';
			if ( pClient->IsPrivFlag(PRIV_GM) || pClient->GetPrivLevel() >= PLEVEL_Counsel )
			{
				chRank = ( pChar && pChar->Player_IsDND()) ? '*' : '+';
			}

			pSrc->Printf( "%x:Acc%c'%s', (%s) Char='%s',(%s)" LOG_CR,
				pClient->m_Socket.GetSocket(),
				chRank,
				(LPCTSTR) pClient->GetAccount()->GetName(),
				(LPCTSTR) PeerName.GetAddrStr(),
				(LPCTSTR) pChar->GetName(),
				(LPCTSTR) pChar->GetTopPoint().v_Get());
		}
		else
		{
			// Not an in-game char
			if ( pSrc->GetPrivLevel() < pClient->GetPrivLevel())
			{
				continue;
			}
			LPCTSTR pszState;
			switch ( pClient->m_ConnectType )
			{
			case CONNECT_TELNET:	pszState = "TelNet"; break;
			case CONNECT_HTTP:		pszState = "Web"; break;
			default: pszState = "NOT LOGGED IN"; break;
			}

			pSrc->Printf( "%x:Acc='%s', (%s) %s" LOG_CR,
				pClient->m_Socket.GetSocket(),
				( pClient->GetAccount()) ? (LPCTSTR) pClient->GetAccount()->GetName() : "<NA>",
				(LPCTSTR) PeerName.GetAddrStr(),
				(LPCTSTR) pszState );
		}
	}
}

void CServer::ListGMPages( CStreamText* pSrc ) const
{
	// NOT USED !
	ASSERT( pSrc );

	CGMPagePtr pPage = g_World.m_GMPages.GetHead();
	for ( ; pPage!= NULL; pPage = pPage->GetNext())
	{
		pSrc->Printf(
			"Account=%s (%s) Reason='%s' Time=%ld",
			(LPCTSTR) pPage->GetName(),
			(LPCTSTR) pPage->GetAccountStatus(),
			(LPCTSTR) pPage->GetReason(),
			pPage->m_timePage );
	}
}

bool CServer::OnConsoleCmd( CGString& sText, CScriptConsole* pSrc )
{
	// Get a command from the console or Telnet.
	// RETURN: 
	//  false = boot the client. (They are doing something bad)

	if ( sText.GetLength() <= 0 ) // just ignore this.
		return( true );

	if ( sText.GetLength() > 1 )
	{
		LPCTSTR pszText = sText;
		if ( g_Cfg.IsConsoleCmd( sText[0] ))
			pszText++;

		if ( ! g_Cfg.CanUsePrivVerb( this, pszText, pSrc ))
		{
			pSrc->Printf( "not privleged for command '%s'" LOG_CR, pszText );
		}
		else
		{
			CSphereExpContext exec( this, pSrc );
			HRESULT hRes = exec.ExecuteCommand( pszText );
			if ( IS_ERROR(hRes))
			{
				CGString sErr;
				sErr.FormatErrorMessage( hRes );
				pSrc->Printf( "command '%s' error '%s'" LOG_CR, (LPCSTR) pszText, (LPCSTR) sErr );
			}
		}
		sText.Empty();
		return( true );
	}

	CGVariant vValRet;

	// shorthand commands.
	switch ( toupper( sText[0] ))
	{
	case '?':
		pSrc->Printf(
			"Available Commands:" LOG_CR
			"# = Immediate Save world" LOG_CR
			"A = Accounts file update" LOG_CR
			"B message = Broadcast a message" LOG_CR
			"C = Clients List (%d)" LOG_CR
			"G = Garbage collection" LOG_CR
			"H = Hear all that is said (%s)" LOG_CR
			"I = Information" LOG_CR
			"L = Toggle log file (%s)" LOG_CR
			"P = Profile Info (%s)" LOG_CR
			"R = Resync Pause" LOG_CR
			"S = Secure mode toggle (%s)" LOG_CR
			"V = Verbose Mode (%s)" LOG_CR
			"X = immediate exit of the server" LOG_CR
			,
			m_Clients.GetCount(),
			g_Log.IsLoggedGroupMask( LOG_GROUP_PLAYER_SPEAK ) ? "ON" : "OFF",
			g_Log.IsFileOpen() ? "OPEN" : "CLOSED",
			m_Profile.IsProfilingActive() ? "ON" : "OFF",
			g_Cfg.m_fSecure ? "ON" : "OFF",
			g_Log.IsLogged( LOGL_TRACE ) ? "ON" : "OFF"
			);
		break;

	case 'H':	// Hear all said.
		{ CGVariant vTmpH("!"); s_Method( M_HearAll, vTmpH, vValRet, pSrc ); }
		break;
	case 'S': // Toggle
		{ CGVariant vTmpS("!"); s_Method( M_Secure, vTmpS, vValRet, pSrc ); }
		break;
	case 'L': // Turn the log file on or off.
		s_Method( M_Log, vValRet, vValRet, pSrc );
		break;
	case 'V':
		s_Method( M_Verbose, vValRet, vValRet, pSrc );
		break;
	case 'I':
		s_Method( M_Information, vValRet, vValRet, pSrc );
		break;
	case '#':
		// Start a syncronous save or finish a background save synchronously
		if ( g_Serv.m_fResyncPause )
		{
//do_resync:
			pSrc->WriteString( "Not allowed during resync pause. Use 'R' to restart." LOG_CR);
			break;
		}
		vValRet = 1;
		s_Method( M_Save, vValRet, vValRet, pSrc );
		break;

	case 'X':
		if (g_Cfg.m_fSecure)
		{
			pSrc->WriteString( "NOTE: Secure mode prevents keyboard exit!" LOG_CR );
		}
		else
		{
			g_Log.Event( LOG_GROUP_INIT|LOG_GROUP_DEBUG, LOGL_FATAL, "Immediate Shutdown initialized!" LOG_CR);
			SetExitFlag( SPHEREERR_CONSOLE_X );
		}
		break;
	case 'A':
		// Force periodic stuff
		g_Accounts.Account_SaveAll();
		g_Cfg.OnTick(true);
		break;
	case 'G':
		g_World.GarbageCollection();
		break;
	case 'C':
	case 'W':
		// List all clients on line.
		ListClients( pSrc );
		break;
	case 'R':
		// resync Pause mode. Allows resync of things in files.
		if ( g_World.IsSaving())
		{
//do_saving:
			// Is this really true ???
			pSrc->WriteString( "Not allowed during background worldsave. Use '#' to finish." LOG_CR);
			break;
		}
		SetResyncPause( !m_fResyncPause, pSrc );
		break;

	case 'P':
		// Display profile information.
		// ? Peer status. Show status of other servers we list.
		{
			pSrc->Printf( "Profiles %s: (%d sec total)" LOG_CR, m_Profile.IsProfilingActive() ? "ON" : "OFF", m_Profile.GetSampleWindowLen());
			for ( int i=0; i < PROFILE_QTY; i++ )
			{
				pSrc->Printf( "'%s'=%s" LOG_CR, (LPCTSTR) g_ProfileProps[i].m_pszName, (LPCTSTR) m_Profile.GetTaskStatusDesc(i));
			}
		}
		break;

#ifdef _DEBUG
	case '!':
		Debug_CheckPoint();
		break;
	case '^':	// hang forever. (intentionally)
		for(;;)
		{
		}
		break;
	case '@':	// cause null pointer error. (intentionally)
		{
			// Sphere_Exception_Win32
			BYTE* pData = NULL;
			BYTE data =* pData;
		}
		break;
#endif

	default:
		pSrc->Printf( "unknown command '%c'" LOG_CR, sText[0] );
		break;
	}

	sText.Empty();
	return( true );
}

//************************************************

CString CServer::GetModeDescription() const
{
	LPCSTR pszMode;
	switch ( m_iModeCode )
	{
	case SERVMODE_RestockAll:	// Major event.
		pszMode = "Restocking";
		break;
	case SERVMODE_Saving:		// Forced save freezes the system.
		pszMode = "Saving";
		break;
	case SERVMODE_ScriptBook:	// Run at Lower Priv Level
	case SERVMODE_Run:			// Game is up and running
		pszMode = "Running";
		break;
	case SERVMODE_Loading:		// Initial load.
		pszMode = "Loading";
		break;
	case SERVMODE_ResyncPause:
		pszMode = "Resync Pause";
		break;
	case SERVMODE_ResyncLoad:	// Loading after resync
		pszMode = "Resync Load";
		break;
	case SERVMODE_Exiting:		// Closing down
		pszMode = "Exiting";
		break;
	case SERVMODE_GarbageCollect:
	case SERVMODE_Test5:			// Sometest modes.
	case SERVMODE_Test8:			// Sometest modes.
		pszMode = "Testing";
		break;
	default:
		pszMode = "Unknown";
		break;
	}
	return pszMode;
}

void CServer::OnTriggerEvent( SERVTRIG_TYPE type, DWORD dwArg1, DWORD dwArg2 )
{
	// CSphereExpContext triggers on the server level.
	// use this to fire events to the COM layer stuff.

	g_ServConsole.OnTriggerEvent( type, dwArg1, dwArg2 );

	// Fire this event to all COM modules as well ?
}

HRESULT CServer::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	HRESULT hRes = g_Cfg.s_PropSet(pszKey, vVal);
	if ( hRes == NO_ERROR )
		return( NO_ERROR );
	hRes = g_World.s_PropSet(pszKey, vVal);
	if ( hRes == NO_ERROR )
		return( NO_ERROR );
	return( CServerDef::s_PropSet(pszKey, vVal));
}

HRESULT CServer::s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc )
{
	// Just do stats values for now.
	HRESULT hRes = g_Cfg.s_PropGet( pszKey, vVal, pSrc );
	if ( hRes == NO_ERROR )
		return( NO_ERROR );
	hRes = g_World.s_PropGet( pszKey, vVal, pSrc );
	if ( hRes == NO_ERROR )
		return( NO_ERROR );
	return( CServerDef::s_PropGet( pszKey, vVal, pSrc ));
}

void CServer::s_WriteProps( CScript &s )
{
	s.WriteSection( g_Cfg.GetResourceBlockName(RES_Sphere));
	s.WriteKey( "NAME", GetName());
	s_WriteServerData( s );
	g_Cfg.s_WriteProps(s);
}

HRESULT CServer::s_Method( int iProp, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	ASSERT(pSrc);
	switch (iProp)
	{
	case M_ProfileGet:
		if ( vArgs.MakeArraySize() < 1 )
			return( HRES_BAD_ARG_QTY );
		iProp = s_FindKeyInTable( vArgs, g_ProfileProps );
		if ( iProp < 0 )
			return( HRES_BAD_ARGUMENTS );
		vValRet = m_Profile.GetTaskStatusDesc(iProp);
		break;

	case M_AllClients:
		if ( vArgs.IsEmpty())
			return HRES_BAD_ARG_QTY;
		{
			// Send a verb to all clients
			CSphereExpContext exec( NULL, pSrc );
			CClientPtr pClient = g_Serv.GetClientHead();
			for ( ; pClient!=NULL; pClient = pClient->GetNext())
			{
				if ( pClient->GetChar() == NULL )
					continue;
				exec.SetBaseObject( REF_CAST(CChar,pClient->GetChar()));
				exec.ExecuteCommand( vArgs.GetPSTR());
			}
		}
		break;

	case M_B:
	case M_Broadcast: 
		g_World.Broadcast( vArgs );
		break;

	case M_BlockIP:
		vArgs.MakeArraySize();
		g_Cfg.SetLogIPBlock( vArgs.GetArrayPSTR(0), vArgs.GetArrayPSTR(1), pSrc );
		break;

#if 0 // def _DEBUG
	case M_Genocide:
		// Loop thru the whole world and kill all thse creatures.
		if ( !vArgs.IsEmpty())
		{
			WORD id = vArgs.GetInt();
		}
		break;
#endif

	case M_HearAll:	
		g_Log.SetLogGroupMask( vArgs.GetDWORDMask( g_Log.GetLogGroupMask(), LOG_GROUP_PLAYER_SPEAK ));
		pSrc->Printf( "Hear All %s." LOG_CR, g_Log.IsLoggedGroupMask(LOG_GROUP_PLAYER_SPEAK) ? "Enabled" : "Disabled" );
		break;

	case M_Information:
		pSrc->WriteString( GetStatusString( 0x22 ));
		pSrc->WriteString( GetStatusString( 0x24 ));
		break;

	case M_Export: 
		if ( pSrc == NULL || pSrc->GetPrivLevel() < PLEVEL_Admin )
			return( HRES_PRIVILEGE_NOT_HELD );
		if ( vArgs.IsEmpty())
			return( HRES_BAD_ARG_QTY );
		{
			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty < 1 )
				return HRES_BAD_ARG_QTY;

			// IMPFLAGS_ITEMS
			return g_World.Export( vArgs.GetArrayPSTR(0), 
				GET_ATTACHED_CCHAR(pSrc),
				(iArgQty>=2)? vArgs.GetArrayInt(1) : IMPFLAGS_ITEMS,
				(iArgQty>=3)? vArgs.GetArrayInt(2) : SHRT_MAX );
		}
		break;

	case M_Import: 
		if ( pSrc == NULL || pSrc->GetPrivLevel() < PLEVEL_Admin )
			return( HRES_PRIVILEGE_NOT_HELD );
		if ( vArgs.IsEmpty())
			return( HRES_BAD_ARG_QTY );
		{
			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty < 1 )
				return HRES_BAD_ARG_QTY;
			// IMPFLAGS_ITEMS
			return g_World.Import( vArgs.GetArrayPSTR(0),
				GET_ATTACHED_CCHAR(pSrc),
				(iArgQty>=2)? vArgs.GetArrayInt(1) : IMPFLAGS_BOTH,
				(iArgQty>=3)? vArgs.GetArrayInt(2) : SHRT_MAX );
		}
		break;
	case M_Load:
		// Load a resource file.
		if ( g_Cfg.LoadResourcesAdd( vArgs.GetPSTR()) == NULL )
			return( HRES_BAD_ARGUMENTS );
		break;

	case M_Log:	// "LOG" = Turn the log file on or off.
		if ( g_Log.IsFileOpen())
		{
			g_Log.Close();
			g_Log.m_fLockOpen = false;
		}
		else
		{
			g_Log.OpenLog();
		}
		pSrc->Printf( "Log file %s." LOG_CR, (LPCTSTR) ( g_Log.IsFileOpen() ? "opened" : "closed" ));
		break;

	case M_Respawn:
		g_World.RespawnDeadNPCs();
		break;

	case M_Restock:
		// set restock time of all vendors in World.
		// set the respawn time of all spawns in World.
		g_World.Restock();
		break;

	case M_Restore:	// "RESTORE" backupfile.SCP Account CharName
		if ( pSrc == NULL || pSrc->GetPrivLevel() < PLEVEL_Admin )
			return( HRES_PRIVILEGE_NOT_HELD );
		if ( vArgs.IsEmpty())
			return( HRES_BAD_ARG_QTY );
		{
			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty < 1 )
				return HRES_BAD_ARG_QTY;
			HRESULT hRes = g_World.Import( vArgs.GetArrayPSTR(0),
				GET_ATTACHED_CCHAR(pSrc),
				IMPFLAGS_BOTH|IMPFLAGS_ACCOUNT, 
				SHRT_MAX,
				vArgs.GetArrayPSTR(1), 
				vArgs.GetArrayPSTR(2));
			if ( hRes )
			{
				return hRes;
			}
			pSrc->WriteString( "Restore success" LOG_CR );
		}
		break;

	case M_Save: // "SAVE" x
		g_World.Save( vArgs.GetInt());
		break;
	case M_Secure:
	case M_Safe:
		if ( vArgs.IsEmpty())
		{
			vValRet.SetBool(g_Cfg.m_fSecure);
		}
		else
		{
			g_Cfg.m_fSecure = vArgs.GetDWORDMask( g_Cfg.m_fSecure, true );
			SetSignals();
			pSrc->Printf( "Secure mode %s." LOG_CR, g_Cfg.m_fSecure ? "re-enabled" : "disabled" );
		}
		break;

	case M_SaveIni:
		g_Cfg.SaveIni();
		break;
	case M_SaveStatics:
		// Save all statics in the world to the statics file.
		return g_World.SaveWorldStatics();

	case M_ListServers:
		ListServers( pSrc );
		break;
	case M_Shutdown:
		SetShutdownTime(( vArgs.IsEmpty()) ? 15 : vArgs.GetInt());
		break;

	case M_UnblockIP:
		g_Cfg.SetLogIPBlock( vArgs, NULL, pSrc );
		break;

	case M_SMsg:
	case M_SysMessage:
		WriteString( vArgs );
		break;

	case M_Verbose:
		{
			bool fSet;
			if ( vArgs.IsEmpty())
			{
				fSet = g_Log.IsLogged(LOGL_TRACE);
			}
			else
			{
				fSet = vArgs.GetBool();
			}
			g_Log.SetLogLevel((fSet) ? LOGL_EVENT : LOGL_TRACE );
			pSrc->Printf( "Verbose display %s." LOG_CR, (LPCTSTR) ( g_Log.IsLogged(LOGL_TRACE) ? "Enabled" : "Disabled" ));
		}
		break;

#if 0 // def _DEBUG
	case M_WEBREQ:
		// request a web page.
		DoWebRequest( vArgs.GetPSTR());
		break;

	case M_Win:
		// Open the console window.
		if ( vArgs.IsEmpty() || vArgs.GetInt())
		{
#ifdef _WIN32
			g_ServConsole.Init( NULL, NULL, 1 );
#else
			g_ServConsole.Init( NULL, 1 );
#endif
		}
		else
		{
			// Hide it ?
			g_ServConsole.Exit();
		}
		break;
#endif

	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}

HRESULT CServer::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp < 0 )
	{
		return( CServerDef::s_Method(pszKey, vArgs, vValRet, pSrc));
	}
	return s_Method( iProp, vArgs, vValRet, pSrc );
}

//*********************************************************

HRESULT CServer::CommandLineArg( TCHAR* pArg )
{
	// Console Command line.
	// This runs after script file enum but before loading the world file.
	// RETURN:
	//  true = keep running after this.
	//

	// NT Service switches:
	// -install			to install the service
	// -remove			to remove the service
	// -debug <params>	to run as a console application for debugging

	if ( pArg == NULL )
		return 0;
	if ( ! _IS_SWITCH( pArg[0] ))
		return 0;

	TCHAR ch = toupper( pArg[1] );
	switch ( ch )
	{
	case '?':
		WriteString( SPHERE_TITLE " " SPHERE_VERSION LOG_CR
			"Command Line Switches:" LOG_CR
			"-? This help list." LOG_CR
			"-INSTALL as NT service" LOG_CR
			"-Lfilename Load this file for the world" LOG_CR
			"-Nstring Set the sphere name." LOG_CR
			"-P# Set the port number." LOG_CR
			"-Tfilename Test this resource script" LOG_CR
			"-Ofilename Output console to this file name" LOG_CR
			"-Q Quit when finished." LOG_CR
			"-D0 Dump all scripts to a single file dumpall.txt" LOG_CR
			"-D1 Dump global variable DEFNAMEs to dumpdefs.txt" LOG_CR
			"-D2## Dump itemdata.mul with this flag mask to dumpitems.txt" LOG_CR
			"-D3 Dump the ground tiles database to dumpterrain.txt" LOG_CR
			"-D4 Dump a list of all char types to dumpchars.txt" LOG_CR
			"-4 Xref itemdata.mul with the scripts. list unscripted items." LOG_CR
			"-5 Xref DUPEITEM= to write item DUPELIST=" LOG_CR
			"-6 Xref char ANIM= defs with ANIM.IDX" LOG_CR
			);
		return( HRES_INTERNAL_ERROR );

	case 'I': // ini or i = Load alternate m_scpIni file.
		if ( ! strcmpi( pArg+1, "install" ))
			break;
		g_Cfg.m_scpIni.SetFilePath( pArg+2 ); // Open script file
		break;

	case 'P': // port or p
		// Set the port.
		m_ip.SetPort( atoi( pArg+2 ));
		break;
	case 'N': // Name or n
		// Set the system name.
		SetName( pArg+2 );
		break;
	case 'L':	// load a particular backup world file.
		if ( ! g_World.LoadAll( pArg+2 ))
			return( HRES_INTERNAL_ERROR );
		break;
	case 'O':	// output
		// put the log output here
		if ( g_Log.Open( pArg+2, OF_SHARE_DENY_WRITE|OF_READWRITE|OF_TEXT ))
		{
			g_Log.m_fLockOpen = true; // ignore further log controls.
		}
		break;
	case 'R':
		if ( ! strcmpi( pArg+1, "remove" ))
			break;
		break;
	case 'X': // Exit
	case 'Q':	// Quit
		// Exit the server.
		return( HRES_INTERNAL_ERROR );

	case 'D': 
		// dump things to a file.
		if ( ! strcmpi( pArg+1, "debug" ))
			break;
		if ( ! g_Cfg.ResourceDump( pArg+2 ))
			goto do_unrecognized;
		break;

	case 'T':	// Test
		// test a script file as best we can.
		SetServerMode( SERVMODE_Run );	// pretent we are not loading.
		g_Cfg.ResourceTest(pArg+2 );
		SetServerMode( SERVMODE_Loading );	// back to loading mode
		break;

	case '4':
		// Xref the RES_ItemDef blocks with the tiledata.mul 
		//  items database to make sure we have defined all.
		g_Cfg.ResourceTestItemMuls();
		break;

	case '5':
		// xref the DUPEITEM= stuff to match DUPELIST=aa,bb,cc,etc stuff
		SetServerMode( SERVMODE_Test5 );	// special mode.
		g_Cfg.ResourceTestItemDupes();
		SetServerMode( SERVMODE_Loading );
		return( HRES_INTERNAL_ERROR );

	case '6':
		// xref the ANIM= lines in RES_CharDef with ANIM.IDX
		g_Cfg.ResourceTestCharAnims();
		return( HRES_INTERNAL_ERROR );

#ifdef _DEBUG
	case '7':
		// read in all the CHARDEF and ITEMDEF tags from a SCP file and write them back out in proper order.
		g_Cfg.ResourceTestSort( pArg+2 );
		return( HRES_INTERNAL_ERROR );

	case '8':
		// Move the RESOURCE= and the TEST for skills in SPHERESKILL.SCP to proper ITEMDEF entries.
		SetServerMode( SERVMODE_Test8 );	// special mode.
		g_Cfg.ResourceTestSkills();
		SetServerMode( SERVMODE_Loading );
		return( HRES_INTERNAL_ERROR );
#endif

	default:
do_unrecognized:
		g_Log.Event( LOG_GROUP_INIT, LOGL_CRIT, "Don't recognize command line data '%s'" LOG_CR, pArg );
		break;
	}

	return( 0 );
}

void CServer::SetResyncPause( bool fPause, CScriptConsole* pSrc )
{
	ASSERT(pSrc);
	if ( fPause )
	{
		m_fResyncPause = true;
		pSrc->WriteString( "Server is PAUSED for Resync." LOG_CR );
		g_World.Broadcast( "Server is being PAUSED for Resync." );

		g_Cfg.Unload(true);
		SetServerMode( SERVMODE_ResyncPause );
	}
	else
	{
		pSrc->WriteString( "Resync Restart" LOG_CR );

		// Set all the SCP files to re-index.
		// Relock the SCP files.
		SetServerMode( SERVMODE_ResyncLoad );	// IsLoading()
		if ( ! g_Cfg.Load(true))
		{
			pSrc->WriteString( "Resync FAILED!" LOG_CR );
			g_World.Broadcast( "Resync FAILED!" );
		}
		else
		{
			pSrc->WriteString( "Resync Complete!" LOG_CR );
			g_World.Broadcast( "Resync Complete!" );
		}
		m_fResyncPause = false;
		SetServerMode( SERVMODE_Run );	// ready to go. ! IsLoading()
	}
}

CClientPtr CServer::FindClientAccount( const CAccount* pAccount ) const
{
	// find the client using this account ?
	for ( CClientPtr pClient = GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
	{
		if ( pClient->GetAccount() == pAccount )
			return pClient;
	}
	return NULL;
}

//*********************************************************

CClientPtr CServer::SocketsAccept( CGSocket& socket, bool fGod ) // Check for messages from the clients
{
	// Accept the incoming socket connection.

	CSocketAddress client_addr;
	CGSocket socknew;
	if ( ! socket.Accept( socknew, client_addr ))
	{
		// NOTE: Client_addr might be invalid.
		g_Log.Event( LOG_GROUP_CLIENTS, LOGL_FATAL, "Failed at client connection to '%s'(?)" LOG_CR, (LPCTSTR) client_addr.GetAddrStr());
		return NULL;
	}

	// Count clients from this ip ? 4 max ?
	// DOS attack = hog up all the connections !!!
	int iClientsOnIPCount = 0;
	CClientPtr pClient;
	for ( pClient = GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
	{
		CSocketAddress addr = pClient->m_Socket.GetPeerName();
		if ( addr.GetAddrIP() == client_addr.GetAddrIP())
			iClientsOnIPCount ++;
	}

	if ( iClientsOnIPCount > g_Cfg.m_iClientsPerIPMax )
	{
		g_Log.Event( LOG_GROUP_CLIENTS|LOG_GROUP_CHEAT, LOGL_ERROR, "Too many connects (%d) on ip '%s'" LOG_CR, iClientsOnIPCount, (LPCTSTR) client_addr.GetAddrStr());
		// kill it by allowing it to go out of scope.
		return NULL;
	}

	CLogIPPtr pLogIP = g_Cfg.m_LogIP.FindLogIP( client_addr, true );
	if ( pLogIP == NULL || pLogIP->IncPingBlock( true ))
	{
		// kill it by allowing it to go out of scope.
		return NULL;
	}

	SPHERE_LOG_NET("SocketsAccept: new client from %s (ip_count=%d)", (LPCTSTR) client_addr.GetAddrStr(), iClientsOnIPCount);
	return( new CClient( socknew.Detach()));
}

void CServer::SocketsReceive() // Check for messages from the clients
{
#ifndef _WIN32
	// Use poll() on Linux - more reliable than select() under qemu-user emulation.
	// Build an array of pollfd entries: god socket, main socket, then client sockets.
	// Use a reasonable max (64 should be more than enough for active clients + 2 server sockets)
	static const int MAX_POLL_FDS = 64;
	struct pollfd pollfds[MAX_POLL_FDS];
	int nfds = 0;

	// God socket
	if ( m_SocketGod.GetSocket() != INVALID_SOCKET )
	{
		pollfds[nfds].fd = m_SocketGod.GetSocket();
		pollfds[nfds].events = POLLIN;
		pollfds[nfds].revents = 0;
		nfds++;
	}
	int iGodIdx = (m_SocketGod.GetSocket() != INVALID_SOCKET) ? 0 : -1;

	// Main socket
	pollfds[nfds].fd = m_SocketMain.GetSocket();
	pollfds[nfds].events = POLLIN;
	pollfds[nfds].revents = 0;
	int iMainIdx = nfds;
	nfds++;

	// Client sockets - also clean up closed ones
	CClientPtr pClientNext;
	CClientPtr pClient = GetClientHead();
	for ( ; pClient!=NULL; pClient = pClientNext )
	{
		pClientNext = pClient->GetNext();
		if ( ! pClient->m_Socket.IsOpen())
		{
			pClient->DeleteThis();
			continue;
		}
		if ( nfds < MAX_POLL_FDS )
		{
			pollfds[nfds].fd = pClient->m_Socket.GetSocket();
			pollfds[nfds].events = POLLIN;
			pollfds[nfds].revents = 0;
			nfds++;
		}
	}

	// we task sleep in here. NOTE: this is where we give time back to the OS.
	m_Profile.SwitchTask( PROFILE_Idle );

	int ret = ::poll( pollfds, nfds, 1 ); // 1ms timeout
	if ( ret <= 0 )
	{
		return;
	}

	m_Profile.SwitchTask( PROFILE_NetworkRx );

	// Any events from clients ?
	for ( pClient = GetClientHead(); pClient!=NULL; pClient = pClientNext )
	{
		pClientNext = pClient->GetNext();
		if ( ! pClient->m_Socket.IsOpen())
		{
			pClient->DeleteThis();
			continue;
		}

		// Find this client's pollfd entry
		bool fReadable = false;
		SOCKET cliSock = pClient->m_Socket.GetSocket();
		for ( int i = 0; i < nfds; i++ )
		{
			if ( pollfds[i].fd == (int)cliSock && (pollfds[i].revents & POLLIN) )
			{
				fReadable = true;
				break;
			}
		}

		if ( fReadable )
		{
			if ( pClient->GetAccount())
			{
				// Update time of last comm.
				pClient->m_timeLastEvent.InitTimeCurrent();
			}
			if ( ! pClient->xRecvData())
			{
				try { pClient->DeleteThis(); } catch (...) {}
				continue;
			}
		}
		else
		{
			// NOTE: Not all CClient are game clients.

			int iLastEventDiff = pClient->m_timeLastEvent.GetCacheAge();

			if ( g_Cfg.m_iDeadSocketTime &&
				iLastEventDiff > g_Cfg.m_iDeadSocketTime &&
				(	pClient->m_ConnectType != CONNECT_TELNET )
				)
			{
				// We have not talked in several minutes.
				DEBUG_ERR(( "%x:Dead Socket Timeout" LOG_CR, pClient->m_Socket.GetSocket()));
				pClient->DeleteThis();
				continue;
			}
			if ( pClient->IsConnectTypePacket())	// packetized?
			{
				if ( iLastEventDiff > 1*60*TICKS_PER_SEC &&
					pClient->m_timeLastSend.GetCacheAge() > 1*60*TICKS_PER_SEC )
				{
					// Send a periodic ping to the client. If no other activity !
					pClient->addPing(0);
				}
			}
		}
	}

	// Any new connections ?
	if ( iGodIdx >= 0 && (pollfds[iGodIdx].revents & POLLIN) )
	{
		SocketsAccept( m_SocketGod, true );
	}
	if ( pollfds[iMainIdx].revents & POLLIN )
	{
		SocketsAccept( m_SocketMain, false );
	}

#else // _WIN32
	// Windows: use select() as before
	CGSocketSet readfds( m_SocketGod.GetSocket());
	readfds.Set( m_SocketMain.GetSocket());

	CClientPtr pClientNext;
	CClientPtr pClient = GetClientHead();
	for ( ; pClient!=NULL; pClient = pClientNext )
	{
		pClientNext = pClient->GetNext();
		if ( ! pClient->m_Socket.IsOpen())
		{
			pClient->DeleteThis();
			continue;
		}
		readfds.Set( pClient->m_Socket.GetSocket());
	}

	// we task sleep in here. NOTE: this is where we give time back to the OS.

	m_Profile.SwitchTask( PROFILE_Idle );

	timeval Timeout;	// time to wait for data.
	Timeout.tv_sec=0;
	Timeout.tv_usec=1000;	// micro seconds = 1/1000000
	int ret = ::select( readfds.GetNFDS(), readfds, NULL, NULL, &Timeout );
	if ( ret <= 0 )
	{
		return;
	}

	m_Profile.SwitchTask( PROFILE_NetworkRx );

	// Any events from clients ?
	for ( pClient = GetClientHead(); pClient!=NULL; pClient = pClientNext )
	{
		pClientNext = pClient->GetNext();
		if ( ! pClient->m_Socket.IsOpen())
		{
			pClient->DeleteThis();
			continue;
		}

		if ( readfds.IsSet( pClient->m_Socket.GetSocket()))
		{
			if ( pClient->GetAccount())
			{
				// Update time of last comm.
				// Only do this if the connection is logged in ?
				pClient->m_timeLastEvent.InitTimeCurrent();	// We should always get pinged every couple minutes or so
			}
#ifndef _WIN32
			extern volatile sig_atomic_t g_fSEGV_catch;
			extern sigjmp_buf g_SEGV_jmpbuf;
			g_fSEGV_catch = 1;
			if ( sigsetjmp(g_SEGV_jmpbuf, 1) != 0 )
			{
				g_fSEGV_catch = 0;
				SPHERE_LOG_ERR("SEGV in xRecvData — dropping client");
				try { pClient->DeleteThis(); } catch (...) {}
				continue;
			}
#endif
			if ( ! pClient->xRecvData())
			{
				try { pClient->DeleteThis(); } catch (...) {}
#ifndef _WIN32
				g_fSEGV_catch = 0;
#endif
				continue;
			}
#ifndef _WIN32
			g_fSEGV_catch = 0;
#endif
		}
		else
		{
			// NOTE: Not all CClient are game clients.

			int iLastEventDiff = pClient->m_timeLastEvent.GetCacheAge();

			if ( g_Cfg.m_iDeadSocketTime &&
				iLastEventDiff > g_Cfg.m_iDeadSocketTime &&
				(	pClient->m_ConnectType != CONNECT_TELNET )
				)
			{
				// We have not talked in several minutes.
				DEBUG_ERR(( "%x:Dead Socket Timeout" LOG_CR, pClient->m_Socket.GetSocket()));
				pClient->DeleteThis();
				continue;
			}
			if ( pClient->IsConnectTypePacket())	// packetized?
			{
				if ( iLastEventDiff > 1*60*TICKS_PER_SEC &&
					pClient->m_timeLastSend.GetCacheAge() > 1*60*TICKS_PER_SEC )
				{
					// Send a periodic ping to the client. If no other activity !
					pClient->addPing(0);
				}
			}
		}
	}

	// Any new connections ? what if there are several ?
	if ( readfds.IsSet( m_SocketGod.GetSocket()))
	{
		SocketsAccept( m_SocketGod, true );
	}
	if ( readfds.IsSet( m_SocketMain.GetSocket()))
	{
#ifndef _WIN32
		extern volatile sig_atomic_t g_fSEGV_catch;
		extern sigjmp_buf g_SEGV_jmpbuf;
		g_fSEGV_catch = 1;
		if ( sigsetjmp(g_SEGV_jmpbuf, 1) != 0 )
		{
			g_fSEGV_catch = 0;
			SPHERE_LOG_ERR("SEGV in SocketsAccept/CClient — recovered");
		}
		else
		{
			SocketsAccept( m_SocketMain, false );
			g_fSEGV_catch = 0;
		}
#else
		SocketsAccept( m_SocketMain, false );
#endif
	}
#endif // _WIN32
}

void CServer::SocketsFlush() // Sends ALL buffered data
{
	for ( CClientPtr pClient = GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
	{
		pClient->xFlush();
		pClient->addPause( false );	// always turn off pause here if it is on.
		pClient->xFlush();
	}
}

void CServer::OnTick()
{
	m_Profile.SwitchTask( PROFILE_Overhead );	// PROFILE_Resources

	OnTick_Busy();	// periodically give the console a tick.

	if ( g_ServConsole.IsCommandReady())	// window is on another thread ?
	{
		{ CGString sCmd = g_ServConsole.GetCommand(); OnConsoleCmd( sCmd, this ); }
	}

	SetValidTime();	// we are a valid game server.

	// Check clients for incoming packets.
	try
	{
		SocketsReceive();
	}
	catch (...)
	{
		g_Log.Event( LOG_GROUP_CLIENTS, LOGL_ERROR, "Exception in SocketsReceive" LOG_CR );
	}

	if ( ! IsLoading())
	{
		m_Profile.SwitchTask( PROFILE_Clients );

		for ( CClientPtr pClient = GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
		{
			if ( ! pClient->xHasData())
				continue;

			bool fRet = false;
			try
			{
				fRet = pClient->xDispatchMsg();
			}
			SPHERE_LOG_TRY_CATCH( "Server xDispatchMsg" )

			try
			{
				pClient->xFinishProcessMsg(fRet);
			}
			catch (...)
			{
				// Client cleanup failed
			}
		}
	}

	m_Profile.SwitchTask( PROFILE_NetworkTx );
	try { SocketsFlush(); } catch (...) {}
	g_Serv.m_Profile.SwitchTask( PROFILE_Overhead ); // PROFILE_Overhead

	if ( m_timeShutdown.IsTimeValid())
	{
		if ( m_timeShutdown.GetTimeDiff() <= 0 )
		{
			SetExitFlag( SPHEREERR_TIMED_CLOSE );
		}
		else if ( GetTimeSinceLastPoll() >= ( 60* TICKS_PER_SEC ))
		{
			SetShutdownTime(-1);	// cancel?
		}
	}

	g_Cfg.OnTick(false);
}

bool CServer::SocketsInit( CGSocket& socket, int iPort )
{
	// Initialize socket
	if ( ! socket.Socket())
	{
		g_Log.Event( LOG_GROUP_INIT, LOGL_FATAL, "Unable to create socket!" LOG_CR);
		return( false );
	}

	// getsockopt retrieve the value of socket option SO_MAX_MSG_SIZE.
	// If the data is too long to pass atomically through the underlying protocol,
	// the error WSAEMSGSIZE is returned, and no data is transmitted.

	m_uSizeMsgMax = 0;
	int iSize = sizeof(m_uSizeMsgMax);
	socket.GetSockOpt( SO_SNDBUF, &m_uSizeMsgMax, &iSize );	// SO_MAX_MSG_SIZE SO_MAX_MSG_SIZE

#ifdef _WIN32
	// blocking io.
	DWORD lVal = 1;	// 0 =  block
	bool fRet = socket.IOCtl( FIONBIO, &lVal );
	DEBUG_CHECK( fRet );
#endif
	// BOOL fon=1;
	// fRet = socket.SetSockOpt( SO_REUSEADDR, &fon, sizeof(fon));
	// DEBUG_CHECK( fRet );

	// Bind to just one specific port if they say so ?
	// If i'm behind a firewall , i want to give people an address diff than the one i bind to !
	CSocketAddress SockAddr( SOCKET_LOCAL_ADDRESS, iPort ); // m_ip

	if ( ! socket.Bind(SockAddr))
	{
		// Probably already a server running.
		// WSANOTINITIALISED, WSAEINVAL 
		g_Log.Event( LOG_GROUP_INIT, LOGL_FATAL, "Unable to bind listen socket %s port %d - Error code: %i" LOG_CR,
			(LPCTSTR) SockAddr.GetAddrStr(), iPort, socket.GetLastError());
		return( false );
	}

	if ( ! socket.Listen())
	{
	}

#if 0 // ndef _WIN32
	fRet = socket.IOCtl( F_SETFL, FNDELAY ); // to avoid blocking on failed accept()
	DEBUG_CHECK( fRet );
#endif

	return( true );
}

bool CServer::SocketsInit() // Initialize sockets
{
	SocketsInit( m_SocketGod, m_ip.GetPort()+1000 );
	if ( ! SocketsInit( m_SocketMain, m_ip.GetPort()))
		return( false );

	// What are we listing our port as to the world.
	// Tell the admin what we know.

	TCHAR szName[ 260 ];
	struct hostent* pHost = NULL;
	int iRet = gethostname( szName, sizeof( szName ));
	if ( iRet )
	{
		strcpy( szName, m_ip.GetAddrStr());
	}
	else
	{
		pHost = gethostbyname( szName );
		if ( pHost != NULL &&
			pHost->h_addr != NULL &&
			pHost->h_name &&
			pHost->h_name[0] )
		{
			strcpy( szName, pHost->h_name );
		}
	}

	g_Log.Event( LOG_GROUP_INIT, LOGL_TRACE, "Server started on '%s' port %d,%d." LOG_CR, szName, m_ip.GetPort(), m_ip.GetPort()+1000 );

	if ( ! iRet )
	{
		if ( pHost == NULL || pHost->h_addr == NULL )	// can't resolve the address.
		{
			g_Log.Event( LOG_GROUP_INIT, LOGL_CRIT, "gethostbyname does not resolve the address." LOG_CR );
		}
		else
		{
			for ( int j=0; pHost->h_aliases[j]; j++ )
			{
				g_Log.Event( LOG_GROUP_INIT, LOGL_TRACE, "Alias '%s'." LOG_CR, (LPCTSTR) pHost->h_aliases[j] );
			}
			// h_addrtype == 2
			// h_length = 4
			for ( int i=0; pHost->h_addr_list[i] != NULL; i++ )
			{
				CSocketAddressIP ip;
				ip.SetAddrIP( *((DWORD*)( pHost->h_addr_list[i] ))); // 0.1.2.3
				if ( ! m_ip.IsLocalAddr() && ! m_ip.IsSameIP( ip ))
				{
					continue;
				}
				g_Log.Event( LOG_GROUP_INIT, LOGL_TRACE, "Monitoring IP '%s'." LOG_CR, (LPCTSTR) ip.GetAddrStr());
			}
		}
	}

	SetServerMode( SERVMODE_Run );	// ready to go. ! IsLoading()
#ifdef _WIN32
	g_BackTask.CreateThread();
#else
	// Linux/QEMU: skip background thread to avoid threading issues.
	// Registration, polling, and mail sending are not needed for local operation.
#endif
	return( true );
}

void CServer::SocketsClose()
{
	m_Clients.DeleteAll();
	m_SocketMain.Close();
	m_SocketGod.Close();
}

bool CServer::Load()
{
	DEBUG_CHECK( IsLoading());

	// Keep track of the thread that is the parent.
	m_dwParentThread = CMainTask::GetCurrentThreadId();

#ifdef _WIN32
	if ( ! m_SocketMain.IsOpen())
	{
		WSADATA wsaData;
#if 0 // def _AFXDLL
		BOOL fRet = AfxSocketInit(&wsaData); 
		if (!fRet)
#else
		int err = WSAStartup( 0x0101, &wsaData );
		if (err)
#endif
		{
			g_Log.Event( LOG_GROUP_INIT, LOGL_FATAL, "Winsock 1.1 not found!" LOG_CR );
			return( false );
		}
		// if ( m_iClientsMax > wsaData.iMaxSockets-1 )
		//	m_iClientsMax = wsaData.iMaxSockets-1;
	}
#endif

#ifdef _DEBUG
	srand( 0 ); // regular randomizer.
#else
	srand( GetTickCount()); // Perform randomize
#endif

	SetSignals();

	g_MulInstall.FindInstall();
	Debug_CheckPoint();

	if ( ! g_Cfg.Load(false))
		return( false );

	Debug_CheckPoint();

	TCHAR szVersion[128];
	g_Log.Event( LOG_GROUP_INIT, LOGL_TRACE, _TEXT("ClientVersion=%s" LOG_CR), (LPCTSTR) m_ClientVersion.WriteCryptVer( szVersion ));
	if ( ! m_ClientVersion.IsValid())
	{
		g_Log.Event( LOG_GROUP_INIT, LOGL_FATAL, "Bad Client Version '%s'" LOG_CR, szVersion );
		return( false );
	}

	return( true );
}

//////////////////////////////////////////////////////////////////
// -CServTimeMaster

bool CServTimeMaster::AdvanceTime()
{
	// RETURN:
	//  true = time has changed measureably.

	DWORD dwTickCount = ::GetTickCount();	// get the system time.

	// On first call, m_dwTickCount may be 0 (uninitialized). Seed it to avoid
	// a massive time jump based on system uptime.
	if ( m_dwTickCount == 0 )
	{
		m_dwTickCount = dwTickCount;
		return false;
	}

	int iTimeSysDiff = dwTickCount - m_dwTickCount;
	iTimeSysDiff = IMULDIV( TICKS_PER_SEC, iTimeSysDiff, 1000 );

	if ( iTimeSysDiff <= 0 )	// assume this will happen sometimes.
	{
		if ( iTimeSysDiff == 0 ) // time is less than TICKS_PER_SEC
		{
			return false;
		}

		// This is normal. for daylight savings etc.
		m_dwTickCount = dwTickCount;
		// just wait til next cycle and we should be ok
		return false;
	}

	m_dwTickCount = dwTickCount;

	long Clock_New = GetTimeRaw() + iTimeSysDiff;

	// CServTime is signed !
	// NOTE: This will overflow after 7 or so years of run time !
	if ( Clock_New <= GetTimeRaw())	// should not happen! (overflow)
	{
		if ( GetTimeRaw() == Clock_New )
		{
			// Weird. This should never happen, but i guess it is harmless  ?!
			g_Log.Event( LOG_GROUP_DEBUG, LOGL_CRIT, "Clock corruption?" );
			return false;
		}

		// Someone has probably messed with the "TIME" value.
		// Very critical !
		g_Log.Event( LOG_GROUP_DEBUG, LOGL_CRIT, "Clock overflow, reset from 0%x to 0%x" LOG_CR, GetTimeRaw(), Clock_New );
		InitTime( Clock_New );	// this may cause may strange things.
		return false;
	}

	InitTime( Clock_New );
	return( true );
}


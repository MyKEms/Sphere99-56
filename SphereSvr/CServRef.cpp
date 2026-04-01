//
// CServerDef.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.

//////////////////////////////////////////////////////////////////////
// -CServerDef

const CScriptProp CServerDef::sm_Props[CServerDef::P_QTY+1] =	// static
{
#define CSERVERDEFPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "cserverdefprops.tbl"
#undef CSERVERDEFPROP
	NULL,
};

CSCRIPT_CLASS_IMP1(ServerDef,CServerDef::sm_Props,NULL,NULL,ResourceObj);

LPCTSTR const CServerDef::sm_AccAppTable[ ACCAPP_QTY+1 ] =
{
	"Closed",		// Closed. Not accepting more.
	"EmailApp",		// Must send email to apply.
	"Free",			// Anyone can just log in and create a full account.
	"GuestAuto",	// You get to be a guest and are automatically sent email with u're new password.
	"GuestTrial",	// You get to be a guest til u're accepted for full by an Admin.
	"Other",		// specified but other ?
	"Unspecified",	// Not specified.
	"WebApp",		// Must fill in a web form and wait for email response
	"WebAuto",		// Must fill in a web form and automatically have access
	"XGM",			// Everyone is a GM.
	NULL,
};

CServerDef::CServerDef( UID_INDEX rid, LPCTSTR pszName, CSocketAddressIP dwIP ) :
	CResourceDef(rid),
	m_ip( dwIP, SPHERE_DEF_PORT )	// SOCKET_LOCAL_ADDRESS
{
	// Statistics.
	memset( m_dwStat, 0, sizeof( m_dwStat ));	// THIS MUST BE FIRST !

	SetName( pszName );
	m_timeCreate.InitTimeCurrent();
	m_iClientsAvg = 0;

	// Set default time zone from UTC
	m_TimeZone = CGTime::GetTimeZoneOffset() / (60*60);	// Greenwich mean time.
	m_eAccApp = ACCAPP_Unspecified;
}

CServerDef::~CServerDef()
{
}

void CServerDef::SetName( LPCTSTR pszName )
{
	if ( ! pszName )
		return;

	// No HTML tags using <> either.
	TCHAR szName[ 2*MAX_ACCOUNT_NAME_SIZE ];
	int len = Str_GetBare( szName, pszName, sizeof(szName), "<>/\"\\" );
	if ( ! len )
		return;

	// allow just basic chars. No spaces, only numbers, letters and underbar.
	if ( g_Cfg.IsObscene( szName ))
	{
		DEBUG_ERR(( "Obscene server '%s' ignored." LOG_CR, szName ));
		return;
	}

	m_sName = szName;
}

void CServerDef::SetValidTime()
{
	m_timeLastValid.InitTimeCurrent();
}

int CServerDef::GetTimeSinceLastValid() const
{
	return( m_timeLastValid.GetCacheAge());
}

void CServerDef::SetPollTime()
{
	m_timeLastPoll.InitTimeCurrent();
}

int CServerDef::GetTimeSinceLastPoll() const
{
	return( m_timeLastPoll.GetCacheAge());
}

void CServerDef::ClientsAvgCalc()
{
	if ( StatGet(SERV_STAT_CLIENTS) > m_iClientsAvg )
	{
		m_iClientsAvg = StatGet(SERV_STAT_CLIENTS);
	}
}

void CServerDef::ClientsAvgReset()
{
	// Match the avg closer to the real current count.

#if 1
	int iClientsNew = StatGet(SERV_STAT_CLIENTS);
	if ( iClientsNew < m_iClientsAvg )
	{
		iClientsNew = (( m_iClientsAvg * 3 ) + ( iClientsNew )) / 4;
	}
	m_iClientsAvg = iClientsNew;
#endif

}

void CServerDef::addToServersList( CUOCommandServer& cmdsrv, int index ) const
{
	// Add myself to the server list.
	cmdsrv.m_count = index;

	// pad zeros to length.
	strcpylen( cmdsrv.m_servname, GetName(), sizeof(cmdsrv.m_servname));

	cmdsrv.m_zero32 = 0;
	cmdsrv.m_percentfull = StatGet(SERV_STAT_CLIENTS);
	cmdsrv.m_timezone = m_TimeZone;	// SPHERE_TIMEZONE

	DWORD dwAddr = m_ip.GetAddrIP();
	cmdsrv.m_ip[3] = ( dwAddr >> 24 ) & 0xFF;
	cmdsrv.m_ip[2] = ( dwAddr >> 16 ) & 0xFF;
	cmdsrv.m_ip[1] = ( dwAddr >> 8  ) & 0xFF;
	cmdsrv.m_ip[0] = ( dwAddr       ) & 0xFF;
}

HRESULT CServerDef::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceObj::s_PropSet(pszKey, vVal));
	}

	switch ( iProp )
	{
	case P_AccApp:
	case P_AccApps:
		// Treat it as a value or a string.
		if ( vVal.IsNumeric())
		{
			m_eAccApp = (ACCAPP_TYPE) vVal.GetInt();
		}
		else
		{
			// Treat it as a string. "Manual","Automatic","Guest"
			m_eAccApp = (ACCAPP_TYPE) FindTableSorted( vVal.GetPSTR(), sm_AccAppTable, COUNTOF( sm_AccAppTable )-1 );
		}
		if ( m_eAccApp < 0 || m_eAccApp >= ACCAPP_QTY )
			m_eAccApp = ACCAPP_Unspecified;
		break;
	case P_Age: // ignore this. (age in days)
		break;
	case P_ClientsAvg:
		{
			m_iClientsAvg = vVal.GetInt();
			if ( m_iClientsAvg < 0 )
				m_iClientsAvg = 0;
			if ( m_iClientsAvg > FD_SETSIZE )	// Number is bugged !
				m_iClientsAvg = FD_SETSIZE;
		}
		break;
	case P_ClientVersion:
	case P_ClientVer:
	case P_CliVer:
		m_ClientVersion.SetCryptVer( vVal.GetPSTR());
		break;
	case P_Create:
		m_timeCreate.InitTimeCurrent( vVal.GetInt()* TICKS_PER_SEC );
		break;
	case P_Admin:
	case P_AdminEmail:
	case P_Email:
	case P_EmailLink:
		if ( this != &g_Serv &&
			! g_Serv.m_sEMail.IsEmpty() &&
			strstr( (LPCTSTR)vVal.GetPSTR(), (LPCTSTR)g_Serv.m_sEMail ))
			return( HRES_BAD_ARGUMENTS );
		if ( ! CMailSMTP::IsValidEmailAddressFormat(vVal))
			return( HRES_BAD_ARGUMENTS );
		if ( CAccount::CheckBlockedEmail(vVal))
			return HRES_INVALID_HANDLE;
		if ( g_Cfg.IsObscene(vVal))	// Is the name unacceptable?
			return( HRES_BAD_ARGUMENTS );
		m_sEMail = vVal.GetPSTR();
		break;
	case P_Lang:
		{
			TCHAR szLang[ 32 ];
			int len = Str_GetBare( szLang, vVal, sizeof(szLang), "<>/\"\\" );
			if ( g_Cfg.IsObscene(szLang))	// Is the name unacceptable?
				return( HRES_BAD_ARGUMENTS );
			m_sLang = szLang;
		}
		break;
	case P_LastPollTime:
		m_timeLastPoll.InitTimeCurrent( vVal.GetInt() * TICKS_PER_SEC );
		break;
	case P_LastValidDate:
		m_dateLastValid.Read( vVal );
		break;
	case P_LastValidTime:
		{
			int iVal = vVal.GetInt() * TICKS_PER_SEC;
			if ( iVal < 0 )
				m_timeLastValid.InitTimeCurrent( iVal );
			else
				m_timeLastValid.InitTimeCurrent( - iVal );
		}
		break;
	case P_Notes:
		// Make sure this is not too long !
		// make sure there are no bad HTML tags in here ?
		{
			TCHAR szTmp[256];
			int len = Str_GetBare( szTmp, vVal.GetPSTR(), COUNTOF(szTmp), "<>/" );	// no tags
			if ( g_Cfg.IsObscene( szTmp ))	// Is the name unacceptable?
				return( HRES_BAD_ARGUMENTS );
			m_sNotes = szTmp;
		}
		break;
	case P_RegPass:
		m_sRegisterPassword = vVal.GetPSTR();
		break;
	case P_Ip:
	case P_ServIP:
		m_ip.SetHostPortStr( vVal.GetPSTR());
		break;

	case P_Name:
	case P_ServName:
		SetName( vVal.GetPSTR());
		break;
	case P_Port:
	case P_ServPort:
		m_ip.SetPort( vVal.GetInt());
		break;

	case P_Accounts:
	case P_StatAccounts:
		SetStat( SERV_STAT_ACCOUNTS, vVal.GetInt());
		break;
	case P_Allocs:
	case P_StatAllocs:
		SetStat( SERV_STAT_ALLOCS, vVal.GetInt());
		break;
	case P_Clients:
	case P_StatClients:
		{
			int iClients = vVal.GetInt();
			if ( iClients < 0 )
				return( HRES_BAD_ARGUMENTS );	// invalid
			if ( iClients > FD_SETSIZE )	// Number is bugged !
				return( HRES_BAD_ARGUMENTS );
			SetStat( SERV_STAT_CLIENTS, iClients );
			if ( iClients > m_iClientsAvg )
				m_iClientsAvg = StatGet(SERV_STAT_CLIENTS);
		}
		break;
	case P_Items:
	case P_StatItems:
		SetStat( SERV_STAT_ITEMS, vVal.GetInt());
		break;
	case P_Mem:
	case P_StatMemory:
		SetStat( SERV_STAT_MEMORY, vVal.GetInt() * 1024 );
		break;
	case P_Chars:
	case P_StatChars:
		SetStat( SERV_STAT_CHARS, vVal.GetInt());
		break;
	case P_Status:
		return( ParseStatus( vVal.GetPSTR(), true ));
	case P_TimeZone:
	case P_TZ:
		m_TimeZone = vVal.GetInt();
		break;
	case P_URL:
	case P_URLLink:
		// It is a basically valid URL ?
		if ( this != &g_Serv )
		{
			if (! g_Serv.m_sURL.IsEmpty() &&
				strstr( vVal.GetPSTR(), g_Serv.m_sURL ))
				return( HRES_BAD_ARGUMENTS );
		}
		if ( g_Cfg.m_sMainLogServerDir.IsEmpty() || this != &g_Serv )
		{
			if ( ! _strnicmp( vVal.GetPSTR(), "www.menasoft.", 13 ))
				return( HRES_BAD_ARGUMENTS );
			if ( ! _strnicmp( vVal.GetPSTR(), "www.sphereserver.", 17 ))
				return( HRES_BAD_ARGUMENTS );
		}
		if ( ! strchr( vVal.GetPSTR(), '.' ))
			return( HRES_BAD_ARGUMENTS );
		if ( g_Cfg.IsObscene( vVal.GetPSTR()))	// Is the name unacceptable?
			return( HRES_BAD_ARGUMENTS );
		m_sURL = vVal.GetPSTR();
		break;

	case P_Valid:
		// Force it to be valid. (not sure why i'd want to do this)
		SetValidTime();
		m_dateLastValid.InitTimeCurrent();
		m_timeLastPoll = (vVal.GetInt()) ? m_timeLastValid : (CServTime)CServTime::GetCurrentTime() ;
		break;

	case P_Ver:
	case P_Version:
		{
			TCHAR szVer[16];
			int len = Str_GetBare( szVer, vVal.GetPSTR(), sizeof(szVer), "<>/\"\\" );
			if ( g_Cfg.IsObscene(szVer))	// Is the name unacceptable?
				return( HRES_BAD_ARGUMENTS );
			m_sServVersion = szVer;
		}
		break;

	default:
		DEBUG_CHECK(0);
		return HRES_INTERNAL_ERROR;
	}
	return( NO_ERROR );
}

HRESULT CServerDef::s_PropGet( int iProp, CGVariant& vValRet, CScriptConsole* pSrc )
{
	switch ( iProp )
	{
	case P_AccApp:
		vValRet.SetInt( m_eAccApp );
		break;
	case P_AccApps:
		// enum string
		ASSERT( m_eAccApp >= 0 && m_eAccApp < ACCAPP_QTY );
		vValRet = sm_AccAppTable[ m_eAccApp ];
		break;
	case P_AdminEmail:
	case P_Admin:
	case P_Email:
		vValRet = m_sEMail;
		break;
	case P_Age:
		// display the age in days.
		vValRet.SetInt( GetAgeHours()/24 );
		break;
	case P_Allocs:
	case P_StatAllocs:
		vValRet.SetInt( StatGet( SERV_STAT_ALLOCS ));
		break;
	case P_ClientsAvg:
		vValRet.SetInt( GetClientsAvg());
		break;
	case P_ClientVersion:
	case P_ClientVer:
	case P_CliVer:
		{
			TCHAR szVersion[ 128 ];
			vValRet = m_ClientVersion.WriteCryptVer( szVersion );
		}
		break;
	case P_Create:
		vValRet.SetInt( m_timeCreate.GetTimeDiff() / TICKS_PER_SEC );
		break;
	case P_EmailLink:
		if ( m_sEMail.IsEmpty())
		{
			break;
		}
		vValRet.SetStrFormat( "<a href=\"mailto:%s\">%s</a>", (LPCTSTR) m_sEMail, (LPCTSTR) m_sEMail );
		break;

	case P_Lang:
		vValRet = m_sLang;
		break;

	case P_LastPollTime:
		vValRet.SetInt( m_timeLastPoll.IsTimeValid() ? ( m_timeLastPoll.GetTimeDiff() / TICKS_PER_SEC ) : -1 );
		break;
	case P_LastValidDate:
		// if ( m_dateLastValid.IsValid())
		// vValRet = m_dateLastValid.Write();

		if ( m_timeLastValid.IsTimeValid() )
			vValRet.SetInt( GetTimeSinceLastValid() / ( TICKS_PER_SEC * 60 ));
		else
			vValRet = "NA";
		break;
	case P_LastValidTime:
		// How many seconds ago.
		vValRet.SetInt( m_timeLastValid.IsTimeValid() ? ( GetTimeSinceLastValid() / TICKS_PER_SEC ) : -1 );
		break;

	case P_Notes:
		vValRet = m_sNotes;
		break;
	case P_RegPass:
		if ( pSrc == NULL || pSrc->GetPrivLevel() < PLEVEL_Admin )
			return( HRES_PRIVILEGE_NOT_HELD );
		vValRet = m_sRegisterPassword;
		break;
	case P_Ip:
	case P_ServIP:
		vValRet = m_ip.GetAddrStr();
		break;
	case P_Name:
	case P_ServName:
		vValRet = GetName();	// What the name should be. Fill in from ping.
		break;
	case P_Port:
	case P_ServPort:
		vValRet.SetInt( m_ip.GetPort());
		break;
	case P_Accounts:
	case P_StatAccounts:
		vValRet.SetInt( StatGet( SERV_STAT_ACCOUNTS ));
		break;
	case P_Clients:
	case P_StatClients:
		vValRet.SetInt( StatGet( SERV_STAT_CLIENTS ));
		break;
	case P_Items:
	case P_StatItems:
		vValRet.SetInt( StatGet( SERV_STAT_ITEMS ));
		break;
	case P_Mem:
	case P_StatMemory:
		vValRet.SetInt( StatGet( SERV_STAT_MEMORY )/1024 );
		break;
	case P_Chars:
	case P_StatChars:
		vValRet.SetInt( StatGet( SERV_STAT_CHARS ));
		break;
	case P_Status:
		vValRet = m_sStatus;
		break;
	case P_TimeZone:
	case P_TZ:
		vValRet.SetInt( m_TimeZone );
		break;
	case P_URL:
		vValRet = m_sURL;
		break;
	case P_URLLink:
		// try to make a link of it.
		if ( m_sURL.IsEmpty())
		{
			vValRet = GetName();
			break;
		}
		vValRet.SetStrFormat( "<a href=\"http://%s\">%s</a>", (LPCTSTR) m_sURL, (LPCTSTR) GetName() );
		break;
	case P_Valid:
		vValRet.SetBool( GetConnectStatus() <= 1 );
		break;
	case P_Ver:
	case P_Version:
		vValRet = m_sServVersion;
		break;
	default:
		DEBUG_CHECK(0);
		return HRES_INTERNAL_ERROR;
	}
	return( NO_ERROR );
}

HRESULT CServerDef::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceObj::s_PropGet( pszKey, vValRet, pSrc ));
	}
	return s_PropGet( iProp, vValRet, pSrc );
}

void CServerDef::s_WriteServerData( CScript& s )
{
	if ( ! m_ip.IsLocalAddr())
	{
		s.WriteKey( "IP", m_ip.GetAddrStr());
	}
	if ( m_ip.GetPort() != SPHERE_DEF_PORT )
	{
		s.WriteKeyInt( "PORT", m_ip.GetPort());
	}
	if ( m_TimeZone != ( CGTime::GetTimeZoneOffset() / (60*60)))
	{
		s.WriteKeyInt( "TZ", m_TimeZone );
	}
	if ( ! m_sURL.IsEmpty())
	{
		s.WriteKey( "URL", m_sURL );
	}
	if ( ! m_sEMail.IsEmpty())
	{
		s.WriteKey( "EMAIL", m_sEMail );
	}
	if ( ! m_sRegisterPassword.IsEmpty())
	{
		s.WriteKey( "REGPASS", m_sRegisterPassword );
	}
	if ( ! m_sNotes.IsEmpty())
	{
		s.WriteKey( "NOTES", m_sNotes );
	}
	if ( ! m_sLang.IsEmpty())
	{
		s.WriteKey( "LANG", m_sLang );
	}
	if ( m_eAccApp != ACCAPP_Unspecified )
	{
		ASSERT( m_eAccApp >= 0 && m_eAccApp < ACCAPP_QTY );
		s.WriteKey( "ACCAPPS", sm_AccAppTable[ m_eAccApp ] );
	}

	// Statistical stuff about other server.
	if ( this != &g_Serv )
	{
		s.WriteKeyInt( "CREATE", m_timeCreate.GetTimeDiff() / TICKS_PER_SEC );
		s.WriteKeyInt( "LASTVALIDTIME", GetTimeSinceLastValid() / TICKS_PER_SEC );
		s.WriteKey( "LASTVALIDDATE", m_dateLastValid.Format(CTIME_FORMAT_DEFAULT));
		if ( StatGet(SERV_STAT_ACCOUNTS))
		{
			s.WriteKeyInt( "ACCOUNTS", StatGet(SERV_STAT_ACCOUNTS) );
		}
		if ( m_iClientsAvg )
		{
			s.WriteKeyInt( "CLIENTSAVG", m_iClientsAvg );
		}
		if ( ! m_sServVersion.IsEmpty())
		{
			s.WriteKey( "VER", m_sServVersion );
		}
	}
}

void CServerDef::s_WriteCreated( CScript& s )
{
	if ( ! m_timeCreate.IsTimeValid() )
		return;
	s.WriteSection( "SERVER %s", (LPCTSTR) GetName() );
	s_WriteServerData(s);
}

HRESULT CServerDef::ParseStatus( LPCTSTR pszStatus, bool fStore )
{
	// Take the status string we get from the server and interpret it.
	// fStore = set the Status msg.

	ASSERT( this != &g_Serv );
	m_dateLastValid.InitTimeCurrent();
	SetValidTime();	// this server seems to be alive.

	TCHAR bBareData[ CSTRING_MAX_LEN ];
	int len = Str_GetBare( bBareData, pszStatus, sizeof(bBareData)-1 );
	if ( ! len )
	{
		return HRES_BAD_ARG_QTY;
	}

	CSphereScriptContext ScriptContext( &g_Cfg.m_scpIni ); // for error reporting.

	// Parse the data we get. NOTE: Older versions did not have , delimiters
	TCHAR* pData = bBareData;
	while ( pData )
	{
		TCHAR* pEquals = strchr( pData, '=' );
		if ( pEquals == NULL )
			break;

		// Start of argument.
		*pEquals = '\0';
		pEquals++;

		TCHAR* pszKey = strrchr( pData, ' ' );	// find start of key. reversed
		if ( pszKey == NULL )
			pszKey = pData;
		else
			pszKey++;

		if ( pEquals[0] == '"' )
		{
			pEquals++;
			pData = strchr( pEquals, '"' );
			if ( pData == NULL )
				break;
			*pData = '\0';
			pData++;
			if ( pData[0] == ',' )
				pData ++;
		}
		else
		{
			pData = strchr( pEquals, ',' );
			if ( pData )
			{
				TCHAR* pEnd = pData;
				pData ++;
				while ( ISWHITESPACE( pEnd[-1] ))
					pEnd--;
				*pEnd = '\0';
			}
		}

		{ CGVariant vTmp(pEquals); s_PropSet( pszKey, vTmp ); }
	}

	if ( fStore )
	{
		m_sStatus = "OK";
	}

	return( NO_ERROR );
}

int CServerDef::GetAgeHours() const
{
	// This is just the amount of time it has been listed.
	return( m_timeCreate.GetCacheAge() / ( TICKS_PER_SEC * 60 * 60 ));
}

int CServerDef::GetConnectStatus() const
{
	// Should this appear in the list ?	
	// 0 = connected recently. (valid)
	// 1 = connected not that long ago. (put in the list)
	// 2 = connected too long ago. (do not show in list)
	// 3 = has no connect very long (toss this server)

	if ( this == &g_Serv )	// we are always a valid server.
		return( 0 );

	if ( g_Cfg.m_iPollServers )
	{
		if ( m_timeLastValid.IsTimeValid() && ( m_timeLastPoll <= m_timeLastValid ))
			return 0;
	}
	else
	{
		if ( g_Cfg.m_sMainLogServerDir.IsEmpty())
			return 1;
	}

	if ( GetTimeSinceLastValid() <= g_Cfg.m_iServerValidListTime )
		return 1;

	// Should this server be listed at all ?
	// Drop it from the list ?

	if ( ! m_timeCreate.IsTimeValid())
		return( 2 ); // Never delete this. it was defined in the *.INI file
	if ( ! m_timeLastValid.IsTimeValid() && ! m_timeLastPoll.IsTimeValid())
		return( 2 );	// it was defined in the *.INI file. but has not updated yet

	// m_iServerValidListTime
	// Give the old reliable servers a break.
	DWORD dwAgeHours = GetAgeHours();

	// How long has it been down ?
	DWORD dwInvalidHours = GetTimeSinceLastValid() / ( TICKS_PER_SEC * 60 * 60 );

	if ( dwInvalidHours <= (7*24) + ( dwAgeHours/24 ) * 6 )
		return 2;

	return 3;	// Dump this server.
}

void CServerDef::SetStatusFail( LPCTSTR pszTrying )
{
	ASSERT(pszTrying);
	m_sStatus.Format( "Failed: %s: down for %d minutes",
		pszTrying,
		GetTimeSinceLastValid() / ( TICKS_PER_SEC * 60 ));
}

bool CServerDef::PollStatus()
{
	// Poll for the status of this server.
	// Try to ping the server to find out what it's status is.
	// NOTE:
	//   This is a synchronous function that could take a while. do in new thread.
	// RETURN:
	//  false = failed to respond

	ASSERT( this != &g_Serv );

	bool fWasUpToDate = ( GetConnectStatus() <= 1 );
	SetPollTime();		// record that i tried to connect.

	CGSocket sock;
	if ( ! sock.Socket())
	{
		SetStatusFail( "Socket" );
		return( false );
	}
	if ( ! sock.ConnectAddr( m_ip ))
	{
		SetStatusFail( "Connect" );
		return( false );
	}

	char bData = 0x21;	// GetStatusString arg data.
	if ( fWasUpToDate && Calc_GetRandVal( 16 ))
	{
		// just ask it for stats. don't bother with header info.
		bData = 0x23;
	}

	// got a connection
	// Ask for the data.
	if ( sock.Send( &bData, 1 ) != 1 )
	{
		SetStatusFail( "Send" );
		return( false );
	}

	// Wait for some sort of response.
	CGSocketSet readfds( sock.GetSocket());

	timeval Timeout;	// time to wait for data.
	Timeout.tv_sec=10;	// wait for 10 seconds for a response.
	Timeout.tv_usec=0;
	int ret = ::select( readfds.GetNFDS(), readfds, NULL, NULL, &Timeout );
	if ( ret <= 0 )
	{
		SetStatusFail( "No Response" );
		return( false );
	}

	// Any events from clients ?
	if ( ! readfds.IsSet( sock.GetSocket()))
	{
		SetStatusFail( "Timeout" );
		return( false );
	}

	// No idea how much data i really have here.
	char bRetData[ CSTRING_MAX_LEN ];
	int len = sock.Receive( bRetData, sizeof( bRetData ) - 1 );
	if ( len <= 0 )
	{
		SetStatusFail( "Receive" );
		return( false );
	}

	CServerLock lock( this );
	HRESULT hRes = ParseStatus( bRetData, bData == 0x21 );
	if ( IS_ERROR(hRes))
	{
		SetStatusFail( "Bad Status Data" );
		return( false );
	}

	return( true );
}

bool CServerDef::QueueCharToServer( CChar* pChar )
{
	// Move this char to this server.
	// Or at least try to.
	// RETURN:
	//  false = this server was not responding last i checked.

	// Thread lock myself.
	CServerLock lock( this );
	if ( pChar )
	{
		// Does not matter if we are already queued.
		// Set up this char to be moved to the remote server.
		m_MoveChars.AttachObj( pChar );

		// we should freeze the char or something to hold them from doing bad stuff in the mean time.
	}
	else
	{
		// anything in the queue now ?
		if ( ! m_MoveChars.GetSize())
			return false;
	}

	// Make sure the mover thread is started.
	if ( m_TaskCharMove.IsActive())
		return true;
	if ( g_Serv.IsLoading())	// can't do this just now.
		return true;
	m_TaskCharMove.CreateThread( MoveCharEntryProc, this );
	return true;
}

int CServerDef::MoveCharToServer( CGSocket &sock, CChar* pChar )
{
	// Delete the char off this server ? or just hold it till later ?
	// RETURN:
	//  0 = ok,
	//  1 = char was rejected.
	//  2 = failure of some sort.

#if 0
	if ( pChar->IsClient())
	{
		// Send a message to the game thread to tell the client to move to it's new server !
		// If they have logged off or don't get it. no big deal.

		pChar->GetClient()->addRelay( this );
	}

#endif

	return( 0 );
}

bool CServerDef::MoveCharsToServer()
{
	// Move any queued chars to the server.
	// This is done on a seperate thread.

	ASSERT( this != &g_Serv );

	CServerLock lock( this );
	m_TaskCharMove.InitInstance();
	ASSERT(m_MoveChars.GetSize());

	SetPollTime();	// record that i tried to connect.

	CGSocket sock;
	if ( ! sock.Socket())
	{
		SetStatusFail( "Socket" );
		return( false );
	}

	lock.ReleaseRefObj();	// release the ref while we are waiting to connect.
	if ( ! sock.ConnectAddr( m_ip ))
	{
		SetStatusFail( "Connect" );
		return( false );
	}

	// Now log into the remote server.

	lock.SetRefObj( this );

	int iRet = 0;

	while ( ! g_Serv.IsLoading() && m_MoveChars.GetSize())
	{
		CCharPtr pChar = g_World.CharFind( m_MoveChars.GetAt(0));
		if ( pChar != NULL )
		{
			iRet = MoveCharToServer( sock, pChar );
			if ( iRet )
			{
				// not responding i would assume ?
				// or the char was just not accepted?
				break;
			}
		}
		m_MoveChars.RemoveAt(0);
	}

	if ( iRet <= 1 )
	{
		m_dateLastValid.InitTimeCurrent();
		SetValidTime();	// this server seems to be alive.
		m_sStatus = "OK";
	}

	m_TaskCharMove.ExitInstance();
	return( true );
}

THREAD_ENTRY_RET _cdecl CServerDef::MoveCharEntryProc( void* lpThreadParameter ) // static
{
	CServerLock pServ = (CServerDef *) lpThreadParameter;
	pServ->MoveCharsToServer();
}


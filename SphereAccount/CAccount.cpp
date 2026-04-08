//
// CAccount.cpp
// Copyright Menace Software (www.menasoft.com).
//

#include "stdafx.h"	// predef header.
#include "caccountbase.h"
#include "caccount.h"

extern "C"
{
	void globalstartsymbol()	// put this here as just the starting offset.
	{
	}
	const BYTE globalstartdata = 0xff;
}

//**********************************************************************
// -CAccountConsole

CGString CAccountConsole::GetName() const	// name of the console.
{
	if ( m_pAccount == NULL )
		return( TEXT("NA"));
	return m_pAccount->GetName();
}

int CAccountConsole::GetPrivLevel() const	// What privs do i have ? PLEVEL_TYPE Higher = more.
{
	// PLEVEL_TYPE
	if ( m_pAccount == NULL )
		return( PLEVEL_NoAccount );
	return m_pAccount->GetPrivLevel();
}

bool CAccountConsole::IsPrivFlag( WORD wPrivFlags ) const
	{	
		// PRIV_GM flags
		if ( m_pAccount == NULL )
			return( false );	// NPC's have no privs.
		return( m_pAccount->IsPrivFlag( wPrivFlags ));
	}

void CAccountConsole::SetPrivFlags( WORD wPrivFlags )
	{
		if ( m_pAccount == NULL )
			return;
		m_pAccount->SetPrivFlags( wPrivFlags );
	}

void CAccountConsole::ClearPrivFlags( WORD wPrivFlags )
	{
		if ( m_pAccount == NULL )
			return;
		m_pAccount->ClearPrivFlags( wPrivFlags );
	}

//**********************************************************************
// -CAccount

const CScriptProp CAccount::sm_Props[CAccount::P_QTY+1] = // static
{
#define CACCOUNTPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "caccountprops.tbl"
#undef CACCOUNTPROP
	NULL,
};

#ifdef USE_JSCRIPT
#define CACCOUNTMETHOD(a,b,c,d) JSCRIPT_METHOD_IMP(CAccount,a)
#include "caccountmethods.tbl"
#undef CACCOUNTMETHOD
#endif

const CScriptMethod CAccount::sm_Methods[CAccount::M_QTY+1] =
{
#define CACCOUNTMETHOD(a,b,c,d) CSCRIPT_METHOD_IMP(a,b,c d)
#include "caccountmethods.tbl"
#undef CACCOUNTMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP1(Account,CAccount::sm_Props,CAccount::sm_Methods,NULL,ResourceObj);

CAccount::CAccount( LPCTSTR pszName, bool fGuest ) :
	CResourceObj( UID_INDEX_CLEAR )	// not set yet.
{
	// Just find a free UID for me.
	// Assume the szName has already been stripped of all bad stuff !

	g_Serv.StatInc( SERV_STAT_ACCOUNTS );

	m_sName = pszName;

	ASSERT( ! m_sName.IsEmpty());
	ASSERT( ! ISWHITESPACE( m_sName[0] ));

	if ( ! _strnicmp( m_sName, "GUEST", 5 ) || fGuest )
	{
		SetPrivLevel( PLEVEL_Guest );
	}
	else
	{
		SetPrivLevel( PLEVEL_Player );
	}

	m_PrivFlags = PRIV_DETAIL;	// Set on by default for everyone.

	// m_dateLastConnect.InitTimeCurrent();	// should be overwritten. (else time created)
	m_Total_Connect_Time = 0;
	m_Last_Connect_Time = 0;
	m_iEmailFailures = 0;

	// Add myself to the list.
	g_Accounts.Account_Add( this );
}

CAccount::~CAccount()
{
	// We should go track down and delete all the chars and clients that use this account !
	// Chat channel bans.
	// GMPAges
	//

	g_Accounts.Account_Cleanup(this);
	ASSERT( m_Chars.GetSize() == 0 );
	g_Serv.StatDec( SERV_STAT_ACCOUNTS );
}

void CAccount::DeleteThis()
{
	// Now track down all my disconnected chars !
	// Just un-reference myself then the CRefObjDef should delete me.
	if ( ! g_Serv.IsLoading())
	{
		DeleteAllChars();
	}

	// What if we have an attached client !?!?
	g_Accounts.Account_Delete(this);
}

int CAccount::NameStrip( TCHAR* pszNameOut, LPCTSTR pszNameInp, int iNameLen ) // static
{
	//	allow just basic chars. 
	//	No spaces, only numbers, letters and underbar. dash -. and dot . ?
	//	This will also be a valid file name or email address.
	// RETURN: 
	//  new length of the name.

	int len=0;
	for ( int i=0; pszNameInp[i] && i<iNameLen; i++ )
	{
		if ( ! CMailSMTP::IsValidEmailAddressChar( pszNameInp[i] ))
			continue;
		pszNameOut[len++] = pszNameInp[i];
	}
	pszNameOut[len] = '\0';

	if ( g_Cfg.IsObscene( pszNameOut ))
	{
		DEBUG_ERR(( "Obscene name '%s' ignored." LOG_CR, pszNameInp ));
		pszNameOut[0] = '\0';
		return( -1 );
	}

	// Check names not allowed.
	M_TYPE_ iProp = (M_TYPE_) s_FindKeyInTable( pszNameOut, CAccountMgr::sm_Methods );
	if ( iProp >= 0 )
	{
		DEBUG_ERR(( "Reserved name '%s' ignored." LOG_CR, pszNameInp ));
		pszNameOut[0] = '\0';
		return( -1 );
	}

	return len;
}

static LPCTSTR const sm_szPrivLevels[ PLEVEL_QTY+1 ] =
{
	"Guest",			// 0 = This is just a guest account. (cannot PK)
	"Player",			// 1 = Player or NPC.
	"Counsel",			// 2 = Can travel and give advice.
	"Seer",				// 3 = Can make things and NPC's but not directly bother players.
	"GM",				// 4 = GM command clearance
	"Dev",				// 5 = Not bothererd by GM's
	"Admin",			// 6 = Can switch in and out of gm mode. assign GM's
	"Owner",			// 7 = Highest allowed level.
	NULL,
};

PLEVEL_TYPE CAccount::GetPrivLevelText( LPCTSTR pszFlags ) // static
{
	PLEVEL_TYPE level = (PLEVEL_TYPE) FindTable( pszFlags, sm_szPrivLevels );
	if ( level >= 0 )
	{
		return( level );
	}

	level = (PLEVEL_TYPE) Exp_GetComplex( pszFlags );
	if ( level < 0 || level >= PLEVEL_QTY )
		return( PLEVEL_Player );

	return( level );
}

void CAccount::SetPrivLevel( PLEVEL_TYPE plevel )
{
	m_PrivLevel = plevel;	// PLEVEL_Counsel
}

CGString CAccount::GetPassword() const
{
	return( m_sCurPassword );
}

void CAccount::SetPassword( LPCTSTR pszPassword )
{
	// limit to 16 chars.
	// Strip surrounding quotes if present (save files quote passwords).
	if ( pszPassword && pszPassword[0] == '"' )
	{
		pszPassword++;
		char szPassword[ MAX_ACCOUNT_PASSWORD_ENTER+2 ];
		strcpylen( szPassword, pszPassword, MAX_ACCOUNT_PASSWORD_ENTER );
		int len = strlen(szPassword);
		if ( len > 0 && szPassword[len-1] == '"' )
			szPassword[len-1] = '\0';
		m_sCurPassword = szPassword;
	}
	else
	{
		char szPassword[ MAX_ACCOUNT_PASSWORD_ENTER+2 ];
		strcpylen( szPassword, pszPassword, MAX_ACCOUNT_PASSWORD_ENTER );
		m_sCurPassword = szPassword;
	}
}

CGString CAccount::GetXPassword() const
{
	g_Accounts.m_Crypt.SetCryptSeed( *((DWORD*)(const char*) m_sName )) ;
	char szPassword[ MAX_ACCOUNT_PASSWORD_ENTER+2 ];
	g_Accounts.m_Crypt.EncryptText( szPassword, GetPassword(), MAX_ACCOUNT_PASSWORD_ENTER );
	return( szPassword ); 
}

void CAccount::SetXPassword( LPCTSTR pszPassword )
{
	g_Accounts.m_Crypt.SetCryptSeed( *((DWORD*)(const char*) m_sName )) ;
	char szPassword[ MAX_ACCOUNT_PASSWORD_ENTER+2 ];
	g_Accounts.m_Crypt.DecryptText( szPassword, pszPassword, MAX_ACCOUNT_PASSWORD_ENTER );
	m_sCurPassword = szPassword;
}

void CAccount::DeleteAllChars()
{
	// When the account is to be deleted we need to clean up all my refs.
	int iQty = m_Chars.GetSize();
	for ( int i=0; i < iQty; i++ )
	{
		// may be UID_PLACE_HOLDER
		CCharPtr pChar = g_World.CharFind( m_Chars.GetAt(i));
		if ( pChar == NULL )
			continue;
		pChar->DeleteThis();
	}
	m_Chars.RemoveAll();
}

int CAccount::DetachChar( CChar* pChar )
{
	// Called by CChar to unlink itself.
	// unlink the CChar from this CAccount.
	ASSERT( pChar );
	if ( m_uidLastChar == pChar->GetUID())
	{
		m_uidLastChar.InitUID();
	}

	return( m_Chars.DetachObj( pChar ));
}

int CAccount::AttachChar( CChar* pChar )
{
	// Called by CChar to link itself.
	// link the char to this account.
	ASSERT(pChar);

	// is it already linked ?
	int i = m_Chars.AttachObj( pChar );
	if ( i >= 0 )
	{
		int iQty = m_Chars.GetSize();
		if ( iQty > UO_MAX_CHARS_PER_ACCT )
		{
			g_pLog->Event( LOG_GROUP_ACCOUNTS, LOGL_ERROR, "Account '%s' has %d characters" LOG_CR, (LPCTSTR) GetName(), iQty );
		}
	}

	return( i );
}

void CAccount::TogPrivFlags( WORD wPrivFlags, CGVariant& vVal )
{
	// s.GetArgFlag
	// No args = toggle the flag.
	// 1 = set the flag.
	// 0 = clear the flag.
	m_PrivFlags = vVal.GetDWORDMask( m_PrivFlags, wPrivFlags );
}

void CAccount::OnLogin( CClient* pClient )
{
	// The account just logged in.

	ASSERT(pClient);

	CSocketAddress PeerName = pClient->m_Socket.GetPeerName();

	if ( GetPrivLevel() >= PLEVEL_Counsel )	// ON by default.
	{
		m_PrivFlags |= PRIV_GM_PAGE;
	}

	// Get the real world time/date.
	CGTime datetime;
	datetime.InitTimeCurrent();

	if ( ! m_Total_Connect_Time || ! m_First_IP.IsValidAddr())
	{
		// FIRSTIP= first IP used.
		m_First_IP = PeerName;
		// FIRSTCONNECTDATE= date/ time of the first connect.
		m_dateFirstConnect = datetime;
	}

	// LASTIP= last IP used.
	m_Last_IP = PeerName;
	// LASTCONNECTDATE= date / time of this connect.
	m_dateLastConnect = datetime;

	if ( pClient->m_ConnectType == CONNECT_TELNET )
	{
		// link the admin client.
		g_Serv.m_nClientsAreAdminTelnets++;
	}
	if ( GetPrivLevel() <= PLEVEL_Guest )
	{
		g_Serv.m_nClientsAreGuests ++;
	}

	g_pLog->Event( LOG_GROUP_CLIENTS, LOGL_TRACE, "%x:Login '%s'" LOG_CR, pClient->m_Socket.GetSocket(), (LPCTSTR) GetName());
	g_Serv.OnTriggerEvent( SERVTRIG_ClientChange, pClient->m_Socket.GetSocket());
}

void CAccount::OnLogout( CClient* pClient )
{
	// CClient is disconnecting from this CAccount.
	ASSERT(pClient);

	if ( pClient->m_ConnectType == CONNECT_TELNET )
	{
		// unlink the admin client.
		g_Serv.m_nClientsAreAdminTelnets --;
	}
	if ( GetPrivLevel() <= PLEVEL_Guest )
	{
		g_Serv.m_nClientsAreGuests--;	// Free up a guest slot.
	}

	if ( pClient->IsConnectTypePacket())	// calculate total game time.
	{
		// LASTCONNECTTIME= How long where you connected last time.
		m_Last_Connect_Time = pClient->m_timeLogin.GetCacheAge() / ( TICKS_PER_SEC* 60 );
		if ( m_Last_Connect_Time <= 0 )
			m_Last_Connect_Time = 1;

		// TOTALCONNECTTIME= qty of minutes connected total.
		m_Total_Connect_Time += m_Last_Connect_Time;
	}

	g_Serv.OnTriggerEvent( SERVTRIG_ClientDetach, pClient->m_Socket.GetSocket());
}

bool CAccount::Block( CScriptConsole* pSrc, DWORD dwMinutes )
{
	// Block this account for some amount of time.
	if ( GetPrivLevel() >= pSrc->GetPrivLevel())
	{
		pSrc->WriteString( "You are not privileged to do that" );
		return( false );
	}

	if ( dwMinutes )
	{
		SetPrivFlags( PRIV_BLOCKED );
		pSrc->Printf( "Account '%s' blocked for %d minutes", (LPCTSTR) GetName(), dwMinutes );
	}

	LPCTSTR pszAction = dwMinutes ? "BLOCKED" : "DISCONNECTED";
	g_pLog->Event( LOG_GROUP_GM_CMDS, LOGL_EVENT, "A'%s' was %s by '%s' (%d min)" LOG_CR, (LPCTSTR) GetName(), pszAction, (LPCTSTR) pSrc->GetName(), dwMinutes );
	return( true );
}

bool CAccount::EmailPassword()
{
	// This counts as being online !
	m_dateLastConnect.InitTimeCurrent();

	return( ScheduleEmailMessage( g_Cfg.ResourceGetIDType( RES_EMailMsg, "EMAIL_PASSWORD" )));
}

bool CAccount::CheckPassword( LPCTSTR pszPassword )
{
	// RETURN:
	//  false = failure.

	ASSERT(!IsPrivFlag( PRIV_BLOCKED ));
	ASSERT(pszPassword);

	if ( m_sCurPassword.IsEmpty())
	{
		// First guy in sets the password.
		// check the account in use first.
		if ( *pszPassword == '\0' )
			return( false );

		SetPassword( pszPassword );
		return( true );
	}

	// Get the password.
	if ( ! _stricmp( GetPassword(), pszPassword ))
		return( true );

	if ( ! m_sNewPassword.IsEmpty() && ! _stricmp( GetNewPassword(), pszPassword ))
	{
		// using the new password.
		// kill the old password.
		SetPassword( m_sNewPassword );
		m_sNewPassword.Empty();
		return( true );
	}

	return( false );	// failure.
}

void CAccount::SetNewPassword( LPCTSTR pszPassword )
{
	TCHAR szTmp[MAX_ACCOUNT_PASSWORD_ENTER+1];

	if ( pszPassword == NULL || pszPassword[0] == '\0' )
	{
		// Set a new random password.
		// use no Oo0 or 1l

		int i;
		for ( i=0; i<8 && i<MAX_ACCOUNT_PASSWORD_ENTER-1; i++ )
		{
			TCHAR ch = Calc_GetRandVal(26+10);
			if ( ch >= 26 )
				ch = ch - 26 + '0';
			else
				ch = ch + 'A';
			if ( strchr( "Oo01lIS5", ch ))	// don't use these chars.
				ch = Calc_GetRandVal(4)+'A';
			szTmp[i] = ch;
		}

		szTmp[i] = '\0';
		m_sNewPassword = szTmp;
		return;
	}

	// limit to 16 chars.
	strncpy( szTmp, pszPassword, MAX_ACCOUNT_PASSWORD_ENTER );
	szTmp[MAX_ACCOUNT_PASSWORD_ENTER] = '\0';

	m_sNewPassword = szTmp;
}

bool CAccount::CheckBlockedEmail( LPCTSTR pszEmail ) // static
{
	// RETURN: true = not allowed.

	if ( strstr( pszEmail, "@here.com" ))	// don't allow this domain.
		return( true );

	CResourceLock s( g_Cfg.ResourceGetDef( CSphereUID( RES_BlockEMail )));
	if ( ! s.IsFileOpen())
	{
		return( false );
	}

	int len1 = strlen( pszEmail );
	// Read in and compare the sections.
	while ( s.ReadLine())
	{
		int len2 = strlen( s.GetKey());
		if ( len2 > len1 )
			continue;
		if ( ! _stricmp( pszEmail + len1 - len2, s.GetKey()))
		{
			// Bad email host detected.
			return( true );
		}
	}

	return( false );
}

bool CAccount::SetEmailAddress( LPCTSTR pszEmail, bool fBlockHost )
{
	//  fBlockHost = 
	// RETURN:
	//  false = not allowed.

	if ( pszEmail == NULL )
		return( false );
	GETNONWHITESPACE( pszEmail );	// Skip leading spaces.
	if ( ! CMailSMTP::IsValidEmailAddressFormat( pszEmail ))
		return( false );

	// Make sure it is a valid email server !
	if ( fBlockHost && CheckBlockedEmail(pszEmail))
	{
		return( false );
	}

	// Not already used ?
	CAccountPtr pAccount = g_Accounts.Account_FindEmail( pszEmail );
	if ( pAccount.IsValidRefObj() && pAccount != this )
	{
		return false;
	}

	m_sEMail = pszEmail;
	m_iEmailFailures = 0;
	ClearPrivFlags( PRIV_EMAIL_VALID );	// we have not tried it yet.
	return( true );
}

bool CAccount::ScheduleEmailMessage( CSphereUID uidEmailMessage )
{
	// ??? Note this might have some thread conflict issues.
	// Queued in forground thread. Dequeued in background thread.
	if ( m_sEMail.IsEmpty())
		return( false );
	if ( m_iEmailFailures > 5 )
		return( false );

	// Make sure this msg is not already scheduled.
	return( m_EMailSchedule.AttachUID(uidEmailMessage) >= 0 );
}

bool CAccount::SendOutgoingMail( CSphereUID uidEmailMessage )
{
	// NOTE: This should be called from a new thread.
	CResourceLock s( g_Cfg.ResourceGetDef( uidEmailMessage ));
	if ( ! s.IsFileOpen())
	{
		// This is an odd problem.
		// we should just move on to the next msg if any ?
		return false;
	}

	CScriptExecContext exec( this, &g_Serv );
	CGStringArray aStrings;
	while (s.ReadLine(false))
	{
		// Parse out any <?methodsorprops?> in it.
		exec.s_ParseEscapes( s.GetLineBuffer(), 0 );
		aStrings.Add( s.GetLineBuffer());
	}

	CGString sName = GetName();	// in case the account is deleted during send.
	CMailSMTP smtp;

	// Set m_EMailGateway

	if ( ! smtp.SendMail( g_Serv.m_ip.GetAddrStr(), 
		m_sEMail, 
		g_Serv.m_sEMail,
		&aStrings ))
	{
		DEBUG_ERR(( "Failed to send mail to '%s' '%s', reason '%s'" LOG_CR, (LPCTSTR) sName, (LPCTSTR) m_sEMail, (LPCTSTR) smtp.GetLastResponse() ));
		return( false );
	}

	return( true );
}

void CAccount::OnTick()
{
	// These routines will block !
	// NOTE: This must run on background thread.

	if ( IsPrivFlag(PRIV_JAILED))
	{
		CVarDefPtr pVar = m_TagDefs.FindKeyPtr("JAIL_RELEASETIME");
		if ( pVar )
		{
			CServTime timeexp;
			timeexp.InitTime( pVar->GetInt());
			if ( timeexp.GetTimeDiff() <= 0 )
			{

			}
		}
	}
	if ( IsPrivFlag(PRIV_BLOCKED))
	{
		CVarDefPtr pVar = m_TagDefs.FindKeyPtr("UNBLOCK_TIME");
		if ( pVar )
		{
			CServTime timeexp;
			timeexp.InitTime( pVar->GetInt());
			if ( timeexp.GetTimeDiff() <= 0 )
			{

			}
		}
	}

	// Check for outgoing emails.

	while ( m_EMailSchedule.GetSize())
	{
		if ( m_iEmailFailures > 5 )
		{
			return;
		}
		if ( g_Serv.IsLoading())	// don't do this now. multi thread problems.
			return;
		CSphereUID uidEmailMessage = m_EMailSchedule.GetAt(0);
		if ( ! SendOutgoingMail( uidEmailMessage ))
		{
			m_iEmailFailures ++;
			return;	// try again later.
		}

		// we are done with this msg.
		m_EMailSchedule.RemoveAt(0);
		m_iEmailFailures = 0;
	}
}

HRESULT CAccount::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceObj::s_PropGet( pszKey, vValRet, pSrc ));
	}
	if ( !pSrc )
	{
		return( HRES_PRIVILEGE_NOT_HELD );
	}

	switch (iProp)
	{
	case P_Tag:
		return HRES_BAD_ARGUMENTS;
	case P_Account:
	case P_Name:
		vValRet = m_sName;
		break;
	case P_Chars:
		vValRet.SetInt( m_Chars.GetSize());
		break;
	case P_Block:
		vValRet.SetBool( IsPrivFlag( PRIV_BLOCKED ));
		break;
	case P_ChatName:
		vValRet = m_sChatName;
		break;
	case P_Comment:
		m_TagDefs.FindKeyVar( "COMMENT", vValRet );
		break;
	case P_Email:
		vValRet = m_sEMail;
		break;
	case P_EmailLink:
		if ( m_sEMail.IsEmpty())
		{
			vValRet.SetVoid();
			break;
		}
		vValRet.SetStrFormat( "<a href=\"mailto:%s\">%s</a>", (LPCTSTR) m_sEMail, (LPCTSTR) m_sEMail );
		break;
	case P_EmailFail:
		vValRet.SetInt( m_iEmailFailures );
		break;
	case P_EmailMsg:
		if ( m_EMailSchedule.GetSize())
		{
			vValRet = g_Cfg.ResourceGetName( m_EMailSchedule.GetAt(0));
		}
		break;
	case P_EmailQ:
		vValRet.SetInt( m_EMailSchedule.GetSize());
		break;
	case P_FirstConnectDate:
		vValRet = m_dateFirstConnect.Format(CTIME_FORMAT_DEFAULT);
		break;
	case P_FirstIP:
		vValRet = m_First_IP.GetAddrStr();
		break;
	case P_Guest:
		vValRet.SetBool( GetPrivLevel() == PLEVEL_Guest );
		break;
	case P_Jail:
	case P_JailTime:
		vValRet.SetBool( IsPrivFlag( PRIV_JAILED ));
		break;
	case P_Lang:
		vValRet = m_lang.GetStr();
		break;
	case P_LastCharUID:
		vValRet.SetUID( m_uidLastChar );
		break;
	case P_LastConnectDate:
		vValRet = m_dateLastConnect.Format(CTIME_FORMAT_DEFAULT);
		break;
	case P_LastConnectTime:
		vValRet.SetInt( m_Last_Connect_Time );
		break;
	case P_LastIP:
		vValRet = m_Last_IP.GetAddrStr();
		break;
	case P_Level:
	case P_PLevel:
		vValRet.SetInt( m_PrivLevel );
		break;
	case P_NewPassword:
		if ( pSrc->GetPrivLevel() < PLEVEL_Admin ||
			pSrc->GetPrivLevel() < GetPrivLevel())	// can't see accounts higher than you
		{
			return( HRES_PRIVILEGE_NOT_HELD );
		}
		vValRet = GetNewPassword();
		break;
	case P_Password:
		if ( pSrc->GetPrivLevel() < PLEVEL_Admin ||
			pSrc->GetPrivLevel() < GetPrivLevel())	// can't see accounts higher than you
		{
			return( HRES_PRIVILEGE_NOT_HELD );
		}
		vValRet = GetPassword();
		break;
	case P_Priv:
	case P_PrivFlags:
		vValRet.SetDWORD( m_PrivFlags );
		break;
	case P_TotalConnectTime:
		vValRet.SetInt( m_Total_Connect_Time );
		break;
#if 0
	case P_XPass:
		// Crypted account password.
		vValRet = GetXPassword();
		break;
#endif

	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CAccount::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	s_FixExtendedProp( pszKey, "Tag", vVal );

	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( HRES_UNKNOWN_PROPERTY );
	}

	switch (iProp)
	{
	case P_Tag: // get/set a local tag.
		return( m_TagDefs.s_PropSetTags( vVal ));

	case P_Account:
	case P_Name: // can't be set this way.
		return( HRES_INVALID_FUNCTION );
	case P_Block:
		if ( vVal.IsEmpty() || vVal.GetInt())
		{
			SetPrivFlags( PRIV_BLOCKED );
		}
		else
		{
			ClearPrivFlags( PRIV_BLOCKED );
		}
		break;
	case P_CharUID:
		// During loading, store the UID directly into m_Chars.
		// The character objects don't exist yet, but we record the UID so that
		// Setup_FillCharList can find them later via CharFind.
		// At runtime, AttachChar is called from Player_SetAccount instead.
		if ( g_Serv.IsLoading())
		{
			DWORD dwUID = vVal.GetInt();
			if ( dwUID )
			{
				m_Chars.AttachUID( CSphereUID(dwUID) );
			}
		}
		break;
	case P_ChatName:
		m_sChatName = vVal.GetStr();
		break;
	case P_Comment:
		return( m_TagDefs.SetKeyVar( "COMMENT", vVal ) >= 0 );
	case P_Email:
	case P_EmailLink:
		m_sEMail = vVal.GetStr();
		break;
	case P_EmailFail:
		m_iEmailFailures = vVal.GetInt();
		break;
	case P_EmailMsg:
		ScheduleEmailMessage( g_Cfg.ResourceGetIDType( RES_EMailMsg, vVal.GetPSTR()));
		break;
	case P_FirstConnectDate:
		m_dateFirstConnect.Read( vVal );
		break;
	case P_FirstIP:
		m_First_IP.SetAddrStr( vVal );
		break;
	case P_Guest:
		if ( vVal.IsEmpty() || vVal.GetBool())
		{
			SetPrivLevel( PLEVEL_Guest );
		}
		else
		{
			if ( GetPrivLevel() == PLEVEL_Guest )
				SetPrivLevel( PLEVEL_Player );
		}
		break;
	case P_Jail:
	case P_JailTime:
		// Set time in minutes?
		if ( vVal.IsEmpty() || vVal.GetInt())
		{
			SetPrivFlags( PRIV_JAILED );
		}
		else
		{
			ClearPrivFlags( PRIV_JAILED );
		}
		break;
	case P_Lang:
		m_lang.Set( vVal );
		break;
	case P_LastCharUID:
		m_uidLastChar = vVal.GetUID();
		break;
	case P_LastConnectDate:
		m_dateLastConnect.Read( vVal );
		break;
	case P_LastConnectTime:
		// Previous total amount of time in game
		m_Last_Connect_Time = vVal.GetInt();
		break;
	case P_LastIP:
		m_Last_IP.SetAddrStr( vVal );
		break;
	case P_Level:
	case P_PLevel:
		SetPrivLevel( GetPrivLevelText( vVal ));
		break;
	case P_NewPassword:
		SetNewPassword( vVal );
		break;
	case P_Password:
		SetPassword( vVal );
		break;
	case P_Priv:
	case P_PrivFlags:
		// Priv flags.
		m_PrivFlags = vVal.GetInt();
		break;
	case P_TotalConnectTime:
		// Previous total amount of time in game
		m_Total_Connect_Time = vVal.GetInt();
		break;
#if 0
	case P_XPass:
		// Crypted account password.
		SetXPassword( vVal );
		break;
#endif

	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

void CAccount::s_WriteProps( CScript& s, bool fTitle )
{
	if ( GetPrivLevel() >= PLEVEL_QTY )
		return;

	// NOTE: This should have "ACCOUNT" section title !

	if ( fTitle )
	{
		s.WriteSection( "ACCOUNT %s", m_sName );
	}
	else
	{
		s.WriteSection( m_sName );
	}

	if ( GetPrivLevel() != PLEVEL_Player )
	{
		s.WriteKey( "PLEVEL", sm_szPrivLevels[ GetPrivLevel() ] );
	}
	if ( m_PrivFlags != PRIV_DETAIL )
	{
		s.WriteKeyDWORD( "PRIV", m_PrivFlags &~( PRIV_BLOCKED | PRIV_JAILED ));
	}
	if ( IsPrivFlag( PRIV_JAILED ))
	{
		s.WriteKeyInt( "JAILTIME", 1 );
	}
	if ( IsPrivFlag( PRIV_BLOCKED ))
	{
		s.WriteKeyInt( "BLOCK", 1 );
	}
	if ( ! m_sCurPassword.IsEmpty())
	{
#if 0
		if ( g_Accounts.m_Crypt.GetCryptVer())
		{
			s.WriteKey( "XPASS", GetXPassword() );
		}
		else
#endif
		{
			s.WriteKey( "PASSWORD", GetPassword() );
		}
	}
	if ( ! m_sNewPassword.IsEmpty())
	{
		s.WriteKey( "NEWPASSWORD", GetNewPassword() );
	}
	if ( m_Total_Connect_Time )
	{
		s.WriteKeyInt( "TOTALCONNECTTIME", m_Total_Connect_Time );
	}
	if ( m_Last_Connect_Time )
	{
		s.WriteKeyInt( "LASTCONNECTTIME", m_Last_Connect_Time );
	}
	if ( m_uidLastChar.IsValidObjUID())
	{
		s.WriteKeyDWORD( "LASTCHARUID", m_uidLastChar );
	}

	m_Chars.s_WriteObjs( s, "CHARUID" );

	if ( ! m_sEMail.IsEmpty())
	{
		s.WriteKey( "EMAIL", m_sEMail );
	}
	if ( m_iEmailFailures )
	{
		s.WriteKeyInt( "EMAILFAIL", m_iEmailFailures );
	}
	// write out any scheduled email message s.
	for ( int i=0; i<m_EMailSchedule.GetSize(); i++ )
	{
		s.WriteKey( "EMAILMSG", g_Cfg.ResourceGetName( m_EMailSchedule.GetAt(i) ));
	}
	if ( m_dateFirstConnect.IsTimeValid())
	{
		s.WriteKey( "FIRSTCONNECTDATE", m_dateFirstConnect.Format(CTIME_FORMAT_DEFAULT));
	}
	if ( m_First_IP.IsValidAddr() )
	{
		s.WriteKey( "FIRSTIP", m_First_IP.GetAddrStr());
	}

	if ( m_dateLastConnect.IsTimeValid())
	{
		s.WriteKey( "LASTCONNECTDATE", m_dateLastConnect.Format(CTIME_FORMAT_DEFAULT));
	}
	if ( m_Last_IP.IsValidAddr() )
	{
		s.WriteKey( "LASTIP", m_Last_IP.GetAddrStr());
	}
	if ( ! m_sChatName.IsEmpty())
	{
		s.WriteKey( "CHATNAME", m_sChatName );
	}
	if ( m_lang.IsDef())
	{
		s.WriteKey( "LANG", m_lang.GetStr());
	}

	m_TagDefs.s_WriteTags( s );
}

HRESULT CAccount::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	ASSERT(pSrc);

	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceObj::s_Method( pszKey, vArgs, vValRet, pSrc ));
	}

	PLEVEL_TYPE plevel = (PLEVEL_TYPE) pSrc->GetPrivLevel();

	if ( plevel < GetPrivLevel())	// can't change accounts higher than you in any way
	{
		// Priv Level too low

		CAccountConsole* pSrcCmd = PTR_CAST(CAccountConsole,pSrc);
		if ( pSrcCmd == NULL || pSrcCmd->GetAccount() != this )
		{
			pSrc->Printf( "Not privileged to access '%s'." LOG_CR, (LPCSTR) GetName());
			return HRES_PRIVILEGE_NOT_HELD;
		}
	}

	switch ( iProp )
	{
	case M_Char:
		// Enumate the refs to cchars
		if ( vArgs.IsEmpty())	// just assume char 0 ?
			return HRES_BAD_ARG_QTY;
		{
			int i = vArgs.GetInt();
			if ( i<0 || i>=m_Chars.GetSize())
				return HRES_INVALID_INDEX;
			vValRet.SetRef( g_World.CharFind( m_Chars.GetAt(i)));
		}
		break;
	case M_Tag:
		// "TAG" = get/set a local tag.
		if ( plevel < PLEVEL_Admin ) // could use GetRef ?
			return HRES_PRIVILEGE_NOT_HELD;
		return( m_TagDefs.s_MethodTags( vArgs, vValRet, pSrc ));

		// case M_SUMMON:	// move all the offline chars here ?

	case M_Delete:
		// delete the account and all it's chars
		// If this is a ref then delete this way is weird.
		if ( plevel < PLEVEL_Admin )
			return HRES_PRIVILEGE_NOT_HELD;
		// can't delete unused priv accounts this way.
		if ( ! vArgs.GetInt())
		{
			if ( GetPrivLevel() > PLEVEL_Player )
			{
				pSrc->Printf( "Can't Delete PrivLevel %d Account '%s' this way." LOG_CR,
					GetPrivLevel(), (LPCTSTR) GetName() );
				return( NO_ERROR );
			}
		}
		DeleteThis();
		break;

	case M_SetEmail:
		// Enter the email address , but filtered.
		if ( !vArgs.IsEmpty())
		{
			if ( ! SetEmailAddress( vArgs.GetStr(), true ))
			{
				pSrc->Printf( "Email address '%s' is not allowed or is already used.", (LPCTSTR) vArgs.GetStr());
				return HRES_BAD_ARGUMENTS;
			}
		}
		pSrc->Printf( "Email for '%s' is '%s'.",
			(LPCTSTR) GetName(), (LPCTSTR) m_sEMail );
		break;

	case M_EmailSend:
		// Schedule one of the email messages to be delivered.
		if ( ! ScheduleEmailMessage( g_Cfg.ResourceGetIDType( RES_EMailMsg, vArgs )))
		{
			return( HRES_BAD_ARGUMENTS );
		}
		break;

	case M_EmailPass:
		// Email me my password.
		EmailPassword();
		break;

	case M_Block:
		if ( plevel < PLEVEL_Admin )
			return HRES_PRIVILEGE_NOT_HELD;
		if ( vArgs.IsEmpty() || vArgs.GetInt())
		{
			SetPrivFlags( PRIV_BLOCKED );
			// fall through
		}
		else
		{
			ClearPrivFlags( PRIV_BLOCKED );
			break;
		}
		// fall through

	case M_Kick:
		// if they are online right now then kick them!
		if ( plevel < PLEVEL_Admin )
		{
			vValRet = false;
			return HRES_PRIVILEGE_NOT_HELD;
		}
		{
			CClient* pClient = g_Serv.FindClientAccount(this);
			if ( pClient )
			{
				pClient->addKick( pSrc, iProp == M_Block );
			}
			else
			{
				Block( pSrc, iProp == M_Block ? INT_MAX : 0 );
			}
		}
		vValRet = true;
		break;

	case M_Status:
		// Give a status line for the account.
		if ( IsPrivFlag( PRIV_BLOCKED ))
		{
			// ??? Time ?
			vValRet = "Blocked";
		}
		else if ( IsPrivFlag( PRIV_JAILED ))
		{
			vValRet = "Jailed";
		}
		else if ( GetPrivLevel() <= PLEVEL_Guest )
		{
			vValRet = "Guest";
		}
		else
		{
			vValRet = "Normal";
		}
		break;

	case M_Export:
		// Export this account out to this file name.
		if ( plevel < PLEVEL_Admin )
			return HRES_PRIVILEGE_NOT_HELD;
		{
			CScript sOut;
			if ( ! sOut.Open( vArgs, OF_WRITE|OF_CREATE|OF_TEXT ))
				return( false );

			// Write out the account header
			s_WriteProps( sOut, true );

			// now write out all the chars.
			int iQty = m_Chars.GetSize();
			for ( int i=0; i<iQty; i++ )
			{
				CCharPtr pChar = g_World.CharFind( m_Chars.GetAt(i));
				if ( pChar == NULL )
					continue;
				pChar->s_WriteSafe( sOut );
			}

			sOut.WriteSection( "EOF" );
		}
		break;

	case M_SetPassword:
		// Set/Clear the password
		if ( vArgs.IsEmpty())
		{
			ClearPassword();
			pSrc->Printf( "Password for '%s' has been cleared.", (LPCTSTR) GetName());
			pSrc->Printf( "Log out, then back in to set the new password." LOG_CR );
			g_pLog->Event( LOG_GROUP_ACCOUNTS, LOGL_EVENT, "Account '%s', password cleared" LOG_CR, (LPCTSTR) GetName());
		}
		else
		{
			SetPassword( vArgs.GetStr());
			pSrc->Printf( "Password for '%s' has been set" LOG_CR, (LPCTSTR) GetName());
			g_pLog->Event( LOG_GROUP_ACCOUNTS, LOGL_EVENT, "Account '%s', password set to '%s'" LOG_CR, (LPCTSTR) GetName(), (LPCTSTR) vArgs.GetStr());
		}
		break;

	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}


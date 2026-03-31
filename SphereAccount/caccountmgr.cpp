//
// CAccountMgr.cpp
// Copyright Menace Software (www.menasoft.com).
//

#include "stdafx.h"	// predef header.
#include "caccountbase.h"
#include "caccount.h"

//**********************************************************************
// -CAccountMgr

#ifdef USE_JSCRIPT
#define CACCOUNTMGRMETHOD(a,b,c,d) JSCRIPT_METHOD_IMP(CAccountMgr,a)
#include "caccountmgrmethods.tbl"
#undef CACCOUNTMGRMETHOD
#endif

const CScriptMethod CAccountMgr::sm_Methods[CAccountMgr::M_QTY+1] =	// CAccountMgr:: // account group verbs.
{
	// { "<accountname>", 0, "[commannd] command this account" },
#define CACCOUNTMGRMETHOD(a,b,c,d) CSCRIPT_METHOD_IMP(a,b,c d)
#include "caccountmgrmethods.tbl"
#undef CACCOUNTMGRMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP1(AccountMgr,NULL,CAccountMgr::sm_Methods,NULL,ResourceObj);

CAccountMgr::CAccountMgr() :
	CResourceObj(UID_INDEX_CLEAR),
	m_UIDs( (1<<RID_TYPE_SHIFT)-1 )
{
	// Setup the password crypt routines.
	IncRefCount();	// static singleton
	m_Accounts.IncRefCount(); // Static item
	m_Crypt.SetCryptMasterVer(0,0x12345678,0x87654321);
}

CAccountMgr::~CAccountMgr()
{
	StaticDestruct();	// static singleton
	m_Accounts.StaticDestruct(); // Static item
}

void CAccountMgr::Empty()
{
	m_Accounts.RemoveAll();
	m_UIDs.DeleteAllUIDs();
}

void CAccountMgr::OnTick()
{
	// Attempt to send the outgoing mail now.
	// NOTE: This will block. so it should be called on a seperate thread !

	// look thru all the accounts for outgoing mail.
	// ??? This is SERIOSLY asking for multi-thread problems !

	for ( int i=0;; i++ )
	{
		CAccountPtr pAccount = Account_Get(i);
		if ( ! pAccount )
			break;
		pAccount->OnTick();
	}
}

CAccountPtr CAccountMgr::Account_FindNameChecked( LPCTSTR pszName )
{
	// Name has alrady been checked for format validity
	CThreadLockPtr lock( &m_Accounts );

	int i = m_Accounts.FindKey( pszName );
	if ( i >= 0 )
	{
		return( Account_Get(i));
	}

	return( NULL );
}

CAccountPtr CAccountMgr::Account_FindNameCheck( LPCTSTR pszName )
{
	TCHAR szName[ MAX_ACCOUNT_NAME_SIZE ];
	// In valid format ?
	if ( CAccount::NameStrip( szName, pszName ) <= 0 )
		return( NULL );

	return Account_FindNameChecked( szName );
}

bool CAccountMgr::Account_Load( CScript& s, bool fChanges )
{
	// NOTE: Account names should not have spaces !!!
	// ARGS:
	//   fChanges = false = trap duplicates

	LPCTSTR pszNameRaw = s.GetKey();
	if ( ! _stricmp( pszNameRaw, "ACCOUNT" ))
	{
		// get rid of the header, just use the arg as the account name.
		pszNameRaw = s.GetArgStr();
	}

	TCHAR szName[ MAX_ACCOUNT_NAME_SIZE ];
	if ( CAccount::NameStrip( szName, pszNameRaw )  <= 0 )
	{
		if ( ! fChanges )
		{
			g_pLog->Event( LOG_GROUP_INIT, LOGL_ERROR, _TEXT("Account '%s': BAD name" LOG_CR), pszNameRaw );
			return false;
		}
	}

	CAccountPtr pAccount = Account_FindNameChecked( szName );
	if ( pAccount )
	{
		// Look for existing duplicate ?
		if ( ! fChanges )
		{
			g_pLog->Event( LOG_GROUP_INIT, LOGL_ERROR, _TEXT("Account '%s': duplicate name" LOG_CR), pszNameRaw );
			return false;
		}
	}
	else
	{
		pAccount = new CAccount( szName );
		ASSERT(pAccount);
	}

	pAccount->s_LoadProps(s);
	return( true );
}

bool CAccountMgr::Account_LoadAll( bool fChanges, bool fClearChanges )
{
	// Load the accounts file. (at start up)

	CString sBaseDir;
	if ( g_Cfg.m_sAcctBaseDir.IsEmpty())
	{
		sBaseDir = g_Cfg.m_sWorldBaseDir;
	}
	else
	{
		sBaseDir = g_Cfg.m_sAcctBaseDir;
	}

	LPCTSTR pszBaseName;
	if ( fChanges )
	{
		pszBaseName = SPHERE_FILE "acct"; // Open script file
	}
	else
	{
		pszBaseName = SPHERE_FILE "accu";
	}

	CGString sLoadName;
	sLoadName.Format( _TEXT("%s%s"), (LPCTSTR)sBaseDir, pszBaseName );

	fprintf(stderr, "DBG: Account_LoadAll fChanges=%d, sBaseDir='%s', loading='%s'\n", fChanges, (LPCTSTR)sBaseDir, (LPCTSTR)sLoadName); fflush(stderr);

	CScript s;
	if ( ! s.Open( sLoadName, fChanges ? (OF_NONCRIT|OF_READ|OF_TEXT) : (OF_READ|OF_TEXT)))
	{
		fprintf(stderr, "DBG: Account_LoadAll failed to open '%s'\n", (LPCTSTR)sLoadName); fflush(stderr);
		if ( ! fChanges )
		{
			if ( Account_LoadAll( true ))	// if we have changes then we are ok.
				return( true );
			g_pLog->Event( LOG_GROUP_INIT, LOGL_FATAL, "Can't open account file '%s'" LOG_CR, (LPCTSTR) s.GetFilePath());
		}
		return false;
	}
	fprintf(stderr, "DBG: Account_LoadAll opened '%s' OK\n", (LPCTSTR)sLoadName); fflush(stderr);

	if ( fClearChanges )
	{
		ASSERT( fChanges );
		// empty the changes file.
		s.Close();
		s.Open( NULL, OF_WRITE|OF_TEXT );
		s.WriteString( "// Accounts are periodically moved to the " SPHERE_FILE "accu" SCRIPT_EXT " file." LOG_CR
			"// All account changes should be made here." LOG_CR
			"// Use the /ACCOUNT UPDATE command to force accounts to update." LOG_CR
			);
		s.WriteSection( "EOF" );
		return true;
	}

	CSphereScriptContext ScriptContext( &s );

	fprintf(stderr, "DBG: Account_LoadAll parsing sections...\n"); fflush(stderr);
	int nAcct = 0;
	while (s.FindNextSection())
	{
		Account_Load( s, fChanges );
		nAcct++;
	}
	fprintf(stderr, "DBG: Account_LoadAll loaded %d sections from '%s'\n", nAcct, (LPCTSTR)sLoadName); fflush(stderr);

	if ( ! fChanges )
	{
		Account_LoadAll( true );
	}

	return( true );
}

bool CAccountMgr::Account_SaveAll()
{
	// Look for changes FIRST.

	Account_LoadAll( true );

	CString sBaseDir;
	if ( g_Cfg.m_sAcctBaseDir.IsEmpty())
	{
		sBaseDir = g_Cfg.m_sWorldBaseDir;
	}
	else
	{
		sBaseDir = g_Cfg.m_sAcctBaseDir;
	}

	CScript s;
	if ( ! CWorld::OpenScriptBackup( s, sBaseDir, "accu", g_World.m_iSaveCountID ))
		return( false );

	s.Printf( "\\\\ " SPHERE_TITLE " %s accounts file" LOG_CR
		"\\\\ NOTE: This file cannot be edited while the server is running." LOG_CR
		"\\\\ Any file changes must be made to " SPHERE_FILE "accu" SCRIPT_EXT ". This is read in at save time." LOG_CR,
		(LPCTSTR) g_Serv.GetName());

	CThreadLockPtr lock( &m_Accounts );
	for ( int i=0;; i++ )
	{
		CAccountPtr pAccount = Account_Get(i);
		if ( ! pAccount.IsValidRefObj())
			break;
		pAccount->s_WriteProps( s, false );
	}

	// Write [EOF]
	s.WriteSection( "EOF" );

	Account_LoadAll( true, true );	// clear the change file now.
	return( true );
}

void CAccountMgr::Account_Cleanup( CAccount* pAccount )
{
	m_UIDs.FreeUID( pAccount );
}

void CAccountMgr::Account_Delete( CAccount* pAccount )
{
	// What if we have an attached client !?!?
	m_Accounts.RemoveArg( pAccount );
}

void CAccountMgr::Account_Add( CAccount* pAccount )
{
	// Add to the uid table and name sorted index.
	m_UIDs.AllocUID( pAccount, RES_Account<<RID_TYPE_SHIFT );
	m_Accounts.AddSortKey(pAccount,pAccount->GetName());
}

CAccountPtr CAccountMgr::Account_Get( int index )
{
	// CThreadLockPtr<CAccountMgr> lock( &m_Accounts ); // We should already be locked!
	if ( index >= m_Accounts.GetSize())
		return( NULL );
	return( CAccountPtr( m_Accounts[index]));
}

CAccountPtr CAccountMgr::Account_FindChatName( LPCTSTR pszChatName )
{
	// IS this new chat name already used ?
	CThreadLockPtr lock( &m_Accounts );
	for ( int i=0;; i++ )
	{
		CAccountPtr pAccount = Account_Get(i);
		if ( ! pAccount )
			break;
		if ( ! pAccount->m_sChatName.CompareNoCase( pszChatName ))
			return( pAccount );
	}
	return( NULL );
}

CAccountPtr CAccountMgr::Account_FindEmail( LPCTSTR pszEmail )
{
	// Is this email already used ?
	if ( ! CMailSMTP::IsValidEmailAddressFormat( pszEmail ))
	{
		return NULL;
	}

	CThreadLockPtr lock( &m_Accounts );
	for ( int i=0;; i++ )
	{
		CAccountPtr pAccount = Account_Get(i);
		if ( ! pAccount.IsValidRefObj())
			break;
		if ( ! pAccount->m_sEMail.CompareNoCase( pszEmail ))
			return( pAccount );
	}
	return( NULL );
}

CAccountPtr CAccountMgr::Account_FindCreatedIP( CSocketAddressIP dwIP )
{
	// Was this ip used to create an account before ?
	// Is this ip already used ?
	CThreadLockPtr lock( &m_Accounts );
	for ( int i=0;; i++ )
	{
		CAccountPtr pAccount = Account_Get(i);
		if ( ! pAccount.IsValidRefObj())
			break;
		if ( pAccount->m_Last_IP == dwIP )
			return( pAccount );
		if ( pAccount->m_First_IP == dwIP )
			return( pAccount );
	}
	return( NULL );
}

CAccountPtr CAccountMgr::Account_Create( LPCTSTR pszName )
{
	// Create one in some circumstances.
	// ASSUME: This account has already been checked not to exist.

	bool fGuest = ( g_Serv.m_eAccApp == ACCAPP_GuestAuto ||
		g_Serv.m_eAccApp == ACCAPP_GuestTrial ||
		! _strnicmp( pszName, "GUEST", 5 ));

	return new CAccount( pszName, fGuest );
}

CAccountPtr CAccountMgr::Account_CreateNew( CScriptConsole* pSrc, LPCTSTR pszName, LPCTSTR pszPassword )
{
	TCHAR szName[ MAX_ACCOUNT_NAME_SIZE ];
	// In valid format ?
	if ( CAccount::NameStrip( szName, pszName ) <= 0 )
	{
		pSrc->Printf( "Account '%s' not allowed" LOG_CR, (LPCTSTR) pszName );
		return( NULL );
	}

	CAccountPtr pAccount = Account_FindNameChecked( szName );
	if ( pAccount != NULL )
	{
		pSrc->Printf( "Account '%s' already exists" LOG_CR, (LPCTSTR) szName );
		return NULL;
	}

	pAccount = new CAccount(szName);
	ASSERT(pAccount);

	pAccount->SetPassword( pszPassword );

	pSrc->Printf( "Account '%s' created" LOG_CR, (LPCTSTR) szName );
	return( pAccount );
}

CAccountPtr CAccountMgr::Account_CreateEmail( CScriptConsole* pSrc, LPCTSTR pszName, LPCTSTR pszEmail )
{
	// ADDEMAIL
	// Apply on the web for the free account.
	// mail the password.
	// ? only allow one new account per ip per day ?

	TCHAR szName[ MAX_ACCOUNT_NAME_SIZE ];
	if ( CAccount::NameStrip( szName, pszName ) <= 0 )
	{
		pSrc->Printf( "Account name '%s' is not valid.", (LPCTSTR) pszName );
		return( NULL );
	}

	CAccountPtr pAccount = Account_FindNameChecked( szName );
	if ( pAccount != NULL )
	{
		pSrc->Printf( "Account '%s' already exists", (LPCTSTR) szName );
		return( NULL );
	}

	pAccount = Account_FindEmail( pszEmail );
	if ( pAccount != NULL )
	{
		pSrc->Printf( "An account with email '%s' already exists.", (LPCTSTR) pszEmail );
		return( NULL );
	}

	CGTime datetime;
	datetime.InitTimeCurrent();

	if ( pSrc->GetPrivLevel() < PLEVEL_Admin )
	{
		// Already been to this ip address today ?
		CClientPtr pClient = PTR_CAST(CClient,pSrc);
		if (pClient)
		{
			pAccount = Account_FindCreatedIP( pClient->m_Socket.GetPeerName() );
			if ( pAccount )
			{
				// If we have been online in the last day then don't bother.
				if ( datetime.GetTime() - pAccount->m_dateLastConnect.GetTime()
					< 2*24*60*60 )
				{
					pSrc->WriteString( "You already created an account recently." );
					return( NULL );
				}
			}
		}
	}

	// Create and mail my account.
	pAccount = new CAccount(szName);
	ASSERT(pAccount);

	// Blocked email address ?
	if ( ! pAccount->SetEmailAddress( pszEmail, true ))
	{
		pSrc->WriteString( "This email address is not allowed or may already be used." );
		pAccount->DeleteThis();
		return( NULL );
	}

	// Send them email with the new password.

	pAccount->SetNewPassword( NULL );
	pAccount->SetPassword( pAccount->GetNewPassword());
	pAccount->m_dateLastConnect = datetime;

	if ( ! pAccount->ScheduleEmailMessage( g_Cfg.ResourceGetIDType( RES_EMailMsg, "EMAIL_NEWTHANKS" )))
	{
		pSrc->Printf( "Scheduling password mailing to '%s' failed", (LPCTSTR) pszEmail );
		pAccount->DeleteThis();
		return( NULL );
	}

	// This counts as being on line.
	pSrc->Printf( "Password has been mailed to '%s'.", (LPCTSTR) pszEmail );
	return( pAccount );
}

bool CAccountMgr::Account_PasswordEmail( CScriptConsole* pSrc, LPCTSTR pszEmail )
{
	// Mail an accounts password to them. recovery
	// Find by email
	// Allow this only once per day to prevent abuse.

	ASSERT(pSrc);

	CAccountPtr pAccount = Account_FindEmail( pszEmail );
	if ( ! pAccount.IsValidRefObj())
	{
		// IS it just the account name ?
		pAccount = Account_FindNameChecked( pszEmail );
		if ( ! pAccount.IsValidRefObj())
		{
			pSrc->Printf( "Email address '%s' does not have an account.", (LPCSTR) pszEmail );
			return( false );
		}
	}

	if ( pSrc->GetPrivLevel() <= PLEVEL_Player )
	{
		// Get the real world time/date.
		CGTime datetime;
		datetime.InitTimeCurrent();

		// If we have been online in the last day then don't bother.
		if ( datetime.GetTime() - pAccount->m_dateLastConnect.GetTime()
			< 24*60*60 )
		{
			pSrc->WriteString( "Try again tomorrow." );
			return( false );
		}
	}

	if ( ! pAccount->EmailPassword())
	{
		pSrc->Printf( "Scheduling password mailing to '%s' failed", (LPCTSTR) pszEmail );
		return( false );
	}

	// This counts as being on line.
	pSrc->Printf( "Password has been mailed to '%s'.", (LPCTSTR) pszEmail );
	return( true );
}

void CAccountMgr::Cmd_AllUnused( CScriptConsole* pSrc, int iDaysTest, CGVariant& vCommand )
{
	// do something to all the unused accounts.

	CGString sCmd = (LPCTSTR)vCommand;
	CGTime datetime;
	datetime.InitTimeCurrent();
	int iDaysCur = datetime.GetTotalDays();

	int iQtyStart = Account_GetCount();
	int iQtyLast = iQtyStart;
	int iCount = 0;
	for ( int i=0;; i++ )
	{
		{
		CAccountPtr pAccount = Account_Get(i);
		if ( ! pAccount )
			break;
		int iDaysAcc = pAccount->m_dateLastConnect.GetTotalDays();
		if ( ! iDaysAcc )
		{
			// account has never been used ? (get create date instead)
			iDaysAcc = pAccount->m_dateFirstConnect.GetTotalDays();
		}

		int iDaysDiff = iDaysCur - iDaysAcc;
		if ( iDaysDiff <= iDaysTest )
			continue;
		// Account has not been used in a while.

		iCount ++;

		CSphereExpContext exec( pAccount, pSrc );

		if ( sCmd.IsEmpty())
		{
			// just list stuff about the account.
			exec.ExecuteCommand( "SHOW LASTCONNECTDATE" );
			continue;
		}

		exec.ExecuteCommand( sCmd );
		}

		int iQty = Account_GetCount();
		if ( iQty != iQtyLast )	// did we delete it ?
		{
			iQtyLast = iQty;
			i--;
		}
	}

	pSrc->Printf( "%d of %d accounts unused for %d days", iCount, iQtyStart, iDaysTest );
}

HRESULT CAccountMgr::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	// Modify the accounts on line. "ACCOUNT"
	ASSERT( pSrc );

	M_TYPE_ iProp = (M_TYPE_) s_FindKeyInTable( pszKey, sm_Methods );

	if ( pSrc->GetPrivLevel() < PLEVEL_Admin )
	{
		switch (iProp)
		{
		case M_EmailPass:
		case M_Qty:
			// anyone can do this.
			break;
		case M_AddEmail:
			if ( g_Serv.m_eAccApp < ACCAPP_WebApp )
				return( HRES_PRIVILEGE_NOT_HELD );
			break;
		default:
			// Lack priv to do this !
			return HRES_PRIVILEGE_NOT_HELD;
		}
	}

	if ( iProp < 0 )
	{
		// Must be a valid account ?
		CAccountPtr pAccount = Account_FindNameCheck( pszKey );
		if ( ! pAccount )
		{
			return CResourceObj::s_Method( pszKey, vArgs, vValRet, pSrc );
		}

		CSphereExpContext exec( pAccount, pSrc );
		if ( vArgs.IsEmpty())
		{
			// just list stuff about the account.
			return exec.ExecuteCommand( "SHOW PLEVEL" );
		}

		return exec.ExecuteCommand( vArgs );
	}

	int iQty;

	switch (iProp)
	{
	case M_Qty:
		vValRet = Account_GetCount();
		break;

	case M_Add:
		iQty = vArgs.MakeArraySize();
		Account_CreateNew( pSrc, vArgs.GetArrayPSTR(0), vArgs.GetArrayPSTR(1));
		break;

	case M_AddEmail:
		iQty = vArgs.MakeArraySize();
		Account_CreateEmail( pSrc, vArgs.GetArrayPSTR(0), vArgs.GetArrayPSTR(1));
		break;

	case M_EmailPass:
		// I only know my email.
		Account_PasswordEmail( pSrc, vArgs );
		break;

	case M_EoF:		// just reserved.
	case M_Account:	// just reserved.
		return HRES_INVALID_FUNCTION;

	case M_Update:
		Account_SaveAll();
		break;

	case M_Unused:
		{
			vArgs.MakeArraySize();
			int iDays = vArgs.GetArrayInt(0);
			vArgs.RemoveArrayElement(0);
			Cmd_AllUnused( pSrc, iDays, vArgs );
		}
		break;

#if 0
	case M_List:
		{
			// List all the accounts. 10 at a time.
			iQty = vArgs.MakeArraySize();
			int iStart = pVerb ? vArgs.GetArrayInt(1) : 0;

			break;
		}
#endif

	default:
		DEBUG_CHECK(0);
		return HRES_INTERNAL_ERROR;
	}

	return NO_ERROR;
}


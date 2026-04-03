//
// Caccount.h
// Copyright Menace Software (www.menasoft.com).
//

#ifndef _INC_CACCOUNT_H
#define _INC_CACCOUNT_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "CResourceObj.h"
#include "CSocket.h"
#include "ctime.h"
#include "sphereproto.h"
#include "cresourcebase.h"
#include "caccountbase.h"

class CAccount : public CResourceObj
{
	// RES_Account

	// How much detail are we allowed / want to see ?
	//  log messages on subjects / level of detail ?
	// console should have an account ?
	// reflect these into some sort of public database ?

public:
	CAccount( LPCTSTR pszName, bool fGuest = false );
	virtual ~CAccount();

	virtual CGString GetName() const
	{
		return( m_sName );
	}

	static bool CheckBlockedEmail( LPCTSTR pszEmail);
	static int NameStrip( TCHAR* pszNameOut, LPCTSTR pszNameInp, int iNameSize = MAX_ACCOUNT_NAME_SIZE );
	static PLEVEL_TYPE GetPrivLevelText( LPCTSTR pszFlags );

	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script

	void s_WriteProps( CScript& s, bool fTitle );

	void OnTick();

	bool Block( CScriptConsole* pSrc, DWORD dwMinutes );
	bool ScheduleEmailMessage( CSphereUID iEmailMessage );

	CGString GetPassword() const;
	void SetPassword( LPCTSTR pszPassword );
	CGString GetXPassword() const;
	void SetXPassword( LPCTSTR pszPassword );

	void ClearPassword()
	{
		m_sCurPassword.Empty();	// can be set on next login.
	}
	bool CheckPassword( LPCTSTR pszPassword );
	LPCTSTR GetNewPassword() const
	{
		return( m_sNewPassword );
	}
	void SetNewPassword( LPCTSTR pszPassword );

	bool EmailPassword();
	bool SetEmailAddress( LPCTSTR pszEmail, bool fCheckBlockHost );

	bool IsPrivFlag( WORD wPrivFlags ) const
	{	// PRIV_GM
		return(( m_PrivFlags & wPrivFlags ) ? true : false);
	}
	void SetPrivFlags( WORD wPrivFlags )
	{
		m_PrivFlags |= wPrivFlags;
	}
	void ClearPrivFlags( WORD wPrivFlags )
	{
		m_PrivFlags &= ~wPrivFlags;
	}
	void TogPrivFlags( WORD wPrivFlags, CGVariant& vVal );
	WORD GetPrivFlags() const
	{
		return m_PrivFlags;
	}
	int GetPrivLevel() const
	{
		return( m_PrivLevel );	// PLEVEL_Counsel
	}
	void SetPrivLevel( PLEVEL_TYPE plevel );

	void OnLogin( CClient* pClient );
	void OnLogout( CClient* pClient );

	int DetachChar( CChar* pChar );
	int AttachChar( CChar* pChar );
	void DeleteAllChars();

	virtual void DeleteThis();

private:
	bool SendOutgoingMail( CSphereUID uidMsg );

public:
	CLanguageID m_lang;			// UNICODE language pref. (ENU=english)
	CGString m_sChatName;		// Chat System Name may be different.

	// int m_iTimeZone;			// Time zone i typically play from.
	int m_Total_Connect_Time;	// Previous total amount of time in game. (minutes) "TOTALCONNECTTIME"

	CSocketAddressIP m_Last_IP;	// last ip i logged in from.
	CGTime m_dateLastConnect;	// The last date i logged in. (use localtime())
	int  m_Last_Connect_Time;	// Amount of time spent online last time. (in minutes)

	CSocketAddressIP m_First_IP;	// first ip i logged in from.
	CGTime m_dateFirstConnect;	// The first date i logged in. (use localtime())

	CSphereUID m_uidLastChar;		// Last char i logged in with.
	CUIDRefArray m_Chars;		// list of chars attached to this account.

	CGString m_sEMail;			// My auto event notifier.
	BYTE m_iEmailFailures;		// How many times has this email address failed ?
	CUIDRefArray m_EMailSchedule;	// RES_EMailMsg id's of msgs scheduled.

	// TAG_COMMENT=X
	// TAG_JAIL_RELEASETIME=X = When in system ticks to release from jail.
	// TAG_UNBLOCK_TIME=X	= time in server run ticks.
	CVarDefArray m_TagDefs;		// attach extra tags here. GM comments etc.

protected:
	DECLARE_MEM_DYNAMIC;
public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CACCOUNTPROP(a,b,c) P_##a,
#include "caccountprops.tbl"
#undef CACCOUNTPROP
		P_QTY,
	};
	enum M_TYPE_
	{
#define CACCOUNTMETHOD(a,b,c,d) M_##a,
#include "caccountmethods.tbl"
#undef CACCOUNTMETHOD
		M_QTY,
	};
	static const CScriptMethod sm_Methods[M_QTY+1];
	static const CScriptProp sm_Props[P_QTY+1];
#ifdef USE_JSCRIPT
#define CACCOUNTMETHOD(a,b,c,d) JSCRIPT_METHOD_DEF(a)
#include "caccountmethods.tbl"
#undef CACCOUNTMETHOD
#endif

private:
	PLEVEL_TYPE m_PrivLevel;

	CGString m_sName;			// Name = no spaces. case independant.
	CGString m_sCurPassword;	// Accounts auto-generated but never used should not last long !
	CGString m_sNewPassword;	// The new password will be transfered when they use it.

	// Selected options attached to the account.
#define PRIV_SERVER			0x0001	// This is a server account. CServerDef - Same name
#define PRIV_GM				0x0002	// Acts as a GM (dif from having PLEVEL_GM)

#define PRIV_GM_PAGE		0x0008	// can Listen to GM pages or not.
#define PRIV_HEARALL		0x0010	// I can hear everything said by people of lower plevel
#define PRIV_ALLMOVE		0x0020	// I can move all things. (GM only)
#define PRIV_DETAIL			0x0040	// Show combat/script error detail messages
#define PRIV_DEBUG			0x0080	// Show all objects as boxes and chars as humans.
#define PRIV_EMAIL_VALID	0x0100	// The email address has been validated.
#define PRIV_PRIV_HIDE		0x0200	// Show the GM title and Invul flags.

#define PRIV_JAILED			0x0800	// Must be /PARDONed from jail.
#define PRIV_BLOCKED		0x2000	// The account is blocked.
#define PRIV_ALLSHOW		0x4000	// Show even the offline chars.
#define PRIV_TEMPORARY		0x8000	// Delete this account when logged out.

	WORD m_PrivFlags;			// optional privileges for char (bit-mapped)
};

class CAccountMgr : public CResourceObj
{
	// The full accounts database. RES_AccountMgr
	// Stuff saved in *ACCT.SCP file.
	friend class CAccount;

public:
	CAccountMgr();
	~CAccountMgr();

	virtual CGString GetName() const	// ( every object must have at least a name )
	{
		return "AccountMgr";
	}

	// Accounts.
	CAccountPtr Account_Get( int index );
	bool Account_Load( CScript& s, bool fChanges );

	bool Account_SaveAll();
	bool Account_LoadAll( bool fChanges = true, bool fClearChanges = false );
	int Account_GetCount() const
	{
		return( m_Accounts.GetSize());
	}

	// Search for accounts.
	// By: Name, ChatName, Email and last CreatedIP.
	CAccountPtr Account_FindNameChecked( LPCTSTR pszName );
	CAccountPtr Account_FindNameCheck( LPCTSTR pszName );
	CAccountPtr Account_FindChatName( LPCTSTR pszChatName );
	CAccountPtr Account_FindEmail( LPCTSTR pszEmail );
	CAccountPtr Account_FindCreatedIP( CSocketAddressIP dwIP  );

	CAccountPtr Account_Create( LPCTSTR pszName );

	CAccountPtr Account_CreateEmail( CScriptConsole* pSrc, LPCTSTR pszName, LPCTSTR pszEmail );
	CAccountPtr Account_CreateNew( CScriptConsole* pSrc, LPCTSTR pszName, LPCTSTR pszPassword );

	bool Account_PasswordEmail( CScriptConsole* pSrc, LPCTSTR pszEmail );

	void Account_Add( CAccount* pAccount );
	void Account_Cleanup( CAccount* pAccount );
	void Account_Delete( CAccount* pAccount );

	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vRetVal, CScriptConsole* pSrc );

	void Empty();
	void OnTick();

private:
	void Cmd_AllUnused( CScriptConsole* pSrc, int iDays, CGVariant& vCommand );

public:
	CSCRIPT_CLASS_DEF1();
	enum M_TYPE_
	{
		// Account manager commands.
#define CACCOUNTMGRMETHOD(a,b,c,d) M_##a,
#include "caccountmgrmethods.tbl"
#undef CACCOUNTMGRMETHOD
		M_QTY,
	};
	static const CScriptMethod sm_Methods[M_QTY+1];
#ifdef USE_JSCRIPT
#define CACCOUNTMGRMETHOD(a,b,c,d) JSCRIPT_METHOD_DEF(a)
#include "caccountmgrmethods.tbl"
#undef CACCOUNTMGRMETHOD
#endif

	CCryptText m_Crypt;	// Crypt the passwords.

private:
	CResLockNameArray<CAccount> m_Accounts;		// Name sorted list.
	CUIDArray m_UIDs;	// UID array pointers to the accounts.
};

extern CAccountMgr g_Accounts;	// All the player accounts. 

#endif


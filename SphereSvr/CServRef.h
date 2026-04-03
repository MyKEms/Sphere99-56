//
// CServerDef.h
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#ifndef _INC_CSERVERDEF_H
#define _INC_CSERVERDEF_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

enum SERV_STAT_TYPE	// same as SDP_StatAccounts
{
	SERV_STAT_ACCOUNTS,
	SERV_STAT_ALLOCS,	// How many calls to new()
	SERV_STAT_CHARS,
	SERV_STAT_CLIENTS,	// How many clients does it say it has ? (use as % full)
	SERV_STAT_ITEMS,
	SERV_STAT_MEMORY,	//
	SERV_STAT_QTY,
};

enum ACCAPP_TYPE	// types of new account applications.
{
	ACCAPP_Closed=0,	// 0=Closed. Not accepting more.
	ACCAPP_EmailApp,	// 1=Must send email to apply.
	ACCAPP_Free,		// 2=Anyone can just log in and create a full account.
	ACCAPP_GuestAuto,	// You get to be a guest and are automatically sent email with u're new password.
	ACCAPP_GuestTrial,	// You get to be a guest til u're accepted for full by an Admin.
	ACCAPP_Other,		// specified but other ?
	ACCAPP_Unspecified,	// Not specified.
	ACCAPP_WebApp,		// Must fill in a web form and wait for email response
	ACCAPP_WebAuto,		// Must fill in a web form and automatically have access
	ACCAPP_XGM,			// Everyone is a GM.
	ACCAPP_QTY,
};

class CServerDef : public CResourceDef	// Array of servers we point to.
{	// NOTE: g_Serv = this server.
public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CSERVERDEFPROP(a,b,c) P_##a,
#include "cserverdefprops.tbl"
#undef CSERVERDEFPROP
		P_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];
	static LPCTSTR const sm_AccAppTable[ ACCAPP_QTY+1 ];

private:
	CGString m_sName;	// What the name should be. Fill in from ping.
	CServTime m_timeLastPoll;		// Last poll time in (if polling)
	CServTime m_timeLastValid;	// Last valid poll time in 
	CGTime	m_dateLastValid;

	CServTime  m_timeCreate;	// When added to the list ? 0 = at start up.

	// Status read from returned string.
	CGString m_sStatus;	// last returned status string.

	// statistics
	DWORD m_dwStat[ SERV_STAT_QTY ];

	// Moving these chars to this server.
	CSphereThread m_TaskCharMove;	// Create a brand new thread to do the move.
	CUIDRefArray m_MoveChars;

protected:
	int m_iClientsAvg;	// peak per day of clients.
	CGString m_sServVersion;	// Version string this server is using.

public:
	CSocketNamedAddr m_ip;	// socket (may have host name) and port.
	CCryptVersion m_ClientVersion;		// SPHERE_CLIENT_VER 12537 = Base client crypt we are using.

	// Breakdown the string. or filled in locally.
	signed char m_TimeZone;	// Hours from GMT. +5=EST
	CGString m_sEMail;		// Admin email address.
	CGString m_sURL;			// URL for the server.
	CGString m_sLang;		// Use CLanguageID instead ?
	CGString m_sNotes;		// Notes to be listed for the server.
	ACCAPP_TYPE m_eAccApp;	// types of new account applications.
	CGString m_sRegisterPassword;	// The password  the server uses to id itself to the registrar.

private:
	static THREAD_ENTRY_RET _cdecl MoveCharEntryProc( void* lpThreadParameter );
	int MoveCharToServer( CGSocket &sock, CChar* pChar );
	bool MoveCharsToServer();
	void SetStatusFail( LPCTSTR pszTrying );

public:
	LPCTSTR GetStatus() const
	{
		return(m_sStatus);
	}
	LPCTSTR GetServerVersion() const
	{
		return( m_sServVersion );
	}
	CServTimeBase GetCreateTime() const
	{
		return m_timeCreate;
	}

	DWORD StatGet( SERV_STAT_TYPE i ) const
	{
		ASSERT( i>=0 && i<SERV_STAT_QTY );
		return( m_dwStat[i] );
	}
	void StatInc( SERV_STAT_TYPE i )
	{
		ASSERT( i>=0 && i<SERV_STAT_QTY );
		m_dwStat[i]++;
	}
	void StatDec( SERV_STAT_TYPE i )
	{
		ASSERT( i>=0 && i<SERV_STAT_QTY );
		DEBUG_CHECK( m_dwStat[i] );
		m_dwStat[i]--;
	}
	void StatChange( SERV_STAT_TYPE i, int iChange )
	{
		ASSERT( i>=0 && i<SERV_STAT_QTY );
		m_dwStat[i] += iChange;
	}
	void SetStat( SERV_STAT_TYPE i, DWORD dwVal )
	{
		ASSERT( i>=0 && i<SERV_STAT_QTY );
		m_dwStat[i] = dwVal;
	}

	virtual CGString GetName() const { return( m_sName ); }
	void SetName( LPCTSTR pszName );

	int GetConnectStatus() const;
	HRESULT ParseStatus( LPCTSTR pszStatus, bool fStore );
	bool PollStatus();	// Do this on a seperate thread.
	bool QueueCharToServer( CChar* pChar = NULL );

	virtual int GetAgeHours() const;

	bool IsSame( const CServerDef* pServNew ) const
	{
		if ( m_sRegisterPassword.IsEmpty())
			return( true );
		return( ! _stricmp( m_sRegisterPassword, pServNew->m_sRegisterPassword ));
	}

	void SetValidTime();
	int GetTimeSinceLastValid() const;
	void SetPollTime();
	int GetTimeSinceLastPoll() const;

	void ClientsAvgCalc();
	void ClientsAvgReset();
	int GetClientsAvg() const
	{
		if ( m_iClientsAvg )
			return( m_iClientsAvg );
		return( StatGet(SERV_STAT_CLIENTS));
	}

	virtual HRESULT s_PropGet( int iProp, CGVariant& vVal, CScriptConsole* pSrc );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );

	void s_WriteServerData( CScript& s );
	void s_WriteCreated( CScript& s );

	void addToServersList( CUOCommandServer& cmdsvr, int iThis ) const;

	CServerDef( UID_INDEX rid, LPCTSTR pszName, CSocketAddressIP dwIP );
	~CServerDef();
};

#endif // _INC_CSERVERDEF_H

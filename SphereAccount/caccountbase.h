//
// Caccountbase.h
// Preliminary account stuff.
// Copyright Menace Software (www.menasoft.com).

#ifndef _INC_CACCOUNTBASE_H
#define _INC_CACCOUNTBASE_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "crefobj.h"
#include "CScriptObj.h"
#include "carraysort.h"
#include "CThread.h"

class CChar;
class CAccount;
class CClient;

typedef CRefPtr<CClient> CClientPtr;
typedef CRefPtr<CAccount> CAccountPtr;
typedef CRefPtr<CAccount> CAccountLock;

enum PLEVEL_TYPE		// Priv levels. (space these out?)
{
	PLEVEL_NoAccount = -1,
	PLEVEL_Guest = 0,		// 0 = This is just a guest account. (cannot PK)
	PLEVEL_Player,			// 1 = Player or NPC.
	PLEVEL_Counsel,			// 2 = Can travel and give advice.
	PLEVEL_Seer,			// 3 = Can make things and NPC's but not directly bother players.
	PLEVEL_GM,				// 4 = GM command clearance
	PLEVEL_Dev,				// 5 = Not bothererd by GM's
	PLEVEL_Admin,			// 6 = Can switch in and out of gm mode. assign GM's
	PLEVEL_Owner,			// 7 = Highest allowed level.
	PLEVEL_QTY,
};

class CAccountConsole : public CScriptConsole
{
	// A CScriptConsole attached to an CAccount
	// A base class for any class that can act like a console and issue commands.
	// CClient, CChar, CServer, CFileConsole
public:
	CAccountPtr m_pAccount;
public:
	virtual CAccountPtr GetAccount() const
	{
		return m_pAccount;
	}
	virtual CGString GetName() const;	// name of the console.
	virtual int GetPrivLevel() const;	// What privs do i have ? PLEVEL_TYPE Higher = more.

	// Are we allowed this priv ?
	// PRIV_GM type flags

	bool IsPrivFlag( WORD wPrivFlags ) const;
	void SetPrivFlags( WORD wPrivFlags );
	void ClearPrivFlags( WORD wPrivFlags );
};

//*************************************************

template <class TYPE>
class CResNameSortArray : public CGRefSortArray<TYPE, LPCTSTR>
{
	// name sorted array.
	// TYPE based on CResourceObj
	virtual int CompareData( TYPE* pData1, TYPE* pData2 ) const
	{
		// Compare a data record to another data record.
		ASSERT( pData1 );
		ASSERT( pData2 );
		return( _stricmp( pData1->GetName(), pData2->GetName()));
	}
	virtual int CompareKey( LPCTSTR pszID, TYPE* pObj ) const
	{
		ASSERT( pszID );
		ASSERT( pObj );
		return( _stricmp( pszID, pObj->GetName()));
	}
public:
	bool RemoveArg( TYPE* pObj )
	{
		for (int i = 0; i < (int)this->GetSize(); i++)
		{
			if (this->GetAt(i) == pObj)
			{
				this->RemoveAt(i);
				return true;
			}
		}
		return false;
	}
};

//*************************************************

template<class TYPE>
class CResLockNameArray : public CResNameSortArray<TYPE>, public CThreadLockableObj
{
	// Lockable, name sorted array.
	// TYPE based on CResourceObj
public:
	bool RemoveArg( TYPE* pObj )
	{
		CThreadLockPtr lock( this );
		return CResNameSortArray<TYPE>::RemoveArg( pObj );
	}
	int AddSortKey( TYPE* pNew, LPCTSTR key )
	{
		CThreadLockPtr lock( this );
		return( CResNameSortArray<TYPE>::AddSortKey( pNew, key ));
	}

	void IncRefCount() { /* stub */ }
	void StaticDestruct() { /* stub */ }

	// FindKey, RemoveAt must use a lock outside as well ! (for index to be meaningful)
};

#endif	// _INC_CACCOUNTBASE_H

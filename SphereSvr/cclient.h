//
// CClient.h
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)

#ifndef _INC_CCLIENT_H
#define _INC_CCLIENT_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class CPartyDef : public CGObListRec, public CRefObjDef
{
	// a list of characters in the party.
protected:
	DECLARE_LISTREC_REF2(CPartyDef);
private:
	CSphereUID m_uidMaster;	// Also in m_Chars (which is proabably sorted)
	CUIDRefArray m_Chars;	// CChar

private:
	bool SendMemberMsg( CChar* pCharDest, const CUOExtData* pExtData, int iLen );
	void SendAll( const CUOExtData* pExtData, int iLen );
	void SendRemoveList( CChar* pCharRemove, CSphereUID uidAct );

public:
	static bool AcceptEvent( CChar* pCharAccept, CSphereUID uidInviter );
	static bool DeclineEvent( CChar* pCharDecline, CSphereUID uidInviter );
	static void MessageClient( CClient* pClient, CSphereUID uidSrc, const NCHAR* pText, int ilenmsg );

	bool IsInParty( const CChar* pChar ) const
	{
		ASSERT(pChar);
		return( m_Chars.FindObj( pChar ) >= 0 );
	}
	bool IsPartyMaster( const CChar* pChar ) const
	{
		ASSERT(pChar);
		return( m_uidMaster == pChar->GetUID());
	}

	bool Disband( CSphereUID uidMaster );
	int AttachChar( CChar* pChar );
	int DetachChar( CChar* pChar );

	void SetLootFlag( CChar* pChar, bool fSet );
	bool GetLootFlag( const CChar* pChar );

	void MessageAll( CSphereUID uidSrc, const NCHAR* pText, int ilenmsg );
	bool MessageMember( CSphereUID uidDst, CSphereUID uidSrc, const NCHAR* pText, int ilenmsg );
	void SysMessageAll( LPCTSTR pText );

	void SendAddList( CSphereUID uid, CChar* pCharDest );

	bool RemoveChar( CSphereUID uid, CSphereUID uidAct );
	void AcceptMember( CChar* pChar );

	CPartyDef( CChar* pCharInvite, CChar* pCharAccept );
	~CPartyDef() {}
};

#endif	// _INC_CCLIENT_H

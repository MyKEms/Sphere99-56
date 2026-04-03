//
// cQuest.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.

//*****************************************************************
// -CPartyDef

CPartyDef::CPartyDef( CChar* pChar1, CChar *pChar2 )
{
	// pChar1 = the master.
	ASSERT(pChar1);
	ASSERT(pChar2);
	m_uidMaster = pChar1->GetUID();
	pChar1->m_pParty.SetRefObj( this );
	pChar2->m_pParty.SetRefObj( this );
	AttachChar(pChar1);
	AttachChar(pChar2);
	SendAddList( UID_INDEX_CLEAR, NULL );	// send full list to all.
}

int CPartyDef::AttachChar( CChar* pChar )
{
	// RETURN:
	//  index of the char in the group. -1 = not in group.
	// NOTE: Clear Loot flag ?

	pChar->m_TagDefs.RemoveKey("PARTY_CANLOOTME");

	return m_Chars.AttachObj( pChar );
}

int CPartyDef::DetachChar( CChar* pChar )
{
	// RETURN:
	//  index of the char in the group. -1 = not in group.
	return( m_Chars.DetachObj( pChar ));
}

void CPartyDef::SetLootFlag( CChar* pChar, bool fSet )
{
	ASSERT( pChar );

	pChar->m_TagDefs.SetKeyInt("PARTY_CANLOOTME",fSet);

	pChar->WriteString( fSet ?
		"You have chosen to allow your party to loot your corpse" :
		"You have chosen to prevent your party from looting your corpse" );
}

bool CPartyDef::GetLootFlag( const CChar* pChar )
{
	return( pChar->m_TagDefs.FindKeyInt("PARTY_CANLOOTME"));
}

void CPartyDef::SysMessageAll( LPCTSTR pText )
{
	// WriteString to all members of the party.
	int iQty = m_Chars.GetSize();
	for ( int i=0; i<iQty; i++ )
	{
		CCharPtr pChar = g_World.CharFind( m_Chars.GetAt(i));
		ASSERT(pChar);
		pChar->WriteString( pText );
	}
}

bool CPartyDef::SendMemberMsg( CChar* pCharDest, const CUOExtData* pExtData, int iLen )
{
	if ( pCharDest == NULL )
	{
		SendAll( pExtData, iLen );
		return( true );
	}

	// Weirdness check. cleanup
	if ( pCharDest->m_pParty != this )
	{
		if ( DetachChar( pCharDest ) >= 0 )	// this is bad!
			return( false );
		return( true );
	}
	else if ( ! m_Chars.IsObjIn( pCharDest ))
	{
		pCharDest->m_pParty.ReleaseRefObj();
		return( true );
	}

	if ( pCharDest->IsClient())
	{
		CClientPtr pClient = pCharDest->GetClient();
		ASSERT(pClient);
		pClient->addExtData( EXTDATA_Party_Msg, pExtData, iLen );
	}

	return( true );
}

void CPartyDef::SendAll( const CUOExtData* pExtData, int iLen )
{
	// Send this to all members of the party.
	int iQty = m_Chars.GetSize();
	for ( int i=0; i<iQty; i++ )
	{
		CCharPtr pChar = g_World.CharFind(m_Chars.GetAt(i));
		ASSERT(pChar);
		if ( ! SendMemberMsg( pChar, pExtData, iLen ))
		{
			iQty--;
			i--;
		}
	}
}

void CPartyDef::AcceptMember( CChar* pChar )
{
	// This person has accepted to be part of the party.
	ASSERT(pChar);

	SendAddList( pChar->GetUID(), NULL );	// tell all that there is a new member.

	pChar->m_pParty.SetRefObj( this );
	AttachChar(pChar);

	// "You have been added to the party"
	// NOTE: We don't have to send the full party to everyone. just the new guy.
	SendAddList( UID_INDEX_CLEAR, pChar );
}

void CPartyDef::SendAddList( CSphereUID uid, CChar* pCharDest )
{
	// Send out the full party manifest to all the clients.

	CUOExtData ExtData;
	ExtData.Party_Msg_Data.m_code = PARTYMSG_Add;

	int iQty;
#if 0
	if ( uid.IsValidObjUID())
	{
		// Send just this one.
		ExtData.Party_Msg_Data.m_uids[0] = uid;
		iQty = 1;
	}
	else
#endif
	{
		// Send All.
		iQty = m_Chars.GetSize();
		ASSERT(iQty);
		for ( int i=0; i<iQty; i++ )
		{
			ExtData.Party_Msg_Data.m_uids[i] = m_Chars.GetAt(i);
		}
	}

	// Now send out the list to all that are clients.
	ExtData.Party_Msg_Data.m_Qty = iQty;
	SendMemberMsg( pCharDest, &ExtData, (iQty*sizeof(DWORD))+2 );
}

void CPartyDef::SendRemoveList( CChar* pCharRemove, CSphereUID uidAct )
{
	// Send out the party manifest to the client.
	// uid = remove just this char. 0=all.
	// uidAct = Who did the removing.

	CUOExtData ExtData;
	ExtData.Party_Msg_Data.m_code = PARTYMSG_Remove;

	int iQty;
	if ( pCharRemove )
	{
		// just removing this one person.
		ExtData.Party_Msg_Data.m_uids[0] = pCharRemove->GetUID();
		iQty = 1;
	}
	else
	{
		// We are disbanding.
		iQty = m_Chars.GetSize();
		ASSERT(iQty);
		for ( int i=0; i<iQty; i++ )
		{
			ExtData.Party_Msg_Data.m_uids[i] = m_Chars.GetAt(i);
		}
	}

	ExtData.Party_Msg_Data.m_Qty = iQty;	// last uid is really the source.
	ExtData.Party_Msg_Data.m_uids[iQty++] = uidAct;

	SendAll( &ExtData, (iQty*sizeof(DWORD))+2 );
}

void CPartyDef::MessageClient( CClient* pClient, CSphereUID uidSrc, const NCHAR* pText, int ilenmsg ) // static
{
	// Message to a single client

	ASSERT(pClient);
	if ( pText == NULL )
		return;

	CUOExtData ExtData;
	ExtData.Party_Msg_Rsp.m_code = PARTYMSG_Msg;
	ExtData.Party_Msg_Rsp.m_UID = uidSrc;
	if ( ilenmsg > MAX_TALK_BUFFER )
		ilenmsg = MAX_TALK_BUFFER;
	memcpy( ExtData.Party_Msg_Rsp.m_msg, pText, ilenmsg );

	pClient->addExtData( EXTDATA_Party_Msg, &ExtData, ilenmsg+5 );
}

bool CPartyDef::MessageMember( CSphereUID uidTo, CSphereUID uidSrc, const NCHAR* pText, int ilenmsg )
{
	// Message to a single members of the party.
	if ( pText == NULL )
		return false;
	if ( g_Log.IsLoggedGroupMask( LOG_GROUP_PLAYER_SPEAK ))
	{
		// Convert from unicode
		// g_Log.Event( LOG_GROUP_PLAYER_SPEAK, LOGL_TRACE, 
		//   "%x:'%s' PartyMsg '%s' mode=%d" LOG_CR, m_Socket.GetSocket(), (LPCTSTR) m_pChar->GetName(), szText, mode );
	}

	CCharPtr pChar = g_World.CharFind(uidTo);
	if ( pChar == NULL )
		return( false );

	CUOExtData ExtData;
	ExtData.Party_Msg_Rsp.m_code = PARTYMSG_Msg;
	ExtData.Party_Msg_Rsp.m_UID = uidSrc;
	if ( ilenmsg > MAX_TALK_BUFFER )
		ilenmsg = MAX_TALK_BUFFER;
	memcpy( ExtData.Party_Msg_Rsp.m_msg, pText, ilenmsg );

	return SendMemberMsg( pChar, &ExtData, ilenmsg+5 );
}

void CPartyDef::MessageAll( CSphereUID uidSrc, const NCHAR* pText, int ilenmsg )
{
	// Message to all members of the party.

	if ( pText == NULL )
		return;

	if ( g_Log.IsLoggedGroupMask( LOG_GROUP_PLAYER_SPEAK ))
	{
		// Convert from unicode
		// g_Log.Event( LOG_GROUP_PLAYER_SPEAK, LOGL_TRACE, "%x:'%s' PartyMsg '%s' mode=%d" LOG_CR, m_Socket.GetSocket(), (LPCTSTR) m_pChar->GetName(), szText, mode );
	}

	CUOExtData ExtData;
	ExtData.Party_Msg_Rsp.m_code = PARTYMSG_Msg;
	ExtData.Party_Msg_Rsp.m_UID = uidSrc;
	if ( ilenmsg > MAX_TALK_BUFFER )
		ilenmsg = MAX_TALK_BUFFER;
	memcpy( ExtData.Party_Msg_Rsp.m_msg, pText, ilenmsg );

	SendAll( &ExtData, ilenmsg+5 );
}

bool CPartyDef::Disband( CSphereUID uidMaster )
{
	// Make sure i am the master.
	if ( ! m_Chars.GetSize())
	{
		return( false );
	}

	if ( m_uidMaster != uidMaster )
	{
		return false;
	}

	SysMessageAll("Your party has disbanded.");
	SendRemoveList( NULL, uidMaster );

	int iQty = m_Chars.GetSize();
	ASSERT(iQty);
	for ( int i=0; i<iQty; i++ )
	{
		CCharPtr pChar = g_World.CharFind(m_Chars.GetAt(i));
		if ( pChar == NULL )
			continue;
		pChar->m_pParty.ReleaseRefObj();
	}

	RemoveSelf();	// should remove itself from the world list.
	delete this;
	return( true );
}

bool CPartyDef::DeclineEvent( CChar* pCharDecline, CSphereUID uidInviter )	// static
{
	// This should happen after a timeout as well.
	// " You notify %s that you do not wish to join the party"

	return( true );
}

bool CPartyDef::AcceptEvent( CChar* pCharAccept, CSphereUID uidInviter )	// static
{
	// We are accepting the invite to join a party
	// Check to see if the invite is genuine. ??? No security Here !!!

	// Party master is only one that can add ! GetAt(0)

	ASSERT( pCharAccept );

	CCharPtr pCharInviter = g_World.CharFind(uidInviter);
	if ( pCharInviter == NULL )
	{
		return false;
	}

	CRefPtr<CPartyDef> pParty = pCharInviter->m_pParty;

	if ( ! pCharAccept->m_pParty.IsValidRefObj())	// Aready in a party !
	{
		if ( pParty == pCharAccept->m_pParty )	// already in this party
			return( true );

		// So leave previous party.
		pCharAccept->m_pParty->RemoveChar( pCharAccept->GetUID(), pCharAccept->GetUID() );
		DEBUG_CHECK(!pCharAccept->m_pParty.IsValidRefObj());
		pCharAccept->m_pParty.ReleaseRefObj();
	}

	CGString sMsg;
	sMsg.Format( "%s has joined the party", (LPCTSTR) pCharAccept->GetName() );

	if ( ! pParty.IsValidRefObj())
	{
		// create the party now.
		pParty = new CPartyDef( pCharInviter, pCharAccept );
		ASSERT(pParty);
		g_World.m_Parties.InsertHead( pParty );
		pCharInviter->WriteString( sMsg );
	}
	else
	{
		// Just add to existing party. Only the party master can do this !
		pParty->SysMessageAll( sMsg );	// tell everyone already in the party about this.
		pParty->AcceptMember( pCharAccept );
	}

	pCharAccept->WriteString( "You have been added to the party" );
	return( true );
}

bool CPartyDef::RemoveChar( CSphereUID uid, CSphereUID uidAct )
{
	// ARGS:
	//  uid = Who is being removed.
	//  uidAct = who removed this person. (Only the master or self can remove)
	//
	// NOTE: remove of the master will cause the party to disband.

	if ( ! m_Chars.GetSize())
	{
		return( false );
	}

	if ( uid != uidAct && uidAct != m_uidMaster )
	{
		// cheating ?
		// must be removed by self or master !
		return( false );
	}

	CCharPtr pChar = g_World.CharFind(uid);	// who is to be removed.
	if ( pChar == NULL || ! m_Chars.IsObjIn(pChar))
	{
		return( false );
	}

	if ( uid == m_uidMaster )
	{
		return( Disband( m_uidMaster ));
	}

	LPCTSTR pszForceMsg = ( (DWORD) uidAct != (DWORD) uid ) ? "been removed from" : "left";

	// Tell the kicked person they are out
	pChar->Printf( "You have %s the party", pszForceMsg );

	// Send the remove message to all.
	SendRemoveList( pChar, uidAct );

	DetachChar( pChar );
	pChar->m_pParty.ReleaseRefObj();

	if ( m_Chars.GetSize() <= 1 )
	{
		// Disband the party
		// "The last person has left the party"
		Disband( m_uidMaster );
	}
	else
	{
		// Tell the others he is gone
		CGString sMsg;
		sMsg.Format( "%s has %s your party", (LPCTSTR) pChar->GetName(), (LPCTSTR) pszForceMsg );
		SysMessageAll(sMsg);
	}
	return true;
}

#if 0

void CPartyDef::ShareKarmaAndFame( int iFame, int iKarma )
{
	// We have all made a kill.
	//
}

#endif

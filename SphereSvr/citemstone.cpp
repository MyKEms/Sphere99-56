//
// CItemStone.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.

//*********************************************************************
// -CItemStone

const CScriptProp CItemStone::sm_Props[CItemStone::P_QTY+1] =
{
#define CITEMSTONEPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "citemstoneprops.tbl"
#undef CITEMSTONEPROP
	NULL,
};

#ifdef USE_JSCRIPT
#define CITEMSTONEMETHOD(a,b,c) JSCRIPT_METHOD_IMP(CItemStone,a)
#include "citemstonemethods.tbl"
#undef CITEMSTONEMETHOD
#endif

const CScriptMethod CItemStone::sm_Methods[CItemStone::M_QTY+1] =
{
#define CITEMSTONEMETHOD(a,b,c) CSCRIPT_METHOD_IMP(a,b,c)
#include "citemstonemethods.tbl"
#undef CITEMSTONEMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP1(ItemStone,CItemStone::sm_Props,CItemStone::sm_Methods,NULL,Item);

CItemStone::CItemStone( ITEMID_TYPE id, CItemDef* pItemDef ) : CItem( id, pItemDef )
{
	// IT_STONE_GUILD
	// IT_STONE_TOWN
	// Town or Guild stone.
	m_itStone.m_iAlign = STONEALIGN_STANDARD;
	m_iDailyDues = 0;

	// track all the stones in the world.
	CResNameSortArray<CItemStone>* pArray = GetStoneArray();
	if ( pArray )
	{
		pArray->AddSortKey( this, GetName());
	}
}

CItemStone::~CItemStone()
{
}

void CItemStone::DeleteThis()
{
	// Must remove early because virtuals will fail in child destructor.

	SetAmount(0);	// Tell everyone we are deleting.

	// Remove this stone from the links of guilds in the world
	CResNameSortArray<CItemStone>* pArray = GetStoneArray();
	if ( pArray )
	{
		pArray->RemoveArg( this );
	}

	// Unlink all my members
	for ( int i=0; i<m_Members.GetSize(); i++ )
	{
		CCharPtr pChar = g_World.CharFind( m_Members.GetAt(i));
		if ( pChar == NULL )
			continue;
		CItemMemoryPtr pMemory = pChar->Memory_FindObj(this);
		if ( pMemory )
		{
			pMemory->DeleteThis();
		}
	}

	// Unlink all my wars.

	CItem::DeleteThis();	
}

MEMORY_TYPE CItemStone::GetMemoryType() const
{
	switch ( GetType())
	{
	case IT_STONE_TOWN: 
		return( MEMORY_TOWN );
	case IT_STONE_GUILD: 
		return( MEMORY_GUILD );
	}
	// Houses have no exclusive memories.
	return( MEMORY_GUARD );
}

CResNameSortArray<CItemStone>* CItemStone::GetStoneArray() const
{
	switch ( GetType())
	{
	case  IT_STONE_GUILD:
		return( &(g_World.m_GuildStones));
	case  IT_STONE_TOWN:
		return( &(g_World.m_TownStones));
	default:
		return NULL;
	}
}

LPCTSTR CItemStone::GetTypeName() const
{
	switch ( GetType())
	{
	case IT_STONE_GUILD:
		return( _TEXT("Guild"));
	case IT_STONE_TOWN:
		return( _TEXT("Town"));
	case IT_STONE_ROOM:
		return( _TEXT("Structure"));
	}
	return( _TEXT("Unk"));
}

LPCTSTR CItemStone::Align_GetName() const
{
	static LPCTSTR const sm_AlignName[] = // STONEALIGN_TYPE
	{
		"Neutral",	// STONEALIGN_STANDARD
		"Order",	// STONEALIGN_ORDER
		"Chaos",	// STONEALIGN_CHAOS
	};
	int iAlign = Align_GetType();
	if ( iAlign >= COUNTOF( sm_AlignName ))
		iAlign = 0;
	return( sm_AlignName[ iAlign ] );
}

bool CItemStone::Member_IsMaster( CChar* pChar ) const
{
	if ( pChar == NULL )
		return false;
	if ( pChar->IsPrivFlag(PRIV_GM))
		return true;
	return( pChar->GetUID() == m_uidMaster );
}
bool CItemStone::Member_IsMember( CChar* pChar ) const
{
	if ( pChar == NULL )
		return false;
	if ( pChar->IsPrivFlag(PRIV_GM))
		return true;
	return( m_Members.IsObjIn(pChar));
}

CCharPtr CItemStone::Member_GetMaster() const
{
	return g_World.CharFind( m_uidMaster );
}

CItemMemoryPtr CItemStone::Member_GetMasterMemory() const
{
	CCharPtr pChar = Member_GetMaster();
	if ( pChar == NULL )
		return NULL;
	return( pChar->Memory_FindObj(this));
}

CItemMemoryPtr CItemStone::Member_FindIndex( int i )
{
	// Find the members memory. 
	// If they dont have one then create one !
	// REUTRN:
	//  NULL = no more.

	if ( ! m_Members.IsValidIndex(i))
		return NULL;

	CCharPtr pChar = g_World.CharFind( m_Members.GetAt(i));
	if ( pChar == NULL || pChar->m_pPlayer == NULL )
	{
		// Delete the member from the list.
		if ( g_Serv.m_iModeCode != SERVMODE_Exiting )
		{
			DEBUG_WARN(( "Stone UID=0%x has mislinked member uid=0%x" LOG_CR, GetUID(), m_Members.GetAt(i)));
		}
		m_Members.RemoveAt(i);
		return Member_FindIndex(i);	// get the next one.
	}

	// Find or create the memory.
	return pChar->Memory_AddObjTypes(this,GetMemoryType());
}

bool CItemStone::Member_AddRecruit( CChar* pChar, bool fMember )
{
	// CLIMODE_TARG_STONE_RECRUIT
	// Set as member or candidate.

	if ( !pChar )
	{
onlyplayers:
		Speak( "Only players can be members!");
		return false;
	}
	if ( ! pChar->IsClient() || ! pChar->m_pPlayer )
	{
		goto onlyplayers;
	}

	CItemStonePtr pStone = pChar->Guild_Find( GetMemoryType());
	if ( pStone && pStone != this )
	{
		CGString sStr;
		sStr.Format( "%s appears to belong to %s. Must resign previous %s", (LPCTSTR) pChar->GetName(), (LPCTSTR) pStone->GetName(), (LPCTSTR) GetTypeName());
		Speak(sStr);
		return false;
	}

	CGString sStr;
	if ( m_Bans.FindObj(pChar))
	{
		// Only master can change this.
		sStr.Format("%s has been banished.", (LPCTSTR) pChar->GetName());
		Speak(sStr);
		return false;
	}

	if ( !fMember && 
		IsType(IT_STONE_TOWN) && IsAttr(ATTR_OWNED))
	{
		// instant member.
		fMember = true;
	}

	if ( !fMember )
	{
		if ( m_Candidates.FindObj(pChar))
		{
			sStr.Format("%s is already a candidate for %s.", (LPCTSTR) pChar->GetName(), (LPCTSTR) GetName());
			Speak(sStr);
			return NULL;
		}

		m_Candidates.AttachObj(pChar);
		return true;
	}

	CItemMemoryPtr pMember = pChar->Memory_AddObjTypes(this, GetMemoryType());
	if (pMember == NULL )
		return false;

	m_Members.AttachObj(pChar);
	Member_ElectMaster();	// just in case this is the first.

	sStr.Format( "%s is now %s of %s", (LPCTSTR) pChar->GetName(), 
		( pChar->GetUID() == m_uidMaster ) ? "master" : "a member",
		(LPCTSTR) GetName());

	Speak(sStr);

	return true;
}

bool CItemStone::IsUniqueName( LPCTSTR pszName ) // static
{
	// Unique guild/town name?
	CResNameSortArray<CItemStone>* pArray = GetStoneArray();
	if ( pArray == NULL )
		return true;
	int i = pArray->FindKey(pszName);
	if ( i < 0 )
		return true;
	return false;
}

void CItemStone::Member_ElectMaster()
{
	// Check who is loyal to who and find the new master.
	if ( GetAmount() == 0 )
		return;	// no reason to elect new if the stone is dead.

	int iResultCode = FixWeirdness();	// try to eliminate bad members.
	if ( iResultCode )
	{
		// The stone is bad ?
		// iResultCode
	}

	// Validate the items and Clear the votes field
	int i=0;
	for (; i< m_Members.GetSize(); i++ )
	{
		CItemMemoryPtr pMember = Member_FindIndex(i);
		if ( pMember == NULL )
			break;
		pMember->Guild_SetVotes(0);
	}

	// Now tally the votes.
	for ( i=0; i< m_Members.GetSize(); i++ )
	{
		CItemMemoryPtr pMember = Member_FindIndex(i);
		if ( pMember == NULL )
			break;

		CSphereUID uidVote = pMember->Guild_GetLoyalTo();
		CCharPtr pCharVoteFor = g_World.CharFind(uidVote);
		if ( pCharVoteFor != NULL )
		{
			CItemMemoryPtr pMemberVoteFor = pCharVoteFor->Memory_FindObj(this);
			if ( pMemberVoteFor != NULL )
			{
				pMemberVoteFor->Guild_SetVotes( pMemberVoteFor->Guild_GetVotes() + 1 );
				continue;
			}
		}

		// not valid to vote for. change to self.
		pMember->Guild_SetLoyalTo(NULL);
		// Assume I voted for myself.
		pMember->Guild_SetVotes( pMember->Guild_GetVotes() + 1 );
	}

	// Find out who won.
	bool fTie = false;
	CItemMemoryPtr pMemberHighest = NULL;
	for ( i=0; i< m_Members.GetSize(); i++ )
	{
		CItemMemoryPtr pMember = Member_FindIndex(i);
		if ( pMember == NULL )
			break;
		if ( pMemberHighest == NULL )
		{
			pMemberHighest = pMember;
			continue;
		}
		if ( pMember->Guild_GetVotes() == pMemberHighest->Guild_GetVotes())
		{
			fTie = true;
		}
		if ( pMember->Guild_GetVotes() > pMemberHighest->Guild_GetVotes())
		{
			fTie = false;
			pMemberHighest = pMember;
		}
	}

	// In the event of a tie, leave the current master as is
	if ( ! fTie && pMemberHighest )
	{
		CCharPtr pChar = PTR_CAST(CChar,pMemberHighest->GetParent());
		if ( pChar )
		{
			m_uidMaster = pChar->GetUID();
		}
	}
}

void CItemStone::UpdateTownName()
{
	// For town stones.
	if ( ! IsTopLevel())
		return;
	CRegionPtr pArea = GetTopPoint().GetRegion(( IsType(IT_STONE_TOWN)) ? REGION_TYPE_AREA : REGION_TYPE_ROOM );
	if ( pArea )
	{
		pArea->SetName( GetIndividualName());
	}
}

bool CItemStone::MoveTo( CPointMap pt )
{
	// Put item on the ground here.
	if ( IsType(IT_STONE_TOWN) || IsType(IT_STONE_ROOM))
	{
		UpdateTownName();
	}
	return CItem::MoveTo(pt);
}

bool CItemStone::SetName( LPCTSTR pszName )
{
	// re-sort g_World.m_GuildStones or g_World.m_TownStones
	if ( ! CItem::SetName( pszName ))
		return( false );

	// Re-sort in the array.
	CResNameSortArray<CItemStone>* pArray = GetStoneArray();
	if ( pArray )
	{
		pArray->RemoveArg( this );
		pArray->AddSortKey( this, GetName());
	}

	if ( IsTopLevel() && ( IsType(IT_STONE_TOWN) || IsType(IT_STONE_ROOM)))
	{
		// If this is a town then set the whole regions name.
		UpdateTownName();
	}
	return( true );
}

//*********************************************************************************
// War

bool CItemStone::War_CanWarWith( CItemStone* pEnemyStone ) const
{
	// Is waring allowed?
	if ( pEnemyStone == NULL )
		return false;
	// Make sure they have actual members first
	if ( ! pEnemyStone->m_Members.GetSize())
		return false;
	// Make sure we do.
	if ( ! m_Members.GetSize())
		return false;
	// Order cannot declare on Order.
	if ( Align_GetType() == STONEALIGN_ORDER &&
		pEnemyStone->Align_GetType() == STONEALIGN_ORDER )
	{
		return( false );
	}
	return true;	// it is possible.
}

void CItemStone::War_AnnounceWar( const CItemStone* pEnemyStone, bool fWeDeclare, bool fWar )
{
	// Announce we are at war or peace.

	ASSERT(pEnemyStone);

	bool fAtWar = War_IsAtWarWith(pEnemyStone);

	TCHAR szTemp[ CSTRING_MAX_LEN ];
	int len = sprintf( szTemp, (fWar) ? "%s %s declared war on %s." : "%s %s requested peace with %s.",
		(fWeDeclare) ? "You" : pEnemyStone->GetName(),
		(fWeDeclare) ? "have" : "has",
		(fWeDeclare) ? pEnemyStone->GetName() : "You" );

	if ( fAtWar )
	{
		sprintf( szTemp+len, " War is ON!" );
	}
	else if ( fWar )
	{
		sprintf( szTemp+len, " War is NOT yet on." );
	}
	else
	{
		sprintf( szTemp+len, " War is OFF." );
	}

	for ( int i=0; i<m_Members.GetSize(); i++ )
	{
		CCharPtr pChar = g_World.CharFind(m_Members.GetChar(i));
		if ( pChar == NULL )
			continue;
		if ( ! pChar->IsClient())
			continue;
		pChar->WriteString( szTemp );
	}
}

bool CItemStone::War_IsAtWarWith( const CItemStone* pEnemyStone ) const
{
	// Boths guild shave declared war on each other.

	if ( pEnemyStone == NULL )
		return( false );

	// Just based on align type.
	if ( IsType(IT_STONE_GUILD) &&
		Align_GetType() != STONEALIGN_STANDARD &&
		pEnemyStone->Align_GetType() != STONEALIGN_STANDARD )
	{
		if ( Align_GetType() == STONEALIGN_CHAOS &&
			pEnemyStone->Align_GetType() == STONEALIGN_CHAOS )
		{
			return( true );
		}
		// Chaos guilds are all at war with each other ?
		return( Align_GetType() != pEnemyStone->Align_GetType());
	}

	// we have declared or they declared.
	return( m_AtWar.IsObjIn( pEnemyStone ));
}

bool CItemStone::War_Undeclare( CItemStone* pStone )
{
	// Declare war.
	// RETURN:
	//  true = change,

	if (pStone==NULL)
		return false;

	bool fIsAtWar = m_AtWar.IsObjIn(this);
	if ( pStone->m_AtWar.IsObjIn(this) || pStone->m_WeDeclared.IsObjIn(this))
	{
		// we were actually at war.
		m_AtWar.DetachObj(pStone);
		m_WeDeclared.DetachObj(pStone);
		m_TheyDeclared.AttachObj(pStone);

		pStone->m_AtWar.DetachObj(this);
		pStone->m_WeDeclared.AttachObj(this);
		pStone->m_TheyDeclared.DetachObj(this);
		return fIsAtWar;
	}

	// We were not at war.
	m_AtWar.DetachObj(pStone);
	m_WeDeclared.DetachObj(pStone);
	m_TheyDeclared.DetachObj(pStone);

	pStone->m_TheyDeclared.DetachObj(this);
	return !fIsAtWar;
}

bool CItemStone::War_Declare( CItemStone* pStone )
{
	// I am declaring war.
	// RETURN:
	//  true = changed war state.

	if (pStone==NULL)
		return false;

	bool fIsAtWar = m_AtWar.IsObjIn(this);
	if ( pStone->m_AtWar.IsObjIn(this) || pStone->m_WeDeclared.IsObjIn(this))
	{
		// the other guy thinks we are already at war.
		// or The other guy has declared war on us already.
		// War is on!

		m_AtWar.AttachObj(pStone);
		m_WeDeclared.DetachObj(pStone);
		m_TheyDeclared.DetachObj(pStone);

		pStone->m_AtWar.AttachObj(this);
		pStone->m_WeDeclared.DetachObj(this);
		pStone->m_TheyDeclared.DetachObj(this);
		return !fIsAtWar;
	}

	// War is off
	// The other guy has not declared war on us anyhow.

	m_AtWar.DetachObj(pStone);
	m_WeDeclared.AttachObj(pStone);
	m_TheyDeclared.DetachObj(pStone);

	pStone->m_TheyDeclared.AttachObj(this);
	return fIsAtWar;
}

#if 0

bool CItemStone::War_WeDeclareWar(CItemStone* pEnemyStone)
{
	if ( ! War_CanWarWith( pEnemyStone ))
		return false;

	// See if they've already declared war on us
	CItemMemoryPtr pMember = FindMember(pEnemyStone);
	if ( pMember )
	{
		if ( pMember->GetWeDeclared())
			return true;
	}
	else // They haven't, make a record of this
	{
		pMember = new CItemMemory( this, pEnemyStone->GetUID(), STONESTATUS_ENEMY );
	}
	pMember->SetWeDeclared(true);

	// Now inform the other stone
	// See if they have already declared war on us
	CItemMemoryPtr pEnemyMember = pEnemyStone->FindMember(this);
	if (!pEnemyMember) // Not yet it seems
	{
		pEnemyMember = new CItemMemory( pEnemyStone, GetUID(), STONESTATUS_ENEMY );
	}
	else
	{
		DEBUG_CHECK( pEnemyMember->GetWeDeclared());
	}
	pEnemyMember->SetTheyDeclared(true);

	// announce to both sides.
	AnnounceWar( pEnemyStone, true, true );
	pEnemyStone->AnnounceWar( this, false, true );
	return( true );
}

void CItemStone::War_TheyDeclarePeace( CItemStone* pEnemyStone, bool fForcePeace )
{
	// Now inform the other stone
	// Make sure we declared war on them
	CItemMemoryPtr pEnemyMember = FindMember(pEnemyStone);
	if ( ! pEnemyMember )
		return;

	bool fPrevTheyDeclared = pEnemyMember->GetTheyDeclared();

	if (!pEnemyMember->GetWeDeclared() || fForcePeace) // If we're not at war with them, delete this record
		pEnemyMember->DeleteThis();
	else
		pEnemyMember->SetTheyDeclared(false);

	if ( ! fPrevTheyDeclared )
		return;

	// announce to both sides.
	pEnemyStone->AnnounceWar( this, true, false );
	AnnounceWar( pEnemyStone, false, false );
}

void CItemStone::War_WeDeclarePeace( CSphereUID uid, bool fForcePeace )
{
	CItemPtr pEnemyStone2 = g_World.ItemFind(uid);
	CItemStonePtr pEnemyStone = REF_CAST(CItemStone,pEnemyStone2);
	if (!pEnemyStone)
		return;

	CItemMemoryPtr pMember = m_WeDeclare.FindMember(pEnemyStone);
	if ( ! pMember ) // No such member
		return;

	// Set my flags on the subject.
	if (!pMember->GetTheyDeclared() || fForcePeace) // If they're not at war with us, delete this record
		pMember->DeleteThis();
	else
		pMember->SetWeDeclared(false);

	pEnemyStone->War_TheyDeclarePeace( this, fForcePeace );
}

#endif

//**********************************************************************************************
// Validation

bool CItemStone::ValidateMemberArray( CUIDRefArray& Array )
{
	bool fChanges = false;
	for ( int i=0; i<Array.GetSize(); i++ )
	{
		CCharPtr pChar = g_World.CharFind( Array.GetAt(i));
		if ( pChar == NULL || pChar->m_pPlayer == NULL )
		{
			DEBUG_WARN(( "Stone UID=0%x has mislinked char uid=0%x" LOG_CR, GetUID(), Array.GetAt(i)));
			Array.RemoveAt(i);
			i--;
			fChanges = true;
		}
	}
	return fChanges;
}

int CItemStone::FixWeirdness()
{
	// Check all my members. Make sure all wars are reciprocated and members are flaged.

	int iResultCode = CItem::FixWeirdness();
	if ( iResultCode )
	{
		return( iResultCode );
	}

	if ( GetAmount() == 0 )	// being deleted or its recursive.
		return 0;

	bool fChanges = false;
	SetAmount(0);	// turn off validation for now. we don't want to delete other members.

	// Make sure all my members have memories of this.
	int iQty = m_Members.GetSize();
	for ( int i=0; i<m_Members.GetSize(); i++ )
	{
		Member_FindIndex(i);	// this will add memories and clean up as needed.
		if ( iQty != m_Members.GetSize())
		{
			fChanges = true;
			i--;
			iQty--;
		}
	}

	// Make sure all the candidates exist.
	fChanges |= ValidateMemberArray(m_Candidates);
	// Make sure all the bans exist.
	fChanges |= ValidateMemberArray(m_Bans);

	// Validate all the warring guilds.
	// Make sure actual wars are mutual.

	iQty = m_WeDeclared.GetSize();
	for ( int i=0; i<iQty; i++ )
	{
		CItemStonePtr pStone = REF_CAST(CItemStone,g_World.ItemFind(m_WeDeclared.GetAt(i)));
		if ( pStone == NULL )
		{
			m_WeDeclared.RemoveAt(i);
			fChanges = true;
			i--;
			iQty--;
		}
		else 
		{
			War_Declare( pStone );
			if ( iQty != m_WeDeclared.GetSize())
			{
				fChanges = true;
				i--;
				iQty--;
			}
		}
	}

	iQty = m_AtWar.GetSize();
	for ( int i=0; i<iQty; i++ )
	{
		CItemStonePtr pStone = REF_CAST(CItemStone,g_World.ItemFind(m_AtWar.GetAt(i)));
		if ( pStone == NULL )
		{
			m_AtWar.RemoveAt(i);
			fChanges = true;
			iQty--;
			i--;
		}
		else 
		{
			War_Declare( pStone );
			if ( iQty != m_AtWar.GetSize())
			{
				fChanges = true;
				iQty--;
				i--;
			}
		}
	}

	for ( int i=0; i<m_TheyDeclared.GetSize(); i++ )
	{
		CItemStonePtr pStone = REF_CAST(CItemStone,g_World.ItemFind(m_TheyDeclared.GetAt(i)));
		if ( pStone == NULL ||
			! pStone->m_WeDeclared.IsObjIn(this))
		{
			m_TheyDeclared.RemoveAt(i);
			i--;
		}
	}

	SetAmount(1);	// turn off validation for now. we don't want to delete other members.
	if ( fChanges )
	{
		Member_ElectMaster();	// May have changed the vote count.
	}
	return( 0 );
}

//*****************************************************************************************************
// Scripting

void CItemStone::s_Serialize( CGFile& a )
{
	// Read and write binary.
	CItem::s_Serialize(a);

}

void CItemStone::s_WriteProps( CScript& s )
{
	CItem::s_WriteProps( s );
	s.WriteKeyInt( "ALIGN", Align_GetType());
	if ( ! m_sAbbrev.IsEmpty())
	{
		s.WriteKey( "ABBREV", m_sAbbrev );
	}
	if ( m_iDailyDues )
	{
		s.WriteKeyInt( "DAILYDUES", m_iDailyDues );
	}
	for ( int i = 0; i<COUNTOF(m_sCharter); i++ )
	{
		if ( ! m_sCharter[i].IsEmpty())
		{
			CGString sKey;
			sKey.Format("CHARTER.%i", i);
			s.WriteKey( sKey, m_sCharter[i] );
		}
	}

	if ( ! m_sWebPageURL.IsEmpty())
	{
		s.WriteKey( "WEBPAGE", GetWebPageURL());
	}

	m_AtWar.s_WriteObjs(s,"AtWar");					// Guilds we are actually at war with. (mutual)
	m_WeDeclared.s_WriteObjs(s,"WeDeclared");		// Guilds we declared war on

	m_Members.s_WriteObjs(s,"Members");				// Members/Resigns from this guild 
	m_Candidates.s_WriteObjs(s,"Candidates");		// Candidates from this guild 
	m_Bans.s_WriteObjs(s,"Bans");					// Bans from this guild 
}

void CItemStone::AddMember( CSphereUID uid, STONESTATUS_TYPE iStatusLevel, CGString sTitle, CSphereUID uidLoyalTo, bool fArg1, bool fArg2 )
{
	// Support this old crap.
	// Link to the memory in the CChar (if exists yet)
	// fArg1 = Paperdoll stone abbreviation (also if they declared war)
	// fArg2 = If we declared war

	if ( ! uid.IsItem())
	{
		// Member will be added via memories.
		return;
	}
	// War on another guild.
	if ( fArg2 && fArg2 )
	{
		// At War
		m_AtWar.AttachUID(uid);
	}
	else if ( fArg2 )
	{
		// We declared.
		m_WeDeclared.AttachUID(uid);
	}
}

HRESULT CItemStone::s_PropSet( LPCTSTR pszKey, CGVariant& vVal ) // Load an item Script
{
	s_FixExtendedProp( pszKey, "Charter", vVal );

	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CItem::s_PropSet(pszKey, vVal));
	}

	switch (iProp)
	{
	case P_Abbrev:
		m_sAbbrev = vVal.GetStr();
		break;
	case P_Align:
	case P_AlignName:
	case P_AlignType:
		Align_SetType( (STONEALIGN_TYPE) vVal.GetInt());
		break;
	case P_DailyDues:
		m_iDailyDues = vVal;
		break;
	case P_Member:
		{	// old style.
			int iArgQty = vVal.MakeArraySize();
			if ( iArgQty < 2 )
				return HRES_BAD_ARG_QTY;
			AddMember( vVal.GetArrayInt(0),					// Member's UID
				(STONESTATUS_TYPE) vVal.GetArrayInt(2),	// Members priv level (use as a type)
				vVal.GetArrayPSTR(1),						// Title
				vVal.GetArrayInt(3),				// Member is loyal to this
				vVal.GetArrayInt(4),			// Paperdoll stone abbreviation (also if they declared war)
				vVal.GetArrayInt(5)			// If we declared war
				);
		}
		break;
	case P_WebPage:
		m_sWebPageURL = vVal.GetStr();
		break;

	case P_Charter:
		{
			if ( vVal.MakeArraySize() < 2 )
				return(HRES_BAD_ARG_QTY);
			int iLine = vVal.GetArrayInt(0);
			if ( iLine >= COUNTOF(m_sCharter))
				return( HRES_INVALID_INDEX );
			m_sCharter[iLine] = vVal.GetArrayStr(1);
		}
		break;

	case P_AtWar:				// Guilds we are actually at war with. (mutual)
		m_AtWar.AttachUID( vVal.GetUID());
		break;
	case P_WeDeclared:			// Guilds we declared war on
		m_WeDeclared.AttachUID( vVal.GetUID());
		break;
	case P_Members:				// Members/Resigns from this guild 
		m_Members.AttachUID( vVal.GetUID());
		break;
	case P_Candidates:			// Candidates from this guild 
		m_Candidates.AttachUID( vVal.GetUID());
		break;
	case P_Bans:				// Bans from this guild 
		m_Bans.AttachUID( vVal.GetUID());
		break;

	case P_SrcAbbrevStatus:
	case P_SrcLoyalTo:
		return HRES_WRITE_FAULT;

	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}

HRESULT CItemStone::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CItem::s_PropGet( pszKey, vValRet, pSrc ));
	}

	switch ( iProp )
	{
	case P_Charter:
		return HRES_INVALID_FUNCTION;
	case P_Abbrev: 
		vValRet = m_sAbbrev;
		break;
	case P_Align:
		vValRet.SetInt( Align_GetType());
		break;
	case P_AlignType:
	case P_AlignName:
		vValRet = Align_GetName();
		break;

	case P_DailyDues:
		vValRet.SetInt( m_iDailyDues );
		break;
	case P_WebPage:
		vValRet = GetWebPageURL();
		break;

	case P_Master:
		vValRet.SetRef( Member_GetMaster());
		break;

	case P_MasterName:
		{
			CCharPtr pMaster = Member_GetMaster();
			vValRet = (pMaster) ? pMaster->GetName() : "vote pending";
		}
		break;

	case P_MasterGenderTitle:
		{
			CCharPtr pMaster = Member_GetMaster();
			if ( pMaster == NULL )
				vValRet = ""; // If no master (vote pending)
			else if ( pMaster->Char_GetDef()->IsFemale())
				vValRet = "Mistress";
			else
				vValRet = "Master";
		}
		break;

	case P_MasterTitle:
		{
			CItemMemoryPtr pMember = Member_GetMasterMemory();
			vValRet = (pMember) ? pMember->Guild_GetTitle() : "";
		}
		break;

	case P_SrcAbbrevStatus:
		{
			CCharPtr pCharSrc = GET_ATTACHED_CCHAR(pSrc);
			if ( pCharSrc == NULL )
				return HRES_PRIVILEGE_NOT_HELD;
			CItemMemoryPtr pMember = pCharSrc->Memory_FindObj(this);
			if ( pMember == NULL )
			{
				vValRet = "nonmember";
				break;
			}
			vValRet = (pMember->Guild_IsAbbrevOn())? "On" : "Off";
		}
		break;

	case P_SrcLoyalTo:
		{
			CCharPtr pCharSrc = GET_ATTACHED_CCHAR(pSrc);
			if ( pCharSrc == NULL )
				return HRES_PRIVILEGE_NOT_HELD;

			CItemMemoryPtr pMember = pCharSrc->Memory_FindObj(this);
			if ( pMember == NULL )
			{
				vValRet = "nonmember";
				break;
			}
			CCharPtr pLoyalTo = g_World.CharFind( pMember->Guild_GetLoyalTo());
			if (pLoyalTo == NULL || pLoyalTo == pCharSrc )
				vValRet = "yourself";
			else
				vValRet = pLoyalTo->GetName();
		}
		break;

	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}

HRESULT CItemStone::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	// NOTE:: ONLY CALL this from CChar::s_Method !!!
	// Little to no security checking here !!!

	ASSERT(pSrc);
	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp < 0 )
	{
		return( CItem::s_Method( pszKey, vArgs, vValRet, pSrc ));
	}

	switch ( iProp )
	{
	case M_AtWar:
		return m_AtWar.s_MethodObjs( vArgs, vValRet, pSrc );
	case M_Bans:		
		return m_Bans.s_MethodObjs( vArgs, vValRet, pSrc );
	case M_Candidates:	
		return m_Candidates.s_MethodObjs( vArgs, vValRet, pSrc );
	case M_Members:		
		return m_Members.s_MethodObjs( vArgs, vValRet, pSrc );
	case M_TheyDeclared:
		return m_TheyDeclared.s_MethodObjs( vArgs, vValRet, pSrc );
	case M_WeDeclared:
		return m_WeDeclared.s_MethodObjs( vArgs, vValRet, pSrc );
	case M_ElectMaster:
		Member_ElectMaster();
		return 0;
	case M_Charter:	// Get/Set charter lines.
		{
			int iQty = vArgs.MakeArraySize();
			if ( iQty <= 1 )
				return(HRES_BAD_ARG_QTY);
			int iLine = vArgs.GetArrayInt(0);
			if ( iLine < 0 || iLine >= COUNTOF(m_sCharter))
				return( HRES_INVALID_INDEX );
			if ( iQty == 1 )	// Read
			{
				vValRet = m_sCharter[iLine];
			}
			else // Write
			{
				m_sCharter[iLine] = vArgs.GetArrayStr(1);
			}
		}
		return 0;
	}

	CCharPtr pCharSrc = GET_ATTACHED_CCHAR(pSrc);
	if ( pCharSrc == NULL || ! pCharSrc->IsClient())
	{
		return( HRES_INVALID_HANDLE );
	}

	CClientPtr pClient = pCharSrc->GetClient();
	ASSERT(pClient);

	// We may or may not be a member.
	CItemMemoryPtr pMember = pCharSrc->Memory_FindObj(this);

	switch ( iProp )
	{
	case M_JoinAsMember:
		Member_AddRecruit( pClient->GetChar(), true );
		break;
	case M_ApplyToJoin:
		Member_AddRecruit( pClient->GetChar(), false );
		break;

		// GUI stuff

#if 0
	case M_AcceptCandidate:
		addStoneDialog(pClient,STONEDISP_ACCEPTCANDIDATE);
		break;
	case M_ChangeAlign:
		if ( !vArgs.IsEmpty())
		{
			Align_SetType( (STONEALIGN_TYPE) vArgs.GetInt());
			CGString sMsg;
			sMsg.Format( "%s is now a %s %s", (LPCTSTR) GetName(), (LPCTSTR) Align_GetName(), (LPCTSTR) GetTypeName());
			Speak( sMsg );
		}
		else
		{
			pClient->Menu_Setup( g_Cfg.ResourceGetIDType( RES_Menu, _TEXT("MENU_GUILD_ALIGN")), this );
		}
		break;
	case M_DeclareFealty:
		addStoneDialog(pClient,STONEDISP_FEALTY);
		break;
	case M_DeclarePeace:
		addStoneDialog(pClient,STONEDISP_DECLAREPEACE);
		break;
	case M_DeclareWar:
		addStoneDialog(pClient,STONEDISP_DECLAREWAR);
		break;
	case M_DismissMember:
		addStoneDialog(pClient,STONEDISP_DISMISSMEMBER);
		break;
	case M_GrantTitle:
		addStoneDialog(pClient,STONEDISP_GRANTTITLE);
		break;
	case M_MasterMenu:
		SetupMenu( pClient, true );
		break;
	case M_Recruit:
		if ( Member_IsMember(pCharSrc))
			pClient->addTarget( CLIMODE_TARG_STONE_RECRUIT, "Who do you want to recruit into the guild?" );
		else
			Speak("Only members can recruit.");
		break;
	case M_RefuseCandidate:
		addStoneDialog(pClient,STONEDISP_REFUSECANDIDATE);
		break;
	case M_Resign:
		if ( pMember == NULL )
			break;
		pMember->DeleteThis();
		break;
	case M_ReturnMainMenu:
		SetupMenu( pClient );
		break;
	case M_SetAbbreviation:
		pClient->addPromptConsole( CLIMODE_PROMPT_STONE_SET_ABBREV, "What shall the abbreviation be?" );
		break;
	case M_SetCharter:
		addStoneDialog(pClient,STONEDISP_SETCHARTER);
		break;
	case M_SetGMTitle:
		pClient->addPromptConsole( CLIMODE_PROMPT_STONE_SET_TITLE, "What shall thy title be?" );
		break;
	case M_SetName:
		{
			CGString sMsg;
			sMsg.Format( "What would you like to rename the %s to?", (LPCTSTR) GetTypeName());
			pClient->addPromptConsole( CLIMODE_PROMPT_STONE_NAME, sMsg );
		}
		break;
		// case M_TELEPORT:
		//	break;
	case M_ToggleAbbreviation:
		if ( pMember == NULL )
			break;
		pMember->Guild_SetAbbrev( ! pMember->Guild_IsAbbrevOn());
		SetupMenu( pClient );
		break;
	case M_ViewCandidates:
		addStoneDialog(pClient,STONEDISP_CANDIDATES);
		break;
	case M_ViewCharter:
		addStoneDialog(pClient,STONEDISP_VIEWCHARTER);
		break;
	case M_ViewEnemys:
		addStoneDialog(pClient,STONEDISP_VIEWENEMYS);
		break;
	case M_ViewRoster:
		addStoneDialog(pClient,STONEDISP_ROSTER);
		break;
	case M_ViewThreats:
		addStoneDialog(pClient,STONEDISP_VIEWTHREATS);
		break;
#endif

	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

//****************************************************************************************
// GUI

void CItemStone::addStoneSetViewCharter( CClient* pClient, STONEDISP_TYPE iStoneMenu )
{
	static LPCTSTR const sm_szDefControls[] =
	{
		"page 0",							// Default page
		"resizepic 0 0 2520 350 400",	// Background pic
		"tilepic 30 50 %d",			// Picture of a stone
		"tilepic 275 50 %d",			// Picture of a stone
		"gumppic 76 126 95",				// Decorative separator
		"gumppic 85 135 96",				// Decorative separator
		"gumppic 264 126 97",			// Decorative separator
		"text 65 35 2301 0",				// Stone name at top
		"text 110 70 0 1",				// Page description
		"text 120 85 0 2",				// Page description
		"text 140 115 0 3",				// Section description
		"text 125 290 0 10",				// Section description
		"gumppic 76 301 95",				// Decorative separator
		"gumppic 85 310 96",				// Decorative separator
		"gumppic 264 301 97",			// Decorative separator
		"text 40 370 0 12",				// Directions
		"button 195 370 2130 2129 1 0 %i",	// Save button
		"button 255 370 2121 2120 1 0 0"		// Cancel button
	};

	CGStringArray asControls;
	while ( asControls.GetSize() < COUNTOF(sm_szDefControls))
	{
		// Fix the button ID so we can trap it later
		asControls.AddFormat( sm_szDefControls[asControls.GetSize()], 
			( asControls.GetSize() > 4 ) ? (int) iStoneMenu : (int) GetDispID());
	}

	bool fView = (iStoneMenu == STONEDISP_VIEWCHARTER);

	CGStringArray asText;
	asText.Add( GetName());
	asText.AddFormat( "%s %s Charter", (fView) ? "View": "Set", (LPCTSTR) GetTypeName());
	asText.Add( "and Web Link" );
	asText.Add( "Charter" );

	for ( int i=0; i< COUNTOF(m_sCharter); i++)
	{
		if ( fView )
		{
			asControls.AddFormat( "text 50 %i 0 %i", 155 + (i*22), i + 4);
		}
		else
		{
			asControls.AddFormat( "gumppic 40 %i 1143", 152 + (i*22));
			asControls.AddFormat( "textentry 50 %i 250 32 0 %i %i", 155 + (i*22), i + CSCRIPT_ARGCHK_VAL, i + 4 );
		}
		if ( fView && i == 0 && m_sCharter[0].IsEmpty())
		{
			asText.Add( "No charter has been specified." );
		}
		else
		{
			asText.Add( Charter_Get(i));
		}
	}

	if ( fView )
	{
		asControls.Add( "text 50 331 0 11" );
	}
	else
	{
		asControls.Add( "gumppic 40 328 1143" );
		asControls.Add( "textentry 50 331 250 32 0 1006 11" ); // CSCRIPT_ARGCHK_VAL
	}

	asText.Add( "Web Link" );
	asText.Add( GetWebPageURL());
	asText.Add( (fView) ? "Go to the web page": "Save this information" );

	pClient->addGumpDialog( CLIMODE_DIALOG_GUILD, asControls, asText, 0x6e, 0x46, NULL );
}

void CItemStone::SetupMenu( CClient* pClient, bool fMasterFunc )
{
#if 0
	if ( pClient == NULL )
		return;
	CCharPtr pCharClient = pClient->GetChar();
	if ( pCharClient == NULL )
		return;

	CItemMemoryPtr pMember = pCharClient->Memory_FindObj(this);
	bool fMaster = Member_IsMaster(pCharClient);

	if ( pMember && pMember->Guild_GetStatus() == STONESTATUS_ACCEPTED )
	{
		// Am i an STONESTATUS_ACCEPTED member ? make me a full member.
		// Ask me if i want to be a member
		Member_AddRecruit( pClient->GetChar(), true );
	}

	LPCTSTR pszResourceName;
	if ( IsType(IT_STONE_TOWN))
	{
		if ( fMaster )
		{
			// Non GM's shouldn't get here, but in case they do (exploit), give them the non GM menu
			if ( fMasterFunc )
			{
				pClient->Dialog_Setup( CLIMODE_DIALOG,
					g_Cfg.ResourceGetIDType( RES_Dialog, "DIALOG_TOWN_MAYORFUNC"), this );
			}
			else if( pClient->m_iClientResourceLevel == 3 || pClient->m_iClientResourceLevel == 4 )
			{
				pClient->Dialog_Setup( CLIMODE_DIALOG,
					g_Cfg.ResourceGetIDType( RES_Dialog, "DIALOG_TOWN_MAYOR"), this );
			}
			else
			{
				pszResourceName = _TEXT("MENU_TOWN_MAYOR");
			}
		}
		else if ( ! pMember )		// non-member view.
		{
			pClient->Dialog_Setup( CLIMODE_DIALOG,
				g_Cfg.ResourceGetIDType( RES_Dialog, "DIALOG_TOWN_NON"), this );
		}
		else
		{
			pClient->Dialog_Setup( CLIMODE_DIALOG,
				g_Cfg.ResourceGetIDType( RES_Dialog, "DIALOG_TOWN_MEMBER"), this );
		}
	}
	else
	{
		if ( fMaster )
		{
			if ( fMasterFunc )
				pszResourceName = _TEXT("MENU_GUILD_MASTERFUNC");
			else
				pszResourceName = _TEXT("MENU_GUILD_MASTER");
		}
		else if ( ! pMember )		// non-member view.
		{
			pszResourceName = _TEXT("MENU_GUILD_NON");
		}
		else
		{
			pszResourceName = _TEXT("MENU_GUILD_MEMBER");
		}
	}
	if( pClient->m_iClientResourceLevel != 3 && pClient->m_iClientResourceLevel != 4 )
	{
		pClient->Menu_Setup( g_Cfg.ResourceGetIDType( RES_Menu, pszResourceName ), this );
	}

#endif

}

#if 0

bool CItemStone::IsInMenu( STONEDISP_TYPE iStoneMenu, const CItemMemory* pMember ) const
{
	ASSERT( pMember );
	switch ( iStoneMenu )
	{
	case STONEDISP_ROSTER:
	case STONEDISP_FEALTY:
	case STONEDISP_GRANTTITLE:
		if ( ! pMember->Guild_IsMember())
			return( false );
		break;
	case STONEDISP_DISMISSMEMBER:
		if ( ! pMember->Guild_IsMember() && pMember->Guild_GetStatus() != STONESTATUS_ACCEPTED )
			return( false );
		break;
	case STONEDISP_ACCEPTCANDIDATE:
	case STONEDISP_REFUSECANDIDATE:
	case STONEDISP_CANDIDATES:
		if ( pMember->Guild_GetStatus() != STONESTATUS_CANDIDATE )
			return( false );
		break;
	case STONEDISP_DECLAREPEACE:
	case STONEDISP_VIEWENEMYS:
		if ( pMember->Guild_GetStatus() != STONESTATUS_ENEMY)
			return( false );
		if ( !pMember->GetWeDeclared())
			return( false );
		break;
	case STONEDISP_VIEWTHREATS:
		if ( pMember->Guild_GetStatus() != STONESTATUS_ENEMY)
			return( false );
		if ( !pMember->GetTheyDeclared())
			return( false );
		break;
	default:
		return( false );
	}
	return( true );
}

bool CItemStone::IsInMenu( STONEDISP_TYPE iStoneMenu, const CItemStone* pOtherStone ) const
{
	if ( iStoneMenu != STONEDISP_DECLAREWAR )
		return( false );

	if ( pOtherStone == this )
		return( false );

	CItemMemoryPtr pMember = FindMember( pOtherStone );
	if (pMember)
	{
		if ( pMember->GetWeDeclared())	// already declared.
			return( false );
	}
	else
	{
		if ( pOtherStone->GetCount() <= 0 )	// Only stones with members can have war declared against them
			return( false );
	}

	return( true );
}

int CItemStone::addStoneListSetup( STONEDISP_TYPE iStoneMenu, CGStringArray* pasText )
{
	// ARGS: pasText = NULL if i just want to count.

	int iTexts = 0;
	if ( iStoneMenu == STONEDISP_DECLAREWAR )
	{
		// This list is special.
		CResNameSortArray<CItemStone>* pArray = GetStoneArray();
		if ( pArray == NULL )
			return 0;

		int iSize = pArray->GetSize();
		for ( int i=0; i < iSize; i++ )
		{
			CItemStonePtr pOtherStone = pArray->GetAt(i);
			if ( GetType() != pOtherStone->GetType())
				continue;
			if ( ! IsInMenu( STONEDISP_DECLAREWAR, pOtherStone ))
				continue;
			if ( pasText )
			{
				pasText->Add( pOtherStone->GetName());
			}
			iTexts++;
		}
		return( iTexts );
	}

	CItemMemoryPtr pMember = GetHead();
	for ( ; pMember != NULL; pMember = pMember->GetNext())
	{
		if ( ! IsInMenu( iStoneMenu, pMember ))
			continue;

		if ( pasText )
		{
			CCharPtr pChar = g_World.CharFind(pMember->GetLinkUID());
			if ( pChar )
			{
				TCHAR szTmp[256];
				strcpy( szTmp, pChar->GetName());
				if ( strlen( pMember->Guild_GetTitle()))
				{
					strcat( szTmp, ", ");
					strcat( szTmp, pMember->Guild_GetTitle());
				}
				pasText->Add( szTmp );
			}
			else
			{
				CItemPtr pItem = g_World.ItemFind( pMember->GetLinkUID());
				if (pItem)
				{
					CItemStonePtr pStoneItem = REF_CAST(CItemStone,pItem);
					if (pStoneItem)
						pasText->Add( pStoneItem->GetName());
					else
						pasText->Add( "Bad stone" );
				}
				else
				{
					pasText->Add( "Bad member" );
				}
			}
		}
		iTexts++;
	}
	return( iTexts );
}

void CItemStone::addStoneList( CClient* pClient, STONEDISP_TYPE iStoneMenu )
{
	// Add a list of members of a type.
	// Estimate the size first.
	static LPCTSTR const sm_szDefControls[] =
	{
		// "nomove",
		"page 0",
		"resizepic 0 0 5100 400 350",
		"text 15 10 0 0",
		"button 13 290 5050 5051 1 0 %i",
		"text 45 292 0 3",
		"button 307 290 5200 5201 1 0 0",
	};

	int iControlLimit = COUNTOF(sm_szDefControls);
	switch ( iStoneMenu )
	{
	case STONEDISP_ROSTER:
	case STONEDISP_CANDIDATES:
	case STONEDISP_VIEWENEMYS:
	case STONEDISP_VIEWTHREATS:
		iControlLimit --;
		break;
	}

	CGStringArray asControls;
	int i;
	for ( i=0; i<iControlLimit; i++ )
	{
		// Fix the button's number so we know what screen this is later
		asControls.AddFormat( sm_szDefControls[i], iStoneMenu );
	}

	CGStringArray asText;

	const char* pszTitle;
	const char* pszArg = "";
	switch ( iStoneMenu )
	{
	case STONEDISP_ROSTER:
		pszTitle = GetName(); 
		pszArg = " Roster";
		break;
	case STONEDISP_CANDIDATES:
		pszTitle = GetName(); 
		pszArg = " Candidates";
		break;
	case STONEDISP_FEALTY:
		pszTitle = "Declare your fealty";
		break;
	case STONEDISP_ACCEPTCANDIDATE:
		pszTitle = "Accept candidate for ";
		pszArg = GetName();
		break;
	case STONEDISP_REFUSECANDIDATE:
		pszTitle = "Refuse candidate for ";
		pszArg = GetName();
		break;
	case STONEDISP_DISMISSMEMBER:
		pszTitle = "Dismiss member from ";
		pszArg = GetName();
		break;
	case STONEDISP_DECLAREWAR:
		pszTitle = "Declaration of war by ";
		pszArg = GetName();
		break;
	case STONEDISP_DECLAREPEACE:
		pszTitle = "Declaration of peace by ";
		pszArg = GetName();
		break;
	case STONEDISP_GRANTTITLE:
		pszTitle = "To whom do you wish to grant a title?";
		break;
	case STONEDISP_VIEWENEMYS:
		pszTitle = GetTypeName();
		pszArg = "s we have declared war on";
		break;
	case STONEDISP_VIEWTHREATS:
		pszTitle = GetTypeName();
		pszArg = "s which have declared war on us";
		break;
	}

	asText.AddFormat( "%s%s", pszTitle, pszArg );

	static LPCTSTR const sm_szDefText[] =
	{
		"Previous page",
		"Next page",
	};
	for ( i=0; i<COUNTOF(sm_szDefText); i++ )
	{
		asText.Add( sm_szDefText[i] );
	}

	switch ( iStoneMenu )
	{
	case STONEDISP_FEALTY:
		asText.Add("I have selected my new lord");
		break;
	case STONEDISP_ACCEPTCANDIDATE:
		asText.Add("Accept this candidate for membership");
		break;
	case STONEDISP_REFUSECANDIDATE:
		asText.Add("Refuse this candidate membership");
		break;
	case STONEDISP_DISMISSMEMBER:
		asText.Add("Dismiss this member");
		break;
	case STONEDISP_DECLAREWAR:
		asText.AddFormat( "Declare war on this %s", (LPCTSTR) GetTypeName());
		break;
	case STONEDISP_DECLAREPEACE:
		asText.AddFormat( "Declare peace with this %s", (LPCTSTR) GetTypeName());
		break;
	case STONEDISP_GRANTTITLE:
		asText.Add( "Grant this member a title");
		break;
	case STONEDISP_ROSTER:
	case STONEDISP_CANDIDATES:
	case STONEDISP_VIEWTHREATS:
	case STONEDISP_VIEWENEMYS:
		asText.Add("Done");
		break;
	}

	// First count the appropriate members
	int iMemberCount = addStoneListSetup( iStoneMenu, NULL );
	int iPages = 0;
	for ( i=0; i<iMemberCount; i++)
	{
		if (i % 10 == 0)
		{
			iPages += 1;
			asControls.AddFormat("page %i", iPages);
			if (iPages > 1)
			{
				asControls.AddFormat("button 15 320 5223 5223 0 %i", iPages - 1);
				asControls.Add( "text 40 317 0 1" );
			}
			if ( iMemberCount > iPages* 10)
			{
				asControls.AddFormat("button 366 320 5224 5224 0 %i", iPages + 1);
				asControls.Add( "text 288 317 0 2" );
			}
		}
		switch ( iStoneMenu )
		{
		case STONEDISP_FEALTY:
		case STONEDISP_DISMISSMEMBER:
		case STONEDISP_ACCEPTCANDIDATE:
		case STONEDISP_REFUSECANDIDATE:
		case STONEDISP_DECLAREWAR:
		case STONEDISP_DECLAREPEACE:
		case STONEDISP_GRANTTITLE:
			asControls.AddFormat("radio 20 %i 5002 5003 0 %i", ((i % 10)* 25) + 35, i + CSCRIPT_ARGCHK_VAL );

		case STONEDISP_ROSTER:
		case STONEDISP_CANDIDATES:
		case STONEDISP_VIEWENEMYS:
		case STONEDISP_VIEWTHREATS:
			asControls.AddFormat("text 55 %i 0 %i", ((i % 10)* 25) + 32, i + 4);
			break;
		}
	}

	addStoneListSetup( iStoneMenu, &asText );

	pClient->addGumpDialog( CLIMODE_DIALOG_GUILD, asControls, asText, 0x6e, 0x46, NULL );
}

void CItemStone::addStoneDialog( CClient* pClient, STONEDISP_TYPE menuid )
{
	ASSERT( pClient );

	// Use this for a stone dispatch routine....
	switch (menuid)
	{
	case STONEDISP_ROSTER:
	case STONEDISP_CANDIDATES:
	case STONEDISP_FEALTY:
	case STONEDISP_ACCEPTCANDIDATE:
	case STONEDISP_REFUSECANDIDATE:
	case STONEDISP_DISMISSMEMBER:
	case STONEDISP_DECLAREWAR:
	case STONEDISP_DECLAREPEACE:
	case STONEDISP_VIEWENEMYS:
	case STONEDISP_VIEWTHREATS:
	case STONEDISP_GRANTTITLE:
		addStoneList(pClient,menuid);
		break;
	case STONEDISP_VIEWCHARTER:
	case STONEDISP_SETCHARTER:
		addStoneSetViewCharter(pClient,menuid);
		break;
	}
}

#endif

bool CItemStone::OnDialogButton( CClient* pClient, STONEDISP_TYPE type, CSphereExpArgs& exec )
{
	// Button presses come here
#if 0

	ASSERT( pClient );
	switch ( type )
	{
	case STONEDISP_NONE: // They right clicked out, or hit the cancel button, no more stone functions
		return true;

	case STONEDISP_VIEWCHARTER:
		// The only button is the web button, so just go there
		pClient->addWebLaunch( GetWebPageURL());
		return true;

	case STONEDISP_SETCHARTER:
		{
			for (int i = 0; i < exec.m_ArgArray.GetSize(); i++)
			{
				CVarDefPtr pVar = exec.m_ArgArray.GetAt(i);
				ASSERT(pVar);
				if ( _strnicmp( pVar->GetKey(), "ARGTXT_", 7 ))
					continue;
				int id = atoi(pVar->GetKey()+7);
				switch ( id )
				{
				case CSCRIPT_ARGCHK_VAL+0:	// Charter[0]
				case CSCRIPT_ARGCHK_VAL+1:	// Charter[1]
				case CSCRIPT_ARGCHK_VAL+2:	// Charter[2]
				case CSCRIPT_ARGCHK_VAL+3:	// Charter[3]
				case CSCRIPT_ARGCHK_VAL+4:	// Charter[4]
				case CSCRIPT_ARGCHK_VAL+5:	// Charter[5]
					Charter_Set(id-CSCRIPT_ARGCHK_VAL, pVar->GetStr());
					break;
				case CSCRIPT_ARGCHK_VAL+6:	// Weblink
					SetWebPage( pVar->GetStr());
					break;
				}
			}
		}
		return true;

	case STONEDISP_DISMISSMEMBER:
	case STONEDISP_ACCEPTCANDIDATE:
	case STONEDISP_REFUSECANDIDATE:
	case STONEDISP_FEALTY:
	case STONEDISP_DECLAREWAR:
	case STONEDISP_DECLAREPEACE:
	case STONEDISP_GRANTTITLE:
		break;

	case STONEDISP_ROSTER:
	case STONEDISP_VIEWTHREATS:
	case STONEDISP_VIEWENEMYS:
	case STONEDISP_CANDIDATES:
		SetupMenu( pClient );
		return( true );

	default:
		return( false );
	}

	CVarDefPtr pVar = exec.m_ArgArray.FindKeyPtr( "ARGCHK_0" );
	if ( pVar == NULL )	 // If they hit ok but didn't pick one, treat it like a cancel
		return true;

	int iMember = pVar->GetInt() - CSCRIPT_ARGCHK_VAL;

	CItemMemoryPtr pMember = NULL;
	bool fFound = false;
	int i = 0;
	int iStoneIndex = 0;

	if ( type == STONEDISP_DECLAREWAR )
	{
		CResNameSortArray<CItemStone>* pArray = GetStoneArray();
		if ( pArray == NULL )
			return 0;
		int iSize = pArray->GetSize();
		for ( ; iStoneIndex < iSize; iStoneIndex ++ )
		{
			CItemStonePtr pOtherStone = pArray->GetAt(iStoneIndex);
			if ( ! IsInMenu( STONEDISP_DECLAREWAR, pOtherStone ))
				continue;
			if (i == iMember)
			{
				fFound = true;
				break;
			}
			i ++;
		}
	}
	else
	{
		pMember = GetHead();
		for (; pMember != NULL; pMember = pMember->GetNext())
		{
			if ( ! IsInMenu( type, pMember ))
				continue;
			if (i == iMember)
			{
				fFound = true;
				break;
			}
			i ++;
		}
	}

	if (fFound)
	{
		switch ( type ) // Button presses come here
		{
		case STONEDISP_DECLAREWAR:
			CResNameSortArray<CItemStone>* pArray = GetStoneArray();
			if ( pArray == NULL )
				return 0;
			if ( ! War_WeDeclareWar( pArray->GetAt(iStoneIndex)))
			{
				pClient->WriteString( "Cannot declare war" );
			}
			break;
		case STONEDISP_ACCEPTCANDIDATE:
			ASSERT( pMember );
			Member_AddRecruit( g_World.CharFind(pMember->GetLinkUID()), true );
			break;
		case STONEDISP_REFUSECANDIDATE:
			ASSERT( pMember );
			pMember->DeleteThis();
			break;
		case STONEDISP_DISMISSMEMBER:
			ASSERT( pMember );
			pMember->DeleteThis();
			break;
		case STONEDISP_FEALTY:
			ASSERT( pMember );
			{
				CItemMemoryPtr pMe = FindMember(pClient->GetChar());
				if ( pMe == NULL ) 
					return( false );
				pMe->SetLoyalTo( g_World.CharFind(pMember->GetLinkUID()));
			}
			break;
		case STONEDISP_DECLAREPEACE:
			ASSERT( pMember );
			War_WeDeclarePeace(pMember->GetLinkUID());
			break;
		case STONEDISP_GRANTTITLE:
			ASSERT( pMember );
			pClient->m_Targ.m_PrvUID = pMember->GetLinkUID();
			pClient->addPromptConsole( CLIMODE_PROMPT_STONE_GRANT_TITLE, "What title dost thou grant?" );
			return( true );
		}
	}
	else
	{
		pClient->WriteString("Who is that?");
	}

	// Now send them back to the screen they came from

	switch ( type )
	{
	case STONEDISP_ACCEPTCANDIDATE:
	case STONEDISP_REFUSECANDIDATE:
	case STONEDISP_DISMISSMEMBER:
	case STONEDISP_DECLAREPEACE:
	case STONEDISP_DECLAREWAR:
		SetupMenu( pClient, true );
		break;
	default:
		SetupMenu( pClient, false );
		break;
	}
#endif

	return true;
}

bool CItemStone::OnPromptResp( CClient* pClient, CLIMODE_TYPE TargMode, LPCTSTR pszText, CGString& sMsg )
{
	ASSERT( pClient );

#if 0
	switch ( TargMode )
	{
	case CLIMODE_PROMPT_STONE_NAME:
		// Set the stone or town name !
		if ( ! CItemStone::IsUniqueName( pszText ))
		{
			if (!_stricmp( pszText, GetName()))
			{
				pClient->WriteString( "Name is unchanged." );
				return false;
			}
			pClient->WriteString( "That name is already taken." );
			CGString sMsg;
			sMsg.Format( "What would you like to rename the %s to?", (LPCTSTR) GetTypeName());
			pClient->addPromptConsole( CLIMODE_PROMPT_STONE_NAME, sMsg );
			return false;
		}

		SetName( pszText );
		if ( ! m_Members.GetSize()) // No members? It must be a brand new stone then, fix it up
		{
			Member_AddRecruit( pClient->GetChar(), true );
		}
		sMsg.Format( "%s renamed: %s", (LPCTSTR) GetTypeName(), (LPCTSTR) pszText );
		break;

	case CLIMODE_PROMPT_STONE_SET_ABBREV:
		Abbrev_Set(pszText);
		sMsg.Format( "Abbreviation set: %s", pszText );
		break;

	case CLIMODE_PROMPT_STONE_GRANT_TITLE:
		{
			CItemMemoryPtr pMember = FindMember( g_World.CharFind(pClient->m_Targ.m_PrvUID));
			if (pMember)
			{
				pMember->Guild_SetTitle(pszText);
				sMsg.Format( "Title set: %s", pszText);
			}
		}
		break;
	case CLIMODE_PROMPT_STONE_SET_TITLE:
		{
			CItemMemoryPtr pMaster = Member_GetMasterMemory();
			pMaster->Guild_SetTitle(pszText);
			sMsg.Format( "Title set: %s", pszText);
		}
		break;
	}

#endif

	return( true );
}

void CItemStone::Use_Item( CClient* pClient )
{

#if 0
	if ( !m_Members.GetSize() && IsType(IT_STONE_GUILD)) // Everyone resigned...new master
	{
		Member_AddRecruit( pClient->GetChar(), true );
	}
	SetupMenu( pClient );
#endif

}


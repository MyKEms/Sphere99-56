//
// CCharNoto.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
// Memory/Criminal actions/Noto.

#include "stdafx.h"	// predef header.

CItemStonePtr CChar::Guild_Find( MEMORY_TYPE MemType ) const
{
	// Get my guild stone for my guild. even if i'm just a STONESTATUS_CANDIDATE ?
	// ARGS:
	//  MemType == MEMORY_GUILD or MEMORY_TOWN

	CItemMemoryPtr pMyGMem = Memory_FindTypes(MemType);
	if ( pMyGMem == NULL )
		return NULL;

	CItemStonePtr pMyStone = pMyGMem->Guild_GetLink();
	if ( pMyStone == NULL )
	{
		// Some sort of mislink ! fix it.
		pMyGMem->DeleteThis();
		return( NULL );
	}
	return( pMyStone );
}

void CChar::Guild_Resign( MEMORY_TYPE MemType )
{
	// response to "I resign from my guild" or "town"
	// Are we in an active battle ?

	if ( IsStatFlag( STATF_DEAD ))
		return;

	CItemMemoryPtr pMyGMem = Memory_FindTypes(MemType);
	if ( pMyGMem == NULL )
		return ;

	CItemMemoryPtr pMemFight = Memory_FindTypes( MEMORY_FIGHT );
	if ( pMemFight )
	{
		CItemStonePtr pMyStone = pMyGMem->Guild_GetLink();
		ASSERT(pMyStone);
		Printf( "You cannot quit your %s while in a fight", (LPCTSTR) pMyStone->GetTypeName());
		return;
	}

	pMyGMem->DeleteThis();
}

LPCTSTR CChar::Guild_Abbrev( MEMORY_TYPE MemType ) const
{
	// Get my guild abbrev if i have chosen to turn it on.
	CItemMemoryPtr pMyGMem = Memory_FindTypes(MemType);
	if ( pMyGMem == NULL )
		return( NULL );
	if ( ! pMyGMem->Guild_IsAbbrevOn())
		return( NULL );
	CItemStonePtr pMyStone = pMyGMem->Guild_GetLink();
	if ( pMyStone == NULL ||
		! pMyStone->Abbrev_Get()[0] )
		return( NULL );
	return( pMyStone->Abbrev_Get());
}

LPCTSTR CChar::Guild_AbbrevAndTitle( MEMORY_TYPE MemType ) const
{
	// Get my [guild abbrev] if i have chosen to turn it on.
	CItemMemoryPtr pMyGMem = Memory_FindTypes(MemType);
	if ( pMyGMem == NULL )
		return( NULL );
	if ( ! pMyGMem->Guild_IsAbbrevOn())
		return( NULL );

	CItemStonePtr pMyStone = pMyGMem->Guild_GetLink();
	if ( pMyStone == NULL ||
		! pMyStone->Abbrev_Get()[0] )
		return( NULL );

	TCHAR* pszTemp = Str_GetTemp();
	int len = sprintf( pszTemp, "[%s", pMyStone->Abbrev_Get() );

	// Get Abbrev,Title
	CGString sTitle = pMyGMem->m_TagDefs.FindKeyStr("TITLE");
	if ( ! sTitle.IsEmpty())
	{
		len += sprintf( pszTemp+len, ", %s", (LPCTSTR) sTitle );
	}

	strcpy( pszTemp+len, "]" );
	return( pszTemp );
}

//*****************************************************************

bool CChar::Noto_IsMurderer() const
{
	// Criminal Murderer ?
	return( m_pPlayer && m_pPlayer->m_wMurders > g_Cfg.m_iMurderMinCount );
}

bool CChar::Noto_IsEvil() const
{
	// animals and humans given more leeway.
	if ( Noto_IsMurderer())
		return( true );
	int iKarma = Stat_Get(STAT_Karma);
	switch ( GetCreatureType())
	{
	case NPCBRAIN_UNDEAD:
	case NPCBRAIN_MONSTER:
		return( iKarma< 0 );	// I am evil ?
	case NPCBRAIN_BESERK:
		return( true );
	case NPCBRAIN_ANIMAL:
		return( iKarma<= -800 );
	}
	if ( m_pPlayer )
	{
		return( iKarma<g_Cfg.m_iPlayerKarmaEvil );
	}
	return( iKarma <= -3000 );
}

bool CChar::Noto_IsNeutral() const
{
	// Should neutrality change in guarded areas ?
	int iKarma = Stat_Get(STAT_Karma);
	switch ( GetCreatureType())
	{
	case NPCBRAIN_MONSTER:
	case NPCBRAIN_BESERK:
		return( iKarma<= 0 );
	case NPCBRAIN_ANIMAL:
		return( iKarma<= 100 );
	}
	if ( m_pPlayer )
	{
		return( iKarma<g_Cfg.m_iPlayerKarmaNeutral );
	}
	return( iKarma<0 );
}

NOTO_TYPE CChar::Noto_GetFlag( const CChar* pCharViewer, bool fAllowIncog ) const
{
	// What is this char to the viewer ?
	// This allows the noto attack check in the client.
	// NOTO_GOOD = it is criminal to attack me.

	if ( fAllowIncog && IsStatFlag( STATF_Incognito ))
	{
		return NOTO_NEUTRAL;
	}

	// Are we in the same party ?
	if ( pCharViewer != this &&
		m_pParty &&
		m_pParty == pCharViewer->m_pParty )
	{
		if ( m_pParty->GetLootFlag(this))
		{
			return(NOTO_GUILD_SAME);
		}
	}

	if ( Noto_IsEvil())
	{
		return( NOTO_EVIL );
	}

	if ( this != pCharViewer ) // Am I checking myself?
	{
		// Check the guild stuff
		CItemStonePtr pMyTown = Guild_Find(MEMORY_TOWN);
		CItemStonePtr pMyGuild = Guild_Find(MEMORY_GUILD);
		if ( pMyGuild || pMyTown )
		{
			CItemStonePtr pViewerGuild = pCharViewer->Guild_Find(MEMORY_GUILD);
			CItemStonePtr pViewerTown = pCharViewer->Guild_Find(MEMORY_TOWN);
			// Are we both in a guild?
			if ( pViewerGuild || pViewerTown )
			{
				if ( pMyGuild )
				{
					if ( pViewerGuild )
					{
						if ( pViewerGuild == pMyGuild ) // Same guild?
							return NOTO_GUILD_SAME; // return green
						if ( pMyGuild->Align_IsSameType( pViewerGuild ))
							return NOTO_GUILD_SAME;
						// Are we in different guilds but at war? (not actually a crime right?)
						if ( pMyGuild->War_IsAtWarWith(pViewerGuild))
							return NOTO_GUILD_WAR; // return orange
					}
					if ( pMyGuild->War_IsAtWarWith(pViewerTown))
						return NOTO_GUILD_WAR; // return orange
				}
				if ( pMyTown )
				{
					if ( pViewerGuild )
					{
						if ( pMyTown->War_IsAtWarWith(pViewerGuild))
							return NOTO_GUILD_WAR; // return orange
					}
					if ( pMyTown->War_IsAtWarWith(pViewerTown))
						return NOTO_GUILD_WAR; // return orange
				}
			}
		}
	}

	if ( IsStatFlag( STATF_Criminal ))	// criminal to everyone.
		return( NOTO_CRIMINAL );

	if ( this != pCharViewer ) // Am I checking myself?
	{
		if ( NPC_IsOwnedBy( pCharViewer, false ))	// All pets are neutral to their owners.
			return( NOTO_NEUTRAL );

		// If they saw me commit a crime or I am their aggressor then
		// criminal to just them.
		CItemMemoryPtr pMemory = pCharViewer->Memory_FindObjTypes( this, MEMORY_SAWCRIME | MEMORY_AGGREIVED );
		if ( pMemory != NULL )
		{
			return( NOTO_CRIMINAL );
		}
	}

	if ( m_pArea.IsValidRefObj() && m_pArea->IsFlag(REGION_FLAG_ARENA))
	{
		// everyone is neutral here.
		return( NOTO_NEUTRAL );
	}
	if ( Noto_IsNeutral())
	{
		return( NOTO_NEUTRAL );
	}
	return( NOTO_GOOD );
}

HUE_TYPE CChar::Noto_GetHue( const CChar* pCharViewer, bool fIncog ) const
{
	// What is this char to the viewer ?
	// Represent as a text Hue.

	switch ( Noto_GetFlag( pCharViewer, fIncog ))
	{
	case NOTO_GOOD:			return HUE_BLUE_LIGHT;	// Blue
	case NOTO_GUILD_SAME:	return 0x0044;	// Green (same guild)
	case NOTO_NEUTRAL:		return 0x03b2;	// Grey 1 (someone that can be attacked)
	case NOTO_CRIMINAL:		return 0x03b2;	// Grey 2 (criminal)
	case NOTO_GUILD_WAR:	return HUE_ORANGE;	// Orange (enemy guild)
	case NOTO_EVIL:			return 0x0026;	// Red
	}
	return HUE_TEXT_DEF;	// ?Grey
}

LPCTSTR CChar::Noto_GetFameTitle() const
{
	if ( IsStatFlag( STATF_Incognito ))
		return( "" );
	if ( ! IsPrivFlag( PRIV_PRIV_HIDE ))
	{
		if ( IsGM())
			return( "GM " );
		switch ( GetPrivLevel())
		{
		case PLEVEL_Seer: return( "Seer " );
		case PLEVEL_Counsel: return( "Counselor " );
		}
	}
	// ??? Mayor ?
	if ( Stat_Get(STAT_Fame) <= 9900 )
		return( "" );
	CCharDefPtr pCharDef = Char_GetDef();
	if ( pCharDef->IsFemale())
		return( "Lady " );
	return( "Lord " );
}

int CChar::Noto_GetLevel() const
{
	// Paperdoll title for character
	// This is so we can inform user of change in title !

	static const int sm_KarmaLevel[] =
	{ 9900, 5000, 1000, 500, 100, -100, -500, -1000, -5000, -9900 };

	int i=0;
	int iKarma = Stat_Get(STAT_Karma);
	for ( ; i<COUNTOF( sm_KarmaLevel ) && iKarma < sm_KarmaLevel[i]; i++ )
		;

	static const WORD sm_FameLevel[] =
	{ 500, 1000, 5000, 9900 };

	int j =0;
	int iFame = Stat_Get(STAT_Fame);
	for ( ; j<COUNTOF( sm_FameLevel ) && iFame > sm_FameLevel[j]; j++ )
		;

	return( ( i* 5 ) + j );
}

LPCTSTR CChar::Noto_GetTitle() const
{
	// Paperdoll title for character

#if 0
	// Murderer titles.
	30 Destroyer of Life
	50 Impaler
	90 Slaughterer of Innocents
	125+ was Evil Lord, Name, enemy of all living

#endif

	// Paperdoll title for character

	LPCTSTR pTitle;

	// Murderer ?
	if ( Noto_IsMurderer())
	{
		pTitle = "Murderer";
	}
	else if ( IsStatFlag( STATF_Criminal ))
	{
		pTitle = "Criminal";
	}
	else
	{
		pTitle = g_Cfg.GetNotoTitle( Noto_GetLevel());
	}

	TCHAR* pTemp = Str_GetTemp();
	sprintf( pTemp, "%s%s%s%s%s",
		(pTitle[0]) ? "The " : "",
		pTitle,
		(pTitle[0]) ? " " : "",
		Noto_GetFameTitle(),
		(LPCTSTR) GetName());

	return( pTemp );
}

void CChar::Noto_Murder()
{
	// I am a murderer (it seems) (update my murder decay item)
	if ( Noto_IsMurderer())
	{
		WriteString( "Murderer!" );
	}
	if ( m_pPlayer && m_pPlayer->m_wMurders )
	{
		// SPELL_Criminal IT_EQ_MURDER_COUNT
		CItemPtr pFlag = Spell_Equip_Create( SPELL_NONE, LAYER_FLAG_Murders, 0, g_Cfg.m_iMurderDecayTime, NULL, false );
		if (pFlag)
		{
			pFlag->SetType(IT_EQ_MURDER_COUNT);
			pFlag->m_itEqMurderCount.m_Decay_Balance = g_Cfg.m_iMurderDecayTime;
		}
	}
}

void CChar::Noto_Criminal()
{
	// I am a criminal and the guards will be on my ass.
	if ( IsGM())
		return;
	WriteString( "Criminal!" );
	// SPELL_Criminal
	CItemPtr pFlag = Spell_Equip_Create( SPELL_NONE, LAYER_FLAG_Criminal, 0, g_Cfg.m_iCriminalTimer, NULL, false );
}

void CChar::Noto_ChangeDeltaMsg( int iDelta, LPCTSTR pszType )
{
	if ( iDelta == 0 )
		return;

#define NOTO_FACTOR 300

	static LPCTSTR const sm_DegreeTable[] =
	{
		"a bit of",
		"a small amount of",
		"a little",
		"some",
		"a moderate amount of",
		"alot of",
		"large amounts of",
		"huge amounts of",		// 300 = huge
	};

	int iDegree = ABS(iDelta) / ( NOTO_FACTOR / ( COUNTOF(sm_DegreeTable) - 1));
	if (iDegree > COUNTOF(sm_DegreeTable) - 1)
		iDegree = COUNTOF(sm_DegreeTable) - 1;

	Printf( "You have %s %s %s.",
		( iDelta < 0 ) ? "lost" : "gained",
		sm_DegreeTable[iDegree], pszType );
}

void CChar::Noto_ChangeNewMsg( int iPrvLevel )
{
	if ( iPrvLevel != Noto_GetLevel())
	{
		// reached a new title level ?
		Printf( "You are now %s", (LPCTSTR) Noto_GetTitle());
	}
}

void CChar::Noto_Fame( int iFameChange )
{
	// Fame should only go down on death, time or cowardice ?

	if ( ! iFameChange )
		return;

	int iFame = Stat_Get(STAT_Fame);
	ASSERT( iFame >= 0 );

	iFameChange = g_Cfg.Calc_FameScale( iFame, iFameChange );
	if ( ! iFameChange )
		return;

	iFame += iFameChange;
	if ( iFame < 0 )
		iFame = 0;
	if ( iFame > 10000)
		iFame = 10000; // Maximum reached

	Noto_ChangeDeltaMsg( iFame - Stat_Get(STAT_Fame), "fame" );
	Stat_Set(STAT_Fame,iFame);
}

void CChar::Noto_Karma( int iKarmaChange, int iBottom )
{
	// iBottom is a variable where you control at what point
	// the loss for this action stop (as in stealing shouldnt
	// take you to dread ). iBottom def. to -10000 if you leave
	// it out.

	if ( ! iKarmaChange )
		return;

	int	iKarma = Stat_Get(STAT_Karma);

	iKarmaChange = g_Cfg.Calc_KarmaScale( iKarma, iKarmaChange );
	if ( ! iKarmaChange )
		return;

	// If we are going to loose karma and are already below bottom
	// then return.
	if (( iKarma <= iBottom ) && ( iKarmaChange < 0 ))
		return;

	iKarma += iKarmaChange;
	if ( iKarmaChange < 0 )
	{
		if ( iKarma < iBottom )
			iKarma = iBottom;
	}
	else
	{
		if ( iKarma > 10000 )
			iKarma = 10000;
	}

	Noto_ChangeDeltaMsg( iKarma - Stat_Get(STAT_Karma), "karma" );
	Stat_Set(STAT_Karma,iKarma);
}

void CChar::Noto_KarmaChangeMessage( int iKarmaChange, int iLimit )
{
	// Change your title ?
	int iPrvLevel = Noto_GetLevel();
	Noto_Karma( iKarmaChange, iLimit );
	Noto_ChangeNewMsg( iPrvLevel );
}

void CChar::Noto_Kill( CChar* pKill, bool fPetKill )
{
	// I participated in killing pKill CChar. (called from Death())
	// I Get some fame/karma. (maybe)

	ASSERT(pKill);

	// What was there noto to me ?
	NOTO_TYPE NotoThem = pKill->Noto_GetFlag( this, false );

	// Fight is over now that i have won. (if i was fighting at all )
	// ie. Magery cast might not be a "fight"
	Fight_Clear( pKill );
	if ( pKill == this )
	{
		DEBUG_CHECK( pKill != this );
		return;
	}

	if ( ! m_pPlayer.IsValidNewObj())
	{
		ASSERT( m_pNPC.IsValidNewObj());

		// I am a guard ?
		if ( ! pKill->m_pPlayer.IsValidNewObj() &&
			m_pNPC->m_Brain == NPCBRAIN_GUARD )
		{
			// Don't create a corpse or loot if NPC killed by a guard.
			// No corpse and no loot !
			pKill->StatFlag_Set( STATF_Conjured );
			return;
		}

		// Check to see if anything is on the corpse I just killed
		Skill_Start( NPCACT_LOOKING );	// look around for loot.
		// If an NPC kills an NPC then it doesn't count.
		if ( pKill->m_pNPC.IsValidNewObj())
			return;
	}
	else if ( NotoThem < NOTO_NEUTRAL )
	{
		ASSERT( m_pPlayer.IsValidNewObj());
		// I'm a murderer !
		if ( ! IsGM())
		{
			m_pPlayer->m_wMurders++;
			Noto_Criminal();
			Noto_Murder();
		}
	}

	// Store current notolevel before changes, used to check if notlvl is changed.
	int iPrvLevel = Noto_GetLevel();
	int iFameChange = g_Cfg.Calc_FameKill( pKill );
	int iKarmaChange = g_Cfg.Calc_KarmaKill( pKill, NotoThem );

	Noto_Karma( iKarmaChange );
	if ( ! fPetKill )	// no real fame for letting your pets do the work !
	{
		Noto_Fame( iFameChange );
	}

	Noto_ChangeNewMsg( iPrvLevel ); // Inform on any notlvl changes.
}

//***************************************************************
// Memory this char has about something in the world.

bool CChar::Memory_UpdateFlags( CItemMemory* pMemory )
{
	// Reset the check timer based on the type of memory.

	ASSERT(pMemory);
	DEBUG_CHECK( pMemory->IsType(IT_EQ_MEMORY_OBJ));

	WORD wMemTypes = pMemory->GetMemoryTypes();

	if ( ! wMemTypes )	// No memories here anymore so kill it.
	{
		return false;
	}

	int iCheckTime;
	if ( wMemTypes & MEMORY_ISPAWNED )
	{
		StatFlag_Set( STATF_Spawned );
	}
	else if ( wMemTypes & MEMORY_IPET )
	{
		StatFlag_Set( STATF_Pet );
	}

	if ( wMemTypes & MEMORY_FOLLOW )
		iCheckTime = TICKS_PER_SEC;
	else if ( wMemTypes & MEMORY_FIGHT )	// update more often to check for retreat.
		iCheckTime = 30*TICKS_PER_SEC;
	else if ( wMemTypes & ( MEMORY_IPET | MEMORY_GUARD | MEMORY_ISPAWNED | MEMORY_GUILD | MEMORY_TOWN ))
		iCheckTime = -1;	// never go away.
	else if ( m_pNPC )	// MEMORY_SPEAK
		iCheckTime = 5*60*TICKS_PER_SEC;
	else
	{
		DEBUG_CHECK( m_pPlayer.IsValidNewObj());
		iCheckTime = 20*60*TICKS_PER_SEC;
	}
	DEBUG_CHECK(iCheckTime);
	pMemory->SetTimeout( iCheckTime );	// update it's decay time.
	return( true );
}

bool CChar::Memory_UpdateClearTypes( CItemMemory* pMemory, WORD MemTypes )
{
	// Just clear these flags but do not delete the memory.
	// RETURN: true = still useful memory.
	ASSERT(pMemory);

	WORD wPrvMemTypes = pMemory->GetMemoryTypes();
	bool fMore = pMemory->SetMemoryTypes( wPrvMemTypes &~ MemTypes );

	MemTypes &= wPrvMemTypes;	// Which actually got turned off ?

	if ( MemTypes & MEMORY_ISPAWNED )
	{
		StatFlag_Clear( STATF_Spawned );
		// I am a memory link to another object.
		CItemPtr pSpawn = g_World.ItemFind( pMemory->m_uidLink );
		if ( pSpawn != NULL &&
			pSpawn->IsType(IT_SPAWN_CHAR) &&
			pSpawn->m_itSpawnChar.m_current )
		{
			pSpawn->m_itSpawnChar.m_current --;
		}
	}
	if ( MemTypes & MEMORY_IPET )
	{
		// Am i still a pet of some sort ?
		if ( Memory_FindTypes( MEMORY_IPET ) == NULL )
		{
			StatFlag_Clear( STATF_Pet );
		}
	}

#ifdef _DEBUG
	// Must be deleted.
	if ( ! fMore && MemTypes && g_Log.IsLogged( LOGL_TRACE ) && g_Serv.m_iModeCode != SERVMODE_Exiting )
	{
		CObjBasePtr pObj = g_World.ObjFind(pMemory->m_uidLink);
		DEBUG_MSG(( "Memory delete from '%s' for '%s', type 0%x" LOG_CR, (LPCTSTR) GetName(), ( pObj != NULL ) ? pObj->GetName() : "?", MemTypes ));
	}
#endif

	return Memory_UpdateFlags( pMemory );
}

void CChar::Memory_AddTypes( CItemMemory* pMemory, WORD MemTypes )
{
	ASSERT(pMemory);
	DEBUG_CHECK(MemTypes);
	pMemory->SetMemoryTypes( pMemory->GetMemoryTypes() | MemTypes );
	if ( MemTypes & MEMORY_FIGHT )
	{
		pMemory->m_itEqMemory.m_ptStart = GetTopPoint();	// Where did the fight start ?
	}
	pMemory->m_itEqMemory.m_timeStart.InitTimeCurrent();
	Memory_UpdateFlags( pMemory );
}

bool CChar::Memory_ClearTypes( CItemMemory* pMemory, WORD MemTypes )
{
	// Clear this memory object of this type.
	ASSERT(pMemory);
	DEBUG_CHECK(MemTypes);
	if ( Memory_UpdateClearTypes( pMemory, MemTypes ))
		return( true );
	pMemory->DeleteThis();
	return( false );
}

CItemMemoryPtr CChar::Memory_CreateObj( CSphereUID uid, WORD MemTypes )
{
	// Create a memory about this object.
	// NOTE: Does not check if object already has a memory.!!!
	//  Assume it does not !

#ifdef _DEBUG
	DEBUG_CHECK( Memory_FindObj( uid ) == NULL );
#endif

	DEBUG_CHECK( MemTypes );
	if (( MemTypes & MEMORY_IPET ) && uid.IsItem())
	{
		MemTypes = MEMORY_ISPAWNED;
	}

	CItemPtr pItem = CItem::CreateBase( ITEMID_MEMORY );
	CItemMemoryPtr pMemory = REF_CAST(CItemMemory,pItem);
	if ( pMemory == NULL )
	{
		DEBUG_ERR(("ITEMID_MEMORY is not correct IT_EQ_MEMORY_OBJ type!" LOG_CR ));
		return( NULL );
	}

	pMemory->SetType(IT_EQ_MEMORY_OBJ);
	pMemory->SetAttr(ATTR_NEWBIE);
	pMemory->m_uidLink = uid;

	Memory_AddTypes( pMemory, MemTypes );
	LayerAdd( pMemory, LAYER_SPECIAL );
	return( pMemory );
}

void CChar::Memory_ClearTypes( WORD MemTypes )
{
	// Remove all the memories of this type.
	DEBUG_CHECK(MemTypes);
	CItemPtr pItemNext;
	CItemPtr pItem=GetHead();
	for ( ; pItem; pItem=pItemNext)
	{
		pItemNext = pItem->GetNext();
		if ( ! pItem->IsMemoryTypes(MemTypes))
			continue;
		CItemMemoryPtr pMemory = REF_CAST(CItemMemory,pItem);
		if ( pMemory == NULL )
			continue;
		Memory_ClearTypes( pMemory, MemTypes );
	}
}

CItemMemoryPtr CChar::Memory_FindObj( CSphereUID uid ) const
{
	// Do I have a memory / link for this object ?
	CItemPtr pItem=GetHead();
	for ( ; pItem; pItem=pItem->GetNext())
	{
		if ( !pItem->IsType(IT_EQ_MEMORY_OBJ))
			continue;
		if ( pItem->m_uidLink != uid )
			continue;
		return( REF_CAST(CItemMemory,pItem));
	}
	return( NULL );
}

CItemMemoryPtr CChar::Memory_FindTypes( WORD MemTypes ) const
{
	// Do we have a certain type of memory.
	// Just find the first one.
	if ( ! MemTypes )
		return( NULL );
	CItemPtr pItem=GetHead();
	for ( ; pItem!=NULL; pItem=pItem->GetNext())
	{
		if ( ! pItem->IsMemoryTypes(MemTypes))
			continue;
		return( REF_CAST(CItemMemory,pItem ));
	}
	return( NULL );
}

CItemMemoryPtr CChar::Memory_AddObjTypes( CSphereUID uid, WORD MemTypes )
{
	// Create a new memory about the target or add types to existing mem.
	DEBUG_CHECK(MemTypes);
	CItemMemoryPtr pMemory = Memory_FindObj( uid );
	if ( pMemory == NULL )
	{
		return Memory_CreateObj( uid, MemTypes );
	}
	Memory_AddTypes( pMemory, MemTypes );
	return( pMemory );
}

bool CChar::Memory_OnTick( CItemMemory* pMemory )
{
	// NOTE: Do not return true unless u update the timer !
	// RETURN: false = done with this memory.
	ASSERT(pMemory);

	CObjBasePtr pObj = g_World.ObjFind(pMemory->m_uidLink);
	if ( pObj == NULL )
		return( false );

	if ( pMemory->IsMemoryTypes( MEMORY_FOLLOW ))
	{
		// I am following. (GM Feature)
		CCharPtr pChar = g_World.CharFind(pMemory->m_uidLink);
		if ( pChar == NULL )
			return( false );

		if ( IsStatFlag( STATF_War ) || IsDisconnected())
		{
			return Memory_ClearTypes( pMemory, MEMORY_FOLLOW );
		}

		CPointMap ptold = GetTopPoint();
		CPointMap ptnew = pChar->GetTopPoint();
		if ( ptold != ptnew )
		{
			UpdateDir( ptnew );
			MoveToChar( ptnew );
			UpdateMove( ptold, NULL, true );
		}
		return( true );
	}
// WESTY MOD (MULTI CONFIRM)
	// If timer expires, destroy the multi and redeed it
	// return: false = delete the memory completely.
	// true = skip it.
	if ( pMemory->IsMemoryTypes( MEMORY_MULTIPREVIEW ) )
	{
		CItemPtr pItem = g_World.ItemFind( pMemory->m_uidLink );
		if( pItem )
		{
			CItemMultiPtr pItemMulti = REF_CAST( CItemMulti, pItem );
			if( !pItemMulti )
			{
				// Hmph, multi is already gone...
#ifdef _DEBUG
				if ( g_Log.IsLogged( LOGL_TRACE ))
				{
					DEBUG_MSG(( "OnTick '%s' Memory of multi preview, but multi is already gone! '%s'" LOG_CR, (LPCTSTR) GetName() ));
				}
#endif
				return( false );
			}
#ifdef _DEBUG
			if ( g_Log.IsLogged( LOGL_TRACE ))
			{
				DEBUG_MSG(( "OnTick '%s' Memory of multi preview '%s'" LOG_CR, (LPCTSTR) GetName(), (LPCTSTR) pItemMulti->GetName()));
			}
#endif
			bool fSucceed = pItemMulti->Multi_DeedConvert( this, pMemory );

			return( false );
		}
	}
// END WESTY MOD

	if ( pMemory->IsMemoryTypes( MEMORY_FIGHT ))
	{
		// Is the fight still valid ?
		return Memory_Fight_OnTick( pMemory );
	}

	if ( pMemory->IsMemoryTypes( MEMORY_IPET | MEMORY_GUARD | MEMORY_ISPAWNED | MEMORY_GUILD | MEMORY_TOWN ))
		return( true );	// never go away.

	return( false );	// kill it?.
}


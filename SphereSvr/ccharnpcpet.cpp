//
// CCharNPCPet.CPP
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
// Actions specific to an NPC.
//

#include "stdafx.h"	// predef header.

void CChar::NPC_PetResponse( bool fSuccess, const char* pszSpeak, CChar* pMaster )
{
	// i take a command from my master.

	UpdateDir( pMaster );
	if ( NPC_CanSpeak())
	{
		if ( pszSpeak == NULL )
		{
			pszSpeak = fSuccess? "Yes Master" : "Sorry";
		}
		Speak( pszSpeak );
	}
	else
	{
		SoundChar( fSuccess? CRESND_RAND1 : CRESND_RAND2 );
	}
}

bool CChar::NPC_PetDrop( CChar* pMaster )
{
	// Drop just the stuff we are carrying for them.
	CItemContainerPtr pPack = GetPack();
	if ( pPack )
	{
		if ( pPack->GetCount())
		{
			pPack->ContentsDump( GetTopPoint(), ATTR_OWNED );
			NPC_PetResponse( true, NULL, pMaster );
			return true;
		}
	}
	NPC_PetResponse( false, "I'm carrying nothing.", pMaster );
	return( false );
}

bool CChar::NPC_PetSpeakStatus( CChar* pSrc )
{
	if ( ! NPC_CanSpeak())
	{
		// Just good or bad ?
		return true;
	}

	CCharDefPtr pCharDef = Char_GetDef();
	int iWage = pCharDef->m_TagDefs.FindKeyInt( "HIREDAYWAGE" );

	if ( ! iWage && ! pSrc->IsGM())
		return( false );	// not sure why i'm a pet .

	CItemContainerPtr pBank = GetBank();

	CGString sMsg;
	if ( NPC_IsVendor())
	{
		CItemContainerPtr pCont = GetBank(LAYER_VENDOR_STOCK);
		if ( iWage )
		{
			sMsg.Format( "I have %d gold on hand. "
				"for which I will work for %d more days. "
				"I have %d items to sell.",
				pBank->m_itEqBankBox.m_Check_Amount,
				pBank->m_itEqBankBox.m_Check_Amount / iWage,
				pCont->GetCount());
		}
		else
		{
			sMsg.Format( "I have %d gold on hand. "
				"I restock to %d gold in %d minutes or every %d minutes. "
				"I have %d items to sell.",
				pBank->m_itEqBankBox.m_Check_Amount,
				pBank->m_itEqBankBox.m_Check_Restock,
				pBank->GetTimerAdjusted() / 60,
				pBank->GetRestockTimeSeconds() / 60,
				pCont->GetCount());
		}
	}
	else if ( iWage )
	{
		sMsg.Format( "I have been paid to work for %d more days.",
			pBank->m_itEqBankBox.m_Check_Amount / iWage );
	}
	else
	{
		// gm status?
		sMsg = "I'm OK";
	}

	Speak( sMsg );
	return( true );
}

bool CChar::NPC_PetVendorCash( CChar* pSrc )
{
	if ( ! NPC_IsVendor())
		return( false );

	// Give up my cash total.
	CItemContainerPtr pBank = GetBank();
	ASSERT(pBank);

	CCharDefPtr pCharDef = Char_GetDef();
	int iWage = pCharDef->m_TagDefs.FindKeyInt( "HIREDAYWAGE" );

	CGString sMsg;
	if ( pBank->m_itEqBankBox.m_Check_Amount > iWage )
	{
		sMsg.Format( "Here is %d gold. I will keep 1 days wage on hand. To get any items say 'Inventory'",
			pBank->m_itEqBankBox.m_Check_Amount - iWage );
		pSrc->AddGoldToPack( pBank->m_itEqBankBox.m_Check_Amount - iWage );
		pBank->m_itEqBankBox.m_Check_Amount = iWage;
	}
	else
	{
		sMsg.Format( "I only have %d gold. That is less that a days wage. Tell me 'release' if you want me to leave.",
			pBank->m_itEqBankBox.m_Check_Amount );
	}
	Speak( sMsg );
	return( true );
}

enum PC_TYPE
{
	PC_ATTACK,
	PC_BOUGHT,
	PC_CASH,
	PC_COME,
	PC_DISMISS,
	PC_DEFEND_ME,
	PC_DROP,	// "GIVE" ?
	PC_DROP_ALL,
	PC_EQUIP,
	PC_EQUIP_ALL,
	PC_FETCH,
	PC_FOLLOW,
	PC_FOLLOW_ME,
	PC_FREEZE,
	PC_FRIEND,
	PC_GET_DRESSED,
	PC_GO,
	PC_GUARD,
	PC_GUARD_ME,
	PC_INVENTORY,
	PC_KILL,
	PC_PRICE,
	PC_RECIEVED,
	PC_RELEASE,
	PC_SAMPLES,
	PC_SPEAK,
	PC_STATUS,
	PC_STAY,
	PC_STAY_HERE,
	PC_STOCK,
	PC_STOP,
	PC_SUIT_UP,
	PC_TRANSFER,
	PC_UNFREEZE,
	PC_QTY,
};

bool CChar::NPC_OnHearPetCmd( LPCTSTR pszCmd, CChar* pSrc, bool fAllPets )
{
	// This should just be another speech block !!!

	// We own this char (pet or hireling)
	// pObjTarget = the m_ActTarg has been set for them to attack.
	// RETURN:
	//  true = we understand this. tho we may not do what we are told.
	//  false = this is not a command we know. or can perform.
	//  if ( GetTargMode() == CLIMODE_TARG_PET_CMD ) it needs a target.

	static LPCTSTR const sm_Pet_table[] =
	{
		"ATTACK",
		"BOUGHT",
		"CASH",
		"COME",
		"DEFEND ME",
		"DISMISS",
		"DROP",	// "GIVE" ?
		"DROP ALL",
		"EQUIP",
		"EQUIP ALL",
		"FETCH",
		"FOLLOW",
		"FOLLOW ME",
		"FREEZE",
		"FRIEND",
		"GET DRESSED",
		"GO",
		"GUARD",
		"GUARD ME",
		"INVENTORY",
		"KILL",
		"PRICE",	// may have args ?
		"RECIEVED",
		"RELEASE",
		"SAMPLES",
		"SPEAK",
		"STATUS",
		"STAY",
		"STAY HERE",
		"STOCK",
		"STOP",
		"SUIT UP",
		"TRANSFER",
		"UNFREEZE",
		NULL,
	};

	// Kill me?
	// attack me?

	if ( m_pPlayer )
		return( false );

	if ( ! NPC_IsOwnedBy( pSrc, true ))
	{
		return( false );	// take no commands
	}

	// Do we have a script command to follow ?
	if ( ScriptBook_Command( pszCmd, false ) == NO_ERROR )
		return( true );

	PC_TYPE iCmd = (PC_TYPE) FindTableSorted( pszCmd, sm_Pet_table, COUNTOF(sm_Pet_table)-1);
	if ( iCmd < 0 )
	{
		if ( _strnicmp( pszCmd, sm_Pet_table[PC_PRICE], 5 ))
			return( false );
		iCmd = PC_PRICE;
	}

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);

	bool fTargAllowGround = false;
	bool fMayBeCrime = false;
	LPCTSTR pTargPrompt = NULL;

	ASSERT(pSrc);
	if ( ! pSrc->IsClient())
		return( false );
	ASSERT( m_pNPC.IsValidNewObj() );

	bool fSuccess = true;
	switch ( iCmd )
	{
	case PC_ATTACK:
	case PC_KILL:
		pTargPrompt = "Who do you want to attack?";
		fMayBeCrime = true;
		break;
	case PC_CASH:
		// Drop all we have.
		NPC_PetVendorCash(pSrc);
		return true;
	case PC_COME:
	case PC_FOLLOW_ME:
		m_Act.m_pt = pSrc->GetTopPoint();
		fSuccess = CheckMoveWalkToward( m_Act.m_pt, false ) ? true : false;
		if ( ! fSuccess )
			break;
	case PC_GUARD_ME:
	case PC_DEFEND_ME:
		m_Act.m_Targ = pSrc->GetUID();
		Skill_Start( NPCACT_FOLLOW_TARG );
		break;
	case PC_DISMISS:
	case PC_RELEASE:
		Skill_Start( SKILL_NONE );
		NPC_PetClearOwners();
		SoundChar( CRESND_RAND2 );	// No noise
		return( true );
	case PC_DROP:
		// Drop just the stuff we are carrying. (not wearing)
		// "GIVE" ?
		NPC_PetDrop( pSrc );
		return( true );
	case PC_DROP_ALL:
		DropAll( NULL, ATTR_OWNED );
		break;
	case PC_FETCH:
		pTargPrompt = "What should they fetch?";
		break;
	case PC_FOLLOW:
		pTargPrompt = "Who should they follow?";
		break;
	case PC_FRIEND:
		pTargPrompt = "Who is their friend?";
		break;
	case PC_GO:
		pTargPrompt = "Where should they go?";
		fTargAllowGround = true;
		break;
	case PC_GUARD:
		pTargPrompt = "What should they guard?";
		fMayBeCrime = true;
		break;
	case PC_PRICE:
		fSuccess = NPC_IsVendor();
		pTargPrompt = "What item would you like to set the price of?";
		break;
	case PC_SPEAK:
		break;

	case PC_GET_DRESSED:
	case PC_SUIT_UP:
	case PC_EQUIP:
	case PC_EQUIP_ALL:
		ItemEquipWeapon(false);
		ItemEquipArmor(false);
		break;

	case PC_STATUS:
		NPC_PetSpeakStatus(pSrc);
		return true;

	case PC_STAY:
	case PC_STAY_HERE:
	case PC_STOP:
		m_ptHome = GetTopPoint();
		m_pNPC->m_Home_Dist_Wander = SPHEREMAP_VIEW_SIGHT;
		Skill_Start( NPCACT_STAY );
		break;
	case PC_TRANSFER:
		pTargPrompt = "Who do you want to transfer to?";
		break;

	case PC_STOCK:
	case PC_INVENTORY:
		// Magic restocking container.
		fSuccess = NPC_IsVendor();
		if ( fSuccess )
		{
			Speak( "Put items you want me to sell in here." );
			pSrc->GetClient()->addBankOpen( this, LAYER_VENDOR_STOCK );
		}
		break;

	case PC_BOUGHT:
	case PC_RECIEVED:
		fSuccess = NPC_IsVendor();
		if ( fSuccess )
		{
			Speak( "This contains the items I have bought." );
			pSrc->GetClient()->addBankOpen( this, LAYER_VENDOR_EXTRA );
		}
		break;

	case PC_SAMPLES:
		fSuccess = NPC_IsVendor();
		if ( fSuccess )
		{
			Speak( "Put sample items like you want me to purchase in here" );
			pSrc->GetClient()->addBankOpen( this, LAYER_VENDOR_BUYS );
		}
		break;

	case PC_FREEZE:
		fSuccess = NPC_IsVendor();
		if ( fSuccess )
		{
			if( ! IsStatFlag(STATF_Immobile) )
			{
				StatFlag_Set(STATF_Immobile);
				Speak( "I am now frozen master." );
			}
			else
				Speak( "I am already frozen master." );
		}
		return( true );

	case PC_UNFREEZE:
		fSuccess = NPC_IsVendor();
		if ( fSuccess )
		{
			if( IsStatFlag(STATF_Immobile) )
			{
				StatFlag_Clear(STATF_Immobile);
				Speak( "I am now unfrozen master." );
			}
			else
				Speak( "I am already unfrozen master." );
		}
		return( true );

	default:
		return( false );
	}

	if ( fSuccess && pTargPrompt )
	{
		pszCmd += strlen( sm_Pet_table[iCmd] );
		GETNONWHITESPACE( pszCmd );

		// I Need a target arg.

		if ( ! pSrc->IsClient())
			return( false );

		pSrc->m_pClient->m_Targ.m_tmPetCmd.m_iCmd = iCmd;
		pSrc->m_pClient->m_Targ.m_tmPetCmd.m_fAllPets = fAllPets;
		pSrc->m_pClient->m_Targ.m_UID = GetUID();
		pSrc->m_pClient->m_Targ.m_sText = pszCmd;

		pSrc->m_pClient->addTarget( CLIMODE_TARG_PET_CMD, pTargPrompt, fTargAllowGround, fMayBeCrime );
		return( true );
	}

	// make some sound to confirm we heard it.
	// Make the yes/no noise.
	NPC_PetResponse( fSuccess, NULL, pSrc );
	return( fSuccess );
}

bool CChar::NPC_OnHearPetCmdTarg( int iCmd, CChar* pSrc, CObjBase* pObj, const CPointMap& pt, LPCTSTR pszArgs )
{
	// Pet commands that required a target.

	if ( ! NPC_IsOwnedBy( pSrc ))
	{
		return( false );	// take no commands
	}

	bool fSuccess = false;	// No they won't do it.

	// Could be NULL
	CItemPtr pItemTarg = PTR_CAST(CItem,pObj);
	CCharPtr pCharTarg = PTR_CAST(CChar,pObj);

	switch ( iCmd )
	{
	case PC_GO:
		// Go to the location x,y
		m_Act.m_pt = pt;
		fSuccess = CheckMoveWalkToward( m_Act.m_pt, false ) ? true : false;
		if ( ! fSuccess)
			break;
		fSuccess = Skill_Start( NPCACT_GOTO );
		break;

	case PC_FETCH:
		if ( pItemTarg == NULL )
			break;
		if ( ! CanUse(pItemTarg, true ))
			break;
		m_Act.m_Targ = pItemTarg->GetUID();
		fSuccess = Skill_Start( NPCACT_GO_FETCH );
		break;

	case PC_GUARD:
		if ( pItemTarg == NULL )
			break;
		m_Act.m_Targ = pItemTarg->GetUID();
		fSuccess = Skill_Start( NPCACT_GUARD_TARG );
		break;
	case PC_TRANSFER:
		// transfer ownership via the transfer command.
		if ( pCharTarg == NULL )
			break;
		fSuccess = NPC_PetSetOwner( pCharTarg );
		break;

	case PC_KILL:
	case PC_ATTACK:
		// Attack the target.
		// NOTE: Only the owner should be able to command me to do something criminal.
		if ( pCharTarg == NULL )
			break;
		// refuse to attack friends.
		if ( NPC_IsOwnedBy( pCharTarg, true ))
		{
			fSuccess = false;	// take no commands
			break;
		}
		fSuccess = pCharTarg->OnAttackedBy( pSrc, 1, true );	// we know who told them to do this.
		if ( fSuccess )
		{
			fSuccess = Fight_Attack( pCharTarg );
		}
		break;

	case PC_FOLLOW:
		if ( pCharTarg == NULL )
			break;
		m_Act.m_pt = pCharTarg->GetTopPoint();
		fSuccess = CheckMoveWalkToward( m_Act.m_pt, false ) ? true : false;
		if ( ! fSuccess)
			break;
		m_Act.m_Targ = pCharTarg->GetUID();
		fSuccess = Skill_Start( NPCACT_FOLLOW_TARG );
		break;
	case PC_FRIEND:
		// Not the same as owner,
		if ( pCharTarg == NULL )
			break;
		Memory_AddObjTypes( pCharTarg, MEMORY_FRIEND );
		break;

	case PC_PRICE:	// "PRICE" the vendor item.
		if ( pItemTarg == NULL )
			break;
		if ( ! NPC_IsVendor())
			break;

		// did they name a price
		if ( isdigit( pszArgs[0] ))
		{
			return NPC_SetVendorPrice( pItemTarg, atoi(pszArgs));
		}

		// test if it is pricable.
		if ( ! NPC_SetVendorPrice( pItemTarg, -1 ))
		{
			return false;
		}

		// Now set it's price.
		if ( ! pSrc->IsClient())
			break;
		pSrc->m_pClient->m_Targ.m_PrvUID = GetUID();
		pSrc->m_pClient->m_Targ.m_UID = pItemTarg->GetUID();
		pSrc->m_pClient->addPromptConsole( CLIMODE_PROMPT_VENDOR_PRICE, "What do you want the price to be?" );
		return( true );
	}

	// Make the yes/no noise.
	NPC_PetResponse( fSuccess, NULL, pSrc );
	return fSuccess;
}

void CChar::NPC_PetClearOwners()
{
	if ( NPC_IsVendor())
	{
		StatFlag_Clear( STATF_INVUL );

		// Drop all the stuff we are trying to sell !.
		CCharPtr pBoss = NPC_PetGetOwner();
		if ( pBoss )	// Give it all back.
		{
			CItemContainerPtr pBankV = GetBank();
			CItemContainerPtr pBankB = pBoss->GetBank();
			pBoss->AddGoldToPack( pBankV->m_itEqBankBox.m_Check_Amount, pBankB );
			pBankV->m_itEqBankBox.m_Check_Amount = 0;
			NPC_Vendor_Dump( pBankB );
		}
		else
		{
			// Guess i'll just keep it.
		}
	}

	if ( IsStatFlag( STATF_Ridden ))	// boot my rider.
	{
		CCharPtr pCharRider = Horse_GetMountChar();
		if ( pCharRider )
		{
			pCharRider->Horse_UnMount();
		}
	}

	Memory_ClearTypes( MEMORY_IPET|MEMORY_FRIEND );
	DEBUG_CHECK( ! IsStatFlag(STATF_Pet));
}

bool CChar::NPC_PetSetOwner( const CChar* pChar )
{
	// If previous owner was OWNER_SPAWN then remove it from spawn count
	if ( IsStatFlag( STATF_Spawned ))
	{
		Memory_ClearTypes( MEMORY_ISPAWNED );
		DEBUG_CHECK( !IsStatFlag( STATF_Spawned ));
	}

	// m_pNPC may not be set yet if this is a conjured creature.

	if ( m_pPlayer.IsValidNewObj() || pChar == this || pChar == NULL )
	{
		// Clear all owners ?
		NPC_PetClearOwners();
		return false;
	}

	// We get some of the noto of our owner.
	// ??? If I am a pet. I have noto of my master.

	m_ptHome.InitPoint();	// No longer homed.
	Memory_AddObjTypes( pChar, MEMORY_IPET );

	if ( NPC_IsVendor())
	{
		// Clear my cash total.
		CItemContainerPtr pBank = GetBank();
		pBank->m_itEqBankBox.m_Check_Amount = 0;
		StatFlag_Set( STATF_INVUL );
	}
	return( true );
}

bool CChar::NPC_OnTickPetStatus( int nFoodLevel )
{
	// Called at the same time as NPC_OnTickFood
	// Is the pet or hireling happy ?
	// RETURN:
	//  true = happy.

	if ( ! IsStatFlag( STATF_Pet ))
		return( true );
	if ( m_pPlayer.IsValidNewObj() )
		return( true );

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);

	// Am i a hireling ?
	int iWage = pCharDef->m_TagDefs.FindKeyInt( "HIREDAYWAGE" );
	if ( ! iWage )
	{
		// I work for food and happiness.
		// Am i happy at the moment ?
		// If not then free myself.

		CGString sMsg;
		sMsg.Format( "looks %s", (LPCTSTR) Food_GetLevelMessage( true, false ));
		Emote( sMsg, GetClient());
		SoundChar( CRESND_RAND2 );

		if ( nFoodLevel <= 0 )
		{
			// How happy are we with being a pet ?
			NPC_PetDesert();
			return false;
		}

		return( true );
	}

	// Hirelings...
	// How fast does my money get used up ?
	int iFoodConsumeRate = pCharDef->GetRace()->GetRegenRate(STAT_Food);
	if ( ! iFoodConsumeRate )
		return( true );

	// I am hired for money not for food.
	int iPeriodWage = IMULDIV( iWage, iFoodConsumeRate, 24* 60* g_Cfg.m_iGameMinuteLength );
	if ( iPeriodWage <= 0 )
		iPeriodWage = 1;

	CItemContainerPtr pBank = GetBank();
	if ( pBank->m_itEqBankBox.m_Check_Amount > iPeriodWage )
	{
		pBank->m_itEqBankBox.m_Check_Amount -= iPeriodWage;
	}
	else
	{
		Speak( "I will work for %d gold", iWage );

		CCharPtr pOwner = NPC_PetGetOwner();
		if ( pOwner )
		{
			Speak( "I'm sorry but my hire time is up." );

			CItemMemoryPtr pMemory = Memory_AddObjTypes( pOwner, MEMORY_SPEAK );
			ASSERT(pMemory);
			pMemory->m_itEqMemory.m_Arg1 = NPC_MEM_ACT_SPEAK_HIRE;

			NPC_PetDesert();
			return( false );
		}

		// Some sort of strange bug to get here.
		Memory_ClearTypes( MEMORY_IPET );
		StatFlag_Clear( STATF_Pet );
	}

	return( true );
}

void CChar::NPC_OnHirePayMore( CItem* pGold, bool fHire )
{
	// We have been handed money.
	// similar to PC_STATUS

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);
	int iWage = pCharDef->m_TagDefs.FindKeyInt( "HIREDAYWAGE" );
	ASSERT(iWage);

	CItemContainerPtr pBank = GetBank();
	ASSERT(pBank);

	if ( pGold )
	{
		if ( fHire )
		{
			pBank->m_itEqBankBox.m_Check_Amount = 0;	// zero any previous balance.
		}

		pBank->m_itEqBankBox.m_Check_Amount += pGold->GetAmount();
		Sound( pGold->GetDropSound( NULL ));
		pGold->DeleteThis();
	}

	CGString sMsg;
	sMsg.Format( "I will work for you for %d days", pBank->m_itEqBankBox.m_Check_Amount / iWage );
	Speak( sMsg );
}

bool CChar::NPC_OnHirePay( CChar* pCharSrc, CItemMemory* pMemory, CItem* pGold )
{
	ASSERT(pCharSrc);
	ASSERT(pMemory);
	DEBUG_CHECK( pMemory->GetMemoryTypes() & MEMORY_SPEAK );

	if ( ! m_pNPC )
		return( false );

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);

	if ( IsStatFlag( STATF_Pet ))
	{
		if ( ! pMemory->IsMemoryTypes(MEMORY_IPET|MEMORY_FRIEND))
		{
			Speak( "Sorry I am already employed." );
			return false;
		}
	}
	else
	{
		int iWage = pCharDef->m_TagDefs.FindKeyInt( "HIREDAYWAGE" );
		if ( ! iWage )
		{
			Speak( "Sorry I am not available for hire." );
			return false;
		}
		if ( pGold->GetAmount() < iWage )
		{
			Speak( "Sorry thats not enough for a days wage." );
			return false;
		}
		// NOTO_TYPE ?
		if ( pMemory->IsMemoryTypes( MEMORY_FIGHT|MEMORY_HARMEDBY|MEMORY_IRRITATEDBY ))
		{
			Speak( "I will not work for you." );
			return false;
		}

		// Put all my loot cash away.
		ContentConsume( CSphereUID(RES_TypeDef,IT_GOLD), INT_MAX, false, 0 );
		// Mark all my stuff ATTR_OWNED - i won't give it away.
		ContentAttrMod( ATTR_OWNED, true );

		NPC_PetSetOwner( pCharSrc );
	}

	pMemory->m_itEqMemory.m_Arg1 = NPC_MEM_ACT_NONE;
	NPC_OnHirePayMore( pGold, true );
	return true;
}

HRESULT CChar::NPC_OnHireHear( CChar* pCharSrc )
{
	if ( m_pNPC == NULL )
		return( HRES_INVALID_HANDLE );

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);

	int iWage = pCharDef->m_TagDefs.FindKeyInt( "HIREDAYWAGE" );
	if ( ! iWage )
	{
		Speak( "Sorry I am not available for hire." );
		return HRES_INVALID_HANDLE;
	}

	CItemMemoryPtr pMemory = Memory_FindObj( pCharSrc );
	if ( pMemory )
	{
		if ( pMemory->IsMemoryTypes(MEMORY_IPET|MEMORY_FRIEND))
		{
			// Next gold i get goes toward hire.
			Memory_AddTypes( pMemory, MEMORY_SPEAK );
			pMemory->m_itEqMemory.m_Arg1 = NPC_MEM_ACT_SPEAK_HIRE;
			NPC_OnHirePayMore( NULL, false );
			return NO_ERROR;
		}
		if ( pMemory->IsMemoryTypes( MEMORY_FIGHT|MEMORY_HARMEDBY|MEMORY_IRRITATEDBY ))
		{
			Speak( "I will not work for you." );
			return HRES_INVALID_HANDLE;
		}
	}
	if ( IsStatFlag( STATF_Pet ))
	{
		Speak( "Sorry I am already employed." );
		return HRES_INVALID_HANDLE;
	}

	CGString sMsg;
	sMsg.Format( Calc_GetRandVal(2) ?
		"I can be hired for %d gold per day." :
		"I require %d gold per day for hire.", iWage );
	Speak( sMsg );

	pMemory = Memory_AddObjTypes( pCharSrc, MEMORY_SPEAK );
	ASSERT(pMemory);
	pMemory->m_itEqMemory.m_Arg1 = NPC_MEM_ACT_SPEAK_HIRE;
	return NO_ERROR;
}

bool CChar::NPC_SetVendorPrice( CItem* pItem, int iPrice )
{
	// player vendors.
	// CLIMODE_PROMPT_VENDOR_PRICE
	// This does not check who is setting the price if if it is valid for them to do so.

	if ( ! NPC_IsVendor())
		return( false );

	if ( pItem == NULL ||
		pItem->GetTopLevelObj() != this ||
		IsMyChild(pItem))
	{
		Speak( "You can only price things in my inventory." );
		return( false );
	}

	CItemVendablePtr pVendItem = PTR_CAST(CItemVendable,pItem);
	if ( pVendItem == NULL )
	{
		Speak( "I can't sell this" );
		return( false );
	}

	if ( iPrice < 0 )	// just a test.
		return( true );

	CGString sMsg;
	sMsg.Format( "Setting price of %s to %d", (LPCTSTR) pVendItem->GetName(), iPrice );
	Speak( sMsg );

	pVendItem->SetPlayerVendorPrice( iPrice );
	return( true );
}

void CChar::NPC_PetDesert()
{
	// How happy are we with being a pet ?
	CCharPtr pCharOwn = NPC_PetGetOwner();
	if ( pCharOwn && ! pCharOwn->CanSee(this))
	{
		pCharOwn->Printf( "You sense that %s has deserted you.", (LPCTSTR)GetName());
	}

	NPC_PetClearOwners();

	if ( pCharOwn )
	{
		CGString sMsg;
		sMsg.Format( "%s decides they are better off as their own master.", (LPCTSTR)GetName());
		Speak( sMsg );

		// free to do as i wish !
		Skill_Start( SKILL_NONE );
	}
}


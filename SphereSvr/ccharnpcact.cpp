//
// CCharNPCAct.CPP
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
// Actions specific to an NPC.
//

#include "stdafx.h"	// predef header.

#define ARCHERY_DIST_MAX 12

//////////////////////////
// CChar

#ifdef USE_JSCRIPT
#define CCHARNPCMETHOD(a,b,c) JSCRIPT_METHOD_IMP(CCharNPC,a)
#include "ccharnpcmethods.tbl"
#undef CCHARNPCMETHOD
#endif

const CScriptMethod CCharNPC::sm_Methods[CCharNPC::M_QTY+1] =
{
#define CCHARNPCMETHOD(a,b,c) CSCRIPT_METHOD_IMP(a,b,c)
#include "ccharnpcmethods.tbl"
#undef CCHARNPCMETHOD
	NULL,			
};

HRESULT CChar::s_MethodNPC( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	// Stuff that only NPC's do.
	int iProp = s_FindKeyInTable( pszKey, CCharNPC::sm_Methods );
	if ( iProp < 0 )
	{
		// Eat all the CClient::sm_Methods and CCharPlayer::sm_Methods verbs ?
		iProp = s_FindKeyInTable( pszKey, CClient::sm_Methods );
		if ( iProp >= 0 )
			return( NO_ERROR );
		return( HRES_UNKNOWN_PROPERTY );
	}

	ASSERT(m_pNPC);
	ASSERT(pSrc);
	CCharPtr pCharSrc = GET_ATTACHED_CCHAR(pSrc);
	switch (iProp)
	{
	case CCharNPC::M_Buy:
		// Open up the buy dialog.
		if ( pCharSrc == NULL || ! pCharSrc->IsClient())
			return( HRES_INVALID_HANDLE );
		if ( m_pNPC->m_Brain == NPCBRAIN_VENDOR_OFFDUTY )
		{
			Speak( "Sorry, I'm currently off-duty. Come back when my shop is open." );
			break;
		}
		if ( ! pCharSrc->GetClient()->addShopMenuBuy( this ))
		{
			Speak( "Sorry I have no goods to sell" );
		}
		break;
	case CCharNPC::M_Bye:
		Skill_Start( SKILL_NONE );
		m_Act.m_Targ.InitUID();
		break;
	case CCharNPC::M_Flee:
	case CCharNPC::M_Leave:
		// Short amount of fleeing.
		m_Act.m_atFlee.m_iStepsMax = vArgs.GetInt();	// how long should it take to get there.
		if ( ! m_Act.m_atFlee.m_iStepsMax )
			m_Act.m_atFlee.m_iStepsMax = 20;
		m_Act.m_atFlee.m_iStepsCurrent = 0;	// how long has it taken ?
		Skill_Start( NPCACT_FLEE );
		break;
	case CCharNPC::M_Hire:
		return NPC_OnHireHear( pCharSrc);
	case CCharNPC::M_PetRetrieve:
		return( NPC_StablePetRetrieve( pCharSrc ));
	case CCharNPC::M_PetStable:
		return( NPC_StablePetSelect( pCharSrc ));
	case CCharNPC::M_Restock:	// individual restock command.
		return NPC_Vendor_Restock( vArgs.GetInt());
	case CCharNPC::M_Run:
	case CCharNPC::M_R:
		m_Act.m_pt = GetTopPoint();
		m_Act.m_pt.Move( CGRect::GetDirStr( vArgs ));
		NPC_WalkToPoint( true );
		break;
	case CCharNPC::M_Sell:
		// Open up the sell dialog.
		if ( pCharSrc == NULL || ! pCharSrc->IsClient())
			return( HRES_INVALID_HANDLE);
		if ( m_pNPC->m_Brain == NPCBRAIN_VENDOR_OFFDUTY )
		{
			Speak( "Sorry, I'm currently off-duty.  Come back when my shop is open." );
			break;
		}
		if ( ! pCharSrc->GetClient()->addShopMenuSell( this ))
		{
			Speak( "You have nothing I'm interested in" );
		}
		break;
	case CCharNPC::M_Shrink:
		// we must own it.
		if ( ! NPC_IsOwnedBy( pCharSrc ))
			return( HRES_INVALID_HANDLE );
		return( ( NPC_Shrink() == NULL ) ? HRES_INVALID_HANDLE : NO_ERROR );
	case CCharNPC::M_Train:
		return( NPC_OnTrainHear( pCharSrc, vArgs.GetPSTR()));
	case CCharNPC::M_W:
	case CCharNPC::M_Walk:
		m_Act.m_pt = GetTopPoint();
		m_Act.m_pt.Move( CGRect::GetDirStr( vArgs.GetPSTR()));
		NPC_WalkToPoint( false );
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

const LAYER_TYPE CChar::sm_VendorLayers[] = // static
{
	LAYER_VENDOR_STOCK, LAYER_VENDOR_EXTRA, LAYER_VENDOR_BUYS, LAYER_BANKBOX,
};

bool CChar::NPC_Vendor_Dump( CItemContainer* pBank )
{
	// Dump the contents of my vendor boxes into this container.

	if ( ! NPC_IsVendor())
		return false;

	for ( int i=0; i<3; i++ )
	{
		CItemContainerPtr pCont = GetBank( sm_VendorLayers[i] );
		if ( pCont == NULL )
			return( false );
		CItemPtr pItemNext;
		CItemPtr pItem = pCont->GetHead();
		for ( ; pItem != NULL; pItem = pItemNext )
		{
			pItemNext = pItem->GetNext();
			pBank->ContentAdd( pItem );
		}
	}

	return( true );
}

HRESULT CChar::NPC_Vendor_Restock( int iTimeSec )
{
	// Restock this NPC char.
	// Then Set the next restock time for this .

	if ( ! m_pNPC.IsValidNewObj())
		return HRES_INVALID_HANDLE;

	if ( IsStatFlag( STATF_Spawned ))
	{
		// kill all spawned creatures.
		DeleteThis();
		return( NO_ERROR );
	}

	// Not a player vendor.
	if ( ! IsStatFlag( STATF_Pet ))
	{
		// Delete all non-newbie stuff we have first ?
		// ReadScriptTrig( Char_GetDef(), CCharDef::T_NPCRestock );

		if ( NPC_IsVendor())
		{
			for ( int i=0; i<COUNTOF(sm_VendorLayers); i++ )
			{
				CItemContainerPtr pCont = GetBank( sm_VendorLayers[i] );
				if ( pCont == NULL )
					return( false );
				if ( iTimeSec )
				{
					pCont->SetRestockTimeSeconds( iTimeSec );
				}
				pCont->Restock();
			}
		}

		if ( m_ptHome.IsValidPoint() && ! IsStatFlag( STATF_Freeze | STATF_Stone | STATF_Immobile ))
		{
			// send them back to their "home"
			MoveNear( m_ptHome, 5 );
		}
	}

	return( NO_ERROR );
}

HRESULT CChar::NPC_StablePetSelect( CChar* pCharPlayer )
{
	// I am a stable master.
	// I will stable a pet for the player.

	if ( pCharPlayer == NULL )
		return( HRES_INVALID_HANDLE );
	if ( ! pCharPlayer->IsClient())
		return( HRES_INVALID_HANDLE );

	// Might have too many pets already ?

	int iCount = 0;
	CItemContainerPtr pBank = GetBank();
	if ( pBank->GetCount() >= UO_MAX_ITEMS_CONT )
	{
		Speak( "I'm sorry the stables are full" );
		return( HRES_INVALID_HANDLE );
	}

	CItemPtr pItem = pBank->GetHead();
	for ( ; pItem!=NULL ; pItem = pItem->GetNext())
	{
		if ( pItem->IsType( IT_FIGURINE ) && pItem->m_uidLink == pCharPlayer->GetUID())
			iCount++;
	}

	if ( iCount > 10 )
	{
		Speak( "I'm sorry you have too many pets stabled here already. Tell me to 'retrieve' if you want them." );
		return( HRES_INVALID_HANDLE );
	}

	pCharPlayer->m_pClient->m_Targ.m_PrvUID = GetUID();
	pCharPlayer->m_pClient->addTarget( CLIMODE_TARG_PET_STABLE, "What pet would you like to stable here" );

	return( NO_ERROR );
}

HRESULT CChar::NPC_StablePetRetrieve( CChar* pCharPlayer )
{
	// Get pets for this person from my inventory.
	// May want to put up a menu ???

	if ( ! m_pNPC.IsValidNewObj())
		return( HRES_INVALID_HANDLE );
	if ( m_pNPC->m_Brain != NPCBRAIN_STABLE )
		return( HRES_INVALID_HANDLE );

	int iCount = 0;
	CItemPtr pItem = GetBank()->GetHead();
	while ( pItem!=NULL )
	{
		CItemPtr pItemNext = static_cast<CItem*>(pItem->GetNext());
		if ( pItem->IsType( IT_FIGURINE ) && pItem->m_uidLink == pCharPlayer->GetUID())
		{
			if ( pCharPlayer->Use_Figurine( pItem, 2 ))
			{
				pItem->DeleteThis();
			}
			iCount++;
		}
		pItem = pItemNext;
	}

	if ( ! iCount )
	{
		Speak( "Sorry I have no stabled animals for you" );
		return( HRES_INVALID_HANDLE );
	}
	else
	{
		Speak( "Treat them well" );
	}

	return( NO_ERROR );
}

void CChar::NPC_ActStart_SpeakTo( CChar* pSrc )
{
	// My new action is that i am speaking to this person.
	// Or just update the amount of time i will wait for this person.

	// if ( IsStatFlag( STATF_Pet )) return;
	ASSERT(pSrc);
	m_Act.m_Targ = pSrc->GetUID();
	m_Act.m_atTalk.m_WaitCount = 20;
	m_Act.m_atTalk.m_HearUnknown = 0;

	Skill_Start( ( pSrc->Stat_Get(STAT_Fame) > 7000 ) ? NPCACT_TALK_FOLLOW : NPCACT_TALK );
	SetTimeout( 3*TICKS_PER_SEC );
	UpdateDir( pSrc );
}

bool CChar::NPC_OnHearFirst( CChar* pCharSrc, CItemMemory* pMemory )
{
	// I've heard them for the first time.
	if ( pMemory == NULL || ! ( pMemory->GetMemoryTypes() & MEMORY_SPEAK ))
	{
		// This or CCharDef::T_SeeNewPlayer will be our first contact with people.
		CSphereExpContext exec(this, pCharSrc);	
		if ( OnTrigger( CCharDef::T_NPCHearGreeting, exec) == TRIGRET_RET_VAL )
			return true;

		// record that we attempted to speak to them.
		pMemory = Memory_AddObjTypes( pCharSrc, MEMORY_SPEAK );
		ASSERT(pMemory);
		pMemory->m_itEqMemory.m_Arg1 = NPC_MEM_ACT_FIRSTSPEAK;
		// m_Act_Hear_Unknown = 0;
	}

	return false;
}

void CChar::NPC_OnHear( LPCTSTR pszCmd, CChar* pSrc )
{
	// This CChar has heard you say something.
	ASSERT( pSrc );
	if ( ! m_pNPC )
		return;

	// Pets always have a basic set of actions.
	if ( NPC_OnHearPetCmd( pszCmd, pSrc, false ))
		return;
	if ( ! NPC_CanSpeak())	// can they speak ?
		return;

	// What where we doing ?
	// too busy to talk ?

	switch ( Skill_GetActive())
	{
	case SKILL_BEGGING: // busy begging. (hack)
		return;
	case NPCACT_TALK:
	case NPCACT_TALK_FOLLOW:
		// Was NPC talking to someone else ?
		if ( m_Act.m_Targ != pSrc->GetUID())
		{
			if ( NPC_Act_Talk())
			{
				CCharPtr pCharOld = g_World.CharFind(m_Act.m_Targ);
				CGString sMsg;
				sMsg.Format( "Excuse me %s, but %s wants to speak to me",
					(LPCTSTR) pCharOld->GetName(), (LPCTSTR) pSrc->GetName());
				Speak( sMsg );
			}
		}
		break;
	}

	// Does the NPC know you ?
	CItemMemoryPtr pMemory = Memory_FindObj( pSrc );
	if ( pMemory && ( pMemory->GetMemoryTypes() & MEMORY_SPEAK ))
	{
		// Are they just spamming me ?
		if ( pMemory->m_itEqMemory.m_timeStart.GetCacheAge() < TICKS_PER_SEC )
			return;
	}

	if ( NPC_OnHearFirst(pSrc, pMemory ))
		return;

	// Do the scripts want me to take some action based on this speech.
	SKILL_TYPE skill = m_Act.m_SkillCurrent;

	CSphereExpArgs exec( this, pSrc, pszCmd );

	int i=0;
	for ( ; i<m_pNPC->m_Speech.GetSize(); i++ )
	{
		TRIGRET_TYPE iRet = OnHearTrigger( m_pNPC->m_Speech[i], exec );
		if ( iRet == TRIGRET_RET_DEFAULT && skill == m_Act.m_SkillCurrent )
		{
			// You are the new speaking target.
			NPC_ActStart_SpeakTo( pSrc );
			continue;
		}
		if ( iRet != TRIGRET_ENDIF && iRet != TRIGRET_RET_FALSE )
			break;
	}

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);

	for ( i=0; i<pCharDef->m_Speech.GetSize(); i++ )
	{
		TRIGRET_TYPE iRet = OnHearTrigger( pCharDef->m_Speech[i], exec );
		if ( iRet == TRIGRET_RET_DEFAULT && skill == m_Act.m_SkillCurrent )
		{
			// You are the new speaking target.
			NPC_ActStart_SpeakTo( pSrc );
			continue;
		}
		if ( iRet != TRIGRET_ENDIF && iRet != TRIGRET_RET_FALSE )
			break;
	}

	// hard code some default reactions.
	if ( m_pNPC->m_Brain == NPCBRAIN_HEALER )
	{
		if ( NPC_LookAtChar( pSrc, 1 ))
			return;
	}

	// can't figure you out.
	if ( OnTrigger( CCharDef::T_NPCHearUnknown, exec) == TRIGRET_RET_VAL )
		return;

	if ( Skill_GetActive() == NPCACT_TALK ||
		Skill_GetActive() == NPCACT_TALK_FOLLOW )
	{
		++ m_Act.m_atTalk.m_HearUnknown;
		int iMaxUnk = 4;
		if ( GetDist( pSrc ) > 4 )
			iMaxUnk = 1;
		if ( m_Act.m_atTalk.m_HearUnknown > iMaxUnk )
		{
			Skill_Start( SKILL_NONE ); // say good by
		}
	}
}

#if 0

bool CChar::OnItemAcquire( CItem* pItem )
{
	// I now have this object.
	// What do i want to use it for ?
	// I can beneift from items that i find.

	// Equip weapons and armor, (if better than what we have)

	// drink (beneficial) potions (etc) only if in war mode.

}

#endif

int CChar::NPC_OnTrainCheck( CChar* pCharSrc, SKILL_TYPE Skill )
{
	// Can we train in this skill ?
	// RETURN: Amount of skill we can train.
	//

	if ( ! IsSkillBase( Skill ))
	{
		Speak( "I know nothing about that" );
		return( 0 );
	}

	int iSkillSrcVal = pCharSrc->Skill_GetBase(Skill);
	int iSkillVal = Skill_GetBase(Skill);
	int iTrainCost = NPC_GetTrainMax( pCharSrc, Skill ) - iSkillSrcVal;

	LPCTSTR pszMsg;
	if ( iSkillVal <= 0 )
	{
		pszMsg = "I know nothing about %s";
	}
	else if ( iSkillSrcVal > iSkillVal )
	{
		pszMsg = "You know more about %s than I do";
	}
	else if ( iTrainCost <= 0 )
	{
		pszMsg = "You already know as much as I can teach of %s";
	}
	else
	{
		return( iTrainCost );
	}

	CGString sMsg;
	sMsg.Format( pszMsg, (LPCTSTR) g_Cfg.GetSkillKey( Skill ));
	Speak( sMsg );
	return( 0 );
}

bool CChar::NPC_OnTrainPay( CChar* pCharSrc, CItemMemory* pMemory, CItem* pGold )
{
	ASSERT( pMemory );
	DEBUG_CHECK( pMemory->GetMemoryTypes() & MEMORY_SPEAK );

	SKILL_TYPE skill = (SKILL_TYPE)( pMemory->m_itEqMemory.m_Arg2 );
	if ( ! IsSkillBase( skill ))
	{
		Speak( "I can't remember what I was supposed to teach you" );
		return( false );
	}

	int iTrainCost = NPC_OnTrainCheck( pCharSrc, skill );
	if ( iTrainCost <= 0 )
		return false;

	Speak( "Let me show you something of how this is done" );

	// Consume as much money as we can train for.
	ASSERT( pGold );
	if ( pGold->GetAmount() < iTrainCost )
	{
		iTrainCost = pGold->GetAmount();
	}
	else if ( pGold->GetAmount() == iTrainCost )
	{
		Speak( "That is all I can teach." );
		pMemory->m_itEqMemory.m_Arg1 = NPC_MEM_ACT_NONE;
	}
	else
	{
		Speak( "I wish i could teach more but I cannot." );
		pMemory->m_itEqMemory.m_Arg1 = NPC_MEM_ACT_NONE;

		// Give change back.
		pGold->UnStackSplit( iTrainCost, pCharSrc );
	}

	GetPackSafe()->ContentAdd( pGold );	// take my cash.

	// Give credit for training.
	pCharSrc->Skill_SetBase( skill, pCharSrc->Skill_GetBase(skill) + iTrainCost );
	return( true );
}

HRESULT CChar::NPC_OnTrainHear( CChar* pCharSrc, LPCTSTR pszCmd )
{
	// We are asking for training ?

	if ( ! m_pNPC.IsValidNewObj() )
		return( HRES_INVALID_HANDLE );
	if ( ! NPC_IsVendor() &&
		m_pNPC->m_Brain != NPCBRAIN_HUMAN )
	{
		return HRES_INVALID_HANDLE;
	}

	if ( Memory_FindObjTypes( pCharSrc, MEMORY_FIGHT|MEMORY_HARMEDBY|MEMORY_IRRITATEDBY|MEMORY_AGGREIVED ))
	{
		Speak( "I would never train the likes of you." );
		return HRES_INVALID_HANDLE;
	}

	// Did they mention a skill name i recognize ?

	int i=SKILL_First;
	for ( ; i<SKILL_QTY; i++ )
	{
		LPCTSTR pSkillKey = g_Cfg.GetSkillKey( (SKILL_TYPE) i );
		if ( ! Str_FindWord( pszCmd, pSkillKey ))
			continue;

		// Can we train in this ?
		int iTrainCost = NPC_OnTrainCheck( pCharSrc, (SKILL_TYPE) i );
		if ( iTrainCost <= 0 )
			return NO_ERROR;

		CGString sMsg;
		sMsg.Format( "For %d gold I will train you in all I know of %s. For less gold I will teach you less",
			iTrainCost, (LPCTSTR) pSkillKey );

		Speak( sMsg );
		CItemMemoryPtr pMemory = Memory_AddObjTypes( pCharSrc, MEMORY_SPEAK );
		ASSERT(pMemory);
		pMemory->m_itEqMemory.m_Arg1 = NPC_MEM_ACT_SPEAK_TRAIN;
		pMemory->m_itEqMemory.m_Arg2 = i;
		return NO_ERROR;
	}

	// What can he teach me about ?
	// Just tell them what we can teach them or set up a memory to train.

	TCHAR szMsg[ CSTRING_MAX_LEN ];
	strcpy( szMsg, "For a fee I can teach thee of " );

	LPCTSTR pPrvSkill = NULL;

	int iCount = 0;
	for ( i=SKILL_First; i<SKILL_QTY; i++ )
	{
		int iDiff = NPC_GetTrainMax( pCharSrc, (SKILL_TYPE)i ) - pCharSrc->Skill_GetBase( (SKILL_TYPE) i);
		if ( iDiff <= 0 )
			continue;

		if ( iCount > 6 )
		{
			pPrvSkill = "more";
			break;
		}
		if ( iCount > 1 )
		{
			strcat( szMsg, ", " );
		}
		if ( pPrvSkill )
		{
			strcat( szMsg, pPrvSkill );
		}
		pPrvSkill = g_Cfg.GetSkillKey( (SKILL_TYPE) i );
		iCount ++;
	}

	if ( iCount == 0 )
	{
		Speak( "There is nothing that I can teach you." );
		return NO_ERROR;
	}
	if ( iCount > 1 )
	{
		strcat( szMsg, " and " );
	}

	strcat( szMsg, pPrvSkill );
	strcat( szMsg, "." );
	Speak( szMsg );
	return( NO_ERROR );
}

bool CChar::NPC_Act_Begging( CChar* pChar )
{
	// SKILL_BEGGING
	bool fSpeak;

	if ( pChar )
	{
		// Is this a proper target for begging ?
		if ( pChar == this ||
			pChar->m_pNPC.IsValidNewObj() ||	// Don't beg from NPC's or the PC you just begged from
			pChar->GetUID() == m_Act.m_TargPrv )	// Already targeting this person.
			return( false );

		if ( pChar->GetUID() == m_Act.m_Targ && Skill_GetActive() == SKILL_BEGGING )
			return( true );

		Skill_Start( SKILL_BEGGING );
		m_Act.m_Targ = pChar->GetUID();
		fSpeak = true;
	}
	else
	{
		// We are already begging.
		pChar = g_World.CharFind(m_Act.m_Targ);
		if ( pChar == NULL )
		{
		bailout:
			m_Act.m_Targ.InitUID();
			Skill_Start( SKILL_NONE );
			return( false );
		}

		if ( ! CanSee( pChar ))	// can we still see them ?
		{
			if ( Calc_GetRandVal(75) > m_StatInt) // Dumb beggars think I'm gone
			{
				goto bailout;
			}
		}

		// Time to beg ?
		fSpeak = ! Calc_GetRandVal(6);
	}

	SetTimeout( 2*TICKS_PER_SEC );

	static LPCTSTR const sm_szSpeakBeggar[] =
	{
		"Could thou spare a few coins?",
		"I have a family to feed, think of the children.",
		"Can you spare any gold?",
		"I have children to feed.",
		"I haven't eaten in days.",
		"Please? Just a few coins.",
	};

	UpdateDir( pChar );	// face PC
	if ( fSpeak )
	{
		Speak( sm_szSpeakBeggar[ Calc_GetRandVal( COUNTOF( sm_szSpeakBeggar )) ] );
	}

	if ( ! Calc_GetRandVal( ( 10100 - pChar->Stat_Get(STAT_Fame)) / 50 ))
	{
		UpdateAnimate( ANIM_BOW );
		return( true );
	}

	return( NPC_Act_Follow( false, 2 ));
}

void CChar::NPC_OnNoticeSnoop( CChar* pCharThief, CChar* pCharMark )
{
	// CScriptArgs Args( SKILL_SNOOPING, pCharThief, pCharMark );
	// OnTrigger( CCharDef::T_NPCSeeAction, Args );

	ASSERT(m_pNPC.IsValidNewObj());

	// start making them angry at you.
	static LPCTSTR const sm_szTextSnoop[] =
	{
		"Get thee hence, busybody!",
		"What do you think your doing ?",
		"Ack, away with you!",
		"Beware or I'll call the guards!",
	};

	if ( pCharMark != this )	// not me so who cares.
		return;

	if ( NPC_CanSpeak())
	{
		Speak( sm_szTextSnoop[ Calc_GetRandVal( COUNTOF( sm_szTextSnoop )) ] );
	}
	if ( ! Calc_GetRandVal(4))
	{
		m_Act.m_Targ = pCharThief->GetUID();
		m_Act.m_atFlee.m_iStepsMax = 20;	// how long should it take to get there.
		m_Act.m_atFlee.m_iStepsCurrent = 0;	// how long has it taken ?
		Skill_Start( NPCACT_FLEE );
	}
}

#if 0

void CChar::NPC_MoveBlockingObject( CPointMap pt )
{
	// we have been blocked from walking to this point.
	// try to move the offending object.

}

#endif

int CChar::NPC_WalkToPoint( bool fRun )
{
	// Move toward my target .
	//
	// RETURN:
	//  0 = we are here.
	//  1 = took the step.
	//  2 = can't take this step right now. (obstacle)

	CPointMap pt = GetTopPoint();
	DIR_TYPE Dir = pt.GetDir( m_Act.m_pt );
	if ( Dir >= DIR_QTY )
		return( 0 );	// we are here.
	if ( m_StatDex <= 0 )
		return( 2 );

	if ( ! CheckMoveWalkDir(pt,Dir,true))
	{
		// try to step around it ?
		int iDiff = 0;
		int iRand = Calc_GetRandVal( 100 );
		if ( iRand < 30 )	// do nothing.
			return( 2 );
		if ( iRand < 35 ) iDiff = 4;	// 5
		else if ( iRand < 40 ) iDiff = 3;	// 10
		else if ( iRand < 65 ) iDiff = 2;
		else iDiff = 1;
		if ( iRand & 1 ) iDiff = -iDiff;
		pt = GetTopPoint();
		Dir = GetDirTurn( Dir, iDiff );
		if ( ! CheckMoveWalkDir(pt,Dir,true))
		{
			// ??? Some object in my way that i could move ?
			// Is there something in the way ?
			// try to move it.
			// NPC_MoveBlockingObject(pt);
			return( 2 );
		}
	}

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);

	// ??? Make sure we are not facing a wall.
	m_dirFace = Dir;	// Face this direction.
	if ( fRun && ( ! pCharDef->Can(CAN_C_RUN|CAN_C_FLY) || m_StatStam <= 1 ))
		fRun = false;

	StatFlag_Mod( STATF_Fly, fRun );

	CPointMap pold = GetTopPoint();
	CheckRevealOnMove();
	MoveToChar( pt );
	UpdateMove( pold );
	CheckLocation( false );	// Look for teleports etc.

	// How fast can they move.
	int iTickNext;
	if ( fRun )
	{
		int iDex = m_StatDex;
		if ( IsStatFlag( STATF_Pet ))	// pets run a little faster.
		{
			if ( iDex < 50 )
				iDex = 50;
		}
		iTickNext = TICKS_PER_SEC/4 + Calc_GetRandVal( (100-iDex)/4 )* TICKS_PER_SEC / 10;
	}
	else
	{
		iTickNext = TICKS_PER_SEC/2 + Calc_GetRandVal( (150-m_StatDex)/2 )* TICKS_PER_SEC / 10;
	}

	SetTimeout( iTickNext );
	return( 1 );
}

bool CChar::NPC_MotivationCheck( MOTIVE_LEVEL iMotivation )
{
	// Am i currently doing something more important ?
	ASSERT( m_pNPC.IsValidNewObj() );
	if ( iMotivation < m_pNPC->m_Act_Motivation )
		return( false );
	m_pNPC->m_Act_Motivation = iMotivation;
	return( true );
}

bool CChar::NPC_LookAtCharGuard( CChar* pChar )
{
	// Does the guard hate the target ?

	if ( pChar->IsStatFlag( STATF_INVUL|STATF_DEAD ))
		return( false );
	if ( ! pChar->Noto_IsCriminal())
		return false;

	static LPCTSTR const sm_szSpeakGuardJeer[] =
	{
		"%s thou art a cowardly swine.",
		"You shall get whats coming to you %s",
		"Evildoers shay pay %s!",
		"Beware of me foul %s",
		"Get thee gone foul %s",
	};

	if ( pChar->IsPrivFlag( PRIV_JAILED ))
	{
		// Don't kill if already jailed.
		return( false );
	}

	if ( ! pChar->m_pArea->IsFlagGuarded() )
	{
		// At least jeer at the criminal.
		if ( Calc_GetRandVal(10))
			return( false );
		CGString sMsg;
		sMsg.Format( sm_szSpeakGuardJeer[ Calc_GetRandVal( COUNTOF( sm_szSpeakGuardJeer )) ], (LPCTSTR) pChar->GetName());
		Speak( sMsg );
		UpdateDir( pChar );
		return false;
	}

	static LPCTSTR const sm_szSpeakGuardStrike[] =
	{
		"Thou shalt regret thine actions, swine!",
		"Death to all evil!",
		"Take this vile criminal!",
		"Evildoers shay pay!",
		"Take this swine!",
	};

	if ( GetTopDist3D( pChar ) > 1 )
	{
		if ( pChar->Skill_GetBase(SKILL_MAGERY))
		{
			Spell_Effect_Teleport( pChar->GetTopPoint(), false, false );
		}
		// If we got intant kill guards enabled, allow the guards
		// to take a swing directly after the teleport.
		if ( g_Cfg.m_fGuardsInstantKill )
		{
			Fight_Hit( pChar );
		}
	}
	if ( ! IsStatFlag( STATF_War ) || m_Act.m_Targ != pChar->GetUID())
	{
		Speak( sm_szSpeakGuardStrike[ Calc_GetRandVal( COUNTOF( sm_szSpeakGuardStrike )) ] );
		Fight_Attack( pChar );
	}
	return( true );
}

bool CChar::NPC_LookAtCharMonster( CChar* pChar )
{
	// return:
	//   true = take new action.
	//   false = continue with any previous action.
	//  motivation level =
	//  0 = not at all.
	//  100 = definitly.
	//

	int iFoodLevel = Food_GetLevelPercent();

	// Attacks those not of my kind.
	if ( ! Noto_IsCriminal() && iFoodLevel > 40 )		// I am not evil ?
	{
		return NPC_LookAtCharHuman( pChar );
	}

	// Attack if i am stronger.
	// or i'm just stupid.
	int iActMotivation = NPC_GetAttackMotivation( pChar );
	if ( iActMotivation <= 0 )
		return( false );
	if ( Fight_IsActive() && m_Act.m_Targ == pChar->GetUID())	// same targ.
		return( false );
	ASSERT( m_pNPC.IsValidNewObj());
	if ( iActMotivation < m_pNPC->m_Act_Motivation )
		return( false );

	int iDist = GetTopDist3D( pChar );
	if ( IsStatFlag( STATF_Hidden ) &&
		! NPC_FightMayCast() &&
		iDist > 1 )
		return false;	// element of suprise.

	Fight_Attack( pChar );
	m_pNPC->m_Act_Motivation = iActMotivation;
	return true;
}

bool CChar::NPC_LookAtCharHuman( CChar* pChar )
{
	if ( pChar->IsStatFlag( STATF_DEAD ))
		return false;

	ASSERT( m_pNPC.IsValidNewObj());

	if ( Noto_IsEvil())		// I am evil.
	{
		// Attack others if we are evil.
		return( NPC_LookAtCharMonster( pChar ));
	}

	if ( ! pChar->Noto_IsCriminal())	// not interesting.
		return( false );

	// Yell for guard if we see someone evil.
	if ( NPC_CanSpeak() &&
		m_pArea->IsFlagGuarded() &&
		! Calc_GetRandVal( 3 ))
	{
		Speak( pChar->IsStatFlag( STATF_Criminal) ?
			"Help! Guards a Criminal!" :
			"Help! Guards a Monster!" );
		// Find a guard.
		CallGuards( pChar );
		if ( IsStatFlag( STATF_War ))
			return( false );

		// run away like a coward.
		m_Act.m_Targ = pChar->GetUID();
		m_Act.m_atFlee.m_iStepsMax = 20;	// how long should it take to get there.
		m_Act.m_atFlee.m_iStepsCurrent = 0;	// how long has it taken ?
		Skill_Start( NPCACT_FLEE );
		m_pNPC->m_Act_Motivation = 80;
		return true;
	}

	// Attack an evil creature ?

	return( false );
}

bool CChar::NPC_LookAtCharHealer( CChar* pChar )
{
	static LPCTSTR const sm_szHealerRefuseEvils[] =
	{
		"Thou hast committed too many unholy acts to be resurrected",
		"Thou hast strayed from the virtues and I shall not resurrect you.",
		"Thy soul is too dark for me to resurrect.",
	};
	static LPCTSTR const sm_szHealerRefuseCriminals[] =
	{
		"Thou art a criminal, I shall not resurrect you.",
		"Because of your lawless ways, I shall not help you.",
		"It wouldn't be right to help someone like you.",
	};
	static LPCTSTR const sm_szHealerRefuseGoods[] =
	{
		"You are good, why would I help you.",
		"Change thy virtuous ways, and I may consider resurrecting you.",
		"Muhaaahaa, You have got to be kidding, I shall not resurrect such as thee.",
	};
	static LPCTSTR const sm_szHealer[] =
	{
		"Thou art dead, but 'tis within my power to resurrect thee.  Live!",
		"Allow me to resurrect thee ghost.  Thy time of true death has not yet come.",
		"Perhaps thou shouldst be more careful.  Here, I shall resurrect thee.",
		"Live again, ghost!  Thy time in this world is not yet done.",
		"I shall attempt to resurrect thee.",
	};

	if ( ! pChar->IsStatFlag( STATF_DEAD ))
		return false;

	UpdateDir( pChar );

	LPCTSTR pszRefuseMsg;

	int iDist = GetDist( pChar );
	if ( pChar->IsStatFlag( STATF_Insubstantial ))
	{
		pszRefuseMsg = "I can't see you clearly ghost. Manifest for me to heal thee.";
	refuse_to_rez:
		if ( Calc_GetRandVal(5) || iDist > 3 )
			return false;
		Speak( pszRefuseMsg );
		return true;
	}

	if ( iDist > 3 )
	{
		if ( Calc_GetRandVal(5))
			return false;
		Speak( "You are too far away ghost. Come closer." );
		return( true );
	}

	// What noto is this char to me ?
	bool ImEvil = Noto_IsEvil();
	bool ImNeutral = Noto_IsNeutral();
	NOTO_TYPE NotoThem = pChar->Noto_GetFlag( this, true );

	if ( !IsStatFlag( STATF_Criminal ) && NotoThem == NOTO_CRIMINAL )
	{
		pszRefuseMsg = sm_szHealerRefuseCriminals[ Calc_GetRandVal( COUNTOF( sm_szHealerRefuseCriminals )) ];
		goto refuse_to_rez;
	}

	if (( !ImNeutral && !ImEvil) && NotoThem >= NOTO_NEUTRAL )
	{
		pszRefuseMsg = sm_szHealerRefuseEvils[ Calc_GetRandVal( COUNTOF( sm_szHealerRefuseEvils )) ];
		goto refuse_to_rez;
	}

	if (( ImNeutral || ImEvil ) && NotoThem == NOTO_GOOD )
	{
		pszRefuseMsg = sm_szHealerRefuseGoods[ Calc_GetRandVal( COUNTOF( sm_szHealerRefuseGoods )) ];
		goto refuse_to_rez;
	}

	// Attempt to res.
	Speak( sm_szHealer[ Calc_GetRandVal( COUNTOF( sm_szHealer )) ] );
	UpdateAnimate( ANIM_CAST_AREA );
	if ( ! pChar->OnSpellEffect( SPELL_Resurrection, this, 1000, NULL ))
	{
		if ( Calc_GetRandVal(2))
			Speak( "I'm sorry but i cannot resurrect you" );
		else
			Speak( "Ah! My magic is failing!" );
	}

	return true;
}

bool CChar::NPC_LookAtItem( CItem* pItem, int iDist )
{
	// I might want to go pickup this item ?
	if ( ! CanSee( pItem ))
		return( false );

	// Am i hungry and this is food ?
	if ( NPC_WantThisItem( pItem ))
	{
		// Looks for unconscious or sleeping people.
		// Don't do this if we've already looted this item
		if ( Memory_FindObj( pItem ) == NULL )
		{
			m_Act.m_Targ = pItem->GetUID();
			CObjBasePtr pObjTop = pItem->GetTopLevelObj();
			ASSERT(pObjTop);
			m_Act.m_TargPrv = pObjTop->GetUID();
			m_Act.m_pt = pObjTop->GetTopPoint();
			m_Act.m_atLooting.m_iDistEstimate = GetTopDist(pObjTop)* 2;
			m_Act.m_atLooting.m_iDistCurrent = 0;
			Skill_Start( NPCACT_LOOTING );
			return true;
		}
	}

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);

	// check for doors we can open.
	if ( m_StatInt > 20 &&
		pCharDef->Can(CAN_C_USEHANDS) &&
		pItem->IsType( IT_DOOR ) &&	// not locked.
		GetDist( pItem ) <= 1 &&
		CanTouch( pItem ) &&
		!Calc_GetRandVal(2))
	{
		// Is it opened or closed?
		if ( pItem->IsDoorOpen())
			return( false );

		// The door is closed.
		UpdateDir( pItem );
		if ( ! Use_Item( pItem ))	// try to open it.
			return( false );

		// Walk through it
		CPointMap pt = GetTopPoint();
		if ( CheckMoveWalkDir( pt, GetDir(pItem), true ))
		{
			m_Act.m_pt = pt;
			NPC_WalkToPoint();
			return true;
		}
	}

	return( false );
}

bool CChar::NPC_LookAtChar( CChar* pChar, int iDist )
{
	// I see a char.
	// Do I want to do something to this char (more that what i'm already doing ?)
	// RETURN:
	//   true = yes i do want to take a new action.
	//

	if ( ! m_pNPC.IsValidNewObj())
		return( false );
	if ( pChar == this )
		return( false );
	if ( ! CanSeeLOS( pChar ))
		return( false );

	if ( NPC_IsOwnedBy( pChar, false ))
	{
		// pets should protect there owners unless told otherwise.
		if ( pChar->Fight_IsActive())
		{
			CCharPtr pCharTarg = g_World.CharFind(pChar->m_Act.m_Targ);
			if ( Fight_Attack(pCharTarg))
				return true;
		}

		// follow my owner again. (Default action)
		m_Act.m_Targ = pChar->GetUID();
		m_Act.m_atFollowTarg.m_DistMin = 1;
		m_Act.m_atFollowTarg.m_DistMax = 6;
		m_pNPC->m_Act_Motivation = 50;
		Skill_Start( NPCACT_FOLLOW_TARG );
		return true;
	}

	else
	{
		// initiate a conversation ?
		if ( ! IsStatFlag( STATF_War ) &&
			( Skill_GetActive() == SKILL_NONE || Skill_GetActive() == NPCACT_WANDER ) && // I'm idle
			pChar->m_pPlayer.IsValidNewObj() &&
			! Memory_FindObjTypes( pChar, MEMORY_SPEAK ))
		{
			CSphereExpContext se(this, pChar);
			if ( OnTrigger( CCharDef::T_NPCSeeNewPlayer, se) != TRIGRET_RET_VAL )
			{
				// record that we attempted to speak to them.
				CItemMemoryPtr pMemory = Memory_AddObjTypes( pChar, MEMORY_SPEAK );
				ASSERT(pMemory);
				pMemory->m_itEqMemory.m_Arg1 = NPC_MEM_ACT_FIRSTSPEAK;
				// m_Act_Hear_Unknown = 0;
			}
		}
	}

	switch ( m_pNPC->m_Brain )	// my type of brain
	{
	case NPCBRAIN_GUARD:
		// Guards should look around for criminals or nasty creatures.
		if ( NPC_LookAtCharGuard( pChar ))
			return true;
		break;

	case NPCBRAIN_BEGGAR:
		if ( NPC_Act_Begging( pChar ))
			return true;
		if ( NPC_LookAtCharHuman( pChar ))
			return( true );
		break;

	case NPCBRAIN_MONSTER:
	case NPCBRAIN_UNDEAD:
	case NPCBRAIN_DRAGON:
		if ( NPC_LookAtCharMonster( pChar ))
			return( true );
		break;

	case NPCBRAIN_BESERK:
		// Blades or EV.
		// ??? Attack everyone you touch !
		if ( iDist <= 1 )
		{
			Fight_Hit( pChar );
		}
		if ( Fight_IsActive()) // Is this a better target than my last ?
		{
			CCharPtr pCharTarg = g_World.CharFind(m_Act.m_Targ);
			if ( pCharTarg != NULL )
			{
				if ( iDist >= GetTopDist3D( pCharTarg ))
					break;
			}
		}
		// if ( ! NPC_MotivationCheck( 50 )) continue;
		Fight_Attack( pChar );
		break;

	case NPCBRAIN_HEALER:
		// Healers should look around for ghosts.
		if ( NPC_LookAtCharHealer( pChar ))
			return( true );
		if ( NPC_LookAtCharHuman( pChar ))
			return( true );
		break;

	case NPCBRAIN_CRIER:
	case NPCBRAIN_BANKER:
	case NPCBRAIN_VENDOR:
	case NPCBRAIN_STABLE:
	case NPCBRAIN_ANIMAL:
	case NPCBRAIN_HUMAN:
	case NPCBRAIN_THIEF:
		if ( NPC_LookAtCharHuman( pChar ))
			return( true );
		break;
	}

	return( false );
}

bool CChar::NPC_LookAround( bool fForceCheckItems )
{
	// Take a look around for other people/chars.
	// We may be doing something already. So check current action motivation level.
	// RETURN:
	//   true = found something better to do.

	ASSERT( m_pNPC.IsValidNewObj());
	if ( m_pNPC->m_Brain == NPCBRAIN_BESERK || ! Calc_GetRandVal(6))
	{
		// Make some random noise
		//
		SoundChar( Calc_GetRandVal(2) ? CRESND_RAND1 : CRESND_RAND2 );
	}

	int iRange = SPHEREMAP_VIEW_SIGHT;
	int iRangeBlur = iRange;

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);

	// If I can't move don't look to far.
	if ( ! pCharDef->Can( CAN_C_WALK | CAN_C_FLY | CAN_C_SWIM | CAN_C_CLIMB ) ||
		IsStatFlag( STATF_Stone | STATF_Freeze | STATF_Immobile ))
	{
		// I'm NOT mobile.
		if ( ! NPC_FightMayCast())	// And i have no distance attack.
		{
			iRange = 2;
			iRangeBlur = 2;
		}
	}
	else
	{
		// I'm mobile. do basic check if i would move here first.

		if ( ! NPC_CheckWalkHere( GetTopPoint(), m_pArea, true ))
		{
			// I should move. Someone lit a fire under me.
			m_Act.m_pt = GetTopPoint();
			m_Act.m_pt.Move( (DIR_TYPE) Calc_GetRandVal( DIR_QTY ));
			NPC_WalkToPoint( true );
			return( true );
		}

		if ( m_StatInt < 50 )
			iRangeBlur = iRange/2;
	}

	// Lower the number of chars we look at if complex.
	if ( GetTopPoint().GetSector()->GetCharComplexity() > g_Cfg.m_iMaxCharComplexity )
		iRange /= 2;

	// Any interesting chars here ?
	int iCount = 0;
	CWorldSearch Area( GetTopPoint(), iRange );
	for(;;)
	{
		CCharPtr pChar = Area.GetNextChar();
		if ( pChar == NULL )
			break;
		if ( pChar == this )	// just myself.
			continue;

		int iDist = GetTopDist3D( pChar );
		if ( iDist > iRangeBlur && ! pChar->IsStatFlag(STATF_Fly))
		{
			if ( Calc_GetRandVal(iDist))
				continue;	// can't see them.
		}
		if ( NPC_LookAtChar( pChar, iDist ))
			return( true );

		if ( ++iCount > g_Cfg.m_iMaxCharComplexity )
			break;
	}

	// Check the ground for good stuff.
	if ( ! fForceCheckItems &&
		m_StatInt > 10 &&
		! IsSkillBase( Skill_GetActive()) &&
		! Calc_GetRandVal( 3 ))
	{
		fForceCheckItems = true;
	}

	if ( fForceCheckItems )
	{
		CWorldSearch Area( GetTopPoint(), iRange );
		for(;;)
		{
			CItemPtr pItem = Area.GetNextItem();
			if ( pItem == NULL )
				break;
			int iDist = GetTopDist3D( pItem );
			if ( iDist > iRangeBlur )
			{
				if ( Calc_GetRandVal(iDist))
					continue;	// can't see them.
			}
			if ( NPC_LookAtItem( pItem, iDist ))
				return( true );
		}
	}

	// Move stuff that is in our way ? (chests etc.)

	return m_pNPC->m_Act_Motivation; // Is this a better target than my last ?
}

void CChar::NPC_Act_Wander()
{
	// NPCACT_WANDER
	// just wander aimlessly. (but within bounds)
	// Stop wandering and re-eval frequently

	if ( ! Calc_GetRandVal( 7 + ( m_StatStam / 30 )))
	{
		// Stop wandering ?
		Skill_Start( SKILL_NONE );
		return;
	}

	if ( Calc_GetRandVal( 2 ))
	{
		if ( NPC_LookAround())
			return;
	}

	// Staggering Walk around.
	m_Act.m_pt = GetTopPoint();
	m_Act.m_pt.Move( GetDirTurn( m_dirFace, 1 - Calc_GetRandVal(3)));

	ASSERT( m_pNPC.IsValidNewObj());
	if ( m_pNPC->m_Home_Dist_Wander && m_ptHome.IsValidPoint())
	{
		if ( m_Act.m_pt.GetDist( m_ptHome ) > m_pNPC->m_Home_Dist_Wander )
		{
			Skill_Start( NPCACT_GO_HOME );
			return;
		}
	}

	NPC_WalkToPoint();
}

bool CChar::NPC_Act_Follow( bool fFlee, int maxDistance, bool forceDistance )
{
	// Follow our target or owner. (m_Act.m_Targ) we may be fighting.
	// false = can't follow any more. give up.

	CCharPtr pChar = g_World.CharFind(m_Act.m_Targ);
	if ( pChar == NULL )
	{
		// free to do as i wish !
		Skill_Start( SKILL_NONE );
		return( false );
	}

	// Have to be able to see target to follow.
	if ( CanSee( pChar ))
	{
		m_Act.m_pt = pChar->GetTopPoint();
	}
	else
	{
		// Monster may get confused because he can't see you.
		// There is a chance they could forget about you if hidden for a while.
		if ( ! Calc_GetRandVal( 1 + (( 100 - m_StatInt) / 20 )))
			return( false );
		if ( fFlee )
			return( false );
	}

	int dist = GetTopPoint().GetDist( m_Act.m_pt );
	if ( dist > SPHEREMAP_VIEW_RADAR*2 )			// too far away ?
		return( false );

	if ( forceDistance )
	{
		if ( dist < maxDistance )
		{
			// start moving away
			fFlee = true;
		}
	}
	else
	{
		if ( fFlee )
		{
			if ( dist >= maxDistance/* 1*/ )
				return( false );
		}
		else
		{
			if ( dist <= maxDistance/* 1*/ )
			{
				// I'm happy here ?
				// Random side step ? How jumpy an i ?
				// if ( IsStatFlag(STATF_War) && ! Calc_GetRandVal(3))
				//{
				//	NPC_WalkToPoint( Calc_GetRandVal(2));
				//}
				return( true );
			}
		}
	}

	if ( fFlee )
	{
		CPointMap ptOld = m_Act.m_pt;
		m_Act.m_pt = GetTopPoint();
		m_Act.m_pt.Move( GetDirTurn( m_Act.m_pt.GetDir( ptOld ), 4 + 1 - Calc_GetRandVal(3)));
		NPC_WalkToPoint( dist < Calc_GetRandVal(10));
		m_Act.m_pt = ptOld;	// last known point of the enemy.
		return( true );
	}

	NPC_WalkToPoint( IsStatFlag( STATF_War ) ? true : ( dist > 3 ));
	return( true );
}

bool CChar::NPC_FightArchery( CChar* pChar )
{
	if ( Skill_GetActive() != SKILL_ARCHERY )
		return( false );

	int iDist = GetTopDist3D( pChar );
	if ( iDist > ARCHERY_DIST_MAX )	// way too far away . close in.
		return( false );

	if ( iDist < g_Cfg.m_iArcheryMinDist && ! Calc_GetRandVal( 2 ))
	{
		// Move away
		NPC_Act_Follow( false, 5, true );
		return( true );
	}

	// Fine here.
	return( true );
}

bool CChar::NPC_FightMagery( CChar* pChar )
{
	// cast a spell if i can ?
	// or i can throw or use archery ?
	// RETURN:
	//  false = revert to melee type fighting.

	ASSERT( pChar );
	if ( ! NPC_FightMayCast())
		return( false );

	int iDist = GetTopDist3D( pChar );
	if ( iDist > ((SPHEREMAP_VIEW_SIGHT*3)/4))	// way too far away . close in.
		return( false );

	if ( iDist <= 1 &&
		Skill_GetBase(SKILL_TACTICS) > 200 &&
		! Calc_GetRandVal(2))
	{
		// Within striking distance.
		// Stand and fight for a bit.
		return( false );
	}

	// A creature with a greater amount of mana will have a greater
	// chance of casting
	int iStatInt = m_StatInt;
	int iSkillVal = Skill_GetBase(SKILL_MAGERY);
	int iChance = iSkillVal +
	(( m_StatMana >= ( iStatInt / 2 )) ? m_StatMana : ( iStatInt - m_StatMana ));
	if ( Calc_GetRandVal( iChance ) < 400 )
	{
		// we failed this test, but we could be casting next time
		// back off from the target a bit
		if ( m_StatMana > ( iStatInt / 3 ) && Calc_GetRandVal( iStatInt ))
		{
			if ( iDist < 4 || iDist > 8  )	// Here is fine?
			{
				NPC_Act_Follow( false, Calc_GetRandVal( 3 ) + 2, true );
			}
			return( true );
		}
		return( false );
	}

	// select proper spell.
	// defensive spells ???
	int imaxspell = MIN(( iSkillVal / 12 )* 8, SPELL_BASE_QTY );

	// does the creature have a spellbook.
	CItemPtr pSpellbook = GetSpellbook();

	for ( int i = Calc_GetRandVal( imaxspell ); 1; i++ )
	{
		if ( i >= imaxspell )	// didn't find a spell.
			return( false );

		SPELL_TYPE spell = (SPELL_TYPE) i;
		CSpellDefPtr pSpellDef = g_Cfg.GetSpellDef( spell );
		if ( pSpellDef == NULL )
			continue;

		if ( pSpellbook )
		{
			if ( ! pSpellbook->IsSpellInBook(spell))
				continue;

			if ( ! pSpellDef->IsSpellType( SPELLFLAG_HARM ))
			{
				// Heal, enhance myself, help friends or dispel conjured creatures.
			}
		}
		else
		{
			if ( ! pSpellDef->IsSpellType( SPELLFLAG_HARM ))
				continue;

			// less chance for berserker spells
			if ( pSpellDef->IsSpellType( SPELLFLAG_SUMMON ) && Calc_GetRandVal( 2 ))
				continue;

			// less chance for field spells as well
			if ( pSpellDef->IsSpellType( SPELLFLAG_FIELD ) && Calc_GetRandVal( 4 ))
				continue;
		}

		if ( ! Spell_CanCast( spell, true, this, false ))
			continue;

		m_Act.m_atMagery.m_Spell = spell;
		break;	// I like this spell.
	}

	// KRJ - give us some distance
	// if the opponent is using melee
	// the bigger the disadvantage we have in hitpoints, the further we will go
	if ( m_StatMana > iStatInt / 3 && Calc_GetRandVal( iStatInt << 1 ))
	{
		if ( iDist < 4 || iDist > 8  )	// Here is fine?
		{
			NPC_Act_Follow( false, 5, true );
		}
	}
	else
	{
		NPC_Act_Follow();
	}

	Reveal();

	m_Act.m_Targ = pChar->GetUID();
	m_Act.m_TargPrv = GetUID();	// I'm casting this directly.
	m_Act.m_pt = pChar->GetTopPoint();

	// Calculate the difficulty
	return( Skill_Start( SKILL_MAGERY ));
}

void CChar::NPC_Act_Fight()
{
	// I am in an attack mode.
	DEBUG_CHECK( IsTopLevel());
	ASSERT( m_pNPC.IsValidNewObj());

	if ( ! Fight_IsActive())	// we did something else i guess.
		return;

	// Review our targets periodically.
	if ( ! IsStatFlag(STATF_Pet) ||
		m_pNPC->m_Brain == NPCBRAIN_BESERK )
	{
		int iObservant = ( 130 - m_StatInt) / 20;
		if ( ! Calc_GetRandVal( 2 + MAX( 0, iObservant )))
		{
			if ( NPC_LookAround())
				return;
		}
	}

	CCharPtr pChar = g_World.CharFind(m_Act.m_Targ);
	if ( pChar == NULL || ! pChar->IsTopLevel()) // target is not valid anymore ?
		return;
	int iDist = GetDist( pChar );

	if ( m_pNPC->m_Brain == NPCBRAIN_GUARD &&
		m_Act.m_atFight.m_War_Swing_State == WAR_SWING_READY &&
		! Calc_GetRandVal(3))
	{
		// If a guard is ever too far away (missed a chance to swing)
		// Teleport me closer.
		NPC_LookAtChar( pChar, iDist );
	}

	// If i'm horribly damaged and smart enough to know it.
	int iMotivation = NPC_GetAttackMotivation( pChar );
	if ( ! IsStatFlag(STATF_Pet))
	{
		if ( iMotivation < 0 )
		{
			m_Act.m_atFlee.m_iStepsMax = 20;	// how long should it take to get there.
			m_Act.m_atFlee.m_iStepsCurrent = 0;	// how long has it taken ?
			Skill_Start( NPCACT_FLEE );	// Run away!
			return;
		}
	}

	// Can only do that with full stamina !
	if ( m_StatStam >= m_StatMaxStam )
	{
		// If I am a dragon maybe I will breath fire.
		// NPCACT_BREATH
		if ( m_pNPC->m_Brain == NPCBRAIN_DRAGON &&
			iDist >= 1 &&
			iDist <= 8 &&
			CanSeeLOS( pChar ))
		{
			UpdateDir( pChar );
			Skill_Start( NPCACT_BREATH );
			return;
		}

		// If I am a giant. i can throw stones.
		// NPCACT_THROWING

		if (( GetDispID() == CREID_OGRE ||
			GetDispID() == CREID_ETTIN ||
			GetDispID() == CREID_Cyclops ) &&
			iDist >= 2 &&
			iDist <= 9 &&
			CanSeeLOS( pChar ) &&
			ContentFind( CSphereUID(RES_TypeDef,IT_AROCK), 0, 2 ))
		{
			UpdateDir( pChar );
			Skill_Start( NPCACT_THROWING );
			return;
		}
	}

	// Maybe i'll cast a spell if I can. if so maintain a distance.
	if ( NPC_FightMagery( pChar ))
		return;

	if ( NPC_FightArchery( pChar ))
		return;

	// Move in for melee type combat.
	NPC_Act_Follow();
}

bool CChar::NPC_Act_Talk()
{
	// NPCACT_TALK:
	// NPCACT_TALK_FOLLOW
	// RETURN:
	//  false = do something else. go Idle
	//  true = just keep waiting.

	CCharPtr pChar = g_World.CharFind(m_Act.m_Targ);
	if ( pChar == NULL )	// they are gone ?
		return( false );

	// too far away.
	int iDist = GetTopDist3D( pChar );
	if ( iDist >= SPHEREMAP_VIEW_SIGHT )	// give up.
		return( false );

	if ( Skill_GetActive() == NPCACT_TALK_FOLLOW && iDist > 3 )
	{
		// try to move closer.
		if ( ! NPC_Act_Follow( false, 4, false ))
			return( false );
	}

	if ( m_Act.m_atTalk.m_WaitCount <= 1 )
	{
		if ( NPC_CanSpeak())
		{
			static LPCTSTR const sm_szText[] =
			{
				"Well it was nice speaking to you %s but i must go about my business",
				"Nice speaking to you %s",
			};
			CGString sMsg;
			sMsg.Format( sm_szText[ Calc_GetRandVal( COUNTOF( sm_szText )) ], (LPCTSTR) pChar->GetName() );
			Speak( sMsg );
		}
		return( false );
	}

	m_Act.m_atTalk.m_WaitCount--;
	return( true );	// just keep waiting.
}

void CChar::NPC_Act_GoHome()
{
	// NPCACT_GO_HOME
	// If our home is not valid then

	if ( ! Calc_GetRandVal( 2 ))
	{
		if ( NPC_LookAround())
			return;
	}

	ASSERT( m_pNPC.IsValidNewObj());
	if ( m_pNPC->m_Brain == NPCBRAIN_GUARD )
	{
		// Had to change this guards were still roaming the forests
		// this goes hand in hand with the change that guards arent
		// called if the criminal makes it outside guarded territory.

		CRegionPtr pArea = m_ptHome.GetRegion( REGION_TYPE_AREA );
		if ( pArea.IsValidRefObj() && pArea->IsFlagGuarded())
		{
			if ( ! m_pArea->IsFlagGuarded())
			{
				if ( Spell_Effect_Teleport( m_ptHome, false, false ))
				{
					Skill_Start( SKILL_NONE );
					return;
				}
			}
		}
		else
		{
			g_Log.Event( LOG_GROUP_DEBUG, LOGL_WARN, "Guard 0%lx '%s' has no guard post (%s)!" LOG_CR, GetUID(), (LPCTSTR) GetName(), (LPCTSTR) GetTopPoint().v_Get());

			// If we arent conjured and still got no valid home
			// then set our status to conjured and take our life.
			if ( ! IsStatFlag(STATF_Conjured))
			{
				StatFlag_Set( STATF_Conjured );
				m_StatHealth = -1000;
				return;
			}
		}

		// else we are conjured and probably a timer started already.
	}

	if ( ! m_ptHome.IsValidPoint())
	{
		Skill_Start( SKILL_NONE );
		return;
	}

	m_Act.m_pt = m_ptHome;
	if ( ! NPC_WalkToPoint()) // get there
	{
		Skill_Start( SKILL_NONE );
		return;
	}

	if ( ! Calc_GetRandVal(40)) // give up...
	{
		// Some NPCs should be able to just teleport back home...
		switch( m_pNPC->m_Brain )
		{
		case NPCBRAIN_VENDOR:
		case NPCBRAIN_STABLE:
		case NPCBRAIN_BANKER:
		case NPCBRAIN_HEALER:
		case NPCBRAIN_CRIER:
			// if ( ! Calc_GetRandVal(100)) // give up...
			{
				Spell_Effect_Teleport( m_ptHome, true, false );
			}
		default:
			Skill_Start( SKILL_NONE );
			break;
		}
	}
}

void CChar::NPC_LootMemory( CItem* pItem )
{
	// Create a memory of this item.
	// I have already looked at it.
	CItemMemoryPtr pMemory = Memory_AddObjTypes( pItem, MEMORY_SPEAK );
	ASSERT(pMemory);
	pMemory->m_itEqMemory.m_Arg1 = NPC_MEM_ACT_IGNORE;

	// If the item is set to decay.
	if ( pItem->IsTimerSet() && ! pItem->IsTimerExpired())
	{
		pMemory->SetTimeout( pItem->GetTimerDiff());	// We'll forget about this once the item is gone
	}
}

bool CChar::NPC_LootContainer( CItemContainer* pContainer )
{
	// Go through the pack and see if there is anything I want there...
	CItemPtr pNext;
	CItemPtr pLoot = pContainer->GetHead();
	for ( ; pLoot != NULL; pLoot = pNext )
	{
		pNext = pLoot->GetNext();
		// Have I checked this one already?
		if ( Memory_FindObj( pLoot ))
			continue;

		if ( pLoot->IsContainer())
		{
			// Loot it as well
			if ( ! NPC_LootContainer( REF_CAST(CItemContainer,pLoot)))
			{
				// Not finished with it
				return false;
			}
		}

		if ( ! NPC_WantThisItem( pLoot ))
			continue;

		// How much can I carry
		if ( CanCarry( pLoot ))
		{
			UpdateAnimate( ANIM_BOW );
			ItemEquip( pLoot );
		}
		else
		{
			// Can't carry the whole stack...how much can I carry?
			NPC_LootMemory( pLoot );
		}

		// I can only pick up one thing at a time, so come back here on my next tick
		SetTimeout( 1 );	// Don't go through things so fast.
		return false;
	}

	// I've gone through everything here...remember that we've looted this container
	NPC_LootMemory( pContainer );
	return true;
}

void CChar::NPC_Act_Looting()
{
	// NPCACT_LOOTING
	// We have seen something good that we want. checking it out now.
	// We just killed something, so we should see if it has anything interesting on it.
	// Find the corpse first

	CItemPtr pItem = g_World.ItemFind( m_Act.m_Targ );
	if ( pItem == NULL )
	{
		Skill_Start( SKILL_NONE );
		return;
	}

	ASSERT( m_pNPC.IsValidNewObj());

	CObjBasePtr pObjTop = pItem->GetTopLevelObj();
	ASSERT(pObjTop);

	if ( m_Act.m_TargPrv != pObjTop->GetUID() ||
		m_Act.m_pt != pObjTop->GetTopPoint())
	{
		// It moved ?
	cantgetit:
		// give up on this.
		NPC_LootMemory( pItem );
		Skill_Start( SKILL_NONE );
		return;
	}

	if ( GetDist( pItem ) > 1 )	// move toward it.
	{
		if ( ++(m_Act.m_atLooting.m_iDistCurrent) > m_Act.m_atLooting.m_iDistEstimate )
		{
			goto cantgetit;
		}
		NPC_WalkToPoint();
		return;
	}

	// Have I already searched this one?
	DEBUG_CHECK( ! Memory_FindObj( pItem ));

	if ( pItem->IsTypeLocked())
	{
		goto cantgetit;
	}

	if ( pItem->IsType(IT_CORPSE))
	{
		// Did I kill this one?
		if ( pItem->m_itCorpse.m_uidKiller != GetUID())
		{
			// Wasn't me...less chance of actually looting it.
			if ( Calc_GetRandVal( 4 ))
			{
				goto cantgetit;
			}
		}
	}

	// Can i reach the object that i want ?
	if ( ! CanTouch( pItem ))
	{
		goto cantgetit;
	}

	// I can reach it
	CItemCorpsePtr pCorpse = REF_CAST(CItemCorpse,pItem);
	if ( pCorpse )
	{
		if ( ! NPC_LootContainer( pCorpse ))
			return;

		// Eat raw meat ? or just evil ?
		if ( m_pNPC->m_Brain == NPCBRAIN_MONSTER ||
			m_pNPC->m_Brain == NPCBRAIN_ANIMAL ||
			m_pNPC->m_Brain == NPCBRAIN_DRAGON )
		{
			// Only do this if it has a resource type we want...
			if ( pCorpse->m_itCorpse.m_timeDeath.IsTimeValid() )
			{
				Use_CarveCorpse( pCorpse );
				Skill_Start( NPCACT_LOOKING );
				return;
			}
		}
	}
	else
	{
		if ( ! CanCarry( pItem ))
		{
			goto cantgetit;
		}

		// can i eat it on the ground ?

		UpdateAnimate( ANIM_BOW );
		ItemBounce( pItem );
	}

	// Done looting
	// We might be looting this becuase we are hungry...
	// What was I doing before this?

	Skill_Start( SKILL_NONE );
}

void CChar::NPC_Act_Flee()
{
	// NPCACT_FLEE
	// I should move faster this way.
	// ??? turn to strike if they are close.
	if ( ++ m_Act.m_atFlee.m_iStepsCurrent >= m_Act.m_atFlee.m_iStepsMax )
	{
		Skill_Start( SKILL_NONE );
		return;
	}
	if ( ! NPC_Act_Follow( true, m_Act.m_atFlee.m_iStepsMax ))
	{
		Skill_Start( SKILL_NONE );
		return;
	}
}

void CChar::NPC_Act_Goto()
{
	// NPCACT_GOTO:
	// Still trying to get to this point.

	switch ( NPC_WalkToPoint())
	{
	case 0:
		// We reached our destination
		NPC_Act_Idle();	// look for something new to do.
		break;
	case 1:
		// Took a step....keep trying to get there.
		break;
	case 2:
		// Give it up...
		// Go directly there...
		if ( m_Act.m_pt.IsValidPoint() &&
			IsHuman() &&
			!IsStatFlag( STATF_Freeze|STATF_Stone|STATF_Immobile ))
			Spell_Effect_Teleport( m_Act.m_pt, true, false);
		else
			NPC_Act_Idle();	// look for something new to do.
		break;
	}
}

void CChar::NPC_Act_Idle()
{
	// Free to do as we please. decide what we want to do next.
	// Idle NPC's should try to take some action.

	ASSERT( m_pNPC.IsValidNewObj());
	m_pNPC->m_Act_Motivation = 0;	// we have no motivation to do anything.

	// Look around for things to do.
	if ( NPC_LookAround())
		return;

	// ---------- If we found nothing else to do. do this. -----------

	// If guards are found outside guarded territories, do the following.
	if ( m_pNPC->m_Brain == NPCBRAIN_GUARD &&
		! m_pArea->IsFlagGuarded() &&
		m_ptHome.IsValidPoint())
	{
		Skill_Start( NPCACT_GO_HOME );
		return;
	}

	// Specific creature random actions.
	if ( m_StatStam >= m_StatMaxStam && ! Calc_GetRandVal( 3 ))
	{
		switch ( GetDispID())
		{
		case CREID_FIRE_ELEM:
			if ( ! g_World.IsItemTypeNear( GetTopPoint(), IT_FIRE ))
			{
				Action_StartSpecial( CREID_FIRE_ELEM );
			}
			return;
		case CREID_GIANT_SPIDER:
			if ( ! g_World.IsItemTypeNear( GetTopPoint(), IT_WEB ))
			{
				Action_StartSpecial( CREID_GIANT_SPIDER );
			}
			return;
		}
	}

	if ( m_ptHome.IsValidPoint() && ! Calc_GetRandVal( 20 ))
	{
		// Periodically head home.
		Skill_Start( NPCACT_GO_HOME );
		return;
	}

	if ( Skill_GetBase(SKILL_HIDING) > 30 &&
		! Calc_GetRandVal( 15 - Skill_GetBase(SKILL_HIDING)/100) &&
		! m_pArea->IsFlagGuarded())
	{
		// Just hide here.
		if ( IsStatFlag( STATF_Hidden ))
			return;
		Skill_Start( SKILL_HIDING );
		return;
	}
	if ( Calc_GetRandVal( 100 - m_StatDex ) < 25 )
	{
		// dex determines how jumpy they are.
		// Decide to wander about ?
		Skill_Start( NPCACT_WANDER );
		return;
	}

	// just stand here for a bit.
	Skill_Start( SKILL_NONE );
	SetTimeout( Calc_GetRandVal(3*TICKS_PER_SEC));
}

bool CChar::NPC_OnItemGive( CChar* pCharSrc, CItem* pItem )
{
	// Someone pCharSrc (Player) is giving me an item.
	// RETURN: true = accepted.
	// Act as if we are speaking to each other.

	ASSERT( pCharSrc );

	if ( ! m_pNPC )
		return( false );	// must just be an offline player.

	CSphereExpArgs execArgs( this, pCharSrc, pItem );
	if ( OnTrigger( CCharDef::T_ReceiveItem, execArgs ) == TRIGRET_RET_VAL )
		return( true );

	if ( NPC_IsOwnedBy( pCharSrc ))
	{
		// Giving something to my own pet.
		if ( pCharSrc->IsGM())
		{
			return( ItemEquip( pItem ) == NO_ERROR );
		}

		// Take stuff from my master.
		if ( NPC_IsVendor())
		{
			if ( pItem->IsType(IT_GOLD))
			{
				Speak( "I will keep this money in account." );
				NPC_OnHirePayMore(pItem);
			}
			else
			{
				Speak( "I will attempt to sell this item for you." );
				GetBank(LAYER_VENDOR_STOCK)->ContentAdd( pItem );
			}
			return true;
		}

		// Human hireling
		if ( NPC_CanSpeak())
		{
			if ( Food_CanEat(pItem))
			{
				if ( NPC_WantThisItem( pItem ) &&
					Use_Eat( pItem ))
				{
					Speak( "Mmm. Thank you." );
					return( true );
				}
				else
				{
					Speak( "No thank you." );
					return( false );
				}
			}
			if ( ! CanCarry( pItem ))
			{
				Speak( "I'm too weak to carry that." );
				return( false );
			}
			if ( pItem->IsType(IT_GOLD))
			{
				// Pay me for my work ? (hirelings)
				Speak( "Thank thee." );
				// NPC_OnHirePayMore(pItem);
				// return;
			}

			if ( Use_Item( pItem ))
				return true;

			Speak( "Tell me to drop this when you want it back." );
			GetPackSafe()->ContentAdd( pItem );
			return( true );
		}

		switch ( pItem->GetType())
		{
		case IT_POTION:
		case IT_DRINK:
		case IT_PITCHER:
		case IT_WATER_WASH:
		case IT_BOOZE:
			if ( Use_Item( pItem ))
				return true;
		}
	}

	// Does the NPC know you ?
	CItemMemoryPtr pMemory = Memory_FindObj( pCharSrc );
	if ( pMemory && ( pMemory->GetMemoryTypes() & MEMORY_SPEAK ))
	{
		// Am i being paid for something ?
		if ( pItem->IsType(IT_GOLD))
		{
			if ( pMemory->m_itEqMemory.m_Arg1 == NPC_MEM_ACT_SPEAK_TRAIN )
			{
				return( NPC_OnTrainPay( pCharSrc, pMemory, pItem ));
			}
			if ( pMemory->m_itEqMemory.m_Arg1 == NPC_MEM_ACT_SPEAK_HIRE )
			{
				return( NPC_OnHirePay( pCharSrc, pMemory, pItem ));
			}
		}
	}

	if ( NPC_OnHearFirst(pCharSrc,pMemory))
		return false;

	if ( ! NPC_WantThisItem( pItem ))
	{
		if ( OnTrigger( CCharDef::T_NPCRefuseItem, execArgs ) == TRIGRET_RET_VAL )
			return false;

		if ( pCharSrc->GetClient() )
		{
			pCharSrc->GetClient()->addObjMessage( "They don't appear to want the item", this );
		}
		return( false );
	}

	NPC_ActStart_SpeakTo( pCharSrc );

	if ( pItem->IsType(IT_GOLD))
	{
		if ( m_pNPC->m_Brain == NPCBRAIN_BANKER )
		{
			// I shall put this item in your bank account.
			CGString sMsg;
			sMsg.Format( "I shall deposit %d gold in your account", pItem->GetAmount());
			Speak( sMsg );
			pCharSrc->GetBank()->ContentAdd( pItem );
			return( true );
		}

		if ( NPC_CanSpeak())
		{
			Speak( "Gold is always welcome. thank thee." );
		}

		ItemEquip( pItem );
		pItem->Update();
		return( true );
	}

	if ( NPC_IsVendor())
	{
		if ( pCharSrc->GetClient() )
		{
			CUOEventSell sell;
			sell.m_UID = pItem->GetUID();
			sell.m_amount = pItem->GetAmount();
			return pCharSrc->GetClient()->Event_VendorSell( GetUID(), 1, &sell );
		}
		return( false );
	}

	// The NPC might want it ?
	switch ( m_pNPC->m_Brain )
	{
	case NPCBRAIN_DRAGON:
	case NPCBRAIN_ANIMAL:
	case NPCBRAIN_MONSTER:
		// Might want food ?
		if ( Food_CanEat(pItem))
		{
			// ??? May know it is poisoned ?
			// ??? May not be hungry
			if ( Use_Eat( pItem, pItem->GetAmount() ))
				return( true );
		}
		break;

	case NPCBRAIN_BEGGAR:
	case NPCBRAIN_THIEF:
		if ( Food_CanEat(pItem) &&
			Use_Eat( pItem ))
		{
			Speak( "Mmm thank you. I was very hungry." );
			return( true );
		}
		if ( ! CanCarry( pItem ))
		{
			Speak( "I'm too weak to carry that." );
			return( false );
		}
		if ( pItem->IsType(IT_GOLD) || Food_CanEat(pItem))
			Speak( "Thank thee! Now I can feed my children!" );
		else
			Speak( "I'll sell this for gold! Thank thee!" );
		ItemEquip( pItem );
		pItem->Update();
		if (m_Act.m_Targ == pCharSrc->GetUID())
		{
			Speak( "If only others were as kind as you." );
			m_Act.m_TargPrv = m_Act.m_Targ;
			m_Act.m_Targ.InitUID();
			Skill_Start( SKILL_NONE );
		}
		return( true );
	}

	if ( OnTrigger( CCharDef::T_NPCAcceptItem, execArgs ) == TRIGRET_RET_VAL )
		return true;

	ItemBounce( pItem );
	return( true );
}

bool CChar::NPC_OnTickFood( int nFoodLevel )
{
	// Check Food usage.
	// Are we hungry enough to take some new action ?
	// RETURN: true = we have taken an action.

	return( false );	// This code appears to cause serious problems ! cut it out for now.

#if 0
	if ( IsStatFlag( STATF_Stone | STATF_Freeze | STATF_DEAD | STATF_Sleeping | STATF_Immobile ))
		return false;

	SoundChar( CRESND_RAND2 );

	switch ( Skill_GetActive())
	{
	case NPCACT_EATING:
		return false;	// We are already looking for food;
	case NPCACT_WANDER:
	case NPCACT_TALK:
	case NPCACT_TALK_FOLLOW:
	case NPCACT_GO_HOME:
	case NPCACT_UNUSED:
		// OK to eat now
		break;
	case NPCACT_GOTO:
	case NPCACT_GUARD_TARG:
	case NPCACT_GO_FETCH:
	case NPCACT_FOLLOW_TARG:
	case NPCACT_STAY:
		if (nFoodLevel > 25)
			return false;	// Only if very hungry
		break;
	case NPCACT_FLEE:
	case NPCACT_BREATH:
	case NPCACT_LOOTING:
		if (nFoodLevel > 12)
			return false; // Only if starving...some things are more important
		break;
	case NPCACT_RIDDEN:
		return false;	// I guess I can't do this now....
	default:
		return false ;	// I'm doing something else (perhaps fighting?)
	}

	NPC_Food_Search();
	return( true );
#endif

}

void CChar::NPC_OnTickAction()
{
	// Our action timer has expired.
	// last skill or task might be complete ?
	// What action should we take now ?

	switch ( Skill_GetActive())
	{
	case SKILL_NONE:
		// We should try to do something new.
		NPC_Act_Idle();
		break;

	case SKILL_BEGGING:
		NPC_Act_Begging( NULL );
		break;

	case SKILL_Stealth:
	case SKILL_HIDING:
		// We are currently hidden.
		if ( NPC_LookAround())
			break;
		// just remain hidden unless we find something new to do.
		if ( Calc_GetRandVal( Skill_GetBase(SKILL_HIDING)))
			break;
		NPC_Act_Idle();
		break;

	case SKILL_ARCHERY:
	case SKILL_FENCING:
	case SKILL_MACEFIGHTING:
	case SKILL_SWORDSMANSHIP:
	case SKILL_WRESTLING:
		// If we are fighting . Periodically review our targets.
		NPC_Act_Fight();
		break;

	case NPCACT_FOLLOW_TARG:
	case NPCACT_GUARD_TARG:
		// continue to follow our target.
		NPC_LookAtChar( g_World.CharFind(m_Act.m_Targ), 1 );
		NPC_Act_Follow();
		break;
	case NPCACT_STAY:
		// Just stay here til told to do otherwise.
		break;
	case NPCACT_GOTO:
		NPC_Act_Goto();
		break;
	case NPCACT_WANDER:
		NPC_Act_Wander();
		break;
	case NPCACT_FLEE:
		NPC_Act_Flee();
		break;
	case NPCACT_TALK:
	case NPCACT_TALK_FOLLOW:
		// Got bored just talking to you.
		if ( ! NPC_Act_Talk())
		{
			NPC_Act_Idle();	// look for something new to do.
		}
		break;
	case NPCACT_GO_HOME:
		NPC_Act_GoHome();
		break;
	case NPCACT_EATING:
		NPC_Act_Eating();
		break;
	case NPCACT_LOOTING:
		NPC_Act_Looting();
		break;
	case NPCACT_LOOKING:
		if ( NPC_LookAround( true ))
			return;
		NPC_Act_Idle();
		break;

	default:
		if ( ! IsSkillBase( Skill_GetActive()))	// unassigned skill ? that's weird
		{
			Skill_Start( SKILL_NONE );
		}
		break;
	}

	if ( IsTimerExpired())	// Was not reset ?
	{
		// default next brain/move tick
		SetTimeout( TICKS_PER_SEC/3 + Calc_GetRandVal( (150-m_StatDex)/2 )* TICKS_PER_SEC / 10 );
	}
}

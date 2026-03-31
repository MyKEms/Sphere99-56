//
// CCharSkill.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//  CChar is either an NPC or a Player.
//

#include "stdafx.h"	// predef header.

extern "C"
{
	// for some reason, including the math.h file gave an error, so I'll declare it here.
	double _cdecl log10(double);
}

//----------------------------------------------------------------------

void CChar::Action_StartSpecial( CREID_TYPE id )
{
	// Take the special creature action.
	// lay egg, breath weapon (fire, lightning, acid, code, paralyze),
	//  create web, fire patch, fire ball,
	// steal, teleport, level drain, absorb magic, curse items,
	// rust items, stealing, charge, hiding, grab, regenerate, play dead.
	// Water = put out fire !

	UpdateAnimate( ANIM_CAST_AREA );

	switch ( id )
	{
	case CREID_FIRE_ELEM:
		// leave a fire patch.
		{
			CItemPtr pItem = CItem::CreateScript( Calc_GetRandVal(2) ? ITEMID_FX_FIRE_F_EW : ITEMID_FX_FIRE_F_NS, this );
			ASSERT(pItem);
			pItem->SetType( IT_FIRE );
			pItem->m_itSpell.m_spell = SPELL_Fire_Field;
			pItem->m_itSpell.m_spelllevel = 100 + Calc_GetRandVal(500);
			pItem->m_itSpell.m_spellcharges = 1;
			pItem->m_uidLink = GetUID();	// Link it back to you
			pItem->MoveToDecay( GetTopPoint(), 30*TICKS_PER_SEC + Calc_GetRandVal(60*TICKS_PER_SEC));
		}
		break;

	case CREID_GIANT_SPIDER:
		// Leave a web patch.
		{
			static const WORD sm_Webs[] =
			{
				ITEMID_WEB1_1,
				ITEMID_WEB1_1+1,
				ITEMID_WEB1_1+2,
				ITEMID_WEB1_4,
			};
			CItemPtr pItem = CItem::CreateScript( (ITEMID_TYPE) sm_Webs[ Calc_GetRandVal( COUNTOF(sm_Webs))], this );
			pItem->SetType(IT_WEB);
			pItem->MoveToCheck( GetTopPoint(), this );
			pItem->SetDecayTime( 3*60*60*TICKS_PER_SEC );
		}
		break;

	default:
		WriteString( "You have no special abilities" );
		return;	// No special ability.
	}

	// loss of stamina for a bit.
	Stat_Change( STAT_Stam, -( 5 + Calc_GetRandVal(5)));	// The cost in stam.
}

void CChar::Stat_Set( STAT_TYPE i, STAT_LEVEL iVal )
{
	ASSERT(((WORD)i)<STAT_QTY );

	CCharDefPtr pCharDef = Char_GetDef(); // only do this on first load ?
	ASSERT(pCharDef);

	// Range check first.

	switch ( i )
	{
	case STAT_Str:
		if ( ! pCharDef->m_Str )	// fill this in if not already filled.
			pCharDef->m_Str = iVal;
		goto check_neg;
	case STAT_Dex:
		if ( ! pCharDef->m_Dex )
			pCharDef->m_Dex = iVal;
		goto check_neg;
	case STAT_Int:
	check_neg:
		if ( iVal <= 0 )
		{
			// Setting stats negative might be acceptable on a temporary basis.
			// iVal = 1;
		}
		break;
	case STAT_Karma:		// -10000 to 10000
		break;
	case STAT_Fame:
		if ( iVal < 0 )
		{
			DEBUG_ERR(( "ID=%s,UID=0%x Fame set out of range %d" LOG_CR, (LPCTSTR) GetResourceName(), GetUID(), iVal ));
			iVal = 0;
		}
		break;
	}

	m_Stat[i] = iVal;

	// Now update anyone who may be looking.
	UpdateStatsFlag();
}

STAT_LEVEL CChar::Stat_GetMax( STAT_TYPE i ) const
{
	if ( m_pPlayer )
	{
		CProfessionPtr pProfession = m_pPlayer->GetProfession();
		ASSERT(pProfession);
		if ( i == STAT_QTY )
		{
			return pProfession->m_StatSumMax;
		}
		ASSERT( i<STAT_BASE_QTY );
		return pProfession->m_StatMax[i];
	}
	else
	{
		if ( i == STAT_QTY )
		{
			return 300;
		}
		return 100;	// just can't grow past this.
	}
}

void CChar::Stat_Change( STAT_TYPE type, int iChange, int iLimit )
{
	// type = STAT_Health, STAT_Stam, and STAT_Mana

	ASSERT( type >= STAT_Health && type <= STAT_Stam );
	int iVal = m_Stat[type];
	STAT_TYPE typeLimit = (STAT_TYPE)( STAT_MaxHealth + ( type-STAT_Health )); 

	if ( iChange )
	{
		if ( ! iLimit )
		{
			iLimit = Stat_Get(typeLimit);
		}
		if ( iChange < 0 )
		{
			iVal += iChange;
		}
		else if ( iVal > iLimit )
		{
			iVal -= iChange;
			if ( iVal < iLimit ) iVal = iLimit;
		}
		else
		{
			iVal += iChange;
			if ( iVal > iLimit ) iVal = iLimit;
		}
		if ( iVal < 0 ) iVal = 0;

		// Stat_Set()
		m_Stat[type] = iVal;
	}

	iLimit = Stat_Get(typeLimit);
	if ( iLimit < 0 )
		iLimit = 0;

	CUOCommand cmd;
	cmd.StatChng.m_Cmd = XCMD_StatChngStr + ( type - STAT_Health );
	cmd.StatChng.m_UID = GetUID();

	cmd.StatChng.m_max = iLimit;
	cmd.StatChng.m_val = iVal;

	if ( IsClient())	// send this just to me
	{
		m_pClient->xSendPkt( &cmd, sizeof(cmd.StatChng));
	}

	if ( type == STAT_Health )	// everyone sees my health
	{
		cmd.StatChng.m_max = 100;
		cmd.StatChng.m_val = GetHealthPercent();

		UpdateCanSee( &cmd, sizeof(cmd.StatChng), m_pClient );
	}
}

//----------------------------------------------------------------------
// Skills

SKILL_TYPE CChar::Skill_GetBest( int iRank ) const // Which skill is the highest for character p
{
	// Get the top n best skills.
	// iRank=0 to x

	if ( iRank < SKILL_First || iRank >= SKILL_QTY )
		iRank = SKILL_First;

	CGTypedArray<DWORD,DWORD> adwSkills;
	adwSkills.SetCount(iRank+1);

	DWORD* pdwSkills = adwSkills.GetBasePtr();
	memset( pdwSkills, 0, (iRank+1)*sizeof(DWORD));

	DWORD dwSkillTmp;
	for ( int i=SKILL_First;i<SKILL_QTY;i++)
	{
		dwSkillTmp = MAKEDWORD( i, Skill_GetBase( (SKILL_TYPE)i ));
		for ( int j=0; j<=iRank; j++ )
		{
			if ( dwSkillTmp > pdwSkills[j] )
			{
				memmove( pdwSkills+j+1, pdwSkills+j, iRank-j );
				pdwSkills[j] = dwSkillTmp;
				break;
			}
		}
	}

	dwSkillTmp = pdwSkills[ iRank ];
	return( (SKILL_TYPE) LOWORD( dwSkillTmp ));
}

STAT_LEVEL CChar::Skill_GetAdjusted( SKILL_TYPE skill ) const
{
	// Get the skill adjusted for str,dex,int = 0-1000

	// m_SkillStat is used to figure out how much
	// of the total bonus comes from the stats
	// so if it's 80, then 20% (100% - 80%) comes from
	// the stat (str,int,dex) bonuses

	// example:

	// These are the cchar's stats:
	// m_Skill[x] = 50.0
	// m_Stat[str] = 50, m_Stat[int] = 30, m_Stat[dex] = 20

	// these are the skill "defs":
	// m_SkillStat = 80
	// m_StatBonus[str] = 50
	// m_StatBonus[int] = 50
	// m_StatBonus[dex] = 0

	// Pure bonus is:
	// 50% of str (25) + 50% of int (15) = 40

	// Percent of pure bonus to apply to raw skill is
	// 20% = 100% - m_SkillStat = 100 - 80

	// adjusted bonus is: 8 (40 * 0.2)

	// so the effective skill is 50 (the raw) + 8 (the bonus)
	// which is 58 in total.

	ASSERT( IsSkillBase( skill ));
	CSkillDefPtr pSkillDef = g_Cfg.GetSkillDef( skill );
	ASSERT(pSkillDef);
	int iPureBonus =
	( pSkillDef->m_StatBonus[STAT_Str] * m_Stat[STAT_Str] ) +
	( pSkillDef->m_StatBonus[STAT_Int] * m_Stat[STAT_Int] ) +
	( pSkillDef->m_StatBonus[STAT_Dex] * m_Stat[STAT_Dex] );

	int iAdjSkill = IMULDIV( pSkillDef->m_StatPercent, iPureBonus, 10000 );

	switch( (SKILL_TYPE) skill )
	{
	case SKILL_PROVOCATION:
	case SKILL_ENTICEMENT:
	case SKILL_PEACEMAKING:
		{
			double sMusicianshipPercent = (double)Skill_GetBase( SKILL_MUSICIANSHIP ) / 1000.0;
			return( (STAT_LEVEL) (Skill_GetBase( (SKILL_TYPE) skill ) * sMusicianshipPercent + iAdjSkill ));
		}
	};

	return( Skill_GetBase( (SKILL_TYPE) skill ) + iAdjSkill );
}

void CChar::Skill_SetBase( SKILL_TYPE skill, STAT_LEVEL wValue )
{
	ASSERT( IsSkillBase(skill));
	if ( wValue < 0 ) wValue = 0;
	m_Skill[skill] = wValue;
	if ( IsClient())
	{
		// Update the skills list
		m_pClient->addSkillWindow( this, skill, false );
	}
}

int CChar::Skill_GetMax( SKILL_TYPE skill ) const
{
	// What is my max potential in this skill ?
	if ( m_pPlayer )
	{
		CProfessionPtr pProfession = m_pPlayer->GetProfession();
		ASSERT(pProfession);

		if ( skill == SKILL_QTY )
		{
			return pProfession->m_SkillSumMax;
		}

		ASSERT( IsSkillBase(skill) );

		int iSkillMax = pProfession->m_SkillLevelMax[skill];
		if ( m_pPlayer->Skill_GetLock(skill) >= SKILLLOCK_DOWN )
		{
			int iSkillLevel = Skill_GetBase(skill);
			if ( iSkillLevel < iSkillMax )
				iSkillMax = iSkillLevel;
		}

		return( iSkillMax );
	}
	else
	{
		if ( skill == SKILL_QTY )
		{
			return 500 * SKILL_QTY;
		}
		return 1000;
	}
}

bool CChar::Skill_Degrade( SKILL_TYPE skillused )
{
	// Degrade skills that are over the cap !
	// RETURN:
	//  false = give no credit for ne skil use.
	//  true = give credit. ok

	if ( ! m_pPlayer )
		return( true );

	int iSkillSum = 0;
	int iSkillSumMax = Skill_GetMax( SKILL_QTY );

	int i;
	for ( i=SKILL_First; i<SKILL_QTY; i++ )
	{
		iSkillSum += Skill_GetBase( (SKILL_TYPE) i );
	}

	// Check for stats degrade first !

	if ( iSkillSum < iSkillSumMax )
		return( true );

	// degrade a random skill
	// NOTE: Take skills over the class cap first ???

	int skillrand = Calc_GetRandVal(SKILL_QTY);
	for ( i=SKILL_First; true; i++ )
	{
		if ( i >= SKILL_QTY )
		{
			// If we cannot decrease a skill then give no more credit !
			return( false );
		}
		if ( skillrand >= SKILL_QTY )
			skillrand = 0;
		if ( skillrand == skillused )	// never degrade the skill i just used !
			continue;
		if ( m_pPlayer->Skill_GetLock( (SKILL_TYPE) skillrand ) != SKILLLOCK_DOWN )
			continue;
		int iSkillLevel = Skill_GetBase( (SKILL_TYPE) skillrand );
		if ( ! iSkillLevel )
			continue;

		// reduce the skill.
		Skill_SetBase( (SKILL_TYPE) skillrand, iSkillLevel - 1 );
		return( true );
	}
}

void CChar::Skill_Experience( SKILL_TYPE skill, int difficulty )
{
	// Give the char credit for using the skill.
	// More credit for the more difficult. or none if too easy
	//
	// ARGS:
	//  difficulty = skill target from 0-100
	//

	if ( ! IsSkillBase(skill))
		return;
	if ( m_pArea.IsValidRefObj() &&
		m_pArea->IsFlag( REGION_FLAG_SAFE ))	// skills don't advance in safe areas.
		return;

	difficulty *= 10;

	int iSkillLevel = Skill_GetBase( skill );
	if ( difficulty < 0 )
	{
		// failure. Give a little experience for failure at low levels.
		if ( iSkillLevel < 300 )
		{
			difficulty = (( MIN( -difficulty, iSkillLevel )) / 2 ) - 8;
		}
		else
		{
			difficulty = 0;
		}
	}
	if ( difficulty > 1000 )
		difficulty = 1000;

	if ( ! Skill_Degrade(skill))
	{
		// If we cannot decrease a skill then give no more credit !
		difficulty = 0;
	}

	CSkillDefPtr pSkillDef = g_Cfg.GetSkillDef(skill);
	ASSERT(pSkillDef);

	int iSkillMax = Skill_GetMax(skill);	// max advance for this skill.

	if ( iSkillLevel < iSkillMax && difficulty ) // Are we in position to gain skill ?
	{
		// ex. ADV_RATE=2000,500,25 for ANATOMY (easy)
		// ex. ADV_RATE=8000,2000,100 for alchemy (hard)
		// assume 100 = a 1 for 1 gain.
		// ex: 8000 = we must use it 80 times to gain .1
		// Higher the number = the less probable to advance.
		// Extrapolate a place in the range.

		// give a bonus or a penalty if the task was too hard or too easy.
		int iSkillAdj = iSkillLevel + ( iSkillLevel - difficulty );

		int iChance = pSkillDef->m_AdvRate.GetChancePercent( iSkillAdj );

		// Altiara Mod
		// If the difficulty was significantly lower than our skill,
		// we shouldn't gain anything at all.
		if ( (difficulty + 500) < iSkillLevel )
			iChance = 0;
		else if ( ( difficulty + 400 ) < iSkillLevel )
			// still a chance, but only half as much
			iChance /= 2;

		if ( iChance <= 0 )
			return; // less than no chance ?

		int iRoll = Calc_GetRandVal(1000);

#ifdef _DEBUG
		if ( IsPrivFlag( PRIV_DETAIL ) &&
			GetPrivLevel() >= PLEVEL_GM &&
			( g_Cfg.m_wDebugFlags & DEBUGF_ADVANCE_STATS ))
		{
			Printf( "%s=%d.%d Difficult=%d Gain Chance=%d.%d%% Roll=%d%%",
				(LPCTSTR) pSkillDef->GetSkillKey(),
				iSkillLevel/10, iSkillLevel%10,
				difficulty/10, iChance/10, iChance%10, iRoll/10 );
		}
#endif

		if ( iRoll <= iChance )
		{
			Skill_SetBase( skill, iSkillLevel + 1 );
		}
	}

	////////////////////////////////////////////////////////
	// Dish out any stat gains - even for failures.

	int iStatSum = 0;

	// Stat effects are unrelated to advance in skill !
	for ( int i=STAT_Str; i<STAT_BASE_QTY; i++ )
	{
		// Can't gain STR or DEX if morphed.
		if ( IsStatFlag( STATF_Polymorph ) && i != STAT_Int )
			continue;

		int iStatVal = Stat_Get((STAT_TYPE)i);
		if ( iStatVal <= 0 )	// some odd condition
			continue;
		iStatSum += iStatVal;

		int iStatMax = Stat_GetMax((STAT_TYPE)i);
		if ( iStatVal >= iStatMax )
			continue;	// nothing grows past this. (even for NPC's)

		// You will tend toward these stat vals if you use this skill a lot.
		int iStatTarg = pSkillDef->m_Stat[i];
		if ( iStatVal >= iStatTarg )
			continue;		// you've got higher stats than this skill is good for.

		// ??? Building stats should consume food !!

		difficulty = IMULDIV( iStatVal, 1000, iStatTarg );
		int iChance = g_Cfg.m_StatAdv[i].GetChancePercent( difficulty );

		// adjust the chance by the percent of this that the skill uses.
		iChance = ( iChance * pSkillDef->m_StatBonus[i] * pSkillDef->m_StatPercent ) / 10000;

		int iRoll = Calc_GetRandVal(1000);

#ifdef _DEBUG
		if ( IsPrivFlag( PRIV_DETAIL ) &&
			GetPrivLevel() >= PLEVEL_GM &&
			( g_Cfg.m_wDebugFlags & DEBUGF_ADVANCE_STATS ))
		{
			Printf( "%s Difficult=%d Gain Chance=%d.%d%% Roll=%d%%",
				(LPCTSTR) g_Stat_Name[i], difficulty, iChance/10, iChance%10, iRoll/10 );
		}
#endif

		if ( iRoll <= iChance )
		{
			Stat_Set( (STAT_TYPE)i, iStatVal+1 );
			Stat_Set( (STAT_TYPE)(i + STAT_MaxHealth), Stat_Get((STAT_TYPE)(i + STAT_MaxHealth)) + 1);		// We weren't adding on to the associated attributes.  :-p
			break;
		}
	}

	// Check for stats degrade.
	int iStatSumAvg = Stat_GetMax( STAT_QTY );

	if ( m_pPlayer &&
		iStatSum > iStatSumAvg &&
		! IsStatFlag( STATF_Polymorph ) &&
		! IsGM())
	{
		// We are at a point where our skills can degrade a bit.
		// In theory magical enhancements make us lazy !

		int iStatSumMax = iStatSumAvg + iStatSumAvg/4;
		if ( iStatSum > iStatSumMax )
		{
			DEBUG_MSG(( "0%x '%s' Exceeds stat max %d" LOG_CR, GetUID(), (LPCTSTR) GetName(), iStatSum ));
		}

		int iChanceForLoss = Calc_GetSCurve( iStatSumMax - iStatSum, ( iStatSumMax - iStatSumAvg ) / 4 );
		int iRoll = Calc_GetRandVal(1000);

#ifdef _DEBUG
		if ( IsPrivFlag( PRIV_DETAIL ) &&
			GetPrivLevel() >= PLEVEL_GM &&
			( g_Cfg.m_wDebugFlags & DEBUGF_ADVANCE_STATS ))
		{
			Printf( "Loss Diff=%d Chance=%d.%d%% Roll=%d%%",
				iStatSumMax - iStatSum,
				iChanceForLoss/10, iChanceForLoss%10, iRoll/10 );
		}
#endif

		if ( iRoll < iChanceForLoss )
		{
			// Find the stat that was used least recently and degrade it.
			int imin = STAT_Str;
			int iminval = INT_MAX;
			for ( int i=STAT_Str; i<STAT_BASE_QTY; i++ )
			{
				if ( iminval > pSkillDef->m_StatBonus[i] )
				{
					imin = i;
					iminval = pSkillDef->m_StatBonus[i];
				}
			}

			int iStatVal = m_Stat[imin];
			if ( iStatVal > 10 )
			{
				Stat_Set( (STAT_TYPE)imin, iStatVal-1 );
			}
		}
	}
}

bool CChar::Skill_CheckSuccess( SKILL_TYPE skill, int difficulty )
{
	// PURPOSE:
	//  Check a skill for success or fail.
	//  DO NOT give experience here.
	// ARGS:
	//  difficulty = 0-100 = The point at which the equiv skill level has a 50% chance of success.
	// RETURN:
	//	true = success in skill.
	//

#if 0
// WESTY MOD
	// In the following, the return vlaue from the script is the success or fail,
	// not weather to override default processing or not

	// RES_Skill
	CSkillDefPtr pSkillDef = g_Cfg.GetSkillDef(skill);
	TRIGRET_TYPE iTrigRet = TRIGRET_RET_VAL;

	CSphereExpContext exec(this,this);

	if ( ! IsSkillBase(skill) || IsGM())
	{
		if( pSkillDef )
		{
			// Run the script and use the return value for our succeess or failure
			// RES_Skill

			CCharActState SaveState = m_Act;
			iTrigRet = pSkillDef->OnTriggerScript( exec, CSkillDef::T_CHECKSUCCESS, CSkillDef::sm_Triggers[CSkillDef::T_CHECKSUCCESS][0] );
			m_Act = SaveState;

			// GM's ALWAYS succeed, no matter what
			if ( IsGM())
				return( true );
			if ( pSkillDef->HasTrigger( CSkillDef::T_CHECKSUCCESS ))
			{
				if( iTrigRet == TRIGRET_RET_FALSE )
					return( false );
			}
		}
		return( true );
	}

	// RES_Skill

	if ( pSkillDef )
	{
		CCharActState SaveState = m_Act;
		iTrigRet = pSkillDef->OnTriggerScript( exec, CSkillDef::T_CHECKSUCCESS, CSkillDef::sm_Triggers[CSkillDef::T_CHECKSUCCESS][0] );
		m_Act = SaveState;
	}

	if( pSkillDef )
	{
		// return whatever the trigger returned
		if( iTrigRet == TRIGRET_RET_VAL )
			return( true );
		if( pSkillDef->HasTrigger( CSkillDef::T_CHECKSUCCESS ) && iTrigRet == TRIGRET_RET_FALSE )
			return( false );
		// fall through for default (end of script, no return???)
	}
// WESTY MOD
#endif

	// Either no script, or TRIGRET_RET_DEFAULT was returned, procede with default
	bool fResult = g_Cfg.Calc_SkillCheck( Skill_GetAdjusted(skill), difficulty );
	if( fResult )
	{
		return( true );
	}
	return false;
}

bool CChar::Skill_UseQuick( SKILL_TYPE skill, int difficulty )
{
	// ARGS:
	//  difficulty = 0-100
	// Use a skill instantly. No wait at all.
	// No interference with other skills.
	if ( ! Skill_CheckSuccess( skill, difficulty ))
	{
		Skill_Experience( skill, -difficulty );
		return( false );
	}
	Skill_Experience( skill, difficulty );
	return( true );
}

void CChar::Skill_Cleanup( void )
{
	// We are done with the skill.
	// We may have succeeded, failed, or cancelled.
	m_Act.m_Difficulty = 0;
	m_Act.m_SkillCurrent = SKILL_NONE;
	SetTimeout( m_pPlayer.IsValidNewObj() ? -1 : TICKS_PER_SEC ); // we should get a brain tick next time.
}

LPCTSTR CChar::Skill_GetName( bool fUse ) const
{
	// Name the current skill we are doing.

	SKILL_TYPE skill = Skill_GetActive();
	if ( skill <= SKILL_NONE )
	{
		return( "Idling" );
	}
	if ( IsSkillBase(skill))
	{
		if ( ! fUse )
		{
			return( g_Cfg.GetSkillKey(skill));
		}

		TCHAR* pszText = Str_GetTemp();
		sprintf( pszText, _TEXT("use %s"), g_Cfg.GetSkillKey(skill));
		return( pszText );
	}

	switch ( skill )
	{
	case NPCACT_FOLLOW_TARG: return( "Following" );
	case NPCACT_STAY: return( "Staying" );
	case NPCACT_GOTO: return( "GoingTo" );
	case NPCACT_WANDER: return( "Wandering" );
	case NPCACT_FLEE: return( "Fleeing" );
	case NPCACT_TALK: return( "Talking" );
	case NPCACT_TALK_FOLLOW: return( "TalkFollow" );
	case NPCACT_GUARD_TARG: return( "Guarding" );
	case NPCACT_GO_HOME: return( "GoingHome" );
	case NPCACT_GO_FETCH: return( "Fetching" );
	case NPCACT_BREATH: return( "Breathing" );
	case NPCACT_EATING:	return( "Eating" );
	case NPCACT_LOOTING: return( "Looting" );
	case NPCACT_THROWING: return( "Throwing" );
	case NPCACT_LOOKING: return( "Looking" );
	case NPCACT_TRAINING: return( "Training" );
	case NPCACT_Napping: return( "Napping" );
	case NPCACT_UNConscious: return( "Unconscious" );
	case NPCACT_Sleeping: return( "Sleeping" );
	case NPCACT_Healing: return( "Healing" );
	case NPCACT_ScriptBook: return( "Marching" );
	}

	return( "Thinking" );
}

void CChar::Skill_SetTimeout()
{
	SKILL_TYPE skill = Skill_GetActive();
	ASSERT( IsSkillBase(skill));
	int iSkillLevel = Skill_GetBase(skill);
	int iDelay = g_Cfg.GetSkillDef(skill)->m_Delay.GetLinear( iSkillLevel );
	SetTimeout(iDelay);
}

bool CChar::Skill_MakeItem_Success( int iQty )
{
	// deliver the goods.

	if ( iQty <= 0 )
		return true;

	CItemVendablePtr pItem = REF_CAST(CItemVendable,CItem::CreateTemplate( m_Act.m_atCreate.m_ItemID, NULL, this ));
	if ( pItem == NULL )
		return( false );

	CGString sMakeMsg;
	int iSkillLevel = Skill_GetBase( Skill_GetActive());	// primary skill value.

	if ( iQty != 1 )	// m_Act.m_atCreate.m_Amount
	{
		// Some item with the REPLICATE flag ?
		pItem->SetAmount( m_Act.m_atCreate.m_Amount ); // Set the quantity if we are making bolts, arrows or shafts
	}
	else if ( pItem->IsType( IT_SCROLL ))
	{
		// scrolls have the skill level of the inscriber ?
		pItem->m_itSpell.m_spelllevel = iSkillLevel;
	}
	else if ( pItem->IsType( IT_POTION ))
	{
		// Create the potion, set various properties,
		// put in pack
		Emote( "pour the completed potion into a bottle" );
		Sound( 0x240 );	// pouring noise.
	}
	else
	{
		// Only set the quality on single items.
		int quality = IMULDIV( iSkillLevel, 2, 10 );	// default value for quality.
		// Quality depends on the skill of the craftsman, and a random chance.
		// minimum quality is 1, maximum quality is 200.  100 is average.
		// How much variance?  This is the difference in quality levels from
		// what I can normally make.
		int variance = 2 - (int) log10( Calc_GetRandVal( 250 ) + 1); // this should result in a value between 0 and 2.
		// Determine if lower or higher quality
		if ( Calc_GetRandVal( 2 ))
		{
			// Better than I can normally make
		}
		else
		{
			// Worse than I can normally make
			variance = -(variance);
		}
		// The quality levels are as follows:
		// 1 - 25 Shoddy
		// 26 - 50 Poor
		// 51 - 75 Below Average
		// 76 - 125 Average
		// 125 - 150 Above Average
		// 151 - 175 Excellent
		// 175 - 200 Superior
		// Determine which range I'm in
		int qualityBase;
		if ( quality < 25 )
			qualityBase = 0;
		else if ( quality < 50 )
			qualityBase = 1;
		else if ( quality < 75 )
			qualityBase = 2;
		else if ( quality < 125 )
			qualityBase = 3;
		else if ( quality < 150 )
			qualityBase = 4;
		else if ( quality < 175 )
			qualityBase = 5;
		else
			qualityBase = 6;
		qualityBase += variance;
		if ( qualityBase < 0 )
			qualityBase = 0;
		if ( qualityBase > 6 )
			qualityBase = 6;

		switch ( qualityBase )
		{
		case 0:
			// Shoddy quality
			sMakeMsg.Format("Due to your poor skill, the item is of shoddy quality");
			quality = Calc_GetRandVal( 25 ) + 1;
			break;
		case 1:
			// Poor quality
			sMakeMsg.Format("You were barely able to make this item.  It is of poor quality");
			quality = Calc_GetRandVal( 25 ) + 26;
			break;
		case 2:
			// Below average quality
			sMakeMsg.Format("You make the item, but it is of below average quality");
			quality = Calc_GetRandVal( 25 ) + 51;
			break;
		case 3:
			// Average quality
			quality = Calc_GetRandVal( 50 ) + 76;
			break;
		case 4:
			// Above average quality
			sMakeMsg.Format("The item is of above average quality");
			quality = Calc_GetRandVal( 25 ) + 126;
			break;
		case 5:
			// Excellent quality
			sMakeMsg.Format("The item is of excellent quality");
			quality = Calc_GetRandVal( 25 ) + 151;
			break;
		case 6:
			// Superior quality
			sMakeMsg.Format("Due to your exceptional skill, the item is of superior quality");
			quality = Calc_GetRandVal( 25 ) + 176;
			break;
		default:
			// How'd we get here?
			quality = 1000;
			break;
		}
		pItem->SetQuality(quality);
		if ( iSkillLevel > 999 && ( quality > 175 ))
		{
			// A GM made this, and it is of high quality
			CGString csNewName;
			csNewName.Format( "%s crafted by %s", (LPCTSTR) pItem->GetName(), (LPCTSTR) GetName());
			pItem->SetName(csNewName);
		}
	}

	pItem->SetAttr(ATTR_MOVE_ALWAYS | ATTR_CAN_DECAY);	// Any made item is movable.

	CSphereExpArgs execArgs( this, this, Skill_GetActive(), 0, (CResourceObj*)(CItem*)pItem );
	if ( OnTrigger( "@SkillMakeItem", execArgs ) == TRIGRET_RET_VAL )
	{
		pItem->DeleteThis();
		return( false );
	}

	if ( !sMakeMsg.IsEmpty() )
		WriteString( sMakeMsg );

	ItemBounce(pItem);
	return( true );
}

bool CChar::Skill_MakeItem( ITEMID_TYPE id, CSphereUID uidTarg, CSkillDef::T_TYPE_ stage )
{
	// "MAKEITEM"
	//
	// SKILL_ALCHEMY
	// SKILL_BLACKSMITHING
	// SKILL_BOWCRAFT
	// SKILL_CARPENTRY
	// SKILL_INSCRIPTION
	// SKILL_TAILORING:
	// SKILL_TINKERING,
	//
	// Confer the new item.
	// Test for consumable items.
	// on CSkillDef::T_Fail do a partial consume of the resources.
	//
	// ARGS:
	//  uidTarg = item targetted to try to make this . (this item should be used to make somehow)
	//  skill = Skill_GetActive()
	//
	// RETURN:
	//   true = success.
	//

	if ( id <= ITEMID_NOTHING )
		return( true );

	CItemDefPtr pItemDef = g_Cfg.FindItemDef( id );
	if ( pItemDef == NULL )
		return( false );

	// Trigger Target item for creating the new item.
	CItemPtr pItemTarg = g_World.ItemFind(uidTarg);
	if ( pItemTarg && stage == CSkillDef::T_Select )
	{
		if ( pItemDef->m_SkillMake.FindResourceMatch( pItemTarg ) < 0 &&
			pItemDef->m_BaseResources.FindResourceMatch( pItemTarg ) < 0 )
		{
			// Not intersect with the specified item
			return( false );
		}
	}

	int iReplicationQty = 1;
	if ( pItemDef->Can( CAN_I_REPLICATE ))
	{
		// For arrows/bolts, how many do they want ?
		// Set the quantity that they want to make.
		if ( pItemTarg != NULL )
		{
			iReplicationQty = pItemTarg->GetAmount();
		}
	}

	// Test the hypothetical required skills and tools
	if ( ! pItemDef->m_SkillMake.IsResourceMatchAll( this ))
	{
		if ( stage == CSkillDef::T_Start )
		{
			WriteString( "You cannot make this" );
		}
		return( false );
	}

	// test or consume the needed resources.
	if ( stage == CSkillDef::T_Fail )
	{
		// If fail only consume part of them.
		ResourceConsumePart( &(pItemDef->m_BaseResources), iReplicationQty, Calc_GetRandVal( 50 ));
		return( false );
	}

	// How many do i actually have resource for?
	iReplicationQty = ResourceConsume( &(pItemDef->m_BaseResources), iReplicationQty, stage != CSkillDef::T_Success );
	if ( ! iReplicationQty )
	{
		if ( stage == CSkillDef::T_Start )
		{
			WriteString( "You lack the resources to make this" );
		}
		return( false );
	}

	if ( stage == CSkillDef::T_Start )
	{
		// Start the skill.
		// Find the primary skill required.

		int i = pItemDef->m_SkillMake.FindResourceType( RES_Skill );
		if ( i < 0 )
		{
			// Weird.
			if ( stage == CSkillDef::T_Start )
			{
				WriteString( "You cannot figure this out" );
			}
			return( false );
		}

		CResourceQty RetMainSkill = pItemDef->m_SkillMake[i];

		m_Act.m_Targ = uidTarg;	// targetted item to start the make process.
		m_Act.m_atCreate.m_ItemID = id;
		m_Act.m_atCreate.m_Amount = iReplicationQty;

		return Skill_Start( (SKILL_TYPE) RetMainSkill.GetResIndex(), RetMainSkill.GetResQty() / 10 );
	}

	if ( stage == CSkillDef::T_Success )
	{
		return( Skill_MakeItem_Success(iReplicationQty));
	}

	return( true );
}

int CChar::Skill_NaturalResource_Setup( CItem* pResBit )
{
	// RETURN: skill difficulty
	//  0-100
	ASSERT(pResBit);

	// Find the ore type located here based on color.
	CResourceDefPtr pResDef = g_Cfg.ResourceGetDef(pResBit->m_itResource.m_rid_res);
	const CRegionResourceDef* pOreDef = REF_CAST(const CRegionResourceDef,pResDef);
	if ( pOreDef == NULL )
	{
		return( -1 );
	}

	return( pOreDef->m_Skill.GetRandom() / 10 );
}

CItemPtr CChar::Skill_NaturalResource_Create( CItem* pResBit, SKILL_TYPE skill )
{
	// Create some natural resource item.
	// skill = Effects qty of items returned.
	// SKILL_MINING
	// SKILL_FISHING
	// SKILL_LUMBERJACKING

	ASSERT(pResBit);

	// Find the ore type located here based on color.
	CResourceDefPtr pResDef = g_Cfg.ResourceGetDef(pResBit->m_itResource.m_rid_res);
	const CRegionResourceDef* pOreDef = REF_CAST(const CRegionResourceDef,pResDef);
	if ( pOreDef == NULL )
	{
		return( NULL );
	}

	// Skill effects how much of the ore i can get all at once.

	ITEMID_TYPE id = (ITEMID_TYPE) RES_GET_INDEX( pOreDef->m_ReapItem );
	if ( id == ITEMID_NOTHING )
	{
		// I intended for there to be nothing here.
		return( NULL );
	}

	// Reap amount is semi-random
	int iAmount = pOreDef->m_ReapAmount.GetLinear( Skill_GetBase(skill));
	iAmount += pOreDef->m_ReapAmount.GetRandom();
	iAmount /= 2;
	if ( ! iAmount)
	{
		iAmount = 1;
	}

	CItemPtr pItem = CItem::CreateScript( id, this );
	ASSERT(pItem);

	iAmount = pResBit->ConsumeAmount( iAmount );	// amount i used up.
	if ( iAmount <= 0 )
	{
		pItem->DeleteThis();
		return( NULL );
	}

	pItem->SetAmount( iAmount );
	return( pItem );
}

bool CChar::Skill_Mining_Smelt( CItem* pItemOre, CItem* pItemTarg )
{
	// SKILL_MINING
	// pItemTarg = forge or another pile of ore.
	// RETURN: true = success.
	if ( pItemOre == NULL || pItemOre == pItemTarg )
	{
		WriteString( "You need to target the ore you want to smelt" );
		return( false );
	}

	// The ore is on the ground
	if ( ! CanUse( pItemOre, true ))
	{
		Printf( "You can't use the %s where it is.", (LPCTSTR) pItemOre->GetName() );
		return( false );
	}

	if ( pItemOre->IsType( IT_ORE ) &&
		pItemTarg != NULL &&
		pItemTarg->IsType( IT_ORE ))
	{
		// combine piles.
		if ( pItemTarg == pItemOre )
			return( false );
		if ( pItemTarg->GetID() != pItemOre->GetID())
			return( false );
		pItemTarg->SetAmountUpdate( pItemOre->GetAmount() + pItemTarg->GetAmount());
		pItemOre->DeleteThis();
		return( true );
	}

#define SKILL_SMELT_FORGE_DIST 3

	if ( pItemTarg != NULL && pItemTarg->IsTopLevel() &&
		pItemTarg->IsType( IT_FORGE ))
	{
		m_Act.m_pt = pItemTarg->GetTopPoint();
	}
	else
	{
		m_Act.m_pt = g_World.FindItemTypeNearby( GetTopPoint(), IT_FORGE, SKILL_SMELT_FORGE_DIST );
	}

	if ( ! m_Act.m_pt.IsValidPoint() || ! CanTouch(m_Act.m_pt))
	{
		WriteString( "You must be near a forge to smelt" );
		return( false );
	}

	CItemDefPtr pOreDef = pItemOre->Item_GetDef();
	if ( pOreDef->IsType( IT_INGOT ))
	{
		WriteString( "Use a smith hammer to make items from ingots." );
		return false;
	}

	// Fire effect ?
	CItemPtr pItemEffect = CItem::CreateBase(ITEMID_FIRE);
	ASSERT(pItemEffect);
	CPointMap pt = m_Act.m_pt;
	pt.m_z += 8;	// on top of the forge.
	pItemEffect->SetAttr( ATTR_MOVE_NEVER );
	pItemEffect->MoveToDecay( pt, TICKS_PER_SEC );
	Sound( 0x2b );

	UpdateDir( m_Act.m_pt );
	if ( pItemOre->IsAttr(ATTR_MAGIC|ATTR_BLESSED|ATTR_BLESSED2|ATTR_MOVE_ALWAYS))	// not magic items
	{
		WriteString( "The fire is not hot enough to melt this." );
		return false;
	}

	CGString sEmote;
	sEmote.Format( "smelt %s", (LPCTSTR) pItemOre->GetName());
	Emote(sEmote);

	int iMiningSkill = Skill_GetAdjusted(SKILL_MINING);
	int iOreQty = pItemOre->GetAmount();
	CItemDefPtr pIngotDef;
	int iIngotQty = 0;

	if ( pOreDef->IsType( IT_ORE ))
	{
		ITEMID_TYPE idIngot = (ITEMID_TYPE) RES_GET_INDEX( pOreDef->m_ttOre.m_IngotID );
		pIngotDef = g_Cfg.FindItemDef(idIngot);
		iIngotQty = 1;	// ingots per ore.
	}
	else
	{
		// Smelting something like armor etc.
		// find the ingot type resources.
		for ( int i=0; i<pOreDef->m_BaseResources.GetSize(); i++ )
		{
			CSphereUID rid = pOreDef->m_BaseResources[i].GetResourceID();
			if ( rid.GetResType() != RES_ItemDef )
				continue;

			CItemDefPtr pBaseDef = g_Cfg.FindItemDef( (ITEMID_TYPE) rid.GetResIndex());
			if ( pBaseDef == NULL )
				continue;

			if ( pBaseDef->IsType( IT_GEM ))
			{
				// bounce the gems out of this.
				CItemPtr pGem = CItem::CreateScript( pBaseDef->GetID(), this );
				ASSERT(pGem);
				pGem->SetAmount( iOreQty * pBaseDef->m_BaseResources[i].GetResQty() );
				ItemBounce( pGem );
				continue;
			}
			if ( pBaseDef->IsType( IT_INGOT ))
			{
				if ( iMiningSkill < pBaseDef->m_ttIngot.m_iSkillMin )
				{
					Printf("You lack the skill to smelt %s", (LPCTSTR) pBaseDef->GetName());
					continue;
				}
				pIngotDef = pBaseDef;
				iIngotQty = pOreDef->m_BaseResources[i].GetResQty();
			}
		}
	}

	if ( pIngotDef == NULL ||
		! pIngotDef->IsType(IT_INGOT))
	{
		WriteString( "It is consumed in the fire." );
		pItemOre->ConsumeAmount( iOreQty );
		return true;
	}

	iIngotQty *= iOreQty;	// max amount

	int iSkillRange = pIngotDef->m_ttIngot.m_iSkillMax - pIngotDef->m_ttIngot.m_iSkillMin;
	int iDifficulty = Calc_GetRandVal(iSkillRange);
	// iIngotQty = IMULDIV( iIngotQty, iDifficulty, iSkillRange );
	// if ( iIngotQty <= 0 )
	//	iIngotQty = 1;

	// try to make ingots.
	iDifficulty = ( pIngotDef->m_ttIngot.m_iSkillMin + iDifficulty ) / 10;
	if ( ! iIngotQty || ! Skill_UseQuick( SKILL_MINING, iDifficulty ))
	{
		Printf( "You smelt the %s but are left with nothing useful.", (LPCTSTR) pItemOre->GetName() );
		// Lose up to half the resources.
		pItemOre->ConsumeAmount( Calc_GetRandVal( pItemOre->GetAmount() / 2 ) + 1 );
		return( false );
	}

	// Payoff - What do i get ?
	// This is the one
	CItemPtr pIngots = CItem::CreateScript( pIngotDef->GetID(), this );
	if ( pIngots == NULL )
	{
		// Either this is really iron, or there isn't an ingot defined for this guy
		WriteString( "You smelt the ore but are left with nothing useful." );
		return( false );
	}

	// give some random loss factor.
	pIngots->SetAmount( iIngotQty );
	pItemOre->ConsumeAmount( pItemOre->GetAmount());
	ItemBounce( pIngots );
	return true;
}

bool CChar::Skill_Tracking( CSphereUID uidTarg, DIR_TYPE & dirPrv, int iDistMax )
{
	// SKILL_TRACKING
	// Allow this to track objects as well.

	CObjBasePtr pObj = g_World.ObjFind(uidTarg);
	if ( pObj == NULL )
	{
		return false;
	}

	CObjBasePtr pObjTop = pObj->GetTopLevelObj();
	if ( pObjTop == NULL )
	{
		return( false );
	}

	int dist = GetTopDist3D( pObjTop );	// disconnect = SHRT_MAX
	if ( dist > iDistMax )
	{
		return false;
	}

	if ( pObjTop->IsChar())
	{
		if ( ! CanDisturb( REF_CAST(CChar,pObjTop)))
			return( false );
	}

	DIR_TYPE dir = GetDir( pObjTop );
	if (( dirPrv != dir ) || ! Calc_GetRandVal(10))
	{
		dirPrv = dir;
		CGString sMsgDist;
		if ( dist )
		{
			LPCTSTR pszDist;
			if ( dist < 16 ) // Closing in message?
				pszDist = " near";
			else if ( dist < 32 )
				pszDist = "";
			else if ( dist < 100 )
				pszDist = " far";
			else
				pszDist = " very far";
			sMsgDist.Format( _TEXT( "%s to the %s" ), pszDist, (LPCTSTR) CGPointBase::sm_szDirs[ dir ] );
		}
		else
		{
			sMsgDist = _TEXT(" here");
		}

		CGString sMsg;
		sMsg.Format( "%s is%s%s", (LPCTSTR) pObj->GetName(),
			pObjTop->IsDisconnected() ? " disconnected" : "",
			(LPCTSTR) sMsgDist );
		ObjMessage( sMsg, this );
	}

	return true;		// keep the skill active.
}

//************************************
// Skill handlers.

int CChar::Skill_Tracking( CSkillDef::T_TYPE_ stage )
{
	// SKILL_TRACKING
	// m_Act.m_Targ = what am i tracking ?
	// m_Act.m_atTracking.m_PrvDir = the previous dir it was in.
	//

	if ( stage == CSkillDef::T_Start )
	{
		// Already checked difficulty earlier.
		return( 0 );
	}
	if ( stage == CSkillDef::T_Fail )
	{
		// This skill didn't fail, it just ended/went out of range, etc...
		ObjMessage( "You have lost your quary.", this ); // say this instead of the failure message
		return( -CSkillDef::T_Abort );
	}
	if ( stage == CSkillDef::T_Stroke )
	{
		int iSkillLevel = Skill_GetAdjusted(SKILL_TRACKING);
		if ( ! Skill_Tracking( m_Act.m_Targ, m_Act.m_atTracking.m_PrvDir, iSkillLevel/20 + 10 ))
			return( -CSkillDef::T_Abort );
		Skill_SetTimeout();		// next update.
		return( -CSkillDef::T_Stroke );	// keep it active.
	}

	return( -CSkillDef::T_Abort );
}

int CChar::Skill_Alchemy( CSkillDef::T_TYPE_ stage )
{
	// SKILL_ALCHEMY
	// m_Act.m_atCreate.m_ItemID = potion we are making.
	// We consume resources on each stroke.
	// This was start in Skill_MakeItem()

	CItemDefPtr pPotionDef = g_Cfg.FindItemDef( m_Act.m_atCreate.m_ItemID );
	if ( pPotionDef == NULL )
	{
		WriteString( "You have no clue how to make this potion." );
		return -CSkillDef::T_Abort;
	}

	if ( stage == CSkillDef::T_Start )
	{
		// See if skill allows a potion made out of targ'd reagent		// Sound( 0x243 );
		m_Act.m_atCreate.m_Stroke_Count = 0; // counts up.
		return( m_Act.m_Difficulty );
	}
	if ( stage == CSkillDef::T_Fail )
	{
		// resources have already been consumed.
		Emote( "toss the failed mixture from the mortar" );
		return( 0 );	// normal failure
	}
	if ( stage == CSkillDef::T_Success )
	{
		// Resources have already been consumed.
		// Now deliver the goods.
		Skill_MakeItem_Success(1);
		return( 0 );
	}

	ASSERT( stage == CSkillDef::T_Stroke );
	if ( stage != CSkillDef::T_Stroke )
		return( -CSkillDef::T_QTY );

	if ( m_Act.m_atCreate.m_Stroke_Count >= pPotionDef->m_BaseResources.GetSize())
	{
		// done.
		return 0;
	}

	// Keep trying and grinding
	//  OK, we know potion being attempted and the bottle
	//  it's going in....do a loop for each reagent

	CResourceQty item = pPotionDef->m_BaseResources[m_Act.m_atCreate.m_Stroke_Count];
	CSphereUID rid = item.GetResourceID();

	CItemDefPtr pReagDef = REF_CAST(CItemDef,g_Cfg.ResourceGetDef( rid ));
	if ( pReagDef == NULL )
	{
		return -CSkillDef::T_Abort;
	}

	if ( pReagDef->IsType(IT_POTION_EMPTY) && m_Act.m_Difficulty < 0 ) // going to fail anyhow.
	{
		// NOTE: Assume the bottle is ALWAYS LAST !
		// Don't consume the bottle.
		return -CSkillDef::T_Abort;
	}

	if ( ContentConsume( rid, item.GetResQty()))
	{
		Printf( "Hmmm, you lack %s for this potion.", (LPCTSTR) pReagDef->GetName() );
		return -CSkillDef::T_Abort;
	}

	if ( GetTopSector()->GetCharComplexity() < 5 && pReagDef->IsType(IT_REAGENT))
	{
		CGString sSpeak;
		sSpeak.Format(( m_Act.m_atCreate.m_Stroke_Count == 0 ) ?
			"start grinding some %s in the mortar" :
			"add %s and continue grinding", (LPCTSTR) pReagDef->GetName());
		Emote( sSpeak );
	}

	Sound( 0x242 );
	m_Act.m_atCreate.m_Stroke_Count ++;
	Skill_SetTimeout();
	return -CSkillDef::T_Stroke;	// keep active.
}

int CChar::Skill_Mining( CSkillDef::T_TYPE_ stage )
{
	// SKILL_MINING
	// m_Act.m_pt = the point we want to mine at.
	// m_Act.m_TargPrv = Shovel
	//
	// Test the chance of precious ore.
	// resource check  to IT_ORE. How much can we get ?
	// RETURN:
	//  Difficulty 0-100

	if ( m_Act.m_pt.m_x == 0xFFFF )
	{
		WriteString( "Try mining in rock!" );
		return( -CSkillDef::T_QTY );
	}

	// Verify so we have a line of sight.
	if ( ! CanSeeLOS( m_Act.m_pt, NULL, 2 ))
	{
		if ( GetTopPoint().GetDist( m_Act.m_pt ) > 2 )
		{
			WriteString("That is too far away." );
		}
		else
		{
			WriteString("You have no line of sight to that location");
		}
		return( -CSkillDef::T_QTY );
	}

	// resource check
	CItemPtr pResBit = g_World.CheckNaturalResource( m_Act.m_pt, IT_ROCK, stage == CSkillDef::T_Start );
	if ( pResBit == NULL )
	{
		WriteString( "Try mining in rock." );
		return( -CSkillDef::T_QTY );
	}
	if ( pResBit->GetAmount() == 0 )
	{
		WriteString( "There is no ore here to mine." );
		return( -CSkillDef::T_QTY );
	}

	CItemPtr pShovel = g_World.ItemFind(m_Act.m_TargPrv);
	if ( pShovel == NULL )
	{
		WriteString( "You must use a shovel or pick." );
		return( -CSkillDef::T_Abort );
	}

	if ( stage == CSkillDef::T_Fail )
		return 0;

	if ( stage == CSkillDef::T_Start )
	{
		m_Act.m_atResource.m_Stroke_Count = Calc_GetRandVal( 5 ) + 2;

		pShovel->OnTakeDamage( 1, this, DAMAGE_HIT_BLUNT );

		return( Skill_NaturalResource_Setup( pResBit ));
	}

	if ( stage == CSkillDef::T_Stroke )
	{
		// Pick a "mining" type sound
		Sound( ( Calc_GetRandVal(2)) ? 0x125 : 0x126 );
		UpdateDir( m_Act.m_pt );

		if ( m_Act.m_atResource.m_Stroke_Count )
		{
			// Keep trying and updating the animation
			m_Act.m_atResource.m_Stroke_Count --;
			UpdateAnimate( ANIM_ATTACK_1H_DOWN );
			Skill_SetTimeout();
			return( -CSkillDef::T_Stroke );	// keep active.
		}

		return( 0 );
	}

	ASSERT( stage == CSkillDef::T_Success );

	CItemPtr pItem = Skill_NaturalResource_Create( pResBit, SKILL_MINING );
	if ( pItem == NULL )
	{
		WriteString( "There is no ore here to mine." );
		return( -CSkillDef::T_Fail );
	}

	ItemBounce( pItem );
	return( 0 );
}

int CChar::Skill_Fishing( CSkillDef::T_TYPE_ stage )
{
	// m_Act.m_pt = where to fish.
	// NOTE: don't check LOS else you can't fish off boats.
	// Check that we dont stand too far away
	// Make sure we aren't in a house
	// RETURN:
	//   difficulty = 0-100

	CRegionPtr pRegion = GetTopRegion( REGION_TYPE_MULTI );
	if ( pRegion  && ! pRegion->IsFlag( REGION_FLAG_SHIP ))
	{
		// We are in a house ?
		WriteString("You can't fish from where you are standing.");
		return( -CSkillDef::T_QTY );
	}

	if ( GetTopPoint().GetDist( m_Act.m_pt ) > 6 )	// cast works for long distances.
	{
		WriteString("That is too far away." );
		return( -CSkillDef::T_QTY );
	}

	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}
	if ( stage == CSkillDef::T_Fail )
	{
		return 0;
	}

	// resource check
	CItemPtr pResBit = g_World.CheckNaturalResource( m_Act.m_pt, IT_WATER, stage == CSkillDef::T_Start );
	if ( pResBit == NULL )
	{
		WriteString( "There are no fish here." );
		return( -CSkillDef::T_QTY );
	}
	if ( pResBit->GetAmount() == 0 )
	{
		WriteString( "There are no fish here." );
		return( -CSkillDef::T_QTY );
	}

	Sound( 0x027 );

	// Create the little splash effect.
	CItemPtr pItemFX = CItem::CreateBase( ITEMID_FX_SPLASH );
	ASSERT(pItemFX);
	pItemFX->SetType(IT_WATER_WASH);	// can't fish here.

	if ( stage == CSkillDef::T_Start )
	{
		pItemFX->MoveToDecay( m_Act.m_pt, 1*TICKS_PER_SEC );

		UpdateAnimate( ANIM_ATTACK_2H_DOWN );
		return( Skill_NaturalResource_Setup( pResBit ));
	}

	if ( stage == CSkillDef::T_Success )
	{
		pItemFX->MoveToDecay( m_Act.m_pt, 3*TICKS_PER_SEC );

		CItemPtr pFish = Skill_NaturalResource_Create( pResBit, SKILL_FISHING );
		if ( pFish == NULL )
		{
			return( -CSkillDef::T_Abort );
		}

		Printf( "You pull out a %s!", (LPCTSTR) pFish->GetName() );
		pFish->MoveToCheck( GetTopPoint(), this );	// put at my feet.
		return( 0 );
	}

	ASSERT(0);
	return( -CSkillDef::T_QTY);
}

int CChar::Skill_Lumberjack( CSkillDef::T_TYPE_ stage )
{
	// RETURN:
	//   difficulty = 0-100

	if ( m_Act.m_pt.m_x == 0xFFFF )
	{
		WriteString( "Try chopping a tree." );
		return( -CSkillDef::T_QTY );
	}

	if ( stage == CSkillDef::T_Fail )
	{
		return 0;
	}

	// 3D distance check and LOS
	if ( ! CanTouch(m_Act.m_pt) || GetTopPoint().GetDist3D( m_Act.m_pt ) > 3 )
	{
		if ( GetTopPoint().GetDist( m_Act.m_pt ) > 3 )
		{
			WriteString("That is too far away." );
		}
		else
		{
			WriteString("You have no line of sight to that location");
		}
		return( -CSkillDef::T_QTY );
	}

	// resource check
	CItemPtr pResBit = g_World.CheckNaturalResource( m_Act.m_pt, IT_TREE, stage == CSkillDef::T_Start );
	if ( pResBit == NULL )
	{
		WriteString( "Try chopping a tree." );
		return( -CSkillDef::T_QTY );
	}
	if ( pResBit->GetAmount() == 0 )
	{
		WriteString( "There are no logs here to chop." );
		return( -CSkillDef::T_QTY );
	}

	if ( stage == CSkillDef::T_Start )
	{
		m_Act.m_atResource.m_Stroke_Count = Calc_GetRandVal( 5 ) + 2;
		return( Skill_NaturalResource_Setup( pResBit ));
	}

	if ( stage == CSkillDef::T_Stroke )
	{
		Sound( 0x13e );	// 0135, 013e, 148, 14a
		UpdateDir( m_Act.m_pt );
		if ( m_Act.m_atResource.m_Stroke_Count )
		{
			// Keep trying and updating the animation
			m_Act.m_atResource.m_Stroke_Count --;
			UpdateAnimate( ANIM_ATTACK_WEAPON );
			Skill_SetTimeout();
			return( -CSkillDef::T_Stroke );	// keep active.
		}
		return 0;
	}

	ASSERT( stage == CSkillDef::T_Success );

	// resource check

	CItemPtr pItem = Skill_NaturalResource_Create( pResBit, SKILL_LUMBERJACKING );
	if ( pItem == NULL )
		return( -CSkillDef::T_Fail );

	ItemBounce( pItem );
	return( 0 );
}

int CChar::Skill_DetectHidden( CSkillDef::T_TYPE_ stage )
{
	// SKILL_DETECTINGHIDDEN
	// Look around for who is hiding.
	// Detect them based on skill diff.
	// ??? Hidden objects ?

	if ( stage == CSkillDef::T_Start )
	{
		// Based on who is hiding ?
		return( 10 );
	}
	if ( stage == CSkillDef::T_Fail )
	{
		return 0;
	}
	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}
	if ( stage != CSkillDef::T_Success )
	{
		ASSERT(0);
		return( -CSkillDef::T_QTY );
	}

	int iRadius = ( Skill_GetAdjusted(SKILL_DETECTINGHIDDEN) / 8 ) + 1;
	CWorldSearch Area( GetTopPoint(), iRadius );
	bool fFound = false;
	for(;;)
	{
		CCharPtr pChar = Area.GetNextChar();
		if ( pChar == NULL )
			break;
		if ( pChar == this )
			continue;
		if ( ! pChar->IsStatFlag( STATF_Invisible | STATF_Hidden ))
			continue;
		// Try to detect them.
		if ( pChar->IsStatFlag( STATF_Hidden ))
		{
			// If there hiding skill is much better than our detect then stay hidden
		}
		pChar->Reveal();
		Printf( "You find %s", (LPCTSTR) pChar->GetName());
		fFound = true;
	}

	if ( ! fFound )
	{
		return( -CSkillDef::T_Fail );
	}

	return( 0 );
}

int CChar::Skill_Cartography( CSkillDef::T_TYPE_ stage )
{
	// Selected a map type and now we are making it.
	// m_Act_Cartography_Dist = the map distance.
	// Find the blank map to write on first.

	if ( stage == CSkillDef::T_Stroke )
		return 0;

	CPointMap pnt = GetTopPoint();
	if ( pnt.m_x >= pnt.GetMulMap()->m_iSizeXWrap )	// maps don't work out here !
	{
		WriteString( "You can't seem to figure out your surroundings." );
		return( -CSkillDef::T_QTY );
	}

	CItemPtr pItem = ContentFind( CSphereUID(RES_TypeDef,IT_MAP_BLANK), 0 );
	if ( pItem == NULL )
	{
		WriteString( "You have no blank parchment to draw on" );
		return( -CSkillDef::T_QTY );
	}

	if ( ! CanUse( pItem, true ))
	{
		Printf( "You can't use the %s where it is.", (LPCTSTR) pItem->GetName() );
		return( false );
	}

	m_Act.m_Targ = pItem->GetUID();

	if ( stage == CSkillDef::T_Start )
	{
		Sound( 0x249 );

		// difficulty related to m_Act.m_atCartography.m_Dist ???

		return( Calc_GetRandVal(100) );
	}
	if ( stage == CSkillDef::T_Fail )
	{
		// consume the map sometimes ?
		// pItem->ConsumeAmount( 1 );
		return 0;
	}
	if ( stage == CSkillDef::T_Success )
	{
		pItem->ConsumeAmount( 1 );

		// Get a valid region.
		CRectMap rect;
		rect.SetRect( pnt.m_x - m_Act.m_atCartography.m_Dist,
			pnt.m_y - m_Act.m_atCartography.m_Dist,
			pnt.m_x + m_Act.m_atCartography.m_Dist,
			pnt.m_y + m_Act.m_atCartography.m_Dist );

		// Now create the map
		pItem = CItem::CreateScript( ITEMID_MAP, this );
		pItem->m_itMap.m_top = rect.m_top;
		pItem->m_itMap.m_left = rect.m_left;
		pItem->m_itMap.m_bottom = rect.m_bottom;
		pItem->m_itMap.m_right = rect.m_right;
		ItemBounce( pItem );
		return( 0 );
	}

	ASSERT(0);
	return( -CSkillDef::T_QTY );
}

int CChar::Skill_Musicianship( CSkillDef::T_TYPE_ stage )
{
	// m_Act.m_Targ = the intrument i targetted to play.

	if ( stage == CSkillDef::T_Stroke )
		return 0;
	if ( stage == CSkillDef::T_Start )
	{
		// no instrument fail immediate
		return Use_PlayMusic( g_World.ItemFind(m_Act.m_Targ), Calc_GetRandVal(90));;
	}

	return( 0 );
}

int CChar::Skill_Peacemaking( CSkillDef::T_TYPE_ stage )
{
	// try to make all those listening peacable.
	// General area effect.
	// make peace if possible. depends on who is listening/fighting.

	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}
	if ( stage == CSkillDef::T_Start )
	{
		// Find musical inst first.

		// Basic skill check.
		int iDifficulty = Use_PlayMusic( NULL, Calc_GetRandVal(40));
		if ( iDifficulty < -1 )	// no instrument fail immediate
			return( -CSkillDef::T_Fail );

		if ( ! iDifficulty )
		{
			iDifficulty = Calc_GetRandVal(40);	// Depend on evil of the creatures here.
		}

		// Who is fighting around us ? determines difficulty.
		return( iDifficulty );
	}

	if ( stage == CSkillDef::T_Fail || stage == CSkillDef::T_Success )
	{
		// Failure just irritates.

		int iRadius = ( Skill_GetAdjusted(SKILL_PEACEMAKING) / 8 ) + 1;
		CWorldSearch Area( GetTopPoint(), iRadius );
		for(;;)
		{
			CCharPtr pChar = Area.GetNextChar();
			if ( pChar == NULL )
				return( -CSkillDef::T_Fail );
			if ( pChar == this )
				continue;
			break;
		}
		return 0;
	}

	ASSERT(0);
	return( -CSkillDef::T_QTY );
}

int CChar::Skill_Enticement( CSkillDef::T_TYPE_ stage )
{
	// m_Act.m_Targ = my target
	// Just keep playing and trying to allure them til we can't
	// Must have a musical instrument.

	CCharPtr pChar = g_World.CharFind(m_Act.m_Targ);
	if ( pChar == NULL )
	{
		return( -CSkillDef::T_QTY );
	}
	if ( stage == CSkillDef::T_Fail )
	{
		return 0;
	}
	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}
	if ( stage == CSkillDef::T_Success )
	{
		// Walk to me ?
		return 0;
	}
	if ( stage == CSkillDef::T_Start )
	{
		// Base music diff, (whole thing won't work if this fails)
		int iDifficulty = Use_PlayMusic( NULL, Calc_GetRandVal(55));
		if ( iDifficulty < -1 )	// no instrument fail immediate
			return( -CSkillDef::T_QTY );

		m_Act.m_atMusician.m_iMusicDifficulty = iDifficulty;

		// Based on the STAT_Int and will of the target.
		if ( ! iDifficulty )
		{
			return pChar->m_StatInt;
		}

		return( iDifficulty );
	}

	ASSERT(0);
	return( -CSkillDef::T_QTY );
}

int CChar::Skill_Provocation( CSkillDef::T_TYPE_ stage )
{
	// m_Act.m_TargPrv = provoke this person
	// m_Act.m_Targ = against this person.

	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}

	CCharPtr pCharProv = g_World.CharFind(m_Act.m_TargPrv);
	CCharPtr pCharTarg = g_World.CharFind(m_Act.m_Targ);

	// If no provoker, then we fail (naturally!)
	if ( pCharProv == NULL || pCharProv == this )
	{
		WriteString( "You are really upset about this" );
		return -CSkillDef::T_QTY;
	}

	if ( stage == CSkillDef::T_Fail )
	{
		if ( pCharProv->IsClient() )
			CheckCrimeSeen( SKILL_NONE, pCharProv, pCharTarg, "provoking" );

		// Might just attack you !
		pCharProv->Fight_Attack( this );
		return( 0 );
	}

	if ( stage == CSkillDef::T_Start )
	{
		int iDifficulty = Use_PlayMusic( NULL, Calc_GetRandVal(40));
		if ( iDifficulty < -1 )	// no instrument fail immediate
			return( false );
		if ( ! iDifficulty )
		{
			iDifficulty = pCharProv->m_StatInt;	// Depend on evil of the creature.
		}

		return( iDifficulty );
	}

	if ( stage != CSkillDef::T_Success )
	{
		ASSERT(0);
		return -CSkillDef::T_QTY;
	}

	if ( pCharProv->IsClient() )
	{
		CheckCrimeSeen( SKILL_NONE, pCharProv, pCharTarg, "provoking" );
		return -CSkillDef::T_Fail;
	}

	// If out of range or something, then we might get attacked ourselves.
	if ( pCharProv->Stat_Get(STAT_Karma) >= 10000 )
	{
		// They are just too good for this.
		pCharProv->Emote( "looks peaceful" );
		return -CSkillDef::T_Abort;
	}

	pCharProv->Emote( "looks furious" );

	// If no target then skill fails
	if ( pCharTarg == NULL )
	{
		return -CSkillDef::T_Fail;
	}

	// He realizes that you are the real bad guy as well.
	if ( ! pCharTarg->OnAttackedBy( this, 1, true ))
	{
		return -CSkillDef::T_Abort;
	}

	pCharProv->Memory_AddObjTypes( this, MEMORY_AGGREIVED|MEMORY_IRRITATEDBY );

	// If out of range we might get attacked ourself.
	if ( pCharProv->GetTopDist3D( pCharTarg ) > SPHEREMAP_VIEW_SIGHT ||
		pCharProv->GetTopDist3D( this ) > SPHEREMAP_VIEW_SIGHT )
	{
		// Check that only "evil" monsters attack provoker back
		if ( pCharProv->Noto_IsEvil())
		{
			pCharProv->Fight_Attack( this );
		}
		return -CSkillDef::T_Abort;
	}

	// If we are provoking against a "good" PC/NPC and the provoked
	// NPC/PC is good, we are flagged criminal for it and guards
	// are called.
	if ( pCharProv->Noto_GetFlag(this)==NOTO_GOOD )
	{
		// lose some karma for this.
		CheckCrimeSeen( SKILL_NONE, pCharProv, pCharTarg, "provoking" );
		return -CSkillDef::T_Abort;
	}

	// If we provoke upon a good char we should go criminal for it
	// but skill still succeed.
	if ( pCharTarg->Noto_GetFlag(this)==NOTO_GOOD )
	{
		CheckCrimeSeen( SKILL_NONE, pCharTarg, pCharProv, "provoking" );
	}

	pCharProv->Fight_Attack( pCharTarg ); // Make the actual provoking.
	return( 0 );
}

int CChar::Skill_Poisoning( CSkillDef::T_TYPE_ stage )
{
	// Act_TargPrv = poison this weapon/food
	// Act_Targ = with this poison.

	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}

	CItemPtr pPoison = g_World.ItemFind(m_Act.m_Targ);
	if ( pPoison == NULL ||
		! pPoison->IsType(IT_POTION))
	{
		return( -CSkillDef::T_Abort );
	}

	if ( stage == CSkillDef::T_Start )
	{
		return Calc_GetRandVal( 60 );
	}
	if ( stage == CSkillDef::T_Fail )
	{
		// Lose the poison sometimes ?
		return( 0 );
	}

	if ( RES_GET_INDEX(pPoison->m_itPotion.m_Type) != SPELL_Poison )
	{
		return( -CSkillDef::T_Abort );
	}

	CItemPtr pItem = g_World.ItemFind(m_Act.m_TargPrv);
	if ( pItem == NULL )
	{
		return( -CSkillDef::T_QTY );
	}

	if ( stage != CSkillDef::T_Success )
	{
		ASSERT(0);
		return( -CSkillDef::T_Abort );
	}

	Sound( 0x247 );	// powdering.

	switch ( pItem->GetType() )
	{
	case IT_FRUIT:
	case IT_FOOD:
	case IT_FOOD_RAW:
	case IT_MEAT_RAW:
		pItem->m_itFood.m_poison_skill = pPoison->m_itPotion.m_skillquality / 10;
		break;
	case IT_WEAPON_MACE_SHARP:
	case IT_WEAPON_SWORD:		// 13 =
	case IT_WEAPON_FENCE:		// 14 = can't be used to chop trees. (make kindling)
		pItem->m_itWeapon.m_poison_skill = pPoison->m_itPotion.m_skillquality / 10;
		break;
	default:
		WriteString( "You can only poison food or piercing weapons." );
		return( -CSkillDef::T_QTY );
	}
	// skill + quality of the poison.
	WriteString( "You apply the poison." );
	pPoison->ConsumeAmount();
	return( 0 );
}

int CChar::Skill_Cooking( CSkillDef::T_TYPE_ stage )
{
	// SKILL_COOKING
	// m_Act.m_Targ = food object to cook.
	// m_Act.m_pt = my fire.
	// How hard to cook is this ?

	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}

	CItemPtr pFoodRaw = g_World.ItemFind(m_Act.m_Targ);
	if ( pFoodRaw == NULL )
	{
		return( -CSkillDef::T_QTY );
	}
	if ( ! pFoodRaw->IsType( IT_FOOD_RAW ) && ! pFoodRaw->IsType( IT_MEAT_RAW ))
	{
		return( -CSkillDef::T_Abort );
	}

	if ( stage == CSkillDef::T_Start )
	{
		return Calc_GetRandVal( 50 );
	}

	// Convert uncooked food to cooked food.
	ITEMID_TYPE id = (ITEMID_TYPE) RES_GET_INDEX( pFoodRaw->m_itFood.m_cook_id );
	if ( ! id )
	{
		id = (ITEMID_TYPE) pFoodRaw->Item_GetDef()->m_ttFoodRaw.m_cook_id.GetResIndex();
		if ( ! id )	// does not cook into anything.
		{
			return( -CSkillDef::T_QTY );
		}
	}

	CItemPtr pFoodCooked;
	if ( stage == CSkillDef::T_Success )
	{
		pFoodCooked = CItem::CreateTemplate( id, NULL, this );
		if ( pFoodCooked )
		{
			WriteString( "Mmm, smells good" );
			pFoodCooked->m_itFood.m_MeatType = pFoodRaw->m_itFood.m_MeatType;
			ItemBounce(pFoodCooked);
		}
	}
	else	// CSkillDef::T_Fail
	{
		// Burn food
	}

	pFoodRaw->ConsumeAmount();

	if ( pFoodCooked == NULL )
	{
		return( -CSkillDef::T_QTY );
	}

	return( 0 );
}

int CChar::Skill_Taming( CSkillDef::T_TYPE_ stage )
{
	// m_Act.m_Targ = creature to tame.
	// Check the min required skill for this creature.
	// Related to INT ?

	CCharPtr pChar = g_World.CharFind(m_Act.m_Targ);
	if ( pChar == NULL )
	{
		return( -CSkillDef::T_QTY );
	}
	if ( pChar == this )
	{
		WriteString( "You are your own master." );
		return( -CSkillDef::T_QTY );
	}
	if ( pChar->m_pPlayer.IsValidNewObj())
	{
		WriteString( "You can't tame them." );
		return( -CSkillDef::T_QTY );
	}
	if ( ! CanTouch( pChar ))
	{
		WriteString( "You are too far away" );
		return -CSkillDef::T_QTY;
	}

	UpdateDir( pChar );

	ASSERT( pChar->m_pNPC.IsValidNewObj());

	int iTameBase = pChar->Skill_GetBase(SKILL_TAMING);
	if ( ! IsGM()) // if its a gm doing it, just check that its not
	{
		// Is it tamable ?
		if ( pChar->IsStatFlag( STATF_Pet ))
		{
			Printf( "%s is already tame.", (LPCTSTR) pChar->GetName());
			return( -CSkillDef::T_QTY );
		}

		// Too smart or not an animal.
		if ( ! iTameBase || pChar->Skill_GetBase(SKILL_ANIMALLORE))
		{
			Printf( "%s cannot be tamed.", (LPCTSTR) pChar->GetName());
			return( -CSkillDef::T_QTY );
		}

		// You shouldn't be able to tame creatures that are above your level
		if ( iTameBase > Skill_GetBase(SKILL_TAMING) )
		{
			Printf( "You have no chance of taming %s.", (LPCTSTR) pChar->GetName());
			return( -CSkillDef::T_QTY );
		}
	}

	if ( stage == CSkillDef::T_Start )
	{
		// The difficulty should be based on the difference between your skill level
		// and the creature's base taming value
		// If TameBase == My taming, difficulty should be 100
		// If TameBase == 0, difficulty should be 0
		// Make it linear for now
		int iDifficulty = (iTameBase * 100) / Skill_GetBase(SKILL_TAMING);
		if ( iDifficulty > 100 )
			iDifficulty = 100;

		if ( pChar->Memory_FindObjTypes( this, MEMORY_FIGHT|MEMORY_HARMEDBY|MEMORY_IRRITATEDBY|MEMORY_AGGREIVED ))
		{
			// I've attacked it b4 ?
			iDifficulty += 50;
		}

		m_Act.m_atTaming.m_Stroke_Count = Calc_GetRandVal( 4 ) + 2;
		return( iDifficulty );
	}

	if ( stage == CSkillDef::T_Fail )
	{
		// chance of being attacked ?
		return( 0 );
	}

	if ( stage == CSkillDef::T_Stroke )
	{
		static LPCTSTR const sm_szTameSpeak[] =
		{
			"I won't hurt you.",
			"I always wanted a %s like you",
			"Good %s",
			"Here %s",
		};

		if ( IsGM())
			return( 0 );
		if ( m_Act.m_atTaming.m_Stroke_Count <= 0 )
			return( 0 );

		CGString sSpeak;
		sSpeak.Format( sm_szTameSpeak[ Calc_GetRandVal( COUNTOF( sm_szTameSpeak )) ], (LPCTSTR) pChar->GetName());
		Speak( sSpeak );

		// Keep trying and updating the animation
		m_Act.m_atTaming.m_Stroke_Count --;
		Skill_SetTimeout();
		return -CSkillDef::T_Stroke;
	}

	ASSERT( stage == CSkillDef::T_Success );

	// Create the memory of being tamed to prevent lame macroers
	CItemMemoryPtr pMemory = pChar->Memory_FindObjTypes( this, MEMORY_SPEAK );
	if ( pMemory &&
		pMemory->m_itEqMemory.m_Arg1 == NPC_MEM_ACT_TAMED)
	{
		// See if I tamed it before
		// I did, no skill to tame it again
		CGString sSpeak;
		sSpeak.Format( "The %s remembers you and accepts you once more as it's master.", (LPCTSTR) pChar->GetName());
		ObjMessage( sSpeak, pChar );

		pChar->NPC_PetSetOwner( this );
		// pChar->Stat_Set( STAT_Food, 50 );	// this is good for something.
		pChar->m_Act.m_Targ = GetUID();
		pChar->Skill_Start( NPCACT_FOLLOW_TARG );
		return -CSkillDef::T_QTY;	// no credit for this.
	}

	pChar->NPC_PetSetOwner( this );
	pChar->Stat_Set( STAT_Food, 50 );	// this is good for something.
	pChar->m_Act.m_Targ = GetUID();
	pChar->Skill_Start( NPCACT_FOLLOW_TARG );
	WriteString( "It seems to accept you as master" );

	// Create the memory of being tamed to prevent lame macroers
	pMemory = pChar->Memory_AddObjTypes( this, MEMORY_SPEAK );
	ASSERT(pMemory);
	pMemory->m_itEqMemory.m_Arg1 = NPC_MEM_ACT_TAMED;
	return( 0 );
}

int CChar::Skill_Lockpicking( CSkillDef::T_TYPE_ stage )
{
	// m_Act.m_Targ = the item to be picked.
	// m_Act.m_TargPrv = The pick.

	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}

	CItemPtr pPick = g_World.ItemFind(m_Act.m_TargPrv);
	if ( pPick == NULL || ! pPick->IsType( IT_LOCKPICK ))
	{
		WriteString( "You need a lock pick." );
		return -CSkillDef::T_QTY;
	}

	CItemPtr pLock = g_World.ItemFind(m_Act.m_Targ);
	if ( pLock == NULL )
	{
		WriteString( "Use the lock pick on a lockable item." );
		return -CSkillDef::T_QTY;
	}

	if ( pPick->GetTopLevelObj() != this )	// the pick is gone !
	{
		WriteString( "Your pick must be on your person." );
		return -CSkillDef::T_QTY;
	}

	if ( stage == CSkillDef::T_Fail )
	{
		// Damage my pick
		pPick->OnTakeDamage( 1, this, DAMAGE_HIT_BLUNT );
		return( 0 );
	}

	if ( ! CanTouch( pLock ))	// we moved too far from the lock.
	{
		WriteString( "You can't reach that." );
		return -CSkillDef::T_QTY;
	}

	if (  stage == CSkillDef::T_Start )
	{
		return( pLock->Use_LockPick( this, true, false ));
	}

	ASSERT( stage == CSkillDef::T_Success );

	if ( pLock->Use_LockPick( this, false, false ) < 0 )
	{
		return -CSkillDef::T_Fail;
	}
	return 0;
}

int CChar::Skill_Hiding( CSkillDef::T_TYPE_ stage )
{
	// SKILL_Stealth = move while already hidden !
	// SKILL_Hiding
	// Skill required varies with terrain and situation ?
	// if we are carrying a light source then this should not work.

#if 0
	// We shoud just stay in HIDING skill. ?
#else
	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}
	if ( stage == CSkillDef::T_Fail )
	{
		Reveal( STATF_Hidden );
		return 0;
	}

	if ( stage == CSkillDef::T_Success )
	{
		if ( IsStatFlag( STATF_Hidden ))
		{
			// I was already hidden ? so un-hide.
			Reveal( STATF_Hidden );
			return( -CSkillDef::T_Abort );
		}

		ObjMessage( "You have hidden yourself well", this );
		StatFlag_Set( STATF_Hidden );
		UpdateMode();
		return( 0 );
	}

	if ( stage == CSkillDef::T_Start )
	{
		// Make sure i am not carrying a light ?

		CItemPtr pItem = GetHead();
		for ( ; pItem; pItem = pItem->GetNext())
		{
			LAYER_TYPE layer = pItem->GetEquipLayer();
			if ( ! CItemDef::IsVisibleLayer( layer ))
				continue;
			if ( pItem->IsType( IT_EQ_HORSE))
			{
				// Horses are hard to hide !
				WriteString( "Your horse reveals you" );
				return( -CSkillDef::T_QTY );
			}
			if ( pItem->Item_GetDef()->Can( CAN_I_LIGHT ))
			{
				WriteString( "You are too well lit to hide" );
				return( -CSkillDef::T_QTY );
			}
		}

		return Calc_GetRandVal(70);
	}
#endif

	ASSERT(0);
	return( -CSkillDef::T_QTY );
}

int CChar::Skill_Herding( CSkillDef::T_TYPE_ stage )
{
	// m_Act.m_Targ = move this creature.
	// m_Act.m_pt = move to here.
	// How do I make them move fast ? or with proper speed ???

	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}

	CCharPtr pChar = g_World.CharFind(m_Act.m_Targ);
	if ( pChar == NULL )
	{
		WriteString( "You lost your target!" );
		return( -CSkillDef::T_QTY );
	}
	CItemPtr pCrook = g_World.ItemFind(m_Act.m_TargPrv);
	if ( pCrook == NULL )
	{
		WriteString( "You lost your crook!" );
		return( -CSkillDef::T_QTY );
	}

	// special GM version to move to coordinates.
	if ( ! IsGM())
	{
		// Herdable ?
		if ( pChar->m_pPlayer.IsValidNewObj() ||
			! pChar->m_pNPC.IsValidNewObj() ||
			pChar->m_pNPC->m_Brain != NPCBRAIN_ANIMAL )
		{
			WriteString( "They look somewhat annoyed" );
			return( -CSkillDef::T_QTY );
		}
	}
	else
	{
		if ( GetPrivLevel() < pChar->GetPrivLevel())
			return( -CSkillDef::T_QTY );
	}

	if ( stage == CSkillDef::T_Start )
	{
		UpdateAnimate( ANIM_ATTACK_WEAPON );
		int iIntVal = pChar->m_StatInt / 2;
		return( iIntVal + Calc_GetRandVal(iIntVal));
	}
	if ( stage == CSkillDef::T_Fail )
	{
		// Irritate the animal.
		return 0;
	}

	//
	// Try to make them walk there.

	ASSERT( stage == CSkillDef::T_Success );

	if ( IsGM())
	{
		pChar->Spell_Effect_Teleport( m_Act.m_pt, true, false );
	}
	else
	{
		pChar->m_Act.m_pt = m_Act.m_pt;
		pChar->Skill_Start( NPCACT_GOTO );
	}

	ObjMessage( "The animal goes where it is instructed", pChar );
	return( 0 );
}

int CChar::Skill_SpiritSpeak( CSkillDef::T_TYPE_ stage )
{
	if ( stage == CSkillDef::T_Fail )
	{
		// bring ghosts ? hehe
		return 0;
	}
	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}
	if ( stage == CSkillDef::T_Start )
	{
		// difficulty based on spirits near ?
		return( Calc_GetRandVal( 90 ));
	}
	if ( stage == CSkillDef::T_Success )
	{
		if ( IsStatFlag( STATF_SpiritSpeak ))
			return( -CSkillDef::T_Abort );
		WriteString( "You establish a connection to the netherworld." );
		Sound( 0x24a );
		Spell_Equip_Create( SPELL_NONE, LAYER_FLAG_SpiritSpeak, m_Act.m_Difficulty*10, 4*60*TICKS_PER_SEC, this, false );
		return( 0 );
	}

	ASSERT(0);
	return( -CSkillDef::T_Abort );
}

int CChar::Skill_Meditation( CSkillDef::T_TYPE_ stage )
{
	// SKILL_MEDITATION
	// Try to regen your mana even faster than normal.
	// Give experience only when we max out.

	if ( stage == CSkillDef::T_Fail || stage == CSkillDef::T_Abort )
	{
		return 0;
	}

	if ( stage == CSkillDef::T_Start )
	{
		if ( m_StatMana >= m_StatMaxMana )
		{
			WriteString( "You are at peace." );
			return( -CSkillDef::T_QTY );
		}
		m_Act.m_atTaming.m_Stroke_Count = 0;

		WriteString( "You attempt a meditative trance." );

		return Calc_GetRandVal(100);	// how hard to get started ?
	}
	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}
	if ( stage == CSkillDef::T_Success )
	{
		if ( m_StatMana >= m_StatMaxMana )
		{
			WriteString( "You are at peace." );
			return( 0 );	// only give skill credit now.
		}

		if ( m_Act.m_atTaming.m_Stroke_Count == 0 )
		{
			Sound( 0x0f9 );
		}
		m_Act.m_atTaming.m_Stroke_Count++;

		Stat_Change( STAT_Mana, 1 );

		// next update. (depends on skill)
		Skill_SetTimeout();

		// Set a new possibility for failure ?
		// iDifficulty = Calc_GetRandVal(100);
		return( -CSkillDef::T_Stroke );
	}

	DEBUG_CHECK(0);
	return( -CSkillDef::T_QTY );
}

int CChar::Skill_Healing( CSkillDef::T_TYPE_ stage )
{
	// SKILL_VETERINARY:
	// SKILL_HEALING
	// m_Act.m_TargPrv = bandages.
	// m_Act.m_Targ = heal target.
	//
	// should depend on the severity of the wounds ?
	// should be just a fast regen over time ?
	// RETURN:
	//  = -3 = failure.

	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}

	CItemPtr pBandage = g_World.ItemFind(m_Act.m_TargPrv);
	if ( pBandage == NULL )
	{
		WriteString("Where are your bandages?");
		return( -CSkillDef::T_QTY );
	}
	if ( ! pBandage->IsType(IT_BANDAGE))
	{
		WriteString("Use a bandage");
		return( -CSkillDef::T_QTY );
	}

	CObjBasePtr pObj = g_World.ObjFind(m_Act.m_Targ);
	if ( ! CanTouch(pObj))
	{
		WriteString("You must be able to reach the target");
		return( -CSkillDef::T_QTY );
	}

	CItemCorpsePtr pCorpse;	// resurrect by corpse.
	CCharPtr pChar;
	if ( pObj->IsItem())
	{
		// Corpse ?
		pCorpse = REF_CAST(CItemCorpse,pObj);
		if ( pCorpse == NULL )
		{
			WriteString("Try healing a creature.");
			return( -CSkillDef::T_QTY );
		}

		pChar = g_World.CharFind(pCorpse->m_uidLink);
	}
	else
	{
		pCorpse = NULL;
		pChar = g_World.CharFind(m_Act.m_Targ);
	}

	if ( pChar == NULL )
	{
		WriteString( "This creature is beyond help." );
		return( -CSkillDef::T_QTY );
	}

	if ( GetDist(pObj) > 2 )
	{
		Printf( "You are too far away to apply bandages on %s", (LPCTSTR) pObj->GetName());
		if ( pChar != this )
		{
			pChar->Printf( "%s is attempting to apply bandages to %s, but they are too far away!",
				(LPCTSTR) GetName(), (LPCTSTR) ( pCorpse ? ( pCorpse->GetName()) : "you" ));
		}
		return( -CSkillDef::T_QTY );
	}

	if ( pCorpse )
	{
		if ( ! pCorpse->IsTopLevel())
		{
			WriteString( "Put the corpse on the ground" );
			return( -CSkillDef::T_QTY );
		}
		CRegionPtr pRegion = pCorpse->GetTopRegion(REGION_TYPE_AREA|REGION_TYPE_MULTI);
		if ( pRegion == NULL )
		{
			return( -CSkillDef::T_QTY );
		}
		if ( pRegion->IsFlag( REGION_ANTIMAGIC_ALL | REGION_ANTIMAGIC_RECALL_IN | REGION_ANTIMAGIC_TELEPORT ))
		{
			WriteString( "Your resurrection attempt is blocked by antimagic." );
			if ( pChar != this )
			{
				pChar->Printf( "%s is attempting to apply bandages to %s, but they are blocked by antimagic!",
					(LPCTSTR) GetName(), (LPCTSTR) pCorpse->GetName() );
			}
			return( -CSkillDef::T_QTY );
		}
	}
	else if ( pChar->IsStatFlag(STATF_DEAD))
	{
		WriteString( "You can't heal a ghost! Try healing their corpse." );
		return( -CSkillDef::T_QTY );
	}

	if ( ! pChar->IsStatFlag( STATF_Poisoned|STATF_DEAD ) &&
		pChar->GetHealthPercent() >= 100 )
	{
		if ( pChar == this )
		{
			Printf( "You are healthy" );
		}
		else
		{
			Printf( "%s does not require you to heal or cure them!", (LPCTSTR) pChar->GetName());
		}
		return( -CSkillDef::T_QTY );
	}

	if ( stage == CSkillDef::T_Fail )
	{
		// just consume the bandage on fail and give some credit for trying.
		pBandage->ConsumeAmount();

		if ( pChar != this )
		{
			pChar->Printf( "%s is attempting to apply bandages to %s, but has failed",
				(LPCTSTR) GetName(), (LPCTSTR) pCorpse->GetName() );
		}

		// Harm the creature ?
		return( -CSkillDef::T_Fail );
	}

	if ( stage == CSkillDef::T_Start )
	{
		if ( pChar != this )
		{
			CGString sMsg;
			sMsg.Format( "apply bandages to %s", (LPCTSTR) pChar->GetName());
			Emote( sMsg );
		}
		else
		{
			Emote( "apply bandages to self" );
		}
		if ( pCorpse )
		{
			// resurrect.
			return( 85 + Calc_GetRandVal(25));
		}
		if ( pChar->IsStatFlag( STATF_Poisoned ))
		{
			// level of the poison ?
			return( 50 + Calc_GetRandVal(50));
		}
		return( Calc_GetRandVal(80));
	}

	ASSERT( stage == CSkillDef::T_Success );

	pBandage->ConsumeAmount();

	CItemPtr pBloodyBandage = CItem::CreateScript(Calc_GetRandVal(2) ? ITEMID_BANDAGES_BLOODY1 : ITEMID_BANDAGES_BLOODY2, this );
	ItemBounce(pBloodyBandage);

	CSkillDefPtr pSkillDef = g_Cfg.GetSkillDef(Skill_GetActive());
	ASSERT(pSkillDef);
	int iSkillLevel = Skill_GetAdjusted( Skill_GetActive());

	if ( pCorpse )
	{
		CGString sText;
		sText.Format( "Resurrect %s", (LPCTSTR) pChar->GetName());

		if ( ! pChar->Spell_Effect_Resurrection( 0, pCorpse ))
		{
		}
		return 0;
	}
	if ( pChar->IsStatFlag( STATF_Poisoned ))
	{
		if ( ! pChar->Spell_Effect_Cure( iSkillLevel, true ))
			return( -1 );

		Printf( "You cure %s of poisons!", (LPCTSTR) (pChar == this) ? "yourself" : ( (LPCTSTR) pChar->GetName()));
		if ( pChar != this )
		{
			pChar->Printf( "%s has cured you of poisons!", (LPCTSTR) GetName());
		}
		return( 0 );
	}

	// LAYER_FLAG_Bandage
	pChar->Stat_Change( STAT_Health, pSkillDef->m_Effect.GetLinear(iSkillLevel));
	return( 0 );
}

int CChar::Skill_RemoveTrap( CSkillDef::T_TYPE_ stage )
{
	// m_Act.m_Targ = trap
	// Is it a trap ?

	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}
	CItemPtr pTrap = g_World.ItemFind(m_Act.m_Targ);
	if ( pTrap == NULL || ! pTrap->IsType(IT_TRAP))
	{
		WriteString( "You should use this skill to disable traps" );
		return( -CSkillDef::T_QTY );
	}
	if ( ! CanTouch(pTrap))
	{
		WriteString( "You can't reach it." );
		return( -CSkillDef::T_QTY );
	}
	if ( stage == CSkillDef::T_Start )
	{
		// How difficult ?
		return Calc_GetRandVal(95);
	}
	if ( stage == CSkillDef::T_Fail )
	{
		Use_Item( pTrap );	// set it off ?
		return 0;
	}
	if ( stage == CSkillDef::T_Success )
	{
		// disable it.
		pTrap->SetTrapState( IT_TRAP_INACTIVE, ITEMID_NOTHING, 5*60 );
		return 0;
	}
	ASSERT(0);
	return( -CSkillDef::T_QTY );
}

int CChar::Skill_Begging( CSkillDef::T_TYPE_ stage )
{
	// m_Act.m_Targ = Our begging target..

	CCharPtr pChar = g_World.CharFind(m_Act.m_Targ);
	if ( pChar == NULL || pChar == this )
	{
		return( -CSkillDef::T_QTY );
	}

	switch ( stage )
	{
	case CSkillDef::T_Start:
		Printf("You grovel at %s's feet", (LPCTSTR) pChar->GetName());
		return( pChar->m_StatInt );
	case CSkillDef::T_Stroke:
		if ( m_pNPC.IsValidNewObj())
			return -CSkillDef::T_Stroke;	// Keep it active.
		return( 0 );
	case CSkillDef::T_Fail:
		// Might they do something bad ?
		return( 0 );
	case CSkillDef::T_Success:
		// Now what ? Not sure how to make begging successful.
		// Give something from my inventory ?
		return( 0 );
	}

	ASSERT(0);
	return( -CSkillDef::T_QTY );
}

int CChar::Skill_Magery( CSkillDef::T_TYPE_ stage )
{
	// SKILL_MAGERY
	//  m_Act.m_pt = location to cast to.
	//  m_Act.m_TargPrv = the source of the spell.
	//  m_Act.m_Targ = target for the spell.
	//  m_atMagery.m_Spell = the spell.

	switch ( stage )
	{
	case CSkillDef::T_Start:
		// NOTE: this should call SetTimeout();
		return Spell_CastStart();
	case CSkillDef::T_Stroke:
		return( 0 );
	case CSkillDef::T_Fail:
		Spell_CastFail();
		return( 0 );
	case CSkillDef::T_Success:
		if ( ! Spell_CastDone())
		{
			return( -CSkillDef::T_Abort );
		}
		return( 0 );
	}

	ASSERT(0);
	return( -CSkillDef::T_Abort );
}

int CChar::Skill_Fighting( CSkillDef::T_TYPE_ stage )
{
	// SKILL_ARCHERY
	// m_Act.m_Targ = attack target.
	// RETURN:
	//  Difficulty against my skill.

	if ( stage == CSkillDef::T_Start )
	{
		// When do we get our next shot?

		DEBUG_CHECK( IsStatFlag( STATF_War ));

		m_Act.m_atFight.m_War_Swing_State = WAR_SWING_EQUIPPING;

		int iDifficulty = g_Cfg.Calc_CombatChanceToHit( this, Skill_GetActive(),
			g_World.CharFind(m_Act.m_Targ),
			g_World.ItemFind(m_uidWeapon));

		// Set the swing timer.
		int iWaitTime = Fight_GetWeaponSwingTimer()/2;	// start the anim immediately.
		if ( Skill_GetActive() == SKILL_ARCHERY )	// anim is funny for archery
			iWaitTime /= 2;
		SetTimeout( iWaitTime );

		return( iDifficulty );
	}

	if ( stage == CSkillDef::T_Stroke )
	{
		// Hit or miss my current target.
		if ( ! IsStatFlag( STATF_War ))
			return -CSkillDef::T_Abort;

		if ( m_Act.m_atFight.m_War_Swing_State != WAR_SWING_SWINGING )
		{
			m_Act.m_atFight.m_War_Swing_State = WAR_SWING_READY;  // Waited my recoil time. So I'm ready.
		}

		Fight_HitTry();	// this cleans up itself.
		return -CSkillDef::T_Stroke;	// Stay in the skill till we hit.
	}

	return -CSkillDef::T_QTY;
}

int CChar::Skill_MakeItem( CSkillDef::T_TYPE_ stage )
{
	// SKILL_BLACKSMITHING:
	// SKILL_BOWCRAFT:
	// SKILL_CARPENTRY:
	// SKILL_INSCRIPTION:
	// SKILL_TAILORING:
	// SKILL_TINKERING:
	//
	// m_Act.m_Targ = the item we want to be part of this process.
	// m_Act.m_atCreate.m_ItemID = new item we are making
	// m_Act.m_atCreate.m_Amount = amount of said item.

	if ( stage == CSkillDef::T_Start )
	{
		return m_Act.m_Difficulty;	// keep the already set difficulty
	}
	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}
	if ( stage == CSkillDef::T_Success )
	{
		if ( ! Skill_MakeItem( m_Act.m_atCreate.m_ItemID, m_Act.m_Targ, CSkillDef::T_Success ))
			return( -CSkillDef::T_Abort );
		return 0;
	}
	if ( stage == CSkillDef::T_Fail )
	{
		Skill_MakeItem( m_Act.m_atCreate.m_ItemID, m_Act.m_Targ, CSkillDef::T_Fail );
		return( 0 );
	}
	ASSERT(0);
	return( -CSkillDef::T_QTY );
}

int CChar::Skill_Tailoring( CSkillDef::T_TYPE_ stage )
{
	if ( stage == CSkillDef::T_Success )
	{
		Sound( SOUND_SNIP );	// snip noise
	}

	return( Skill_MakeItem( stage ));
}

int CChar::Skill_Inscription( CSkillDef::T_TYPE_ stage )
{
	if ( stage == CSkillDef::T_Start )
	{
		// Can we even attempt to make this scroll ?
		// m_Act.m_atCreate.m_ItemID = create this item
		Sound( 0x249 );

		// Stratics says you loose mana regardless of success or failure
		for( DWORD dwSpell = SPELL_Clumsy; dwSpell < SPELL_BOOK_QTY; dwSpell++ )
		{
			CSpellDefPtr pSpellDef = g_Cfg.GetSpellDef( (SPELL_TYPE)dwSpell );
			if ( pSpellDef == NULL )
				continue;
			if ( pSpellDef->m_idScroll == m_Act.m_atCreate.m_ItemID )
			{
				// Consume mana.
				Stat_Change( STAT_Mana, -pSpellDef->m_wManaUse );
			}
		}
	}

	return( Skill_MakeItem( stage ));
}

int CChar::Skill_Bowcraft( CSkillDef::T_TYPE_ stage )
{
	// SKILL_BOWCRAFT
	// m_Act.m_Targ = the item we want to be part of this process.
	// m_Act.m_atCreate.m_ItemID = new item we are making
	// m_Act.m_atCreate.m_Amount = amount of said item.

	Sound( 0x055 );
	UpdateAnimate( ANIM_SALUTE );

	if ( stage == CSkillDef::T_Start )
	{
		// Might be based on how many arrows to make ???
		m_Act.m_atCreate.m_Stroke_Count = Calc_GetRandVal( 2 ) + 1;
	}

	return( Skill_MakeItem( stage ));
}

int CChar::Skill_Blacksmith( CSkillDef::T_TYPE_ stage )
{
	// m_Act.m_atCreate.m_ItemID = create this item
	// m_Act.m_pt = the anvil.
	// m_Act.m_Targ = the hammer.

	m_Act.m_pt = g_World.FindItemTypeNearby( GetTopPoint(), IT_FORGE, 3 );
	if ( ! m_Act.m_pt.IsValidPoint())
	{
		WriteString( "You must be near a forge to smith" );
		return( -CSkillDef::T_QTY );
	}

	UpdateDir( m_Act.m_pt );	// toward the forge

	if ( stage == CSkillDef::T_Start )
	{
		m_Act.m_atCreate.m_Stroke_Count = Calc_GetRandVal( 4 ) + 2;
	}

	if ( stage == CSkillDef::T_Stroke )
	{
		Sound( 0x02a );
		if ( m_Act.m_atCreate.m_Stroke_Count <= 0 )
			return 0;

		// Keep trying and updating the animation
		m_Act.m_atCreate.m_Stroke_Count --;
		UpdateAnimate( ANIM_ATTACK_WEAPON );	// ANIM_ATTACK_1H_DOWN
		Skill_SetTimeout();
		return( -CSkillDef::T_Stroke );	// keep active.
	}

	return( Skill_MakeItem( stage ));
}

int CChar::Skill_Carpentry( CSkillDef::T_TYPE_ stage )
{
	// m_Act.m_Targ = the item we want to be part of this process.
	// m_Act.m_atCreate.m_ItemID = new item we are making
	// m_Act.m_atCreate.m_Amount = amount of said item.

	Sound( 0x23d );

	if ( stage == CSkillDef::T_Start )
	{
		// m_Act.m_atCreate.m_ItemID = create this item
		m_Act.m_atCreate.m_Stroke_Count = Calc_GetRandVal( 3 ) + 2;
	}

	if ( stage == CSkillDef::T_Stroke )
	{
		if ( m_Act.m_atCreate.m_Stroke_Count <= 0 )
			return 0;

		// Keep trying and updating the animation
		m_Act.m_atCreate.m_Stroke_Count --;
		UpdateAnimate( ANIM_ATTACK_WEAPON );
		Skill_SetTimeout();
		return( -CSkillDef::T_Stroke );	// keep active.
	}

	return( Skill_MakeItem( stage ));
}

int CChar::Skill_Information( CSkillDef::T_TYPE_ stage )
{
	// SKILL_ANIMALLORE:
	// SKILL_ARMSLORE:
	// SKILL_ANATOMY:
	// SKILL_ITEMID:
	// SKILL_EVALINT:
	// SKILL_FORENSICS:
	// SKILL_TASTEID:
	// Difficulty should depend on the target item !!!??
	// m_Act.m_Targ = target.

	if ( ! IsClient())	// purely informational
		return( -CSkillDef::T_QTY );

	if ( stage == CSkillDef::T_Fail )
	{
		return 0;
	}
	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}

	SKILL_TYPE skill = Skill_GetActive();
	int iSkillLevel = Skill_GetAdjusted(skill);

	if ( stage == CSkillDef::T_Start )
	{
		return GetClient()->OnSkill_Info( skill, m_Act.m_Targ, iSkillLevel, true );
	}
	if ( stage == CSkillDef::T_Success )
	{
		return GetClient()->OnSkill_Info( skill, m_Act.m_Targ, iSkillLevel, false );
	}

	ASSERT(0);
	return( -CSkillDef::T_QTY );
}

int CChar::Skill_Act_Napping( CSkillDef::T_TYPE_ stage )
{
	// NPCACT_Napping:
	// we are taking a small nap. keep napping til we wake. (or move)
	// AFK command

	if ( stage == CSkillDef::T_Start )
	{
		// we are taking a small nap.
		SetTimeout( 2*TICKS_PER_SEC );
		return( 0 );
	}

	if ( stage == CSkillDef::T_Stroke )
	{
		if ( m_Act.m_pt != GetTopPoint())
			return( -CSkillDef::T_QTY );	// we moved.
		SetTimeout( 8*TICKS_PER_SEC );
		Speak( "z", HUE_WHITE, TALKMODE_WHISPER );
		return -CSkillDef::T_Stroke;	// Stay in the skill till we hit.
	}

	return( -CSkillDef::T_QTY );	// something odd
}

int CChar::Skill_Act_Breath( CSkillDef::T_TYPE_ stage )
{
	// NPCACT_BREATH
	// A Dragon I assume.
	// m_Act.m_Targ = my target.

	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}
	if ( stage == CSkillDef::T_Fail )
	{
		return 0;
	}
	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}

	CCharPtr pChar = g_World.CharFind(m_Act.m_Targ);
	if ( pChar == NULL )
	{
		return -CSkillDef::T_QTY;
	}

	m_Act.m_pt = pChar->GetTopPoint();
	UpdateDir( m_Act.m_pt );

	if ( stage == CSkillDef::T_Start )
	{
		Stat_Change( STAT_Stam, -10 );
		UpdateAnimate( ANIM_MON_Stomp, false );
		SetTimeout( 3*TICKS_PER_SEC );
		return 0;
	}

	ASSERT( stage == CSkillDef::T_Success );

	CPointMap pntMe = GetTopPoint();
	if ( pntMe.GetDist( m_Act.m_pt ) > SPHEREMAP_VIEW_SIGHT )
	{
		m_Act.m_pt.StepLinePath( pntMe, SPHEREMAP_VIEW_SIGHT );
	}

	Sound( 0x227 );
	int iDamage = m_StatStam/4 + Calc_GetRandVal( m_StatStam/4 );
	g_World.Explode( this, m_Act.m_pt, 3, iDamage, DAMAGE_FIRE | DAMAGE_GENERAL );
	return( 0 );
}

int CChar::Skill_Act_Looting( CSkillDef::T_TYPE_ stage )
{
	// NPCACT_LOOTING
	// m_Act.m_Targ = the item i want.

	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}
	if ( stage == CSkillDef::T_Start )
	{
		if ( m_Act.m_atLooting.m_iDistCurrent == 0 )
		{
			CSphereExpArgs Args( this, this, (CResourceObj*)(CItem*)g_World.ItemFind(m_Act.m_Targ));
			if ( OnTrigger( CCharDef::T_NPCSeeWantItem, Args ) == TRIGRET_RET_VAL )
				return( false );
		}
		SetTimeout( 1*TICKS_PER_SEC );
		return 0;
	}

	return( -CSkillDef::T_QTY );
}

int CChar::Skill_Act_Throwing( CSkillDef::T_TYPE_ stage )
{
	// NPCACT_THROWING
	// m_Act.m_Targ = my target.

	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}
	CCharPtr pChar = g_World.CharFind(m_Act.m_Targ);
	if ( pChar == NULL )
	{
		return( -CSkillDef::T_QTY );
	}

	m_Act.m_pt = pChar->GetTopPoint();
	UpdateDir( m_Act.m_pt );

	if ( stage == CSkillDef::T_Start )
	{
		Stat_Change( STAT_Stam, -( 4 + Calc_GetRandVal(6)));
		UpdateAnimate( ANIM_MON_Stomp );
		return 0;
	}

	if ( stage != CSkillDef::T_Success )
	{
		return( -CSkillDef::T_QTY );
	}

	CPointMap pntMe = GetTopPoint();
	if ( pntMe.GetDist( m_Act.m_pt ) > SPHEREMAP_VIEW_SIGHT )
	{
		m_Act.m_pt.StepLinePath( pntMe, SPHEREMAP_VIEW_SIGHT );
	}
	SoundChar( CRESND_GETHIT );

	// a rock or a boulder ?
	ITEMID_TYPE id;
	int iDamage;
	if ( ! Calc_GetRandVal( 3 ))
	{
		iDamage = m_StatStam/4 + Calc_GetRandVal( m_StatStam/4 );
		id = (ITEMID_TYPE)( ITEMID_ROCK_B_LO + Calc_GetRandVal(ITEMID_ROCK_B_HI-ITEMID_ROCK_B_LO));
	}
	else
	{
		iDamage = 2 + Calc_GetRandVal( m_StatStam/4 );
		id = (ITEMID_TYPE)( ITEMID_ROCK_2_LO + Calc_GetRandVal(ITEMID_ROCK_2_HI-ITEMID_ROCK_2_LO));
	}

	CItemPtr pRock = CItem::CreateScript(id,this);
	ASSERT(pRock);
	pRock->SetAttr(ATTR_CAN_DECAY);
	// pRock->MoveNear( m_Act.m_pt, Calc_GetRandVal(2));
	pRock->MoveToCheck( m_Act.m_pt, this );
	pRock->Effect( EFFECT_BOLT, id, this );

	// did it hit ?
	if ( ! Calc_GetRandVal( pChar->GetTopPoint().GetDist( m_Act.m_pt )))
	{
		pChar->OnTakeDamage( iDamage, this, DAMAGE_HIT_BLUNT );
	}

	return( 0 );
}

int CChar::Skill_Act_Training( CSkillDef::T_TYPE_ stage )
{
	// NPCACT_TRAINING
	// finished some traing maneuver.

	if ( stage == CSkillDef::T_Start )
	{
		SetTimeout( 1*TICKS_PER_SEC );
		return 0;
	}
	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}
	if ( stage != CSkillDef::T_Success )
	{
		return( -CSkillDef::T_QTY );
	}

	if ( m_Act.m_TargPrv == m_uidWeapon )
	{
		CItemPtr pItem = g_World.ItemFind(m_Act.m_Targ);
		if ( pItem )
		{
			switch ( pItem->GetType())
			{
			case IT_TRAIN_DUMMY:	// Train dummy.
				Use_Train_Dummy(pItem, false);
				break;
			case IT_TRAIN_PICKPOCKET:
				Use_Train_PickPocketDip(pItem, false);
				break;
			case IT_ARCHERY_BUTTE:	// Archery Butte
				Use_Train_ArcheryButte(pItem, false);
				break;
			}
		}
	}

	return 0;
}

//************************************
// General skill stuff.

int CChar::Skill_Stage( CSkillDef::T_TYPE_ stage )
{
	// Call Triggers

	SKILL_TYPE skill = Skill_GetActive();
	if ( skill == SKILL_NONE )
		return -1;

	CSphereExpArgs execArgs( this, this, skill, 0, 0 );

	TRIGRET_TYPE iTrigRet = OnTrigger( (CCharDef::T_TYPE_)( CCharDef::T_SkillAbort + ( stage - CSkillDef::T_Abort )), execArgs );
	if ( iTrigRet == TRIGRET_RET_VAL )
	{
		return execArgs.m_vValRet;
	}

	CSkillDefWPtr pSkillDef = g_Cfg.GetSkillDefW( skill);
	if ( pSkillDef )
	{
		// RES_Skill
		//CCharActState SaveState = m_Act;
		iTrigRet = pSkillDef->OnTriggerScript( execArgs, stage, CSkillDef::sm_Triggers[stage].m_pszName );
		// m_Act = SaveState;
		if ( iTrigRet == TRIGRET_RET_VAL )
		{
			// They handled success, just clean up, don't do skill experience
			return execArgs.m_vValRet;
		}
	}

	switch ( skill)
	{
	case SKILL_NONE:	// idling.
		return 0;
	case SKILL_ALCHEMY:
		return Skill_Alchemy(stage);
	case SKILL_ANATOMY:
	case SKILL_ANIMALLORE:
	case SKILL_ITEMID:
	case SKILL_ARMSLORE:
		return Skill_Information(stage);
	case SKILL_PARRYING:
		return 0;
	case SKILL_BEGGING:
		return Skill_Begging(stage);
	case SKILL_BLACKSMITHING:
		return Skill_Blacksmith(stage);
	case SKILL_BOWCRAFT:
		return Skill_Bowcraft(stage);
	case SKILL_PEACEMAKING:
		return Skill_Peacemaking(stage);
	case SKILL_CAMPING:
		return 0;
	case SKILL_CARPENTRY:
		return Skill_Carpentry(stage);
	case SKILL_CARTOGRAPHY:
		return Skill_Cartography(stage);
	case SKILL_COOKING:
		return Skill_Cooking(stage);
	case SKILL_DETECTINGHIDDEN:
		return Skill_DetectHidden(stage);
	case SKILL_ENTICEMENT:
		return Skill_Enticement(stage);
	case SKILL_EVALINT:
		return Skill_Information(stage);
	case SKILL_HEALING:
		return Skill_Healing(stage);
	case SKILL_FISHING:
		return Skill_Fishing(stage);
	case SKILL_FORENSICS:
		return Skill_Information(stage);
	case SKILL_HERDING:
		return Skill_Herding(stage);
	case SKILL_HIDING:
		return Skill_Hiding(stage);
	case SKILL_PROVOCATION:
		return Skill_Provocation(stage);
	case SKILL_INSCRIPTION:
		return Skill_Inscription(stage);
	case SKILL_LOCKPICKING:
		return Skill_Lockpicking(stage);
	case SKILL_MAGERY:
		return Skill_Magery(stage);
	case SKILL_MAGICRESISTANCE:
		return 0;
	case SKILL_TACTICS:
		return 0;
	case SKILL_SNOOPING:
		return Skill_Snooping(stage);
	case SKILL_MUSICIANSHIP:
		return Skill_Musicianship(stage);
	case SKILL_POISONING:	// 30
		return Skill_Poisoning(stage);
	case SKILL_ARCHERY:
		return Skill_Fighting(stage);
	case SKILL_SPIRITSPEAK:
		return Skill_SpiritSpeak(stage);
	case SKILL_STEALING:
		return Skill_Stealing(stage);
	case SKILL_TAILORING:
		return Skill_Tailoring(stage);
	case SKILL_TAMING:
		return Skill_Taming(stage);
	case SKILL_TASTEID:
		return Skill_Information(stage);
	case SKILL_TINKERING:
		return Skill_MakeItem(stage);
	case SKILL_TRACKING:
		return Skill_Tracking(stage);
	case SKILL_VETERINARY:
		return Skill_Healing(stage);
	case SKILL_SWORDSMANSHIP:
	case SKILL_MACEFIGHTING:
	case SKILL_FENCING:
	case SKILL_WRESTLING:
		return Skill_Fighting(stage);
	case SKILL_LUMBERJACKING:
		return Skill_Lumberjack(stage);
	case SKILL_MINING:
		return Skill_Mining(stage);
	case SKILL_MEDITATION:
		return Skill_Meditation(stage);
	case SKILL_Stealth:
		return Skill_Hiding(stage);
	case SKILL_RemoveTrap:
		return Skill_RemoveTrap(stage);
	case SKILL_NECROMANCY:
		return Skill_Magery(stage);

	case NPCACT_BREATH:
		return Skill_Act_Breath(stage);
	case NPCACT_LOOTING:
		return Skill_Act_Looting(stage);
	case NPCACT_THROWING:
		return Skill_Act_Throwing(stage);
	case NPCACT_TRAINING:
		return Skill_Act_Training(stage);
	case NPCACT_Napping:
		return Skill_Act_Napping(stage);

	default:
		if ( ! IsSkillBase( skill))
		{
			DEBUG_CHECK( IsSkillNPC( skill));
			if ( stage == CSkillDef::T_Stroke )
				return( -CSkillDef::T_Stroke ); // keep these active. (NPC modes)
			return 0;
		}
	}

	WriteString( "Skill not implemented!" );
	return -CSkillDef::T_QTY;
}

void CChar::Skill_Fail( bool fCancel )
{
	// This is the normal skill check failure.
	// Other types of failure don't come here.
	//
	// ARGS:
	//	fCancel = no credt.
	//  else We still get some credit for having tried.

	SKILL_TYPE skill = Skill_GetActive();
	if ( skill == SKILL_NONE )
		return;

	if ( ! IsSkillBase(skill))
	{
		DEBUG_CHECK( IsSkillNPC(skill));
		Skill_Cleanup();
		return;
	}

	if ( m_Act.m_Difficulty > 0 )
	{
		m_Act.m_Difficulty = - m_Act.m_Difficulty;
	}

	if ( Skill_Stage( CSkillDef::T_Fail ) >= 0 )
	{
		// Get some experience for failure ?
		Skill_Experience( skill, m_Act.m_Difficulty );
	}

	Skill_Cleanup();
}

int CChar::Skill_Done()
{
	// We just finished using a skill. ASYNC timer expired.
	// m_Act_Skill = the skill.
	// Consume resources that have not already been consumed.
	// Confer the benefits of the skill.
	// calc skill gain based on this.
	//
	// RETURN: Did we succeed or fail ?
	//   0 = success
	//	 -CSkillDef::T_Stroke = stay in skill. (stroke)
	//   -CSkillDef::T_Fail = we must print the fail msg. (credit for trying)
	//   -CSkillDef::T_Abort = we must print the fail msg. (But get no credit, canceled )
	//   -CSkillDef::T_QTY = special failure. clean up the skill but say nothing. (no credit)

	SKILL_TYPE skill = Skill_GetActive();
	if ( skill == SKILL_NONE )	// we should not be coming here (timer should not have expired)
		return -CSkillDef::T_QTY;

	// multi stroke tried stuff here first.
	// or stuff that never really fails.
	int iRet = Skill_Stage(CSkillDef::T_Stroke);
	if ( iRet < 0 )
		return( iRet );
	if ( m_Act.m_Difficulty < 0 && !IsGM())
	{
		// Was Bound to fail. But we had to wait for the timer anyhow.
		return -CSkillDef::T_Fail;
	}

	// Success for the skill.
	iRet = Skill_Stage(CSkillDef::T_Success);
	if ( iRet < 0 )
		return iRet;

	// Success = Advance the skill
	Skill_Experience( skill, m_Act.m_Difficulty );
	Skill_Cleanup();
	return( -CSkillDef::T_Success );
}

bool CChar::Skill_Wait( SKILL_TYPE skilltry )
{
	// Some sort of push button skill.
	// We want to do some new skill. Can we ?
	// If this is the same skill then tell them to wait.

	if ( IsStatFlag( STATF_DEAD | STATF_Sleeping | STATF_Freeze | STATF_Stone ))
	{
		WriteString( "You can't do much in your current state." );
		return( true );
	}

	SKILL_TYPE skill = Skill_GetActive();
	if ( skill == SKILL_NONE )	// not currently doing anything.
	{
		Reveal();
		return( false );
	}

	// What if we are in combat mode ?
	if ( IsStatFlag( STATF_War ))
	{
		WriteString( "You are preoccupied with thoughts of battle." );
		return( true );
	}

	// Passive skills just cancel.
	// SKILL_SPIRITSPEAK ?
	if ( skilltry != skill )
	{
		if ( skill == SKILL_MEDITATION ||
			skill == SKILL_HIDING ||
			skill == SKILL_Stealth )
		{
			Skill_Fail( true );
			return( false );
		}
	}

	WriteString( "You must wait to perform another action" );
	return ( true );
}

bool CChar::Skill_Start( SKILL_TYPE skill, int iDifficulty )
{
	// We have all the info we need to do the skill. (targeting etc)
	// Set up how long we have to wait before we get the desired results from this skill.
	// Set up any animations/sounds in the mean time.
	// Calc if we will succeed or fail.
	// ARGS:
	//  iDifficulty = 0-100
	// RETURN:
	//  false = failed outright with no wait. "You have no chance of taming this"

	if ( g_Serv.IsLoading())
	{
		if ( skill != SKILL_NONE &&
			! IsSkillBase(skill) &&
			! IsSkillNPC(skill))
		{
			DEBUG_ERR(( "UID:0%x Bad Skill %d for '%s'" LOG_CR, GetUID(), skill, (LPCTSTR) GetName()));
			return( false );
		}
		m_Act.m_SkillCurrent = skill;
		return( true );
	}

	if ( Skill_GetActive() != SKILL_NONE )
	{
		Skill_Fail( true );	// Fail previous skill unfinished. (with NO skill gain!)
	}

	if ( skill != SKILL_NONE )
	{
		m_Act.m_SkillCurrent = skill;	// Start using a skill.
		m_Act.m_Difficulty = iDifficulty;

		ASSERT( IsSkillBase(skill) || IsSkillNPC(skill));

		// Some skill can start right away. Need no targeting.
		// 0-100 scale of Difficulty
		m_Act.m_Difficulty = Skill_Stage(CSkillDef::T_Start);
		if ( m_Act.m_Difficulty < 0 )
		{
			Skill_Cleanup();
			return(false);
		}

		if ( IsSkillBase(skill))
		{
			CSkillDefPtr pSkillDef = g_Cfg.GetSkillDef(skill);
			ASSERT(pSkillDef);
			int iWaitTime = pSkillDef->m_Delay.GetLinear( Skill_GetBase(skill) );
			if ( iWaitTime )
			{
				// How long before complete skill.
				SetTimeout( iWaitTime );
			}
		}
		if ( IsTimerExpired())
		{
			// the skill should have set it's own delay!?
			SetTimeout( 1 );
		}
		if ( m_Act.m_Difficulty > 0 )
		{
			if ( ! Skill_CheckSuccess( skill, m_Act.m_Difficulty ))
				m_Act.m_Difficulty = - m_Act.m_Difficulty; // will result in Failure ?
		}
	}

	// emote the action i am taking.
	if ( g_Cfg.m_wDebugFlags & DEBUGF_NPC_EMOTE )
	{
		Emote( Skill_GetName(true));
	}

	return( true );
}


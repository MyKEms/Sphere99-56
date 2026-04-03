//
// CCharFight.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
// Fight/Criminal actions/Noto.
// OnTakeDamage

#include "stdafx.h"	// predef header.

#define ARCHERY_DIST_MAX 12

//////////////////////////////////////////////////////////////////////////////

void CChar::OnNoticeCrime( CChar* pCriminal, const CChar* pCharMark )
{
	// I noticed a crime.
	ASSERT(pCriminal);
	if ( pCriminal == this )
		return;

	// NPCBRAIN_BESERK creatures cause criminal fault on the part of their masters.
	if ( pCriminal->m_pNPC.IsValidNewObj() && 
		pCriminal->m_pNPC->m_Brain == NPCBRAIN_BESERK )
	{
		CCharPtr pOwner = pCriminal->NPC_PetGetOwner();
		if ( pOwner != NULL && pOwner != this )
		{
			OnNoticeCrime( pOwner, pCharMark );
		}
	}

	if ( m_pPlayer.IsValidNewObj())
	{
		// I have the option of attacking the criminal. or calling the guards.
		Memory_AddObjTypes( pCriminal, MEMORY_SAWCRIME );
		return;
	}

	// NPC's can take other actions.

	ASSERT(m_pNPC.IsValidNewObj());
	bool fMyMaster = NPC_IsOwnedBy( pCriminal );

	if ( this != pCharMark )	// it's not me.
	{
		// Thieves and beggars don't care.
		if ( m_pNPC->m_Brain == NPCBRAIN_THIEF || m_pNPC->m_Brain == NPCBRAIN_BEGGAR )
			return;
		if ( fMyMaster )	// I won't rat you out.
			return;
		// Or if the target is evil ?

		// Or if I am evil.
	}
	else
	{
		// I being the victim can retaliate.
		Memory_AddObjTypes( pCriminal, MEMORY_SAWCRIME );
		OnHarmedBy( pCriminal, 1 );
	}

	// Alert the guards !!!?
	if ( ! NPC_CanSpeak())
		return;	// I can't talk anyhow.

	pCriminal->Noto_Criminal();

	if ( GetCreatureType() != NPCBRAIN_HUMAN )
	{
		// Good monsters don't call for guards outside guarded areas.
		if ( ! m_pArea.IsValidRefObj() || ! m_pArea->IsFlagGuarded())
			return;
	}

	if ( m_pNPC->m_Brain != NPCBRAIN_GUARD )
	{
		Speak( "Help! Guards a Criminal!" );
	}

	// Find a guard.
	CallGuards( pCriminal );
}

bool CChar::CheckCrimeSeen( SKILL_TYPE SkillToSee, CChar* pCharMark, const CObjBase* pItem, LPCTSTR pAction )
{
	// I am commiting a crime.
	// Did others see me commit or try to commit the crime.
	//  SkillToSee = NONE = everyone can notice this.
	// RETURN:
	//  true = somebody saw me.

	bool fSeen = false;

	// Who notices ?
	CWorldSearch AreaChars( GetTopPoint(), SPHEREMAP_VIEW_SIGHT );
	for(;;)		
	{
		CCharPtr pChar = AreaChars.GetNextChar();
		if ( pChar == NULL )
			break;
		if ( pChar == this )
			continue;	// I saw myself before.
		if ( ! pChar->CanSeeLOS( this ))
			continue;

		bool fYour = ( pCharMark == pChar );
		if ( ! g_Cfg.Calc_CrimeSeen( this, pChar, SkillToSee, fYour ))
			continue;

		CGString sMsg;
		if ( pAction != NULL )
		{
			if ( pItem->IsChar())
			{
				sMsg.Format( "You notice %s %s %s against %s", (LPCTSTR) GetName(), pAction, (LPCTSTR) pCharMark->GetName(), (LPCTSTR) pItem->GetName());
			}
			else if ( pCharMark == NULL )
			{
				sMsg.Format( "You notice %s %s %s.", (LPCTSTR) GetName(), pAction, (LPCTSTR) pItem->GetName());
			}
			else
			{
				sMsg.Format( "You notice %s %s %s%s %s",
					(LPCTSTR) GetName(), pAction, fYour ? "your" : (LPCTSTR) pCharMark->GetName(),
					fYour ? "" : "'s",
					(LPCTSTR) pItem->GetName());
			}
			pChar->ObjMessage( sMsg, this );
		}

		// If a GM sees you it is not a crime.
		if ( pChar->IsGM())
			continue;
		fSeen = true;

		// They are not a criminal til someone calls the guards !!!
		if ( SkillToSee == SKILL_SNOOPING )
		{
			// Off chance of being a criminal. (hehe)
			if ( ! Calc_GetRandVal( g_Cfg.m_iSnoopCriminal ))
			{
				pChar->OnNoticeCrime( this, pCharMark );
			}
			if ( pChar->m_pNPC.IsValidNewObj())
			{
				pChar->NPC_OnNoticeSnoop( this, pCharMark );
			}
		}
		else
		{
			pChar->OnNoticeCrime( this, pCharMark );
		}
	}
	return( fSeen );
}

bool CChar::Skill_Snoop_Check( const CItemContainer* pItem )
{
	// Assume the container is not locked.
	// return: true = snoop or can't open at all.
	//  false = instant open.

	if ( pItem == NULL )
		return( true );

	ASSERT( pItem->IsItem());

	if ( ! IsGM())
		switch ( pItem->GetType())
	{
	case IT_SHIP_HOLD_LOCK:
	case IT_SHIP_HOLD:
		// Must be on board a ship to open the hatch.
		if ( ! m_pArea || m_pArea->GetUIDIndex() != pItem->m_uidLink )
		{
			WriteString( _TEXT("You can only open the hatch on board the ship"));
			return( true );
		}
		break;
	case IT_EQ_BANK_BOX:
		// Some sort of cheater.
		return( false );
	}

	CCharPtr pCharMark;
	if ( ! IsTakeCrime( pItem, &pCharMark ) || pCharMark == NULL )
		return( false );

	DEBUG_CHECK( ! IsGM());
	if ( Skill_Wait(SKILL_SNOOPING))
		return( true );

	m_Act.m_Targ = pItem->GetUID();
	Skill_Start( SKILL_SNOOPING );
	return( true );
}

int CChar::Skill_Snooping( CSkillDef::T_TYPE_ stage )
{
	// SKILL_SNOOPING
	// m_Act.m_Targ = object to snoop into.
	// RETURN:
	// -CSkillDef::T_QTY = no chance. and not a crime
	// -CSkillDef::T_Fail = no chance and caught.
	// 0-100 = difficulty = percent chance of failure.

	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}

	// Assume the container is not locked.
	CItemPtr pCont2 = g_World.ItemFind( m_Act.m_Targ );
	CItemContainerPtr pCont = REF_CAST(CItemContainer,pCont2);
	if ( pCont == NULL )
	{
		return( -CSkillDef::T_QTY );
	}

	CCharPtr pCharMark;
	if ( ! IsTakeCrime( pCont, &pCharMark ) || pCharMark == NULL )
	{
		// Not a crime really.
		return( 0 );
	}

	DEBUG_CHECK( ! IsGM());
	if ( ! CanTouch( pCont ))
	{
		WriteString( "You can't reach it." );
		return( -CSkillDef::T_QTY );
	}

	if ( GetTopDist3D( pCharMark ) > 2 )
	{
		WriteString( "Your mark is too far away." );
		return( -CSkillDef::T_QTY );
	}

	PLEVEL_TYPE plevel = (PLEVEL_TYPE) GetPrivLevel();
	bool fForceFail = ( plevel < pCharMark->GetPrivLevel());
	if ( stage == CSkillDef::T_Start )
	{
		if ( fForceFail )
			return( -CSkillDef::T_Fail );

		if ( plevel >= PLEVEL_Counsel && plevel > pCharMark->GetPrivLevel())	// i'm higher priv.
			return( 0 );

		// return the difficulty.

		return( pCharMark->m_StatDex );
	}

	if ( fForceFail )
	{
		stage = CSkillDef::T_Fail;
	}

	// did anyone see this ?

	if ( CheckCrimeSeen( SKILL_SNOOPING, pCharMark, pCont, (stage == CSkillDef::T_Fail)? "attempting to peek into" : "peeking into" ))
	{
		Noto_KarmaChangeMessage( -10, -500 );
	}

	//
	// View the container.
	//
	if ( stage == CSkillDef::T_Success )
	{
		if ( IsClient())
		{
			m_pClient->addContainerSetup( pCont );
		}
	}
	return( 0 );
}

int CChar::Skill_Stealing( CSkillDef::T_TYPE_ stage )
{
	// m_Act.m_Targ = object to steal.
	// RETURN:
	// -CSkillDef::T_QTY = no chance. and not a crime
	// -CSkillDef::T_Fail = no chance and caught.
	// 0-100 = difficulty = percent chance of failure.
	//

	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}

	CCharPtr pCharMark;
	CItemPtr pItem = g_World.ItemFind( m_Act.m_Targ );
	if ( pItem == NULL )	// on a chars head ? = random steal.
	{
		pCharMark = g_World.CharFind(m_Act.m_Targ);
		if ( pCharMark == NULL )
		{
			WriteString( "Nothing to steal here." );
			return( -CSkillDef::T_QTY );
		}
		CItemContainerPtr pPack = pCharMark->GetPack();
		if ( pPack == NULL )
		{
		cantsteal:
			WriteString( "They have nothing to steal" );
			return( -CSkillDef::T_QTY );
		}
		pItem = pPack->ContentFindRandom();
		if ( pItem == NULL )
		{
			goto cantsteal;
		}
		m_Act.m_Targ = pItem->GetUID();
	}

	if ( pItem->IsType(IT_TRAIN_PICKPOCKET))
	{
		WriteString( "Just dclick this to practice stealing" );
		return -CSkillDef::T_QTY;
	}
	if ( pItem->IsType( IT_GAME_PIECE ))
	{
		return -CSkillDef::T_QTY;
	}
	if ( ! CanTouch( pItem ))
	{
		WriteString( "You can't reach it." );
		return( -CSkillDef::T_Abort );
	}
	if ( ! CanMove( pItem ) ||
		! CanCarry( pItem ))
	{
		WriteString( "That is too heavy." );
		return( -CSkillDef::T_Abort );
	}
	if ( ! IsTakeCrime( pItem, & pCharMark ))
	{
		WriteString( "No need to steal this" );

		// Just pick it up ?
		return( -CSkillDef::T_QTY );
	}
	if ( m_pArea->IsFlag(REGION_FLAG_SAFE))
	{
		WriteString( "No stealing is possible here." );
		return( -CSkillDef::T_QTY );
	}

	Reveal();	// If we take an item off the ground we are revealed.

	bool fGround = false;
	if ( pCharMark != NULL )
	{
		if ( GetTopDist3D( pCharMark ) > 2 )
		{
			WriteString( "Your mark is too far away." );
			return -CSkillDef::T_QTY;
		}
		if ( m_pArea->IsFlag(REGION_FLAG_NO_PVP) && 
			pCharMark->m_pPlayer && 
			! IsGM())
		{
			WriteString( "Can't harm other players here." );
			return( -1 );
		}
		if ( GetPrivLevel() < pCharMark->GetPrivLevel())
		{
			return -CSkillDef::T_Fail;
		}
		if ( stage == CSkillDef::T_Start )
		{
			return g_Cfg.Calc_StealingItem( this, pItem, pCharMark );
		}
	}
	else
	{
		// stealing off the ground should always succeed.
		// it's just a matter of getting caught.
		if ( stage == CSkillDef::T_Start )
		{
			return 1;	// town stuff on the ground is too easy.
		}
		fGround = true;
	}

	// Deliver the goods.

	if ( stage == CSkillDef::T_Success || fGround )
	{
		pItem->ClrAttr(ATTR_OWNED);	// Now it's mine
		CItemContainerPtr pPack = GetPack();
		if ( pItem->GetParent() != pPack && pPack )
		{
			pItem->RemoveFromView();
			// Put in my invent.
			pPack->ContentAdd( pItem );
		}
	}

	if ( m_Act.m_Difficulty == 0 )
		return( 0 );	// Too easy to be bad. hehe

	// You should only be able to go down to -1000 karma by stealing.
	if ( CheckCrimeSeen( SKILL_STEALING, pCharMark, pItem, (stage == CSkillDef::T_Fail)? "attempting to steal" : "stealing" ))
	{
		Noto_KarmaChangeMessage( -100, -1000 );
	}
	return( 0 );
}

void CChar::CallGuards( CChar* pCriminal )
{
	// I just yelled for guards.

	if ( ! m_pArea.IsValidRefObj())
		return;

	// Is there anything for guards to see ?
	if ( pCriminal == NULL )
	{
		CWorldSearch AreaCrime( GetTopPoint(), SPHEREMAP_VIEW_SIGHT );
		for(;;)
		{
			CCharPtr pChar = AreaCrime.GetNextChar();
			if ( pChar == NULL )
				break;

			// never respond if a criminal can't be found.
			// Only players call guards this way.
			// NPC's flag criminal instantly.
			if ( m_pPlayer.IsValidNewObj() &&
				Memory_FindObjTypes( pChar, MEMORY_SAWCRIME ))
			{
				pChar->Noto_Criminal();
			}

			if ( ! pChar->Noto_IsCriminal())
				continue;

			// can't respond to guardcalls if the criminal is outside a guarded zone.
			if ( ! pChar->m_pArea->IsFlagGuarded())
				continue;

			pCriminal = pChar;
		}

		if ( pCriminal == NULL )
			return;
	}

	// Am i in guarded zone ?
	// Guards can't respond if you are outside.
	ASSERT(pCriminal->GetTopPoint().IsValidPoint());
	if ( ! pCriminal->m_pArea->IsFlagGuarded())
		return;

	// Is there a free guard near by ?
	CSphereUID PrevGuardTarg;
	CCharPtr pGuard = NULL;
	CWorldSearch AreaGuard( GetTopPoint(), SPHEREMAP_VIEW_RADAR );
	for(;;)
	{
		pGuard = AreaGuard.GetNextChar();
		if ( pGuard == NULL )
		{
			break;
		}
		if ( pGuard->m_pPlayer.IsValidNewObj())
			continue;
		ASSERT(pGuard->m_pNPC.IsValidNewObj());
		if ( pGuard->m_pNPC->m_Brain != NPCBRAIN_GUARD )
			continue;
		if ( pGuard->IsStatFlag( STATF_War ))
		{
			if ( PrevGuardTarg.IsValidRID() && PrevGuardTarg == pGuard->m_Act.m_Targ )	// take him off this case.
				break;
			PrevGuardTarg = pGuard->m_Act.m_Targ;
			continue;	// busy.
		}
		break;	// we have a free guard.
	}

	if ( pGuard == NULL )
	{
		// Spawn a new guard.
		CSphereUID rid;
		CVarDefPtr pVar = m_pArea->m_TagDefs.FindKeyPtr("GUARDID");
		if ( pVar )
		{
			rid = CSphereUID( RES_CharDef, pVar->GetDWORD());
		}
		else
		{
			rid = g_Cfg.ResourceGetIDType( RES_CharDef, "GUARDS" );
		}
		if ( ! rid.IsValidRID())
			return;
		if ( rid.GetResIndex() == CREID_INVALID )
			return;
		pGuard = CChar::CreateNPC( (CREID_TYPE) rid.GetResIndex());
		ASSERT(pGuard);

		// Spawned guards should go away after x minutes.
		CItemPtr pSpell = pGuard->Spell_Equip_Create( SPELL_Summon, LAYER_SPELL_Summon, 1000, g_Cfg.m_iGuardLingerTime, pCriminal, true );
		ASSERT(pSpell);

		pGuard->Spell_Effect_Teleport( pCriminal->GetTopPoint(), false, false );
	}

	pGuard->NPC_LookAtCharGuard( pCriminal );
}

void CChar::OnHelpedBy( CChar* pCharSrc )
{
	// Helping evil is alway criminal!
	if ( Noto_IsEvil())
	{
		pCharSrc->Noto_Criminal();
		return;
	}
	if ( g_Cfg.m_fHelpingCriminalsIsACrime )
	{
		if ( Noto_GetFlag( pCharSrc, false ) > NOTO_NEUTRAL )
		{
			pCharSrc->Noto_Criminal();
			return;
		}
	}
}

void CChar::OnHarmedBy( CChar* pCharSrc, int iHarmQty )
{
	// i notice a Crime or attack against me ..
	// Actual harm has taken place.
	// Attack back if i'm not doing something else..

	bool fFightActive = IsStatFlag( STATF_War );

	// record that it is ok to attack them back.
	CItemMemoryPtr pMemory = Memory_AddObjTypes( pCharSrc, MEMORY_HARMEDBY );
	ASSERT( pMemory );
	pMemory->ChangeMotivation(-iHarmQty);

	if ( fFightActive && g_World.CharFind(m_Act.m_Targ))
	{
		// In war mode already
		if ( m_pPlayer )
			return;
		if ( Calc_GetRandVal( 10 ))
			return;
		// NPC will Change targets.
	}

	bool fMyMaster = NPC_IsOwnedBy( pCharSrc );
	if ( fMyMaster )
	{
		// I Quit !
		NPC_PetDesert();
	}

	// I will Auto-Defend myself.
	// If i'm not doing anything else.
	if ( ! IsStatFlag( STATF_War ) || 
		Skill_GetActive() == SKILL_NONE )
	{
		Fight_Attack( pCharSrc );
		if ( ! fFightActive )	// change to auto defend puts us in war mode.
		{
			UpdateMode();
		}
	}
}

bool CChar::Fight_Stunned( DIR_TYPE dir )
{
	// I have been hit and stunned.
	// Get thrown back if possible.
	// NPCACT_Stunned

#if 0

	CanMoveTo()

#endif

	return( false );
}

bool CChar::OnAttackedBy( CChar* pCharSrc, int iHarmQty, bool fCommandPet )
{
	// We have been attacked in some way by this CChar.
	// Might not actually be doing any real damage. (yet)
	//
	// They may have just commanded their pet to attack me.
	// Cast a bad spell at me.
	// Fired projectile at me.
	// Attempted to provoke me ?
	//
	// RETURN: 
	//  true = ok.
	//  false = we are immune to this char ! (or they to us)

	if ( pCharSrc == NULL )
		return true;	// field spell ?
	if ( pCharSrc == this )
		return true;	// self induced

	pCharSrc->Reveal();	// fix invis exploit

	if ( pCharSrc->IsStatFlag( STATF_INVUL ) && ! pCharSrc->IsGM())
	{
		// Can't do any damage either.
		pCharSrc->WriteString( "The attack is magically blocked" );
		return( false );
	}

	// Am i already attacking the source anyhow
	if ( Fight_IsActive() && m_Act.m_Targ == pCharSrc->GetUID())
		return true;

	Memory_AddObjTypes( pCharSrc, MEMORY_HARMEDBY|MEMORY_IRRITATEDBY );

	// Are they a criminal for it ? Is attacking me a crime ?
	if ( Noto_GetFlag(pCharSrc) == NOTO_GOOD )
	{
		if ( IsClient())
		{
			// I decide if this is a crime.
			OnNoticeCrime( pCharSrc, this );
		}
		else
		{
			// If it is a pet then this a crime others can report.
			pCharSrc->CheckCrimeSeen( SKILL_NONE, this, NULL, NULL );
		}
	}

	if ( ! fCommandPet )
	{
		// possibly retaliate. (auto defend)
		OnHarmedBy( pCharSrc, iHarmQty );
	}

	return( true );
}

int CChar::CalcArmorDefense( void ) const
{
	// When armor is added or subtracted check this.
	// This is the general AC number printed.
	// Tho not really used to compute damage.

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);
	CRaceClassPtr pRace = pCharDef->GetRace();
	ASSERT(pRace);

	int iDefenseTotal = 0;
	int iArmorCount = 0;
	WORD wBodyPartArmor[BODYPART_ARMOR_QTY];
	memset( wBodyPartArmor, 0, sizeof(wBodyPartArmor));
	for ( CItemPtr pItem=GetHead(); pItem!=NULL; pItem=pItem->GetNext())
	{
		int iDefense = pItem->Armor_GetDefense(true);

		// IsTypeSpellable() ? ! IT_WAND
		if (( pItem->IsType(IT_SPELL) || pItem->IsTypeArmor()) &&
			pItem->m_itSpell.m_spell )
		{
			SPELL_TYPE spell = (SPELL_TYPE) RES_GET_INDEX(pItem->m_itSpell.m_spell);
			switch (spell)
			{
			case SPELL_Steelskin:		// turns your skin into steel, giving a boost to your AR.
			case SPELL_Stoneskin:		// turns your skin into stone, giving a boost to your AR.
			case SPELL_Protection:
			case SPELL_Arch_Prot:
				// Effect of protection spells.
				iDefenseTotal += g_Cfg.GetSpellEffect(spell, pItem->m_itSpell.m_spelllevel )* 100;
				break;
			}
		}

		// reverse of sm_BodyParts
		switch ( pItem->GetEquipLayer())
		{
		case LAYER_HELM:		// 6
			wBodyPartArmor[ BODYPART_HEAD ] = MAX( wBodyPartArmor[ BODYPART_HEAD ], iDefense );
			break;
		case LAYER_COLLAR:	// 10 = gorget or necklace.
			wBodyPartArmor[ BODYPART_NECK ] = MAX( wBodyPartArmor[ BODYPART_NECK ], iDefense );
			break;
		case LAYER_CAPE:		// 20 = cape
			wBodyPartArmor[ BODYPART_CHEST ] = MAX( wBodyPartArmor[ BODYPART_CHEST ], iDefense );
			wBodyPartArmor[ BODYPART_BACK ] = MAX( wBodyPartArmor[ BODYPART_BACK ], iDefense );
			break;
		case LAYER_ROBE:		// 22 = robe over all.
			wBodyPartArmor[ BODYPART_CHEST ] = MAX( wBodyPartArmor[ BODYPART_CHEST ], iDefense );
			wBodyPartArmor[ BODYPART_BACK ] = MAX( wBodyPartArmor[ BODYPART_BACK ], iDefense );
			break;
		case LAYER_SHIRT:
			wBodyPartArmor[ BODYPART_CHEST ] = MAX( wBodyPartArmor[ BODYPART_CHEST ], iDefense );
			wBodyPartArmor[ BODYPART_BACK ] = MAX( wBodyPartArmor[ BODYPART_BACK ], iDefense );
			wBodyPartArmor[ BODYPART_ARMS ] = MAX( wBodyPartArmor[ BODYPART_ARMS ], iDefense );
			break;
		case LAYER_TUNIC:	// 17 = jester suit
			if ( pItem->GetID() != ITEMID_SASH )
				wBodyPartArmor[ BODYPART_ARMS ] = MAX( wBodyPartArmor[ BODYPART_ARMS ], iDefense );
			wBodyPartArmor[ BODYPART_CHEST ] = MAX( wBodyPartArmor[ BODYPART_CHEST ], iDefense );
			wBodyPartArmor[ BODYPART_BACK ] = MAX( wBodyPartArmor[ BODYPART_BACK ], iDefense );
			break;
		case LAYER_CHEST:	// 13 = armor chest
			wBodyPartArmor[ BODYPART_CHEST ] = MAX( wBodyPartArmor[ BODYPART_CHEST ], iDefense );
			wBodyPartArmor[ BODYPART_BACK ] = MAX( wBodyPartArmor[ BODYPART_BACK ], iDefense );
			break;
		case LAYER_ARMS:		// 19 = armor
			wBodyPartArmor[ BODYPART_ARMS ] = MAX( wBodyPartArmor[ BODYPART_ARMS ], iDefense );
			break;
		case LAYER_PANTS:
			wBodyPartArmor[ BODYPART_LEGS ] = MAX( wBodyPartArmor[ BODYPART_LEGS ], iDefense );
			wBodyPartArmor[ BODYPART_FEET ] = MAX( wBodyPartArmor[ BODYPART_FEET ], iDefense );
			break;
		case LAYER_SKIRT:
			wBodyPartArmor[ BODYPART_LEGS ] = MAX( wBodyPartArmor[ BODYPART_LEGS ], iDefense );
			wBodyPartArmor[ BODYPART_FEET ] = MAX( wBodyPartArmor[ BODYPART_FEET ], iDefense );
			break;
		case LAYER_HALF_APRON:
			wBodyPartArmor[ BODYPART_CHEST ] = MAX( wBodyPartArmor[ BODYPART_CHEST ], iDefense );
			wBodyPartArmor[ BODYPART_BACK ] = MAX( wBodyPartArmor[ BODYPART_BACK ], iDefense );
			break;
		case LAYER_SHOES:
			wBodyPartArmor[ BODYPART_FEET ] = MAX( wBodyPartArmor[ BODYPART_FEET ], iDefense );
			wBodyPartArmor[ BODYPART_LEGS ] = MAX( wBodyPartArmor[ BODYPART_LEGS ], iDefense );
			break;
		case LAYER_GLOVES:	// 7
			wBodyPartArmor[ BODYPART_HANDS ] = MAX( wBodyPartArmor[ BODYPART_HANDS ], iDefense );
			break;
		case LAYER_LEGS:
			wBodyPartArmor[ BODYPART_LEGS ] = MAX( wBodyPartArmor[ BODYPART_LEGS ], iDefense );
			wBodyPartArmor[ BODYPART_FEET ] = MAX( wBodyPartArmor[ BODYPART_FEET ], iDefense );
			break;

		case LAYER_HAND2:
			// Shield effect.
			if ( pItem->IsType( IT_SHIELD ))
			{
				iDefenseTotal += iDefense* ( Skill_GetAdjusted(SKILL_PARRYING) / 10 );
			}
			continue;

		default:
			continue;
		}

		iArmorCount ++;
	}
	if ( iArmorCount )
	{
		for ( int i=0; i<BODYPART_ARMOR_QTY; i++ )
		{
			iDefenseTotal += pRace->GetBodyPart((BODYPART_TYPE)i)->m_wCoveragePercent * wBodyPartArmor[i];
		}
	}
	return( iDefenseTotal / 100 );
}

int CChar::OnTakeDamageHitPoint( int iDmg, CChar* pSrc, DAMAGE_TYPE uType )
{
	// Point strike type damage.
	// Where did i get hit?
	// Deflect some damage with shield or weapon ?
	// Damage equip
	// ARGS:
	//  uType = ( DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH )

	ASSERT( ! ( uType & DAMAGE_GENERAL ));

	if ( IsStatFlag(STATF_HasShield) &&
		(uType & ( DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH )) &&
		! (uType & (DAMAGE_GOD|DAMAGE_ELECTRIC)))
	{
		CItemPtr pShield = LayerFind(LAYER_HAND2);
		if ( pShield )
		{
			// What is attackers skill for getting around your sheild?
			int iDifficulty = (pSrc!=NULL) ? (pSrc->Skill_GetBase(SKILL_TACTICS)/10) : 100;
			iDifficulty = Calc_GetRandVal( iDifficulty );
			if ( Skill_UseQuick( SKILL_PARRYING, iDifficulty ))
			{
				// Damage the shield.
				// Let through some damage.
				int iDefense = pShield->Armor_GetDefense(false);
				if ( pShield->OnTakeDamage( MIN( iDmg, iDefense ), pSrc, uType ))
				{
					WriteString( "You parry the blow" );
				}
				iDmg -= iDefense; // damage absorbed by shield
			}
		}
	}

	// Assumes humanoid type body. Orcs, Headless, trolls, humans etc.
	// ??? If not humanoid ??
	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);
	CRaceClassPtr pRace = pCharDef->GetRace();
	ASSERT(pRace);

	if ( pCharDef->Can(CAN_C_NONHUMANOID))
	{
		// ??? we may want some sort of message ?
		return( iDmg );
	}

	// Where was the hit ?
	int iHitRoll = Calc_GetRandVal( 100 ); // determine area of body hit
	BODYPART_TYPE iHitArea=BODYPART_HEAD;
	while ( iHitArea<BODYPART_ARMOR_QTY-1 )
	{
		iHitRoll -= pRace->GetBodyPart(iHitArea)->m_wCoveragePercent ;
		if ( iHitRoll < 0 )
			break;
		iHitArea = (BODYPART_TYPE)( iHitArea + 1 );
	}

	if ( (uType & ( DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH |DAMAGE_FIRE|DAMAGE_ELECTRIC)) &&
		! (uType & (DAMAGE_GENERAL|DAMAGE_GOD)))
	{

		static LPCTSTR const sm_Hit_Head1[][2] =
		{
			"hit you straight in the face!",	"You hit %s straight in the face!",
			"hits you in the head!",			"You hit %s in the head!",
			"hit you square in the jaw!",		"You hit %s square in the jaw!",
		};
		static LPCTSTR const sm_Hit_Head2[][2] =
		{
			"scores a stunning blow to your head!",		"You score a stunning blow to %ss head!",
			"smashes a blow across your face!",			"You smash a blow across %ss face!",
			"scores a terrible hit to your temple!",	"You score a terrible hit to %ss temple!",
		};
		static LPCTSTR const sm_Hit_Chest1[][2] =
		{
			"hits your Chest!",					"You hit %ss Chest!",
			"lands a blow to your stomach!",	"You land a blow to %ss stomach!",
			"hits you in the ribs!",			"You hit %s in the ribs!",
		};
		static LPCTSTR const sm_Hit_Chest2[][2] =
		{
			"lands a terrible blow to your chest!",	"You land a terrible blow to %ss chest!",
			"knocks the wind out of you!",			"You knock the wind out of %s!",
			"smashed you in the rib cage!",			"You smash %s in the rib cage!",
		};
		static LPCTSTR const sm_Hit_Arm[][2] =
		{
			"hits your left arm!",		"You hit %ss left arm!",
			"hits your right arm!",		"You hit %ss right arm!",
			"hits your right arm!",		"You hit %ss right arm!",
		};
		static LPCTSTR const sm_Hit_Legs[][2] =
		{
			"hits your left thigh!",	"You hit %ss left thigh!",
			"hits your right thigh!",	"You hit %ss right thigh!",
			"hits you in the groin!",	"You hit %s in the groin!",
		};
		static LPCTSTR const sm_Hit_Hands[][2] =	// later include exclusion of left hand if have shield
		{
			"hits your left hand!",		"You hit %ss left hand!",
			"hits your right hand!",	"You hit %ss right hand!",
			"hits your right hand!",	"You hit %ss right hand!",
		};
		static LPCTSTR const sm_Hit_Neck1[2] =
		{
			"hits you in the throat!",		"You hit %s in the throat!",
		};
		static LPCTSTR const sm_Hit_Neck2[2] =
		{
			"smashes you in the throat!",	"You smash %s in the throat!",
		};
		static LPCTSTR const sm_Hit_Back[2] =
		{
			"scores a hit to your back!",	"You score a hit to %ss back!",
		};
		static LPCTSTR const sm_Hit_Feet[2] =
		{
			"hits your foot!",				"You hit %s foot!",
		};

		int iMsg = Calc_GetRandVal(3);
		LPCTSTR const* ppMsg;
		switch ( iHitArea )
		{
		case BODYPART_HEAD:
			ppMsg = (iDmg>10) ? sm_Hit_Head2[iMsg] : sm_Hit_Head1[iMsg] ;
			break;
		case BODYPART_NECK:
			ppMsg = (iDmg>10) ? sm_Hit_Neck2 : sm_Hit_Neck1 ;
			break;
		case BODYPART_BACK:
			ppMsg = sm_Hit_Back;
			break;
		case BODYPART_CHEST:
			ppMsg = (iDmg>10) ? sm_Hit_Chest2[iMsg] : sm_Hit_Chest1[iMsg] ;
			break;
		case BODYPART_ARMS:
			ppMsg = sm_Hit_Arm[iMsg];
			break;
		case BODYPART_HANDS:
			ppMsg = sm_Hit_Hands[iMsg];
			break;
		case BODYPART_LEGS:
			ppMsg = sm_Hit_Legs[iMsg];
			break;
		case BODYPART_FEET:
			ppMsg = sm_Hit_Feet;
			break;
		default:
			ASSERT(0);
			break;
		}

		if ( pSrc != this )
		{
			if ( IsPrivFlag(PRIV_DETAIL))
			{
				Printf( "%s %s", ( pSrc == NULL ) ? "It" : (LPCTSTR) pSrc->GetName(), ppMsg[0] );
			}
			if ( pSrc != NULL && pSrc->IsPrivFlag(PRIV_DETAIL))
			{
				pSrc->Printf( ppMsg[1], (LPCTSTR) GetName());
			}
		}
	}

	// Do damage to my armor. (what if it is empty ?)

	int iMaxCoverage = 0;	// coverage at the hit zone.

	CItemPtr pArmorNext;
	for ( CItemPtr pArmor = GetHead(); pArmor != NULL; pArmor = pArmorNext )
	{
		pArmorNext = pArmor->GetNext();

		if ( pArmor->IsType(IT_SPELL) && pArmor->m_itSpell.m_spell )
		{
			SPELL_TYPE spell = (SPELL_TYPE) RES_GET_INDEX(pArmor->m_itSpell.m_spell);
			switch ( spell )
			{
			case SPELL_Steelskin:		// turns your skin into steel, giving a boost to your AR.
			case SPELL_Stoneskin:		// turns your skin into stone, giving a boost to your AR.
			case SPELL_Arch_Prot:
			case SPELL_Protection:
				// Effect of protection spells are general.
				iMaxCoverage = MAX( iMaxCoverage, g_Cfg.GetSpellEffect( spell, pArmor->m_itSpell.m_spelllevel ));
				continue;
			}
		}

		LAYER_TYPE layer = pArmor->GetEquipLayer();
		if ( ! CItemDef::IsVisibleLayer( layer ))
			continue;

		const CRaceBodyPartType* pBodyPart = pCharDef->GetRace()->GetBodyPart(iHitArea);
		for ( int i=0; pBodyPart->m_pLayers[i] != LAYER_NONE; i++ ) // layers covering the armor zone.
		{
			if ( pBodyPart->m_pLayers[i] == layer )
			{
				// This piece of armor takes damage.
				iMaxCoverage = MAX( iMaxCoverage, pArmor->Armor_GetDefense(false));
				pArmor->OnTakeDamage( iDmg, pSrc, uType );
				break;
			}
		}
	}

	// iDmg = ( iDmg* GW_GetSCurve( iMaxCoverage - iDmg, 10 )) / 100;
	iDmg -= iMaxCoverage;
	return( iDmg );
}

int CChar::OnTakeDamage( int iDmg, CChar* pSrc, DAMAGE_TYPE uType )
{
	// Someone or something hit us.
	// NOTE: There is NO reciprocation here.
	// Pre- armor absorb calc.
	//
	// uType =
	//	DAMAGE_GOD		0x01	// Nothing can block this.
	//	DAMAGE_HIT_BLUNT		0x02	// Physical hit of some sort.
	//	DAMAGE_MAGIC	0x04	// Magic blast of some sort. (we can be immune to magic to some extent)
	//	DAMAGE_POISON	0x08	// Or biological of some sort ? (HARM spell)
	//	DAMAGE_FIRE		0x10	// Fire damage of course.  (Some creatures are immune to fire)
	//	DAMAGE_ELECTRIC 0x20	// lightning.
	//  DAMAGE_DRAIN
	//	DAMAGE_GENERAL	0x80	// All over damage. As apposed to hitting just one point.
	//
	// RETURN: 
	//  health points damage actually done to me (not to my armor).
	//  -1 = already dead = invalid target. 
	//   0 = no damage. 
	//  INT_MAX = killed.

	if ( iDmg <= 0 )
		return( 0 );
	if ( pSrc == NULL )	// done by myself i suppose.
		pSrc = this;

	if ( pSrc != this )	// this could be an infinite loop if called in @GetHit
	{
		CSphereExpArgs execArgs( this, pSrc, iDmg, (int)uType, (CResourceObj*)NULL );
		if ( OnTrigger( CCharDef::T_GetHit, execArgs ) == TRIGRET_RET_VAL )
			return( 0 );
	}

	if ( uType & ( DAMAGE_ELECTRIC | DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH | DAMAGE_FIRE | DAMAGE_MAGIC ))
	{
		StatFlag_Clear( STATF_Freeze );	// remove paralyze. OnFreezeCheck();
	}

	if (( uType & DAMAGE_MAGIC ) && pSrc != this )
	{
		// The damage gets scaled based on the mage�s Evaluate Intelligence
		// and the victim�s Resisting Spells skill. The scaling can range in intensity
		// based on the difference between these two skills. If Evaluate Intelligence is
		// greater than Resisting Spells, then full damage is taken.
		int iSrcEvalInt = pSrc->Skill_GetAdjusted( SKILL_EVALINT );
		int iMyResist = Skill_GetAdjusted( SKILL_MAGICRESISTANCE );
		int iDelta = iSrcEvalInt - iMyResist;
		int iDivisor = iDelta > 0 ? 5000 : 2000;
		double dPercent = (double)iDelta / (double)iDivisor;
		iDmg = iDmg + (int)(iDmg* dPercent);
	}

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);

	if ( uType & ( DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH ))
	{
		// A physical blow of some sort.
		// Try to allow the armor or shield to take some damage.
		Reveal();

		// Check for reactive armor.
		if ( IsStatFlag( STATF_Reactive ) && ! ( uType & DAMAGE_GOD ))
		{
			// reflect some damage back.
			if ( pSrc && GetTopDist3D( pSrc ) <= 2 )
			{
				// ???
				// Reactive armor Spell strength is NOT the same as MAGERY !!!???
				int iSkillVal = Skill_GetAdjusted(SKILL_MAGERY);
				int iEffect = g_Cfg.GetSpellEffect( SPELL_Reactive_Armor, iSkillVal );
				int iRefDam = Calc_GetRandVal( IMULDIV( iDmg, iEffect, 1000 ));
				iDmg -= iRefDam;
				pSrc->OnTakeDamage( iRefDam, this, uType );
				pSrc->Effect( EFFECT_OBJ, ITEMID_FX_CURSE_EFFECT, this, 9, 6 );
			}
		}

		// absorbed by armor ?
		if ( ! ( uType & DAMAGE_GENERAL ))
		{
			iDmg = OnTakeDamageHitPoint( iDmg, pSrc, uType );
			iDmg -= pCharDef->m_armor.GetRandom();
		}
		else if ( ! ( uType & DAMAGE_GOD ))
		{
			// general overall damage.
			iDmg -= Calc_GetRandVal( m_ArmorDisplay );
			// ??? take some random damage to my equipped items.
		}
	}

	if ( IsStatFlag( STATF_INVUL ))
	{
	effect_bounce:
		if ( iDmg )
		{
			Effect( EFFECT_OBJ, ITEMID_FX_GLOW, this, 9, 30, false );
		}
		iDmg = 0;
	}
	else if ( ! ( uType & DAMAGE_GOD ))
	{
		if ( m_pArea.IsValidRefObj())
		{
			if ( m_pArea->IsFlag(REGION_FLAG_SAFE))
				goto effect_bounce;
			if ( m_pArea->IsFlag(REGION_FLAG_NO_PVP) && 
				m_pPlayer.IsValidNewObj() && 
				pSrc && 
				pSrc->m_pPlayer.IsValidNewObj())
				goto effect_bounce;
		}
		if ( IsStatFlag(STATF_Stone))	// can't hurt us anyhow.
		{
			goto effect_bounce;
		}
	}

	if ( m_StatHealth <= 0 )	// Already dead.
		return( -1 );

	if ( uType & DAMAGE_FIRE )
	{
		if ( pCharDef->Can(CAN_C_FIRE_IMMUNE)) // immune to the fire part.
		{
			// If there is any other sort of damage then dish it out as well.
			if ( ! ( uType & ( DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH | DAMAGE_POISON | DAMAGE_ELECTRIC )))
				return( 0 );	// No effect.
			iDmg /= 2;
		}
	}

	// defend myself. (even though it may not have hurt me.)
	if ( ! OnAttackedBy( pSrc, iDmg, false ))
		return( 0 );

	// Did it hurt ?
	if ( iDmg <= 0 )
		return( 0 );

	// Make blood depending on hit damage. assuming the creature has blood
	ITEMID_TYPE id = ITEMID_NOTHING;
	if ( pCharDef->m_wBloodHue != (HUE_TYPE)-1 )
	{
		if ( iDmg > 10 )
		{
			id = (ITEMID_TYPE)( ITEMID_BLOOD1 + Calc_GetRandVal(ITEMID_BLOOD6-ITEMID_BLOOD1));
		}
		else if ( Calc_GetRandVal( iDmg ) > 5 )
		{
			id = ITEMID_BLOOD_SPLAT;	// minor hit.
		}
		if ( id &&
			! IsStatFlag( STATF_Conjured ) &&
			( uType & ( DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH )))	// A blow of some sort.
		{
			CItemPtr pBlood = CItem::CreateBase( id );
			ASSERT(pBlood);
			pBlood->SetHue( pCharDef->m_wBloodHue );
			pBlood->MoveToDecay( GetTopPoint(), 7*TICKS_PER_SEC );
		}
	}

	Stat_Change( STAT_Health, -iDmg );
	if ( m_StatHealth <= 0 )
	{
		// We will die from this...make sure the killer is set correctly...if we don't do this, the person we are currently
		// attacking will get credit for killing us.
		// Killed by a guard looks here !
		if ( pSrc )
		{
			m_Act.m_Targ = pSrc->GetUID();
		}
		return( -1 );	// INT_MAX ?
	}

	SoundChar( CRESND_GETHIT );
	if ( m_Act.m_atFight.m_War_Swing_State != WAR_SWING_SWINGING )	// Not interrupt my swing.
	{
		UpdateAnimate( ANIM_GET_HIT );
	}
	return( iDmg );
}

int CChar::OnGetHit( int iDmg, CChar* pSrc, DAMAGE_TYPE uType )
{
	// We are hit by someone. we can get mad at them here.

	return( OnTakeDamage( iDmg, pSrc, uType ));
}

//*******************************************************************************
// Fight specific memories.

void CChar::Memory_Fight_Retreat( CChar* pTarg, CItemMemory* pFight )
{
	// The fight is over because somebody ran away.
	if ( pTarg == NULL || pTarg->IsStatFlag( STATF_DEAD ))
		return;

	ASSERT(pFight);
	int iMyDistFromBattle = GetTopPoint().GetDist( pFight->m_itEqMemory.m_ptStart );
	int iHisDistFromBattle = pTarg->GetTopPoint().GetDist( pFight->m_itEqMemory.m_ptStart );

	bool fCowardice = ( iMyDistFromBattle > iHisDistFromBattle );

	if ( fCowardice && ! pFight->IsMemoryTypes( MEMORY_IAGGRESSOR ))
	{
		// cowardice is ok if i was attacked.
		return;
	}

	Printf( fCowardice ?
		"You have retreated from the battle with %s" :
		"%s has retreated from the battle.", (LPCTSTR) pTarg->GetName());

	// Lose some fame.
	if ( fCowardice )
	{
		Noto_Fame( -1 );
	}
}

bool CChar::Memory_Fight_OnTick( CItemMemory* pMemory )
{
	// Check on the status of the fight.
	// return: false = delete the memory completely.
	//  true = skip it.

	ASSERT(pMemory);
	CCharPtr pTarg = g_World.CharFind(pMemory->m_uidLink);
	if ( pTarg == NULL )
		return( false );	// They are gone for some reason ?

#ifdef _DEBUG
	if ( g_Log.IsLogged( LOGL_TRACE ))
	{
		DEBUG_MSG(( "OnTick '%s' Memory of Fighting '%s'" LOG_CR, (LPCTSTR) GetName(), (LPCTSTR) pTarg->GetName()));
	}
#endif

	if ( GetDist(pTarg) > SPHEREMAP_VIEW_RADAR )
	{
		Memory_Fight_Retreat( pTarg, pMemory );
	clearit:
		Memory_ClearTypes( pMemory, MEMORY_FIGHT|MEMORY_IAGGRESSOR );
		return( true );
	}

	int iTimeDiff = pMemory->m_itEqMemory.m_timeStart.GetCacheAge();
	// DEBUG_CHECK( iTime >= 0 );

	// If am fully healthy then it's not much of a fight.
	if ( iTimeDiff > 60*60*TICKS_PER_SEC )
		goto clearit;
	if ( pTarg->GetHealthPercent() >= 100 && iTimeDiff > 2*60*TICKS_PER_SEC )
		goto clearit;

	pMemory->SetTimeout(20*TICKS_PER_SEC);
	return( true );	// reschedule it.
}

void CChar::Memory_Fight_Start( const CChar* pTarg )
{
	// I am attacking this creature.
	// i might be the aggressor or just retaliating.
	// This is just the "Intent" to fight. Maybe No damage done yet.

	ASSERT(pTarg);
	if ( Fight_IsActive() && m_Act.m_Targ == pTarg->GetUID())
	{
		// quick check that things are ok.
		return;
	}
	if ( pTarg == this )
		return;

	WORD MemTypes;
	CItemMemoryPtr pMemory = Memory_FindObj( pTarg );
	if ( pMemory == NULL )
	{
		// I have no memory of them yet.
		// There was no fight. Am I the aggressor ?
		CItemMemoryPtr pTargMemory = pTarg->Memory_FindObj( this );
		if (pTargMemory)	// My target remembers me.
		{
			if ( pTargMemory->IsMemoryTypes( MEMORY_IAGGRESSOR ))
				MemTypes = MEMORY_HARMEDBY;
			else if ( pTargMemory->IsMemoryTypes( MEMORY_HARMEDBY|MEMORY_SAWCRIME|MEMORY_AGGREIVED ))
				MemTypes = MEMORY_IAGGRESSOR;
			else
				MemTypes = 0;
		}
		else
		{
			// Hmm, I must have started this i guess.
			MemTypes = MEMORY_IAGGRESSOR;
		}
		pMemory = Memory_CreateObj( pTarg, MEMORY_FIGHT|MEMORY_WAR_TARG|MemTypes );
	}
	else
	{
		// I have a memory of them.
		bool fMemPrvType = pMemory->IsMemoryTypes(MEMORY_WAR_TARG);
		if ( fMemPrvType )
			return;
		if ( pMemory->IsMemoryTypes(MEMORY_HARMEDBY|MEMORY_SAWCRIME|MEMORY_AGGREIVED))
		{
			// I am defending myself rightly.
			MemTypes = 0;
		}
		else
		{
			MemTypes = MEMORY_IAGGRESSOR;
		}
		// Update the fights status
		Memory_AddTypes( pMemory, MEMORY_FIGHT|MEMORY_WAR_TARG|MemTypes );
	}

#ifdef _DEBUG
	if ( g_Log.IsLogged( LOGL_TRACE ))
	{
		DEBUG_MSG(( "Fight_Start '%s' attacks '%s', type 0%x" LOG_CR, (LPCTSTR) GetName(), (LPCTSTR) pTarg->GetName(), MemTypes ));
	}
#endif

	if ( IsClient())
	{
		// This may be a useless command. How do i say the fight is over ?
		// This causes the funny turn to the target during combat !
		CUOCommand cmd;
		cmd.Fight.m_Cmd = XCMD_Fight;
		cmd.Fight.m_dir = 0; // GetDirFlag();
		cmd.Fight.m_AttackerUID = GetUID();
		cmd.Fight.m_AttackedUID = pTarg->GetUID();
		GetClient()->xSendPkt( &cmd, sizeof( cmd.Fight ));
	}
	else
	{
		if ( m_pNPC && m_pNPC->m_Brain == NPCBRAIN_BESERK ) // it will attack everything.
			return;
	}

	if ( GetTopSector()->GetCharComplexity() < 7 )
	{
		// too busy for this.
		CGString sMsgThem;
		sMsgThem.Format( "*You see %s attacking %s*", (LPCTSTR) GetName(), (LPCTSTR) pTarg->GetName());

		// Don't bother telling me who i just attacked.
		UpdateObjMessage( sMsgThem, NULL, pTarg->GetClient(), HUE_RED, TALKMODE_EMOTE );
	}

	if ( pTarg->IsClient() && pTarg->CanSee(this))
	{
		CGString sMsgYou;
		sMsgYou.Format( "*%s is attacking you*", (LPCTSTR) GetName());
		pTarg->GetClient()->addObjMessage( sMsgYou, this, HUE_RED );
	}
}

//********************************************************

SKILL_TYPE CChar::Fight_GetWeaponSkill() const
{
	// What sort of weapon am i using?
	CItemPtr pWeapon = g_World.ItemFind( m_uidWeapon );
	if ( pWeapon == NULL )
		return( SKILL_WRESTLING );
	return( pWeapon->Weapon_GetSkill());
}

bool CChar::Fight_IsActive() const
{
	// Am i in an active fight mode ? 
	//  = War mode + War skill in use.
	if ( ! IsStatFlag(STATF_War))
		return( false );
	switch ( Skill_GetActive())
	{
	case SKILL_ARCHERY:
	case SKILL_FENCING:
	case SKILL_MACEFIGHTING:
	case SKILL_SWORDSMANSHIP:
	case SKILL_WRESTLING:
		return( true );
	}
	return( false );
}

int CChar::Fight_CalcDamage( CItem* pWeapon, SKILL_TYPE skill ) const
{
	// Calc_CombatDamageDealt
	// STR bonus on close combat weapons.
	// and DEX bonus on archery weapons

	if ( m_pNPC &&
		m_pNPC->m_Brain == NPCBRAIN_GUARD &&
		g_Cfg.m_fGuardsInstantKill )
	{
		return( 20000 );	// swing made.
	}

	int iDmg = 1;

	if ( pWeapon != NULL )
	{
		iDmg = pWeapon->Weapon_GetAttack(false);
	}
	else
	{
		// Pure wrestling damage.
		// Base type attack for our body. claws/etc
		CCharDefPtr pCharDef = Char_GetDef();
		ASSERT(pCharDef);
		iDmg = pCharDef->m_attack.GetRandom();
	}

	// We can always do some damage.
	if ( iDmg <= 0 ) 
		iDmg = 1;

	// Add the STR/DEX bonus.
	int iDmgAdj = Calc_GetRandVal( ( skill == SKILL_ARCHERY ) ? m_StatDex : m_StatStr );

	// A random value of the bonus is added (0-10%)
	iDmg += iDmgAdj / 10;

	// SKILL_ANATOMY bonus for melee combat?

	return( iDmg );
}

void CChar::Fight_ResetWeaponSwingTimer()
{
	if ( Fight_IsActive())
	{
		// The target or the weapon might have changed.
		// So restart the skill
		Skill_Start( Fight_GetWeaponSkill());
	}
}

int CChar::Fight_GetWeaponSwingTimer()
{
	// We have just equipped the weapon or gone into War mode.
	// Set the swing timer for the weapon or on us for .
	//   m_Act.m_atFight.m_War_Swing_State = WAR_SWING_EQUIPPING or WAR_SWING_SWINGING;
	// RETURN:
	//  tenths of a sec. TICKS_PER_SEC

	return( g_Cfg.Calc_CombatAttackSpeed( this, g_World.ItemFind( m_uidWeapon )));
}

void CChar::Fight_ClearAll()
{
	// clear all my active targets. Toggle out of war mode.
	CItemPtr pItem=GetHead();
	for ( ; pItem!=NULL; pItem=pItem->GetNext())
	{
		if ( ! pItem->IsMemoryTypes(MEMORY_WAR_TARG))
			continue;
		Memory_ClearTypes( REF_CAST(CItemMemory,pItem), MEMORY_WAR_TARG|MEMORY_IAGGRESSOR );
	}

	// Our target is gone.
	StatFlag_Clear( STATF_War );
	Skill_Start( SKILL_NONE );
	m_Act.m_Targ.InitUID();
	UpdateMode();
}

CCharPtr CChar::Fight_FindBestTarget()
{
	// Find my next target I am at war with.
	// If i am an NPC with no more targets then drop out of war mode.
	// RETURN:
	//  Target

	// DEBUG_CHECK( IsStatFlag( STATF_War ));

	SKILL_TYPE skillWeapon = Fight_GetWeaponSkill();
	int iClosest = INT_MAX;	// closest
	CCharPtr pChar;
	CCharPtr pClosest;

	// Check memories of those i have chosen to attack.
	CItemPtr pItem=GetHead();
	for ( ; pItem; pItem=pItem->GetNext())
	{
		if ( ! pItem->IsMemoryTypes(MEMORY_WAR_TARG))
			continue;
		pChar = g_World.CharFind(pItem->m_uidLink);
		int iDist = GetDist(pChar);

		if ( skillWeapon == SKILL_ARCHERY )
		{
			// archery dist is different.
			if ( iDist < g_Cfg.m_iArcheryMinDist || iDist > ARCHERY_DIST_MAX )
				continue;
			if ( m_Act.m_Targ == pChar->GetUID())	// alternate.
				continue;
			return( pChar );
		}
		else if ( iDist < iClosest )
		{
			// ??? in the npc case. can i actually reach this target ?
			pClosest = pChar;
			iClosest = iDist;
		}
	}

	if ( pClosest )
		return( pClosest );
	return( pChar );
}

bool CChar::Fight_Clear( const CChar* pChar )
{
	// I no longer want to attack this char.
	// ASSUME because they are dead./ out of range etc.

	if (pChar)
	{
		// DEBUG_CHECK( IsStatFlag( STATF_War ));

		CItemMemoryPtr pFight = Memory_FindObj( pChar ); // My memory of the fight.
		if ( pFight != NULL )
		{
			Memory_ClearTypes( pFight, MEMORY_FIGHT|MEMORY_WAR_TARG|MEMORY_IAGGRESSOR );
		}
	}

	// Go to my next target.
	pChar = Fight_FindBestTarget();
	if ( pChar == NULL )
	{
		// If i am an NPC with no more targets then drop out of war mode.
		if ( ! m_pPlayer.IsValidNewObj())
		{
			Fight_ClearAll();
		}
	}
	else
	{
		Fight_Attack(pChar);	// attack my next best target instead.
	}

	return( pChar != NULL );	// I did not know about this ?
}

bool CChar::Fight_Attack( const CChar* pCharTarg )
{
	// We want to attack someone.
	// But they won't notice til we actually hit them.
	// This is just my intent.
	// This may be an auto defend.
	// RETURN:
	//  true = new attack is accepted.

	if ( pCharTarg == NULL ||
		pCharTarg == this ||
		! CanSee(pCharTarg) ||
		pCharTarg->IsStatFlag( STATF_DEAD ) ||
		pCharTarg->IsDisconnected() ||
		IsStatFlag( STATF_DEAD ))
	{
		// Not a valid target.
		Fight_Clear( pCharTarg );
		return( false );
	}

	if ( GetPrivLevel() <= PLEVEL_Guest &&
		pCharTarg->m_pPlayer &&
		pCharTarg->GetPrivLevel() > PLEVEL_Guest )
	{
		WriteString( "Your guest curse prevents you from taking this action" );
		Fight_Clear( pCharTarg );
		return( false );
	}

	// Record the start of the fight.
	Memory_Fight_Start( pCharTarg );

	// I am attacking. (or defending)
	StatFlag_Set( STATF_War );

	// Skill interruption ?
	SKILL_TYPE skillWeapon = Fight_GetWeaponSkill();
	if ( Skill_GetActive() != skillWeapon ||
		m_Act.m_Targ != pCharTarg->GetUID())
	{
		m_Act.m_Targ = pCharTarg->GetUID();
		Skill_Start( skillWeapon );
	}

	return( true );
}

bool CChar::Fight_AttackNext()
{
	return Fight_Attack( Fight_FindBestTarget());
}

void CChar::Fight_HitTry()
{
	// A timer has expired so try to take a hit.
	// I am ready to swing or already swinging.
	// but i might not be close enough.
	// RETURN:
	//  false = no swing taken
	//  true = continue fighting

	ASSERT( Fight_IsActive());
	ASSERT( m_Act.m_atFight.m_War_Swing_State == WAR_SWING_READY || m_Act.m_atFight.m_War_Swing_State == WAR_SWING_SWINGING );

	CCharPtr pCharTarg = g_World.CharFind(m_Act.m_Targ);
	if ( pCharTarg == NULL )
	{
		// Might be dead ? Clear this.
		// move to my next target.
		Fight_AttackNext();
		return;
	}

	// Try to hit my target. I'm ready.
	switch ( Fight_Hit( pCharTarg ))
	{
	case WAR_SWING_INVALID:	// target is invalid.
		Fight_Clear( pCharTarg );
		Fight_AttackNext();
		return;
	case WAR_SWING_EQUIPPING:
		// Assume I want to continue hitting
		// Swing again. (start swing delay)
		// same target.
		{
			SKILL_TYPE skill = Skill_GetActive();
			Skill_Cleanup();	// Smooth transition = not a cancel of skill.
			Skill_Start( skill );
		}
		return;
	case WAR_SWING_READY:	// probably too far away. can't take my swing right now.
		// Try for a diff target ?
		Fight_AttackNext();
		return;
	case WAR_SWING_SWINGING:	// must come back here again to complete.
		return;
	}

	ASSERT(0);
}

WAR_SWING_TYPE CChar::Fight_Hit( CChar* pCharTarg )
{
	// Attempt to hit our target.
	// pCharTarg = the target.
	//  NOT - WAR_SWING_EQUIPPING = 0,	// we are recoiling our weapon.
	//  WAR_SWING_READY,			// we can swing at any time.
	//  WAR_SWING_SWINGING,			// we are swinging our weapon.
	// RETURN:
	//  WAR_SWING_INVALID = target is invalid
	//  WAR_SWING_EQUIPPING = swing made.
	//  WAR_SWING_READY = can't take my swing right now. but i'm ready
	//  WAR_SWING_SWINGING = taking my swing now.

	if ( pCharTarg == NULL || pCharTarg == this )
		return( WAR_SWING_INVALID );

	if ( IsStatFlag( STATF_DEAD | STATF_Sleeping | STATF_Freeze | STATF_Stone ) ||
		pCharTarg->IsStatFlag( STATF_DEAD ))
		return( WAR_SWING_INVALID );

	int dist = GetTopDist3D( pCharTarg );
	if ( dist > SPHEREMAP_VIEW_RADAR )
		return( WAR_SWING_INVALID );

	CItemPtr pWeapon = g_World.ItemFind( m_uidWeapon );
	CItemPtr pAmmo;
	SKILL_TYPE skill = Skill_GetActive();
	if ( skill == SKILL_ARCHERY )
	{
		// Archery type skill.
		ASSERT( pWeapon );
		DEBUG_CHECK( pWeapon->IsItemEquipped());
		CItemDefPtr pWeaponDef = pWeapon->Item_GetDef();
		ASSERT( pWeaponDef );
		DEBUG_CHECK( pWeaponDef->IsType(IT_WEAPON_BOW) || pWeaponDef->IsType(IT_WEAPON_XBOW));

		if ( dist > ARCHERY_DIST_MAX )
			return( WAR_SWING_READY );	// can't hit now.
		if ( ! CanSeeLOS( pCharTarg ))
			return( WAR_SWING_READY );

		if ( IsStatFlag( STATF_HasShield ))	// this should never happen.
		{
			WriteString( "Your shield prevents you from using your bow correctly." );
			return( WAR_SWING_EQUIPPING );
		}

		Reveal();
		UpdateDir( pCharTarg );

		if ( dist < g_Cfg.m_iArcheryMinDist )
		{
			// ??? the bow is acting like a (poor) blunt weapon at this range?
			WriteString( "You are too close" );
			int iTime = Fight_GetWeaponSwingTimer();
			UpdateAnimate( ANIM_ATTACK_1H_WIDE, false, false, iTime/TICKS_PER_SEC );
			return( WAR_SWING_EQUIPPING );
		}

		// Consume the bolts/arrows
		ITEMID_TYPE AmmoID = (ITEMID_TYPE) pWeaponDef->m_ttWeaponBow.m_idAmmo.GetResIndex();
		if ( AmmoID )
		{
			pAmmo = ContentFind( pWeaponDef->m_ttWeaponBow.m_idAmmo );
			if ( m_pPlayer && pAmmo == NULL )
			{
				WriteString( "You have no ammunition" );
				Skill_Start( SKILL_NONE );
				return( WAR_SWING_INVALID );
			}
		}

		if ( m_Act.m_atFight.m_War_Swing_State == WAR_SWING_READY )
		{
			// just start the bow animation.

			{
			CSphereExpContext exec( this, pCharTarg );
			if ( OnTrigger( CCharDef::T_HitTry, exec ) == TRIGRET_RET_VAL )
				return( WAR_SWING_READY );
			}

			m_Act.m_atFight.m_War_Swing_State = WAR_SWING_SWINGING;
			int iTime = Fight_GetWeaponSwingTimer();
			SetTimeout( iTime* 3 / 4 );	// try again sooner.
			UpdateAnimate( ANIM_ATTACK_WEAPON, true, false, iTime/TICKS_PER_SEC );
			return( WAR_SWING_SWINGING );
		}

		// now use the ammo
		if ( pAmmo )
		{
			pAmmo->UnStackSplit( 1, this );
		}

		pCharTarg->Effect( EFFECT_BOLT, (ITEMID_TYPE) pWeaponDef->m_ttWeaponBow.m_idAmmoX.GetResIndex(), this, 16, 0, false );
	}
	else
	{
		// A hand weapon of some sort.
		// if ( ! CanTouch( pCharTarg )) return( 2 );
		if ( m_Act.m_atFight.m_War_Swing_State == WAR_SWING_READY )
		{
			// We are swinging.
			if ( dist > 1 )
				return( WAR_SWING_READY );	// can't hit now.

			Reveal();
			UpdateDir( pCharTarg );

			{
			CSphereExpContext exec( this, pCharTarg );
			if ( OnTrigger( CCharDef::T_HitTry, exec ) == TRIGRET_RET_VAL )
				return( WAR_SWING_READY );
			}

			m_Act.m_atFight.m_War_Swing_State = WAR_SWING_SWINGING;
			int iTime = Fight_GetWeaponSwingTimer();
			SetTimeout( iTime/2 );	// try again sooner.
			UpdateAnimate( ANIM_ATTACK_WEAPON, true, false, iTime/TICKS_PER_SEC );
			return( WAR_SWING_SWINGING );
		}

		// done with the swing.
		// UpdateAnimate( ANIM_ATTACK_WEAPON, true, true );	// recoil anim
		if ( dist > 2 )
			return( WAR_SWING_READY );	// can't hit now.

		Reveal();
		UpdateDir( pCharTarg );
	}

	ASSERT( m_Act.m_atFight.m_War_Swing_State != WAR_SWING_READY );

	// We made our swing. so we must recoil.
	m_Act.m_atFight.m_War_Swing_State = WAR_SWING_EQUIPPING;
	int iTime = Fight_GetWeaponSwingTimer();
	SetTimeout( iTime/2 );	// try again sooner.

	// Use up some stamina to hit ?
	int iWeaponWeight = (( pWeapon ) ? ( pWeapon->GetWeight()/WEIGHT_UNITS + 1 ) : 1 );
	if ( ! Calc_GetRandVal( 10 + iWeaponWeight ))
	{
		Stat_Change( STAT_Stam, -1 );
	}

	// Check if we hit something;
	if ( m_Act.m_Difficulty < 0 )
	{
		{
		CSphereExpContext se( this, pCharTarg );
		if ( OnTrigger( CCharDef::T_HitMiss, se ) == TRIGRET_RET_VAL )
			return( WAR_SWING_EQUIPPING );
		}

		// We missed. (miss noise)
		if ( skill == SKILL_ARCHERY )
		{
			// 0x223 = bolt miss or dart ?
			// do some thing with the arrow.
			Sound( Calc_GetRandVal(2) ? 0x233 : 0x238 );
			// Sometime arrows should be lost/broken when we miss

			if ( pAmmo && Calc_GetRandVal(5))
			{
				// int pAmmo->OnTakeDamage( 2, pSrc, DAMAGE_HIT_BLUNT )
				pAmmo->MoveToCheck( pCharTarg->GetTopPoint(), this );
			}
			return( WAR_SWING_EQUIPPING );	// Made our full swing. (tho we missed)
		}

		if ( skill == SKILL_WRESTLING )
		{
			if ( SoundChar( Calc_GetRandVal(2) ? CRESND_RAND1 : CRESND_RAND2 ))
				return( WAR_SWING_EQUIPPING );
		}

		static const SOUND_TYPE sm_Snd_Miss[] =
		{
			0x238, // = swish01
			0x239, // = swish02
			0x23a, // = swish03
		};

		Sound( sm_Snd_Miss[ Calc_GetRandVal( COUNTOF( sm_Snd_Miss )) ] );

		if ( IsPrivFlag(PRIV_DETAIL))
		{
			Printf( "You Miss %s", (LPCTSTR) pCharTarg->GetName());
		}
		if ( pCharTarg->IsPrivFlag(PRIV_DETAIL))
		{
			pCharTarg->Printf( "%s missed you.", (LPCTSTR) GetName());
		}

		return( WAR_SWING_EQUIPPING );	// Made our full swing. (tho we missed)
	}

	// We hit...
	// Calculate base damage.
	int	iDmg = Fight_CalcDamage( pWeapon, skill );

	// if ( iDmg ) // use OnTakeDamage below instead.
	//	pCharTarg->OnHarmedBy( this, iDmg );

	{
	CSphereExpContext exec( this, pCharTarg );
	if ( OnTrigger( CCharDef::T_Hit, exec ) == TRIGRET_RET_VAL )
		return( WAR_SWING_EQUIPPING );
	}

	if ( skill == SKILL_ARCHERY )
	{
		// There's a chance that the arrow will stick in the target
		if ( pAmmo && !Calc_GetRandVal(2))
		{
			// int pAmmo->OnTakeDamage( 2+iDmg, pCharTarg, DAMAGE_HIT_BLUNT )

			pCharTarg->ItemBounce( pAmmo );
		}
	}

	// Raise skill
	Skill_UseQuick( SKILL_TACTICS, pCharTarg->Skill_GetBase(SKILL_TACTICS)/10 );

	// Hit noise. based on weapon type.
	SoundChar( CRESND_HIT );

	if ( pWeapon != NULL )
	{
		// poison weapon ?
		if ( pWeapon->m_itWeapon.m_poison_skill &&
			Calc_GetRandVal( 100 ) < pWeapon->m_itWeapon.m_poison_skill )
		{
			// Poison delivered.
			int iPoisonDeliver = Calc_GetRandVal(pWeapon->m_itWeapon.m_poison_skill);

			pCharTarg->Spell_Effect_Poison( 10*iPoisonDeliver, iPoisonDeliver/5, this );

			// Diminish the poison on the weapon.
			pWeapon->m_itWeapon.m_poison_skill -= iPoisonDeliver / 2;
		}

		// damage the weapon ?
		pWeapon->OnTakeDamage( iDmg/4, pCharTarg );
	}
	else
	{
		// Base type attack for our body. claws/etc
		// intrinsic attacks ?
		// Poisonous bite/sting ?
		if ( m_pNPC &&
			m_pNPC->m_Brain == NPCBRAIN_MONSTER &&
			Skill_GetBase(SKILL_POISONING) > 300 &&
			Calc_GetRandVal( 1000 ) < Skill_GetBase(SKILL_POISONING))
		{
			// Poison delivered.
			int iSkill = Skill_GetAdjusted( SKILL_POISONING );
			pCharTarg->Spell_Effect_Poison( Calc_GetRandVal(iSkill), Calc_GetRandVal(iSkill/50), this );
		}
	}

	// Took my swing. Do Damage !
	iDmg = pCharTarg->OnTakeDamage( iDmg, this, ( DAMAGE_HIT_BLUNT | DAMAGE_HIT_PIERCE | DAMAGE_HIT_SLASH ));
	if ( iDmg > 0 )
	{
		// Is we do no damage we get no experience!
		Skill_Experience( skill, m_Act.m_Difficulty );	// Get experience for it.
	}

	return( WAR_SWING_EQUIPPING );	// Made our full swing.
}


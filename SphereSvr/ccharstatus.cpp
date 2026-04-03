//
// CChar.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//  CChar is either an NPC or a Player.
//

#include "stdafx.h"	// predef header.

bool CChar::IsResourceMatch( CSphereUID rid, DWORD dwAmount )
{
	// Is the char a match for this test ?
	switch ( rid.GetResType())
	{
	case RES_Events:	// do i have these events ?
		if ( m_pNPC )
		{
			CCharDefPtr pCharDef = Char_GetDef();
			ASSERT(pCharDef);
			if ( pCharDef->m_Events.FindResourceID( rid ) >= 0 )
				return( true );
		}
		break;	// fall to default.

	case RES_Skill:
		// Do i have this skill level at least ?
		// A min skill is required.
		if ( Skill_GetBase((SKILL_TYPE) rid.GetResIndex()) < dwAmount )
			return( false );
		return( true );

	case RES_RaceClass:
		// Am i this type of char race ?
		if ( rid == Char_GetDef()->GetRace()->GetUIDIndex())
			return true;
		//if ( rid == Char_GetDef()->GetRaceGroup()->GetUIDIndex())
		//	return true;
		return( false );

	case RES_CharDef:
		// Am i this type of char ?
		{
			CCharDefPtr pCharDef = Char_GetDef();
			ASSERT(pCharDef);
			if ( pCharDef->GetResourceID() == rid )
				return( true );
		}
		break;
	case RES_Speech:	// do i have this speech ?
		if ( m_pNPC )
		{
			if ( m_pNPC->m_Speech.FindResourceID( rid ) >= 0 )
				return( true );
			CCharDefPtr pCharDef = Char_GetDef();
			ASSERT(pCharDef);
			if ( pCharDef->m_Speech.FindResourceID( rid ) >= 0 )
				return( true );
		}
		break;
	case RES_TypeDef:
	case RES_ItemDef:
		// Do i have these in my posession ?
		if ( ! ContentConsume( rid, dwAmount, true ))
		{
			return( true );
		}
		return( false );
	}

	return( CObjBase::IsResourceMatch( rid, dwAmount ));
}

CItemContainerPtr CChar::GetBank( LAYER_TYPE layer )
{
	// Get our bank box or vendor box.
	// If we dont have 1 then create it.

	ITEMID_TYPE id;
	switch ( layer )
	{
	case LAYER_PACK:
		id = ITEMID_BACKPACK;
		break;

	case LAYER_VENDOR_STOCK:
	case LAYER_VENDOR_EXTRA:
	case LAYER_VENDOR_BUYS:
		if ( ! NPC_IsVendor())
			return( NULL );
		id = ITEMID_VENDOR_BOX;
		break;

	case LAYER_BANKBOX:
	default:
		id = ITEMID_BANK_BOX;
		layer = LAYER_BANKBOX;
		break;
	}

	CItemPtr pItemTest = LayerFind( layer );
	CItemContainerPtr pBankBox = REF_CAST(CItemContainer,pItemTest);
	if ( pBankBox == NULL && ! g_Serv.IsLoading())
	{
		if ( pItemTest )
		{
			DEBUG_ERR(( "Junk in bank box layer %d!" LOG_CR, layer ));
			DEBUG_CHECK( ! pItemTest->IsWeird());
			pItemTest->DeleteThis();
		}

		// Give them a bank box if not already have one.
		pItemTest = CItem::CreateScript( id, this );
		pBankBox = REF_CAST(CItemContainer,pItemTest);
		ASSERT(pBankBox);
		pBankBox->SetAttr(ATTR_NEWBIE | ATTR_MOVE_NEVER);
		LayerAdd( pBankBox, layer );
		if ( layer != LAYER_PACK )
		{
			pBankBox->SetRestockTimeSeconds( 15*60 );
			pBankBox->SetTimeout( pBankBox->GetRestockTimeSeconds()* TICKS_PER_SEC );	// restock count
		}
	}
	return( pBankBox );
}

CItemPtr CChar::LayerFind( LAYER_TYPE layer ) const
{
	// Find an item i have equipped.
	CItemPtr pItem=GetHead();
	for ( ; pItem!=NULL; pItem=pItem->GetNext())
	{
		if ( pItem->GetEquipLayer() == layer )
			break;
	}
	return( pItem );
}

long CChar::GetMakeValue()
{
	// NOTE: NPC's have a value of sorts ?
	return( 1 + CContainer::GetMakeValue());
}

int CChar::GetWeightLoadPercent( int iWeight ) const
{
	// Get a percent of load.
	if ( IsGM())
		return( 1 );
	return( IMULDIV( iWeight, 100, g_Cfg.Calc_MaxCarryWeight(this)));
}

bool CChar::CanCarry( const CItem* pItem ) const
{
	if ( IsGM())
		return( true );

	int iMaxWeight = g_Cfg.Calc_MaxCarryWeight(this);

	// We are already carrying it ?
	CObjBasePtr pObjTop = pItem->GetTopLevelObj();
	if ( (CObjBase*) this == pObjTop )
	{
		if ( GetTotalWeight() > iMaxWeight )
			return( false );
	}
	else
	{
		int iWeight = pItem->GetWeight();
		if ( GetTotalWeight() + iWeight > iMaxWeight )
			return( false );
	}

	return( true );
}

LAYER_TYPE CChar::CanEquipLayer( CItem* pItem, LAYER_TYPE layer, CChar* pCharMsg, bool fTest )
{
	// This takes care of any conflicting items in the slot !
	// NOTE: Do not check to see if i can pick this up or steal this etc.
	// LAYER_NONE = can't equip this .
	// NOTE: CanEquipLayer may bounce an item . If it stacks with this we are in trouble !

	if ( pItem==NULL || !pItem->IsValidUID())
		return LAYER_NONE;	// somehow became invalid ?

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);

	CItemDefPtr pItemDef = pItem->Item_GetDef();
	ASSERT(pItemDef);

	if ( layer >= LAYER_QTY )	// only applies if pItemDef->IsTypeEquippable()
	{
		layer = pItemDef->GetEquipLayer();
	}

	// already here ?
	if ( IsMyChild(pItem) && pItem->GetEquipLayer() == layer )
	{
		return layer;
	}

	// visible item LAYER_TYPE ?
	bool fVisibleLayer = CItemDef::IsVisibleLayer( layer );
	if ( m_pPlayer && 
		fVisibleLayer && 
		pItemDef->IsTypeEquippable() && 
		! g_Serv.IsLoading())	// May not have a pack to put htis in!
	{
		if ( pItemDef->m_ttEquippable.m_StrReq &&
			m_StatStr < pItemDef->m_ttEquippable.m_StrReq )
		{
			if ( g_Serv.IsLoading())
			{
				DEBUG_MSG(( "CanEquipLayer 0%x can't equip item %s (%d<%d)" LOG_CR, 
					GetUID(), (LPCTSTR) pItem->GetResourceName(), m_StatStr, pItemDef->m_ttEquippable.m_StrReq ));
			}
			Printf( "Not strong enough to equip %s.", (LPCTSTR) pItem->GetName());
			if ( pCharMsg && pCharMsg != this )
			{
				pCharMsg->Printf( "Not strong enough to equip %s.", (LPCTSTR) pItem->GetName());
			}
			return LAYER_NONE;	// can't equip stuff.
		}
	}

	CItemPtr pItemPrev;
	switch ( layer )
	{
	case LAYER_NONE:
	case LAYER_SPECIAL:
		switch ( pItem->GetType() )
		{
		case IT_EQ_TRADE_WINDOW:
		case IT_EQ_MEMORY_OBJ:
		case IT_EQ_SCRIPT_BOOK:
		case IT_EQ_SCRIPT:
		case IT_EQ_MESSAGE:
		case IT_EQ_DIALOG:
			// We can have multiple items of these.
			return LAYER_SPECIAL;
		}
		return( LAYER_NONE );	// not legal !

	case LAYER_HIDDEN:
		DEBUG_ERR(( "ALERT: Weird layer 9 used for '%s' check this" LOG_CR, (LPCTSTR) pItem->GetResourceName()));
		break;	// return( LAYER_NONE );	// not legal !

	case LAYER_HAIR:
		if ( ! pItem->IsType(IT_HAIR))
		{
			goto cantequipthis;
		}
		break;
	case LAYER_BEARD:
		if ( ! pItem->IsType(IT_BEARD))
		{
			goto cantequipthis;
		}
		break;
	case LAYER_PACK:
		if ( ! pItem->IsType(IT_CONTAINER))
		{
			goto cantequipthis;
		}
		break;
	case LAYER_BANKBOX:
		if ( ! pItem->IsType(IT_EQ_BANK_BOX))
		{
			goto cantequipthis;
		}
		break;
	case LAYER_HORSE:
		// Only humans can ride horses !?
		if ( ! pItem->IsType(IT_EQ_HORSE) || ! IsHuman())
		{
			goto cantequipthis;
		}
		break;
	case LAYER_HAND1:
	case LAYER_HAND2:
		if ( ! pItemDef->IsTypeEquippable())
		{
			goto cantequipthis;
		}
		if ( ! pCharDef->Can( CAN_C_USEHANDS ))
		{
			goto cantequipthis;
		}
		if ( layer == LAYER_HAND2 )
		{
			// Is this a 2 handed weapon ? shields and lights are ok
			if ( pItem->IsTypeWeapon() || pItem->IsType(IT_FISH_POLE))
			{
				// Make sure the other hand is not full.
				pItemPrev = LayerFind( LAYER_HAND1 );
			}
		}
		else
		{
			// do i have a 2 handed weapon already equipped ? shields and lights are ok
			pItemPrev = LayerFind( LAYER_HAND2 );
			if ( pItemPrev != NULL )
			{
				if ( ! pItemPrev->IsTypeWeapon() && ! pItemPrev->IsType(IT_FISH_POLE))
				{
					pItemPrev = NULL;	// must be a shield or light.
				}
			}
		}
		break;

	case LAYER_COLLAR:
	case LAYER_RING:
	case LAYER_EARS:	// anyone can use these !
	case LAYER_LIGHT:
		break;

	default:
		// Can i equip this with my body type ?
		if ( fVisibleLayer )
		{
			if ( ! pCharDef->Can( CAN_C_EQUIP ))
			{
				// some creatures can equip certain special items ??? (orc lord?)
			cantequipthis:
				if ( pCharMsg)
				{
					pCharMsg->WriteString( "You can't equip this." );
				}
				if ( g_Log.IsLogged( LOGL_TRACE ) || g_Serv.IsLoading())
				{
					// We really don't need to know about this do we?  Rose sure doesn't seem to want to
					DEBUG_MSG(( "ContentAdd Creature '%s' can't equip %d, id='%s' '%s'" LOG_CR, (LPCTSTR) GetResourceName(), layer,  (LPCTSTR) pItem->GetResourceName(), (LPCTSTR) pItem->GetName()));
				}
				return LAYER_NONE;	// can't equip stuff.
			}
		}
		break;
	}

	// Check for objects already in this slot.
	// Deal with it.
	if ( pItemPrev == NULL )
	{
		pItemPrev = LayerFind( layer );
	}
	if ( pItemPrev != NULL )
	{
		switch ( layer )
		{
		case LAYER_PACK:
			// this should not happen.
			// Put it in my main pack.
			// DEBUG_CHECK( layer != LAYER_PACK );
			return LAYER_NONE;
		case LAYER_HORSE:
			// this should not happen.
			DEBUG_CHECK( layer != LAYER_HORSE );
			if ( ! fTest )
			{
				ItemBounce( pItemPrev );
			}
			break;
		case LAYER_DRAGGING:	// drop it.
			if ( ! fTest )
			{
				ItemBounce( pItemPrev );
			}
			break;
		case LAYER_BEARD:
		case LAYER_HAIR:
			if ( ! fTest )
			{
				pItemPrev->DeleteThis();
			}
			break;
		default:
			if ( layer >= LAYER_SPELL_STATS )
			{
				// Magic spell slots just get bumped.
				pItemPrev->DeleteThis();
				break;
			}
			// DEBUG_ERR(( "LayerAdd Layer %d already used" LOG_CR, layer ));
			// Force the current layer item into the pack ?
			if ( ! CanMove( pItemPrev, true ))
			{
				return LAYER_NONE;
			}
			if ( ! fTest )
			{
				ItemBounce( pItemPrev );
			}
			break;
		}
	}

	if ( fVisibleLayer && pItem->GetAmount() > 1 && ! fTest )
	{
		// can only use 1..
		pItem->UnStackSplit( 1, this );
	}
	return( layer );
}

bool CChar::CheckCorpseCrime( const CItemCorpse *pCorpse, bool fLooting, bool fTest )
{
	// fLooting = looting as apposed to carving.
	// RETURN: true = criminal act !

	if ( pCorpse == NULL )
		return( false );
	CCharPtr pCharGhost = g_World.CharFind(pCorpse->m_uidLink);
	if ( pCharGhost == NULL )
		return( false );
	if ( pCharGhost == this )	// ok but strange to carve up your own corpse.
		return( false );
	if ( ! g_Cfg.m_fLootingIsACrime )
		return( false );

	// It's ok to loot a guild member !
	NOTO_TYPE noto = pCharGhost->Noto_GetFlag( this, false );
	if ( noto != NOTO_GOOD )
	{
		return( false );	// not a crime.
	}

	if ( ! fTest )
	{
		// Anyone see me do this ?
		CheckCrimeSeen( SKILL_NONE, pCharGhost, pCorpse, fLooting ? "looting" : NULL );
		Noto_Criminal();
	}

	return true; // got flagged
}

CItemCorpsePtr CChar::FindMyCorpse( int iRadius ) const
{
	// If they are standing on there own corpse then res the corpse !
	CWorldSearch Area( GetTopPoint(), iRadius );
	for(;;)
	{
		CItemPtr pItem = Area.GetNextItem();
		if ( pItem == NULL )
			break;
		if ( ! pItem->IsType( IT_CORPSE ))
			continue;
		CItemCorpsePtr pCorpse = REF_CAST(CItemCorpse,pItem);
		if ( pCorpse == NULL )
			continue;
		if ( pCorpse->m_uidLink != GetUID())
			continue;
		return( pCorpse );
	}
	return( NULL );
}

int CChar::GetHealthPercent() const
{
	return( IMULDIV( m_StatHealth, 100, m_StatMaxHealth ));
}

bool CChar::IsSwimming() const
{
	// Am i in/over/slightly under the water now ?
	// NOTE: This is a bit more complex because we need to test if we are slightly under water.

	CPointMap ptTop = GetTopPoint();

	CPointMap pt = g_World.FindItemTypeNearby( ptTop, IT_WATER );
	if ( ! pt.IsValidPoint())
		return( false );	// no water here.

	int iDistZ = ptTop.m_z - pt.m_z;
	if ( iDistZ < -PLAYER_HEIGHT )
	{
		// standing far under the water some how.
		return( false );
	}
	if ( iDistZ <= 0 )
	{
		// we are in or below the water.
		return( true );
	}

	// Is there a solid surface under us ?
	CMulMapBlockState block( GetMoveCanFlags());
	g_World.GetHeightPoint( ptTop, block, m_pArea );
	if ( block.GetResultZ() == pt.m_z )	// we are in the water.
	{
		return( true );
	}

#if 0 
	if ( ! ( wBlockFlags & CAN_C_FLY ))	// none of this really matters if i could just fly.
	{

	}
#endif

	return( false );
}

NPCBRAIN_TYPE CChar::GetCreatureType() const
{
	// return 1 for animal, 2 for monster, 3 for NPC humans and PCs
	// For tracking and other purposes.

	// handle the exceptions.
	CREID_TYPE id = GetDispID();
	if ( id >= CREID_MAN )
	{
		if ( id == CREID_BLADES || id == CREID_VORTEX )
			return NPCBRAIN_BESERK;
		if ( id >= CREID_IRON_GOLEM && id <= CREID_MULTICOLORED_HORDE_DAEMON )
		{
			switch ( id )
			{
			case CREID_SERPENTINE_DRAGON:
			case CREID_SKELETAL_DRAGON:
			case CREID_REPTILE_LORD:
			case CREID_ANCIENT_WYRM:
			case CREID_SWAMP_DRAGON1:
			case CREID_SWAMP_DRAGON2:
				return NPCBRAIN_DRAGON;
			case CREID_SHADE:
			case CREID_MUMMY:
				return NPCBRAIN_UNDEAD;
			default:
				return NPCBRAIN_MONSTER;
			}
		}
		return( NPCBRAIN_HUMAN );
	}
	if ( id >= CREID_HORSE1 )
		return( NPCBRAIN_ANIMAL );
	switch ( id )
	{
	case CREID_EAGLE:
	case CREID_BIRD:
	case CREID_GORILLA:
	case CREID_Snake:
	case CREID_Dolphin:
	case CREID_Giant_Toad:	// T2A 0x50 = Giant Toad
	case CREID_Bull_Frog:	// T2A 0x51 = Bull Frog
		return NPCBRAIN_ANIMAL;
	}
	return( NPCBRAIN_MONSTER );
}

LPCTSTR CChar::GetPronoun() const
{
	switch ( GetDispID())
	{
	case CREID_CHILD_MB:
	case CREID_CHILD_MD:
	case CREID_MAN:
	case CREID_GHOSTMAN:
		return( "he" );
	case CREID_CHILD_FB:
	case CREID_CHILD_FD:
	case CREID_WOMAN:
	case CREID_GHOSTWOMAN:
		return( "she" );
	default:
		return( "it" );
	}
}

LPCTSTR CChar::GetPossessPronoun() const
{
	switch ( GetDispID())
	{
	case CREID_CHILD_MB:
	case CREID_CHILD_MD:
	case CREID_MAN:
	case CREID_GHOSTMAN:
		return( "his" );
	case CREID_CHILD_FB:
	case CREID_CHILD_FD:
	case CREID_WOMAN:
	case CREID_GHOSTWOMAN:
		return( "her" );
	default:
		return( "it's" );
	}
}

CAN_TYPE CChar::GetMoveCanFlags() const
{
	// What things block us ? CAN_C_GHOST
	if ( IsPrivFlag(PRIV_GM|PRIV_ALLMOVE))	// nothing blocks us.
	{
		// NOTE: Do not add CAN_C_FLY automatically
		return( CAN_C_GHOST|CAN_C_SWIM|CAN_C_WALK|CAN_C_PASSWALLS|CAN_C_CLIMB|CAN_C_FIRE_IMMUNE|CAN_C_INDOORS );
	}
	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);
	return( pCharDef->GetCanFlags());
}

BYTE CChar::GetModeFlag( bool fTrueSight ) const
{
	BYTE mode = 0;
	if ( IsStatFlag( STATF_Poisoned ))
		mode |= CHARMODE_POISON;
	if ( IsStatFlag( STATF_War ))
		mode |= CHARMODE_WAR;
	//	if ( IsStatFlag( STATF_Freeze|STATF_Sleeping|STATF_Hallucinating|STATF_Stone ))
	//		mode |= CHARMODE_YELLOW;
	if ( ! fTrueSight && IsStatFlag( STATF_Insubstantial | STATF_Invisible | STATF_Hidden | STATF_Sleeping ))	// if not me, this will not show up !
		mode |= CHARMODE_INVIS;
	return( mode );
}

BYTE CChar::GetLightLevel() const
{
	// Get personal light level.

	if ( IsStatFlag( STATF_DEAD ) || IsPrivFlag(PRIV_DEBUG))	// dead don't need light.
		return( LIGHT_BRIGHT + 1 );
	if ( IsStatFlag( STATF_Sleeping ) && ! IsGM())	// eyes closed.
		return( LIGHT_DARK/2 );
	if ( IsStatFlag( STATF_NightSight ))
		return( LIGHT_BRIGHT );
	return( GetTopSector()->GetLight());
}

CItemPtr CChar::GetSpellbook() const
{
	// IT_SPELLBOOK
	// Look for the equipped spellbook first.
	CItemPtr pBook = ContentFind( CSphereUID(RES_TypeDef,IT_SPELLBOOK), 0, 1 );
	if ( pBook != NULL )
		return( pBook );
	// Look in the top level of the pack only.
	CItemContainerPtr pPack = GetPack();
	if ( pPack )
	{
		pBook = pPack->ContentFind( CSphereUID(RES_TypeDef,IT_SPELLBOOK), 0, 4 );
		if ( pBook != NULL )
			return( pBook );
	}
	// Sorry no spellbook found.
	return( NULL );
}

int CChar::Food_GetLevelPercent() const
{
	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);
	if ( pCharDef->m_MaxFood == 0 )
		return 100;
	else
		return IMULDIV( m_StatFood, 100, pCharDef->m_MaxFood );
}

LPCTSTR CChar::Food_GetLevelMessage( bool fPet, bool fHappy ) const
{
	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);
	if ( pCharDef->m_MaxFood == 0)
		return "unaffected by food";
	int index = IMULDIV( m_StatFood, 8, pCharDef->m_MaxFood );

	if ( fPet )
	{
		static LPCTSTR const sm_szPetHunger[] =
		{
			"confused",
			"ravenously hungry",
			"very hungry",
			"hungry",
			"a little hungry",
			"satisfied",
			"very satisfied",
			"full",
		};
		static LPCTSTR const sm_szPetHappy[] =
		{
			"confused",
			"very unhappy",
			"unhappy",
			"fairly content",
			"content",
			"happy",
			"very happy",
			"extremely happy",
		};

		if ( index >= COUNTOF(sm_szPetHunger)-1 )
			index = COUNTOF(sm_szPetHunger)-1;

		return( fHappy ? sm_szPetHappy[index] : sm_szPetHunger[index] );
	}

	static LPCTSTR const sm_szFoodLevel[] =
	{
		"starving",			// "weak with hunger",
		"very hungry",
		"hungry",
		"fairly content",	// "a might pekish",
		"content",
		"fed",
		"well fed",
		"stuffed",
	};

	if ( index >= COUNTOF(sm_szFoodLevel)-1 )
		index = COUNTOF(sm_szFoodLevel)-1;

	return( sm_szFoodLevel[ index ] );
}

int CChar::Food_CanEat( CObjBase* pObj ) const
{
	// Would i want to eat this creature ? hehe
	// would i want to eat some of this item ?
	// 0 = not at all.
	// 10 = only if starving.
	// 20 = needs to be prepared.
	// 50 = not bad.
	// 75 = yummy.
	// 100 = my favorite (i will drop other things to get this).
	//

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);

	int iRet = pCharDef->m_FoodType.FindResourceMatch( pObj );
	if ( iRet >= 0 )
	{
		return( pCharDef->m_FoodType[iRet].GetResQty()); // how bad do i want it ?
	}

	// ???
	return( 0 );
}

static const CAssocStrVal sm_SkillTitles[] =
{
	"", INT_MIN,
	"Neophyte", 300,
	"Novice", 400,
	"Apprentice", 500,
	"Journeyman", 600,
	"Expert", 700,
	"Adept", 800,
	"Master", 900,
	"Grandmaster", 980,
	NULL, INT_MAX,
};

LPCTSTR CChar::GetTradeTitle() const // Paperdoll title for character p (2)
{
	CVarDefPtr pVar = m_TagDefs.FindKeyPtr( "TITLE" );
	if ( pVar )
		return( pVar->GetPSTR());

	TCHAR* pszTmp = Str_GetTemp();

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);

	// Incognito ?
	// If polymorphed then use the poly name.
	if ( IsStatFlag( STATF_Incognito ) ||
		! IsHuman() ||
		( m_pNPC && pCharDef->GetTypeName() != pCharDef->GetTradeName()))
	{
		if ( ! IsIndividualName())
			return( "" );	// same as type anyhow.
		sprintf( pszTmp, "the %s", (LPCTSTR) pCharDef->GetTradeName());
		return( pszTmp );
	}

	SKILL_TYPE skill = Skill_GetBest();
	int len = sprintf( pszTmp, "%s ", (LPCTSTR) sm_SkillTitles->FindValSorted( Skill_GetBase(skill)));
	sprintf( pszTmp+len, g_Cfg.GetSkillDef(skill)->m_sTitle, (pCharDef->IsFemale()) ? "woman" : "man" );
	return( pszTmp );
}

bool CChar::CanDisturb( CChar* pChar ) const
{
	// I can see/disturb only players with priv same/less than me.
	if ( pChar == NULL )
		return( false );
	if ( GetPrivLevel() < pChar->GetPrivLevel())
	{
		return( ! pChar->Player_IsDND());
	}
	return( true );
}

int CChar::GetVisibleDistance() const
{
	// from how far away can people see me ?
	// or rather how hard am i too see ?
	// do not for calculate light level etc.
	// figure in my hiding skill and invis etc.

	return( SPHEREMAP_VIEW_SIZE );
}

int CChar::GetVisualAbility() const
{
	// How well can i see ?	100% = normal.
	// > 100% means i can see things from much farther away. 
	// ?? also i can see invis ?
	// 
	return SPHEREMAP_VIEW_SIZE;
}

bool CChar::CanSeeInContainer( const CItemContainer* pContItem ) const
{
	// This is a container of some sort. Can i see inside it ?

	if ( pContItem == NULL )	// must be a CChar ?
		return( true );

	if ( pContItem->IsSearchable())	// not a bank box or locked box.
		return( true );

	// Not normally searchable.
	// Make some special cases for searchable.

	CCharPtr pChar = REF_CAST(CChar,pContItem->GetTopLevelObj());
	if ( pChar == NULL )
	{
		if ( IsGM())
			return( true );
		return( false );
	}

	if ( ! pChar->NPC_IsOwnedBy( this ))	// player vendor boxes.
		return( false );

	if ( pContItem->IsType(IT_EQ_VENDOR_BOX) ||
		pContItem->IsType(IT_EQ_BANK_BOX))
	{
		// must be in the same position i opened it legitamitly.
		// see addBankOpen

		if ( IsGM())
			return( true );
		if ( pContItem->m_itEqBankBox.m_pntOpen != GetTopPoint())
		{
			if ( IsClient())
			{
				g_Log.Event( LOG_GROUP_CHEAT, LOGL_WARN,
					"%x:Cheater is using 3rd party tools to access bank box" LOG_CR,
					GetClient()->m_Socket.GetSocket());
			}
			return( false );
		}
	}

	return( true );
}

bool CChar::CanSee( const CObjBaseTemplate* pObj ) const
{
	// Can I see this object ( char or item ) ?
	// Am I blind ?
	// Is it in a house i am not also in?

	if ( pObj == NULL )
		return( false );

	if ( IsDisconnected())
	{
		// This could just be a ridden horse !?? in which case i sort of can see it.
		return( false );
	}

	if ( pObj->IsItem())
	{
		CItemPtr pItem = STATIC_CAST(CItem,const_cast<CObjBaseTemplate *>(pObj));
		ASSERT(pItem);
		if ( ! CanSeeItem( pItem ))
			return( false );
		// object is in some sort of container?
		CObjBasePtr pObjCont = pItem->GetContainer();
		if ( pObjCont != NULL )
		{
			if ( ! CanSeeInContainer( REF_CAST(CItemContainer,pObjCont)))
				return( false );
			return( CanSee( pObjCont ));
		}
	}
	else
	{
		CCharPtr pChar = STATIC_CAST(CChar,const_cast<CObjBaseTemplate *>(pObj));
		ASSERT(pChar);
		if ( this == pChar )	// can always see myself.
			return( true );

		// ??? How good is his hiding skill ? compared to my seeing skill 
		//  in the current environment
		//  consider: distance, ground cover, light level, on horse etc.
		// war mode makes us more observant ?

		if ( pChar->IsStatFlag(STATF_DEAD) && m_pNPC )
		{
			if ( m_pNPC->m_Brain != NPCBRAIN_HEALER )
				return( false );
		}
		else if ( pChar->IsStatFlag( STATF_Insubstantial | STATF_Invisible | STATF_Hidden ))
		{
			// Characters can be invisible, but not to GM's (true sight ?)
			if ( GetPrivLevel() <= pChar->GetPrivLevel())
				return( false );
		}
		if ( pChar->IsDisconnected() && pChar->IsStatFlag(STATF_Ridden))
		{
			CCharPtr pCharRider = Horse_GetMountChar();
			if ( pCharRider )
			{
				return( CanSee( pCharRider ));
			}
		}
	}

	if (( pObj->IsTopLevel() || pObj->IsDisconnected()) &&
		IsPrivFlag( PRIV_ALLSHOW ))
	{
		// don't exclude for logged out and diff maps.
		return( GetTopPoint().GetDistBase( pObj->GetTopPoint()) <= pObj->GetVisibleDistance());
	}

	return( GetDist( pObj ) <= pObj->GetVisibleDistance());
}

bool CChar::CanSeeLOS( const CPointMap& ptDst, CPointMap* pptBlock, int iMaxDist ) const
{
	// Max distance of iMaxDist
	// Line of sight check
	// NOTE: if not blocked. pptBlock is undefined.
	if ( IsGM())
		return( true );

	CPointMap ptSrc = GetTopPoint();	// start where i am.
	ptSrc.m_z += PLAYER_HEIGHT/2;

	int iDist = ptSrc.GetDist( ptDst );
	if ( iDist == 0 )
	{
		// it is directly above or below me ?
		return( ptSrc.GetDistZ( ptDst ) <= PLAYER_HEIGHT/2 );
	}

	// Walk towards the object. If any spot is too far above our heads
	// then we can not see what's behind it.

	int iDistTry = 0;
	while ( --iDist > 0 )
	{
		DIR_TYPE dir = ptSrc.GetDir( ptDst );
		ptSrc.Move( dir );	// NOTE: The dir is very coarse and can change slightly.

		CMulMapBlockState block( CAN_C_SWIM | CAN_C_WALK | CAN_C_FLY | CAN_C_CLIMB | CAN_C_INDOORS, 1 );
		g_World.GetHeightPoint( ptSrc, block, ptSrc.GetRegion( REGION_TYPE_MULTI ));
		if ( block.m_Bottom.m_BlockFlags & (CAN_I_BLOCK | CAN_I_DOOR))
		{
		blocked:
			if ( pptBlock != NULL )
				*pptBlock = ptSrc;
			return false; // blocked
		}
		if ( iDistTry > iMaxDist )
		{
			// just went too far.
			goto blocked;
		}
		iDistTry ++;
	}

	return true; // made it all the way to the object with no obstructions.
}

bool CChar::CanSeeLOS( const CObjBaseTemplate* pObj ) const
{
	if ( ! CanSee( pObj ))
		return( false );
	CObjBasePtr pObjTop = pObj->GetTopLevelObj();
	return( CanSeeLOS( pObjTop->GetTopPoint(), NULL, pObjTop->GetVisibleDistance()));
}

bool CChar::CanTouch( const CPointMap & pt ) const
{
	// Can I reach this from where i am.
	// swords, spears, arms length = x units.
	// Use or Grab.
	// Check for blocking objects.
	// It this in a container we can't get to ?

	return( CanSeeLOS( pt, NULL, 6 ));
}

bool CChar::CanTouch( const CObjBase* pObj ) const
{
	// Can I reach this from where i am.
	// swords, spears, arms length = x units.
	// Use or Grab. May be in snooped container.
	// Check for blocking objects.
	// Is this in a container we can't get to ?

	if ( pObj == NULL )
		return( false );

	bool fDeathImmune = IsGM();

	CObjBasePtr pObjTop = pObj->GetTopLevelObj();
	ASSERT(pObjTop);

	int iDist = GetTopDist3D( pObjTop );

	if ( pObj->IsItem())	// some objects can be used anytime. (even by the dead.)
	{
		const CItem* pItem = PTR_CAST(const CItem,pObj);
		ASSERT( pItem );
		switch ( pItem->GetType())
		{
		case IT_SIGN_GUMP:	// can be seen from a distance.
			return( iDist < pObjTop->GetVisibleDistance());

		case IT_TELESCOPE:
		case IT_SHRINE:	// We can use shrines when dead !!
			fDeathImmune = true;
			break;
		case IT_SHIP_SIDE:
		case IT_SHIP_SIDE_LOCKED:
		case IT_SHIP_PLANK:
		case IT_ARCHERY_BUTTE:	// use from distance.
			if ( IsStatFlag( STATF_Sleeping | STATF_Freeze | STATF_Stone ))
				break;
			return( GetTopDist3D( pItem->GetTopLevelObj()) <= SPHEREMAP_VIEW_SIZE );
		}

		// Search up to the top level object.
		for(;;)
		{
			// What is this inside of ?
			CObjBasePtr pObjCont = pItem->GetContainer();
			if ( pObjCont == NULL )
				break;	// reached top level.

			pObj = pObjCont;
			if ( ! CanSeeInContainer( PTR_CAST(const CItemContainer,pObj) ))
			{
				return( false );
			}

			pItem = PTR_CAST(const CItem,pObj);
			if ( pItem == NULL )
				break;
		}
	}

	// We now have the top level object.
	DEBUG_CHECK( pObj->IsTopLevel() || pObj->IsDisconnected());
	DEBUG_CHECK( pObj == pObjTop );

	if ( ! fDeathImmune )
	{
		if ( IsStatFlag( STATF_DEAD | STATF_Sleeping | STATF_Freeze | STATF_Stone ))
			return( false );
	}

	if ( pObjTop->IsChar())
	{
		CCharPtr pChar = REF_CAST(CChar,pObjTop);
		ASSERT( pChar );
		if ( pChar == this )	// something i am carrying. short cut.
		{
			return( true );
		}
		if ( IsGM())
		{
			return( GetPrivLevel() >= pChar->GetPrivLevel());
		}
		if ( pChar->IsStatFlag( STATF_DEAD|STATF_Stone ))
			return( false );
	}
	else
	{
		// on the ground or In a container on the ground.
		if ( IsGM())
			return( true );
	}

	if ( iDist > 3 )	// max touch distance.
		return( false );

	return( CanSeeLOS( pObjTop->GetTopPoint(), NULL, pObjTop->GetVisibleDistance()));
}

IT_TYPE CChar::CanTouchStatic( CPointMap& pt, ITEMID_TYPE id, CItem* pItem ) const
{
	// Might be a dynamic or a static.
	// RETURN:
	//  IT_JUNK = too far away.
	//  set pt to the top level point.

	if ( pItem )
	{
		if ( ! CanTouch( pItem ))
			return( IT_JUNK );
		pt = GetTopLevelObj()->GetTopPoint();
		return( pItem->GetType());
	}

	// Its a static !

	CItemDefPtr pItemDef = g_Cfg.FindItemDef(id);
	if ( pItemDef == NULL )
		return( IT_NORMAL );

	if ( ! CanTouch( pt ))
		return( IT_JUNK );

	// Is this static really here ?
	const CMulMapBlock* pMapBlock = g_World.GetMapBlock( pt );
	ASSERT( pMapBlock );

	int x2=pMapBlock->GetOffsetX(pt.m_x);
	int y2=pMapBlock->GetOffsetY(pt.m_y);

	int iQty = pMapBlock->m_Statics.GetStaticQty();
	for ( int i=0; i < iQty; i++ )
	{
		if ( ! pMapBlock->m_Statics.IsStaticPoint(i,x2,y2))
			continue;
		const CMulStaticItemRec* pStatic = pMapBlock->m_Statics.GetStatic( i );
		if ( id == pStatic->GetDispID() )
			return( pItemDef->GetType() );
	}

	return( IT_NORMAL );
}

bool CChar::CanHear( const CObjBaseTemplate* pSrc, TALKMODE_TYPE mode ) const
{
	// can we hear text or sound. (not necessarily understand it (ghost))
	// Can't hear TALKMODE_SAY through house walls.
	// NOTE: Assume pClient->CanHear() has already been called. (if it matters)

	if ( pSrc == NULL )	// must be broadcast i guess.
		return( true );

	int iHearRange;
	switch ( mode )
	{
	case TALKMODE_YELL:
		iHearRange = SPHEREMAP_VIEW_RADAR;
		break;
	case TALKMODE_BROADCAST:
		return( true );
	case TALKMODE_WHISPER:
		iHearRange = 3;
		break;
	default:
		iHearRange = SPHEREMAP_VIEW_SIZE;
		break;
	}

	CObjBasePtr pSrcTop = pSrc->GetTopLevelObj();
	int iDist = GetTopDist3D( pSrcTop );
	if ( iDist > iHearRange )	// too far away
		return( false );

	if ( iHearRange > SPHEREMAP_VIEW_SIZE )	// a yell goes through walls..
		return( true );

	// sound can be blocked if in house.
	CRegionComplexPtr pSrcRegion;
	if ( pSrc->IsChar())
	{
		CCharPtr pCharSrc = REF_CAST(CChar,pSrcTop);
		ASSERT(pCharSrc);
		pSrcRegion = pCharSrc->GetArea();
	}
	else
	{
		pSrcRegion = REF_CAST(CRegionComplex,pSrcTop->GetTopRegion( REGION_TYPE_MULTI|REGION_TYPE_AREA ));
	}

	if ( m_pArea == pSrcRegion )	// same region is always ok.
		return( true );
	if ( pSrcRegion == NULL || ! m_pArea ) // should not happen really.
		return( false );
	if ( pSrcRegion->IsMultiRegion() && ! pSrcRegion->IsFlag(REGION_FLAG_SHIP))
		return( false );
	if ( m_pArea->IsMultiRegion() && ! m_pArea->IsFlag(REGION_FLAG_SHIP))
		return( false );

	return( true );
}

bool CChar::CanMove( CItem* pItem, bool fMsg )
{
	// Is it possible that i could move this ?
	// NOT: test if need to steal. IsTakeCrime()
	// NOT: test if close enough. CanTouch()

	if ( pItem == NULL )
		return( false );
	if ( IsPrivFlag( PRIV_ALLMOVE | PRIV_DEBUG | PRIV_GM ))
		return( true );
	if ( IsStatFlag( STATF_Stone | STATF_Freeze | STATF_Insubstantial | STATF_DEAD | STATF_Sleeping ))
	{
		if ( fMsg )
		{
			Printf( "you can't reach anything in your state." );
		}
		return( false );
	}

#if 0
	// ??? Trigger to test 'can move'
	if ( ! pItem->OnTrigger( CItemDef::T_CAN_MOVE ))
	{
		return( false );
	}
#endif

	if ( pItem->IsTopLevel() )
	{
		if ( pItem->IsTopLevelMultiLocked())
		{
			if ( fMsg )
			{
				WriteString( "It appears to be locked to the structure." );
			}
			return false;
		}
	}
	else	// equipped or in a container.
	{
		if ( pItem->IsAttr( ATTR_CURSED|ATTR_CURSED2 ) && pItem->IsItemEquipped())
		{
			// "It seems to be stuck where it is"
			//
			if ( fMsg )
			{
				pItem->SetAttr(ATTR_IDENTIFIED);
				Printf( "%s appears to be cursed.", (LPCTSTR) pItem->GetName());
			}
			return false;
		}

		// Can't steal/move newbie items on another cchar. (even if pet)
		if ( pItem->IsAttr( ATTR_NEWBIE|ATTR_BLESSED2|ATTR_CURSED|ATTR_CURSED2 ))
		{
			CObjBasePtr pObjTop = pItem->GetTopLevelObj();
			if ( pObjTop->IsItem())	// is this a corpse or sleeping person ?
			{
				CItemCorpsePtr pCorpse = REF_CAST(CItemCorpse,pObjTop);
				if ( pCorpse )
				{
					CCharPtr pChar = pCorpse->IsCorpseSleeping();
					if ( pChar && pChar != this )
					{
						return( false );
					}
					// Lock loot on player corpses till it decays ?!?

				}
			}
			else if ( pObjTop->IsChar() && pObjTop != this )
			{
				if ( ! pItem->IsItemEquipped() ||
					pItem->GetEquipLayer() != LAYER_DRAGGING )
				{
					return( false );
				}
			}
		}

		if ( pItem->IsItemEquipped())
		{
			LAYER_TYPE layer = pItem->GetEquipLayer();
			switch ( layer )
			{
			case LAYER_DRAGGING:
				return( true );
			case LAYER_HAIR:
			case LAYER_BEARD:
			case LAYER_PACK:
			case LAYER_HORSE:	// not this way.
				if ( ! IsGM())
					return( false );
				break;
			default:
				if ( ! CItemDef::IsVisibleLayer(layer) && ! IsGM())
				{
					return( false );
				}
			}
		}
	}

	return( pItem->IsMovable());
}

bool CChar::IsTakeCrime( const CItem* pItem, CCharPtr* ppCharMark ) const
{
	// We are snooping or stealing.
	// Is taking this item a crime ?
	// RETURN:
	//  ppCharMark = The character we are offending.
	//  false = no crime.

	// May have been a static object
	if ( pItem == NULL )
		return false;
	CObjBasePtr pObjTop = pItem->GetTopLevelObj();
	if ( pObjTop == NULL )
		return false;
	CCharPtr pCharMark = REF_CAST(CChar,pObjTop);
	if ( ppCharMark != NULL )
	{
		*ppCharMark = pCharMark;
	}

	if ( IsPrivFlag(PRIV_GM|PRIV_ALLMOVE))
		return( false );

	if ( pCharMark == this )
	{
		// this is yours
		return( false );
	}

	if ( pCharMark == NULL )	// In some (or is) container.
	{
		if ( pItem->IsAttr(ATTR_OWNED) && pItem->m_uidLink != GetUID())
			return( true );

		CItemContainerPtr pCont = REF_CAST(CItemContainer,pObjTop);
		if (pCont)
		{
			if ( pCont->IsAttr(ATTR_OWNED))
				return( true );

			// On corpse
			// ??? what if the container is locked ?
		}

		CItemCorpsePtr pCorpseItem = REF_CAST(CItemCorpse,pObjTop);
		if ( pCorpseItem )
		{
			// Taking stuff off someones corpse can be a crime !
			return( const_cast <CChar*>(this)->CheckCorpseCrime( pCorpseItem, true, true ));
		}

		return( false );	// i guess it's not a crime.
	}

	if ( pCharMark->NPC_IsOwnedBy( this ))	// He let's you
		return( false );

	// Pack animal has no owner ?
	if ( pCharMark->GetCreatureType() == NPCBRAIN_ANIMAL &&	// free to take.
		! pCharMark->IsStatFlag( STATF_Pet ) &&
		! pCharMark->m_pPlayer )
	{
		return( false );
	}

	return( true );
}

bool CChar::CanUse( CItem* pItem, bool fMoveOrConsume )
{
	// Can the Char use ( CONSUME )  the item where it is ?
	// NOTE: Although this implies that we pick it up and use it.

	if ( ! CanTouch(pItem) )
		return( false );

	if ( fMoveOrConsume )
	{
		if ( ! CanMove(pItem))
			return( false );	// ? using does not imply moving in all cases ! such as reading ?
		if ( IsTakeCrime( pItem ))
			return( false );
	}
	else
	{
		// Just snooping i guess.
		if ( pItem->IsTopLevel())
			return( true );
		// The item is on another character ?
		CObjBasePtr pObjTop = pItem->GetTopLevelObj();
		if ( pObjTop != ( STATIC_CAST(const CObjBaseTemplate,this)))
		{
			if ( IsGM())
				return( true );
			if ( pItem->IsType(IT_CONTAINER))
				return( true );
			if ( pItem->IsType(IT_BOOK))
				return( true );
			return( false );
		}
	}

	return( true );
}

CRegionPtr CChar::CheckValidMove( CPointMapBase& ptDest, CMulMapBlockState& block ) const
{
	// Is it ok for me to move here ? is it blocked for me?
	// ignore other characters for now.
	// RETURN:
	//  The new region we may be in.
	//  ptDest.m_z = the proper value for this location. (if walking)
	//  blocks = what is blocking me. (can be null = don't care)

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef);
	block.m_CanFlags = GetMoveCanFlags();
	block.m_iHeight = pCharDef->GetHeight();

	CRegionPtr pArea = ptDest.GetRegion( REGION_TYPE_MULTI | REGION_TYPE_AREA );
	if ( pArea == NULL )
		return( NULL );

	// In theory the client only lets us go valid places.
	// But in reality the client and server are not really in sync.

	g_World.GetHeightPoint( ptDest, block, pArea );
	PNT_Z_TYPE z = block.GetResultZ();

	if ( ! IsGM())	// this is a GM ?
	{
		if ( block.IsResultBlocked())
		{
			return( NULL );
		}
		if ( ! pCharDef->Can( CAN_C_FLY ))
		{
			if ( z > ptDest.m_z + block.m_iHeight )	// Too high to climb.
				return( NULL );
		}
	}

	ptDest.m_z = z;
	return( pArea );
}


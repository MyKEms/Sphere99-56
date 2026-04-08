//
// CClientEvent.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
#include "stdafx.h"	// predef header.
#include "spherelog.h"

static void s_CombineKeys(TCHAR* pszOut, LPCTSTR pszKey, LPCTSTR pszArg) {
	sprintf(pszOut, "%s.%s", pszKey, pszArg ? pszArg : "");
}

/////////////////////////////////
// Events from the Client.

void CClient::Event_MapEdit( CSphereUID uid, const CUOEvent* pEvent )
{
	CItemMapPtr pMap = REF_CAST(CItemMap,g_World.ItemFind(uid));
	if ( ! m_pChar->CanTouch( pMap ))	// sanity check.
	{
		WriteString( "You can't reach it" );
		return;
	}
	if ( pMap->m_itMap.m_fPinsGlued )
	{
		WriteString( "The pins seem to be glued in place" );
		if ( ! IsPrivFlag(PRIV_GM))
		{
			return;
		}
	}

	// NOTE: while in edit mode, right click canceling of the
	// dialog sends the same msg as
	// request to edit in either mode...strange huh?

	switch (pEvent->MapEdit.m_action)
	{
	case MAP_ADD: // add pin
		if ( pMap->m_Pins.GetSize() > CItemMap::MAX_PINS )
			return;	// too many.
		{
			CPointMap PinNew( pEvent->MapEdit.m_pin_x, pEvent->MapEdit.m_pin_y );
			pMap->m_Pins.Add(PinNew);
		}
		break;
	case MAP_INSERT: // insert between 2 pins
		if ( pMap->m_Pins.GetSize() > CItemMap::MAX_PINS )
			return;	// too many.
		{
			CPointMap PinNew( pEvent->MapEdit.m_pin_x, pEvent->MapEdit.m_pin_y );
			pMap->m_Pins.InsertAt( pEvent->MapEdit.m_pin, PinNew );
		}
		break;
	case MAP_MOVE: // move pin
		if ( pEvent->MapEdit.m_pin >= pMap->m_Pins.GetSize())
		{
			WriteString( "That's strange... (bad pin)" );
			return;
		}
		else
		{
			CPointMap& pin = pMap->m_Pins.ElementAt(pEvent->MapEdit.m_pin);
			pin.m_x = pEvent->MapEdit.m_pin_x;
			pin.m_y = pEvent->MapEdit.m_pin_y;
		}
		break;
	case MAP_DELETE: // delete pin
		{
			if ( pEvent->MapEdit.m_pin >= pMap->m_Pins.GetSize())
			{
				WriteString( "That's strange... (bad pin)" );
				return;
			}
			pMap->m_Pins.RemoveAt(pEvent->MapEdit.m_pin);
		}
		break;
	case MAP_CLEAR: // clear all pins
		pMap->m_Pins.RemoveAll();
		break;
	case MAP_TOGGLE: // edit req/cancel
		addMapMode( pMap, MAP_SENT, ! pMap->m_fPlotMode );
		break;
	}
}

void CClient::Event_Item_Dye( CSphereUID uid, HUE_TYPE wHue ) // Rehue an item
{
	// CLIMODE_DYE
	// Result from addDyeOption()
	// NOTE: uid has already been cleared for dyeing.

	if ( GetTargMode() != CLIMODE_DYE && uid != m_Targ.m_UID )
	{
		return;
	}

	CObjBasePtr pObj = g_World.ObjFind(uid);
	if ( ! m_pChar->CanTouch( pObj ))	// sanity check.
	{
		WriteString( "You can't reach it" );
		return;
	}

	ClearTargMode();

	if ( ! IsPrivFlag( PRIV_GM ))
	{
		if ( pObj->IsChar())
			return;
		if ( wHue<HUE_BLUE_LOW || wHue>HUE_DYE_HIGH )
		{
			// Cheater using colors out of range.
			wHue = HUE_DYE_HIGH;
		}

		// Is this colorable ?
	}
	else
	{
		if ( pObj->IsChar())
		{
			pObj->RemoveFromView();
			wHue |= HUE_UNDERWEAR;
		}
	}

	pObj->SetHue( wHue );
	pObj->Update();
}

void CClient::Event_Tips( WORD i ) // Tip of the day window
{
	if (i==0)
		i=1;
	CString sResourceName;
	sResourceName.Format( "SCROLL_TIP_%d", i );

	CResourceLock s( g_Cfg.ResourceGetDefByName( RES_Scroll, sResourceName ));
	if ( s.IsFileOpen())
	{
		addScrollScript( s, SCROLL_TYPE_TIPS, i );
	}
}

void CClient::Event_Book_Title( CSphereUID uid, LPCTSTR pszTitle, LPCTSTR pszAuthor )
{
	// XCMD_BookOpen
	// user is changing the books title/author info.

	CItemPtr pBook2 = g_World.ItemFind(uid);
	CItemMessagePtr pBook = REF_CAST(CItemMessage,pBook2 );
	if ( ! m_pChar->CanTouch( pBook ))	// sanity check.
	{
		WriteString( "you can't reach it" );
		return;
	}
	if ( ! pBook->IsBookWritable())	// Not blank
		return;

	pBook->SetName( pszTitle );
	pBook->m_sAuthor = pszAuthor;
}

void CClient::Event_Book_Page( CSphereUID uid, const CUOEvent* pEvent ) // Book window
{
	// XCMD_BookPage
	// Read or write to a book page.

	CItemPtr pBook = g_World.ItemFind(uid);	// the book.
	if ( ! m_pChar->CanSee( pBook ))
	{
		addObjectRemoveCantSee( uid, "the book" );
		return;
	}

	int iPage = pEvent->BookPage.m_page[0].m_pagenum;	// page.
	DEBUG_CHECK( iPage > 0 );

	if ( pEvent->BookPage.m_page[0].m_lines == 0xFFFF ||
		pEvent->BookPage.m_len <= 0x0d )
	{
		// just a request for pages.
		addBookPage( pBook, iPage );
		return;
	}

	// Trying to write to the book.
	CItemMessagePtr pText = REF_CAST(CItemMessage,pBook);
	if ( pText == NULL || ! pBook->IsBookWritable()) // not blank ?
		return;

	int iLines = pEvent->BookPage.m_page[0].m_lines;
	DEBUG_CHECK( iLines <= 8 );
	DEBUG_CHECK( pEvent->BookPage.m_pages == 1 );

	if ( ! iLines || iPage <= 0 || iPage > MAX_BOOK_PAGES )
		return;
	if ( iLines > MAX_BOOK_LINES ) iLines = MAX_BOOK_LINES;
	iPage --;

	int len = 0;
	TCHAR szTemp[ CSTRING_MAX_LEN ];
	for ( int i=0; i<iLines; i++ )
	{
		len += strcpylen( szTemp+len, pEvent->BookPage.m_page[0].m_text+len );
		szTemp[len++] = '\t';
	}

	szTemp[--len] = '\0';
	pText->SetPageText( iPage, szTemp );
}

void CClient::Event_Item_Pickup( CSphereUID uid, int amount ) // Client grabs an item
{
	// Player/client is picking up an item.

	if ( g_Log.IsLogged( LOGL_TRACE ))
	{
		DEBUG_MSG(( "%x:Event_Item_Pickup %d" LOG_CR, m_Socket.GetSocket(), uid ));
	}

	CItemPtr pItem = g_World.ItemFind(uid);
	if ( pItem == NULL || pItem->IsWeird())
	{
		addItemDragCancel(0);
		addObjectRemove( uid );
		return;
	}

	// Where is the item coming from ? (just in case we have to toss it back)
	CObjBasePtr pObjParent = PTR_CAST(CObjBase,pItem->GetParent());
	m_Targ.m_PrvUID = ( pObjParent ) ? (DWORD) pObjParent->GetUID() : UID_INDEX_CLEAR;
	m_Targ.m_pt = pItem->GetUnkPoint();

	amount = m_pChar->ItemPickup( pItem, amount );
	if ( amount < 0 )
	{
		addItemDragCancel(0);
		return;
	}

	addPause();
	SetTargMode( CLIMODE_DRAG );
	m_Targ.m_UID = uid;
}

void CClient::Event_Item_Drop( CSphereUID uidItem, CPointMap pt, CSphereUID uidOn )
{
	// Client drops an item.
	// This started from the Event_Item_Pickup()

	ASSERT( m_pChar );

	CItemPtr pItem = g_World.ItemFind(uidItem);
	pt.m_mapplane = m_pChar->GetTopMap();
	CObjBasePtr pObjOn = g_World.ObjFind(uidOn);

	if ( g_Log.IsLogged( LOGL_TRACE ))
	{
		DEBUG_MSG(( "%x:Event_Item_Drop %lx on %lx, x=%d, y=%d" LOG_CR,
			m_Socket.GetSocket(), uidItem, uidOn, pt.m_x, pt.m_y ));
	}

	// Are we out of sync ?
	if ( pItem == NULL ||
		pItem == pObjOn ||	// silliness.
		GetTargMode() != CLIMODE_DRAG ||
		pItem != m_pChar->LayerFind( LAYER_DRAGGING ))
	{
		addItemDragCancel(5);
		return;
	}

	ClearTargMode();	// done dragging
	addPause();

	if ( pObjOn != NULL )	// Put on or in another object
	{
		if ( ! m_pChar->CanTouch( pObjOn ))	// Must also be LOS !
		{
			goto cantdrop;
		}

		if ( pObjOn->IsChar())	// Drop on a chars head.
		{
			CCharPtr pChar = REF_CAST(CChar,pObjOn);
			if ( pChar != m_pChar )
			{
				if ( ! Cmd_SecureTrade( pChar, pItem ))
					goto cantdrop;
				return;
			}

			// dropped on myself. Get my Pack.
			pObjOn = m_pChar->GetPackSafe();
		}

		// On a container item ?
		CItemContainerPtr pContItem = REF_CAST( CItemContainer, pObjOn );

		// Is the object on a person ? check the weight.
		CObjBasePtr pObjTop = pObjOn->GetTopLevelObj();
		if ( pObjTop->IsChar())
		{
			CCharPtr pChar = REF_CAST( CChar, pObjTop );
			ASSERT(pChar);
			if ( ! pChar->NPC_IsOwnedBy( m_pChar ))
			{
				// Slyly dropping item in someone elses pack.
				// or just dropping on their trade window.
				if ( ! Cmd_SecureTrade( pChar, pItem ))
					goto cantdrop;
				return;
			}
			if ( ! pChar->m_pPlayer.IsValidNewObj())
			{
				// newbie items lose newbie status when transfered to NPC
				pItem->ClrAttr(ATTR_NEWBIE|ATTR_OWNED);
			}
			if ( pChar->GetBank()->IsItemInside( pContItem ))
			{
				// Diff Weight restrict for bank box and items in the bank box.
				if ( ! pChar->GetBank()->CanContainerHold( pItem, m_pChar ))
					goto cantdrop;
			}
			else if ( ! pChar->CanCarry( pItem ))
			{
				// WriteString( "That is too heavy" );
				goto cantdrop;
			}
		}

		if ( pContItem != NULL )
		{
			// NOTE: Client only allows this sort of drop on.
			// Putting it into some sort of container.
			if ( ! pContItem->CanContainerHold( pItem, m_pChar ))
				goto cantdrop;
		}
		else
		{
			// dropped on top of a non container item.
			// can i pile them ?
			// Still in same container.
			CItemPtr pItemOn = REF_CAST(CItem,pObjOn);
			ASSERT(pItemOn);
			pObjOn = pItemOn->GetContainer();
			pt = pItemOn->GetUnkPoint();
			if ( ! pItem->Stack( pItemOn ))
			{
				if ( pItemOn->IsType(IT_SPELLBOOK))
				{
					if ( pItemOn->AddSpellbookScroll( pItem ))
					{
						WriteString( "Can't add this to the spellbook" );
						goto cantdrop;
					}
					addSound( 0x057, pItemOn );	// add to inv sound.
					return;
				}

				// Just drop on top of the current item.
				// Client probably doesn't allow this anyhow.
			}
		}
	}
	else
	{
		if ( ! m_pChar->CanTouch( pt ))	// Must also be LOS !
		{
		cantdrop:
			// The item was in the LAYER_DRAGGING.
			// Try to bounce it back to where it came from.
			m_pChar->ItemBounce( pItem );
			return;
		}
	}

	// Game pieces can only be droped on their game boards.
	if ( pItem->IsType(IT_GAME_PIECE))
	{
		if ( pObjOn == NULL || m_Targ.m_PrvUID != pObjOn->GetUID())
		{
			CItemPtr pGame2 = g_World.ItemFind(m_Targ.m_PrvUID);
			CItemContainerPtr pGame = REF_CAST( CItemContainer, pGame2 );
			if ( pGame != NULL )
			{
				pGame->ContentAdd( pItem, m_Targ.m_pt );
			}
			else
			{
				DEBUG_CHECK( 0 );
				pItem->DeleteThis();	// Not sure what else to do with it.
			}
			return;
		}
	}

	// do the dragging anim for everyone else to see.

	if ( pObjOn != NULL )
	{
		// in pack or other CItemContainer.
		m_pChar->UpdateDrag( pItem, pObjOn );
		CItemContainerPtr pContOn = REF_CAST( CItemContainer, pObjOn);
		ASSERT(pContOn);
		pContOn->ContentAdd( pItem, pt );
		addSound( pItem->GetDropSound( pObjOn ));
	}
	else
	{
		// on ground
		m_pChar->UpdateDrag( pItem, NULL, &pt );
		m_pChar->ItemDrop( pItem, pt );
	}
}

void CClient::Event_Item_Equip( CSphereUID uidItem, LAYER_TYPE layer, CSphereUID uidChar ) // Item is dropped on paperdoll
{
	// This started from the Event_Item_Pickup()

	CItemPtr pItem = g_World.ItemFind(uidItem);
	CCharPtr pChar = g_World.CharFind(uidChar);

	if ( pItem == NULL ||
		GetTargMode() != CLIMODE_DRAG ||
		pItem != m_pChar->LayerFind( LAYER_DRAGGING ))
	{
		// I have no idea why i got here.
		addItemDragCancel(5);
		return;
	}

	ClearTargMode(); // done dragging.

	if ( pChar == NULL ||
		layer >= LAYER_HORSE )	// Can't equip this way.
	{
	cantequip:
		// The item was in the LAYER_DRAGGING.
		m_pChar->ItemBounce( pItem );	// put in pack or drop at our feet.
		return;
	}

	if ( ! pChar->NPC_IsOwnedBy( m_pChar ))
	{
		// trying to equip another char ?
		// can if he works for you.
		// else just give it to him ?
		goto cantequip;
	}

	if ( pChar->ItemEquip( pItem, m_pChar ) != NO_ERROR )
	{
		goto cantequip;
	}

#ifdef _DEBUG
	if ( g_Log.IsLogged( LOGL_TRACE ))
	{
		DEBUG_MSG(( "%x:Item %s equipped on layer %i." LOG_CR, m_Socket.GetSocket(), (LPCTSTR) pItem->GetResourceName(), layer ));
	}
#endif
}

void CClient::Event_Skill_Locks( const CUOEvent* pEvent )
{
	// Skill lock buttons in the skills window.
	ASSERT( GetChar());
	ASSERT( GetChar()->m_pPlayer.IsValidNewObj() );
	DEBUG_CHECK( ! m_ProtoVer.GetCryptVer() || m_ProtoVer.GetCryptVer() >= 0x126020 );

	int len = pEvent->Skill.m_len;
	len -= 3;
	for ( int i=0; len; i++ )
	{
		SKILL_TYPE index = (SKILL_TYPE)(WORD) pEvent->Skill.skills[i].m_index;
		SKILLLOCK_TYPE state = (SKILLLOCK_TYPE) pEvent->Skill.skills[i].m_lock;

		GetChar()->m_pPlayer->Skill_SetLock( index, state );

		len -= sizeof( pEvent->Skill.skills[0] );
	}
}

void CClient::Event_Skill_Use( SKILL_TYPE skill ) // Skill is clicked on the skill list
{
	// All the push button skills come through here.
	// Any "Last skill" macro comes here as well. (push button only)

	bool fContinue = false;

	if ( m_pChar->Skill_Wait(skill))
		return;

	SetTargMode();
	m_Targ.m_UID.InitUID();	// This is a start point for targ more.

	CSkillDef* pSkillDef = g_Cfg.GetSkillDef(skill);

	bool fCheckCrime;

	switch ( skill )
	{
	case SKILL_ARMSLORE:
	case SKILL_ITEMID:
	case SKILL_ANATOMY:
	case SKILL_ANIMALLORE:
	case SKILL_EVALINT:
	case SKILL_FORENSICS:
	case SKILL_TASTEID:

	case SKILL_BEGGING:
	case SKILL_TAMING:
	case SKILL_RemoveTrap:
		fCheckCrime = false;

dotargetting:
		// Go into targtting mode.
		ASSERT(pSkillDef);
		if ( pSkillDef->m_sTargetPrompt.IsEmpty())
		{
			DEBUG_ERR(( "%x: Event_Skill_Use no prompt skill %d" LOG_CR, m_Socket.GetSocket(), skill ));
			return;
		}

		m_Targ.m_tmSkillTarg.m_Skill = skill;	// targetting what skill ?
		addTarget( CLIMODE_TARG_SKILL, pSkillDef->m_sTargetPrompt, false, fCheckCrime );
		return;

	case SKILL_ENTICEMENT:
	case SKILL_PROVOCATION:
		if ( m_pChar->ContentFind( CSphereUID(RES_TypeDef,IT_MUSICAL), 0, 255 ) == NULL )
		{
			WriteString( "You have no musical instrument available" );
			return;
		}
	case SKILL_STEALING:
	case SKILL_POISONING:
		// Go into targeting mode.
		fCheckCrime = true;
		goto dotargetting;

	case SKILL_PEACEMAKING:
	case SKILL_Stealth:	// How is this supposed to work.
	case SKILL_HIDING:
	case SKILL_SPIRITSPEAK:
	case SKILL_DETECTINGHIDDEN:
	case SKILL_MEDITATION:
		// These start/stop automatically.
		m_pChar->Skill_Start(skill);
		return;

	case SKILL_TRACKING:
		Cmd_Skill_Tracking( -1, false );
		break;

	case SKILL_CARTOGRAPHY:
		// Menu select for map type.
		Cmd_Skill_Cartography( 0 );
		break;

	case SKILL_INSCRIPTION:
		// Menu select for spell type.
		Cmd_Skill_Inscription();
		break;

	default:
		Printf( "There is no skill %d. Please tell support you saw this message.", skill );
		break;
	}
}

void CClient::Event_Walking( DIR_TYPE dir, bool fRun, BYTE bWalkCount, DWORD dwEcho ) // Player moves
{
	// XCMD_Walk
	// The client sometimes echos 1 or 2 zeros or invalid echos when you first start
	//	walking (the invalid non zeros happen when you log off and don't exit the
	//	client.exe all the way and then log back in, XXX doesn't clear the stack)

	ASSERT(m_pChar);

	if ( m_pChar->IsStatFlag( STATF_Freeze | STATF_Stone | STATF_Immobile ) &&
		m_pChar->OnFreezeCheck(0))
	{
		addPlayerWalkCancel();
		return;
	}

	if ( bWalkCount != (BYTE)( m_wWalkCount+1 ))	// && bWalkCount != 255
	{
		// DEBUG_MSG(( "%x: New Walk Count %d!=%d" LOG_CR, m_Socket.GetSocket(), bWalkCount, m_wWalkCount ));
		if ( (WORD)(m_wWalkCount) == 0xFFFF )
			return;	// just playing catch up with last reset. don't cancel again.
		addPlayerWalkCancel();
		return;
	}

	if ( dir >= DIR_QTY )
	{
		addPlayerWalkCancel();
		return;
	}

	m_pChar->StatFlag_Mod( STATF_Fly, fRun );

	CPointMap pt = m_pChar->GetTopPoint();
	CPointMap ptold = pt;
	bool fMove = true;

	if ( dir == m_pChar->m_dirFace )
	{
		// Move in this dir.
		m_iWalkStepCount ++;
		if (( m_iWalkStepCount & 7 ) == 0 )	// we have taken 8 steps ? direction changes don't bWalkCount.
		{
			// Throttle walk speed.
			// Client only allows 4 steps of walk ahead.
			if ( bWalkCount )
			{
				CServTime currenttime = CServTime::GetCurrentTime();
				int iTimeDiff = currenttime.GetTimeDiff( m_timeWalkStep );

				// What is the best speed we should be capable of ?
				// 1/10 of a sec per step is = Horse mount.
				int iTimeMin;
				if ( fRun )
				{
					if ( m_pChar->IsStatFlag( STATF_OnHorse ))
						iTimeMin = 7;
					else
						iTimeMin = 14;
				}
				else
				{
					if ( m_pChar->IsStatFlag( STATF_OnHorse ))
						iTimeMin = 10;
					else
						iTimeMin = 20;
				}

				if ( iTimeDiff < iTimeMin && iTimeDiff >= 0 && ! IsPrivFlag(PRIV_GM))	// TICKS_PER_SEC
				{
					// walking too fast.
					DEBUG_MSG(( "%x: Fast Walk ?" LOG_CR, m_Socket.GetSocket() ));
					addPlayerWalkCancel();
					m_iWalkStepCount--; // eval again next time !
					return;
				}
			}
			m_timeWalkStep.InitTimeCurrent();
		}

		// Check if we have gone indoors.
		bool fRoof = m_pChar->IsStatFlag( STATF_InDoors );

		// Check the z height here.
		// The client already knows this but doesn't tell us.
		if ( ! m_pChar->CheckMoveWalkDir( pt, dir, true ))
		{
			addPlayerWalkCancel();
			return;
		}

		// Are we invis ?
		m_pChar->CheckRevealOnMove();
		m_pChar->MoveToChar( pt );

		// Should i update the weather ?
		if ( fRoof != m_pChar->IsStatFlag( STATF_InDoors ))
		{
			addWeather( WEATHER_DEFAULT );
		}

		// did i step on a telepad, trap, etc ?
		if ( m_pChar->CheckLocation())
		{
			// We stepped on teleporter
			return;
		}
	}
	else
	{
		// Just a change in dir.
		m_pChar->m_dirFace = dir;
		fMove = false;
	}

	// Ack the move. ( if this does not go back we get rubber banding )
	m_wWalkCount = bWalkCount;	// m_wWalkCount++

	CUOCommand cmd;
	cmd.WalkAck.m_Cmd = XCMD_WalkAck;
	cmd.WalkAck.m_count = (BYTE) m_wWalkCount;
	// Not really sure what this does.
	cmd.WalkAck.m_flag = ( m_pChar->IsStatFlag( STATF_Insubstantial | STATF_Invisible | STATF_Hidden | STATF_Sleeping )) ?
		0 : 0x41;
	xSendPkt( &cmd, sizeof( cmd.WalkAck ));
	xFlush();	// Walk acks must be sent immediately — client rubber-bands otherwise

	if ( ! fMove )
	{
		// Show others I have turned !!
		m_pChar->UpdateMode( this );
		return;
	}

	// Who now sees me ?
	m_pChar->UpdateMove( ptold, this );

	// What new stuff do I now see ?
	addPlayerSee( ptold );
}

void CClient::Event_CombatMode( bool fWar ) // Only for switching to combat mode
{
	// If peacmaking then this doens't work ??
	// Say "you are feeling too peacefull"

	m_pChar->StatFlag_Mod( STATF_War, fWar );

	if ( m_pChar->IsStatFlag( STATF_DEAD ))
	{
		// Manifest the ghost.
		// War mode for ghosts.
		m_pChar->StatFlag_Mod( STATF_Insubstantial, ! fWar );
	}

	m_pChar->Skill_Fail( true );	// clean up current skill.
	if ( ! fWar )
	{
		m_pChar->Fight_ClearAll();
	}

	addPlayerWarMode();
	m_pChar->UpdateMode( this, m_pChar->IsStatFlag( STATF_DEAD ));
}

bool CClient::Event_Command( LPCTSTR pszCommand ) // Client entered a console command like /ADD
{
	// Get a spoken /command.
	// Copy it to temp buffer.

	ASSERT(pszCommand);
	if ( pszCommand[0] == '\0' )
		return false;

	// Assume you don't mean yourself !
	static LPCTSTR const sm_szCmd_Redirect[] =		// default to redirect these.
	{
		"BANK",
		"CONTROL",
		"DUPE",
		"FORGIVE",
		"JAIL",
		"KICK",
		"KILL",
		"NUDGEDOWN",
		"NUDGEUP",
		"PARDON",
		"PRIVSET",
		"PROPS",
		"REMOVE",
		"SHRINK",
		"TWEAK",
		NULL,
	};

	if ( s_FindKeyInTable( pszCommand, sm_szCmd_Redirect ) >= 0 )
	{
		// targetted verbs are logged once the target is selected.
		// Is this command avail for your priv level (or lower) ?
		return addTargetVerb( pszCommand, NULL );
	}

	// targ base self.
	return OnTarg_Obj_Command( m_pChar, pszCommand );
}

void CClient::Event_Attack( CSphereUID uid )
{
	// d-click in war mode
	// I am attacking someone.

	// ??? Combat music = MIDI = 37,38,39

	CCharPtr pChar = g_World.CharFind(uid);
	if ( pChar == NULL )
		return;

	// Accept or decline the attack.
	CUOCommand cmd;
	cmd.AttackOK.m_Cmd = XCMD_AttackOK;
	cmd.AttackOK.m_UID = (m_pChar->Fight_Attack( pChar )) ? (DWORD) pChar->GetUID() : 0;
	xSendPkt( &cmd, sizeof( cmd.AttackOK ));
}

void CClient::Event_VendorBuy( CSphereUID uidVendor, const CUOEvent* pEvent )
{
	// Client/Player buying items from the Vendor

	if ( pEvent->VendorBuy.m_flag == 0 )	// just a close command.
		return;

	CCharPtr pVendor = g_World.CharFind(uidVendor);
	if ( ! m_pChar->CanTouch( pVendor ))
	{
		WriteString( "You can't reach the vendor" );
		return;
	}

	if ( ! pVendor->NPC_IsVendor())	// cheat
		return;

	int iConvertFactor = pVendor->NPC_GetVendorMarkup(m_pChar);

	// Calculate the total cost of goods.
	int costtotal=0;
	bool fSoldout = false;
	int nItems = (pEvent->VendorBuy.m_len - 8) / sizeof( pEvent->VendorBuy.m_item[0] );
	int i=0;
	for ( ;i<nItems;i++)
	{
		CSphereUID uid( pEvent->VendorBuy.m_item[i].m_UID );
		CItemVendablePtr pItem = REF_CAST(CItemVendable,g_World.ItemFind(uid));
		if ( pItem == NULL )
			continue;	// ignore it for now.

		// Do cheat/sanity check on goods sold.
		long iPrice = pItem->GetVendorPrice(iConvertFactor);
		WORD iQty = pEvent->VendorBuy.m_item[i].m_amount;
		if ( ! iPrice ||
			pItem->GetTopLevelObj() != pVendor ||
			iQty > pItem->GetAmount())
		{
			fSoldout = true;
			continue;
		}
		costtotal += iQty* iPrice;
	}

	if ( fSoldout )
	{
		pVendor->Speak( "Alas, I don't have all those goods in stock.  Let me know if there is something else thou wouldst buy." );
		// ??? Update the vendors list and try again.
		return;
	}

	bool fBoss = pVendor->NPC_IsOwnedBy( m_pChar );
	if ( ! fBoss )
	{
		if ( m_pChar->ContentConsume( CSphereUID(RES_TypeDef,IT_GOLD), costtotal, true ))
		{
			pVendor->Speak( "Alas, thou dost not possess sufficient gold for this purchase!" );
			return;
		}
	}

	if ( costtotal <= 0 )
	{
		pVendor->Speak( "You have bought nothing. But feel free to browse" );
		return;
	}

	CItemContainerPtr pVendExtra = m_pChar->GetBank( LAYER_VENDOR_EXTRA );
	CItemContainerPtr pPack = m_pChar->GetPackSafe();

	// Move the items bought into your pack.
	for ( i=0;i<nItems;i++)
	{
		CSphereUID uid( pEvent->VendorBuy.m_item[i].m_UID );
		CItemVendablePtr pItem = REF_CAST( CItemVendable, g_World.ItemFind(uid));
		if ( pItem == NULL )
			continue;	// ignore it i guess.
		if ( ! pItem->IsValidSaleItem( true ))
			continue;	// sorry can't buy this !

		WORD iQty = pEvent->VendorBuy.m_item[i].m_amount;
		pItem->SetAmount( pItem->GetAmount() - iQty );

		switch ( pItem->GetType() )
		{
		case IT_FIGURINE:
			// Buying animals is special.
			m_pChar->Use_Figurine( pItem, 2 );
			goto do_consume;
		case IT_BEARD:
			if ( m_pChar->GetDispID() != CREID_MAN )
			{
			nohair:
				pVendor->Speak( "Sorry not much i can do for you" );
				continue;
			}
		case IT_HAIR:
			// Must be added directly. can't exist in pack!
			if ( ! m_pChar->IsHuman())
				goto nohair;
			{
				CItemPtr pItemHair = CItem::CreateDupeItem( pItem );
				m_pChar->LayerAdd(pItemHair, ( pItem->GetType() == IT_HAIR ) ? LAYER_HAIR : LAYER_BEARD );
				pItemHair->SetTimeout( 55000*TICKS_PER_SEC );	// set the grow timer.
				pVendor->UpdateAnimate(ANIM_ATTACK_1H_WIDE);
				m_pChar->Sound( SOUND_SNIP );	// snip noise.
			}
			continue;
		}

		if ( iQty > 1 && ! pItem->Item_GetDef()->IsStackableType())
		{
			while ( iQty -- )
			{
				CItemPtr pItemNew = CItem::CreateDupeItem( pItem );
				pItemNew->SetAmount(1);
				pPack->ContentAdd(pItemNew);
			}
		}
		else
		{
			CItemPtr pItemNew = CItem::CreateDupeItem( pItem );
			pItemNew->SetAmount( iQty );
			pPack->ContentAdd( pItemNew );
		}

	do_consume:
		if ( pItem->GetAmount() == 0 &&
			( pVendor->IsStatFlag( STATF_Pet ) ||
			( pVendExtra && pVendExtra->IsItemInside(pItem))))
		{
			// we can buy it all.
			// not allowed to delete all from LAYER_VENDOR_STOCK
			pItem->DeleteThis();
		}
		else
		{
			pItem->Update();
		}
	}

	CGString sMsg;
	sMsg.Format(
		fBoss ?
		"Here you are, %s. "
		"That is %d gold coin%s worth of goods." :
		"Here you are, %s. "
		"That will be %d gold coin%s. "
		"I thank thee for thy business.",
		(LPCTSTR) m_pChar->GetName(), costtotal, (costtotal==1) ? "" : "s" );
	pVendor->Speak( sMsg );

	// Take the gold.
	if ( ! fBoss )
	{
		m_pChar->ContentConsume( CSphereUID(RES_TypeDef,IT_GOLD), costtotal );

		// Add the gold to the vendors total to play with.
		pVendor->GetBank()->m_itEqBankBox.m_Check_Amount += costtotal;
	}

	// Clear the vendor display.
	addVendorClose(pVendor);
	if ( i )
	{
		addSound( 0x057 );	// add to inv sound.
	}
}

bool CClient::Event_VendorSell( CSphereUID uidVendor, int iSellCount, const CUOEventSell* pSellItems )
{
	// Player Selling items to the vendor.
	// Done with the selling action.

	CCharPtr pVendor = g_World.CharFind(uidVendor);
	if ( ! m_pChar->CanTouch( pVendor ))
	{
		WriteString( "Too far away from the Vendor" );
		return false;
	}
	if ( iSellCount <= 0 )
	{
		addVendorClose( pVendor );
		// pVendor->Speak( "You have sold nothing" );
		return false;
	}

	if ( ! pVendor->NPC_IsVendor())	// cheat
		return false;

	CItemContainerPtr pBank = pVendor->GetBank();
	CItemContainerPtr pContStock = pVendor->GetBank( LAYER_VENDOR_STOCK );
	CItemContainerPtr pContBuy = pVendor->GetBank( LAYER_VENDOR_BUYS );
	CItemContainerPtr pContExtra = pVendor->GetBank( LAYER_VENDOR_EXTRA );
	if ( pBank == NULL || pContStock == NULL || pContExtra == NULL )
	{
		addVendorClose( pVendor );
		pVendor->Speak( "Ahh, Guards my goods are gone !!" );
		return false;
	}

	int iConvertFactor = -pVendor->NPC_GetVendorMarkup(m_pChar);

	// MAX_ITEMS_CONT ???
	int iGold = 0;
	bool fShortfall = false;

	for ( int i=0; i<iSellCount; i++ )
	{
		CSphereUID uid( pSellItems[i].m_UID );
		CItemVendablePtr pItem = REF_CAST(CItemVendable,g_World.ItemFind(uid));
		if ( pItem == NULL )
			continue;

		// Do we still have it ? (cheat check)
		if ( pItem->GetTopLevelObj() != m_pChar )
			continue;

		// Find the valid sell item from vendors stuff.
		CItemVendablePtr pItemSample = CChar::NPC_FindVendableItem( pItem, pContBuy, pContStock );
		if ( pItemSample == NULL )
			continue;

		// How many of these does the vendor actually want ?
		// Subtract what i already have on hand !
		int amounttoBuy;
		if ( pItemSample->GetParent() == pContBuy )
		{
			amounttoBuy = pItemSample->GetAmount() - pItemSample->GetContainedLayer();
		}
		else
		{
			amounttoBuy = pItemSample->GetContainedLayer()* 2;
		}
		if ( amounttoBuy <= 0 )
			continue;

		// Now how much did i say i wanted to sell ?
		int amount = pSellItems[i].m_amount;
		if ( pItem->GetAmount() < amount )	// Selling more than i have ?
		{
			amount = pItem->GetAmount();
		}
		if ( amount > amounttoBuy )
			amount = amounttoBuy;

		long iPrice = pItemSample->GetVendorPrice(iConvertFactor)* amount;

		// Can vendor afford this ?
		if ( iPrice > pBank->m_itEqBankBox.m_Check_Amount )
		{
			fShortfall = true;
			break;
		}
		pBank->m_itEqBankBox.m_Check_Amount -= iPrice;

		// give them the appropriate amount of gold.
		iGold += iPrice;

		// Take the items from player.
		// Put items in vendor inventory.
		CItemPtr pItemNew;
		if ( amount >= pItem->GetAmount())
		{
			// Transfer all.
			pItem->RemoveFromView();
			pItemNew = pItem;
		}
		else
		{
			// Just part of the stack.
			pItemNew = CItem::CreateDupeItem( pItem );
			pItemNew->SetAmount( amount );
			pItem->SetAmountUpdate( pItem->GetAmount() - amount );
		}

		// Record how many of these we have now bought.

		if ( pItemSample->GetParent() == pContBuy )
		{
			pItemSample->SetContainedLayer( pItemSample->GetContainedLayer() + amount );
			pContExtra->ContentAdd( pItemNew );
		}
		else
		{
			pItemSample->SetAmount( pItemSample->GetAmount() + amount );
			pItemNew->DeleteThis();
		}
	}

	if ( iGold )
	{
		CGString sMsg;
		sMsg.Format( "Here you are, %d gold coin%s. "
			"I thank thee for thy business.",
			iGold, (iGold==1) ? "" : "s" );
		pVendor->Speak( sMsg );

		if ( fShortfall )
		{
			pVendor->Speak( "Alas I have run out of money." );
		}

		m_pChar->AddGoldToPack( iGold );
	}
	else
	{
		if ( fShortfall )
		{
			pVendor->Speak( "I cannot afford any more at the moment" );
		}
	}

	addVendorClose( pVendor );
	return( iGold );
}

void CClient::Event_BBoardRequest( CSphereUID uid, const CUOEvent* pEvent )
{
	// Answer a request reguarding the BBoard.
	// addBulletinBoard

	CItemPtr pBoard2 = g_World.ItemFind(uid);
	CItemContainerPtr pBoard = REF_CAST( CItemContainer, pBoard2 );
	if ( ! m_pChar->CanSee( pBoard ))
	{
		addObjectRemoveCantSee( uid, "the board" );
		return;
	}

	ASSERT(pBoard);
	DEBUG_CHECK( pBoard->IsType(IT_BBOARD));
	CSphereUID uidMsg( (DWORD)( pEvent->BBoard.m_UIDMsg ) );

	switch ( pEvent->BBoard.m_flag )
	{
	case BBOARDF_REQ_FULL:
	case BBOARDF_REQ_HEAD:
		// request for message header and/or body.
		if ( pEvent->BBoard.m_len != 0x0c )
		{
			DEBUG_ERR(( "%x:BBoard feed back message bad length %d" LOG_CR, m_Socket.GetSocket(), (int) pEvent->BBoard.m_len ));
			return;
		}
		if ( ! addBBoardMessage( pBoard, (BBOARDF_TYPE) pEvent->BBoard.m_flag, uidMsg ))
		{
			// sanity check fails.
			addObjectRemoveCantSee( (DWORD)( pEvent->BBoard.m_UIDMsg ), "the message" );
			return;
		}
		break;
	case BBOARDF_NEW_MSG:
		// Submit a message
		if ( pEvent->BBoard.m_len < 0x0c )
		{
			DEBUG_ERR(( "%x:BBoard feed back message bad length %d" LOG_CR, m_Socket.GetSocket(), (int) pEvent->BBoard.m_len ));
			return;
		}
		if ( ! m_pChar->CanTouch( pBoard ))
		{
			WriteString( "You can't reach the bboard" );
			return;
		}
		if ( pBoard->GetCount() > 32 )
		{
			// Role a message off.
			static_cast<CItem*>(pBoard->GetAt(pBoard->GetCount()-1))->DeleteThis();
		}
		// if pMsgItem then this is a reply to it !
		{
			CItemPtr pItemNew = CItem::CreateBase( ITEMID_BBOARD_MSG );
			CItemMessagePtr pMsgNew = REF_CAST(CItemMessage,pItemNew);
			if ( pMsgNew == NULL )
			{
				DEBUG_ERR(( "%x:BBoard can't create message item" LOG_CR, m_Socket.GetSocket()));
				return;
			}

			int lenstr = pEvent->BBoard.m_data[0];
			pMsgNew->SetName( (LPCTSTR) &pEvent->BBoard.m_data[1] );
			// pMsgNew->m_itBook.m_Time = CServTime::GetCurrentTime(); // field removed
			pMsgNew->m_sAuthor = m_pChar->GetName();
			pMsgNew->m_uidLink = m_pChar->GetUID();	// Link it to you forever.

			int len = 1 + lenstr;
			int lines = pEvent->BBoard.m_data[len++];
			if ( lines > 32 ) lines = 32;	// limit this.

			while ( lines-- )
			{
				lenstr = pEvent->BBoard.m_data[len++];
				pMsgNew->AddPageText( (LPCTSTR) &pEvent->BBoard.m_data[len] );
				len += lenstr;
			}

			pBoard->ContentAdd(pItemNew);
		}
		break;

	case BBOARDF_DELETE:
		// remove the msg. (if it is yours?)
		{
			CItemPtr pMsgItem2 = g_World.ItemFind(uidMsg);
			CItemMessagePtr pMsgItem = REF_CAST(CItemMessage,pMsgItem2);
			if ( ! pBoard->IsItemInside( pMsgItem ))
			{
				WriteString( "Corrupt bboard message" );
				return;
			}
			if ( ! IsPrivFlag(PRIV_GM) && pMsgItem->m_uidLink != m_pChar->GetUID())
			{
				WriteString( "This board message is not yours" );
				return;
			}

			pMsgItem->DeleteThis();
		}
		break;

	default:
		DEBUG_ERR(( "%x:BBoard unknown flag %d" LOG_CR, m_Socket.GetSocket(), (int) pEvent->BBoard.m_flag ));
		return;
	}
}

void CClient::Event_SecureTrade( CSphereUID uid, SECURE_TRADE_TYPE action, bool fCheck )
{
	// pressed a button on the secure trade window.

	CItemPtr pItem = g_World.ItemFind(uid);
	CItemContainerPtr pCont = REF_CAST( CItemContainer, pItem );
	if ( pCont == NULL )
	{
		return;
	}
	if ( m_pChar != pCont->GetParent() || ! pCont->IsType(IT_EQ_TRADE_WINDOW))
	{
		DEBUG_ERR(( "%x:Trade window weirdness" LOG_CR, m_Socket.GetSocket() ));
		return;
	}

	// perform the trade.
	switch ( action )
	{
	case SECURE_TRADE_CLOSE: // Cancel trade.  Send each person cancel messages, move items.
		pCont->DeleteThis();
		return;
	case SECURE_TRADE_CHANGE: // Change check marks.  Possibly conclude trade
		if ( m_pChar->GetDist( pCont ) > SPHEREMAP_VIEW_SIZE )
		{
			// To far away.
			WriteString( "You are too far away to trade items" );
			return;
		}
		pCont->Trade_Status( fCheck );
		return;
	}
}

void CClient::Event_Profile( BYTE fWriteMode, CSphereUID uid, const CUOEvent* pEvent )
{
	// XCMD_CharProfile
	// mode = 0 = Get profile, 1 = Set profile

	// DEBUG_MSG(( "%x:XCMD_CharProfile" LOG_CR, m_Socket.GetSocket()));

	CCharPtr pChar = g_World.CharFind(uid);
	if ( pChar == NULL )
		return;
	CSphereExpArgs exec(pChar, pChar, 3);
	if ( pChar->OnTrigger( CCharDef::T_UserButton, exec) == TRIGRET_RET_VAL )
		return;

	if ( fWriteMode )
	{
		// write stuff to the profile.
		ASSERT( pEvent->CharProfile.m_Cmd == XCMD_CharProfile );
		if ( m_pChar != pChar )
		{
			if ( ! IsPrivFlag(PRIV_GM))
				return;
			if ( m_pChar->GetPrivLevel() < pChar->GetPrivLevel())
				return;
		}

		const int iSizeAll = sizeof(pEvent->CharProfile);
		const int iSizeTxt = sizeof(pEvent->CharProfile.m_utext);

		int len = pEvent->CharProfile.m_len;
		if ( len <= (sizeof(pEvent->CharProfile)-sizeof(pEvent->CharProfile.m_utext)))
			return;

		int iTextLen = pEvent->CharProfile.m_textlen;
		if ( iTextLen*sizeof(NCHAR) != len - (sizeof(pEvent->CharProfile)-sizeof(pEvent->CharProfile.m_utext)) )
			return;

		// BYTE pEvent->CharProfile.m_unk1;	// 8
		BYTE retcode = pEvent->CharProfile.m_retcode;	// 0=canceled, 1=okayed or something similar???

		TCHAR szProfile[CSTRING_MAX_LEN-16];
		int iWLen = CvtNUNICODEToSystem( szProfile, COUNTOF(szProfile), pEvent->CharProfile.m_utext, iTextLen );

		TCHAR szProfile2[CSTRING_MAX_LEN-16];
		Str_EscSeqAdd( szProfile2, szProfile, COUNTOF(szProfile2));

		pChar->m_TagDefs.SetKeyStr( "PROFILE", szProfile2 );
		return;
	}

	bool fIncognito = m_pChar->IsStatFlag( STATF_Incognito ) && ! IsPrivFlag(PRIV_GM);

	// addCharProfile();
	CUOCommand cmd;

	cmd.CharProfile.m_Cmd = XCMD_CharProfile;
	cmd.CharProfile.m_UID = uid;

	int len = strcpylen( cmd.CharProfile.m_title, (LPCTSTR) pChar->GetName());
	len++;

	CGString sConstText;
	sConstText.Format( "%s, %s", (LPCTSTR) pChar->Noto_GetTitle(), (LPCTSTR) pChar->GetTradeTitle());

	int iWLen = CvtSystemToNUNICODE(
		(NCHAR *) ( cmd.CharProfile.m_title + len ), 1024,
		sConstText );

	len += (iWLen+1)*sizeof(NCHAR);

	LPCTSTR pszProfile = fIncognito ? "" : ((LPCTSTR) pChar->m_TagDefs.FindKeyStr( "PROFILE" ));

	TCHAR szProfile2[CSTRING_MAX_LEN-16];
	Str_EscSeqRemove( szProfile2, pszProfile, COUNTOF(szProfile2));

	iWLen = CvtSystemToNUNICODE(
		(NCHAR *) ( cmd.CharProfile.m_title + len ), COUNTOF(szProfile2),
		szProfile2 );

	len += (iWLen+1)*sizeof(NCHAR);
	len += 7;
	cmd.CharProfile.m_len = len;

	xSendPkt( &cmd, len );
}

void CClient::Event_MailMsg( CSphereUID uid1, CSphereUID uid2 )
{
	// NOTE: How do i protect this from spamming others !!!
	// Drag the mail bag to this clients char.

	CCharPtr pChar = g_World.CharFind(uid1);
	if ( pChar == NULL )
	{
		WriteString( "Drop mail on other players" );
		return;
	}
	if ( pChar == m_pChar ) // this is normal (for some reason) at startup.
	{
		return;
	}
	// Might be an NPC ?
	pChar->Printf( "'%s' has dropped mail on you.", (LPCTSTR) m_pChar->GetName() );
}

void CClient::Event_ToolTip( CSphereUID uid )
{
	CObjBasePtr pObj = g_World.ObjFind(uid);
	if ( pObj == NULL )
		return;

	CSphereExpContext exec(pObj, m_pChar);
	if ( pObj->OnTrigger( "@UserToolTip", exec) == TRIGRET_RET_VAL )	// CCharDef::T_ToolTip, CItemDef::T_ToolTip
		return;

	CGString sStr;
	sStr.Format( "'%s'", (LPCTSTR) pObj->GetName());
	addToolTip( g_World.ObjFind(uid), sStr );
}

void CClient::Event_ExtData( EXTDATA_TYPE type, const CUOExtData* pData, int len )
{
	// XCMD_ExtData = 5 bytes of overhead before this.
	// DEBUG_MSG(( "%x:XCMD_ExtData t=%d,l=%d" LOG_CR, m_Socket.GetSocket(), type, len ));

	switch ( type )
	{
	case EXTDATA_UnkFromCli5:
		// Sent at start up for the party system ?
		break;
	case EXTDATA_Lang:
		// Tell what lang i use.
		GetAccount()->m_lang.Set( pData->Lang.m_code );
		break;
	case EXTDATA_Party_Msg: // = 6
		// Messages about the party we are in.
		ASSERT(m_pChar);
		switch ( pData->Party_Msg_Opt.m_code )
		{
		case PARTYMSG_Add:
			// request to add a new member. len=5. m_msg = 4x0's
			addTarget( CLIMODE_TARG_PARTY_ADD, "Who would you like to add to your party?", false, false );
			break;
		case PARTYMSG_Disband:
			if ( ! m_pChar->m_pParty.IsValidRefObj())
				return;
			m_pChar->m_pParty->Disband( m_pChar->GetUID());
			break;
		case PARTYMSG_Remove:
			// Request to remove this member of the party.
			if ( ! m_pChar->m_pParty.IsValidRefObj())
				return;
			if ( len != 5 )
				return;
			m_pChar->m_pParty->RemoveChar( (DWORD) pData->Party_Msg_Rsp.m_UID, m_pChar->GetUID());
			break;
		case PARTYMSG_MsgMember:
			// Message a specific member of my party.
			if ( ! m_pChar->m_pParty.IsValidRefObj())
				return;
			if ( len < 6 )
				return;
			m_pChar->m_pParty->MessageMember( (DWORD) pData->Party_Msg_Rsp.m_UID, m_pChar->GetUID(), pData->Party_Msg_Rsp.m_msg, len-1 );
			break;
		case PARTYMSG_Msg:
			// Send this message to the whole party.
			if ( len < 6 )
				return;
			if ( ! m_pChar->m_pParty.IsValidRefObj())
			{
				// No Party !
				// We must send a response back to the client for this or it will hang !?
				CPartyDef::MessageClient( this, m_pChar->GetUID(), (const NCHAR*)( pData->Party_Msg_Opt.m_data ), len-1 );
			}
			else
			{
				m_pChar->m_pParty->MessageAll( m_pChar->GetUID(), (const NCHAR*)( pData->Party_Msg_Opt.m_data ), len-1 );
			}
			break;
		case PARTYMSG_Option:
			// set the loot flag.
			if ( ! m_pChar->m_pParty.IsValidRefObj())
				return;
			if ( len != 2 )
				return;
			m_pChar->m_pParty->SetLootFlag( m_pChar, pData->Party_Msg_Opt.m_data[0] );
			break;
		case PARTYMSG_Accept:
			// We accept or decline the offer of an invite.
			if ( len != 5 )
				return;
			CPartyDef::AcceptEvent( m_pChar, (DWORD) pData->Party_Msg_Rsp.m_UID );
			break;

		case PARTYMSG_Decline:
			// decline party invite.
			// " You notify %s that you do not wish to join the party"
			CPartyDef::DeclineEvent( m_pChar, (DWORD) pData->Party_Msg_Rsp.m_UID );
			break;

		default:
			Printf( "Unknown party system msg %d", pData->Party_Msg_Rsp.m_code );
			break;
		}
		break;
	case EXTDATA_Arrow_Click:
		// ??? Attach this to an object and then send commands to the object !
		// CCharDef::T_UserButton
		{
		WriteString( "Follow the Arrow" );
		}
		break;
	case EXTDATA_StatusClose:
		// The status window has been closed. (need send no more updates)
		{
		// 4 bytes = uid of the char status closed.
		}
		break;

	case EXTDATA_Wrestle_DisArm:	// From Client: Wrestling disarm
	case EXTDATA_Wrestle_Stun:		// From Client: Wrestling stun
		// CCharDef::T_UserButton
		WriteString( "Sorry wrestling moves not available yet" );
		break;

	case EXTDATA_Anim:
		if ( len < 4 )
			return;
		if ( pData->Anim.m_Anim == ANIM_FIDGET_YAWN )
		{
			m_pChar->Emote( "Yawn" );
		}
		m_pChar->UpdateAnimate( (ANIM_TYPE) (int) pData->Anim.m_Anim, false );
		break;

	case EXTDATA_ContextMenuReq:
	case EXTDATA_ContextMenuSel:
		// CCharDef::T_UserButton
		DEBUG_MSG(( "Context Menu msg %d,%d.", type, len ));
		break;

	case EXTDATA_UnkFromCli15:	// LEN = 5
		if ( len == 5 )
			break;

	default:
		DEBUG_MSG(( "Unknown extended msg %d,%d.", type, len ));
		// Printf( "Unknown extended msg %d,%d.", type, len );
		break;
	}
}

void CClient::Event_ExtCmd( EXTCMD_TYPE type, const char* pszName )
{
	// parse the args.
	CString sTmp = pszName;
	TCHAR* ppArgs[2];
	Str_ParseCmdsStr( sTmp, ppArgs, COUNTOF(ppArgs), " " );

	switch ( type )
	{

	case EXTCMD_OPEN_SPELLBOOK: // 67 = open spell book if we have one.
		{
			CItemPtr pBook = m_pChar->GetSpellbook();
			if ( pBook == NULL )
			{
				WriteString( "You have no spellbook" );
				break;
			}
			// Must send proper context info or it will crash tha client.
			addContentsTopDown( REF_CAST( CItemContainer, pBook->GetContainer()));
			addItem( pBook );
			addSpellbookOpen( pBook );
		}
		break;
	case EXTCMD_ANIMATE: // Cmd_Animate
		if ( !_stricmp( ppArgs[0],"bow"))
			m_pChar->UpdateAnimate( ANIM_BOW );
		else if ( ! _stricmp( ppArgs[0],"salute"))
			m_pChar->UpdateAnimate( ANIM_SALUTE );
		else
		{
			DEBUG_ERR(( "%x:Event Animate '%s'" LOG_CR, m_Socket.GetSocket(), ppArgs[0] ));
		}
		break;

	case EXTCMD_SKILL:			// Skill
		Event_Skill_Use( (SKILL_TYPE) atoi( ppArgs[0] ));
		break;

	case EXTCMD_AUTOTARG:	// bizarre new autotarget mode.
		// "target x y z"
		{
			CSphereUID uid( atoi( ppArgs[0] ));
			CObjBasePtr pObj = g_World.ObjFind(uid);
			if ( pObj )
			{
				DEBUG_ERR(( "%x:Event Autotarget '%s' '%s'" LOG_CR, m_Socket.GetSocket(), (LPCTSTR) pObj->GetName(), ppArgs[1] ));
			}
			else
			{
				DEBUG_ERR(( "%x:Event Autotarget UNK '%s' '%s'" LOG_CR, m_Socket.GetSocket(), ppArgs[0], ppArgs[1] ));
			}
		}
		break;

	case EXTCMD_CAST_MACRO:	// macro spell.
	case EXTCMD_CAST_BOOK:	// cast spell from book.
		Cmd_Skill_Magery( (SPELL_TYPE) atoi( ppArgs[0] ), m_pChar );
		break;

	case EXTCMD_DOOR_AUTO: // open door macro = Attempt to open a door around us.
		if ( m_pChar )
		{
			CWorldSearch Area( m_pChar->GetTopPoint(), 4 );
			for(;;)
			{
				CItemPtr pItem = Area.GetNextItem();
				if ( pItem == NULL )
					break;
				switch ( pItem->GetType() )
				{
				case IT_PORT_LOCKED:	// Can only be trigered.
				case IT_PORTCULIS:
				case IT_DOOR_LOCKED:
				case IT_DOOR:
					m_pChar->Use_Item( pItem, false );
					return;
				}
			}
		}
		break;

		// case 107: // seen this but no idea what it does.

	default:
		DEBUG_ERR(( "%x:Event_ExtCmd unk %d, '%s'" LOG_CR, m_Socket.GetSocket(), type, pszName ));
	}
}

void CClient::Event_PromptResp( LPCTSTR pszText, int len )
{
	// result of addPrompt
	TCHAR szText[MAX_TALK_BUFFER];
	if ( len <= 0 )	// cancel
	{
		szText[0] = '\0';
	}
	else
	{
		len = Str_GetBare( szText, pszText, sizeof(szText), "|~,=[]{|}~" );
	}

	LPCTSTR pszReName = NULL;
	LPCTSTR pszPrefix = NULL;

	CLIMODE_TYPE PrvTargMode = GetTargMode();
	ClearTargMode();

	switch ( PrvTargMode )
	{
	case CLIMODE_PROMPT_GM_PAGE_TEXT:
		// m_Targ.m_sText
		Cmd_GM_Page( szText );
		return;
// WESTY MOD (MULTI CONFIRM)
	case CLIMODE_PROMPT_MULTI_CONFIRM:
	{
		CCharPtr pChar = GetChar();

		// Find the multi they placed for pre-viewing
		CItemPtr pItem = g_World.ItemFind( pChar->m_Act.m_Targ );
		CItemMultiPtr pItemMulti = NULL;
		if( pItem )
			pItemMulti = REF_CAST( CItemMulti, pItem );

		if( !pItemMulti )
		{
			// They answered the prompt after the memory expired.
			// Multi was destroyed and redeeded
			return;
		}
		if( !_stricmp( pszText, "YES" ) )
		{
			// Create the key and put it in their pack
			CItemPtr pKey;
			// Create the key to the door.
			ITEMID_TYPE id = pItemMulti->IsAttr(ATTR_MAGIC) ?
								ITEMID_KEY_MAGIC : ITEMID_KEY_COPPER ;
			pKey = CItem::CreateScript(id,GET_ATTACHED_CCHAR(this));
			ASSERT(pKey);
			pKey->SetType(IT_KEY);
			if ( g_Cfg.m_fAutoNewbieKeys )
				pKey->SetAttr(ATTR_NEWBIE);
			pKey->SetAttr( pItemMulti->GetAttr() & ATTR_MAGIC );
			pKey->m_itKey.m_lockUID = pKey->m_uidLink = pItemMulti->GetUID();

			pItemMulti->m_itShip.m_uidCreator = pChar->GetUID();
			CItemMemoryPtr pMemory = pChar->Memory_AddObjTypes( pItemMulti, MEMORY_GUARD );
			ASSERT(pMemory);
			pMemory->SetTimeout( 60 * TICKS_PER_SEC );

			if( pKey )
			{
				// Put in your pack
				pChar->GetPackSafe()->ContentAdd( pKey );

				// Put dupe key in the bank.
				pKey = CItem::CreateDupeItem( pKey );
				pChar->GetBank()->ContentAdd( pKey );
				pChar->WriteString( "The duplicate key is in your bank account" );
			}

			// We need to get rid of the memory object
			CItemMemoryPtr pMultiMemory = m_pChar->Memory_FindTypes( MEMORY_MULTIPREVIEW );
			if ( pMultiMemory )
			{
				pMultiMemory->DeleteThis();
				return;
			}
		}
		else if( !_stricmp( pszText, "NO" ) || len == 0 )
		{
			// Time out the memory, it'll handle this redeeding
			CItemMemoryPtr pMemory = m_pChar->Memory_FindTypes( MEMORY_MULTIPREVIEW );
			if( !pMemory )
			{
				// They answered after the memory expired.
				pChar->WriteString( "You didn't answer within 60 seconds. "
					"The preview was canceled and your deed is back in your pack." );
				return;
			}
			pMemory->SetTimeout( 1 );
			return;
		}
		else
		{
			// They didn't send back a correct response, send them back a new prompt
			WriteString( "Only a YES or NO please, case sensitive!" );
			addPromptConsole( CLIMODE_PROMPT_MULTI_CONFIRM,
				"To procede in placeing this building, type YES, to cancel type NO" );
		}
		return;
	}
// END WESTY MOD
	case CLIMODE_PROMPT_VENDOR_PRICE:
		// Setting the vendor price for an item.
		{
			if ( szText[0] == '\0' )	// cancel
				return;
			CCharPtr pCharVendor = g_World.CharFind(m_Targ.m_PrvUID);
			if ( pCharVendor )
			{
				pCharVendor->NPC_SetVendorPrice( g_World.ItemFind(m_Targ.m_UID), atoi(szText) );
			}
		}
		return;

	case CLIMODE_PROMPT_NAME_RUNE:
		pszReName = "Rune";
		pszPrefix = "";
		//		pszPrefix = "Rune to:";
		break;

	case CLIMODE_PROMPT_NAME_KEY:
		pszReName = "Key";
		pszPrefix = "Key to:";
		break;

	case CLIMODE_PROMPT_NAME_SHIP:
		pszReName = "Ship";
		pszPrefix = "SS ";
		break;

	case CLIMODE_PROMPT_NAME_SIGN:
		pszReName = "Sign";
		pszPrefix = "";
		break;

	case CLIMODE_PROMPT_STONE_NAME:
		pszReName = "Stone";
		pszPrefix = "Stone for the ";
		break;

	case CLIMODE_PROMPT_STONE_SET_ABBREV:
		pszReName = "Abbreviation";
		pszPrefix = "";
		break;

	case CLIMODE_PROMPT_STONE_GRANT_TITLE:
	case CLIMODE_PROMPT_STONE_SET_TITLE:
		pszReName = "Title";
		pszPrefix = "";
		break;

	case CLIMODE_PROMPT_TARG_VERB:
		// Send a msg to the pre-targetted player. "ETARGVERB"
		// m_Targ.m_UID = the target.
		// m_Targ.m_sText = the prefix.
		if ( szText[0] != '\0' )
		{
			TCHAR szCmd[MAX_TALK_BUFFER];
			s_CombineKeys( szCmd, (LPCTSTR) m_Targ.m_sText, szText );
			OnTarg_Obj_Command( g_World.ObjFind(m_Targ.m_UID), szCmd );
		}
		return;

	default:
		// DEBUG_ERR(( "%x:Unrequested Prompt mode %d" LOG_CR, m_Socket.GetSocket(), PrvTargMode ));
		WriteString( "Unexpected prompt info" );
		return;
	}

	ASSERT(pszReName);

	CGString sMsg;
	CItemPtr pItem = g_World.ItemFind(m_Targ.m_UID);

	if ( pItem == NULL || szText[0] == '\0' )
	{
		Printf( "%s Renaming Canceled", pszReName );
		return;
	}

	if ( g_Cfg.IsObscene( szText ))
	{
		Printf( "%s name %s is not allowed.", pszReName, szText );
		return;
	}

	sMsg.Format( "%s%s", pszPrefix, szText );
	switch (pItem->GetType())
	{
	case IT_STONE_GUILD:
	case IT_STONE_TOWN:
	case IT_STONE_ROOM:
		{
			CItemStonePtr pStone = REF_CAST(CItemStone,pItem);
			ASSERT(pStone);
			if ( ! pStone )
				return;
			if ( ! pStone->OnPromptResp( this, PrvTargMode, szText, sMsg ))
				return;
		}
		break;
	default:
		pItem->SetName( sMsg );
		sMsg.Format( "%s renamed: %s", pszReName, (LPCTSTR) pItem->GetName());
		break;
	}

	WriteString( sMsg );
}

void CClient::Event_Talk_Common( TCHAR* szText ) // PC speech
{
	// ??? Allow NPC's to talk to each other in the future.
	// Do hearing here so there is not feedback loop with NPC's talking to each other.
	if ( g_Cfg.IsConsoleCmd( szText[0] ))
	{
		Event_Command( szText+1 );
		return;
	}

	ASSERT( m_pChar );
	ASSERT( m_pChar->m_pPlayer );
	ASSERT( m_pChar->m_pArea );

	// Special GLOBAL things heard. SCRIPT THESE !!!

	if ( ! _strnicmp( szText, "home home home", 14 ) &&
		m_pChar->IsStatFlag( STATF_DEAD ) &&
		m_pChar->m_ptHome.IsCharValid())
	{
		if ( m_pChar->Spell_Effect_Teleport( m_pChar->m_ptHome, false, false ))
			return;
	}
	if ( ! _strnicmp( szText, "I resign from my guild", 22 ))
	{
		m_pChar->Guild_Resign(MEMORY_GUILD);
		return;
	}
	if ( ! _strnicmp( szText, "I resign from my town", 21 ))
	{
		m_pChar->Guild_Resign(MEMORY_TOWN);
		return;
	}

	static LPCTSTR const sm_szTextMurderer[] =
	{
		"Thy conscience is clear.",
		"Although thou hast committed murder in the past, the guards will still protect you.",
		"Your soul is black with guilt. Because thou hast murdered, the guards shall smite thee.",
		"Thou hast done great naughty! I wouldst not show my face in town if I were thee.",
	};

	if ( ! _strnicmp( szText, "I must consider my sins", 23 ))
	{
		int i = m_pChar->m_pPlayer->m_wMurders;
		int iMin = 0;
		int iMax = COUNTOF(sm_szTextMurderer)-1;
		if ( m_pChar->Noto_IsMurderer())
			iMin = 2;
		else 
			iMax = 1;
		if ( i > iMax )
			i = iMax;
		if ( i < iMin )
			i = iMin;
		WriteString( sm_szTextMurderer[i] );
		return;
	}

	// Guards are special
	// They can't hear u if your dead.
	bool fGhostSpeak = m_pChar->IsSpeakAsGhost();
	if ( ! fGhostSpeak && ( Str_FindWord( szText, "GUARD" ) || Str_FindWord( szText, "GUARDS" )))
	{
		m_pChar->CallGuards(NULL);
	}

	// Are we in a region that can hear ?
	if ( m_pChar->m_pArea->IsMultiRegion())
	{
		CItemPtr pItemMulti2 = g_World.ItemFind(m_pChar->m_pArea->GetUIDIndex());
		CItemMultiPtr pItemMulti = REF_CAST(CItemMulti,pItemMulti2);
		if ( pItemMulti )
		{
			pItemMulti->OnHearRegion( szText, m_pChar );
		}
	}

	// Are there items on the ground that might hear u ?
	CSectorPtr pSector = m_pChar->GetTopPoint().GetSector();
	if ( pSector->HasListenItems())
	{
		pSector->OnHearItem( m_pChar, szText );
	}

	// Find an NPC that may have heard us.
	CCharPtr pCharAlt = NULL;
	int iAltDist = SPHEREMAP_VIEW_SIGHT;
	CCharPtr pChar;
	int i=0;

	CWorldSearch AreaChars( m_pChar->GetTopPoint(), SPHEREMAP_VIEW_SIGHT );
	for(;;)
	{
		pChar = AreaChars.GetNextChar();
		if ( pChar == NULL )
			break;

		if ( pChar->IsStatFlag(STATF_COMM_CRYSTAL))
		{
			pChar->OnHearEquip( m_pChar, szText );
		}

		if ( pChar == m_pChar )
			continue;

		if ( fGhostSpeak && ! pChar->CanUnderstandGhost())
			continue;

		bool fNamed = false;
		i = 0;
		if ( ! _strnicmp( szText, "PETS", 4 ))
			i = 5;
		else if ( ! _strnicmp( szText, "ALL", 3 ))
			i = 4;
		else
		{
			// Named the char specifically ?
			i = pChar->NPC_OnHearName( szText );
			fNamed = true;
		}
		if ( i )
		{
			while ( ISWHITESPACE( szText[i] ))
				i++;

			if ( pChar->NPC_OnHearPetCmd( szText+i, m_pChar, !fNamed ))
			{
				if ( fNamed )
					return;
				if ( GetTargMode() == CLIMODE_TARG_PET_CMD )
					return;
				// The command might apply to other pets.
				continue;
			}
			if ( fNamed )
				break;
		}

		// Are we close to the char ?
		int iDist = m_pChar->GetTopDist3D( pChar );

		if ( pChar->Skill_GetActive() == NPCACT_TALK &&
			pChar->m_Act.m_Targ == m_pChar->GetUID()) // already talking to him
		{
			pCharAlt = pChar;
			iAltDist = 1;
		}
		else if ( pChar->IsClient() && iAltDist >= 2 )	// PC's have higher priority
		{
			pCharAlt = pChar;
			iAltDist = 2;	// high priority
		}
		else if ( iDist < iAltDist )	// closest NPC guy ?
		{
			pCharAlt = pChar;
			iAltDist = iDist;
		}

		// NPC's with special key words ?
		if ( pChar->m_pNPC.IsValidNewObj() )
		{
			if ( pChar->m_pNPC->m_Brain == NPCBRAIN_BANKER )
			{
				if ( Str_FindWord( szText, "BANK" ))
					break;
			}
		}
	}

	if ( pChar == NULL )
	{
		i = 0;
		pChar = pCharAlt;
		if ( pChar == NULL )
			return;	// no one heard it.
	}

	// Change to all upper case for ease of search. ???
	_strupr( szText );

	// The char hears you say this.
	pChar->NPC_OnHear( &szText[i], m_pChar );
}

void CClient::Event_Talk( LPCTSTR pszText, HUE_TYPE wHue, TALKMODE_TYPE mode ) // PC speech
{
	if ( GetAccount() == NULL )
		return;
	ASSERT( GetChar());

	// store the language of choice.
	GetAccount()->m_lang.Set( NULL );	// default.

	// Rip out the unprintables first.
	TCHAR szText[MAX_TALK_BUFFER];
	int len = Str_GetBare( szText, pszText, sizeof(szText)-1 );
	if ( len <= 0 )
		return;
	pszText = szText;

	if ( ! g_Cfg.IsConsoleCmd( pszText[0] ))
	{
		if ( wHue > HUE_DYE_HIGH )
		{
			g_Log.Event( LOG_GROUP_CHEAT, LOGL_WARN,
				"%x:Cheater '%s' is using bad text colors" LOG_CR,
				m_Socket.GetSocket(), (LPCTSTR) GetAccount()->GetName());
			wHue = HUE_TEXT_DEF;
		}
		if ( mode < 0 || mode > TALKMODE_CLIENT_MAX )
		{
			g_Log.Event( LOG_GROUP_CHEAT, LOGL_WARN,
				"%x:Cheater '%s' is using bad text mode" LOG_CR,
				m_Socket.GetSocket(), (LPCTSTR) GetAccount()->GetName());
			mode = TALKMODE_CLIENT_MAX;
		}
		if ( g_Log.IsLogged( LOG_GROUP_PLAYER_SPEAK, LOGL_TRACE ))
		{
			if ( m_pChar->IsStatFlag( STATF_Insubstantial | STATF_Invisible | STATF_Hidden ))
				g_Log.Event( LOG_GROUP_PLAYER_SPEAK, LOGL_TRACE, "%x:'%s' (not visible) Says '%s' mode=%d" LOG_CR, m_Socket.GetSocket(), (LPCTSTR) m_pChar->GetName(), szText, mode );
			else
				g_Log.Event( LOG_GROUP_PLAYER_SPEAK, LOGL_TRACE, "%x:'%s' Says '%s' mode=%d" LOG_CR, m_Socket.GetSocket(), (LPCTSTR) m_pChar->GetName(), szText, mode );
		}
		m_pChar->m_SpeechHue = wHue;
		m_pChar->Speak( szText, wHue, mode );	// echo this back.
	}
	else
	{
	}

	Event_Talk_Common( (TCHAR*) pszText );
}

void CClient::Event_TalkUNICODE( const CUOEvent* pEvent )
{
	// Get the text in wide bytes.
	// ENU = English
	// FRC = French
	// mode == TALKMODE_SYSTEM if coming from player talking.

	CAccountPtr pAccount = GetAccount();
	if ( pAccount == NULL )	// this should not happen
		return;

	// store the default language of choice. CLanguageID
	pAccount->m_lang.Set( pEvent->TalkUNICODE.m_lang );

	int iLen = pEvent->TalkUNICODE.m_len - sizeof(pEvent->TalkUNICODE);

	HUE_TYPE wHue = pEvent->TalkUNICODE.m_wHue;
	BYTE mode = pEvent->TalkUNICODE.m_mode;

	if ( mode >= TALKMODE_TOKENIZED )
	{
		// A really odd client "feature" in v2.0.7 .
		// This is not really UNICODE !!! odd tokenized normal text.
		// skip 3 or more bytes of junk,
		// 00 10 01 = I want the balance
		// 00 10 02 = bank
		// 00 11 71 = buy
		// 00 30 03 00 40 04 = check join member
		// 00 20 02 00 20 = buy bank sdfsdf
		// 00 40 01 00 20 07 00 20 = bank guards balance
		// 00 40 01 00 20 07 00 20 = sdf bank bbb guards ccc balance ddd
		// 00 40 01 00 20 07 00 20 = balance guards bank
		// 00 10 07 = guards sdf sdf
		// 00 30 36 04 f1 61 = stop (this pattern makes no sense)
		// 00 20 07 15 c0 = .add c_h_guard
		// and skill

		mode -= TALKMODE_TOKENIZED;
		LPCTSTR pszText = (LPCTSTR)(pEvent->TalkUNICODE.m_utext);

		int i;
		for ( i=0; i<iLen; i++ )
		{
			TCHAR ch = pszText[i];
			if ( ch >= 0x20 )
				break;
			i++;
			ch = pszText[i];
			if ( ((BYTE) ch ) > 0xc0 )
			{
				i++;
				continue;
			}
			ch = pszText[i+1];
			if ( i <= 2 || ch < 0x20 || ch >= 0x80 )
				i++;
		}

		Event_Talk( pszText+i, wHue, (TALKMODE_TYPE) mode );
		return;
	}

	TCHAR szText[MAX_TALK_BUFFER];
	int iLenChars = iLen/sizeof(WCHAR);
	iLen = CvtNUNICODEToSystem( szText, sizeof(szText), pEvent->TalkUNICODE.m_utext, iLenChars );
	if ( iLen <= 0 )
		return;

#if 0
	// No double chars ? what about stuff over 0x80 ? Euro characters ?
	if ( iLenChars == iLen )
	{
		// If there is no real unicode in the speech then optimize by processing it as normal ?
		// It's just english anyhow ?
		Event_Talk( szText, wHue,
			(TALKMODE_TYPE)( pEvent->TalkUNICODE.m_mode ));
		return;
	}
#endif

	// Non-english.
	if ( ! g_Cfg.IsConsoleCmd( szText[0] ))
	{
		if ( wHue > HUE_DYE_HIGH )
		{
			g_Log.Event( LOG_GROUP_CHEAT, LOGL_WARN,
				"%x:Cheater '%s' is using bad text colors" LOG_CR,
				m_Socket.GetSocket(), (LPCTSTR) GetAccount()->GetName());
			wHue = HUE_TEXT_DEF;
		}
		if ( mode < 0 || mode > TALKMODE_CLIENT_MAX )
		{
			g_Log.Event( LOG_GROUP_CHEAT, LOGL_WARN,
				"%x:Cheater '%s' is using bad text mode" LOG_CR,
				m_Socket.GetSocket(), (LPCTSTR) GetAccount()->GetName());
			mode = TALKMODE_CLIENT_MAX;
		}
		if ( g_Log.IsLogged( LOG_GROUP_PLAYER_SPEAK, LOGL_TRACE ))
		{
			if( m_pChar->IsStatFlag( STATF_Insubstantial | STATF_Invisible | STATF_Hidden ) )
				g_Log.Event( LOG_GROUP_PLAYER_SPEAK, LOGL_TRACE, "%x:'%s' (not visible) Says UNICODE '%s' '%s' mode=%d" LOG_CR, m_Socket.GetSocket(), (LPCTSTR) m_pChar->GetName(), pAccount->m_lang.GetStr(), szText, pEvent->Talk.m_mode );
			else
				g_Log.Event( LOG_GROUP_PLAYER_SPEAK, LOGL_TRACE, "%x:'%s' Says UNICODE '%s' '%s' mode=%d" LOG_CR, m_Socket.GetSocket(), (LPCTSTR) m_pChar->GetName(), pAccount->m_lang.GetStr(), szText, pEvent->Talk.m_mode );
		}
		m_pChar->m_SpeechHue = wHue;
		m_pChar->SpeakUTF8( szText,
			wHue,
			(TALKMODE_TYPE) pEvent->TalkUNICODE.m_mode,
			m_pChar->m_fonttype,
			pAccount->m_lang );
	}

	Event_Talk_Common( szText );
}

bool CClient::Event_DeathOption( DEATH_MODE_TYPE mode, const CUOEvent* pEvent )
{
	if ( m_pChar == NULL )
		return false;
	if ( mode != DEATH_MODE_MANIFEST )
	{
		// Death menu option.
		if ( mode == DEATH_MODE_RES_IMMEDIATE ) // res w/penalties ?
		{
			// Insta res should go back to the spot of death !
			static LPCTSTR const sm_Res_Fail_Msg[] =
			{
				"The connection between your spirit and the world is too weak.",
				"Thou hast drained thyself in thy efforts to reassert thine lifeforce in the physical world.",
				"No matter how strong your efforts, you cannot reestablish contact with the physical world.",
			};
			if ( GetTargMode() != CLIMODE_DEATH )
			{
				WriteString( sm_Res_Fail_Msg[ Calc_GetRandVal( COUNTOF( sm_Res_Fail_Msg )) ] );
			}
			else
			{
				SetTargMode();
				m_pChar->MoveToChar( m_Targ.m_pt ); // Insta res takes us back to where we died.
				if ( ! m_pChar->Spell_Effect_Resurrection( 10, NULL ))
				{
					WriteString( sm_Res_Fail_Msg[ Calc_GetRandVal( COUNTOF( sm_Res_Fail_Msg )) ] );
				}
			}
		}
		else // DEATH_MODE_PLAY_GHOST
		{
			// Play as a ghost.
			// "As the mortal pains fade, you become aware of yourself as a spirit."
			WriteString( "You are a ghost" );
			addSound( 0x17f );	// Creepy noise.
		}

		addPlayerStart( m_pChar ); // Do practically a full reset (to clear death menu)
		return( true );
	}

	// Toggle manifest mode. (this has more data) (anomoly to size rules)
	if ( ! xCheckMsgSize( sizeof( pEvent->DeathMenu )))
		return(false);
	Event_CombatMode( pEvent->DeathMenu.m_manifest );
	return( true );
}

void CClient::Event_SetName( CSphereUID uid, const char* pszCharName )
{
	// Set the name in the character status window.
	ASSERT( m_pChar );
	CCharPtr pChar = g_World.CharFind(uid);
	if ( pChar == NULL )
		return;

	// Do we have the right to do this ?
	if ( ! pChar->NPC_IsOwnedBy( m_pChar, true ))
		return;

	pChar->SetName( pszCharName );
}

void CClient::Event_ScrollClose( DWORD dwContext )
{
	// A certain scroll has closed.
	DEBUG_MSG(( "%x:XCMD_Scroll(close) 0%x" LOG_CR, m_Socket.GetSocket(), dwContext ));

	// Make a trigger out of this.
}

void CClient::Event_MenuChoice( CSphereUID uidItem, DWORD context, WORD select ) // Choice from GMMenu or Itemmenu received
{
	// Select from a menu. CMenuItem
	// result of addMenu call previous.
	// select = 0 = cancel.

	if ( context != GetTargMode() || uidItem != m_Targ.m_tmMenu.m_UID )
	{
		// DEBUG_ERR(( "%x: Menu choice unrequested %d!=%d" LOG_CR, m_Socket.GetSocket(), context, m_Targ_Mode ));
		WriteString( "Unexpected menu info" );
		return;
	}

	ClearTargMode();

	// Item Script or GM menu script got us here.
	switch ( context )
	{
	case CLIMODE_MENU:
		// A generic menu from script.
		Menu_OnSelect( m_Targ.m_tmMenu.m_ResourceID, select, g_World.ObjFind(uidItem) );
		return;
	case CLIMODE_MENU_SKILL:
		// Some skill menu got us here.
		if ( select >= COUNTOF(m_Targ.m_tmMenu.m_Item))
			return;
		Cmd_Skill_Menu( m_Targ.m_tmMenu.m_ResourceID, (select) ? m_Targ.m_tmMenu.m_Item[select] : 0 );
		return;
	case CLIMODE_MENU_SKILL_TRACK_SETUP:
		// PreTracking menu got us here.
		Cmd_Skill_Tracking( select, false );
		return;
	case CLIMODE_MENU_SKILL_TRACK:
		// Tracking menu got us here. Start tracking the selected creature.
		Cmd_Skill_Tracking( select, true );
		return;

	case CLIMODE_MENU_GM_PAGES:
		// Select a GM page from the menu.
		Cmd_GM_PageSelect( select );
		return;
	case CLIMODE_MENU_EDIT:
		// m_Targ.m_sText = what are we doing to it ?
		Cmd_EditItem( g_World.ObjFind(uidItem), select );
		return;
	default:
		DEBUG_ERR(( "%x:Unknown Targetting mode for menu %d" LOG_CR, m_Socket.GetSocket(), context ));
		return;
	}
}

void CClient::Event_GumpInpValRet( const CUOEvent* pEvent )
{
	// Text was typed into the gump on the screen.
	// pEvent->GumpInpValRet
	// result of addGumpInputBox. GumpInputBox
	// ARGS:
	// 	m_Targ.m_UID = pObj->GetUID();
	//  m_Targ.m_sText = verb

	CSphereUID uidItem( pEvent->GumpInpValRet.m_UID );
	WORD context = pEvent->GumpInpValRet.m_context;	// word context is odd.
	BYTE retcode = pEvent->GumpInpValRet.m_retcode; // 0=canceled, 1=okayed
	WORD textlen = pEvent->GumpInpValRet.m_textlen; // length of text entered
	LPCTSTR pszText = pEvent->GumpInpValRet.m_text;

	if ( GetTargMode() != CLIMODE_INPVAL || uidItem != m_Targ.m_UID )
	{
		// DEBUG_ERR(( "%x:Event_GumpInpValRetIn unexpected input %d!=%d" LOG_CR, m_Socket.GetSocket(), context, GetTargMode()));
		WriteString( "Unexpected text input" );
		return;
	}

	ClearTargMode();

	CObjBasePtr pObj = g_World.ObjFind(uidItem);
	if ( pObj == NULL )
		return;

	// take action based on the parent context.
	if (retcode == 1)	// ok
	{
		// Properties Dialog, page x
		// m_Targ.m_sText = the verb we are dealing with.
		// m_Prop_UID = object we are after.

		TCHAR szCmd[MAX_TALK_BUFFER];
		sprintf( szCmd, "%s=%s", (LPCSTR) m_Targ.m_sText, pszText );
		OnTarg_Obj_Command( pObj, szCmd );
	}

	Dialog_Setup( CLIMODE_DIALOG, m_Targ.m_tmInpVal.m_PrvGumpID, pObj ); // put back the client.
}

//************************************************************************

void CClient::Event_GumpDialogRet( const CUOEvent* pEvent )
{
	// CLIMODE_DIALOG
	// initiated by addGumpDialog()
	// A button was pressed in a gump on the screen.
	// possibly multiple check boxes.
	ASSERT(m_pChar);

	// First let's completely decode this packet
	CSphereUID uid( pEvent->GumpDialogRet.m_UID );
	DWORD context = pEvent->GumpDialogRet.m_context;
	DWORD dwButtonID = pEvent->GumpDialogRet.m_buttonID;

	// DEBUG_MSG(("uid: %x, context: %i, gump: %i" LOG_CR, (DWORD) uid, context, dwButtonID ));

	CObjBasePtr pObj = g_World.ObjFind(uid);
	if ( pObj == NULL )
	{
		return;
	}

	if ( context != GetTargMode() || uid != m_Targ.m_tmGumpDialog.m_UID )
	{
		// DEBUG_ERR(( "%x: Event_GumpDialogRet unexpected input %d!=%d" LOG_CR, m_Socket.GetSocket(), context, GetTargMode()));
		if ( pObj == m_pChar )
		{
			if ( dwButtonID == 1 )
			{
				// Buttons on the paper doll always valid.
				return;
			}
		}
		Printf( "Unexpected button input %d", dwButtonID );
		return;
	}

	// package up the gump response info.
	CSphereExpArgs exec(pObj,this);

	DWORD iCheckQty = pEvent->GumpDialogRet.m_checkQty; // this has the total of all checked boxes and radios
	int i = 0;
	for ( ; i < iCheckQty; i++ ) // Store the returned checked boxes' ids for possible later use
	{
		exec.AddCheck( i, pEvent->GumpDialogRet.m_checkIds[i] );
	}

	// Find out how many textentry boxes we have that returned data
	CUOEvent* pMsg = (CUOEvent *)(((BYTE*)(pEvent))+(iCheckQty-1)*sizeof(pEvent->GumpDialogRet.m_checkIds[0]));
	DWORD iTextQty = pMsg->GumpDialogRet.m_textQty;
	for ( i = 0; i < iTextQty; i++)
	{
		// Get the length....no need to store this permanently
		int lenstr = pMsg->GumpDialogRet.m_texts[0].m_len;

		TCHAR szTmp2[CSTRING_MAX_LEN]; // use this as szTmp2 storage

		// Do a loop and "convert" from unicode to normal ascii
		CvtNUNICODEToSystem( szTmp2, sizeof(szTmp2), pMsg->GumpDialogRet.m_texts[0].m_utext, lenstr );

		exec.AddText( pMsg->GumpDialogRet.m_texts[0].m_id, szTmp2 );

		lenstr = sizeof(pMsg->GumpDialogRet.m_texts[0]) + ( lenstr - 1 )* sizeof(NCHAR);
		pMsg = (CUOEvent *)(((BYTE*)pMsg)+lenstr);
	}

	ClearTargMode();

	// NOTE: Get rid of hard coded dialogs !!!???

	switch ( context ) // This is the page number
	{

	case CLIMODE_DIALOG_ADMIN: // Admin console stuff comes here
		{
			switch ( dwButtonID ) // Button presses come here
			{
			case 801: // Previous page
				addGumpDialogAdmin( m_Targ.m_tmGumpAdmin.m_iPageNum - 1, m_Targ.m_tmGumpAdmin.m_iSortType );
				break;
			case 802: // Next page
				addGumpDialogAdmin( m_Targ.m_tmGumpAdmin.m_iPageNum + 1, m_Targ.m_tmGumpAdmin.m_iSortType );
				break;

			case 901: // Open up the options page for this client
			case 902:
			case 903:
			case 904:
			case 905:
			case 906:
			case 907:
			case 908:
			case 909:
			case 910:
				{
				dwButtonID -= 901;
				if ( dwButtonID >= COUNTOF( m_Targ.m_tmGumpAdmin.m_Item ))
					return;
				DEBUG_CHECK( dwButtonID < ADMIN_CLIENTS_PER_PAGE );
				m_Targ.m_UID = m_Targ.m_tmGumpAdmin.m_Item[dwButtonID];
				if ( m_Targ.m_UID.IsValidObjUID())
				{
					CSphereExpContext exec( pObj, this );
					exec.ExecuteCommand( "f_AdminMenu" );
				}
				else
					addGumpDialogAdmin( m_Targ.m_tmGumpAdmin.m_iPageNum, m_Targ.m_tmGumpAdmin.m_iSortType );
				}
				break;
			}
		}
		return;

	case CLIMODE_DIALOG_GUILD: // Guild/Leige/Townstones stuff comes here
		{
			CItemPtr pStone2 = g_World.ItemFind(m_Targ.m_UID);
			CItemStonePtr pStone = REF_CAST(CItemStone,pStone2);
			if ( pStone == NULL )
				return;
			if ( pStone->OnDialogButton( this, (STONEDISP_TYPE) dwButtonID, exec ))
				return;
		}
		break;

	case CLIMODE_DIALOG_HAIR_DYE: // Using hair dye
		{
			if (dwButtonID == 0) // They cancelled
				return;
			CVarDefPtr pVar = exec.m_ArgArray.FindKeyPtr("ARGCHK0");
			if ( pVar == NULL )
				return;	// They didn't pick a wHue, but they hit OK
			CItemPtr pItem = g_World.ItemFind(m_Targ.m_UID);
			if ( pItem == NULL )
				return;
			CItemPtr pFacialHair = m_pChar->LayerFind( LAYER_BEARD );
			CItemPtr pHeadHair = m_pChar->LayerFind( LAYER_HAIR );
			if (!pFacialHair && !pHeadHair)
			{
				WriteString("You have no hair to dye!");
				return;
			}
			if (pFacialHair)
				pFacialHair->SetHue( pVar->GetDWORD() + 1 );
			if (pHeadHair)
				pHeadHair->SetHue( pVar->GetDWORD() + 1 );
			m_pChar->Update();
			pItem->ConsumeAmount();// Consume it
		}
		return;
	}

	//
	// Call the scripted response. Lose all the checks and text.
	//
	Dialog_OnButton( m_Targ.m_tmGumpDialog.m_ResourceID, dwButtonID, exec );
}

bool CClient::Event_DoubleClick( CSphereUID uid, bool fMacro, bool fTestTouch )
{
	// Try to use the object in some way. (DClick)
	// will trigger a OnTarg_Use_Item() ussually.
	// fMacro = ALTP vs dbl click. no unmount.

	// Allow some static in game objects to have function?
	// Not possible with dclick.

	ASSERT(m_pChar);
	if ( g_Log.IsLogged( LOGL_TRACE ))
	{
		DEBUG_MSG(( "%x:Event_DoubleClick 0%x" LOG_CR, m_Socket.GetSocket(), (DWORD) uid ));
	}

	CObjBasePtr pObj = g_World.ObjFind(uid);
	if ( ! m_pChar->CanSee( pObj ))
	{
		addObjectRemoveCantSee( uid, "the target" );
		return false;
	}

	// Face the object we are using/activating.
	SetTargMode();
	m_Targ.m_UID = uid;
	m_pChar->UpdateDir( pObj );

	if ( pObj->IsItem())
	{
		return Cmd_Use_Item( REF_CAST(CItem,pObj), fTestTouch );
	}

	// Must be a char
	CCharPtr pChar = REF_CAST(CChar,pObj);
	ASSERT(pChar);

	CSphereExpContext exec(pChar, m_pChar);
	if ( pChar->OnTrigger( CCharDef::T_UserDClick, exec)
		== TRIGRET_RET_VAL )
	{
		return true;
	}

	if ( ! fMacro )
	{
		if ( pChar == m_pChar )
		{
			if ( m_pChar->Horse_UnMount())
				return true;
		}
		if ( pChar->m_pNPC &&
			pChar->GetCreatureType() != NPCBRAIN_HUMAN )
		{
			if ( m_pChar->Horse_Mount( pChar ))
				return true;
			// can't mount i guess.
			switch ( pChar->GetID())
			{
			case CREID_HORSE_PACK:
			case CREID_LLAMA_PACK:
				// pack animals open container.
				return Cmd_Use_Item( pChar->GetPackSafe(), fTestTouch );
			default:
				if ( IsPrivFlag(PRIV_GM))
				{
					// snoop the creature.
					return Cmd_Use_Item( pChar->GetPackSafe(), false );
				}
				return false;
			}
		}
	}

#if 0
	CSphereExpContext se2(pChar, m_pChar);
	if ( pChar->OnTrigger( CCharDef::T_UserDClick, se2)
		== TRIGRET_RET_VAL )
	{
		return true;
	}
#endif

	// open paper doll.

	CUOCommand cmd;
	cmd.PaperDoll.m_Cmd = XCMD_PaperDoll;
	cmd.PaperDoll.m_UID = pChar->GetUID();

	int len;

	bool fUseTradeTitle = ( ! pChar->IsStatFlag( STATF_Incognito ) && g_Cfg.m_fCharTitles );
	if ( fUseTradeTitle )
	{
		len = sprintf( cmd.PaperDoll.m_text, (LPCTSTR) pChar->Noto_GetTitle());
	}
	else
	{
		len = sprintf( cmd.PaperDoll.m_text, (LPCTSTR) pChar->GetName());
	}

	if ( ! pChar->IsStatFlag( STATF_Incognito ))
	{
		// AbbrevAndTitle
		const TCHAR* pszGuild = pChar->Guild_AbbrevAndTitle(MEMORY_GUILD);
		if ( pszGuild)
		{
			len += sprintf( cmd.PaperDoll.m_text+len, ", %s", pszGuild );
		}
		if ( fUseTradeTitle )
		{
			len += sprintf( cmd.PaperDoll.m_text+len, ", %s", (LPCTSTR) pChar->GetTradeTitle());
		}
	}

	cmd.PaperDoll.m_text[ sizeof(cmd.PaperDoll.m_text)-1 ] = '\0';
	cmd.PaperDoll.m_mode = pChar->GetModeFlag();	// 0=normal, 0x40 = attack
	xSendPkt( &cmd, sizeof( cmd.PaperDoll ));
	return( true );
}

void CClient::Event_SingleClick( CSphereUID uid )
{
	// the ALLNAMES macro comes thru here.
	ASSERT(m_pChar);
	if ( g_Log.IsLogged( LOGL_TRACE ))
	{
		DEBUG_MSG(( "%x:Event_SingleClick 0%lx" LOG_CR, m_Socket.GetSocket(), (DWORD) uid ));
	}

	CObjBasePtr pObj = g_World.ObjFind(uid);
	if ( ! m_pChar->CanSee( pObj ))
	{
		// ALLNAMES makes this happen as we are running thru an area.
		// So display no msg. Do not use (addObjectRemoveCantSee)
		addObjectRemove( uid );
		return;
	}

	ASSERT(pObj);
	if ( m_pChar->Skill_GetActive() == NPCACT_OneClickCmd )
	{
		OnTarg_Obj_Command( pObj, m_Targ.m_sText );
		return;
	}

	CSphereExpContext exec(pObj, m_pChar);
	if ( pObj->OnTrigger( "@UserClick", exec) == TRIGRET_RET_VAL )	// CCharDef::T_Click, CItemDef::T_Click
		return;

	if ( pObj->IsItem())
	{
		addItemName( REF_CAST(CItem,pObj));
		return;
	}

	if ( pObj->IsChar())
	{
		addCharName( REF_CAST(CChar,pObj) );
		return;
	}

	Printf( "Bogus item uid=0%x?", uid );
}

void CClient::Event_ClientVersion( const char* pData, int iLen )
{
	// XCMD_ClientVersion

	m_ProtoVer.SetCryptVer( pData );
	if ( ! CheckProtoVersion())
	{
		// Protocol version is not allowed !
		m_Socket.Close();
		return;
	}

	m_fClientVer3d = ( iLen > 8 );
	m_iClientResourceLevel = 2;	// t2A resource level. (maps and animations)
	if ( m_fClientVer3d )
	{
		m_iClientResourceLevel = 3;	// at least this.
	}

	DEBUG_MSG(( "%x:XCMD_ClientVersion '%s',%d" LOG_CR, m_Socket.GetSocket(), pData, m_fClientVer3d ));
}

void CClient::Event_Spy( const CUOEvent* pEvent )
{
	// XCMD_Spy: // Spy tells us stuff about the clients computer.
	DEBUG_MSG(( "XCMD_Spy" LOG_CR ));
}

void CClient::Event_Target( DWORD context, CSphereUID uid, CPointMap pt, ITEMID_TYPE id )
{
	// XCMD_Target
	// If player clicks on something with the targetting cursor
	// Assume addTarget was called before this.
	// NOTE: Make sure they can actually validly trarget this item !

	ASSERT(m_pChar);
	if ( context != GetTargMode())
	{
		// DEBUG_ERR(( "%x: Unrequested target info ?" LOG_CR, m_Socket.GetSocket()));
		WriteString( "Unexpected target info" );
		return;
	}
	if ( ! pt.IsValidXY() && ! uid.IsValidObjUID())
	{
		// canceled
		SetTargMode();
		return;
	}

	pt.m_mapplane = m_pChar->GetTopMap();

	CLIMODE_TYPE prevmode = GetTargMode();
	ClearTargMode();

	CObjBasePtr pObj = g_World.ObjFind(uid);
	if ( IsPrivFlag( PRIV_GM ))
	{
		if ( uid.IsValidObjUID() && pObj == NULL )
		{
			addObjectRemoveCantSee( uid, "the target" );
			return;
		}
	}
	else
	{
		if ( uid.IsValidObjUID())
		{
			if ( ! m_pChar->CanSee(pObj))
			{
				addObjectRemoveCantSee( uid, "the target" );
				return;
			}
		}
		else
		{
			// The point must be valid.
			if ( m_pChar->GetTopDist(pt) > SPHEREMAP_VIEW_SIZE )
			{
				return;
			}
		}
	}

	if ( pObj )
	{
		// Point inside a container is not really meaningful here.
		pt = pObj->GetTopLevelObj()->GetTopPoint();
	}

	bool fSuccess = false;

	switch ( prevmode )
	{
		// GM stuff.

	case CLIMODE_TARG_OBJ_SET:		fSuccess = OnTarg_Obj_Command( pObj, m_Targ.m_sText ); break;
	case CLIMODE_TARG_OBJ_INFO:		fSuccess = OnTarg_Obj_Info( pObj, pt, id );  break;

	case CLIMODE_TARG_UNEXTRACT:	fSuccess = OnTarg_UnExtract( pObj, pt ); break;
	case CLIMODE_TARG_ADDITEM:		fSuccess = OnTarg_Item_Add( pObj, pt ); break;
	case CLIMODE_TARG_LINK:			fSuccess = OnTarg_Item_Link( pObj ); break;
	case CLIMODE_TARG_TILE:			fSuccess = OnTarg_Tile( pObj, pt );  break;

		// Player stuff.

	case CLIMODE_TARG_SKILL:			fSuccess = OnTarg_Skill( pObj ); break;
	case CLIMODE_TARG_SKILL_MAGERY:     fSuccess = OnTarg_Skill_Magery( pObj, pt ); break;
	case CLIMODE_TARG_SKILL_HERD_DEST:  fSuccess = OnTarg_Skill_Herd_Dest( pObj, pt ); break;
	case CLIMODE_TARG_SKILL_POISON:		fSuccess = OnTarg_Skill_Poison( pObj ); break;
	case CLIMODE_TARG_SKILL_PROVOKE:	fSuccess = OnTarg_Skill_Provoke( pObj ); break;

	case CLIMODE_TARG_REPAIR:			fSuccess = m_pChar->Use_Repair( g_World.ItemFind(uid)); break;
	case CLIMODE_TARG_PET_CMD:			fSuccess = OnTarg_Pet_Command( pObj, pt ); break;
	case CLIMODE_TARG_PET_STABLE:		fSuccess = OnTarg_Pet_Stable( g_World.CharFind(uid)); break;

	case CLIMODE_TARG_USE_ITEM:			fSuccess = OnTarg_Use_Item( pObj, pt, id );  break;
	case CLIMODE_TARG_STONE_RECRUIT:	fSuccess = OnTarg_Stone_Recruit( g_World.CharFind(uid) );  break;
	case CLIMODE_TARG_PARTY_ADD:		fSuccess = OnTarg_Party_Add( g_World.CharFind(uid) );  break;
	}
}

//----------------------------------------------------------------------

bool CClient::xCheckMsgSize( int iLenExpect )
{
	// Is there enough data from client to process this packet ?
	// ARGS: 
	//  len = the expected amount of data.
	// RETURN:
	//  true = the data matches.

	// Sometimes the desired size is not the same as the expected size ?
	// negative = minimum size of a variable size.

	ASSERT(iLenExpect);

	int iLenAvail = m_bin.GetDataQty();
	if ( iLenAvail < 1 )
		return false;

	if ( iLenExpect < 0 || iLenExpect >= 0x8000 ) // var length
	{
		if ( iLenAvail < 3 )
			return(false);
		const CUOEvent* pEvent = (const CUOEvent *) m_bin.RemoveDataLock();
		iLenExpect = pEvent->Talk.m_len;
		if ( iLenExpect > sizeof( CUOEvent ))
			return false;
	}
	else
	{
		ASSERT(iLenExpect <= sizeof( CUOEvent ));
	}

	if ( iLenAvail < iLenExpect )
		return(false);

	m_bin_msg_len = iLenExpect;
	return( true );
}

bool CClient::xDispatchMsg()
{
	// Process a single message we have Received from client.
	// No reason to ever process more than 1 at a time.
	// Do speed throttling here ?
	// RETURN:
	//  false = the message was not formatted correctly (dump the client)
	//   or is not yet complete !

	m_bin_msg_len = 0xFFFF; // all the data.
	if ( m_bin.GetDataQty() < 1 )	// just get the command
	{
		return( false );
	}

	if ( ! m_Crypt.IsInitCrypt())
	{
		SPHERE_LOG_ERR("xDispatchMsg: crypt not initialized, dropping data");
		m_bin.RemoveDataAmount( m_bin.GetDataQty());
		return false;
	}

	const CUOEvent* pEvent = (const CUOEvent *) m_bin.RemoveDataLock();

	SPHERE_LOG_NET("xDispatchMsg: cmd=0x%02x binQty=%d connType=%d", pEvent->Default.m_Cmd, m_bin.GetDataQty(), m_ConnectType);

	// check the packet size first.
	if ( pEvent->Default.m_Cmd >= XCMD_QTY ) // bad packet type ?
	{
		SPHERE_LOG_ERR("xDispatchMsg: BAD cmd=0x%02x >= XCMD_QTY=%d", pEvent->Default.m_Cmd, XCMD_QTY);
		return( false );
	}
	// Throttle non-critical packets to prevent client flooding.
	// Skip throttle for: walk, skill, and all login-phase packets.
	if ( pEvent->Default.m_Cmd != XCMD_Walk &&
		pEvent->Default.m_Cmd != XCMD_Skill &&
		pEvent->Default.m_Cmd != XCMD_ServerSelect &&
		pEvent->Default.m_Cmd != XCMD_Spy &&
		pEvent->Default.m_Cmd != XCMD_CharListReq &&
		pEvent->Default.m_Cmd != XCMD_CharPlay &&
		pEvent->Default.m_Cmd != XCMD_ServersReq &&
		m_ConnectType == CONNECT_GAME &&
		GetAccount() != NULL )
	{
		int iAge = m_timeLastDispatch.GetCacheAge();
		if ( iAge < TICKS_PER_SEC/2 )
		{
			m_bin_msg_len = 0;	// wait longer. then process this.
			return true;
		}
		m_timeLastDispatch.InitTimeCurrent();
	}

	if ( pEvent->Default.m_Cmd == XCMD_Ping )
	{
		// Ping from client. 0 = request?
		// Client seems not to require a response !
		if ( ! xCheckMsgSize( sizeof( pEvent->Ping )))
			return(false);
		return( true );
	}

	// NOTE: What about client version differences ! 
	// none so far since 2.0
	if ( m_ProtoVer.GetCryptVer() >= 0x126000 )
	{
		if ( ! xCheckMsgSize( g_Packet_Lengths[pEvent->Default.m_Cmd] ))
			return(false);
	}

	if ( m_ConnectType != CONNECT_GAME || ! GetAccount() )
	{
		// login server or a game server that has not yet logged in.

		switch ( pEvent->Default.m_Cmd )
		{
		case XCMD_ServersReq: // First Login
			if ( ! xCheckMsgSize( sizeof( pEvent->ServersReq )))
				return(false);
			return( Login_ServerList( pEvent->ServersReq.m_acctname, pEvent->ServersReq.m_acctpass ) == LOGIN_SUCCESS );
		case XCMD_ServerSelect:// Server Select - relay me to the server.
			if ( ! xCheckMsgSize( sizeof( pEvent->ServerSelect )))
				return(false);
			return( Login_Relay( pEvent->ServerSelect.m_select ));
		case XCMD_CharListReq: // Second Login to select char
			if ( ! xCheckMsgSize( sizeof( pEvent->CharListReq )))
				return(false);
			return( Setup_CharListReq( pEvent->CharListReq.m_acctname, pEvent->CharListReq.m_acctpass, pEvent->CharListReq.m_Account ) == LOGIN_SUCCESS );
		case XCMD_Spy: // Spy tells us stuff about the clients computer.
			if ( ! xCheckMsgSize( sizeof( pEvent->Spy )))
				return(false);
			Event_Spy( pEvent );
			return( true );
		case XCMD_War: // Tab = Combat Mode (toss this)
			if ( ! xCheckMsgSize( sizeof( pEvent->War )))
				return(false);
			return( true );
		}

		// No other messages are valid at this point.
		return( false );
	}

	/////////////////////////////////////////////////////
	// We should be encrypted below here. CONNECT_GAME

	// Get messages from the client.
	switch ( pEvent->Default.m_Cmd )
	{
	case XCMD_Create: // Character Create
		if ( m_bin_PrvMsg == XCMD_Walk && m_Crypt.GetCryptVer() == 0 )
		{
			// just eat this. (ignition artifact)
			if ( ! xCheckMsgSize( 4 ))
				return(false);
			return( true );
		}
		if ( m_ProtoVer.GetCryptVer() >= 0x126000 )
		{
			if ( ! xCheckMsgSize( sizeof( pEvent->Create )))
				return(false);
		}
		else
		{
			if ( ! xCheckMsgSize( sizeof( pEvent->Create_v25 )))
				return(false);
		}
		Setup_CreateDialog( pEvent );
		return( true );
	case XCMD_CharDelete: // Character Delete
		if ( ! xCheckMsgSize( sizeof( pEvent->CharDelete )))
			return(false);
		addDeleteErr( Setup_Delete( pEvent->CharDelete.m_slot ));
		return( true );
	case XCMD_CharPlay: // Character Select
		if ( ! xCheckMsgSize( sizeof( pEvent->CharPlay )))
			return(false);
		if ( ! Setup_Play( pEvent->CharPlay.m_slot ))
		{
			addLoginErr( LOGIN_ERR_NONE );
		}
		return( true );
	case XCMD_TipReq: // Get Tip
		if ( ! xCheckMsgSize( sizeof( pEvent->TipReq )))
			return(false);
		Event_Tips( pEvent->TipReq.m_index + 1 );
		return( true );
	}

	// must have a logged in char to use any other messages.
	if ( m_pChar == NULL )
	{
		return( false );
	}

	//////////////////////////////////////////////////////
	// We are now playing.

	switch ( pEvent->Default.m_Cmd )
	{
	case XCMD_Walk: // Walk
		// Modern clients (ClassicUO, etc.) always send v26 (7 bytes) regardless
		// of encryption. Use v26 if we have enough data, v25 as fallback.
		if ( m_bin.GetDataQty() >= (int)sizeof( pEvent->Walk_v26 ) )
		{
			if ( ! xCheckMsgSize( sizeof( pEvent->Walk_v26 )))
				return(false);
			Event_Walking( (DIR_TYPE)( pEvent->Walk_v26.m_dir & 0x0F ),
				(bool)( pEvent->Walk_v26.m_dir & 0x80 ),
				pEvent->Walk_v26.m_count, pEvent->Walk_v26.m_cryptcode );
		}
		else
		{
			if ( ! xCheckMsgSize( sizeof( pEvent->Walk_v25 )))
				return(false);
			Event_Walking( (DIR_TYPE)( pEvent->Walk_v25.m_dir & 0x0F ),
				(bool)( pEvent->Walk_v25.m_dir & 0x80 ),
				pEvent->Walk_v25.m_count, 0 );
		}
		break;
	case XCMD_Talk: // Speech or at least text was typed.
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_Talk( pEvent->Talk.m_text, pEvent->Talk.m_wHue, (TALKMODE_TYPE)( pEvent->Talk.m_mode ));
		break;
	case XCMD_GodMode:	// Turn on/off GM flag.
		if ( ! xCheckMsgSize(sizeof(pEvent->GodMode)))
			return(false);
		if ( GetPrivLevel() >= PLEVEL_GM )
		{
			if ( pEvent->GodMode.m_fToggle )
				SetPrivFlags( PRIV_GM );
			else
				ClearPrivFlags( PRIV_GM );
		}
		else
		{
			// Cheater
		}
		break;
	case XCMD_Attack: // Attack
		if ( ! xCheckMsgSize( sizeof( pEvent->Click )))
			return(false);
		Event_Attack( (DWORD) pEvent->Click.m_UID );
		break;
	case XCMD_DClick:// Doubleclick
		if ( ! xCheckMsgSize( sizeof( pEvent->Click )))
			return(false);
		Event_DoubleClick( ((DWORD)(pEvent->Click.m_UID)) &~ RID_F_RESOURCE, ((DWORD)(pEvent->Click.m_UID)) & RID_F_RESOURCE, true );
		break;
	case XCMD_ItemPickupReq: // Pick up Item
		if ( ! xCheckMsgSize( sizeof( pEvent->ItemPickupReq )))
			return(false);
		Event_Item_Pickup( (DWORD) pEvent->ItemPickupReq.m_UID, pEvent->ItemPickupReq.m_amount );
		break;
	case XCMD_ItemDropReq: // Drop Item
		if ( ! xCheckMsgSize( sizeof( pEvent->ItemDropReq )))
			return(false);
		Event_Item_Drop( CSphereUID( pEvent->ItemDropReq.m_UID ),
			CPointMap( pEvent->ItemDropReq.m_x, pEvent->ItemDropReq.m_y, pEvent->ItemDropReq.m_z ),
			CSphereUID( pEvent->ItemDropReq.m_UIDCont ));
		break;
	case XCMD_Click: // Singleclick
		if ( ! xCheckMsgSize( sizeof( pEvent->Click )))
			return(false);
		Event_SingleClick( (DWORD) pEvent->Click.m_UID );
		break;
	case XCMD_ExtCmd: // Ext. Command
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_ExtCmd( (EXTCMD_TYPE) pEvent->ExtCmd.m_type, pEvent->ExtCmd.m_name );
		break;
	case XCMD_ItemEquipReq: // Equip Item
		if ( ! xCheckMsgSize( sizeof( pEvent->ItemEquipReq )))
			return(false);
		Event_Item_Equip( CSphereUID( pEvent->ItemEquipReq.m_UID ),
			(LAYER_TYPE)( pEvent->ItemEquipReq.m_layer ),
			CSphereUID( pEvent->ItemEquipReq.m_UIDChar ));
		break;
	case XCMD_WalkAck: // Resync Request
		if ( ! xCheckMsgSize( sizeof( pEvent->WalkAck )))
			return(false);
		addReSync();
		break;
	case XCMD_DeathMenu:	// DeathOpt (un)Manifest ghost (size anomoly)
		if ( ! xCheckMsgSize(2))
			return(false);
		if ( ! Event_DeathOption( (DEATH_MODE_TYPE) pEvent->DeathMenu.m_mode, pEvent ))
			return( false );
		break;
	case XCMD_CharStatReq: // Status Request
		if ( ! xCheckMsgSize( sizeof( pEvent->CharStatReq )))
			return(false);
		if ( pEvent->CharStatReq.m_type == 4 )
		{
			addCharStatWindow( (DWORD) pEvent->CharStatReq.m_UID, true );
		}
		else if ( pEvent->CharStatReq.m_type == 5 )
		{
			addSkillWindow( m_pChar, SKILL_QTY, true );
		}
		else
		{
			DEBUG_MSG(("XCMD_CharStatReq Unk Type %d" LOG_CR, pEvent->CharStatReq.m_type ));
		}
		break;
	case XCMD_Skill:	// Skill locking.
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_Skill_Locks(pEvent);
		break;
	case XCMD_VendorBuy:	// Buy item from vendor.
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_VendorBuy( (DWORD) pEvent->VendorBuy.m_UIDVendor, pEvent );
		break;
	case XCMD_MapEdit:	// plot course on map.
		if ( ! xCheckMsgSize( sizeof( pEvent->MapEdit )))
			return(false);
		Event_MapEdit( (DWORD) pEvent->MapEdit.m_UID, pEvent );
		break;
	case XCMD_BookPage: // Read/Change Book
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_Book_Page( (DWORD) pEvent->BookPage.m_UID, pEvent );
		break;
	case XCMD_Options: // Options set
		if ( ! xCheckMsgSize(-1))
			return(false);
		DEBUG_MSG(( "%x:XCMD_Options len=%d" LOG_CR, m_Socket.GetSocket(), pEvent->Options.m_len ));
		break;
	case XCMD_Target: // Targeting
		if ( ! xCheckMsgSize( sizeof( pEvent->Target )))
			return(false);
		Event_Target( (DWORD)( pEvent->Target.m_context ),
			CSphereUID( pEvent->Target.m_UID ),
			CPointMap( pEvent->Target.m_x, pEvent->Target.m_y, pEvent->Target.m_z ),
			(ITEMID_TYPE)(WORD) pEvent->Target.m_id );	// if static tile.
		break;
	case XCMD_SecureTrade: // Secure trading
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_SecureTrade( (DWORD) pEvent->SecureTrade.m_UID,
			(SECURE_TRADE_TYPE) pEvent->SecureTrade.m_action,
			(bool)(DWORD) pEvent->SecureTrade.m_UID1 );
		break;
	case XCMD_BBoard: // BBoard Request.
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_BBoardRequest( (DWORD) pEvent->BBoard.m_UID, pEvent );
		break;
	case XCMD_War: // Combat Mode
		if ( ! xCheckMsgSize( sizeof( pEvent->War )))
			return(false);
		Event_CombatMode( pEvent->War.m_warmode );
		break;
	case XCMD_CharName: // Rename Character(pet)
		if ( ! xCheckMsgSize( sizeof( pEvent->CharName )))
			return(false);
		Event_SetName( (DWORD) pEvent->CharName.m_UID,
			pEvent->CharName.m_charname );
		break;
	case XCMD_MenuChoice: // Menu Choice
		if ( ! xCheckMsgSize( sizeof( pEvent->MenuChoice )))
			return(false);
		Event_MenuChoice( CSphereUID( pEvent->MenuChoice.m_UID ),
			pEvent->MenuChoice.m_context,
			pEvent->MenuChoice.m_select );
		break;
	case XCMD_BookOpen:	// Change a books title/author.
		if ( m_ProtoVer.GetCryptVer() < 0x126000 )
		{
			if ( ! xCheckMsgSize( sizeof( pEvent->BookOpen_v25 )))
				return(false);
			Event_Book_Title( (DWORD) pEvent->BookOpen_v25.m_UID,
				pEvent->BookOpen_v25.m_title, 
				pEvent->BookOpen_v25.m_author );
		}
		else
		{
			if ( ! xCheckMsgSize( sizeof( pEvent->BookOpen_v26 )))
				return(false);
			Event_Book_Title( (DWORD) pEvent->BookOpen_v26.m_UID, 
				pEvent->BookOpen_v26.m_title, 
				pEvent->BookOpen_v26.m_author );
		}
		break;
	case XCMD_DyeVat: // Color Select Dialog
		if ( ! xCheckMsgSize( sizeof( pEvent->DyeVat )))
			return(false);
		Event_Item_Dye( (DWORD) pEvent->DyeVat.m_UID, pEvent->DyeVat.m_wHue );
		break;
	case XCMD_Prompt: // Response to console prompt.
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_PromptResp( pEvent->Prompt.m_text, pEvent->Prompt.m_len-sizeof(pEvent->Prompt));
		break;
	case XCMD_HelpPage: // GM Page (i want to page a gm!)
		if ( ! xCheckMsgSize( sizeof( pEvent->HelpPage )))
			return(false);
		if ( m_pChar == NULL )
			return( false );
		{
		CSphereExpContext exec( m_pChar, this );
		exec.ExecuteCommand( "HelpPage" );
		}
		break;
	case XCMD_VendorSell: // Vendor Sell
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_VendorSell( (DWORD) pEvent->VendorSell.m_UIDVendor,
			pEvent->VendorSell.m_count, 
			pEvent->VendorSell.m_item );
		break;
	case XCMD_Scroll:	// Scroll Closed
		if ( ! xCheckMsgSize( sizeof( pEvent->Scroll )))
			return(false);
		Event_ScrollClose( (DWORD) pEvent->Scroll.m_context );
		break;
	case XCMD_GumpInpValRet:	// Gump text input
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_GumpInpValRet(pEvent);
		break;
	case XCMD_TalkUNICODE:	// Talk unicode.
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_TalkUNICODE(pEvent);
		break;
	case XCMD_GumpDialogRet:	// Gump menu.
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_GumpDialogRet(pEvent);
		break;

	case XCMD_ChatText:	// ChatText
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_ChatText( pEvent->ChatText.m_utext, pEvent->ChatText.m_len, CLanguageID( pEvent->ChatText.m_lang ));
		break;
	case XCMD_Chat: // Chat
		if ( ! xCheckMsgSize( sizeof( pEvent->Chat)))
			return(false);
		Event_ChatButton(pEvent->Chat.m_uname);
		break;
	case XCMD_ToolTipReq:	// Tool Tip Req
		if ( ! xCheckMsgSize( sizeof( pEvent->ToolTipReq )))
			return(false);
		Event_ToolTip( (DWORD) pEvent->ToolTipReq.m_UID );
		break;
	case XCMD_CharProfile:	// Get Character Profile.
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_Profile( pEvent->CharProfile.m_WriteMode, (DWORD) pEvent->CharProfile.m_UID, pEvent );
		break;
	case XCMD_MailMsg:
		if ( ! xCheckMsgSize( sizeof(pEvent->MailMsg)))
			return(false);
		Event_MailMsg( (DWORD) pEvent->MailMsg.m_uid1, (DWORD) pEvent->MailMsg.m_uid2 );
		break;
	case XCMD_ClientVersion:	// Client Version string packet
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_ClientVersion( pEvent->ClientVersion.m_text, pEvent->ClientVersion.m_len-3 );
		break;
	case XCMD_ExtData:	// Add someone to the party system.
		if ( ! xCheckMsgSize(-1))
			return(false);
		Event_ExtData( (EXTDATA_TYPE)(WORD) pEvent->ExtData.m_type, &(pEvent->ExtData.m_u), pEvent->ExtData.m_len-5 );
		break;

	case XCMD_UnkChangeName: // 0x98 = variable.
	case XCMD_UnkUpdateRange: // 0xc8 = 90 bytes.
		// ignore this new unknown message
		break;

	default:
		// clear socket I have no idea what this is.
		return( false );
	}

	return( true );
}


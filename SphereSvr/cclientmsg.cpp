//
// CClientMsg.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
// Game server messages. (No login stuff)
//

#include "stdafx.h"	// predef header.

/////////////////////////////////////////////////////////////////
// -CClient stuff.

void CClient::addPause( bool fPause )
{
	// 0 = restart. 1=pause
	DEBUG_CHECK( this );
	if ( this == NULL )
		return;
	if ( m_pChar == NULL )
		return;
	if ( m_fPaused == fPause )
		return;

	if ( ! fPause )
	{
		if ( m_fUpdateStats )
		{
			// update all my own stats.
			addCharStatWindow( m_pChar->GetUID(), false );
		}
	}

	m_fPaused = fPause;
	CUOCommand cmd;
	cmd.Pause.m_Cmd = XCMD_Pause;
	cmd.Pause.m_Arg = fPause;
	xSendPkt( &cmd, sizeof( cmd.Pause ));
}

bool CClient::addDeleteErr( DELETE_ERR_TYPE code )
{
	// code
	if ( code == DELETE_SUCCESS )
		return true;

	DEBUG_ERR(( "%x:Bad Char Delete Attempted %d" LOG_CR, m_Socket.GetSocket(), code ));

	CUOCommand cmd;
	cmd.DeleteBad.m_Cmd = XCMD_DeleteBad;
	cmd.DeleteBad.m_code = code;
	xSendPkt( &cmd, sizeof( cmd.DeleteBad ));
	xFlush();
	return( false );
}

void CClient::addExtData( EXTDATA_TYPE type, const CUOExtData* pData, int iSize )
{
	// adds 5 bytes of overhead.
	CUOCommand cmd;
	int iSizeTotal = iSize + sizeof(cmd.ExtData) - sizeof(cmd.ExtData.m_u);
	cmd.ExtData.m_Cmd = XCMD_ExtData;
	cmd.ExtData.m_len = iSizeTotal;
	cmd.ExtData.m_type = type;
	memcpy( &(cmd.ExtData.m_u), pData, iSize );
	xSendPkt( &cmd, iSizeTotal );
}

void CClient::addOptions()
{
	CUOCommand cmd;

#if 0
	// set options ? // ??? not needed ?
	for ( int i=1; i<=3; i++ )
	{
		cmd.Options.m_Cmd = XCMD_Options;
		cmd.Options.m_len = sizeof( cmd.Options );
		cmd.Options.m_index = i;
		xSendPkt( &cmd, sizeof( cmd.Options ));
	}
#endif

	cmd.ReDrawAll.m_Cmd = XCMD_ReDrawAll;	// ???
	xSendPkt( &cmd, sizeof( cmd.ReDrawAll ));

#if 0
	// Send time. (real or game time ??? why ?)
	long lCurrentTime = CServTime::GetCurrentTime();
	cmd.Time.m_Cmd = XCMD_Time;
	cmd.Time.m_hours = ( lCurrentTime / ( 60*60*TICKS_PER_SEC )) % 24;
	cmd.Time.m_min   = ( lCurrentTime / ( 60*TICKS_PER_SEC )) % 60;
	cmd.Time.m_sec   = ( lCurrentTime	/ ( TICKS_PER_SEC )) % 60;
	xSendPkt( &cmd, sizeof( cmd.Time ));
#endif

}

void CClient::addObjectRemoveCantSee( CSphereUID uid, LPCTSTR pszName )
{
	// Seems this object got out of sync some how.
	if ( pszName == NULL ) pszName = "it";
	Printf( "You can't see %s", pszName );
	addObjectRemove( uid );
}

void CClient::addObjectRemove( CSphereUID uid )
{
	// Tell the client to remove the item or char
	DEBUG_CHECK( m_pChar );
	addPause();
	CUOCommand cmd;
	cmd.Remove.m_Cmd = XCMD_Remove;
	cmd.Remove.m_UID = uid;
	xSendPkt( &cmd, sizeof( cmd.Remove ));
}

void CClient::addObjectRemove( const CObjBase* pObj )
{
	addObjectRemove( pObj->GetUID());
}

void CClient::addRemoveAll( bool fItems, bool fChars )
{
	addPause();
	if ( fItems )
	{
		// Remove any multi objects first ! or client will hang
		CWorldSearch AreaItems( GetChar()->GetTopPoint(), SPHEREMAP_VIEW_RADAR );
		AreaItems.SetFilterAllShow( IsPrivFlag( PRIV_ALLSHOW ));	// show logged out chars?
		for(;;)
		{
			CItemPtr pItem = AreaItems.GetNextItem();
			if ( pItem == NULL )
				break;
			addObjectRemove( pItem );
		}
	}
	if ( fChars )
	{
		// Remove any multi objects first ! or client will hang
		CWorldSearch AreaChars( GetChar()->GetTopPoint(), SPHEREMAP_VIEW_SIZE );
		AreaChars.SetFilterAllShow( IsPrivFlag( PRIV_ALLSHOW ));	// show logged out chars?
		for(;;)
		{
			CCharPtr pChar = AreaChars.GetNextChar();
			if ( pChar == NULL )
				break;
			addObjectRemove( pChar );
		}
	}
}

void CClient::addObjectLight( const CObjBase* pObj, LIGHT_PATTERN iLightPattern )		// Object light level.
{
	// ??? this does not seem to work !
	if ( pObj == NULL )
		return;
	CUOCommand cmd;
	cmd.LightPoint.m_Cmd = XCMD_LightPoint;
	cmd.LightPoint.m_UID = pObj->GetUID();
	cmd.LightPoint.m_level = iLightPattern;	// light level/pattern.
	xSendPkt( &cmd, sizeof( cmd.LightPoint ));
}

void CClient::addItem_OnGround( CItem* pItem ) // Send items (on ground)
{
	// Show the client an item on the ground.
	ASSERT(pItem);
	// Get base values.
	DWORD dwUID = pItem->GetUID();
	ITEMID_TYPE wID = pItem->GetDispID();
	HUE_TYPE wHue = pItem->GetHue();
	WORD wAmount = ( pItem->GetAmount() > 1 ) ? pItem->GetAmount() : 0;
	CPointMap pt = pItem->GetTopPoint();
	BYTE bFlags = 0;
	BYTE bDir = DIR_N;

	// Modify the values for the specific client/item.
	bool fHumanCorpse = ( wID == ITEMID_CORPSE && CCharDef::IsHumanID( pItem->GetCorpseType()));

	if ( m_pChar->IsStatFlag( STATF_Hallucinating ))
	{
		// cmd.Put.m_id = pItem->GetDispID();	// ??? random displayable item ?
		wHue = Calc_GetRandVal( HUE_DYE_HIGH );	// restrict colors
	}
	else if ( wID != ITEMID_CORPSE )
	{
		if ( wHue & HUE_UNDERWEAR )	// on monster this just colors the underwaer. thats it.
			wHue = 0;
		else if (( wHue & HUE_MASK_HI ) > HUE_QTY )
			wHue &= HUE_MASK_LO | HUE_TRANSLUCENT;
		else
			wHue &= HUE_MASK_HI | HUE_TRANSLUCENT;
	}
	else
	{
		// allow HUE_UNDERWEAR colors only on corpses
		if (( wHue & HUE_MASK_HI ) > HUE_QTY )
			wHue &= HUE_MASK_LO | HUE_UNDERWEAR | HUE_TRANSLUCENT;
		else
			wHue &= HUE_MASK_HI | HUE_UNDERWEAR | HUE_TRANSLUCENT;
	}

	if ( m_pChar->CanMove( pItem, false ))	// wID != ITEMID_CORPSE &&
	{
		bFlags |= ITEMF_MOVABLE;
	}

	if ( IsPrivFlag(PRIV_DEBUG))
	{
		// just use safe numbers
		wID = ITEMID_WorldGem;	// bigger item ???
		pt.m_z = m_pChar->GetTopZ();
		wAmount = 0;
		bFlags |= ITEMF_MOVABLE;
	}
	else
	{
		if ( ! m_pChar->CanSeeItem( pItem ))
		{
			bFlags |= ITEMF_INVIS;
		}
		if ( pItem->Item_GetDef()->Can( CAN_I_LIGHT ))	// ??? Gate or other ? IT_LIGHT_LIT ?
		{
			if ( pItem->IsTypeLit())
			{
				bDir = pItem->m_itLight.m_pattern;
			}
			else
			{
				bDir = LIGHT_LARGE;
			}
		}
		else if ( wID == ITEMID_CORPSE )	// IsType( IT_CORPSE )
		{
			// If this item has direction facing use it
			bDir = pItem->m_itCorpse.m_facing_dir;	// DIR_N
		}
	}

	// Pack values in our strange format.
	CUOCommand cmd;
	BYTE* pData = cmd.m_Raw + 9;

	cmd.Put.m_Cmd = XCMD_Put;

	if ( wAmount ) dwUID |= RID_F_RESOURCE;	// Enable amount feild
	cmd.Put.m_UID = dwUID;

	cmd.Put.m_id = wID;
	if ( wAmount )
	{
		PACKWORD(pData,wAmount);
		pData += 2;
	}

	if ( bDir ) pt.m_x |= 0x8000;
	PACKWORD(pData,pt.m_x);
	pData += 2;

	if ( wHue ) pt.m_y |= 0x8000;	 // Enable m_wHue and m_movable
	if ( bFlags ) pt.m_y |= 0x4000;
	PACKWORD(pData,pt.m_y);
	pData += 2;

	if ( bDir )
	{
		pData[0] = bDir;
		pData++;
	}

	pData[0] = pt.m_z;
	pData++;

	if ( wHue )
	{
		PACKWORD(pData,wHue);
		pData += 2;
	}

	if ( bFlags )	// m_flags = ITEMF_MOVABLE (020, 080)
	{
		pData[0] = bFlags;
		pData++;
	}

	int iLen = pData - (cmd.m_Raw);
	ASSERT( iLen );
	cmd.Put.m_len = iLen;

	xSendPkt( &cmd, iLen );

	if ( pItem->IsType(IT_SOUND))
	{
		addSound( (SOUND_TYPE) pItem->m_itSound.m_Sound, pItem, pItem->m_itSound.m_Repeat );
	}

	if ( ! IsPrivFlag(PRIV_DEBUG) && fHumanCorpse )	// cloths on corpse
	{
		CItemCorpsePtr pCorpse = PTR_CAST(CItemCorpse,pItem);
		ASSERT(pCorpse);

		// send all the items on the corpse.
		addContents( pCorpse, false, true, false );
		// equip the proper items on the corpse.
		addContents( pCorpse, true, true, false );
	}
}

void CClient::addItem_Equipped( const CItem* pItem )
{
	ASSERT(pItem);
	// Equip a single item on a CChar.
	CCharPtr pChar = PTR_CAST(CChar,pItem->GetParent());
	ASSERT( pChar != NULL );
	DEBUG_CHECK( pItem->GetEquipLayer() < LAYER_SPECIAL );

	if ( ! m_pChar->CanSeeItem( pItem ) && m_pChar != pChar )
		return;

	LAYER_TYPE layer = pItem->GetEquipLayer();

	CUOCommand cmd;
	cmd.ItemEquip.m_Cmd = XCMD_ItemEquip;
	cmd.ItemEquip.m_UID = pItem->GetUID();
	cmd.ItemEquip.m_id = ( layer == LAYER_BANKBOX ) ? ITEMID_CHEST_SILVER : pItem->GetDispID();
	cmd.ItemEquip.m_zero7 = 0;
	cmd.ItemEquip.m_layer = layer;
	cmd.ItemEquip.m_UIDchar = pChar->GetUID();

	HUE_TYPE wHue;
	GetAdjustedItemID( pChar, pItem, wHue );
	cmd.ItemEquip.m_wHue = wHue;

	xSendPkt( &cmd, sizeof( cmd.ItemEquip ));
}

void CClient::addItem_InContainer( const CItem* pItem )
{
	ASSERT(pItem);
	CItemContainerPtr pCont = PTR_CAST( CItemContainer, pItem->GetParent());
	if ( pCont == NULL )
		return;

	POINT pt = pItem->GetContainedPoint();
	ITEMID_TYPE id = pItem->GetDispID();
	if ( id >= ITEMID_MULTI )
		id = ITEMID_GEM1;

	// Add a single item in a container.
	CUOCommand cmd;
	cmd.ContAdd.m_Cmd = XCMD_ContAdd;
	cmd.ContAdd.m_UID = pItem->GetUID();
	cmd.ContAdd.m_id = id;
	cmd.ContAdd.m_zero7 = 0;
	cmd.ContAdd.m_amount = pItem->GetAmount();
	cmd.ContAdd.m_x = pt.x;
	cmd.ContAdd.m_y = pt.y;
	cmd.ContAdd.m_UIDCont = pCont->GetUID();

	HUE_TYPE wHue;
	if ( m_pChar->IsStatFlag( STATF_Hallucinating ))
	{
		wHue = Calc_GetRandVal( HUE_DYE_HIGH );	// restrict colors
	}
	else
	{
		wHue = pItem->GetHue() & HUE_MASK_HI;
		if ( wHue > HUE_QTY )
			wHue &= HUE_MASK_LO;
	}
	cmd.ContAdd.m_wHue = wHue;

	xSendPkt( &cmd, sizeof( cmd.ContAdd ));
}

void CClient::addItem( CItem* pItem )
{
	if ( pItem == NULL )
		return;
	addPause();
	if ( pItem->IsTopLevel())
	{
		addItem_OnGround( pItem );
	}
	else if ( pItem->IsItemEquipped())
	{
		addItem_Equipped( pItem );
	}
	else if ( pItem->IsItemInContainer())
	{
		addItem_InContainer( pItem );
	}
}

int CClient::addContents( const CItemContainer* pContainer, bool fCorpseEquip, bool fCorpseFilter, bool fShop ) // Send Backpack (with items)
{
	// NOTE: We needed to send the header for this FIRST !!!
	// 1 = equip a corpse
	// 0 = contents.

	addPause();

	bool fLayer[LAYER_HORSE];	// make sure layers only used 1 time.
	memset( fLayer, 0, sizeof(fLayer));

	CUOCommand cmd;

	// send all the items in the container.
	int count = 0;
	for ( CItemPtr pItem=pContainer->GetHead(); pItem!=NULL; pItem=pItem->GetNext())
	{
		LAYER_TYPE layer;
		if ( fCorpseFilter )	// dressing a corpse is different from opening the coffin!
		{
			layer = (LAYER_TYPE) pItem->GetContainedLayer();
			ASSERT( layer < LAYER_HORSE );
			switch ( layer )	// don't put these on a corpse.
			{
			case LAYER_NONE:
			case LAYER_PACK:	// these display strange.
				// case LAYER_LIGHT:
				continue;
			case LAYER_HIDDEN:
				DEBUG_CHECK(0);
				continue;
			}
			// Make certain that no more than one of each layer goes out to client....crashes otherwise!!
			if ( fLayer[layer] )
			{
				DEBUG_CHECK( !fLayer[layer]);
				continue;
			}
			fLayer[layer] = true;
		}
		if ( fCorpseEquip )	// list equipped items on a corpse
		{
			ASSERT( fCorpseFilter );
			cmd.CorpEquip.m_item[count].m_layer	= pItem->GetContainedLayer();
			cmd.CorpEquip.m_item[count].m_UID	= pItem->GetUID();
		}
		else	// Content items
		{
			ITEMID_TYPE id = pItem->GetDispID();
			if ( id >= ITEMID_MULTI )
				id = ITEMID_GEM1;

			cmd.Content.m_item[count].m_UID		= pItem->GetUID();
			cmd.Content.m_item[count].m_id		= id;
			cmd.Content.m_item[count].m_zero6	= 0;
			cmd.Content.m_item[count].m_amount	= pItem->GetAmount();
			if ( fShop )
			{
				CItemVendablePtr pVendItem = REF_CAST(CItemVendable,pItem);
				if ( ! pVendItem )
					continue;
				if ( ! pVendItem->GetAmount())
					continue;
				if ( pVendItem->IsType(IT_GOLD))
					continue;
				if ( pVendItem->GetAmount() > g_Cfg.m_iVendorMaxSell )
				{
					cmd.Content.m_item[count].m_amount = g_Cfg.m_iVendorMaxSell;
				}
				cmd.Content.m_item[count].m_x	= count;
				cmd.Content.m_item[count].m_y	= count;
			}
			else
			{
				POINT pt = pItem->GetContainedPoint();
				cmd.Content.m_item[count].m_x	= pt.x;
				cmd.Content.m_item[count].m_y	= pt.y;
			}
			cmd.Content.m_item[count].m_UIDCont	= pContainer->GetUID();

			HUE_TYPE wHue;
			if ( m_pChar->IsStatFlag( STATF_Hallucinating ))
			{
				wHue = Calc_GetRandVal( HUE_DYE_HIGH );
			}
			else
			{
				wHue = pItem->GetHue() & HUE_MASK_HI;
				if ( wHue > HUE_QTY )
					wHue &= HUE_MASK_LO;	// restrict colors
			}
			cmd.Content.m_item[count].m_wHue = wHue;
		}
		count ++;
	}

	if ( ! count )
	{
		return 0;
	}
	int len;
	if ( fCorpseEquip )
	{
		cmd.CorpEquip.m_item[count].m_layer = LAYER_NONE;	// terminator.
		len = sizeof( cmd.CorpEquip ) - sizeof(cmd.CorpEquip.m_item) + ( count* sizeof(cmd.CorpEquip.m_item[0])) + 1;
		cmd.CorpEquip.m_Cmd = XCMD_CorpEquip;
		cmd.CorpEquip.m_len = len;
		cmd.CorpEquip.m_UID = pContainer->GetUID();
	}
	else
	{
		len = sizeof( cmd.Content ) - sizeof(cmd.Content.m_item) + ( count* sizeof(cmd.Content.m_item[0]));
		cmd.Content.m_Cmd = XCMD_Content;
		cmd.Content.m_len = len;
		cmd.Content.m_count = count;
	}

	xSendPkt( &cmd, len );
	return( count );
}

void CClient::addContentsTopDown( const CItemContainer* pContainer )
{
	// This descends recursively up to the top container and then
	// ascends back down, adding contents as it goes

	if ( pContainer == NULL )
		return;
	CItemContainerPtr pAncestor = REF_CAST( CItemContainer, pContainer->GetContainer());
	if( pAncestor )
	{
		addContentsTopDown( pAncestor );
	}
	addContents( pContainer, false, false, false );
}

void CClient::addOpenGump( const CObjBase* pContainer, GUMP_TYPE gump )
{
	// NOTE: if pContainer has not already been sent to the client
	//  this will crash client.
	CUOCommand cmd;
	cmd.ContOpen.m_Cmd = XCMD_ContOpen;
	cmd.ContOpen.m_UID = pContainer->GetUID();
	cmd.ContOpen.m_gump = gump;

	// we automatically get open sound for this,.
	xSendPkt( &cmd, sizeof( cmd.ContOpen ));
}

bool CClient::addContainerSetup( const CItemContainer* pContainer ) // Send Backpack (with items)
{
	ASSERT(pContainer);
	DEBUG_CHECK( ! pContainer->IsWeird());
	ASSERT( pContainer->IsItem());

	// open the conatiner with the proper GUMP.
	CItemDefPtr pItemDef = pContainer->Item_GetDef();
	GUMP_TYPE gump = pItemDef->IsTypeContainer();
	if ( gump <= GUMP_RESERVED )
	{
		return false;
	}

	addOpenGump( pContainer, gump );
	addContents( pContainer, false, false, false );
	return( true );
}

void CClient::addSeason( SEASON_TYPE season, bool fNormalCursor )
{
	ASSERT(m_pChar);
	if ( m_pChar->IsStatFlag( STATF_DEAD ))	// everything looks like this when dead.
	{
		season = SEASON_Desolate;
	}
	if ( season == m_Env.m_Season )	// the season i saw last.
		return;

	m_Env.m_Season = season;

	CUOCommand cmd;
	cmd.Season.m_Cmd = XCMD_Season;
	cmd.Season.m_season = season;
	cmd.Season.m_cursor = fNormalCursor ? 1 : 0;
	xSendPkt( &cmd, sizeof( cmd.Season ));
}

void CClient::addWeather( WEATHER_TYPE weather ) // Send new weather to player
{
	ASSERT( m_pChar );

	if ( g_Cfg.m_fNoWeather )
		return;

	if ( m_pChar->IsStatFlag( STATF_InDoors ))
	{
		// If there is a roof over our head at the moment then stop rain.
		weather = WEATHER_DRY;
	}
	else if ( weather == WEATHER_DEFAULT )
	{
		weather = m_pChar->GetTopSector()->GetWeather();
	}

	if ( weather == m_Env.m_Weather )
		return;

	CUOCommand cmd;
	cmd.Weather.m_Cmd = XCMD_Weather;
	cmd.Weather.m_type = weather;
	cmd.Weather.m_ext1 = 0x40;
	cmd.Weather.m_unk010 = 0x10;

	// m_ext = seems to control a very gradual fade in and out ?
	switch ( weather )
	{
	case WEATHER_RAIN:	// rain
	case WEATHER_STORM:
	case WEATHER_SNOW:	// snow
		// fix weird client transition problem.
		// May only transition from Dry.
		addWeather( WEATHER_DRY );
		break;
	default:	// dry or cloudy
		cmd.Weather.m_type = WEATHER_DRY;
		cmd.Weather.m_ext1 = 0;
		break;
	}

	m_Env.m_Weather = weather;
	xSendPkt( &cmd, sizeof(cmd.Weather));
}

void CClient::addLight( int iLight )
{
	// NOTE: This could just be a flash of light.
	// Global light level.
	ASSERT(m_pChar);
	if ( iLight < LIGHT_BRIGHT )
	{
		iLight = m_pChar->GetLightLevel();
	}

	// Scale light level for non-t2a.
	if ( iLight < LIGHT_BRIGHT )
		iLight = LIGHT_BRIGHT;
	if ( iLight > LIGHT_DARK )
		iLight = LIGHT_DARK;

	if ( m_ProtoVer.GetCryptVer() <= 0x126020 )
	{
		// Scale to old darkness.
		iLight = IMULDIV( iLight, LIGHT_DARK_OLD, LIGHT_DARK );
	}

	if ( iLight == m_Env.m_Light )
		return;
	m_Env.m_Light = iLight;

	CUOCommand cmd;
	cmd.Light.m_Cmd = XCMD_Light;
	cmd.Light.m_level = iLight;
	xSendPkt( &cmd, sizeof( cmd.Light ));
}

void CClient::addArrowQuest( int x, int y )
{
	CUOCommand cmd;
	cmd.Arrow.m_Cmd = XCMD_Arrow;
	cmd.Arrow.m_Active = ( x && y ) ? 1 : 0;	// 1/0
	cmd.Arrow.m_x = x;
	cmd.Arrow.m_y = y;
	xSendPkt( &cmd, sizeof( cmd.Arrow ));
}

void CClient::addMusic( WORD id )
{
	// Music is ussually appropriate for the region.
	CUOCommand cmd;
	cmd.PlayMusic.m_Cmd = XCMD_PlayMusic;
	cmd.PlayMusic.m_musicid = id;
	xSendPkt( &cmd, sizeof( cmd.PlayMusic ));
}

HRESULT CClient::addKick( CScriptConsole* pSrc, DWORD dwMinutes )
{
	// Kick me out.
	ASSERT( pSrc );
	if ( GetAccount() == NULL )
	{
		m_Socket.Close();
		return( NO_ERROR );
	}

	if ( ! GetAccount()->Block( pSrc, dwMinutes ))
		return( HRES_INVALID_HANDLE );

	LPCTSTR pszAction = dwMinutes ? "KICKED" : "DISCONNECTED";
	Printf( "You have been %s by '%s'", (LPCTSTR) pszAction, (LPCTSTR) pSrc->GetName());

	if ( IsConnectTypePacket())
	{
		CUOCommand cmd;
		cmd.Kick.m_Cmd = XCMD_Kick;
		cmd.Kick.m_unk1 = 0; // pSrc->GetUID();	// The kickers uid ?
		xSendPkt( &cmd, sizeof( cmd.Kick ));
	}

	m_Socket.Close();
	return( NO_ERROR );
}

void CClient::addSound( SOUND_TYPE id, const CObjBaseTemplate* pBase, int iOnce )
{
	// ARGS:
	//  iOnce = 1 = default.

	CPointMap pt;
	if ( pBase )
	{
		pBase = pBase->GetTopLevelObj();
		pt = pBase->GetTopPoint();
	}
	else
	{
		pt = m_pChar->GetTopPoint();
	}

	if ( id <= 0 || id == SOUND_CLEAR )
	{
		// Clear any recurring sounds.
		if ( ! iOnce )
		{
			// Only if far away from the last one.
			if ( ! m_pntSoundRecur.IsValidPoint())
				return;
			if ( m_pntSoundRecur.GetDist( pt ) < SPHEREMAP_VIEW_SIZE )
				return;
		}

		// Force a clear.
		m_pntSoundRecur.InitPoint();
	}
	else
	{
		if ( ! iOnce )
		{
			// This sound should repeat.
			if ( ! pBase )
				return;
			if ( m_pntSoundRecur.IsValidPoint())
			{
				addSound( SOUND_CLEAR );
			}
			m_pntSoundRecur = pt;
		}
	}

	CUOCommand cmd;
	cmd.Sound.m_Cmd = XCMD_Sound;
	cmd.Sound.m_flags = iOnce;
	cmd.Sound.m_id = id;
	cmd.Sound.m_volume = 0;
	cmd.Sound.m_x = pt.m_x;
	cmd.Sound.m_y = pt.m_y;
	cmd.Sound.m_z = pt.m_z;

	xSendPkt( &cmd, sizeof(cmd.Sound));
}

void CClient::addItemDragCancel( BYTE type )
{
	// Sound seems to be automatic ???
	// addSound( 0x051 );
	CUOCommand cmd;
	cmd.DragCancel.m_Cmd = XCMD_DragCancel;
	cmd.DragCancel.m_type = type;
	xSendPkt( &cmd, sizeof( cmd.DragCancel ));
}

void CClient::addBarkSpeakTable( SPKTAB_TYPE index, const CObjBaseTemplate* pSrc, HUE_TYPE wHue, TALKMODE_TYPE mode, FONT_TYPE font )
{
	// The text is really client side.
	// this is just a resource id for it.

	CUOCommand cmd;
	cmd.SpeakTable.m_Cmd = XCMD_SpeakTable;

	if ( pSrc == NULL || pSrc->IsItem())
	{
		cmd.SpeakTable.m_id = 0;
	}
	else	// char id only.
	{
		CCharPtr pChar = PTR_CAST(CChar,const_cast <CObjBaseTemplate*>(pSrc));
		ASSERT(pChar);
		cmd.SpeakTable.m_id = pChar->GetDispID();
	}

	cmd.SpeakTable.m_mode = mode;		// 9 = TALKMODE_TYPE
	cmd.SpeakTable.m_wHue = wHue;		// 10-11 = HUE_TYPE.
	cmd.SpeakTable.m_font = font;		// 12-13 = FONT_TYPE
	cmd.SpeakTable.m_index = index; // predefined message type (SPKTAB_TYPE)

	int iLen = sizeof(cmd.SpeakTable);
	int iNameLen;
	if ( pSrc == NULL )
	{
		cmd.SpeakTable.m_UID = 0;
		iNameLen = strcpylen( cmd.SpeakTable.m_charname, "System" );
	}
	else
	{
		cmd.SpeakTable.m_UID = pSrc->GetUID();
		iNameLen = strcpylen( cmd.SpeakTable.m_charname, (LPCTSTR) pSrc->GetName(), sizeof(cmd.SpeakTable.m_charname));
	}
	memset( cmd.SpeakTable.m_charname+iNameLen, 0, sizeof(cmd.SpeakTable.m_charname)-iNameLen );

	cmd.SpeakTable.m_len = iLen;		// 1-2 = var len size. (50)
	xSendPkt( &cmd, iLen );
}

void CClient::addBarkUNICODE( const NCHAR* pwText, const CObjBaseTemplate* pSrc, HUE_TYPE wHue, TALKMODE_TYPE mode, FONT_TYPE font, CLanguageID lang )
{
	if ( pwText == NULL )
		return;

	if ( ! IsConnectTypePacket())
	{
		// Need to convert back from unicode !
		// WriteString(pwText);
		return;
	}

	if ( mode == TALKMODE_BROADCAST )
	{
		mode = TALKMODE_SYSTEM;
		pSrc = NULL;
	}
	if ( mode < 0 || mode > TALKMODE_CLIENT_MAX )
	{
		mode = TALKMODE_CLIENT_MAX;
	}
	if ( wHue > HUE_DYE_HIGH )
	{
		wHue = HUE_TEXT_DEF;
	}

	if ( ISINTRESOURCE(pwText))
	{
		// SPKTAB_TYPE
		// This is not really a string but just a resource ID on the client side.
		addBarkSpeakTable( (SPKTAB_TYPE) GETINTRESOURCE(pwText), pSrc, wHue, mode, font );
		return;
	}

	CUOCommand cmd;
	cmd.SpeakUNICODE.m_Cmd = XCMD_SpeakUNICODE;

	// string copy unicode (til null)
	int i=0;
	for ( ; pwText[i] && i < MAX_TALK_BUFFER; i++ )
	{
		cmd.SpeakUNICODE.m_utext[i] = pwText[i];
	}
	cmd.SpeakUNICODE.m_utext[i++] = '\0';	// add for the null

	int len = sizeof(cmd.SpeakUNICODE) + (i*sizeof(NCHAR));
	cmd.SpeakUNICODE.m_len = len;
	cmd.SpeakUNICODE.m_mode = mode;		// mode = range.
	cmd.SpeakUNICODE.m_wHue = wHue;
	cmd.SpeakUNICODE.m_font = font;		// font. 3 = system message just to you !

	lang.GetStrDef(cmd.SpeakUNICODE.m_lang);

	int iNameLen;
	if ( pSrc == NULL )
	{
		cmd.SpeakUNICODE.m_UID = 0;	// 0x01010101;
		iNameLen = strcpylen( cmd.SpeakUNICODE.m_charname, "System" );
	}
	else
	{
		cmd.SpeakUNICODE.m_UID = pSrc->GetUID();
		iNameLen = strcpylen( cmd.SpeakUNICODE.m_charname, (LPCTSTR) pSrc->GetName(), sizeof(cmd.SpeakUNICODE.m_charname));
	}
	memset( cmd.SpeakUNICODE.m_charname+iNameLen, 0, sizeof(cmd.SpeakUNICODE.m_charname)-iNameLen );

	if ( pSrc == NULL || pSrc->IsItem())
	{
		cmd.SpeakUNICODE.m_id = 0;	// 0x0101;
	}
	else	// char id only.
	{
		CCharPtr pChar = PTR_CAST(CChar,const_cast <CObjBaseTemplate*>(pSrc));
		ASSERT(pChar);
		cmd.SpeakUNICODE.m_id = pChar->GetDispID();
	}
	xSendPkt( &cmd, len );
}

void CClient::addBark( LPCTSTR pszText, const CObjBaseTemplate* pSrc, HUE_TYPE wHue, TALKMODE_TYPE mode, FONT_TYPE font )
{
	// HUE_TYPE = HUE_BLACK
	if ( pszText == NULL )
		return;

	if ( ! IsConnectTypePacket())
	{
		WriteString(pszText);
		return;
	}

	if ( mode == TALKMODE_BROADCAST )
	{
		mode = TALKMODE_SYSTEM;
		pSrc = NULL;
	}
	if ( mode < 0 || mode > TALKMODE_CLIENT_MAX )
	{
		mode = TALKMODE_CLIENT_MAX;
	}
	if ( wHue > HUE_DYE_HIGH )
	{
		wHue = HUE_TEXT_DEF;
	}

	CUOCommand cmd;

	if ( ISINTRESOURCE(pszText))
	{
		// SPKTAB_TYPE
		// This is not really a string but just a resource ID on the client side.
		addBarkSpeakTable( (SPKTAB_TYPE) GETINTRESOURCE(pszText), pSrc, wHue, mode, font );
		return;
	}

	cmd.Speak.m_Cmd = XCMD_Speak;
	int len = strlen(pszText);
	DEBUG_CHECK( len < MAX_TALK_BUFFER );
	len += sizeof(cmd.Speak);
	cmd.Speak.m_len = len;
	cmd.Speak.m_mode = mode;		// mode = range.
	cmd.Speak.m_wHue = wHue;	// needs to be in range ( 0 to HUE_DYE_HIGH )
	cmd.Speak.m_font = font;		// font. 3 = system message just to you !

	int iNameLen;
	if ( pSrc == NULL )
	{
		cmd.Speak.m_UID = 0;
		iNameLen = strcpylen( cmd.Speak.m_charname, "System" );
	}
	else
	{
		cmd.Speak.m_UID = pSrc->GetUID();
		iNameLen = strcpylen( cmd.Speak.m_charname, (LPCTSTR) pSrc->GetName(), sizeof(cmd.Speak.m_charname));
	}
	memset( cmd.Speak.m_charname+iNameLen, 0, sizeof(cmd.Speak.m_charname)-iNameLen );

	if ( pSrc == NULL || pSrc->IsItem())
	{
		cmd.Speak.m_id = 0; // 0x0101;
	}
	else	// char id only.
	{
		CCharPtr pChar = PTR_CAST(CChar,const_cast <CObjBaseTemplate*>(pSrc));
		ASSERT(pChar);
		cmd.Speak.m_id = pChar->GetDispID();
	}
	strcpylen( cmd.Speak.m_text, pszText, MAX_TALK_BUFFER );
	xSendPkt( &cmd, len );
}

void CClient::addObjMessage( LPCTSTR pMsg, const CObjBaseTemplate* pSrc, HUE_TYPE wHue ) // The message when an item is clicked
{
	addBark( pMsg, pSrc, wHue, ( pSrc == m_pChar ) ? TALKMODE_OBJ : TALKMODE_ITEM, FONT_NORMAL );
}

void CClient::addSysMessage( LPCTSTR pszMsg, HUE_TYPE wHue ) // System message (In lower left corner)
{
	// m_pChar->m_SpeechHue
	addBark( pszMsg, NULL, wHue, TALKMODE_SYSTEM, FONT_NORMAL );
}

void CClient::addSysMessageU( CGVariant& vArgs ) // System message (In lower left corner)
{
	// SYSMESSAGEU
	// convert to UNICODE.

#if 0
	if ( vArgs.GetStr()[0] == '"' )
	{
		// Parse the args. possible other stuff at the end.
	}
#endif 

	NCHAR szBuffer[ MAX_TALK_BUFFER ];
	int iLen = CvtSystemToNUNICODE( szBuffer, COUNTOF(szBuffer), vArgs );
	addBarkUNICODE( szBuffer, NULL, HUE_TEXT_DEF, TALKMODE_SYSTEM, FONT_NORMAL, g_Cfg.m_LangDef );
}

void CClient::addEffect( EFFECT_TYPE motion, ITEMID_TYPE id, const CObjBaseTemplate* pDst, const CObjBaseTemplate* pSrc, BYTE bSpeedSeconds, BYTE bLoop, bool fExplode )
{
	// bSpeedSeconds = seconds = 0=very fast, 7=slow.
	// wHue =

	ASSERT(m_pChar);
	ASSERT( pDst );
	pDst = pDst->GetTopLevelObj();
	CPointMap ptDst = pDst->GetTopPoint();

	CUOCommand cmd;
	cmd.Effect.m_Cmd = XCMD_Effect;
	cmd.Effect.m_motion = motion;
	cmd.Effect.m_id = id;
	cmd.Effect.m_UID = pDst->GetUID();

	CPointMap ptSrc;
	if ( pSrc != NULL && motion == EFFECT_BOLT )
	{
		pSrc = pSrc->GetTopLevelObj();
		ptSrc = pSrc->GetTopPoint();
	}
	else
	{
		ptSrc = ptDst;
	}

	cmd.Effect.m_speed = bSpeedSeconds;		// 22= 0=very fast, 7=slow.
	cmd.Effect.m_loop = bLoop;		// 23= 0 is really long.  1 is the shortest., 6 = longer
	cmd.Effect.m_wHue = 0; // 0x300;		// 24=0 unknown
	cmd.Effect.m_OneDir = true;		// 26=1=point in single dir else turn.
	cmd.Effect.m_explode = fExplode;	// 27=effects that explode on impact.

	cmd.Effect.m_srcx = ptSrc.m_x;
	cmd.Effect.m_srcy = ptSrc.m_y;
	cmd.Effect.m_srcz = ptSrc.m_z;
	cmd.Effect.m_dstx = ptDst.m_x;
	cmd.Effect.m_dsty = ptDst.m_y;
	cmd.Effect.m_dstz = ptDst.m_z;

	switch ( motion )
	{
	case EFFECT_BOLT:	// a targetted bolt
		if ( ! pSrc )
			return;
		cmd.Effect.m_targUID = pDst->GetUID();
		cmd.Effect.m_UID = pSrc->GetUID();	// source
		cmd.Effect.m_OneDir = false;
		cmd.Effect.m_loop = 0;	// Does not apply.
		break;

	case EFFECT_LIGHTNING:	// lightning bolt.
		break;

	case EFFECT_XYZ:	// Stay at current xyz ??? not sure about this.
		break;

	case EFFECT_OBJ:	// effect at single Object.
		break;
	}

	xSendPkt( &cmd, sizeof( cmd.Effect ));
}

void CClient::GetAdjustedItemID( const CChar* pChar, const CItem* pItem, HUE_TYPE & wHue )
{
	// An equipped item.
	ASSERT( pChar );

	if ( m_pChar->IsStatFlag( STATF_Hallucinating ))
	{
		wHue = Calc_GetRandVal( HUE_DYE_HIGH );
	}
	else if ( pChar->IsStatFlag(STATF_Stone))
	{
		wHue = HUE_STONE;
	}
	else
	{
		wHue = pItem->GetHue();
		if (( wHue & HUE_MASK_HI ) > HUE_QTY )
			wHue &= HUE_MASK_LO | HUE_TRANSLUCENT;
		else
			wHue &= HUE_MASK_HI | HUE_TRANSLUCENT;
	}
}

void CClient::GetAdjustedCharID( const CChar* pChar, CREID_TYPE& id, HUE_TYPE& wHue )
{
	// Some clients can't see all creature artwork and colors.
	// try to do the translation here,.
	// id = the artwork id i want to show.

	ASSERT( GetAccount());
	ASSERT( pChar );

	if ( IsPrivFlag(PRIV_DEBUG))
	{
		id = CREID_MAN;
		wHue = HUE_DEFAULT;
		return;
	}

	id = pChar->GetDispID();

	CCharDefPtr pCharDef = pChar->Char_GetDef();

	if ( m_pChar->IsStatFlag( STATF_Hallucinating )) // viewer is Hallucinating.
	{
		if ( pChar != m_pChar )
		{
			// Get a random creature from the artwork.
			id = (CREID_TYPE) Calc_GetRandVal(CREID_EQUIP_GM_ROBE);
			for ( int iTries = 0; iTries < CREID_EQUIP_GM_ROBE; iTries ++ )
			{
				pCharDef = g_Cfg.FindCharDef( id );
				if ( pCharDef )
					break;
				id = (CREID_TYPE) ( id + 1 );
				if ( id >= CREID_EQUIP_GM_ROBE )
					id = (CREID_TYPE) 1;
			}
		}

		wHue = Calc_GetRandVal( HUE_DYE_HIGH );
	}
	else if ( pChar->IsStatFlag(STATF_Stone))	// turned to stone.
	{
		wHue = HUE_STONE;
	}
	else
	{
// WESTY MOD
		if ( ( id != CREID_MAN && id != CREID_WOMAN ) && id == pChar->GetID())
// END WESTY
		{
			//ignore colors for those creatures that have base artwork.
			wHue = HUE_DEFAULT;
		}
		else
		{
			wHue = pChar->GetHue();
		}

		// allow transparency and underwear colors
		if (( wHue & HUE_MASK_HI ) > HUE_QTY )
			wHue &= HUE_MASK_LO | HUE_UNDERWEAR | HUE_TRANSLUCENT;
		else
			wHue &= HUE_MASK_HI | HUE_UNDERWEAR | HUE_TRANSLUCENT;
	}

	// If they do not have correct resource level.
	if ( m_iClientResourceLevel < pCharDef->m_iResourceLevel )
	{
		// RESDISPID
		id = pCharDef->m_ResDispId;

#if 0
		switch ( id )
		{
		case CREID_Tera_Warrior:
		case CREID_Tera_Drone:
		case CREID_Tera_Matriarch:
			id = CREID_GIANT_SNAKE;
			break;
		case CREID_Titan:
		case CREID_Cyclops:
			id = CREID_OGRE;
			break;
		case CREID_Giant_Toad:
		case CREID_Bull_Frog:
			id = CREID_GiantRat;
			break;
		case CREID_Ophid_Mage:
		case CREID_Ophid_Warrior:
		case CREID_Ophid_Queen:
			id = CREID_GIANT_SPIDER;
			break;
		case CREID_SEA_Creature:
			id = CREID_SEA_SERP;
			break;
		case CREID_LavaLizard:
			id = CREID_Alligator;
			break;
		case CREID_Ostard_Desert:
		case CREID_Ostard_Frenz:
		case CREID_Ostard_Forest:
			id = CREID_HORSE1;
			break;
		case CREID_VORTEX:
			id = CREID_AIR_ELEM;
			break;
		}
#endif

	}
}

void CClient::addCharMove( const CChar* pChar )
{
	// This char has just moved on screen.
	// or changed in a subtle way like "hidden"
	// NOTE: If i have been turned this will NOT update myself.

	addPause();

	CUOCommand cmd;
	cmd.CharMove.m_Cmd = XCMD_CharMove;
	cmd.CharMove.m_UID = pChar->GetUID();

	CREID_TYPE id;
	HUE_TYPE wHue;
	GetAdjustedCharID( pChar, id, wHue );
	cmd.CharMove.m_id = id;
	cmd.CharMove.m_wHue = wHue;

	CPointMap pt = pChar->GetTopPoint();
	cmd.CharMove.m_x  = pt.m_x;
	cmd.CharMove.m_y  = pt.m_y;
	cmd.CharMove.m_z = pt.m_z;
	cmd.CharMove.m_dir = pChar->GetDirFlag();
	cmd.CharMove.m_mode = pChar->GetModeFlag( pChar->CanSeeMode( m_pChar ));
	cmd.CharMove.m_noto = pChar->Noto_GetFlag( m_pChar );

	xSendPkt( &cmd, sizeof(cmd.CharMove));
}

void CClient::addChar( const CChar* pChar )
{
	// Full update about a char.
	// ie. someone can see this.

	addPause();
	// if ( pChar == m_pChar ) m_wWalkCount = -1;

	CUOCommand cmd;
	cmd.Char.m_Cmd = XCMD_Char;
	cmd.Char.m_UID = pChar->GetUID();

	CREID_TYPE id;
	HUE_TYPE wHue;
	GetAdjustedCharID( pChar, id, wHue );
	cmd.Char.m_id = id;
	cmd.Char.m_wHue = wHue;

	CPointMap pt = pChar->GetTopPoint();
	pt.GetSector()->SetSectorWakeStatus( pt.m_mapplane );	// if it can be seen then wake it.

	cmd.Char.m_x = pt.m_x;
	cmd.Char.m_y = pt.m_y;
	cmd.Char.m_z = pt.m_z;
	cmd.Char.m_dir = pChar->GetDirFlag();
	cmd.Char.m_mode = pChar->GetModeFlag( pChar->CanSeeMode( m_pChar ));
	cmd.Char.m_noto = pChar->Noto_GetFlag( m_pChar );

	int len = ( sizeof( cmd.Char ) - sizeof( cmd.Char.equip ));
	CUOCommand* pCmd = &cmd;

#if 1 // def _DEBUG
	bool fLayer[LAYER_HORSE+1];
	memset( fLayer, 0, sizeof(fLayer));
	//#endif

	if ( ! pChar->IsStatFlag( STATF_Sleeping ))
	{
		// extend the current struct for all the equipped items.
		for ( CItemPtr pItem=pChar->GetHead(); pItem!=NULL; pItem=pItem->GetNext())
		{
			LAYER_TYPE layer = pItem->GetEquipLayer();
			if ( ! CItemDef::IsVisibleLayer( layer ))
				continue;
			if ( ! m_pChar->CanSeeItem( pItem ) && m_pChar != pChar )
				continue;

			//#ifdef _DEBUG
			// Make certain that no more than one of each layer goes out to client....crashes otherwise!!
			if ( fLayer[layer] )
			{
				DEBUG_ERR(( "'%s' Has multiple items on layer %d, '%s'" LOG_CR, (LPCTSTR) GetName(), layer, (LPCTSTR) pItem->GetName()));
				continue;
			}
			fLayer[layer] = true;
			//#endif

			pCmd->Char.equip[0].m_UID = pItem->GetUID();
			pCmd->Char.equip[0].m_layer = layer;

			HUE_TYPE wHue;
			GetAdjustedItemID( pChar, pItem, wHue );
			if ( wHue )
			{
				pCmd->Char.equip[0].m_id = pItem->GetDispID() | 0x8000;	// include wHue.
				pCmd->Char.equip[0].m_wHue = wHue;
				len += sizeof(pCmd->Char.equip[0]);
				pCmd = (CUOCommand*)(((BYTE*)pCmd)+sizeof(pCmd->Char.equip[0]));
			}
			else
			{
				pCmd->Char.equip[0].m_id = pItem->GetDispID();
				len += sizeof(pCmd->Char.equip[0]) - sizeof(pCmd->Char.equip[0].m_wHue);
				pCmd = (CUOCommand*)(((BYTE*)pCmd)+sizeof(pCmd->Char.equip[0])-sizeof(pCmd->Char.equip[0].m_wHue));
			}
		}
	}
#endif

	pCmd->Char.equip[0].m_UID = 0;	// terminator.
	len += sizeof( DWORD );

	cmd.Char.m_len = len;
	xSendPkt( &cmd, len );
}

void CClient::addItemName( CItem* pItem )
{
	// NOTE: CanSee() has already been called.
	// At one time name length must be < 30 chars. is that still true ?

	ASSERT(pItem);

	bool fIdentified = ( IsPrivFlag(PRIV_GM) || pItem->IsAttr( ATTR_IDENTIFIED ));
	LPCTSTR pszNameFull = (LPCTSTR) pItem->GetNameFull( fIdentified );

	TCHAR szName[ MAX_ITEM_NAME_SIZE + 256 ];
	int len = strcpylen( szName, pszNameFull, sizeof(szName));

	CContainer* pCont = PTR_CAST(CContainer,pItem);
	if ( pCont != NULL )
	{
		// ??? Corpses show hair as an item !!
		len += sprintf( szName+len, " (%d items)", pCont->GetCount());
	}

	// obviously damaged ?
	else if ( pItem->IsTypeArmorWeapon() || pItem->IsType(IT_SHOVEL))
	{
		int iPercent = pItem->Armor_GetRepairPercent();
		if ( iPercent < 50 &&
			( m_pChar->Skill_GetAdjusted( SKILL_ARMSLORE ) / 10 > iPercent ))
		{
			len += sprintf( szName+len, " (%s)", pItem->Armor_GetRepairDesc());
		}
	}

	// Show the priced value
	CItemContainerPtr pMyCont = PTR_CAST( CItemContainer, pItem->GetParent());
	if ( pMyCont && pMyCont->IsType(IT_EQ_VENDOR_BOX))
	{
		CItemVendablePtr pVendItem = PTR_CAST( CItemVendable, pItem );
		if ( pVendItem )
		{
			len += sprintf( szName+len, " (%d gp)",	pVendItem->GetBasePrice());
		}
	}

	HUE_TYPE wHue = HUE_TEXT_DEF;
	CItemCorpsePtr pCorpseItem = PTR_CAST( CItemCorpse, pItem );
	if ( pCorpseItem )
	{
		CCharPtr pCharCorpse = g_World.CharFind(pCorpseItem->m_uidLink);
		if ( pCharCorpse )
		{
			wHue = pCharCorpse->Noto_GetHue( m_pChar, true );
		}
	}

	if ( IsPrivFlag( PRIV_GM ))
	{
		if ( pItem->IsAttr(ATTR_INVIS))
		{
			len += strcpylen( szName+len, " (invis)" );
		}
		if ( pItem->IsAttr(ATTR_STATIC))
		{
			len += strcpylen( szName+len, " (static)" );
		}
		// Show the restock count
		if ( pMyCont != NULL && pMyCont->IsType(IT_EQ_VENDOR_BOX))
		{
			len += sprintf( szName+len, " (%d restock)", pItem->GetContainedLayer());
		}
		switch ( pItem->GetType())
		{
		case IT_ADVANCE_GATE:
			if ( pItem->m_itAdvanceGate.m_Type )
			{
				CCharDefPtr pCharDef = g_Cfg.FindCharDef( pItem->m_itAdvanceGate.m_Type );
				len += sprintf( szName+len, " (%x=%s)", pItem->m_itAdvanceGate.m_Type, (pCharDef==NULL)?"?": pCharDef->GetTradeName());
			}
			break;
		case IT_SPAWN_CHAR:
		case IT_SPAWN_ITEM:
			{
				len += pItem->Spawn_GetName( szName + len );
			}
			break;

		case IT_TREE:
		case IT_GRASS:
		case IT_ROCK:
		case IT_WATER:
			{
				CResourceDefPtr pResDef = g_Cfg.ResourceGetDef( pItem->m_itResource.m_rid_res );
				if ( pResDef)
				{
					len += sprintf( szName+len, " (%s)", (LPCTSTR) pResDef->GetName());
				}
			}
			break;
		}
	}
	if ( IsPrivFlag( PRIV_DEBUG ))
	{
		// Show UID
		len += sprintf( szName+len, " [0%lx]", pItem->GetUID());
	}

	addObjMessage( szName, pItem, wHue );
}

void CClient::addCharName( const CChar* pChar ) // Singleclick text for a character
{
	// Karma wHue codes ?
	ASSERT( pChar );

#if 0 // def _DEBUG

	if ( pChar != m_pChar )
	{
		addContextMenu( CLIMODE_TARG_QTY, NULL, 1, (CChar*) pChar );
		return;
	}

#endif

	HUE_TYPE wHue = pChar->Noto_GetHue( m_pChar, true );

	TCHAR szTemp[ CSTRING_MAX_LEN ];

	strcpy( szTemp, pChar->Noto_GetFameTitle());
	strcat( szTemp, (LPCTSTR) pChar->GetName());

	if ( ! pChar->IsStatFlag( STATF_Incognito ))
	{
		// Guild abbrev.
		LPCTSTR pAbbrev = pChar->Guild_AbbrevAndTitle(MEMORY_TOWN);
		if ( pAbbrev )
		{
			strcat( szTemp, pAbbrev );
		}
		pAbbrev = pChar->Guild_AbbrevAndTitle(MEMORY_GUILD);
		if ( pAbbrev )
		{
			strcat( szTemp, pAbbrev );
		}
	}

	bool fAllShow = IsPrivFlag(PRIV_DEBUG|PRIV_ALLSHOW);

	if ( g_Cfg.m_fCharTags || fAllShow )
	{
		if ( pChar->m_pArea.IsValidRefObj() &&
			pChar->m_pArea->IsFlagGuarded() &&
			pChar->m_pNPC.IsValidNewObj())
		{
			if ( pChar->IsStatFlag( STATF_Pet ))
				strcat( szTemp, " [tame]" );
			else
				strcat( szTemp, " [npc]" );
		}
		if ( pChar->IsStatFlag( STATF_INVUL ))
		{
			bool fPrivHide = ( pChar->IsStatFlag( STATF_Incognito ) || pChar->IsPrivFlag( PRIV_PRIV_HIDE ));
			if ( ! fPrivHide )
				strcat( szTemp, " [invul]" );
		}
		if ( pChar->IsStatFlag( STATF_Stone ))
			strcat( szTemp, " [stone]" );
		else if ( pChar->IsStatFlag( STATF_Freeze ))
			strcat( szTemp, " [frozen]" );
		else if ( pChar->IsStatFlag( STATF_Immobile ))
			strcat( szTemp, " [immobile]" );
		if ( pChar->IsStatFlag( STATF_Insubstantial | STATF_Invisible | STATF_Hidden ))
			strcat( szTemp, " [hidden]" );
		if ( pChar->IsStatFlag( STATF_Sleeping ))
			strcat( szTemp, " [sleeping]" );
		if ( pChar->IsStatFlag( STATF_Hallucinating ))
			strcat( szTemp, " [hallu]" );

		if ( fAllShow )
		{
			if ( pChar->IsStatFlag(STATF_Spawned))
			{
				strcat( szTemp, " [spawn]" );
			}
			if ( IsPrivFlag( PRIV_DEBUG ))
			{
				// Show UID
				sprintf( szTemp+strlen(szTemp), " [0%lx]", pChar->GetUID());
			}
		}
	}
	if ( ! fAllShow && pChar->Skill_GetActive() == NPCACT_Napping )
	{
		strcat( szTemp, " [afk]" );
	}
	if ( pChar->GetPrivLevel() <= PLEVEL_Guest )
	{
		strcat( szTemp, " [guest]" );
	}
	if ( pChar->IsPrivFlag( PRIV_JAILED ))
	{
		strcat( szTemp, " [jailed]" );
	}
	if ( pChar->IsDisconnected())
	{
		strcat( szTemp, " [logout]" );
	}
	if (( fAllShow || pChar == m_pChar ) && pChar->IsStatFlag( STATF_Criminal ))
	{
		strcat( szTemp, " [criminal]" );
	}
	if ( fAllShow || ( IsPrivFlag(PRIV_GM) && ( g_Cfg.m_wDebugFlags & DEBUGF_NPC_EMOTE )))
	{
		strcat( szTemp, " [" );
		strcat( szTemp, (LPCTSTR) pChar->Skill_GetName());
		strcat( szTemp, "]" );
	}

	addObjMessage( szTemp, pChar, wHue );
}

void CClient::addPlayerWalkCancel( void )
{
	// Resync CChar client back to a previous move.
	CUOCommand cmd;
	cmd.WalkCancel.m_Cmd = XCMD_WalkCancel;
	cmd.WalkCancel.m_count = m_wWalkCount;	// sequence #

	CPointMap pt = m_pChar->GetTopPoint();

	cmd.WalkCancel.m_x = pt.m_x;
	cmd.WalkCancel.m_y = pt.m_y;
	cmd.WalkCancel.m_dir = m_pChar->GetDirFlag();
	cmd.WalkCancel.m_z = pt.m_z;
	xSendPkt( &cmd, sizeof( cmd.WalkCancel ));
	m_wWalkCount = -1;
}

void CClient::addPlayerStart( CChar* pChar )
{
	if ( m_pChar != pChar )	// death option just uses this as a reload.
	{
		// This must be a CONTROL command ?
		CharDisconnect();
		if ( pChar->IsClient())	// not sure why this would happen but take care of it anyhow.
		{
			pChar->GetClient()->CharDisconnect();
			ASSERT(!pChar->IsClient());
		}
		m_pChar = pChar;
		m_pChar->ClientAttach( this );
		g_Serv.OnTriggerEvent( SERVTRIG_ClientChange, m_Socket.GetSocket());
	}

	ASSERT( m_pChar->IsClient());
	ASSERT( m_pChar->m_pPlayer );
	DEBUG_CHECK( ! m_pChar->m_pNPC );

	CItemPtr pItemChange = m_pChar->LayerFind(LAYER_FLAG_ClientLinger);
	if ( pItemChange != NULL )
	{
		pItemChange->DeleteThis();
	}

	m_Env.SetInvalid();
	addPause();	// XXX seems to do this. before doing anything else.

	CPointMap pt = m_pChar->GetTopPoint();
	CMulMap* pMulMap = pt.GetMulMap();

	CUOExtData ExtData;
	ExtData.Map_Plane.m_mapplane = pMulMap->GetClientMap();
	addExtData( EXTDATA_Map_Plane, &ExtData, sizeof(ExtData.Map_Plane));

	static const BYTE sm_Pkt_Start1[] =	// no idea what this stuff does.
	"\xff\xff\xff\xff" // "\x00\x00\xFF\x00"
	"\x00\x00"
	"\x00\x00"
	"\x18\x00"	// mode?
	"\x10\xf0"
	"\x00\x00"
	"\x00\x00\x00\x00";

	CUOCommand cmd;
	memcpy( &cmd.Start.m_unk_19, sm_Pkt_Start1, sizeof( sm_Pkt_Start1 ));

	cmd.Start.m_Cmd = XCMD_Start;
	cmd.Start.m_UID = m_pChar->GetUID();
	cmd.Start.m_zero_5 = 0;
	cmd.Start.m_id = m_pChar->GetDispID();
	cmd.Start.m_x = pt.m_x;
	cmd.Start.m_y = pt.m_y;
	cmd.Start.m_z = pt.m_z;
	cmd.Start.m_dir = m_pChar->GetDirFlag();
	cmd.Start.m_zero_18 = 0;

	//cmd.Start.m_mode = 0x0700 | m_pChar->GetModeFlag();
	xSendPkt( &cmd, sizeof( cmd.Start ));

	// unknown stuff.
	static const BYTE sm_Pkt_Unk1[7*4] =	// no idea what this stuff does.
	{
		0x00,0x00,0x00,0x03, // always the same
		0x00,0x00,0x07,0x0f, // ?
		0x00,0x00,0x05,0x7d, // ?
		0x00,0x00,0x38,0x08, // ?
		0x00,0x00,0x38,0x17, // ?	
		0x00,0x00,0x01,0x0c, // always the same
		0x00,0x00,0x01,0x0c, // always the same
	};
	memcpy( ExtData.Unk24.m_data, sm_Pkt_Unk1, sizeof(sm_Pkt_Unk1));
	addExtData( EXTDATA_Unk24, &ExtData, sizeof(sm_Pkt_Unk1));
	addFeatureEnable(3);

	if ( pChar->m_pParty )
	{
		pChar->m_pParty->SendAddList( UID_INDEX_CLEAR, m_pChar );
	}

	ClearTargMode();	// clear death menu mode. etc. ready to walk about. cancel any previos modes.

	addPlayerWarMode();
	m_pChar->MoveToChar( pt );	// Make sure we are in active list.
	m_pChar->Update();
	addOptions();
	addSound( SOUND_CLEAR );
	addWeather( WEATHER_DEFAULT );
	addLight();		// Current light level where I am.
}

void CClient::addPlayerWarMode()
{
	CUOCommand cmd;
	cmd.War.m_Cmd = XCMD_War;
	cmd.War.m_warmode = ( m_pChar->IsStatFlag( STATF_War )) ? 1 : 0;
	cmd.War.m_unk2[0] = 0;
	cmd.War.m_unk2[1] = 0x32;	// ?
	cmd.War.m_unk2[2] = 0;
	xSendPkt( &cmd, sizeof( cmd.War ));
}

void CClient::addToolTip( const CObjBase* pObj, LPCTSTR pszText )
{
	if ( pObj == NULL )
		return;
	if ( pObj->IsChar())
		return; // no tips on chars.

	addPause(); // XXX does this, don't know if needed

	CUOCommand cmd;
	int i = CvtSystemToNUNICODE( cmd.ToolTip.m_utext, MAX_TALK_BUFFER, pszText );
	int len = ((i + 1)* sizeof(NCHAR)) + ( sizeof(cmd.ToolTip) - sizeof(cmd.ToolTip.m_utext));

	cmd.ToolTip.m_Cmd = XCMD_ToolTip;
	cmd.ToolTip.m_len = len;
	cmd.ToolTip.m_UID = pObj->GetUID();

	xSendPkt( &cmd, len );
}

bool CClient::addBookOpen( CItem* pBook )
{
	// word wrap is done when typed in the client. (it has a var size font)
	ASSERT(pBook);
	DEBUG_CHECK( pBook->IsTypeBook());

	int iPagesNow = 0;
	bool fWritable;
	WORD nPages = 0;
	CGString sTitle;
	CGString sAuthor;

	if ( pBook->IsBookSystem())
	{
		fWritable = false;

		CResourceLock s( g_Cfg.ResourceGetDef( pBook->m_itBook.m_ResID ));
		if ( ! s.IsFileOpen())
			return false;

		while ( s.ReadKeyParse())
		{
			switch ( s_FindKeyInTable( s.GetKey(), CItemMessage::sm_Props ))
			{
			case CItemMessage::P_Author:
				sAuthor = s.GetArgStr();
				break;
			case CItemMessage::P_Pages:
				nPages = s.GetArgInt();
				break;
			case CItemMessage::P_Title:
				sTitle = s.GetArgStr();
				break;
			}
		}
		if ( ! sTitle.IsEmpty())
		{
			pBook->SetName( sTitle );	// Make sure the book is named.
		}
	}
	else
	{
		// User written book.
		CItemMessagePtr pMsgItem = PTR_CAST(CItemMessage,pBook);
		if ( pMsgItem == NULL )
			return false;

		fWritable = pMsgItem->IsBookWritable() ? true : false;	// Not sealed
		nPages = fWritable ? ( MAX_BOOK_PAGES ) : ( pMsgItem->GetPageCount());	// Max pages.
		sTitle = pMsgItem->GetName();
		sAuthor = (pMsgItem->m_sAuthor.IsEmpty()) ? "unknown" : (LPCTSTR)( pMsgItem->m_sAuthor );

		if ( fWritable )	// For some reason we must send them now.
		{
			iPagesNow = pMsgItem->GetPageCount();
		}
	}

	CUOCommand cmd;
	if ( m_ProtoVer.GetCryptVer() >= 0x126000 )
	{
		cmd.BookOpen_v26.m_Cmd = XCMD_BookOpen;
		cmd.BookOpen_v26.m_UID = pBook->GetUID();
		cmd.BookOpen_v26.m_writable = fWritable ? 1 : 0;
		cmd.BookOpen_v26.m_NEWunk1 = fWritable ? 1 : 0;
		cmd.BookOpen_v26.m_pages = nPages;
		strcpy( cmd.BookOpen_v26.m_title, sTitle );
		strcpy( cmd.BookOpen_v26.m_author, sAuthor );
		xSendPkt( &cmd, sizeof( cmd.BookOpen_v26 ));
	}
	else
	{
		cmd.BookOpen_v25.m_Cmd = XCMD_BookOpen;
		cmd.BookOpen_v25.m_UID = pBook->GetUID();
		cmd.BookOpen_v25.m_writable = fWritable ? 1 : 0;
		cmd.BookOpen_v25.m_pages = nPages;
		strcpy( cmd.BookOpen_v25.m_title, sTitle );
		strcpy( cmd.BookOpen_v25.m_author, sAuthor );
		xSendPkt( &cmd, sizeof( cmd.BookOpen_v25 ));
	}

	// We could just send all the pages now if we want.
	if ( iPagesNow )
	{
		if ( iPagesNow>nPages )
			iPagesNow=nPages;
		for ( int i=0; i<iPagesNow; i++ )
		{
			addBookPage( pBook, i+1 );
		}
	}

	return( true );
}

void CClient::addBookPage( const CItem* pBook, int iPage )
{
	// ARGS:
	//  iPage = 1 based page.
	DEBUG_CHECK( iPage > 0 );
	if ( iPage <= 0 )
		return;

	CUOCommand cmd;
	cmd.BookPage.m_Cmd = XCMD_BookPage;
	cmd.BookPage.m_UID = pBook->GetUID();
	cmd.BookPage.m_pages = 1;	// we can send multiple pages if we wish.
	cmd.BookPage.m_page[0].m_pagenum = iPage;	// 1 based page.

	int lines=0;
	int length=0;

	if ( pBook->IsBookSystem())
	{
		CResourceLock s( g_Cfg.ResourceGetDef( CSphereUID( RES_Book, pBook->m_itBook.m_ResID.GetResIndex(), iPage )));
		if ( ! s.IsFileOpen())
			return;

		// NOTE: Includes new lines. (??? cr as well ?)
		while (s.ReadLine(false))
		{
			lines++;
			length += strcpylen( cmd.BookPage.m_page[0].m_text+length, s.GetKey()) + 1;
		}
	}
	else
	{
		// User written book pages.
		const CItemMessage* pMsgItem = PTR_CAST(const CItemMessage,pBook);
		if ( pMsgItem == NULL )
			return;
		iPage --;
		if ( iPage < pMsgItem->GetPageCount())
		{
			// Copy the pages to the book
			LPCTSTR pszText = pMsgItem->GetPageText(iPage);
			if ( pszText )
			{
				for(;;)
				{
					TCHAR ch = pszText[ length ];
					if ( ch == '\t' )
					{
						ch = '\0';
						lines++;
					}
					cmd.BookPage.m_page[0].m_text[ length ] = ch;
					if ( pszText[ length ++ ] == '\0' )
					{
						if ( length > 1 ) lines ++;
						break;
					}
				}
			}
		}
	}

	length += sizeof( cmd.BookPage ) - sizeof( cmd.BookPage.m_page[0].m_text );
	cmd.BookPage.m_len = length;
	cmd.BookPage.m_page[0].m_lines = lines;

	xSendPkt( &cmd, length );
}

int CClient::Setup_FillCharList( CUOEventCharDef* pCharList, const CChar* pCharFirst )
{
	// list available chars for your account that are idle.
	CAccountPtr pAccount = GetAccount();
	ASSERT( pAccount );
	int j=0;

	if ( pCharFirst && pCharFirst->GetAccount() == pAccount )
	{
		m_Targ.m_tmSetupCharList[0] = pCharFirst->GetUID();
		int iLen = strcpylen( pCharList[0].m_charname, pCharFirst->GetName(), sizeof( pCharList[0].m_charname ));
		memset( pCharList[0].m_charname+iLen, 0, sizeof(pCharList[0].m_charname)-iLen );
		memset( pCharList[0].m_charpass, 0, sizeof(pCharList[0].m_charpass));
		j++;
	}

	int iQty = pAccount->m_Chars.GetSize();
	for ( int k=0; k<iQty; k++ )
	{
		CSphereUID uid( pAccount->m_Chars.GetAt(k));
		CCharPtr pChar = g_World.CharFind(uid);
		if ( pChar == NULL )
			continue;
		DEBUG_CHECK( pChar->GetAccount() == pAccount );
		if ( pCharFirst == pChar )
			continue;
		if ( j >= UO_MAX_CHARS_PER_ACCT )
			break;

		// if ( j >= g_Cfg.m_iMaxCharsPerAccount && ! IsPrivFlag(PRIV_GM)) break;

		m_Targ.m_tmSetupCharList[j] = uid;
		int iLen = strcpylen( pCharList[j].m_charname, pChar->GetName(), sizeof( pCharList[0].m_charname ));
		memset( pCharList[j].m_charname+iLen, 0, sizeof(pCharList[0].m_charname)-iLen );
		memset( pCharList[j].m_charpass, 0, sizeof(pCharList[0].m_charpass));
		j++;
	}

	// always show max count for some stupid reason. (client bug)
	// pad out the rest of the chars.
	for ( ;j<UO_MAX_CHARS_PER_ACCT;j++)
	{
		m_Targ.m_tmSetupCharList[j].InitUID();
		memset( pCharList[j].m_charname, 0, sizeof(pCharList[0].m_charname));
		memset( pCharList[j].m_charpass, 0, sizeof(pCharList[0].m_charpass));
	}

	if ( m_ProtoVer.GetCryptVer() >= 0x00300001 )
		return iQty;
	return( UO_MAX_CHARS_PER_ACCT );
}

void CClient::addCharList3()
{
	// just pop up a list of chars for this account.
	// GM's may switch chars on the fly.

	ASSERT( GetAccount());

	CUOCommand cmd;
	cmd.CharList3.m_Cmd = XCMD_CharList3;
	cmd.CharList3.m_len = sizeof( cmd.CharList3 );
	cmd.CharList3.m_count = Setup_FillCharList( cmd.CharList3.m_char, m_pChar );
	cmd.CharList3.m_unk = 0;

	xSendPkt( &cmd, sizeof( cmd.CharList3 ));

	CharDisconnect();	// since there is no undoing this in the client.
	SetTargMode( CLIMODE_SETUP_CHARLIST );
}

void CClient::SetTargMode( CLIMODE_TYPE targmode, LPCTSTR pPrompt )
{
	// Keep track of what mode the client should be in.
	// ??? Get rid of menu stuff if previous targ mode.
	// Can i close a menu ?
	// Cancel a cursor input.

	if ( GetTargMode() == targmode )
		return;

	if ( GetTargMode() != CLIMODE_NORMAL && targmode != CLIMODE_NORMAL )
	{
		// Just clear the old target mode
		WriteString( "Previous Targeting Cancelled" );
	}

	m_Targ.m_Mode = targmode;
	WriteString( (targmode == CLIMODE_NORMAL ) ? "Targeting Cancelled" : pPrompt );
}

void CClient::addPromptConsole( CLIMODE_TYPE targmode, LPCTSTR pPrompt )
{
	// Prompt client for string return.
	// NOTE: This should result in Event_PromptResp()
	SetTargMode( targmode, pPrompt );

	CUOCommand cmd;
	cmd.Prompt.m_Cmd = XCMD_Prompt;
	cmd.Prompt.m_len = sizeof( cmd.Prompt );
	memset( cmd.Prompt.m_unk3, 0, sizeof(cmd.Prompt.m_unk3));
	cmd.Prompt.m_text[0] = '\0';

	xSendPkt( &cmd, cmd.Prompt.m_len );
}

bool CClient::addTarget( CLIMODE_TYPE targmode, LPCTSTR pPrompt, bool fAllowGround, bool fCheckCrime ) // Send targeting cursor to client
{
	// Give client a mouse targeting cursor.
	// will this be selective for us ? objects only or chars only ? not on the ground (statics) ?
	// NOTE: 
	//  Expect XCMD_Target back.
	//  This should result in Event_Target()

	if ( m_pChar == NULL )
		return false;

	SetTargMode( targmode, pPrompt );

	CUOCommand cmd;
	memset( &(cmd.Target), 0, sizeof( cmd.Target ));
	cmd.Target.m_Cmd = XCMD_Target;
	cmd.Target.m_TargType = fAllowGround; // fAllowGround;	// 1=allow xyz, 0=objects only.
	cmd.Target.m_context = targmode ;	// my id code for action.
	cmd.Target.m_fCheckCrime = fCheckCrime;

	xSendPkt( &cmd, sizeof( cmd.Target ));
	return true;
}

void CClient::addTargetDeed( const CItem* pDeed )
{
	// Place an item from a deed. preview all the stuff

	ASSERT( m_Targ.m_UID == pDeed->GetUID());
	ITEMID_TYPE iddef = (ITEMID_TYPE) RES_GET_INDEX( pDeed->m_itDeed.m_Type );
	m_Targ.m_tmUseItem.m_pParent = pDeed->GetParent();	// Cheat Verify.
	addTargetItems( CLIMODE_TARG_USE_ITEM, iddef );
}

bool CClient::addTargetSummon( CLIMODE_TYPE mode, CREID_TYPE baseID, bool fNotoCheck )
{
	CCharDefPtr pBase = g_Cfg.FindCharDef( baseID );
	if ( pBase == NULL )
		return( false );

	CGString sPrompt;
	sPrompt.Format( "Where would you like to summon the '%s'?", (LPCTSTR) pBase->GetTradeName());

	return addTarget( mode, sPrompt, true, fNotoCheck );
}

bool CClient::addTargetItems( CLIMODE_TYPE targmode, ITEMID_TYPE id )
{
	// Add a list of items to place at target.
	// preview all the stuff (if possible)

	ASSERT(m_pChar);

	CString sName;
	CItemDefPtr pItemDef;
	if ( id < ITEMID_TEMPLATE )
	{
		pItemDef = g_Cfg.FindItemDef( id );
		if ( pItemDef == NULL )
			return false;

		sName = pItemDef->GetName();

		if ( pItemDef->IsType(IT_STONE_GUILD))
		{
			// Check if they are already in a guild first
			CItemStonePtr pStone = m_pChar->Guild_Find(MEMORY_GUILD);
			if (pStone)
			{
				WriteString( "You are already a member of a guild. Resign first!");
				return false;
			}
		}
	}
	else
	{
		pItemDef = NULL;
		sName = "template";
	}

	CGString sPrompt;
	sPrompt.Format( "Where would you like to place the %s?", (LPCSTR) sName );

	if ( CItemDef::IsID_Multi( id ))	// a multi we get from Multi.mul
	{
		SetTargMode( targmode, sPrompt );

		CUOCommand cmd;
		cmd.TargetMulti.m_Cmd = XCMD_TargetMulti;
		cmd.TargetMulti.m_fAllowGround = 1;
		cmd.TargetMulti.m_context = targmode ;	// 5=my id code for action.
		memset( cmd.TargetMulti.m_zero6, 0, sizeof(cmd.TargetMulti.m_zero6));
		cmd.TargetMulti.m_id = id - ITEMID_MULTI;

		// Add any extra stuff attached to the multi. preview this.

		memset( cmd.TargetMulti.m_zero20, 0, sizeof(cmd.TargetMulti.m_zero20));

		xSendPkt( &cmd, sizeof( cmd.TargetMulti ));
		return true;
	}

	if ( ! m_ProtoVer.GetCryptVer() ||
		m_ProtoVer.GetCryptVer() >= 0x126030 )
	{
		// preview not supported by this ver?
		addTarget( targmode, sPrompt, true );
		return true;
	}

	CUOCommand cmd;
	cmd.TargetItems.m_Cmd = XCMD_TargetItems;
	cmd.TargetItems.m_fAllowGround = 1;
	cmd.TargetItems.m_code = targmode;
	cmd.TargetItems.m_xOffset = 0;
	cmd.TargetItems.m_yOffset = 0;
	cmd.TargetItems.m_zOffset = 0;

	int iCount;
	CItemDefMultiPtr pBaseMulti = REF_CAST(CItemDefMulti,pItemDef);
	if ( pBaseMulti )
	{
		iCount = pBaseMulti->m_Components.GetSize();
		for ( int i=0; i<iCount; i++ )
		{
			ASSERT( pBaseMulti->m_Components[i].GetDispID() < ITEMID_MULTI );
			cmd.TargetItems.m_item[i].m_id = pBaseMulti->m_Components[i].m_wTileID;
			cmd.TargetItems.m_item[i].m_dx = pBaseMulti->m_Components[i].m_dx;
			cmd.TargetItems.m_item[i].m_dy = pBaseMulti->m_Components[i].m_dy;
			cmd.TargetItems.m_item[i].m_dz = pBaseMulti->m_Components[i].m_dz;
			cmd.TargetItems.m_item[i].m_unk = 0;
		}
	}
	else
	{
		iCount = 1;
		cmd.TargetItems.m_item[0].m_id = id;
		cmd.TargetItems.m_item[0].m_dx = 0;
		cmd.TargetItems.m_item[0].m_dy = 0;
		cmd.TargetItems.m_item[0].m_dz = 0;
		cmd.TargetItems.m_item[0].m_unk = 0;
	}

	cmd.TargetItems.m_count = iCount;
	int len = ( sizeof(cmd.TargetItems) - sizeof(cmd.TargetItems.m_item)) + ( iCount* sizeof(cmd.TargetItems.m_item[0]));
	cmd.TargetItems.m_len = len;

	SetTargMode( targmode, sPrompt );
	xSendPkt( &cmd, len);

	return( true );
}

void CClient::addDyeOption( const CObjBase* pObj )
{
	// Put up the wHue chart for the client.
	// This results in a Event_Item_Dye message. CLIMODE_DYE
	ITEMID_TYPE id;
	if ( pObj->IsItem())
	{
		CItemPtr pItem = PTR_CAST(CItem,const_cast <CObjBase*>(pObj));
		ASSERT(pItem);
		id = pItem->GetDispID();
	}
	else
	{
		// Get the item equiv for the creature.
		CCharPtr pChar = PTR_CAST(CChar,const_cast <CObjBase*>(pObj));
		ASSERT(pChar);
		id = pChar->Char_GetDef()->m_trackID;
	}

	CUOCommand cmd;
	cmd.DyeVat.m_Cmd = XCMD_DyeVat;
	cmd.DyeVat.m_UID = pObj->GetUID();
	cmd.DyeVat.m_HueDef = pObj->GetHue();
	cmd.DyeVat.m_id = id;
	xSendPkt( &cmd, sizeof( cmd.DyeVat ));

	m_Targ.m_UID = pObj->GetUID();
	SetTargMode( CLIMODE_DYE );

	// should respond with XCMD_DyeVat
}

void CClient::addSkillWindow( CChar* pChar, SKILL_TYPE skill, bool fExtra ) // Opens the skills list
{
	if ( fExtra )
	{
		CSphereExpArgs exec(pChar, m_pChar, CLIENT_BUTTON_SKILLS );
		pChar->OnTrigger( CCharDef::T_UserButton, exec);
	}

	CUOCommand cmd;
	cmd.Skill.m_Cmd = XCMD_Skill;

	bool fVer12602 = (! m_ProtoVer.GetCryptVer() || m_ProtoVer.GetCryptVer() >= 0x126020 );

	int len = ( fVer12602 ) ? sizeof(cmd.Skill) : sizeof(cmd.Skill_v261);

	// Whos skills do we want to look at ?
	if ( skill >= SKILL_QTY )
	{	// all skills
		cmd.Skill.m_single = 0;
		int i=SKILL_First;
		for ( ; i<SKILL_QTY; i++ )
		{
			int iskillval = pChar->Skill_GetBase( (SKILL_TYPE) i);
			if ( fVer12602 )
			{
				cmd.Skill.skills[i].m_index = i+1;
				cmd.Skill.skills[i].m_val = pChar->Skill_GetAdjusted( (SKILL_TYPE) i);
				cmd.Skill.skills[i].m_valraw = iskillval;
				cmd.Skill.skills[i].m_lock = pChar->Skill_GetLock( (SKILL_TYPE) i);
			}
			else
			{
				cmd.Skill_v261.skills[i].m_index = i+1;
				cmd.Skill_v261.skills[i].m_val = iskillval;
			}
		}
		if ( fVer12602 )
		{
			cmd.Skill.skills[i].m_index = 0;	// terminator.
			len += ((SKILL_QTY-1)* sizeof(cmd.Skill.skills[0])) + sizeof(NWORD);
		}
		else
		{
			cmd.Skill_v261.skills[i].m_index = 0;	// terminator.
			len += ((SKILL_QTY-1)* sizeof(cmd.Skill_v261.skills[0])) + sizeof(NWORD);
		}
	}
	else
	{	// Just one skill update.
		cmd.Skill.m_single = 0xff;
		int iskillval = pChar->Skill_GetBase( skill );
		if ( fVer12602 )
		{
			cmd.Skill.skills[0].m_index = skill;
			cmd.Skill.skills[0].m_val = pChar->Skill_GetAdjusted( skill );
			cmd.Skill.skills[0].m_valraw = iskillval;
			cmd.Skill.skills[0].m_lock = pChar->Skill_GetLock( skill );
		}
		else
		{
			cmd.Skill_v261.skills[0].m_index = skill;
			cmd.Skill_v261.skills[0].m_val = iskillval;
		}
	}

	cmd.Skill.m_len = len;
	xSendPkt( &cmd, len );
}

void CClient::addPlayerSee( const CPointMap& ptold )
{
	// adjust to my new location.
	// What do I now see here ?

	// What new things do i see (on the ground) ?
	CWorldSearch AreaItems( m_pChar->GetTopPoint(), SPHEREMAP_VIEW_RADAR );
	AreaItems.SetFilterAllShow( IsPrivFlag( PRIV_ALLSHOW ));
	for(;;)
	{
		CItemPtr pItem = AreaItems.GetNextItem();
		if ( pItem == NULL )
			break;
		if ( ! CanSee( pItem ))
			continue;

		// I didn't see it before
		if ( ptold.GetDist( pItem->GetTopPoint()) > SPHEREMAP_VIEW_SIZE )
		{
			addItem_OnGround( pItem );
		}
	}

	// What new people do i see ?
	CWorldSearch AreaChars( m_pChar->GetTopPoint(), SPHEREMAP_VIEW_SIZE );
	AreaChars.SetFilterAllShow( IsPrivFlag( PRIV_ALLSHOW ));	// show logged out chars?
	for(;;)
	{
		CCharPtr pChar = AreaChars.GetNextChar();
		if ( pChar == NULL )
			break;
		if ( m_pChar == pChar )
			continue;	// I saw myself before.
		if ( ! CanSee( pChar ))
			continue;

		if ( ptold.GetDist( pChar->GetTopPoint()) > SPHEREMAP_VIEW_SIZE )
		{
			addChar( pChar );
		}
	}
}

void CClient::addPlayerView( const CPointMap & ptold )
{
	// I moved = Change my point of view. Teleport etc..

	addPause();

	CPointMap pt = m_pChar->GetTopPoint();
	CMulMap* pMulMap = pt.GetMulMap();

	CUOExtData ExtData;
	ExtData.Map_Plane.m_mapplane = pMulMap->GetClientMap();
	addExtData( EXTDATA_Map_Plane, &ExtData, sizeof(ExtData.Map_Plane));

	CUOCommand cmd;
	cmd.View.m_Cmd = XCMD_View;
	cmd.View.m_UID = m_pChar->GetUID();

	CREID_TYPE id;
	HUE_TYPE wHue;
	GetAdjustedCharID( m_pChar, id, wHue );
	cmd.View.m_id = id;
	cmd.View.m_zero7 = 0;
	cmd.View.m_wHue = wHue;
	cmd.View.m_mode = m_pChar->GetModeFlag();

	cmd.View.m_x = pt.m_x;
	cmd.View.m_y = pt.m_y;
	cmd.View.m_zero15 = 0;
	cmd.View.m_dir = m_pChar->GetDirFlag();
	cmd.View.m_z = pt.m_z;
	xSendPkt( &cmd, sizeof( cmd.View ));

	// resync this stuff.
	m_wWalkCount = -1;

	if ( ptold == pt )
		return;	// not a real move i guess. might just have been a change in face dir.

	m_Env.SetInvalid();	// Must resend environ stuff.

	// What can i see here ?
	addPlayerSee( ptold );
}

void CClient::addReSync()
{
	// Reloads the client with all it needs.
	CPointMap ptold;	// invalid value.
	addPlayerView( ptold );
	addChar( m_pChar );
	addLight();		// Current light level where I am.
}

bool CClient::addCharStatWindow( CSphereUID uid, bool fExtra ) // Opens the status window
{
	// ARGS:
	//   fExtra = send me extra info about myself.

	CCharPtr pChar = g_World.CharFind(uid);
	if ( pChar == NULL )
		return false;

	if ( fExtra )
	{
		CSphereExpArgs exec(pChar, m_pChar, CLIENT_BUTTON_STATS );
		pChar->OnTrigger( CCharDef::T_UserButton, exec);
	}

	CUOCommand cmd;
	cmd.Status.m_Cmd = XCMD_Status;
	cmd.Status.m_UID = pChar->GetUID();

	// WARNING: Char names must be <= 30 !!!
	// "kellen the magincia counsel me" > 30 char names !!
	strcpylen( cmd.Status.m_charname, (LPCTSTR) pChar->GetName(), sizeof( cmd.Status.m_charname ));

	// renamable ?
	if ( m_pChar != pChar &&	// can't rename self. it looks weird.
		pChar->NPC_IsOwnedBy( m_pChar ) && // my pet.
		! pChar->Char_GetDef()->m_TagDefs.FindKeyInt( "HIREDAYWAGE" ))	// can't rename hirelings
	{
		cmd.Status.m_perm = 0xFF;	// I can rename them
	}
	else
	{
		cmd.Status.m_perm = 0x00;
	}

	// Dont bother sending the rest of this if not my self.
	cmd.Status.m_ValidStats = 0;

	if ( pChar == m_pChar )
	{
		m_fUpdateStats = false;

		cmd.Status.m_health = pChar->Stat_Get(STAT_Health);
		cmd.Status.m_maxhealth = pChar->m_StatMaxHealth;

		cmd.Status.m_len = sizeof( cmd.Status );
		cmd.Status.m_ValidStats |= 1;	// valid stats
		cmd.Status.m_sex = ( pChar->Char_GetDef()->IsFemale()) ? 1 : 0;
		cmd.Status.m_str = pChar->Stat_Get(STAT_Str);
		cmd.Status.m_dex = pChar->Stat_Get(STAT_Dex);
		cmd.Status.m_int = pChar->Stat_Get(STAT_Int);
		cmd.Status.m_stam =	pChar->Stat_Get(STAT_Stam);
		cmd.Status.m_maxstam = pChar->m_StatMaxStam;
		cmd.Status.m_mana =	pChar->Stat_Get(STAT_Mana);
		cmd.Status.m_maxmana = pChar->m_StatMaxMana;
		cmd.Status.m_gold = pChar->ContentCount( CSphereUID(RES_TypeDef,IT_GOLD));	/// ??? optimize this count is too often.
		cmd.Status.m_armor = pChar->m_ArmorDisplay;
		cmd.Status.m_weight = pChar->GetTotalWeight() / WEIGHT_UNITS;
	}
	else
	{
		cmd.Status.m_health = pChar->GetHealthPercent();
		cmd.Status.m_maxhealth = 100;
		cmd.Status.m_len = ((BYTE*)&(cmd.Status.m_sex)) - ((BYTE*)&(cmd.Status));
	}
	xSendPkt( &cmd, cmd.Status.m_len );
	return true;
}

void CClient::addSpellbookOpen( CItem* pBook )
{
	// NOTE: if the spellbook item is not present on the client it will crash.
	// count what spells I have.

	if ( pBook->GetDispID() == ITEMID_SPELLBOOK2 )
	{
		// weird client bug.
		pBook->SetDispID( ITEMID_SPELLBOOK );
		pBook->Update();
		return;
	}

	int count=0;
	int i=SPELL_Clumsy;
	for ( ;i<SPELL_BOOK_QTY;i++ )
	{
		if ( pBook->IsSpellInBook( (SPELL_TYPE) i ))
		{
			count++;
		}
	}

	addPause();	// try to get rid of the double open.
	if ( count > 0 )
	{
		CUOCommand cmd;
		int len = sizeof( cmd.Content ) - sizeof(cmd.Content.m_item) + ( count* sizeof(cmd.Content.m_item[0]));
		cmd.Content.m_Cmd = XCMD_Content;
		cmd.Content.m_len = len;
		cmd.Content.m_count = count;

		int j=0;
		for ( i=SPELL_Clumsy; i<SPELL_BOOK_QTY; i++ )
			if ( pBook->IsSpellInBook( (SPELL_TYPE) i ))
		{
			CSpellDefPtr pSpellDef = g_Cfg.GetSpellDef( (SPELL_TYPE) i );
			ASSERT(pSpellDef);

			cmd.Content.m_item[j].m_UID = UID_F_ITEM + UID_INDEX_FREE + i; // just some unused uid.
			cmd.Content.m_item[j].m_id = pSpellDef->m_idScroll;	// scroll id. (0x1F2E)
			cmd.Content.m_item[j].m_zero6 = 0;
			cmd.Content.m_item[j].m_amount = i;
			cmd.Content.m_item[j].m_x = 0x48;	// may not mean anything ?
			cmd.Content.m_item[j].m_y = 0x7D;
			cmd.Content.m_item[j].m_UIDCont = pBook->GetUID();
			cmd.Content.m_item[j].m_wHue = HUE_DEFAULT;
			j++;
		}

		xSendPkt( &cmd, len );
	}

	addOpenGump( pBook, GUMP_OPEN_SPELLBOOK );
}

#if 0
void CClient::addScrollText( LPCTSTR pszText )
{
	// ??? We need to do word wrap here. But it has a var size font !
	CUOCommand cmd;
	cmd.Scroll.m_Cmd = XCMD_Scroll;
	cmd.Scroll.m_type = SCROLL_TYPE_NOTICE;
	cmd.Scroll.m_scrollID = 0;

	int length = strcpylen( cmd.Scroll.m_text, pszText );
	cmd.Scroll.m_lentext = length;
	length += sizeof( cmd.Scroll ) - sizeof( cmd.Scroll.m_text );
	cmd.Scroll.m_len = length;

	xSendPkt( &cmd, length );
}
#endif

void CClient::addScrollScript( CResourceLock& s, SCROLL_TYPE type, DWORD context, LPCTSTR pszHeader )
{
	// RES_Scroll or RES_Help
	CUOCommand cmd;
	cmd.Scroll.m_Cmd = XCMD_Scroll;
	cmd.Scroll.m_type = type;
	cmd.Scroll.m_context = context;	// for ScrollClose ?

	int length=0;

	if ( pszHeader )
	{
		length = strcpylen( &cmd.Scroll.m_text[0], pszHeader );
		cmd.Scroll.m_text[length++] = '\r';	// 0x0d
		length += strcpylen( &cmd.Scroll.m_text[length], "  " );
		cmd.Scroll.m_text[length++] = '\r';	// 0x0d
	}

	while (s.ReadLine(false))
	{
		if ( ! _strnicmp( s.GetKey(), ".line", 5 ))	// just a blank line.
		{
			length += strcpylen( &cmd.Scroll.m_text[length], " " );
		}
		else
		{
			length += strcpylen( &cmd.Scroll.m_text[length], s.GetKey());
		}
		cmd.Scroll.m_text[length++] = '\r';	// 0x0d
	}

	cmd.Scroll.m_lentext = length;
	length += sizeof( cmd.Scroll ) - sizeof( cmd.Scroll.m_text );
	cmd.Scroll.m_len = length;

	xSendPkt( &cmd, length );
}

void CClient::addScrollResource( LPCTSTR pszSec, SCROLL_TYPE type, DWORD scrollID )
{
	// SCROLL_TYPE
	// type = 0 = TIPS
	// type = 2 = UPDATES

	CResourceLock s( g_Cfg.ResourceGetDefByName( RES_Scroll, pszSec ));
	if ( ! s.IsFileOpen())
		return;
	addScrollScript( s, type, scrollID );
}

void CClient::addVendorClose( const CChar* pVendor )
{
	// Clear the vendor display.
	CUOCommand cmd;
	cmd.VendorBuy.m_Cmd = XCMD_VendorBuy;
	cmd.VendorBuy.m_len = sizeof( cmd.VendorBuy );
	cmd.VendorBuy.m_UIDVendor = pVendor->GetUID();
	cmd.VendorBuy.m_flag = 0;	// 0x00 = no items following, 0x02 - items following
	xSendPkt( &cmd, sizeof( cmd.VendorBuy ));
}

int CClient::addShopItems( CChar* pVendor, LAYER_TYPE layer )
{
	// Player buying from vendor.
	// Show the Buy menu for the contents of this container
	// RETURN: the number of items in container.
	//   < 0 = error.
	CItemContainerPtr pContainer = pVendor->GetBank( layer );
	if ( pContainer == NULL )
		return( -1 );

	addItem( pContainer );
	addContents( pContainer, false, false, true );

	// add the names and prices for stuff.
	CUOCommand cmd;
	cmd.VendOpenBuy.m_Cmd = XCMD_VendOpenBuy;
	cmd.VendOpenBuy.m_VendorUID = pContainer->GetUID();
	int len = sizeof(cmd.VendOpenBuy) - sizeof(cmd.VendOpenBuy.m_item);

	int iConvertFactor = pVendor->NPC_GetVendorMarkup(m_pChar);

	CUOCommand* pCur = &cmd;
	int count = 0;
	for ( CItemPtr pItem = pContainer->GetHead(); pItem!=NULL; pItem = pItem->GetNext())
	{
		if ( ! pItem->GetAmount())
			continue;

		CItemVendablePtr pVendItem = REF_CAST(CItemVendable,pItem);
		if ( pVendItem == NULL )
			continue;
		long lPrice = pVendItem->GetVendorPrice(iConvertFactor);
		if ( ! lPrice )
		{
			pVendItem->IsValidNPCSaleItem();
			// This stuff will show up anyhow. unpriced.
			// continue;
			lPrice = 100000;
		}

		pCur->VendOpenBuy.m_item[0].m_price = lPrice;
		int lenname = strcpylen( pCur->VendOpenBuy.m_item[0].m_text, (LPCTSTR) pVendItem->GetName());
		pCur->VendOpenBuy.m_item[0].m_len = lenname + 1;
		lenname += sizeof( cmd.VendOpenBuy.m_item[0] );
		len += lenname;
		pCur = (CUOCommand *)( ((BYTE*) pCur ) + lenname );
		if ( ++count >= UO_MAX_ITEMS_CONT )
			break;
	}

	// if ( ! count ) return( 0 );

	cmd.VendOpenBuy.m_len = len;
	cmd.VendOpenBuy.m_count = count;
	xSendPkt( &cmd, len );
	return( count );
}

bool CClient::addShopMenuBuy( CChar* pVendor )
{
	// Try to buy stuff that the vendor has.
	ASSERT( pVendor );
	if ( ! pVendor->NPC_IsVendor())
		return( false );

	addPause();
	addChar( pVendor );			// Send the NPC again to make sure info is current. (XXX does this we might not have to)

	int iTotal = 0;
	int iRes = addShopItems( pVendor, LAYER_VENDOR_STOCK );
	if ( iRes < 0 )
		return( false );
	iTotal = iRes;
	iRes = addShopItems( pVendor, LAYER_VENDOR_EXTRA );
	if ( iRes < 0 )
		return( false );
	iTotal += iRes;
	if ( iTotal <= 0 )
		return( false );

	addOpenGump( pVendor, GUMP_VENDOR_RECT );
	addCharStatWindow( m_pChar->GetUID(), false );	// Make sure the gold total has been updated.
	return( true );
}

int CClient::addShopMenuSellFind( CItemContainer* pSearch, CItemContainer* pVendBuys, CItemContainer* pVend2, int iConvertFactor, CUOCommand* & pCur )
{
	// What things do you have in your inventory that the vendor would want ?
	// Search sub containers if necessary.
	// RETURN: How many items did we find.

	ASSERT(pSearch);
	if ( pVendBuys == NULL || pVend2 == NULL )
		return( 0 );

	int iCount = 0;

	for ( CItemPtr pItem = pSearch->GetHead() ; pItem!=NULL; pItem = pItem->GetNext())
	{
		// We won't buy containers with items in it.
		// But we will look in them.
		CItemContainerPtr pContItem = REF_CAST( CItemContainer, pItem );
		if ( pContItem != NULL && pContItem->GetCount())
		{
			if ( pContItem->IsSearchable())
			{
				iCount += addShopMenuSellFind( pContItem, pVendBuys, pVend2, iConvertFactor, pCur );
			}
			continue;
		}

		CItemVendablePtr pVendItem = REF_CAST( CItemVendable, pItem);
		if ( pVendItem == NULL )
			continue;

		CItemVendablePtr pItemSample = CChar::NPC_FindVendableItem( pVendItem, pVendBuys, pVend2 );
		if ( pItemSample == NULL )
			continue;

		// I already have enough of these ?
		if ( pItemSample->GetParent() == pVendBuys )
		{
			if ( pItemSample->GetContainedLayer() >= pItemSample->GetAmount())
				continue;
		}
		else
		{
			if ( pItemSample->GetAmount() >= pItemSample->GetContainedLayer()*2 )
				continue;
		}

		pCur->VendOpenSell.m_item[0].m_UID = pVendItem->GetUID();
		pCur->VendOpenSell.m_item[0].m_id = pVendItem->GetDispID();

		HUE_TYPE wHue = pVendItem->GetHue() & HUE_MASK_HI;
		if ( wHue > HUE_QTY )
			wHue &= HUE_MASK_LO;

		pCur->VendOpenSell.m_item[0].m_wHue = wHue;
		pCur->VendOpenSell.m_item[0].m_amount = pVendItem->GetAmount();
		pCur->VendOpenSell.m_item[0].m_price = pItemSample->GetVendorPrice(iConvertFactor);

		int lenname = strcpylen( pCur->VendOpenSell.m_item[0].m_text, (LPCTSTR) pVendItem->GetName());
		pCur->VendOpenSell.m_item[0].m_len = lenname + 1;
		pCur = (CUOCommand *)( ((BYTE*) pCur ) + lenname + sizeof( pCur->VendOpenSell.m_item[0] ));

		if ( ++iCount >= UO_MAX_ITEMS_CONT )
			break;	// 75
	}

	return( iCount );
}

bool CClient::addShopMenuSell( CChar* pVendor )
{
	// Player selling to vendor.
	// What things do you have in your inventory that the vendor would want ?
	// Should end with a returned Event_VendorSell()

	ASSERT(pVendor);
	if ( ! pVendor->NPC_IsVendor())
		return( false );

	int iConvertFactor = - pVendor->NPC_GetVendorMarkup(m_pChar);

	addPause();
	CItemContainerPtr pVendBuys = pVendor->GetBank( LAYER_VENDOR_BUYS );
	addItem( pVendBuys );
	CItemContainerPtr pContainer2 = pVendor->GetBank( LAYER_VENDOR_STOCK );
	addItem( pContainer2 );
	CItemContainerPtr pContainer3 = pVendor->GetBank( LAYER_VENDOR_EXTRA );
	addItem( pContainer3 );	// do this or crash client.

	if ( pVendor->IsStatFlag( STATF_Pet ))	// Player vendor.
	{
		pContainer2 = pContainer3;
	}

	CUOCommand cmd;
	cmd.VendOpenSell.m_Cmd = XCMD_VendOpenSell;
	cmd.VendOpenSell.m_UIDVendor = pVendor->GetUID();

	CUOCommand* pCur = (CUOCommand *)((BYTE*) &cmd );
	int iCount = addShopMenuSellFind( m_pChar->GetPackSafe(), pVendBuys, pContainer2, iConvertFactor, pCur );
	if ( ! iCount )
	{
		pVendor->Speak( "Thou doth posses nothing of interest to me." );
		return( false );
	}

	cmd.VendOpenSell.m_len = (((BYTE*)pCur) - ((BYTE*) &cmd )) + sizeof( cmd.VendOpenSell ) - sizeof(cmd.VendOpenSell.m_item);
	cmd.VendOpenSell.m_count = iCount;

	xSendPkt( &cmd, cmd.VendOpenSell.m_len );
	return( true );
}

void CClient::addBankOpen( CChar* pChar, LAYER_TYPE layer )
{
	// open it up for this pChar.
	ASSERT( pChar );

	CItemContainerPtr pBankBox = pChar->GetBank(layer);
	ASSERT(pBankBox);
	addItem( pBankBox );	// may crash client if we dont do this.

	if ( pChar != GetChar())
	{
		// xbank verb on others needs this ?
		// addChar( pChar );
	}

	addContainerSetup( pBankBox );
	pBankBox->OnOpenEvent( m_pChar, pChar );
}

void CClient::addMap( CItemMap* pMap )
{
	// Make player drawn maps possible. (m_map_type=0) ???

	if ( pMap == NULL )
	{
	blank_map:
		WriteString( "This map is blank." );
		return;
	}
	if ( pMap->IsType(IT_MAP_BLANK))
		goto blank_map;

	CRectMap rect;
	rect.SetRect( pMap->m_itMap.m_left,
		pMap->m_itMap.m_top,
		pMap->m_itMap.m_right,
		pMap->m_itMap.m_bottom );

	if ( ! rect.IsValid())
		goto blank_map;
	if ( rect.IsRectEmpty())
		goto blank_map;

	DEBUG_CHECK( pMap->IsType(IT_MAP));

	addPause();

	CUOCommand cmd;
	cmd.MapDisplay.m_Cmd = XCMD_MapDisplay;
	cmd.MapDisplay.m_UID = pMap->GetUID();
	cmd.MapDisplay.m_Gump_Corner = GUMP_MAP_2_NORTH;
	cmd.MapDisplay.m_x_ul = rect.m_left;
	cmd.MapDisplay.m_y_ul = rect.m_top;
	cmd.MapDisplay.m_x_lr = rect.m_right;
	cmd.MapDisplay.m_y_lr = rect.m_bottom;
	cmd.MapDisplay.m_xsize = 0xc8;	// ??? we could make bigger maps ?
	cmd.MapDisplay.m_ysize = 0xc8;
	xSendPkt( &cmd, sizeof( cmd.MapDisplay ));

	addMapMode( pMap, MAP_UNSENT, false );

	// Now show all the pins
	cmd.MapEdit.m_Cmd = XCMD_MapEdit;
	cmd.MapEdit.m_UID = pMap->GetUID();
	cmd.MapEdit.m_Mode = 0x1;	// MAP_PIN?
	cmd.MapEdit.m_Req = 0x00;

	for ( int i=0; i < pMap->m_Pins.GetSize(); i++ )
	{
		cmd.MapEdit.m_pin_x = pMap->m_Pins[i].m_x;
		cmd.MapEdit.m_pin_y = pMap->m_Pins[i].m_y;
		xSendPkt( &cmd, sizeof( cmd.MapEdit ));
	}
}

void CClient::addMapMode( CItemMap* pMap, MAPCMD_TYPE iType, bool fEdit )
{
	// NOTE: MAPMODE_* depends on who is looking. Multi clients could interfere with each other ?

	ASSERT( pMap );
	DEBUG_CHECK( pMap->IsType(IT_MAP));
	pMap->m_fPlotMode = fEdit;

	CUOCommand cmd;
	cmd.MapEdit.m_Cmd = XCMD_MapEdit;
	cmd.MapEdit.m_UID = pMap->GetUID();
	cmd.MapEdit.m_Mode = iType;
	cmd.MapEdit.m_Req = fEdit;
	cmd.MapEdit.m_pin_x = 0;
	cmd.MapEdit.m_pin_y = 0;
	xSendPkt( &cmd, sizeof( cmd.MapEdit ));
}

void CClient::addBulletinBoard( const CItemContainer* pBoard )
{
	// Open up the bulletin board and all it's messages
	// Event_BBoardRequest

	ASSERT( pBoard );
	DEBUG_CHECK( pBoard->IsType(IT_BBOARD));

	addPause();

	CUOCommand cmd;

	// Give the bboard name.
	cmd.BBoard.m_Cmd = XCMD_BBoard;
	int len = strcpylen( (TCHAR *) cmd.BBoard.m_data, (LPCTSTR) pBoard->GetName(), MAX_ITEM_NAME_SIZE );
	len += sizeof(cmd.BBoard);
	cmd.BBoard.m_len = len;
	cmd.BBoard.m_flag = BBOARDF_NAME;
	cmd.BBoard.m_UID = pBoard->GetUID(); // 4-7 = UID for the bboard.
	xSendPkt( &cmd, len );

	// Send Content messages for all the items on the bboard.
	// Not sure what x,y are here, date/time maybe ?
	addContents( pBoard, false, false, false );

	// The client will now ask for the headers it wants.
}

bool CClient::addBBoardMessage( const CItemContainer* pBoard, BBOARDF_TYPE flag, CSphereUID uidMsg )
{
	ASSERT(pBoard);

	CItemPtr pMsgItem2 = g_World.ItemFind( uidMsg );
	CItemMessage* pMsgItem = REF_CAST(CItemMessage,pMsgItem2 );
	if ( ! pBoard->IsItemInside( pMsgItem ))
		return( false );

	addPause();

	// Send back the message header and/or body.
	CUOCommand cmd;
	cmd.BBoard.m_Cmd = XCMD_BBoard;
	cmd.BBoard.m_flag = ( flag == BBOARDF_REQ_FULL ) ? BBOARDF_MSG_BODY : BBOARDF_MSG_HEAD;
	cmd.BBoard.m_UID = pBoard->GetUID();	// 4-7 = UID for the bboard.

	int len = 4;
	PACKDWORD(cmd.BBoard.m_data+0,pMsgItem->GetUID());

	if ( flag == BBOARDF_REQ_HEAD )
	{
		// just the header has this ? (replied to message?)
		PACKDWORD(cmd.BBoard.m_data+4,0);
		len += 4;
	}

	// author name. if it has one.
	if ( pMsgItem->m_sAuthor.IsEmpty())
	{
		cmd.BBoard.m_data[len++] = 0x01;
		cmd.BBoard.m_data[len++] = 0;
	}
	else
	{
		CCharPtr pChar = g_World.CharFind(pMsgItem->m_uidLink);
		if ( pChar == NULL )	// junk it if bad author. (deleted?)
		{
			pMsgItem->DeleteThis();
			return false;
		}
		LPCTSTR pszAuthor = pMsgItem->m_sAuthor;
		if ( IsPrivFlag(PRIV_GM))
		{
			pszAuthor = (LPCTSTR) m_pChar->GetName();
		}
		int lenstr = strlen(pszAuthor) + 1;
		cmd.BBoard.m_data[len++] = lenstr;
		strcpy( (TCHAR*) &cmd.BBoard.m_data[len], pszAuthor);
		len += lenstr;
	}

	// Pad this out with spaces to indent next field.
	int lenstr = strlen( pMsgItem->GetName()) + 1;
	cmd.BBoard.m_data[len++] = lenstr;
	strcpy( (TCHAR*) &cmd.BBoard.m_data[len], (LPCTSTR) pMsgItem->GetName());
	len += lenstr;

	// Get the BBoard message time stamp. m_itBook.m_Time

	CGString sDate;
	sDate.Format( "Day %d", ( g_World.GetGameWorldTime( pMsgItem->m_timeCreate ) / (24*60)) % 365 );
	lenstr = sDate.GetLength()+1;
	cmd.BBoard.m_data[len++] = lenstr;
	strcpy( (TCHAR*) &cmd.BBoard.m_data[len], sDate );
	len += lenstr;

	if ( flag == BBOARDF_REQ_FULL )
	{
		// request for full message body
		//
		PACKDWORD(&(cmd.BBoard.m_data[len]),0);
		len += 4;

		// Pack the text into seperate lines.
		int lines = pMsgItem->GetPageCount();

		// number of lines.
		PACKWORD(&(cmd.BBoard.m_data[len]),lines);
		len += 2;

		// Now pack all the lines
		for ( int i=0; i<lines; i++ )
		{
			LPCTSTR pszText = pMsgItem->GetPageText(i);
			if ( pszText == NULL )
				continue;
			lenstr = strlen(pszText)+1;
			cmd.BBoard.m_data[len++] = lenstr;
			strcpy( (TCHAR*) &cmd.BBoard.m_data[len], pszText );
			len += lenstr;
		}
	}

	len = sizeof( cmd.BBoard ) - sizeof( cmd.BBoard.m_data ) + len;
	cmd.BBoard.m_len = len;
	xSendPkt( &cmd, len );
	return( true );
}

void CClient::addPing( BYTE bCode )
{
	if ( ! IsConnectTypePacket())
	{
		// Not sure how to ping anything but a game client.
		return;
	}
	// Client never responds to this.
	CUOCommand cmd;
	cmd.Ping.m_Cmd = XCMD_Ping;
	cmd.Ping.m_bCode = bCode;
	xSendPkt( &cmd, sizeof(cmd.Ping)); // who knows what this does?
}

void CClient::addFeatureEnable( WORD wFeatureMask )
{
	CUOCommand cmd;
	cmd.FeatureEnable.m_Cmd = XCMD_FeatureEnable;
	cmd.FeatureEnable.m_enable = wFeatureMask;
	xSendPkt( &cmd, sizeof( cmd.FeatureEnable ));
	// xSendPkt( &cmd, sizeof( cmd.FeatureEnable )); // for some reason XXX always sends this twice????

	cmd.ReDrawAll.m_Cmd = XCMD_ReDrawAll;
	xSendPkt( &cmd, sizeof(cmd.ReDrawAll));
}

void CClient::addGumpTextDisp( const CObjBase* pObj, GUMP_TYPE gump, LPCTSTR pszName, LPCTSTR pszText )
{
	DEBUG_CHECK( gump < GUMP_QTY );

	// ??? how do we control where exactly the text goes ??

	// display a gump with some text on it.
	int lenname = pszName ? strlen( pszName ) : 0 ;
	lenname++;
	int lentext = pszText ? strlen( pszText ) : 0 ;
	lentext++;

	CUOCommand cmd;

	int len = ( sizeof(cmd.GumpTextDisp) - 2 ) + lenname + lentext;

	cmd.GumpTextDisp.m_Cmd = XCMD_GumpTextDisp;
	cmd.GumpTextDisp.m_len = len;
	cmd.GumpTextDisp.m_UID = pObj ? ((DWORD)( pObj->GetUID())) : UID_INDEX_CLEAR;
	cmd.GumpTextDisp.m_gump = gump;
	cmd.GumpTextDisp.m_len_unktext = lenname;
	cmd.GumpTextDisp.m_unk11 = 10;	// ? not HUE_TYPE, not x,
	strcpy( cmd.GumpTextDisp.m_unktext, ( pszName ) ? pszName : "" );

	CUOCommand* pCmd = (CUOCommand *)(((BYTE*)(&cmd))+lenname-1);
	strcpy( pCmd->GumpTextDisp.m_text, ( pszText ) ? pszText : "" );
	xSendPkt( &cmd, len );
}

void CClient::addContextMenu( CLIMODE_TYPE mode, const CMenuItem* item, int count, CObjBase* pObj )
{
	// EXTDATA_ContextMenu
	if ( ! count )
		return;
	if ( pObj == NULL )
		pObj = m_pChar;

	static const BYTE sm_bData[] = 
	{
		0x00,0x01,
		0x02,0x58,0x13,0x0f,
		0x07,0x00,
		0x0a,0x17,0xeb,0x00,
		0x20,
		0xff,0xff,
		0x00,0x8b,0x17,0xe8,0x00,0x00,
		0x00,0xcd,0x17,0x75,0x00,0x00,
		0x00,0xe3,0x17,0x8b,0x00,0x00,
		0x00,0xf0,0x17,0x98,0x00,0x00,
		0x00,0xf1,0x17,0x99,0x00,0x00,
		0x00,0xf2,0x17,0x9a,0x00,0x00,
	};

	BYTE bData[ sizeof(sm_bData) ];
	memcpy( bData, sm_bData, sizeof(bData));

	((CUOExtData*)bData)->ContextMenu.m_uid = pObj->GetUID();

	addExtData( EXTDATA_ContextMenu, (CUOExtData*)bData, sizeof(bData));

	SetTargMode( mode );
}

void CClient::addItemMenu( CLIMODE_TYPE mode, const CMenuItem* item, int count, CObjBase* pObj )
{
	// We must set GetTargMode() to show what mode we are in for menu select.
	// Will result in Event_MenuChoice()
	// cmd.ItemMenu.

	if ( ! count )
		return;
	if ( pObj == NULL )
		pObj = m_pChar;

	CUOCommand cmd;
	cmd.MenuItems.m_Cmd = XCMD_MenuItems;
	cmd.MenuItems.m_UID = pObj->GetUID();
	cmd.MenuItems.m_context = mode;

	int len = sizeof( cmd.MenuItems ) - sizeof( cmd.MenuItems.m_item );
	int lenttitle = item[0].m_sText.GetLength();
	cmd.MenuItems.m_lenname = lenttitle;
	strcpy( cmd.MenuItems.m_name, item[0].m_sText );

	lenttitle --;
	len += lenttitle;
	CUOCommand* pCmd = (CUOCommand *)(((BYTE*)&cmd) + lenttitle );
	pCmd->MenuItems.m_count = count;

	// Strings in here are NOT null terminated.
	for ( int i=1; i<=count; i++ )
	{
		int lenitem = item[i].m_sText.GetLength();
		if ( lenitem <= 0 || lenitem >= 256 )
		{
			DEBUG_ERR(("Bad option length %d in menu item %d" LOG_CR, lenitem, i ));
			continue;
		}

		pCmd->MenuItems.m_item[0].m_id = item[i].m_id; // image next to menu.
		pCmd->MenuItems.m_item[0].m_check = 0;	// check or not ?
		pCmd->MenuItems.m_item[0].m_lentext = lenitem;
		strcpy( pCmd->MenuItems.m_item[0].m_name, item[i].m_sText );

		lenitem += sizeof( cmd.MenuItems.m_item[0] ) - 1;
		pCmd = (CUOCommand *)(((BYTE*)pCmd) + lenitem );
		len += lenitem;
	}

	cmd.MenuItems.m_len = len;
	xSendPkt( &cmd, len );

	m_Targ.m_tmMenu.m_UID = pObj->GetUID();

	SetTargMode( mode );
}

void CClient::addGumpInpVal( bool fCancel, INPVAL_STYLE style,
	DWORD iMaxLength,
	LPCTSTR pszText1,
	LPCTSTR pszText2,
	CObjBase* pObj )
{
	// CLIMODE_INPVAL
	// Should result in Event_GumpInpValRet
	// just input an objects attribute.
	// 	from "INPDLG" verb maxchars
	//
	// ARGS:
	// 	m_Targ.m_UID = pObj->GetUID();
	//  m_Targ_Text = verb

	ASSERT( pObj );

	CUOCommand cmd;
	cmd.GumpInpVal.m_Cmd = XCMD_GumpInpVal;
	cmd.GumpInpVal.m_UID = pObj->GetUID();
	cmd.GumpInpVal.m_context = CLIMODE_INPVAL;

	int len1 = strlen(pszText1);
	cmd.GumpInpVal.m_textlen1 = len1+1;
	strcpy( cmd.GumpInpVal.m_text1, pszText1 );

	CUOCommand* pCmd2 = (CUOCommand*)( ((BYTE*)&cmd) + len1 );
	pCmd2->GumpInpVal.m_cancel = fCancel ? 1 : 0;
	pCmd2->GumpInpVal.m_style = (BYTE) style;
	pCmd2->GumpInpVal.m_mask = iMaxLength;	// !!!

	int len2;
	switch (style)
	{
	case INPVAL_STYLE_TEXTEDIT: // Text
		len2 = sprintf( pCmd2->GumpInpVal.m_text2,
			"%s (%i chars max)", pszText2, iMaxLength );
		break;
	case INPVAL_STYLE_NUMEDIT: // Numeric
		len2 = sprintf( pCmd2->GumpInpVal.m_text2,
			"%s (0 - %i)", pszText2, iMaxLength );
		break;
	default:
	case INPVAL_STYLE_NOEDIT: // None
		len2 = 0;
		pCmd2->GumpInpVal.m_text2[0] = '\0';
		break;
	}

	pCmd2->GumpInpVal.m_textlen2 = len2+1;

	int len = sizeof(cmd.GumpInpVal) + len1 + len2;
	cmd.GumpInpVal.m_len = len;
	xSendPkt( &cmd, len);

	m_Targ.m_tmInpVal.m_UID = pObj->GetUID();
	m_Targ.m_tmInpVal.m_PrvGumpID = m_Targ.m_tmGumpDialog.m_ResourceID;

	m_Targ.m_UID = pObj->GetUID();
	// m_Targ_Text = verb
	SetTargMode( CLIMODE_INPVAL );
}

//---------------------------------------------------------------------

TRIGRET_TYPE CClient::Menu_OnSelect( CSphereUID rid, int iSelect, CObjBase* pObj ) // Menus for general purpose
{
	// A select was made. so run the script.
	// iSelect = 0 = cancel.

	CResourceLock s( g_Cfg.ResourceGetDef( rid ));
	if ( ! s.IsFileOpen())
	{
		// Can't find the resource ?
		return( TRIGRET_ENDIF );
	}

	if ( pObj == NULL )
		pObj = m_pChar;

	// execute the menu script.
	int i=0;	// 1 based selection.
	while ( s.ReadKeyParse())
	{
		if ( ! s.IsLineTrigger())
			continue;

		i++;
		if ( i < iSelect )
			continue;
		if ( i > iSelect )
			break;

		CSphereExpContext exec(pObj,m_pChar);
		return exec.ExecuteScript( s, TRIGRUN_SECTION_TRUE );
	}

	// No selection ?
	return( TRIGRET_ENDIF );
}

bool CMenuItem::ParseLine( TCHAR* pszArgs, CSphereExpContext& exec )
{
	TCHAR* pszArgStart = pszArgs;
	while ( _ISCSYM( *pszArgs ))
		pszArgs++;

	if ( *pszArgs )
	{
		*pszArgs = '\0';
		pszArgs++;
		GETNONWHITESPACE( pszArgs );
	}

	// The item id (if we want to have an item type menu) or 0

	if ( strcmp( pszArgStart, "0" ))
	{
		m_id = (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, pszArgStart );
		CItemDefPtr pItemBase = g_Cfg.FindItemDef( (ITEMID_TYPE) m_id );
		if ( pItemBase != NULL )
		{
			m_id = pItemBase->GetDispID();
			exec.SetBaseObject( REF_CAST(CItemDef,pItemBase));
		}
		else
		{
			DEBUG_ERR(( "Bad MENU item id '%s'" LOG_CR, pszArgStart ));
			return( false );	// skip this.
		}
	}
	else
	{
		m_id = 0;
	}

	exec.s_ParseEscapes( pszArgs, 0 );
	m_sText = pszArgs;

	if ( m_sText.IsEmpty())
	{
		if ( exec.GetBaseObject())	// use the objects name by default.
		{
			m_sText = exec.GetBaseObject()->GetName();
			if ( ! m_sText.IsEmpty())
				return( true );
		}
		DEBUG_ERR(( "Bad MENU item text '%s'" LOG_CR, pszArgStart ));
	}

	return( !m_sText.IsEmpty());
}

bool CClient::Menu_Setup( CSphereUID ridMenu, CObjBase* pObj, bool fContextMenu )
{
	// Menus for general purpose
	// Expect Event_MenuChoice() back.

	CResourceLock sLock( g_Cfg.ResourceGetDef( ridMenu ));
	if ( ! sLock.IsFileOpen())
	{
		return false;
	}

	if ( pObj == NULL )
	{
		pObj = m_pChar;
	}

	CSphereExpContext exec( pObj, m_pChar );

	sLock.ReadLine();	// get title for the menu.
	exec.s_ParseEscapes( sLock.GetLineBuffer(), 0 );

	CMenuItem item[UO_MAX_MENU_ITEMS];
	item[0].m_sText = sLock.GetKey();
	// item[0].m_id = ridMenu.m_internalrid;	// general context id

	int i=0;
	while (sLock.ReadKeyParse())
	{
		if ( ! sLock.IsLineTrigger())
			continue;
		if ( ++i >= COUNTOF( item ))
			break;
		if ( ! item[i].ParseLine( sLock.GetArgMod(), exec ))
		{
			i--;
		}
	}

	m_Targ.m_tmMenu.m_ResourceID = ridMenu;

	if ( fContextMenu )
	{
		addContextMenu( CLIMODE_MENU, item, i, pObj );
	}
	else
	{
		addItemMenu( CLIMODE_MENU, item, i, pObj );
	}

	return true;
}

//---------------------------------------------------------------------
// Login type stuff.

LOGIN_ERR_TYPE CClient::Setup_Start( CChar* pChar ) // Send character startup stuff to player
{
	// Char is logging into the world. Tell them what is going on.
	// Play this char.
	ASSERT( GetAccount());
	ASSERT( pChar );

	CharDisconnect();	// I'm already logged in as someone else ?

	g_Log.Event( LOG_GROUP_CLIENTS, LOGL_TRACE, "%x:Setup_Start acct='%s', char='%s'" LOG_CR, m_Socket.GetSocket(), (LPCTSTR) GetAccount()->GetName(), (LPCTSTR) pChar->GetName());

#ifndef _DEBUG
#ifdef _WIN32
	srand( GetTickCount()); // Perform randomize
#else
	srand( time(NULL) ); // Perform randomize
#endif
#endif

	bool fQuickLogIn = false;
	if ( ! pChar->IsDisconnected())
	{
		// The players char is already in game ! Client linger time re-login.
		fQuickLogIn = true;
	}

	addPlayerStart( pChar );
	ASSERT(m_pChar);

	WriteString( g_szServerDescription );

	CSphereExpContext exec(pChar, this);
	TRIGRET_TYPE iRet = pChar->OnTrigger( CCharDef::T_LogIn, exec);
	if ( iRet == TRIGRET_RET_FALSE )
	{
		return LOGIN_ERR_BLOCKED;
	}

	if ( fQuickLogIn )
	{
		WriteString( "Welcome Back" );
	}
	else
	{
		Printf( (g_Serv.m_Clients.GetCount()==2) ?
			"There is 1 other player here." : "There are %d other players here.",
			g_Serv.m_Clients.GetCount()-1 );

		// Get the intro script.
		addScrollResource(( GetPrivLevel() <= PLEVEL_Guest ) ? "SCROLL_GUEST" : "SCROLL_MOTD", SCROLL_TYPE_UPDATES );

		// How long have i been away.
		CGTime timeCur;
		timeCur.InitTimeCurrent();
		int iDays = timeCur.GetTotalDays() - GetAccount()->m_dateLastConnect.GetTotalDays();
		if ( iDays > 0 )	// neg = new char
		{
			Printf( "You have been gone for %d day(s)", iDays );
		}
	}

	if ( ! g_Cfg.m_fSecure )
	{
		addBark( "WARNING: The world is NOT running in SECURE MODE",
			NULL, HUE_TEXT_DEF, TALKMODE_SYSTEM, FONT_BOLD );
	}
	if ( IsPrivFlag( PRIV_GM_PAGE ))
	{
		Printf( "There are %d GM pages pending. Use /PAGE command.", g_World.m_GMPages.GetCount());
	}
	if ( g_Cfg.m_fRequireEmail )
	{
		// Have you set your email notification ?
		if ( GetAccount()->m_sEMail.IsEmpty())
		{
			Printf( "Email address for account '%s' has not yet been set.",
				(LPCTSTR)GetAccount()->GetName(), (LPCTSTR)( GetAccount()->m_sEMail ));
			WriteString( "Use the command /EMAIL name@yoursp.com to set it." );

			// set me to "GUEST" mode.
		}
		else if ( ! IsPrivFlag( PRIV_EMAIL_VALID ))
		{
			if ( GetAccount()->m_iEmailFailures > 5 )
			{
				Printf( "Email '%s' for account '%s' has failed %d times.",
					(LPCTSTR)GetAccount()->GetName(), (LPCTSTR)( GetAccount()->m_sEMail ), GetAccount()->m_iEmailFailures );
			}
			else
			{
				Printf( "Email '%s' for account '%s' has not yet verified.",
					(LPCTSTR)GetAccount()->GetName(), (LPCTSTR)( GetAccount()->m_sEMail ));
			}
			WriteString( "Use the command /EMAIL name@yoursp.com to change it." );
		}
	}
	if ( IsPrivFlag( PRIV_JAILED ))
	{
		m_pChar->JailEffect();
	}
#if 1
	if ( ! fQuickLogIn && 
		m_pChar->m_pArea.IsValidRefObj() &&
		m_pChar->m_pArea->IsFlagGuarded() &&
		! m_pChar->m_pArea->IsFlag( REGION_FLAG_ANNOUNCE ))
	{
		CVarDefPtr pVar = m_pChar->m_pArea->m_TagDefs.FindKeyPtr("GUARDOWNER");
		Printf( "You are under the protection of %s guards",
			(pVar) ? (LPCTSTR) pVar->GetPSTR() : "the" );
	}
#endif
	if ( g_Serv.m_timeShutdown.IsTimeValid())
	{
		addBark( "WARNING: The system is scheduled to shutdown soon",
			NULL, HUE_TEXT_DEF, TALKMODE_SYSTEM, FONT_BOLD );
	}

	// Announce you to the world.
	Announce( true );
	m_pChar->Update( this );

	if ( g_Cfg.m_fRequireEmail && GetAccount()->m_sEMail.IsEmpty())
	{
		// Prompt to set this right now !
	}

	// don't login on the water ! (unless i can swim)
	if ( ! m_pChar->Char_GetDef()->Can(CAN_C_SWIM) &&
		! IsPrivFlag(PRIV_GM) &&
		m_pChar->IsSwimming())
	{
		// bring to the nearest shore.
		// drift to nearest shore ?
		int iDist = 1;
		int i;
		for ( i=0; i<20; i++)
		{
			// try diagonal in all directions
			int iDistNew = iDist + 20;
			for ( int iDir = DIR_NE; iDir <= DIR_NW; iDir += 2 )
			{
				if ( m_pChar->MoveToValidSpot( (DIR_TYPE) iDir, iDistNew, iDist ))
				{
					i = 100;	// breakout
					break;
				}
			}
			iDist = iDistNew;
		}
		if ( i < 100 )
		{
			WriteString( "You are stranded in the water !" );
		}
		else
		{
			WriteString( "You have drifted to the nearest shore." );
		}
	}

	// Set up all my IT_EQ_MESSAGE gumps.
	for ( CItemPtr pItem=m_pChar->GetHead(); pItem!=NULL; pItem=pItem->GetNext())
	{
		if ( pItem->IsType(IT_EQ_MESSAGE))
		{
			// ??? Just launch the command such as "dialog b_thisismydialogmessage"
		}
	}

	DEBUG_TRACE(( "%x:Setup_Start done" LOG_CR, m_Socket.GetSocket()));
	return LOGIN_SUCCESS;
}

void CClient::Setup_CreateDialog( const CUOEvent* pEvent ) // All the character creation stuff
{
	// XCMD_Create
	ASSERT( GetAccount());
	if ( m_pChar != NULL )
	{
		// Loggin in as a new player while already on line !
		WriteString( "Already on line" );
		DEBUG_ERR(( "%x:Setup_CreateDialog acct='%s' already on line!" LOG_CR, m_Socket.GetSocket(), (LPCTSTR) GetAccount()->GetName()));
		return;
	}

	// Make sure they don't already have too many chars !
	int iMaxChars = ( IsPrivFlag( PRIV_GM )) ? UO_MAX_CHARS_PER_ACCT : ( g_Cfg.m_iMaxCharsPerAccount );
	int iQtyChars = GetAccount()->m_Chars.GetSize();
	if ( iQtyChars >= iMaxChars )
	{
		Printf( "You already have %d character(s).", iQtyChars );
		if ( GetPrivLevel() < PLEVEL_Seer )
		{
			addLoginErr( LOGIN_ERR_OTHER );
			return;
		}
	}

	CCharPtr pChar = CChar::CreateBasic( CREID_MAN );
	ASSERT(pChar);
	pChar->InitPlayer( pEvent, this );

	g_Log.Event( LOG_GROUP_CLIENTS, LOGL_TRACE, "%x:Setup_CreateDialog acct='%s', char='%s'" LOG_CR,
		m_Socket.GetSocket(), (LPCTSTR)GetAccount()->GetName(), (LPCTSTR)pChar->GetName());

	Setup_Start( pChar );
}

bool CClient::Setup_Play( int iSlot ) // After hitting "Play Character" button
{
	// Mode == CLIMODE_SETUP_CHARLIST

	DEBUG_TRACE(( "%x:Setup_Play slot %d" LOG_CR, m_Socket.GetSocket(), iSlot ));

	if ( ! GetAccount())
		return( false );
	if ( iSlot >= COUNTOF(m_Targ.m_tmSetupCharList))
		return false;

	CCharPtr pChar = g_World.CharFind(m_Targ.m_tmSetupCharList[ iSlot ]);
	if ( pChar == NULL || pChar->GetAccount() != GetAccount())
		return false;

	Setup_Start( pChar );
	return( true );
}

DELETE_ERR_TYPE CClient::Setup_Delete( int iSlot ) // Deletion of character
{
	DEBUG_MSG(( "%x:Setup_Delete slot=%d" LOG_CR, m_Socket.GetSocket(), iSlot ));

	// DELETE_ERR_BAD_PASS = 0, // 0 That character password is invalid.
	// DELETE_ERR_IN_USE,	// 2 That character is being played right now.
	// DELETE_ERR_NOT_OLD_ENOUGH, // 3 That character is not old enough to delete. The character must be 7 days old before it can be deleted.
	// DELETE_ERR_QD_4_BACKUP, // 4 That character is currently queued for backup and cannot be deleted.

	if ( GetAccount() == NULL || iSlot >= COUNTOF(m_Targ.m_tmSetupCharList))
		return DELETE_ERR_NOT_EXIST;

	CCharPtr pChar = g_World.CharFind( m_Targ.m_tmSetupCharList[iSlot] );
	if ( pChar == NULL || pChar->GetAccount() != GetAccount())
		return DELETE_ERR_BAD_PASS;

	if ( ! pChar->IsDisconnected())
	{
		return DELETE_ERR_IN_USE;
	}

	// Make sure the char is at least x days old.
	if ( g_Cfg.m_iMinCharDeleteTime &&
		pChar->m_timeCreate.GetCacheAge() < g_Cfg.m_iMinCharDeleteTime )
	{
		if ( GetPrivLevel() < PLEVEL_Seer )
		{
			return DELETE_ERR_NOT_OLD_ENOUGH;
		}
	}

	pChar->DeleteThis();
	// refill the list.

	CUOCommand cmd;
	cmd.CharList2.m_Cmd = XCMD_CharList2;
	int len = sizeof( cmd.CharList2 );
	cmd.CharList2.m_len = len;
	cmd.CharList2.m_count = Setup_FillCharList( cmd.CharList2.m_char, g_World.CharFind(GetAccount()->m_uidLastChar));
	xSendPkt( &cmd, len );

	return( DELETE_SUCCESS );
}

LOGIN_ERR_TYPE CClient::Setup_CharListReq( const char* pszAccName, const char* pszPassword, DWORD dwAccount )
{
	// XCMD_CharListReq
	// Gameserver login and request character listing
	// dwAccount is the account id we got from XCMD_Relay
	// This is used to alter the encryption.

	if ( m_ConnectType != CONNECT_GAME )	// Not a game connection ?
		return(LOGIN_ERR_OTHER);

	switch ( GetTargMode())
	{
	case CLIMODE_SETUP_RELAY:
		ClearTargMode();
		break;
	}

	LOGIN_ERR_TYPE lErr = LogIn( pszAccName, pszPassword );
	if ( lErr != LOGIN_SUCCESS )
	{
		if ( ! dwAccount && lErr != LOGIN_ERR_OTHER )
		{
			addLoginErr(lErr);
		}
		return( lErr );
	}
	CAccountPtr pAccount = GetAccount();
	ASSERT( pAccount);

	CCharPtr pCharLast = g_World.CharFind(pAccount->m_uidLastChar);
	if ( pCharLast &&
		pCharLast->GetAccount() == pAccount &&
		pAccount->GetPrivLevel() <= PLEVEL_GM &&
		! pCharLast->IsDisconnected())
	{
		// If the last char is lingering then log back into this char instantly.
		// m_iClientLingerTime
		return Setup_Start(pCharLast);
	}

	// DEBUG_MSG(( "%x:Setup_ListFill" LOG_CR, m_Socket.GetSocket()));

	CUOCommand cmd;
	cmd.CharList.m_Cmd = XCMD_CharList;
	int len = sizeof( cmd.CharList ) - sizeof(cmd.CharList.m_start) + ( g_Cfg.m_StartDefs.GetSize()* sizeof(cmd.CharList.m_start[0]));
	cmd.CharList.m_len = len;

	DEBUG_CHECK( COUNTOF(cmd.CharList.m_char) == UO_MAX_CHARS_PER_ACCT );

	// list chars to your account that may still be logged in !
	// "LASTCHARUID" = list this one first.
	cmd.CharList.m_count = Setup_FillCharList( cmd.CharList.m_char, pCharLast );

	// now list all the starting locations. (just in case we create a new char.)
	// NOTE: New Versions of the client just ignore all this stuff.

	int iCount = g_Cfg.m_StartDefs.GetSize();
	cmd.CharList.m_startcount = iCount;
	for ( int i=0;i<iCount;i++)
	{
		cmd.CharList.m_start[i].m_id = i+1;
		strcpylen( cmd.CharList.m_start[i].m_area, g_Cfg.m_StartDefs[i]->m_sArea, sizeof(cmd.CharList.m_start[i].m_area));
		strcpylen( cmd.CharList.m_start[i].m_name, g_Cfg.m_StartDefs[i]->m_sName, sizeof(cmd.CharList.m_start[i].m_name));
	}

	xSendPkt( &cmd, len );
	m_Targ.m_Mode = CLIMODE_SETUP_CHARLIST;
	return LOGIN_SUCCESS;
}

LOGIN_ERR_TYPE CClient::LogIn( CAccount* pAccount )
{
	if ( pAccount == NULL )
		return( LOGIN_ERR_NONE );

	if ( pAccount->IsPrivFlag( PRIV_BLOCKED ))
	{
		if ( ! g_Serv.m_sEMail.IsEmpty())
			Printf( "Account blocked. Send email to admin %s.", (LPCTSTR) g_Serv.m_sEMail );
		return( LOGIN_ERR_BLOCKED );
	}

	CSocketAddress PeerName = m_Socket.GetPeerName();

	// Look for this account already in use.
	CClientPtr pClientPrev = g_Serv.FindClientAccount( pAccount );
	if ( pClientPrev != NULL && pClientPrev != this )
	{
		// Only if it's from a diff ip ?

		CSocketAddress PeerNamePrv = pClientPrev->m_Socket.GetPeerName();

		if ( ! PeerName.IsSameIP( PeerNamePrv ))
		{
			WriteString( "Account already in use." );
			return( LOGIN_ERR_USED );
		}

		// From same ip is ok.
		// So the old connect is bad !
		if ( IsConnectTypePacket() && pClientPrev->IsConnectTypePacket())
		{
			pClientPrev->m_Socket.Close();
		}
		else if ( m_ConnectType == pClientPrev->m_ConnectType )
		{
			return( LOGIN_ERR_USED );
		}
	}
	if ( g_Cfg.m_iClientsMax <= 0 )
	{
		// Allow no one but locals on.
		CSocketAddress SockName = m_Socket.GetSockName();
		if ( ! PeerName.IsLocalAddr() && SockName.GetAddrIP() != PeerName.GetAddrIP())
		{
			WriteString( "Server is in local debug mode." );
			return( LOGIN_ERR_BLOCKED );
		}
	}
	if ( g_Cfg.m_iClientsMax <= 1 )
	{
		// Allow no one but Administrator on.
		if ( pAccount->GetPrivLevel() < PLEVEL_Admin )
		{
			WriteString( "Server is in admin only mode." );
			return( LOGIN_ERR_BLOCKED );
		}
	}
	if ( pAccount->GetPrivLevel() < PLEVEL_GM &&
		g_Serv.m_Clients.GetCount() > g_Cfg.m_iClientsMax  )
	{
		// Give them a polite goodbye.
		WriteString( "Sorry the server is full. Try again later." );
		return( LOGIN_ERR_BLOCKED );
	}

	m_timeLogin.InitTimeCurrent();	// g_World clock of login time. "LASTCONNECTTIME"
	m_pAccount = pAccount;
	// SERVTRIG_ClientChange

	pAccount->OnLogin( this );

	return( LOGIN_SUCCESS );
}

LOGIN_ERR_TYPE CClient::LogIn( LPCTSTR pszAccName, LPCTSTR pszPassword )
{
	// Try to validate this account.
	// Do not send output messages as this might be a console or web page or game client.
	// NOTE: addLoginErr() will get called after this.

	if ( GetAccount()) // already logged in.
		return( LOGIN_SUCCESS );

	TCHAR szAccountName[ MAX_NAME_SIZE ];
	TCHAR szTmp[ MAX_NAME_SIZE ];
	int iLen1 = strlen( pszAccName );
	int iLen3 = CAccount::NameStrip( szAccountName, pszAccName );
	if ( iLen3 < 0 )
	{
		Printf( "Bad account name '%s'", (LPCTSTR) szAccountName );
		return( LOGIN_ERR_BLOCKED );
	}
	if ( iLen3 == 0 ||
		iLen1 != iLen3 ||
		iLen1 > MAX_NAME_SIZE )	// a corrupt message.
	{
badformat:
		TCHAR szVersion[ 256 ];
		Printf( "Bad login format. Check Client Version '%s' ?", (LPCTSTR) m_Crypt.WriteCryptVer( szVersion ));
		return( LOGIN_ERR_OTHER );
	}

	iLen1 = strlen( pszPassword );
	iLen3 = Str_GetBare( szTmp, pszPassword, MAX_NAME_SIZE );
	if ( iLen1 != iLen3 )	// a corrupt message.
		goto badformat;

	bool fGuestAccount = ! _strnicmp( szAccountName, "GUEST", 5 );
	if ( fGuestAccount )
	{
		// trying to log in as some sort of guest.
		// Find or create a new guest account.

		for ( int i=0;; i++ )
		{
			if ( i>=g_Cfg.m_iGuestsMax )
			{
				WriteString( "All guest accounts are currently used." );
				return( LOGIN_ERR_BLOCKED );
			}

			sprintf( szAccountName, "GUEST%d", i );
			CAccountPtr pAccount = g_Accounts.Account_FindNameChecked( szAccountName );
			if ( ! pAccount.IsValidRefObj())
			{
				pAccount = g_Accounts.Account_Create( szAccountName );
				ASSERT( pAccount );
			}

			if ( g_Serv.FindClientAccount( pAccount ) == NULL )	// not used.
			{
				strcpy( szAccountName, (LPCTSTR) pAccount->GetName());
				break;
			}
		}
	}
	else
	{
		if ( pszPassword[0] == '\0' )
		{
			WriteString( "Must supply a password." );
			return( LOGIN_ERR_BAD_PASS );
		}
	}

	bool fAdminCreate = ( ! _stricmp( szAccountName, "Administrator" )) && ( g_Accounts.Account_GetCount() == 0 );
	bool fAutoCreate = ( g_Serv.m_eAccApp == ACCAPP_Free || g_Serv.m_eAccApp == ACCAPP_GuestAuto || g_Serv.m_eAccApp == ACCAPP_GuestTrial || g_Serv.m_eAccApp == ACCAPP_XGM );

	CAccountPtr pAccount = g_Accounts.Account_FindNameChecked( szAccountName );
	if ( ! pAccount )
	{
		if ( ! fAutoCreate && ! fAdminCreate )
		{
			// No account by this name.
			g_Log.Event( LOG_GROUP_CLIENTS, LOGL_TRACE,
#ifdef _DEBUG
				"%x:ERR Login NO Account '%s', pass='%s'" LOG_CR, m_Socket.GetSocket(), szAccountName, pszPassword );
#else
				"%x:ERR Login NO Account '%s'" LOG_CR, m_Socket.GetSocket(), szAccountName );
#endif
			Printf( "Unknown account name '%s'. Try using a 'guest' account.", (LPCTSTR) szAccountName );
			return( LOGIN_ERR_NONE );
		}

		pAccount = g_Accounts.Account_Create( szAccountName );
		ASSERT(pAccount);

		if ( fAdminCreate )
		{
			pAccount->SetPrivLevel( PLEVEL_Admin );
		}
		if ( g_Serv.m_eAccApp == ACCAPP_XGM )
		{
			pAccount->SetPrivLevel( PLEVEL_GM );
		}
	}

	if ( ! fGuestAccount && ! pAccount->IsPrivFlag( PRIV_BLOCKED ))
	{
		if ( ! pAccount->CheckPassword(pszPassword))
		{
			g_Log.Event( LOG_GROUP_CLIENTS, LOGL_TRACE, 
#ifdef _DEBUG
			"%x: '%s' bad pass '%s' != '%s'" LOG_CR, m_Socket.GetSocket(), (LPCTSTR) pAccount->GetName(), (LPCTSTR) pszPassword, (LPCTSTR) pAccount->GetPassword());
#else
			"%x: '%s' bad password" LOG_CR, m_Socket.GetSocket(), (LPCTSTR) pAccount->GetName());
#endif
			WriteString( "Bad password for this account." );

			// Attempt to block the connection if this is an account attack.

			CLogIPPtr pLogIP = g_Cfg.FindLogIP(m_Socket);
			if ( pLogIP )
			{
				pLogIP->IncBadPassword(szAccountName);
			}

			return( LOGIN_ERR_BAD_PASS );
		}
	}

	return LogIn( pAccount );
}


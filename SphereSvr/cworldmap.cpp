//
// CWorldMap.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.

//************************
// Natural resources.

CItemPtr CWorld::CheckNaturalResource( const CPointMap& pt, IT_TYPE Type, bool fTest )
{
	// RETURN:
	//  The resource tracking item.
	//  NULL = nothing here.

	if ( ! pt.IsValidPoint())
		return( NULL );

	// Check/Decrement natural resources at this location.
	// We create an invis object to time out and produce more.
	// RETURN: Quantity they wanted. 0 = none here.

	if ( fTest )	// Is the resource avail at all here ?
	{
		if ( ! g_World.IsItemTypeNear( pt, Type ))
			return( NULL );
	}

	// Find the resource object.
	CItemPtr pResBit;
	CWorldSearch Area(pt);
	for(;;)
	{
		pResBit = Area.GetNextItem();
		if ( ! pResBit )
			break;
		// NOTE: ??? Not all resource objects are world gems. should they be ?
		// I wanted to make tree stumps etc be the resource block some day.

		if ( pResBit->IsType(Type) && pResBit->GetID() == ITEMID_WorldGem )
			break;
	}

	// If none then create one.
	if ( pResBit )
	{
		return( pResBit );
	}

	// What type of ore is here ?
	// NOTE: This is unrelated to the fact that we might not be skilled enough to find it.
	// Odds of a vein of ore being present are unrelated to my skill level.
	// Odds of my finding it are.
	// RES_RegionResource from RES_RegionType linked to RES_Area

	CRegionComplexPtr pRegion = REF_CAST(CRegionComplex,pt.GetRegion( REGION_TYPE_AREA ));
	if ( pRegion == NULL )
	{
		DEBUG_CHECK(pRegion);
		return( NULL );
	}

	int iQty = pRegion->m_Events.GetSize();
	if ( iQty == 0 )
	{
		// just use the background (default) region for this
		CPointMap ptZero(0,0);
		pRegion = REF_CAST(CRegionComplex,ptZero.GetRegion( REGION_TYPE_AREA ));
	}

	// Find RES_RegionType
	const CRegionType* pResGroup = pRegion->FindNaturalResource( Type );
	if ( pResGroup == NULL )
	{
		return( NULL );
	}

	// Find RES_RegionResource
	CSphereUID rid = pResGroup->GetMemberID( pResGroup->GetRandMemberIndex());
	const CRegionResourceDef *pOreDef = REF_CAST(const CRegionResourceDef,g_Cfg.ResourceGetDef( rid ));
	if ( pOreDef == NULL )
	{
		return( NULL );
	}

	pResBit = CItem::CreateScript( ITEMID_WorldGem, NULL );
	ASSERT(pResBit);

	pResBit->SetType( Type );
	pResBit->SetAttr( ATTR_INVIS | ATTR_MOVE_NEVER );	// use the newbie flag just to show that it has just been created.
	pResBit->m_itResource.m_rid_res = rid;

	// Total amount of ore here.
	pResBit->SetAmount( pOreDef->m_Amount.GetRandom());

	pResBit->MoveToDecay( pt, pOreDef->m_iRegenerateTime );	// Delete myself in this amount of time.

	return( pResBit );
}

//////////////////////////////////////////////////////////////////
// Map reading and blocking.

bool CWorld::IsItemTypeNear( const CPointMap& pt, IT_TYPE iType, int iDistance )
{
	CPointMap ptn = FindItemTypeNearby( pt, iType, iDistance );
	return( ptn.IsValidPoint());
}

CPointMap CWorld::FindItemTypeNearby( const CPointMap& pt, IT_TYPE iType, int iDistance )
{
	// Find the closest item of this type.
	// This does not mean that i can touch it.
	// ARGS:
	//   iDistance = 2d distance to search.

	CPointMap ptFound;
	int iTestDistance;

	// Check dynamics first since they are the easiest.
	CWorldSearch Area( pt, iDistance );
	for(;;)
	{
		CItemPtr pItem = Area.GetNextItem();
		if ( pItem == NULL )
			break;
		if ( ! pItem->IsType( iType ) &&
			! pItem->Item_GetDef()->IsType(iType) )
			continue;

		iTestDistance = pt.GetDist(pItem->GetTopPoint());
		if ( iTestDistance > iDistance )
			continue;

		ptFound = pItem->GetTopPoint();
		iDistance = iTestDistance;	// tighten up the search.
		if ( ! iDistance )
			return( ptFound );
	}

	// Check for appropriate terrain type
	CRectMap rect;
	rect.SetRect( pt.m_x - iDistance, pt.m_y - iDistance,
		pt.m_x + iDistance + 1, pt.m_y + iDistance + 1 );

	for (int x = rect.m_left; x < rect.m_right; x++ )
	{
		for ( int y = rect.m_top; y < rect.m_bottom; y++ )
		{
			CPointMap ptTest( x, y, pt.m_z, pt.m_mapplane );
			const CMulMapMeter* pMeter = GetMapMeter( ptTest );
			ASSERT(pMeter);
			ptTest.m_z = pMeter->m_z;

			switch (iType)
			{
			case IT_ROCK:
				if ( pMeter->IsTerrainRock())
				{
				checkterrain:
					iTestDistance = pt.GetDist(ptTest);
					if ( iTestDistance > iDistance )
						break;

					ptFound = ptTest;
					iDistance = iTestDistance;	// tighten up the search.
					if ( ! iDistance )
						return( ptFound );

					rect.SetRect( pt.m_x - iDistance, pt.m_y - iDistance,
						pt.m_x + iDistance + 1, pt.m_y + iDistance + 1 );
				}
				break;
			case IT_WATER:
				if ( pMeter->IsTerrainWater())
					goto checkterrain;
				break;
			case IT_GRASS:
				if ( pMeter->IsTerrainGrass())
					goto checkterrain;
				break;
			case IT_DIRT:
				if ( pMeter->IsTerrainDirt())
					goto checkterrain;
				break;
			}
		}
	}

	// Any statics here? (checks just 1 8x8 block !???)
	const CMulMapBlock* pMapBlock = GetMapBlock( pt );
	ASSERT( pMapBlock );

	int iQty = pMapBlock->m_Statics.GetStaticQty();
	if ( iQty )  // no static items here.
	{
		int x2=pMapBlock->GetOffsetX(pt.m_x);
		int y2=pMapBlock->GetOffsetY(pt.m_y);

		for ( int i=0; i < iQty; i++ )
		{
			const CMulStaticItemRec* pStatic = pMapBlock->m_Statics.GetStatic( i );

			// inside the range we want ?
			CPointMap ptTest( pStatic->m_x+pMapBlock->m_pt.m_x, pStatic->m_y+pMapBlock->m_pt.m_y, pStatic->m_z, pt.m_mapplane );
			iTestDistance = pt.GetDist(ptTest);
			if ( iTestDistance > iDistance )
				continue;

			ITEMID_TYPE idTile = pStatic->GetDispID();

			// Check the script def for the item.
			CItemDefPtr pItemDef = g_Cfg.FindItemDef( idTile );
			if ( pItemDef == NULL )
				continue;
			if ( ! pItemDef->IsType( iType ))
				continue;
			ptFound = ptTest;
			iDistance = iTestDistance;
			if ( ! iDistance )
				return( ptFound );
		}
	}

	// Parts of multis ?

	return ptFound;
}

//****************************************************

#if 0

static int GetHeightTerrain( const CPointMap & pt, int z, const CMulMapBlock* pMapBlock )
{
	// Stupid terrain is an average of the stuff around it?!
	for ( int i=1; i<4; i++ )
	{
		DIR_TYPE DirCheck;
		switch ( i )
		{
		case 1:
			DirCheck = DIR_E;
			break;
		case 2:
			DirCheck = DIR_SE;
			break;
		case 3:
			DirCheck = DIR_S;
			break;
		}

		CPointMap ptTest = pt;
		ptTest.Move( DirCheck );

		const CMulMapMeter* pMeter = pMapBlock->GetTerrain( SPHEREMAP_BLOCK_OFFSET(ptTest.m_x), SPHEREMAP_BLOCK_OFFSET(ptTest.m_y));
		ASSERT(pMeter);

	}
}

#endif

void CWorld::GetHeightPoint( const CPointMap& pt, CMulMapBlockState& block, const CRegionBasic* pRegion )
{
	// Given our coords at pt including pt.m_z
	// Get Height of statics/dynamics/terrain at/above given coordinates
	// do gravity here for the z.
	// What is the height that gravity would put me at should i step here ?
	//
	// ARGS:
	//  pt = the point of interest.
	//  pt.m_z = my starting altitude.
	//  block = track current state.
	//  block.m_CanFlags = what we can pass thru. doors, water, walls ?
	//		CAN_C_GHOST	= Moves thru doors etc.	- CAN_I_DOOR
	//		CAN_C_SWIM = walk thru water - CAN_I_WATER
	//		CAN_C_WALK = it is possible that i can step up. - CAN_I_PLATFORM
	//		CAN_C_PASSWALLS	= walk through all blocking items - CAN_I_BLOCK
	//		CAN_C_CLIMB = climb ladders etc. - CAN_I_CLIMB
	//		CAN_C_FIRE_IMMUNE = i can walk into lava etc. - CAN_I_FIRE
	//		CAN_C_FLY  = gravity does not effect me.
	//  pRegion = possible regional effects. (multi's)
	//
	// RETURN:
	//  block.m_Bottom.m_z = Our new height at pt.m_x,pt.m_y
	//  block.m_Bottom.wBlockFlags = our blocking flags at the given location. CAN_I_WATER,CAN_I_PLATFORM,CAN_I_DOOR,CAN_I_CLIMB,
	//    CAN_I_ROOF = i am covered from the sky.
	// NOTE: 
	//  what is the dynamic blocking object ? maybe the NPC will move it ?

	block.m_z = pt.m_z;

	CAN_TYPE wBlockThis = 0;	// what are the current blocking flags ?
	const CMulMapBlock* pMapBlock = GetMapBlock( pt );
	ASSERT(pMapBlock);

	{
		int iQty = pMapBlock->m_Statics.GetStaticQty();
		if ( iQty )  // no static items here.
		{
			int x2=pMapBlock->GetOffsetX(pt.m_x);
			int y2=pMapBlock->GetOffsetY(pt.m_y);
			for ( int i=0; i<iQty; i++ )
			{
				if ( ! pMapBlock->m_Statics.IsStaticPoint( i, x2, y2 ))
					continue;
				const CMulStaticItemRec* pStatic = pMapBlock->m_Statics.GetStatic( i );
				PNT_Z_TYPE z = pStatic->m_z;
				if ( ! block.IsUsableZ(z,PLAYER_HEIGHT))
					continue;

				// This static is at the coordinates in question.
				// enough room for me to stand here ?
				wBlockThis = 0;
				PNT_Z_TYPE zHeight = CItemDef::GetItemHeight( pStatic->GetDispID(), wBlockThis );
				block.CheckTile( wBlockThis, z, zHeight, pStatic->GetDispID() + TERRAIN_QTY );
			}
		}
	}

	// Any multi items here ? 
	if ( pRegion != NULL && pRegion->IsMatchType(REGION_TYPE_MULTI))
	{
		CItemPtr pItem = g_World.ItemFind(pRegion->GetUIDIndex());
		if ( pItem != NULL )
		{
			const CMulMultiType* pMulti = g_Cfg.GetMultiItemDefs( pItem->GetDispID());
			if ( pMulti )
			{
				int x2 = pt.m_x - pItem->GetTopPoint().m_x;
				int y2 = pt.m_y - pItem->GetTopPoint().m_y;

				int iQty = pMulti->GetItemCount();
				for ( int i=0; iQty--; i++ )
				{
					const CMulMultiItemRec* pMultiItem = pMulti->GetItem(i);
					ASSERT(pMultiItem);

					if ( ! pMultiItem->m_visible )
						continue;
					if ( pMultiItem->m_dx != x2 || pMultiItem->m_dy != y2 )
						continue;

					PNT_Z_TYPE zitem = pItem->GetTopZ() + pMultiItem->m_dz;
					if ( ! block.IsUsableZ(zitem,PLAYER_HEIGHT))
						continue;

					wBlockThis = 0;
					PNT_Z_TYPE zHeight = CItemDef::GetItemHeight( pMultiItem->GetDispID(), wBlockThis );
					block.CheckTile( wBlockThis, zitem, zHeight, pMultiItem->GetDispID() + TERRAIN_QTY );
				}
			}
		}
	}

	{
		// Any dynamic items here ?
		// NOTE: This could just be an item that an NPC could just move ?
		CWorldSearch Area( pt );
		for(;;)
		{
			CItemPtr pItem = Area.GetNextItem();
			if ( pItem == NULL )
				break;

			PNT_Z_TYPE zitem = pItem->GetTopZ();
			if ( ! block.IsUsableZ(zitem,PLAYER_HEIGHT))
				continue;

			// Invis items should not block ???
			CItemDefPtr pItemDef = pItem->Item_GetDef();
			ASSERT(pItemDef);
			block.CheckTile(
				pItemDef->Can( CAN_I_DOOR | CAN_I_WATER | CAN_I_CLIMB | CAN_I_BLOCK | CAN_I_PLATFORM ),
				zitem, pItemDef->GetHeight(), pItemDef->GetDispID() + TERRAIN_QTY );
		}
	}

	// Check Terrain here.
	// Terrain height is screwed. Since it is related to all the terrain around it.

	{
		const CMulMapMeter* pMeter = pMapBlock->GetTerrain( SPHEREMAP_BLOCK_OFFSET(pt.m_x), SPHEREMAP_BLOCK_OFFSET(pt.m_y));
		ASSERT(pMeter);

		if ( block.IsUsableZ(pMeter->m_z,0))
		{
			if ( pMeter->m_wTerrainIndex == TERRAIN_HOLE )
				wBlockThis = 0;
			else if ( pMeter->m_wTerrainIndex == TERRAIN_NULL )	// inter dungeon type.
				wBlockThis = CAN_I_BLOCK;
			else
			{
				CMulTerrainInfo land( pMeter->m_wTerrainIndex );
				wBlockThis = CMulMapBlockState::GetBlockFromTileData( land.m_flags );
				if ( wBlockThis == 0 )
					wBlockThis = CAN_I_PLATFORM;
			}
			block.CheckTile( wBlockThis, pMeter->m_z, 0, pMeter->m_wTerrainIndex );
		}
	}

	if ( block.m_Bottom.m_z == SPHEREMAP_SIZE_MIN_Z )
	{
		block.m_Bottom = block.m_Lowest;
	}

#ifdef _DEBUG
	if ( g_Cfg.m_wDebugFlags & 0x08 )
	{
		CGString sMsg;
		sMsg.Format( "New space from z=%d is over(z=%d,n='%s',b=0%x), under(z=%d,n='%s',b=0%x)" LOG_CR,
			block.m_z,

			block.m_Bottom.m_z,	// the z we would stand on,
			(LPCTSTR) CMulMapBlockState::GetTileName(block.m_Bottom.m_wTile),
			block.m_Bottom.m_BlockFlags, // The bottom item has these blocking flags.

			block.m_Top.m_z,	// the z we would stand on,
			(LPCTSTR) CMulMapBlockState::GetTileName(block.m_Top.m_wTile),
			block.m_Top.m_BlockFlags // The bottom item has these blocking flags.

			);
		DEBUG_MSG(( sMsg ));
	}
#endif
}


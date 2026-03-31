//
// CSectorTemplate.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"
#include "spherecommon.h"
#include "spheremul.h"
#include "cregionmap.h"
#include "csectortemplate.h"

#ifdef SPHERE_SVR

////////////////////////////////////////////////////////////////////////
// -CCharsActiveList

void CCharsActiveList::SetWakeStatus( MAPPLANE_TYPE iMapPlane )
{
	m_timeLastClient.InitTimeCurrent();	// mark time in case it's the last client
	m_dwMapPlaneClients |= GetPlaneMask(iMapPlane);
}

void CCharsActiveList::OnRemoveOb( CGObListRec* pObRec )
{
	// Override this = called when removed from group.
	CCharPtr pChar = STATIC_CAST(CChar,pObRec);
	ASSERT( pChar );
	DEBUG_CHECK( pChar->IsTopLevel());
	if ( pChar->IsClient())
	{
		ClientDetach();
	}
	CGObList::OnRemoveOb(pObRec);
	// DEBUG_CHECK( pChar->GetParent() == NULL );
}

void CCharsActiveList::AddCharToSector( CChar* pChar )
{
	ASSERT( pChar );
	// ASSERT( pChar->m_pt.IsValid());
	if ( pChar->IsClient())
	{
		ClientAttach();
		SetWakeStatus( pChar->GetTopMap());
	}
	CGObList::InsertHead(pChar);
}

//////////////////////////////////////////////////////////////
// -CItemList

bool CItemsList::sm_fNotAMove = false;

void CItemsList::OnRemoveOb( CGObListRec* pObRec )
{
	// Item is picked up off the ground. (may be put right back down though)
	CItemPtr pItem = STATIC_CAST(CItem,pObRec);
	ASSERT( pItem );
	DEBUG_CHECK( pItem->IsTopLevel());
	DEBUG_CHECK( pItem->GetTopPoint().IsValidPoint());

	if ( ! sm_fNotAMove )
	{
		pItem->OnMoveFrom();	// IT_MULTI, IT_SHIP and IT_COMM_CRYSTAL
	}

	CGObList::OnRemoveOb(pObRec);
	//DEBUG_CHECK( pItem->GetParent() == NULL ); // might be deleted
}

void CItemsList::AddItemToSector( CItemPtr pItem )
{
	// Add to top level.
	// Either MoveTo() or SetTimeout is being called.
	ASSERT( pItem );
	CGObList::InsertHead( pItem );
}

#endif	// SPHERE_SVR

//////////////////////////////////////////////////////////////////
// -CSectorTemplate

CSectorTemplate::CSectorTemplate() :
	CResourceObj( GetIndex())
{
}

CSectorTemplate::~CSectorTemplate()
{
}

int CSectorTemplate::GetIndex() const
{
	CSectorPtr pSectorZero = g_World.GetSector(0);
	int i = (((BYTE*)this) - ((BYTE*)pSectorZero));
	i /= sizeof(CSector);
	ASSERT( i>=0 && i < SECTOR_QTY );
	return( i );
}

CPointMap CSectorTemplate::GetBasePoint() const
{
	// What is the coord base of this sector. upper left point.
	int index = GetIndex();
	ASSERT( index >= 0 && index < SECTOR_QTY );
	CPointMap pt(( index % SECTOR_COLS )* SECTOR_SIZE_X, ( index / SECTOR_COLS )* SECTOR_SIZE_Y );
	return( pt );
}

bool CSectorTemplate::IsMapCacheActive() const
{
	return m_MapBlockCache.GetSize() > 0 ;
}

void CSectorTemplate::CheckMapBlockCache( int iAge )
{
	// Clean out the sectors map cache if it has not been used recently.
	// iAge == 0 = delete all.

	int iQty = m_MapBlockCache.GetSize();
	for ( int i=0; i<iQty; i++ )
	{
		CMulMapBlock* pMapBlock = m_MapBlockCache.ElementAt(i);
		ASSERT(pMapBlock);
		if ( iAge <= 0 || pMapBlock->m_timeCache.GetCacheAge() >= iAge )
		{
			m_MapBlockCache.RemoveAt(i);
			i--;
			iQty--;
		}
	}
}

#ifdef SPHERE_MAP
const CMulMapColorBlock* CSectorTemplate::GetMapColorBlock( const CPointMap& pt )
{
	return STATIC_CAST(const CMulMapColorBlock,GetMapBlock( pt ));
}
#endif

const CMulMapBlock* CSectorTemplate::GetMapBlock( const CPointMap& pt )
{
	// Get a map block from the cache. load it if not.

	ASSERT( pt.IsValidXY());
	CPointMap pntBlock( SPHEREMAP_BLOCK_ALIGN(pt.m_x), SPHEREMAP_BLOCK_ALIGN(pt.m_y), 0, pt.m_mapplane );
	ASSERT( m_MapBlockCache.GetSize() <= (SPHEREMAP_BLOCK_SIZE* SPHEREMAP_BLOCK_SIZE));

	CMulMapBlock* pMapBlock=NULL;
	HASH_INDEX dwHashIndex = pntBlock.GetHashCode();

	// Find it in cache.
	int i = m_MapBlockCache.FindKey(dwHashIndex);
	if ( i >= 0 )
	{
		pMapBlock = m_MapBlockCache.ElementAt(i);
		ASSERT(pMapBlock);
		pMapBlock->m_timeCache.InitTimeCurrent();
		return( pMapBlock );
	}

	// else load it.
	try
	{
#ifdef SPHERE_MAP
		pMapBlock = new CMulMapColorBlock( pntBlock );
#else
		pMapBlock = new CMulMapBlock( pntBlock );
#endif
	}
#ifdef SPHERE_SVR
	SPHERE_LOG_TRY_CATCH( "GetMapBlock" )
#else
	catch (CGException& e)
	{
		// throw e;
		return( NULL );
	}
	catch (...)
	{
		return( NULL );
	}
#endif

	// Add it to the cache.
	ASSERT(pMapBlock);
	if ( pMapBlock )
	{
		m_MapBlockCache.AddSortKey( pMapBlock, dwHashIndex );
	}
	return( pMapBlock );
}

bool CSectorTemplate::IsInDungeonRegion() const
{
	// Is this sector in a dungeon or underground ?
	// Used for light / weather calcs.

	CPointMap pt = GetMidPoint();
	CRegionPtr pRegion = GetRegion( pt, REGION_TYPE_AREA );
	ASSERT(pRegion);
	return( pRegion->IsFlag(REGION_FLAG_UNDERGROUND));
}

CRegionPtr CSectorTemplate::GetRegion( const CPointMapBase& pt, DWORD dwType ) const
{
	// Does it match the mask of types we care about ?
	// ASSUME: sorted so that the smallest are first.
	//
	// REGION_TYPE_AREA => RES_Area = World region area only = CRegionComplex
	// REGION_TYPE_ROOM => RES_Room = NPC House areas only = CRegionBasic.
	// REGION_TYPE_MULTI => RES_WorldItem = UID linked types in general = CRegionComplex

	int iQty = m_RegionLinks.GetSize();
	for ( int i=0; i<iQty; i++ )
	{
		CRegionPtr pRegion = m_RegionLinks[i];
		ASSERT(pRegion);

		if ( ! pRegion->IsMatchType(dwType))
			continue;
		if ( ! pRegion->IsPointInside( pt ))
			continue;
		return( pRegion );
	}
	return( NULL );
}

void CSectorTemplate::UnloadRegions()
{
	m_Teleports.RemoveAll();

#ifdef SPHERE_SVR
	CCharPtr pChar = m_Chars_Active.GetHead();
	for ( ; pChar != NULL; pChar = pChar->GetNext())
	{
		pChar->m_pArea.ReleaseRefObj();
	}
#endif

	m_RegionLinks.RemoveAll();
}

bool CSectorTemplate::UnLinkRegion( CRegionBasic* pRegionOld, bool fRetestChars )
{
	// NOTE: What about unlinking it from all the CChar(s) here ? m_pArea
	ASSERT(pRegionOld);

	// Find and remove the region from our links
	{
		bool bFound = false;
		for (int i = m_RegionLinks.GetCount() - 1; i >= 0; i--)
		{
			if (&m_RegionLinks.ElementAt(i) == pRegionOld)
			{
				m_RegionLinks.RemoveAt(i);
				bFound = true;
				break;
			}
		}
		if (!bFound)
			return false;
	}

#ifdef SPHERE_SVR
	if ( fRetestChars )
	{
		CCharPtr pChar = m_Chars_Active.GetHead();
		for ( ; pChar != NULL; pChar = pChar->GetNext())
		{
			if ( ! pChar->m_pArea.IsValidRefObj() || pChar->m_pArea == pRegionOld )
			{
				pChar->MoveToRegionReTest( REGION_TYPE_MULTI | REGION_TYPE_AREA );
			}
		}
	}
#endif

	return true;
}

bool CSectorTemplate::LinkRegion( CRegionBasic* pRegionNew )
{
	// link in a region. may have just moved !
	// Make sure the smaller regions are first in the array !
	// Later added regions from the MAP file should be the smaller ones,
	//  according to the old rules.

	ASSERT(pRegionNew);
	ASSERT( pRegionNew->IsOverlapped( GetRect()));
	int iQty = m_RegionLinks.GetSize();

#ifdef _DEBUG
	if ( pRegionNew->m_pt.m_x == 2420 && pRegionNew->m_pt.m_y == 472 )
	{
		ASSERT(pRegionNew->GetRefCount());
	}
#endif

	for ( int i=0; i<iQty; i++ )
	{
		CRegionPtr pRegion = m_RegionLinks[i];
		ASSERT(pRegion);
		ASSERT(pRegion->GetRefCount() >= 2 );
		if ( pRegionNew == pRegion )
		{
			DEBUG_ERR(( "region already linked!" LOG_CR ));
			return false;
		}

		if ( pRegion->IsOverlapped(pRegionNew))
		{
			if ( pRegion->IsEqualRegion( pRegionNew ) && 
				pRegion->m_pt.IsSameMapPlane( pRegionNew->m_pt.m_mapplane ))
			{
				DEBUG_ERR(( "Conflicting region!" LOG_CR ));
				return( false );
			}
			if ( pRegionNew->IsInside(pRegion))	
				continue;

			// must insert before this.
			m_RegionLinks.InsertAt( i, pRegionNew );
			return( true );
		}

		DEBUG_CHECK( iQty >= 1 );
	}

	m_RegionLinks.Add( pRegionNew );
	return( true );
}

CTeleportPtr CSectorTemplate::GetTeleport2d( const CPointMap& pt ) const
{
	// Any teleports here at this 2d point ?

	int i = m_Teleports.FindKey( pt.GetHashCode());
	if ( i < 0 )
		return( NULL );
	return m_Teleports.ConstElementAt(i);
}

CTeleportPtr CSectorTemplate::GetTeleport( const CPointMap& pt ) const
{
	// Any teleports here at this point ?
	// factor in z coords.

	CTeleportPtr pTeleport = GetTeleport2d( pt );
	if ( pTeleport == NULL )
		return( NULL );

	int zdiff = pt.m_z - pTeleport->m_ptSrc.m_z;
	if ( ABS(zdiff) > 5 )
		return( NULL );

	// Check m_mapplane ?
	if ( ! pTeleport->m_ptSrc.IsSameMapPlane( pt.m_mapplane ))
		return( NULL );

	return( pTeleport );
}

bool CSectorTemplate::AddTeleport( CTeleport* pTeleport )
{
	// NOTE: can't be 2 teleports in the same place !
	// ASSERT( Teleport is actually in this sector !

	int i = m_Teleports.FindKey( pTeleport->GetHashCode());
	if ( i >= 0 )
	{
		// Not already me?
		DEBUG_ERR(( "Conflicting teleport %s!" LOG_CR, (LPCTSTR) pTeleport->m_ptSrc.v_Get() ));
		return( false );
	}
	m_Teleports.AddSortKey( pTeleport, pTeleport->GetHashCode());
	return( true );
}


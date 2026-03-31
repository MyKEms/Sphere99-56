//
// CSectorTemplate.h
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#ifndef _INC_CSECTOR_H
#define _INC_CSECTOR_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "CResourceObj.h"
#include "cobjbasetemplate.h"
#include "cMulMap.h"
#include "cregionmap.h"

class CCharsList : public CGObListType<CChar>
{
	// Inactive (disconnected) CChar (s)
	// Use this just for validation purposes.
};

class CCharsActiveList : public CGObListType<CChar>
{
#ifdef SPHERE_SVR
private:
	int	   m_iClients;			// How many clients in this sector now?
public:
	DWORD  m_dwMapPlaneClients;	// What planes are we active on.
	CServTime m_timeLastClient;	// age the sector based on last client here.
protected:
	virtual void OnRemoveOb( CGObListRec* pObRec );	// Override this = called when removed from list.
public:
	int HasClients() const { return( m_iClients ); }
	void ClientAttach()
	{
		m_iClients++;
	}
	void ClientDetach()
	{
		DEBUG_CHECK(m_iClients>0);
		m_iClients--;
	}
	DWORD GetPlaneMask( MAPPLANE_TYPE iMapPlane ) const
	{
		return( _1BITMASK(iMapPlane & 0x3f));
	}
	void SetWakeStatus( MAPPLANE_TYPE iMapPlane );
	bool IsAwakePlane( MAPPLANE_TYPE iMapPlane ) const
	{
		return( m_dwMapPlaneClients & GetPlaneMask(iMapPlane));
	}
	void AddCharToSector( CChar* pChar );

	CCharsActiveList()
	{
		m_iClients = 0;
		m_dwMapPlaneClients = 0;
	}
#endif
};

class CItemsList : public CGObListType<CItem>
{
#ifdef SPHERE_SVR
	// Top level list of items.
	// ??? Sort by point ?
public:
	// we are just setting a timer ? not actually moving it.
	static bool sm_fNotAMove;	// hack flag to prevent items from bouncing around too much.
protected:
	virtual void OnRemoveOb( CGObListRec* pObRec );	// Override this = called when removed from list.
public:
	void AddItemToSector( CItemPtr pItem );
#endif
};

class CSectorTemplate : public CResourceObj	// square region of the world.
{
public:
	CSectorTemplate();
	~CSectorTemplate();

	// Location map units.
	int GetIndex() const;
	CPointMap GetBasePoint() const;
	CPointMap GetMidPoint() const
	{
		CPointMap pt = GetBasePoint();
		pt.m_x += SECTOR_SIZE_X/2;	// East
		pt.m_y += SECTOR_SIZE_Y/2;	// South
		return( pt );
	}
	CRectMap GetRect() const
	{
		// Get a rectangle for the sector.
		CPointMap pt = GetBasePoint();
		CRectMap rect;
		rect.m_left = pt.m_x;
		rect.m_top = pt.m_y;
		rect.m_right = pt.m_x + SECTOR_SIZE_X;	// East
		rect.m_bottom = pt.m_y + SECTOR_SIZE_Y;	// South
		return( rect );
	}

	// CItem and CChar objects.
	void UnloadObjects()
	{
		// Used by SPHERE_MAP
		m_Items_Timer.Empty();
		m_Chars_Active.Empty();
	}

	// CMulMapBlock
	bool IsMapCacheActive() const;
	void CheckMapBlockCache( int iAge );
	const CMulMapBlock* GetMapBlock( const CPointMap & pt );
	const CMulMapColorBlock* GetMapColorBlock( const CPointMap & pt );

	// CRegionBasic
	int GetRegionCount() const
	{
		return m_RegionLinks.GetSize();
	}
	CRegionPtr GetRegion( const CPointMapBase& pt, DWORD dwType ) const;
	bool UnLinkRegion( CRegionBasic* pRegionOld, bool fRetestChars );
	bool LinkRegion( CRegionBasic* pRegionNew );

	void UnloadRegions();
	bool IsInDungeonRegion() const;	// in case weather etc is effected.

	// CTeleport(s) in the region.
	CTeleportPtr GetTeleport( const CPointMap & pt ) const;
	CTeleportPtr GetTeleport2d( const CPointMap & pt ) const;
	bool AddTeleport( CTeleport* pTeleport );

public:
	CGRefArray<CRegionBasic> m_RegionLinks;		// CRegionBasic(s) overlapping this CSector.
	CHashArray<CTeleport> m_Teleports;	// CTeleport array

	// Search for items and chars in a region must check 4 quandrants around location.
	CCharsActiveList m_Chars_Active;		// CChar(s) in this CSector.
	CCharsList m_Chars_Disconnect;	// Idle player characters. Dead NPC's and ridden horses.
	CItemsList m_Items_Timer;	// CItem(s) in this CSector that need timers.
	CItemsList m_Items_Inert;	// CItem(s) in this CSector. (no timer required)
private:
	CHashArray<CMulMapBlock> m_MapBlockCache;	// CMulMapBlock Cache Map Stuff. Max of 8*8=64 items in here. from MAP0.MUL file.
};

#endif // _INC_CSECTOR_H

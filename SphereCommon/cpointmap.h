//
// CPointMap.h
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#ifndef _INC_CPOINTMAP_H
#define _INC_CPOINTMAP_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "cstring.h"
#include "cpointbase.h"
#include "cregion.h"
#include "spheremul.h"

class CMulMap;
typedef CMulMap* CMulMapPtr;
class CSector;
typedef CSector* CSectorPtr;
class CRegionBasic;
typedef CRefPtr<CRegionBasic> CRegionPtr;

#define SECTOR_SIZE_X	64	// in meters.
#define SECTOR_SIZE_Y	64

#define SECTOR_COLS	( UO_SIZE0_X / SECTOR_SIZE_X ) // 96=
#define SECTOR_ROWS	( UO_SIZE0_Y / SECTOR_SIZE_Y ) // 64=
#define SECTOR_QTY	( SECTOR_COLS * SECTOR_ROWS ) // 6144=0x1800

struct CPointMapBase : public CGPointBase
{
	// UnInit point.
	// Coordinate the point within the CWorld, CMulMap and CSector.

#define REGION_TYPE_AREA  1		// RES_Area CRegionComplex
#define REGION_TYPE_ROOM  2		// RES_Room CRegionBasic
#define REGION_TYPE_MULTI 4		// RES_WorldItem
	CMulMapPtr GetMulMap() const;	// What VERFILE_MAP0 is this on ?
	CSectorPtr GetSector() const;
	CRegionPtr GetRegion( DWORD dwType ) const;
	bool IsSameMapPlane(BYTE mapplane) const { throw "not implemented"; }
	bool IsValidZ() const
	{
		return( m_z > SPHEREMAP_SIZE_MIN_Z && m_z < SPHEREMAP_SIZE_Z );
	}
	bool IsValidXY() const;
	bool IsValidPoint() const
	{
		return( IsValidXY() && IsValidZ());
	}
	bool IsCharValid() const;
	void MoveWrap( DIR_TYPE dir );
	void ValidatePoint();
	int GetDistZAdj( const CGPointBase& pt ) const
	{
		return( GetDistZ(pt) / (PLAYER_HEIGHT/2) );
	}
	void Move(DIR_TYPE dir) { throw "not implemented"; }
	void Move(short dx, short dy, short dz = 0) { m_x += dx; m_y += dy; m_z += dz; }
	void MoveN(DIR_TYPE dir, int amount) { throw "not implemented"; }

	bool operator==(const CPointMapBase& rhs) const { throw "not implemented"; }
	bool operator!=(const CPointMapBase& rhs) const { throw "not implemented"; }
};

struct CPointMap : public CPointMapBase
{
	// A point in the world (or in a container) (initialized)
	CPointMap& operator = ( POINT pt )
	{
		Set(pt);
		return( * this );
	}
	CPointMap& operator = ( POINTS pt )
	{
		Set(pt);
		return( * this );
	}
	CPointMap& operator = ( const CGPointBase& pt )
	{
		Set(pt);
		return( * this );
	}
	CPointMap( PNT_X_TYPE x, PNT_Y_TYPE y, PNT_Z_TYPE z = 0, MAPPLANE_TYPE map = 0 )
	{
		m_x = x;
		m_y = y;
		m_z = z;
		m_mapplane = map;
	}
	CPointMap( POINT pt )
	{
		Set(pt);
	}
	CPointMap( POINTS pt )
	{
		Set(pt);
	}
	CPointMap( const CGPointBase& pt )
	{
		Set( pt );
	}
	CPointMap( CGVariant& vVal )
	{
		v_Set( vVal );
	}
	CPointMap() 
	{
		InitPoint(); 
	}
	HASH_INDEX GetHashCode() const
	{
		HASH_INDEX hash = 3049 * m_x;
		hash *= 5039 + m_y;
		hash *= 883 + m_z;
		hash *= 9719 + m_mapplane;

		return hash;
	}

	bool IsSameMapPlane(BYTE mapplane) const { return m_mapplane == mapplane; }

	bool operator==(const CPointMap& rhs) const { throw "not implemented"; }
	bool operator!=(const CPointMap& rhs) const { throw "not implemented"; }
	void operator+=(const CPointMap& rhs) { throw "not implemented"; }
	void operator-=(const CPointMap& rhs) { throw "not implemented"; }
};


struct CRectMap : public CGRect
{
	// A rectangle on the map.
public:

	bool IsValid() const
	{
		int iSizeX = Width();
		if ( iSizeX < 0 || iSizeX > UO_SIZE0_X ) // map 0 ?
			return( false );
		int iSizeY = Height();
		if ( iSizeY < 0 || iSizeY > UO_SIZE0_Y ) // map 0 ?
			return( false );
		return( true );
	}

	// Make sure the rectangle is not out of bounds or backwards.
	// Keep in range for the selected map.
	void NormalizeRect();
	void NormalizeRectMax();
	CPointMap GetCenter() const { throw "not implemented"; }

	CSectorPtr GetSector( int i ) const;	// get all the sectors that make up this rect.

	void v_Set(CGVariant& val) { throw "not implemented"; }
};

#endif	// _INC_CPOINTMAP_H

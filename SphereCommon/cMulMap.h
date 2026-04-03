//
// CMulMap.H
// Copyright Menace Software (www.menasoft.com).
//

#ifndef _INC_CSPHEREMAP_H
#define _INC_CSPHEREMAP_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "cpointmap.h"
//#include "CArray.h"

//************************************************************

class CMulMap
{
	// RES_Map
	// Define a map plane in the world. 
	// Define the whole map. UO_SIZE0_X 
public:
	MAPPLANE_TYPE m_iMapNum;			// Map plane number for client.
	int m_iResourceLevel;			// 0=base, 2=T2A, 3=3rd dawn, 4=Black

	// ie. map0.mul, staidx0.mul, statics0.mul
	VERFILE_TYPE m_file;	// base of 3 files that specify the map. VERFILE_MAP0,VERFILE_STAIDX0,VERFILE_STATICS0
	VERFILE_TYPE m_filedif;	// base of 5 files VERFILEX_MAPDIFL0, VERFILEX_MAPDIF0, VERFILEX_STADIFL0, VERFILEX_STADIFI0, VERFILEX_STADIF0
	int m_iSizeX;			// x,y, size of the map. in meters. must be mult  of 64
	int m_iSizeY;
	int m_iSizeXWrap;		// Wierd mid point wrap on map0.mul.
	SEASON_TYPE m_Season;	// default season.
	// Name ?

	DWORD* m_pMapDiff;		// terrain changes.
	DWORD* m_pStatDiff;		// static items changes

public:
	~CMulMap();
	MAPPLANE_TYPE GetClientMap() const;
	int FindMapDiff( DWORD ulBlockIndex ) const;
	int FindStatDiff( DWORD ulBlockIndex ) const;
	void LoadDiffs();
};

extern CMulMap g_MulMap[8];

//************************************************************

class CMulStaticsBlock
{
	// A bunch of static items in a 8x8 block.
private:
	CGTypedArray<CMulStaticItemRec,CMulStaticItemRec&> m_Statics;
public:
	void LoadStatics( const CMulMap* pMap, DWORD dwBlockIndex );
public:
	int GetStaticQty() const
	{
		return( m_Statics.GetSize());
	}
	const CMulStaticItemRec* GetStatic( int i ) const
	{
		return( & m_Statics.ConstElementAt(i));
	}
	bool IsStaticPoint( int i, int xo, int yo ) const
	{
		ASSERT( xo >= 0 && xo < SPHEREMAP_BLOCK_SIZE );
		ASSERT( yo >= 0 && yo < SPHEREMAP_BLOCK_SIZE );
		const CMulStaticItemRec* pStatic = GetStatic(i);
		return( pStatic->m_x == xo && pStatic->m_y == yo );
	}
	CMulStaticsBlock()
	{
	}
	~CMulStaticsBlock()
	{
	}
};

//*****************************************************************
// For walk blocking.

	// Map Movement flags.
typedef WORD CAN_TYPE; 		// Store the mask here.

#define CAN_C_GHOST			0x0001	// Moves thru doors etc.
#define CAN_C_SWIM			0x0002	// dolphin, elemental or is water
#define CAN_C_WALK			0x0004	// Can walk on land, climbed on walked over else Frozen by nature(Corpser) or can just swim
#define CAN_C_PASSWALLS		0x0008	// Walk thru walls.
#define CAN_C_CLIMB			0x0010	// I can climb ladders, etc
#define CAN_C_FIRE_IMMUNE	0x0020	// Has some immunity to fire ? (will walk into it (lava))
#define CAN_C_INDOORS		0x0040	// Can go under roof. Not really used except to mask.
#define CAN_C_FLY			0x0080	// I can fly over most things. Mongbat, Bird etc.

#define CAN_I_DOOR			0x0001	// Is a door UFLAG4_DOOR
#define CAN_I_WATER			0x0002	// Need to swim in it. UFLAG1_WATER
#define CAN_I_PLATFORM		0x0004	// we can walk on top of it. (even tho the item itself might block) UFLAG2_PLATFORM
#define CAN_I_BLOCK			0x0008	// need to walk thru walls or fly over. UFLAG1_BLOCK
#define CAN_I_CLIMB			0x0010	// step up on it, UFLAG2_CLIMBABLE
#define CAN_I_FIRE			0x0020	// Is a fire. Ussually blocks as well. UFLAG1_DAMAGE
#define CAN_I_ROOF			0x0040	// We are under a roof. can't rain on us. UFLAG4_ROOF

	// Upper byte is type specific.
	// CItemDef specific defs.
#define CAN_I_PILE			0x0100	// Can item be piled UFLAG2_STACKABLE (*.mul)
#define CAN_I_DYE			0x0200	// Can item be dyed UFLAG3_CLOTH? (sort of)
#define CAN_I_FLIP			0x0400	// will flip by default.
#define CAN_I_LIGHT			0x0800	// UFLAG3_LIGHT
#define CAN_I_REPAIR		0x1000	// Is it repairable (difficulty based on value)
#define CAN_I_REPLICATE		0x2000	// Things like arrows are pretty much all the same.

	// CCharDef specific defs.
#define CAN_C_EQUIP			0x0100	// Can equip stuff. (humanoid)
#define CAN_C_USEHANDS		0x0200	// Can wield weapons (INT dependant), open doors ?, pick up/manipulate things
#define CAN_C_NOCORPSE		0x0400	// Creature leaves no corpse.
#define CAN_C_FEMALE		0x0800	// It is female by nature.
#define CAN_C_NONHUMANOID	0x1000	// Body type for combat messages.
#define CAN_C_RUN			0x2000	// Can run (not needed if they can fly)

struct CMulMapBlocker
{
	CAN_TYPE m_BlockFlags;	// How does this item block ? CAN_I_PLATFORM
	WORD m_wTile;			// TERRAIN_QTY + id.
	PNT_Z_TYPE m_z;		// Top of a solid object. or bottom of non blocking one.
};

struct CMulMapBlockState
{
	// Go through the list of stuff at this location to decide what is  blocking us and what is not.
	//  dwBlockFlags = what we can pass thru. doors, water, walls ?
	//		CAN_C_GHOST	= Moves thru doors etc.	- CAN_I_DOOR = UFLAG4_DOOR
	//		CAN_C_SWIM = walk thru water - CAN_I_WATER = UFLAG1_WATER
	//		CAN_C_WALK = it is possible that i can step up. - CAN_I_PLATFORM = UFLAG2_PLATFORM
	//		CAN_C_PASSWALLS	= walk through all blcking items - CAN_I_BLOCK = UFLAG1_BLOCK
	//		CAN_C_CLIMB  = can climb ladders etc. -  CAN_I_CLIMB = UFLAG2_CLIMBABLE
	//		CAN_C_FIRE_IMMUNE = i can walk into lava etc. - CAN_I_FIRE = UFLAG1_DAMAGE
	//		CAN_C_FLY  = gravity does not effect me.

	CAN_TYPE m_CanFlags;	// The block flags we can overcome.
	PNT_Z_TYPE m_iHeight;		// My height = (ie. needed to stand here)

	PNT_Z_TYPE m_z;		// the z we start at. (stay at if we are flying)

	CMulMapBlocker m_Top;		// What would be over my head.
	CMulMapBlocker m_Bottom;	// What i would be standing on.
	CMulMapBlocker m_Lowest;	// the lowest item we have found.

public:
	CMulMapBlockState( CAN_TYPE CanFlags=0, PNT_Z_TYPE iHeight = PLAYER_HEIGHT );

	static CAN_TYPE GetBlockFromTileData( DWORD dwFlags );
	static LPCTSTR GetTileName( WORD wID );

	void Init( CAN_TYPE CanFlags, PNT_Z_TYPE iHeight );

	bool IsUsableZ( PNT_Z_TYPE zBottom, PNT_Z_TYPE zHeightEstimate ) const
	{
		// Is this item in a range i care about?
		if ( zBottom > m_Top.m_z )	// above something that is already over my head.
			return( false );
		// NOTE: Assume multi overlapping items are not normal. so estimates are safe
		if ( zBottom + zHeightEstimate < m_Bottom.m_z )	// way below my feet
			return( false );
		return( true );
	}
	bool CheckTile( CAN_TYPE BlockFlags, PNT_Z_TYPE zBottom, PNT_Z_TYPE zheight, WORD wID );

	// Pass along my results.
	bool IsResultBlocked( CAN_TYPE CanFlags ) const
	{
		return( m_Bottom.m_BlockFlags &~ CanFlags );
	}
	bool IsResultBlocked() const
	{
		return( m_Bottom.m_BlockFlags &~ m_CanFlags );
	}
	bool IsResultCovered() const
	{
		// No weather. // we are covered by something.
		return( m_Top.m_BlockFlags );
	}
	PNT_Z_TYPE GetResultZ() const
	{
		if ( m_CanFlags & CAN_C_FLY )	// gravity has no effect on me.
		{
			// But i still cannot fly through solid objects !
			return( MAX( m_Bottom.m_z, m_z ));
		}
		return( m_Bottom.m_z );
	}
};

//*****************************************************************

class CMulMapBlock : public CMemDynamic, public CRefObjDef // Cache this from the MUL files. 8x8 block of the world.
{
	// A SPHEREMAP_BLOCK_SIZE (8x8) square block of meters.
protected:
	DECLARE_MEM_DYNAMIC;
private:
	static int sm_iCount;	// count total number of loaded blocks.
	CMulTerrainBlock m_Terrain;	// TERRAIN_TYPE and z levels.

public:
	// Block aligned of course.
	const CPointMap m_pt;	// The upper left corner. (ignore z) sort by this
	CMulStaticsBlock m_Statics;
	CServTime m_timeCache;	// keep track of the use time of this item. (client does not care about this)

private:
	// NOTE: This will "throw" on failure !
	void Load( const CMulMap* pMap, int bx, int by );

public:
	CMulMapBlock( const CPointMap& pt );
	CMulMapBlock( const CMulMap* pMap, int bx, int by );

	virtual ~CMulMapBlock()
	{
		sm_iCount--;
	}

	int GetOffsetX( int x ) const
	{
		// Allow this to go out of bounds.
		// ASSERT( ( x-m_pt.m_x) == SPHEREMAP_BLOCK_OFFSET(x));
		return( x - m_pt.m_x );
	}
	int GetOffsetY( int y ) const
	{
		return( y - m_pt.m_y );
	}

	HASH_INDEX GetHashCode() const
	{
		return( m_pt.GetHashCode() );
	}

	const CMulMapMeter* GetTerrain( int xoffset, int yoffset ) const
	{
		ASSERT( xoffset >= 0 && xoffset < SPHEREMAP_BLOCK_SIZE );
		ASSERT( yoffset >= 0 && yoffset < SPHEREMAP_BLOCK_SIZE );
		return( &( m_Terrain.m_Meter[ yoffset*SPHEREMAP_BLOCK_SIZE + xoffset ] ));
	}
	const CMulTerrainBlock* GetTerrainBlock() const
	{
		return( &m_Terrain );
	}
};

class CMulMapColorBlock : public CMulMapBlock
{
	// colorized CMulMapBlock
	// Client and map editor would use this.
protected:
	DECLARE_MEM_DYNAMIC;
private:
	WORD m_wMapColorMode;	// what stuff do i care about for m_MapColor.
	COLOR_TYPE m_MapColor[SPHEREMAP_BLOCK_SIZE*SPHEREMAP_BLOCK_SIZE];

public:
	static COLOR_TYPE GetMapColorItem( WORD idTile );
	static WORD GetMapColorMode( bool fMap, bool fStatics, bool fColor, PNT_Z_TYPE zclip );

	COLOR_TYPE GetMapColor( int xo, int yo ) const
	{
		ASSERT( xo >= 0 && xo < SPHEREMAP_BLOCK_SIZE );
		ASSERT( yo >= 0 && yo < SPHEREMAP_BLOCK_SIZE );
		return( m_MapColor[(yo*SPHEREMAP_BLOCK_SIZE) + xo] );
	}
	void UnloadMapColors()
	{
		m_wMapColorMode = 0;
	}
	void LoadMapColors( WORD wMode );

	CMulMapColorBlock( const CPointMap& pt );
};

inline WORD CMulMapColorBlock::GetMapColorMode( bool fMap, bool fStatics, bool fColor, PNT_Z_TYPE zclip )
{
	// Create the wMapColorMode mask.
	WORD wMapColorMode = (BYTE) zclip;
	if ( fMap ) wMapColorMode |= 0x1000;
	if ( fStatics ) wMapColorMode |= 0x2000;
	if ( fColor ) wMapColorMode |= 0x4000;
	return( wMapColorMode );
}

#endif // _INC_CMULMAP_H

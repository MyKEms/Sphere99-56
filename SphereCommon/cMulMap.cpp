//
// CMulMap.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"
#include "spherecommon.h"
#include "spheremul.h"
#include "cregionmap.h"
#include "cMulMap.h"
#include "cMulInst.h"

//****************************************************************

MAPPLANE_TYPE CMulMap::GetClientMap() const
{
	// How does the client want to refer to this ?
	return m_iMapNum; // ( m_iMapNum ) ? m_iMapNum : 1;	// 0 is map 1.
}

int CMulMap::FindMapDiff( DWORD ulBlockIndex ) const
{
	return -1;
}

int CMulMap::FindStatDiff( DWORD ulBlockIndex ) const
{
	return -1;
}

void CMulMap::LoadDiffs()
{

}

//////////////////////////////////////////////////////////////////
// -CMulMapBlockState

CMulMapBlockState::CMulMapBlockState( CAN_TYPE CanFlags, PNT_Z_TYPE iHeight )
{
	Init( CanFlags, iHeight );
}

void CMulMapBlockState::Init( CAN_TYPE CanFlags, PNT_Z_TYPE iHeight )
{
	m_CanFlags = CanFlags;
	m_iHeight = iHeight;

	m_z = SPHEREMAP_SIZE_Z;	// Set later.

	m_Top.m_BlockFlags = 0;
	m_Top.m_wTile = 0;
	m_Top.m_z = SPHEREMAP_SIZE_Z;	// the z of the item that would be over our head.

	m_Bottom.m_BlockFlags = CAN_I_BLOCK; // The bottom item has these blocking flags.
	m_Bottom.m_wTile = 0;
	m_Bottom.m_z = SPHEREMAP_SIZE_MIN_Z;	// the z we would stand on,

	m_Lowest.m_BlockFlags = CAN_I_BLOCK;
	m_Lowest.m_wTile = 0;
	m_Lowest.m_z = SPHEREMAP_SIZE_Z;
}

CAN_TYPE CMulMapBlockState::GetBlockFromTileData( DWORD dwFlags ) // static
{
	// Convert UFLAG1_BLOCK flags from tiledata.mul to CAN_TYPE
	CAN_TYPE BlockFlags = 0;
	if ( dwFlags & UFLAG1_WATER )
		BlockFlags |= CAN_I_WATER;
	if ( dwFlags & UFLAG1_BLOCK )
		BlockFlags |= CAN_I_BLOCK;
	if ( dwFlags & UFLAG2_PLATFORM )
		BlockFlags |= CAN_I_PLATFORM;
	if ( dwFlags & UFLAG2_CLIMBABLE )
		BlockFlags |= CAN_I_CLIMB;
	if ( dwFlags & UFLAG4_DOOR )
		BlockFlags |= CAN_I_DOOR;
	if ( dwFlags & UFLAG1_DAMAGE )
		BlockFlags |= CAN_I_FIRE;
	return BlockFlags;
}

LPCTSTR CMulMapBlockState::GetTileName( WORD wID )	// static
{
	// Terrain or item tile.
	if ( wID == 0 )
	{
		return( "<null>" );
	}
	TCHAR* pStr = Str_GetTemp();
	if ( wID < TERRAIN_QTY )
	{
		CMulTerrainInfo land( wID );
		strcpy( pStr, land.m_name );
	}
	else
	{
		wID -= TERRAIN_QTY;
		CMulItemInfo item( (ITEMID_TYPE) wID );
		strcpy( pStr, item.m_name );
	}
	return( pStr );
}

bool CMulMapBlockState::CheckTile( CAN_TYPE BlockFlags, PNT_Z_TYPE zBottom, PNT_Z_TYPE zHeight, WORD wID )
{
	// Will this item have some effect on me ? given BlockFlags
	// Is this tile above or below me.
	// RETURN:
	//  true = continue processing

	PNT_Z_TYPE zTop = zBottom;
	if ( BlockFlags & CAN_I_CLIMB )
		zTop += ( zHeight / 2 );	// standing position is half way up climbable items.
	else
		zTop += zHeight;

	if ( zTop < m_Bottom.m_z )	// below something i can already step on.
		return true;			// ignore it.

	if ( ! BlockFlags )	// no effect anyhow.
		return( true );

	// If this item does not block me at all then i guess it just does not count.
	if ( ! ( BlockFlags &~ m_CanFlags ))
	{	// this does not block me.
		if ( ! ( BlockFlags & CAN_I_CLIMB ))
		{
			if ( BlockFlags & CAN_I_PLATFORM )	// i can always walk on the platform.
			{
				zBottom = zTop;	// or i could walk under it.
			}
			else
			{
				zTop = zBottom;
			}
			// zHeight = 0;	// no effective height.
		}
	}

	if ( zTop < m_Lowest.m_z )
	{
		m_Lowest.m_BlockFlags = BlockFlags;
		m_Lowest.m_wTile = wID;
		m_Lowest.m_z = zTop;
	}

	// if i can't fit under this anyhow. it is something below me. (potentially)
	if ( zBottom < ( m_z + m_iHeight/2 ))
	{
		// This is the new item ( that would be ) under me.
		// NOTE: Platform items should take precendence over non-platforms.
		if ( zTop >= m_Bottom.m_z )
		{
			if ( zTop == m_Bottom.m_z )
			{
				if ( m_Bottom.m_BlockFlags & CAN_I_PLATFORM )
					return( true );
			}
			m_Bottom.m_BlockFlags = BlockFlags;
			m_Bottom.m_wTile = wID;
			m_Bottom.m_z = zTop;
		}
	}
	else
	{
		// I could potentially fit under this. ( it would be above me )
		if ( zBottom <= m_Top.m_z )
		{
			m_Top.m_BlockFlags = BlockFlags;
			m_Top.m_wTile = wID;
			m_Top.m_z = zBottom;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////
// -CMulStaticsBlock

void CMulStaticsBlock::LoadStatics( const CMulMap* pMap, DWORD ulBlockIndex )
{
	// long ulBlockIndex = (bx*(UO_SIZE0_Y/SPHEREMAP_BLOCK_SIZE) + by);
	// NOTE: What is index.m_wVal3 and index.m_wVal4 in VERFILE_STAIDX0 ?
	ASSERT( GetStaticQty() <= 0 );

	CMulIndexRec index;
	VERFILE_TYPE datafile;

	int iMapDiff = pMap->FindStatDiff(ulBlockIndex);
	if (iMapDiff>=0)
	{
		if ( ! g_MulInstall.ReadMulIndex( (VERFILE_TYPE)( pMap->m_file+3 ), VERFILE_QTY, iMapDiff, index ))
			return;
		datafile = (VERFILE_TYPE)( pMap->m_filedif+4 );
	}
	else
	{
		if ( ! g_MulInstall.ReadMulIndex( (VERFILE_TYPE)( pMap->m_file+1 ), VERFILE_QTY, ulBlockIndex, index ))	// (VERFILE_TYPE)( pMap->m_file+2 )
			return;
		datafile = (VERFILE_TYPE)( pMap->m_file+2 );
	}

	m_Statics.SetCount( index.GetBlockLength() / sizeof( CMulStaticItemRec ));
	ASSERT( GetStaticQty());

	if ( ! g_MulInstall.ReadMulData( datafile, index, m_Statics.GetBasePtr() ))
	{
		throw CGException(LOGL_CRIT, CGFile::GetLastError(), "CMulMapBlock: Read fStatics0");
	}
}

//////////////////////////////////////////////////////////////////
// -CMulMapBlock

int CMulMapBlock::sm_iCount = 0;

CMulMapBlock::CMulMapBlock( const CPointMap& pt ) :
	m_pt( pt )	// The upper left corner.
{
	sm_iCount++;
	ASSERT( ! SPHEREMAP_BLOCK_OFFSET(pt.m_x));
	ASSERT( ! SPHEREMAP_BLOCK_OFFSET(pt.m_y));
	Load( pt.GetMulMap(), pt.m_x/SPHEREMAP_BLOCK_SIZE, pt.m_y/SPHEREMAP_BLOCK_SIZE );
}

CMulMapBlock::CMulMapBlock( const CMulMap* pMap, int bx, int by ) :
	m_pt( bx * SPHEREMAP_BLOCK_SIZE, by * SPHEREMAP_BLOCK_SIZE )
{
	sm_iCount++;
	Load( pMap, bx, by );
}

void CMulMapBlock::Load( const CMulMap* pMap, int bx, int by )
{
	// Read in all the terrain and statics data for this block.

	ASSERT(pMap);

	m_timeCache.InitTime();		// This is invalid !

	ASSERT( bx < (pMap->m_iSizeX/SPHEREMAP_BLOCK_SIZE) );
	ASSERT( by < (pMap->m_iSizeY/SPHEREMAP_BLOCK_SIZE) );

	DWORD ulBlockIndex = (bx*(pMap->m_iSizeY/SPHEREMAP_BLOCK_SIZE) + by);

	CMulIndexRec index;
	VERFILE_TYPE datafile;

	int iMapDiff = pMap->FindMapDiff(ulBlockIndex);
	if ( iMapDiff >= 0 )
	{
		index.SetupIndex( iMapDiff * sizeof(CMulTerrainBlock), sizeof(CMulTerrainBlock));
		datafile = pMap->m_file;
	}
	else
	{
		// VERDATA not used !
		// if ( g_MulVerData.FindVerDataBlock( pMap->m_file, ulBlockIndex, index )) {} 
		index.SetupIndex( ulBlockIndex * sizeof(CMulTerrainBlock), sizeof(CMulTerrainBlock));
		datafile = pMap->m_file;
	}
	if ( ! g_MulInstall.ReadMulData( datafile, index, &m_Terrain ))
	{
		memset( &m_Terrain, 0, sizeof( m_Terrain ));
		throw CGException(LOGL_CRIT, CGFile::GetLastError(), "CMulMapBlock: Seek Ver");
	}

	m_Statics.LoadStatics( pMap, ulBlockIndex );

	m_timeCache.InitTimeCurrent();		// validate.
}

//************************************************************

CMulMapColorBlock::CMulMapColorBlock( const CPointMap& pt ) :
	CMulMapBlock(pt)	
{
	// figure out the map colors here just once when loaded.
	memset( m_MapColor, 0, sizeof(m_MapColor));
	m_wMapColorMode = 0;
}

COLOR_TYPE CMulMapColorBlock::GetMapColorItem( WORD idTile ) // static
{
	// ??? Cache this !
	// Do radar color translation on this terrain or item tile.
	// NOTE: this is HOrridly SLOW !
	// Do radar color translation on this terrain or item tile.
	// color index for ITEMID_TYPE id = TERRAIN_QTY + id
	//

	LONG lOffset = idTile * (LONG) sizeof(WORD);
	if ( g_MulInstall.m_File[VERFILE_RADARCOL].Seek( lOffset, SEEK_SET ) != lOffset )
	{
		return( 0 );
	}
	COLOR_TYPE wColor;
	if ( g_MulInstall.m_File[VERFILE_RADARCOL].Read( &wColor, sizeof(wColor)) != sizeof(wColor))
	{
		return( 0 );
	}
	return( wColor );
}

void CMulMapColorBlock::LoadMapColors( WORD wMode )
{
	// wMode = set by GetMapColorMode()

	m_timeCache.InitTimeCurrent();		// validate.

	if ( m_wMapColorMode == wMode )	// we already have a valid color map here.
		return;
	m_wMapColorMode = wMode;

	PNT_Z_TYPE zclip = (PNT_Z_TYPE)(BYTE)(wMode);
	PNT_Z_TYPE zsort[SPHEREMAP_BLOCK_SIZE*SPHEREMAP_BLOCK_SIZE];

	if ( wMode & 0x1000 )	// map terrain base.
	{
		int n=0;
		for ( int y2=0; y2<SPHEREMAP_BLOCK_SIZE; y2++ )
		{
			for ( int x2=0; x2<SPHEREMAP_BLOCK_SIZE; x2++, n++ )
			{
				const CMulMapMeter* pMapMeter = GetTerrain(x2,y2);

				// Get the base map blocks.
				int z = pMapMeter->m_z;
				if ( z <= zclip )
				{
					zsort[n] = z;
					m_MapColor[n] = pMapMeter->m_wTerrainIndex;
				}
				else
				{
					zsort[n] = SPHEREMAP_SIZE_MIN_Z;
					m_MapColor[n] = 0;
				}
			}
		}
	}
	else
	{
		for ( int n=0; n<SPHEREMAP_BLOCK_SIZE*SPHEREMAP_BLOCK_SIZE; n++ )
		{
			zsort[n] = SPHEREMAP_SIZE_MIN_Z;
			m_MapColor[n] = 0;
		}
	}

	if ( wMode & 0x2000 )	// statics
	{
		int iQty = m_Statics.GetStaticQty();
		for ( int i=0; i<iQty; i++ )
		{
			const CMulStaticItemRec* pStatics = m_Statics.GetStatic(i);

			int n = (pStatics->m_y * SPHEREMAP_BLOCK_SIZE) + pStatics->m_x;
			if ( pStatics->m_z >= zsort[n] &&
				pStatics->m_z <= zclip )
			{
				zsort[n] = pStatics->m_z;
				m_MapColor[n] = TERRAIN_QTY + pStatics->GetDispID();
			}
		}
	}

	if ( wMode & 0x4000 )	// map color translation
	{
		// Do the color translation for the block.
		for ( int n=0; n<SPHEREMAP_BLOCK_SIZE*SPHEREMAP_BLOCK_SIZE; n++ )
		{
			m_MapColor[n] = GetMapColorItem( m_MapColor[n] );
		}
	}
}

bool CMulMapMeter::IsTerrainWater() const
{
	// IT_WATER
	switch ( m_wTerrainIndex )
	{
	case TERRAIN_WATER1:
	case TERRAIN_WATER2:
	case TERRAIN_WATER3:
	case TERRAIN_WATER4:
	case TERRAIN_WATER5:
	case TERRAIN_WATER6:
		return( true );
	}
	return( false );
}

bool CMulMapMeter::IsTerrainRock() const
{
	// Is the terrain tile (not item tile) minable ?
	// IT_ROCK
	static const TERRAIN_TYPE sm_Terrain_OreBase[] =
	{
		TERRAIN_ROCK1, TERRAIN_ROCK2,
		0x00ec, 0x00f7,
		0x00fc, 0x0107,
		0x010c, 0x0117,
		0x011e, 0x0129,
		0x0141, 0x0144,
		0x021f, 0x0243,
		0x024a, 0x0257,
		0x0259, 0x026d
	};

	for ( int i=0;i<COUNTOF(sm_Terrain_OreBase);i+=2)
	{
		if ( m_wTerrainIndex >= sm_Terrain_OreBase[i] &&
			m_wTerrainIndex <= sm_Terrain_OreBase[i+1])
			return true;
	}
	return false;
}

bool CMulMapMeter::IsTerrainDirt() const
{
	// IT_DIRT - furrows

	if ( m_wTerrainIndex >= 0x09 && m_wTerrainIndex <= 0x015 )
		return( true );
	if ( m_wTerrainIndex >= 0x0150 && m_wTerrainIndex <= 0x015c )
		return( true );

	return( false );
}

bool CMulMapMeter::IsTerrainGrass() const
{
	// Is the terrain tile valid for grazing?
	// This is used with the Hunger AI
	// IT_GRASS
	static const TERRAIN_TYPE sm_Terrain_GrassBase[] =
	{
		0x0003, 0x0006,
		0x007d, 0x008c,
		0x00c0, 0x00db,
		0x00f8, 0x00fb,
		0x015d, 0x0164,
		0x01a4, 0x01a7,
		0x0231, 0x0234,
		0x0239, 0x0243,
		0x0324, 0x032b,
		0x036f, 0x0376,
		0x037b, 0x037e,
		0x03bf, 0x03c6,
		0x03cb, 0x3ce,
		0x0579, 0x0580,
		0x058b, 0x058c,
		0x05e3, 0x0604,
		0x066b, 0x066e,
		0x067d, 0x0684,
		0x0695, 0x069c,
		0x06a1, 0x06c2,
		0x06d2, 0x06d9,
		0x06de, 0x06e1
	};

	for ( int i=0;i<COUNTOF(sm_Terrain_GrassBase);i+=2)
	{
		if ( m_wTerrainIndex >= sm_Terrain_GrassBase[i] &&
			m_wTerrainIndex <= sm_Terrain_GrassBase[i+1] )
			return true;
	}
	return false;
}


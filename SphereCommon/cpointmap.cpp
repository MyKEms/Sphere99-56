//
// CPointMap.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"
#include "spherecommon.h"
#include "spheremul.h"
#include "cpointmap.h"
#include "cMulMap.h"

CMulMap g_MulMap[8] = // { ClientMapPlane, ClientResourceLevel, }
{
	{ 0, 0, VERFILE_MAP0, VERFILEX_MAPDIFL0, UO_SIZE0_X, UO_SIZE0_Y, UO_SIZE0_X_REAL, SEASON_Nice 	},	// Felucca 
	{ 1, 2, VERFILE_MAP0, VERFILEX_MAPDIFL1, UO_SIZE0_X, UO_SIZE0_Y, UO_SIZE0_X_REAL, SEASON_Desolate },	// Trammel
	{ 2, 3, VERFILE_MAP2, VERFILEX_MAPDIFL2, UO_SIZE2_X, UO_SIZE2_Y, UO_SIZE2_X,	  SEASON_Nice	},	// Ilshenar
	{ 0, 5, VERFILE_MAP0, VERFILE_QTY,		 UO_SIZE0_X, UO_SIZE0_Y, UO_SIZE0_X_REAL, SEASON_Spring	},
	{ 0, 5, VERFILE_MAP0, VERFILE_QTY,		 UO_SIZE0_X, UO_SIZE0_Y, UO_SIZE0_X_REAL, SEASON_Spring	},
	{ 0, 5, VERFILE_MAP0, VERFILE_QTY,		 UO_SIZE0_X, UO_SIZE0_Y, UO_SIZE0_X_REAL, SEASON_Spring	},
	{ 0, 5, VERFILE_MAP0, VERFILE_QTY,		 UO_SIZE0_X, UO_SIZE0_Y, UO_SIZE0_X_REAL, SEASON_Spring	},
	{ 0, 5, VERFILE_MAP0, VERFILE_QTY,		 UO_SIZE0_X, UO_SIZE0_Y, UO_SIZE0_X_REAL, SEASON_Spring	},
};

CMulMap::~CMulMap()
{
}

//******************************************************

CMulMap* CPointMapBase::GetMulMap() const
{
	if ( m_mapplane >= COUNTOF(g_MulMap))
		return( &g_MulMap[0] );
	return( &g_MulMap[m_mapplane] );
}

void CPointMapBase::ValidatePoint()
{
	const CMulMap* pMulMap = GetMulMap();
	ASSERT(pMulMap);
	if ( m_x < 0 ) 
		m_x = 1;
	if ( m_y < 0 ) 
		m_y = 1;
	if ( m_x >= pMulMap->m_iSizeX ) 
		m_x = pMulMap->m_iSizeX-2;
	if ( m_y >= pMulMap->m_iSizeY ) 
		m_y = pMulMap->m_iSizeY-2;
}

void CPointMapBase::MoveWrap( DIR_TYPE dir )
{
	// Check for world wrap.
	// CPointMapBase ptOld = *this;
	Move(dir);

#if 0
	CMulMap* pMulMap = GetMulMap();
	ASSERT(pMulMap);

	if (m_y < 0)
		m_y += pMulMap->m_iSizeY;
	if (m_x < 0)
		m_x += pMulMap->m_iSizeXWrap;
	if (m_y >= pMulMap->m_iSizeY)
		m_y -= pMulMap->m_iSizeY;
	if (m_x >= pMulMap->m_iSizeX)
		m_x -= pMulMap->m_iSizeX;
	if (m_x == pMulMap->m_iSizeXWrap)
		m_x = 0;
#else
	ValidatePoint();
#endif

}

bool CPointMapBase::IsValidXY() const
{
	CMulMap* pMulMap = GetMulMap();
	ASSERT(pMulMap);
	if ( m_x < 0 || m_x >= pMulMap->m_iSizeX )
		return( false );
	if ( m_y < 0 || m_y >= pMulMap->m_iSizeY )
		return( false );
	return( true );
}

bool CPointMapBase::IsCharValid() const
{
	// Can a char stand here ?
	CMulMap* pMulMap = GetMulMap();
	ASSERT(pMulMap);
	if ( ! IsValidZ())
		return( false );
	if ( m_x <= 0 || m_x >= pMulMap->m_iSizeX )
		return( false );
	if ( m_y <= 0 || m_y >= pMulMap->m_iSizeY )
		return( false );
	return( true );
}

#if defined(SPHERE_SVR) || defined(SPHERE_MAP)	// server only.

CSectorPtr CPointMapBase::GetSector() const
{
	// Get the world Sector we are in.
	return( g_World.GetSector( *this ));
}

CRegionPtr CPointMapBase::GetRegion( DWORD dwType ) const
{
	// What region in the current CSector am i in ?
	// We only need to update this every 8 or so steps ?
	// REGION_TYPE_AREA

	CSectorPtr pSector = GetSector();
	if ( pSector == NULL )
		return NULL;
	return( pSector->GetRegion( *this, dwType ));
}

//*************************************************************************
// -CRectMap

void CRectMap::NormalizeRect()
{
	CGRect::NormalizeRect();
	NormalizeRectMax();
}

void CRectMap::NormalizeRectMax()
{
	CGRect::NormalizeRectMax( UO_SIZE0_X, UO_SIZE0_Y );
}

CSectorPtr CRectMap::GetSector( int i ) const	// get all the sectors that make up this rect.
{
	// get all the CSector(s) that overlap this rect.
	// RETURN: NULL = no more

	// Align new rect.
	CRectMap rect;
	rect.m_left = m_left &~ (SECTOR_SIZE_X-1);
	rect.m_right = ( m_right | (SECTOR_SIZE_X-1)) + 1;
	rect.m_top = m_top &~ (SECTOR_SIZE_Y-1);
	rect.m_bottom = ( m_bottom | (SECTOR_SIZE_Y-1)) + 1;
	rect.NormalizeRectMax();

	int width = (rect.Width()) / SECTOR_SIZE_X;
	ASSERT(width<=SECTOR_COLS);
	int height = (rect.Height()) / SECTOR_SIZE_Y;
	ASSERT(height<=SECTOR_ROWS);

	int iBase = (( rect.m_top / SECTOR_SIZE_Y ) * SECTOR_COLS ) + ( rect.m_left / SECTOR_SIZE_X );

	if ( i >= ( height * width ))
	{
		if ( ! i )	// empty rect.
		{
			return( g_World.GetSector(iBase) );
		}
		return( NULL );
	}

	int indexoffset = (( i / width ) * SECTOR_COLS ) + ( i % width );

	return( g_World.GetSector(iBase+indexoffset) );
}

#endif

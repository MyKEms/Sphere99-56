//
// CMulVer.Cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"
#include "spherecommon.h"
#include "spheremul.h"
#include "cMulInst.h"
#include "cMulMap.h"

CMulVerData	g_MulVerData;	// changes to the existing mul files.

//////////////////////////////////////////////////////////////////////
// -CMulVerData

bool CMulVerData::Load( CGFile& file )
{
	// ARGS: file = m_File[VERFILE_QTY]
	// assume it is in sorted order.
	if ( GetSize())	// already loaded.
	{
		return true;
	}

	// #define g_fVerData g_MulInstall.m_File[VERFILE_VERDATA]

	if ( ! file.IsFileOpen())		// T2a might not have this.
		return true;

	file.SeekToBegin();
	DWORD dwQty;
	if ( file.Read( (void *) &dwQty, sizeof(dwQty)) <= 0 )
	{
		throw CGException( LOGL_CRIT, CGFile::GetLastError(), "VerData: Read Qty");
	}

	if ( ! dwQty )
		return true;

	SetCount( dwQty );

	LONG dwSizeRead = file.Read( (void *) GetBasePtr(), dwQty * sizeof( CMulVersionBlock ));
	if ( dwSizeRead <= 0 )
	{
		throw CGException( LOGL_CRIT, CGFile::GetLastError(), "VerData: Read");
	}

	dwSizeRead /= sizeof(CMulVersionBlock);
	if ( dwQty != dwSizeRead )
	{
		// NOTE: dwSizeRead might be less than expected ?

	}

	// Now sort it for fast searching.
	// Make sure it is sorted.
	// QSort(); // not implemented in stub

#if 0 // def _DEBUG
	if ( ! IsSorted())
	{
		DEBUG_ERR(( "VerData Array is NOT sorted !" LOG_CR ));
		throw CGException( LOGL_CRIT, (DWORD) -1, "VerData: NOT Sorted!");
	}
#endif

	return true;
}

bool CMulVerData::FindVerDataBlock( VERFILE_TYPE type, DWORD id, CMulIndexRec& Index ) const
{
	// Search the verdata.mul for changes to stuff.
	// search for a specific block.
	// assume it is in sorted order.
	// type = VERFILE_MAP0, VERFILE_STATICS0 have no verdata but use mapdiff instead!

	if ( type < 0 || type >= VERFILE_QTY )
		return false;

	HASH_INDEX dwHashIndex = VERDATA_MAKE_HASH(type,id);
	int i = FindKey( dwHashIndex );
	if ( i < 0 )
		return false;

	Index.CopyIndex( GetEntry(i));
	return( true );
}

//*********************************************8
// -CMulItemInfo

CMulItemInfo::CMulItemInfo( ITEMID_TYPE id )
{
	if ( id >= ITEMID_MULTI )
	{
		// composite/multi type objects.
		m_flags = 0;
		m_weight = 0xFF;
		m_layer = LAYER_NONE;
		m_dwAnim = 0;
		m_height = 0;
		strcpy( m_name, ( id <= ITEMID_SHIP6_W ) ? "ship" : "structure" );
		return;
	}

	VERFILE_TYPE filedata;
	long offset;
	CMulIndexRec Index;
	if ( g_MulVerData.FindVerDataBlock( VERFILE_TILEDATA, (id+TERRAIN_QTY)/UOTILE_BLOCK_QTY, Index ))
	{
		// 
		filedata = VERFILE_VERDATA;
		offset = Index.GetFileOffset() + offsetof( CUOItemTypeBlock, m_Tiles ) + (id%UOTILE_BLOCK_QTY) * sizeof(CUOItemTypeRec);
		ASSERT( Index.GetBlockLength() >= sizeof( CUOItemTypeRec ));
	}
	else
	{
		filedata = VERFILE_TILEDATA;
		offset = UOTILE_TERRAIN_SIZE + 4 + (( id / UOTILE_BLOCK_QTY ) * 4 ) + ( id * sizeof( CUOItemTypeRec ));
	}

	if ( g_MulInstall.m_File[filedata].Seek( offset, SEEK_SET ) != offset )
	{
		throw CGException(LOGL_CRIT, CGFile::GetLastError(), "CTileItemType.ReadInfo: TileData Seek");
	}

	if ( g_MulInstall.m_File[filedata].Read( STATIC_CAST(CUOItemTypeRec,this), sizeof(CUOItemTypeRec)) <= 0 )
	{
		throw CGException(LOGL_CRIT, CGFile::GetLastError(), "CTileItemType.ReadInfo: TileData Read");
	}
}

CMulTerrainInfo::CMulTerrainInfo( TERRAIN_TYPE id )
{
	ASSERT( id < TERRAIN_QTY );

	VERFILE_TYPE filedata;
	long offset;
	CMulIndexRec Index;
	if ( g_MulVerData.FindVerDataBlock( VERFILE_TILEDATA, id/UOTILE_BLOCK_QTY, Index ))
	{
		// offsetof(CUOTerrainTypeBlock,m_Tiles[id%UOTILE_BLOCK_QTY])
		filedata = VERFILE_VERDATA;
		offset = Index.GetFileOffset() + sizeof(DWORD) + (sizeof(CUOTerrainTypeRec)*(id%UOTILE_BLOCK_QTY));
		ASSERT( Index.GetBlockLength() >= sizeof( CUOTerrainTypeRec ));
	}
	else
	{
		filedata = VERFILE_TILEDATA;
		offset = 4 + (( id / UOTILE_BLOCK_QTY ) * 4 ) + ( id * sizeof( CUOTerrainTypeRec ));
	}

	if ( g_MulInstall.m_File[filedata].Seek( offset, SEEK_SET ) != offset )
	{
		throw CGException(LOGL_CRIT, CGFile::GetLastError(), "CTileTerrainType.ReadInfo: TileData Seek");
	}

	if ( g_MulInstall.m_File[filedata].Read( STATIC_CAST(CUOTerrainTypeRec,this), sizeof(CUOTerrainTypeRec)) <= 0 )
	{
		throw CGException(LOGL_CRIT, CGFile::GetLastError(), "CTileTerrainType.ReadInfo: TileData Read");
	}
}


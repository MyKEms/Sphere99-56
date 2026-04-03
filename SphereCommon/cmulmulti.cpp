//
// CMulMulti.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"
#include "spherecommon.h"
#include "cmulmulti.h"

//////////////////////////////////////////////////////////////////////
// CMulBlockType

void CMulBlockType::LoadMulData( VERFILE_TYPE filetype, const CMulIndexRec & Index )
{
	// Read the data block from the data file.
	Alloc( Index.GetBlockLength());
	if ( ! g_MulInstall.ReadMulData( filetype, Index, (void *)m_Data.GetData()))
	{
		throw CGException(LOGL_CRIT, E_FAIL, "LoadMulData: Read");
	}
	m_timeCache.InitTimeCurrent();
}

//////////////////////////////////////////////////////////////////////
// -CMulMultiType

CMulMultiType::CMulMultiType( HASH_INDEX dwHashIndex ) :
	CMulBlockType( dwHashIndex )
{
	assert( GetBaseIndex() < MULTI_QTY );
	Load();
}

CMulMultiType::CMulMultiType( MULTI_TYPE id ) :
	CMulBlockType( VERDATA_MAKE_HASH( VERFILE_MULTI, id ))
{
	// id = itemid_type - ITEMID_MULTI
	assert( GetBaseIndex() < MULTI_QTY );
	Load();
}

bool CMulMultiType::Load()
{
	// When this is placed on the ground etc.
	// Just load the whole thing.
	CMulIndexRec Index;
	if ( ! g_MulInstall.ReadMulIndex( VERFILE_MULTIIDX, VERFILE_MULTI, GetID(), Index ))
		return( false );
	LoadMulData( VERFILE_MULTI, Index );
	return( true );
}

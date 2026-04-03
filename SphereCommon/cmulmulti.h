//
// CMulMulti.h
// Copyright Menace Software (www.menasoft.com).
//

#ifndef _INC_CMULMULTI_H
#define _INC_CMULMULTI_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "CMemBlock.h"
#include "crefobj.h"
#include "cMulInst.h"

//**************************************************************************

class CMulBlockType : public CResourceObj
{
	// a generic resource block from one of the mul files.
	// CRefObjDef = How many active instances of this tile/resource item are in use.
	// CRefObjDef = only used by CMulCache
	// CUIDObjBase = VERDATA_MAKE_HASH
	DECLARE_MEM_DYNAMIC
private:
	CMemLenBlock m_Data;	// Store the MUL data. (exact format varies)
public:
	CServTime m_timeCache;	// For resources that are cached based on usage.

protected:
	bool IsLoaded() const
	{
		return( m_Data.IsValid());
	}
	void Alloc( DWORD dwSize )
	{
		assert( ! IsLoaded() );
		m_Data.Alloc( dwSize );	// size in BYTES will allow for some odd sized data. (anims)
	}
	void Free()
	{
		assert( ! GetRefCount() );	// make sure all refs are gone !
		m_Data.Free();
	}
	virtual bool Load() = 0;	// force the actual load of all the data here.
	WORD* GetLoadedData() const
	{
		// We force the assumption that the data is already loaded.
		ASSERT( IsLoaded());
		return (WORD*) m_Data.GetData();
	}

public:
	bool IsValidOffset( const void* pTest )
	{
		return( m_Data.IsValidOffset( pTest ));
	}

	int GetLoadDataSize() const
	{
		return m_Data.GetDataLength();
	}
	WORD* GetLoadData() 
	{
		if ( ! LoadCheck())
		{
			return( NULL );
		}
		return (WORD*) m_Data.GetData();
	}
	void LoadMulData( VERFILE_TYPE filedata, const CMulIndexRec& Index );

	bool LoadCheck()
	{
		// Make sure the full data is loaded as we are about to use it.
		if ( IsLoaded())
			return( true );
		return Load();
	}

	DWORD GetBaseIndex() const
	{
		return( VERDATA_GET_INDEX( GetUIDIndex() ));
	}
	VERFILE_TYPE GetFileType() const
	{
		return( VERDATA_GET_FILE( GetUIDIndex() ));
	}

	CMulBlockType( HASH_INDEX dwHashIndex ) :
		CResourceObj( dwHashIndex )	// VERDATA_MAKE_HASH
	{
	}
	virtual ~CMulBlockType()
	{
	}
};

//**************************************************************************

struct CMulMultiType : public CMulBlockType
{
	// A multi item. house, boat etc.
	// Load all the relevent info for the multi.
	// VERFILE_MULTI
protected:
	DECLARE_MEM_DYNAMIC;
private:
	bool Load();
public:
	virtual CGString GetName() const
	{
		return( TEXT("Multi")); // GetResourceName() ?
	}
	int GetItemCount() const
	{
		ASSERT( IsLoaded());
		return( GetLoadDataSize() / sizeof(CMulMultiItemRec));
	}
	void* GetData() const
	{
		CMulMultiItemRec* pData = (CMulMultiItemRec *) GetLoadedData();
		if ( pData == NULL )
			return( NULL );
		return (void *) &pData[0];
	}
	CMulMultiItemRec* GetItem( int i ) const
	{
		// Get a component item.
		CMulMultiItemRec* pData = (CMulMultiItemRec *) GetLoadedData();
		if ( pData == NULL )
			return( NULL );
		assert( i < GetItemCount());
		return( &pData[i] );			
	}
	CMulMultiItemRec* GetFirstVisibleItem() const
	{
		// Get the first component item.
		CMulMultiItemRec* pData = (CMulMultiItemRec *) GetLoadedData();
		if ( pData == NULL )
			return( NULL );
		assert( GetItemCount());
		return( &pData[0] );			
	}

	MULTI_TYPE GetID() const
	{
		return( (MULTI_TYPE) GetBaseIndex());
	}
	MULTI_TYPE GetMultiID() const
	{
		return( (MULTI_TYPE) GetBaseIndex());
	}

	CMulMultiType( HASH_INDEX dwHashIndex );
	CMulMultiType( MULTI_TYPE id );
};

#endif // _INC_CMULMULTI_H


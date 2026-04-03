//
// CObjBaseTemplate.H
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#ifndef _INC_COBJBASETEMPLATE_H
#define _INC_COBJBASETEMPLATE_H

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "cpointmap.h"

class CObjBase;
typedef CRefPtr<CObjBase> CObjBasePtr;
class CItem;
typedef CRefPtr<CItem> CItemPtr;
class CChar;
typedef CRefPtr<CChar> CCharPtr;

class CObjBaseTemplate : public CGObListRec, public CResourceObj
{
	// A dynamic object of some sort.
	// There should always be a CObjBase derived object later.
	// uid = How the server/client will refer to this. 0 = static item

public:
	DECLARE_LISTREC_REF2(CObjBaseTemplate);

private:
	CGString	m_sName;	// individual name for the object. (if it needs one, else use type name)
	CPointMap	m_pt;		// 3d location in the world. only use 2d if in container.

public:
	// Not saved. Just used for internal sorting.
#if defined(SPHERE_CLIENT) || defined(SPHERE_MAP)
	PNT_Z_TYPE m_z_sort;		// by what z should this be displayed..
	PNT_Z_TYPE m_z_top;		// z + height.
#endif

protected:
	void DupeCopy( const CObjBaseTemplate* pObj )
	{
		// NOTE: Never copy m_UID. but copy all else.
		ASSERT(pObj);
		m_sName = pObj->m_sName;
		m_pt = pObj->m_pt;
	}

	void SetUnkZ( PNT_Z_TYPE z )
	{
		m_pt.m_z = z;
	}

public:

	CSphereUID GetUID() const	// UID_F_ITEM
	{
		return( GetUIDIndex()); 
	}

	// Location

	virtual int IsWeird() const;

	virtual CObjBasePtr GetTopLevelObj( void ) const // get my parent or me.
	= 0;

#if defined(SPHERE_SVR)
	CSectorPtr GetTopSector() const
	{
		// Assume it is top level.
		return( GetTopPoint().GetSector());
	}
#endif

	bool IsDisconnected() const;	// not really placed in the world ?
	bool IsTopLevel() const;
	bool IsChar() const;
	bool IsItem() const;
	bool IsItemInContainer() const;
	bool IsItemEquipped() const;

#if defined(_DEBUG) 
	bool IsValidContainer() const;
#endif

	LAYER_TYPE GetEquipLayer() const
	{
		DEBUG_CHECK( IsItem());
		//DEBUG_CHECK( IsItemEquipped());
		return( (LAYER_TYPE) m_pt.m_z );
	}
	void SetEquipLayer( LAYER_TYPE layer )
	{
		DEBUG_CHECK( IsItem());
		//DEBUG_CHECK( IsItemEquipped());
		m_pt.m_x = 0;	// these don't apply.
		m_pt.m_y = 0;
		m_pt.m_z = layer; // layer equipped.
		m_pt.m_mapplane = MAPPLANE_DEF;
	}

	WORD GetContainedLayer() const
	{
		// used for corpse or Restock count as well in Vendor container.
		DEBUG_CHECK( IsItem());
		//DEBUG_CHECK( IsItemInContainer());
		return( MAKEWORD( m_pt.m_z, m_pt.m_mapplane ));
	}
	void SetContainedLayer( WORD layer )
	{
		// used for corpse or Restock count as well in Vendor container.
		DEBUG_CHECK( IsItem());
		//DEBUG_CHECK( IsItemInContainer());
		m_pt.m_z = LOBYTE( layer );
		m_pt.m_mapplane = HIBYTE( layer );
	}

	const POINT GetContainedPoint() const
	{
		// xy POINT is all that really matters here.
		DEBUG_CHECK( IsItem());
		//DEBUG_CHECK( IsItemInContainer());
		return( m_pt );
	}
	void SetContainedPoint( const POINT& pt )
	{
		DEBUG_CHECK( IsItem());
		//DEBUG_CHECK( IsItemInContainer());
		m_pt.m_x = pt.x;
		m_pt.m_y = pt.y;
		m_pt.m_z = LAYER_NONE;
		m_pt.m_mapplane = MAPPLANE_DEF;
	}

	void SetTopPoint( const CPointMap& pt )
	{
		ASSERT( pt.IsValidPoint());	// already checked b4.
		DEBUG_CHECK( IsTopLevel() || IsDisconnected());
		m_pt = pt;
	}
	const CPointMap& GetTopPoint() const
	{
		// NOTE: ASSUME This is a Top level obj!
		DEBUG_CHECK( IsTopLevel() || IsDisconnected());
		return( m_pt );
	}
	void SetTopZ( PNT_Z_TYPE z )
	{
		// NOTE: ASSUME This is a Top level obj!
		DEBUG_CHECK( IsTopLevel() || IsDisconnected());
		m_pt.m_z = z;
	}
	PNT_Z_TYPE GetTopZ() const
	{
		// NOTE: ASSUME This is a Top level obj!
		DEBUG_CHECK( IsTopLevel() || IsDisconnected());
		return( m_pt.m_z );
	}
	MAPPLANE_TYPE GetTopMap() const
	{
		// NOTE: ASSUME This is a Top level obj!
		DEBUG_CHECK( IsTopLevel() || IsDisconnected());
		return( m_pt.m_mapplane );
	}

	void SetUnkPoint( const CPointMap& pt )
	{
		m_pt = pt;
	}
	const CPointMap& GetUnkPoint() const
	{
		// don't care where this
		return( m_pt );
	}
	PNT_Z_TYPE GetUnkZ() const
	{
		return( m_pt.m_z );
	}

	// Distance and direction
	int GetTopDist( const CPointMap& pt ) const
	{
		// NOTE: ASSUME This is a Top level obj!
		return( GetTopPoint().GetDist( pt ));
	}

	int GetTopDist( const CObjBaseTemplate* pObj ) const
	{
		// don't check for logged out.
		// NOTE: ASSUME This is a Top level obj!
		// Assume both already at top level.
		ASSERT( pObj );
		if ( pObj->IsDisconnected())
			return( SHRT_MAX );
		return( GetTopPoint().GetDist( pObj->GetTopPoint()));
	}

	int GetDist( const CObjBaseTemplate* pObj ) const;

	int GetTopDist3D( const CObjBaseTemplate* pObj ) const // 3D Distance between points
	{
		// logged out chars have infinite distance
		// NOTE: ASSUME This is a Top level obj!
		// Assume both already at top level.
		ASSERT( pObj );
		if ( pObj->IsDisconnected())
			return( SHRT_MAX );
		return( GetTopPoint().GetDist3D( pObj->GetTopPoint()));
	}

	DIR_TYPE GetTopDir( const CObjBaseTemplate* pObj, DIR_TYPE DirDefault = DIR_QTY ) const
	{
		// NOTE: ASSUME This is a Top level obj!
		ASSERT( pObj );
		return( GetTopPoint().GetDir( pObj->GetTopPoint(), DirDefault ));
	}

	DIR_TYPE GetDir( const CObjBaseTemplate* pObj, DIR_TYPE DirDefault = DIR_QTY ) const;

	virtual int GetVisibleDistance() const
	{
		// What is the average distance someone could see this at.
		// -1 = magically invisible. 0 = hiding very well.
		// default visual range. larger for large objects (multis).
		return( SPHEREMAP_VIEW_SIZE );
	}

	CRegionPtr GetTopRegion( DWORD dwRegionFlags ) const
	{
		// What region is this object in?
		// NOTE: ASSUME This is a Top level obj!
		return GetTopPoint().GetRegion( dwRegionFlags );
	}

	// Names
	LPCTSTR GetIndividualName() const
	{
		return( m_sName );
	}
	bool IsIndividualName() const
	{
		return( ! m_sName.IsEmpty());
	}
	virtual CGString GetName() const
	{
		// Might just be a type name if the individual has no name.
		return( m_sName );
	}
	virtual bool SetName( LPCTSTR pszName )
	{
		// NOTE: Name length <= MAX_NAME_SIZE
		ASSERT(pszName);
		m_sName = pszName;
		return true;
	}

	CObjBaseTemplate();
	virtual ~CObjBaseTemplate();
};

#endif // _INC_COBJBASETEMPLATE_H

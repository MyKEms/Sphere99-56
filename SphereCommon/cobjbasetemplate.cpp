//
// CObjBaseTemplate.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"
#include "spherecommon.h"
#include "cobjbasetemplate.h"

//************************************************************************
// -CObjBaseTemplate

CObjBaseTemplate::CObjBaseTemplate() : 
	CResourceObj(UID_INDEX_CLEAR) // fill this in later.
{
#if defined(SPHERE_CLIENT) || defined(SPHERE_MAP)
	m_z_sort = SPHEREMAP_SIZE_MIN_Z;
	m_z_top = SPHEREMAP_SIZE_MIN_Z;
#endif
}

CObjBaseTemplate::~CObjBaseTemplate()
{
}

bool CObjBaseTemplate::IsChar() const
{
	return( PTR_CAST(const CChar,this) != NULL );
}

bool CObjBaseTemplate::IsItem() const
{
	return( PTR_CAST(const CItem,this) != NULL );
}

bool CObjBaseTemplate::IsDisconnected() const
{
	if ( GetParent() == NULL )
		return true;

#ifdef SPHERE_SVR
#if defined(_DEBUG)
	ASSERT(IsValidContainer());
#endif

	if ( GetParent() == &g_World.m_ObjNew )
		return true;
	if ( GetParent() == &g_World.m_ObjDelete )
		return true;

	if ( IsChar())
	{
		return( PTR_CAST(CCharsList,GetParent()) != NULL );
	}
	// not valid for item to be disconnected. (except at create time)
	ASSERT(IsItem());
#endif

	return( false );
}

bool CObjBaseTemplate::IsTopLevel() const
{
#ifdef _DEBUG
	if ( GetParent())
	{
		ASSERT(IsValidContainer());
	}
#endif

#ifdef SPHERE_CLIENT
	return( PTR_CAST(CMapMeter,GetParent()) != NULL );
#endif

#if defined(SPHERE_MAP) || defined(SPHERE_SVR)
	if ( IsChar())
	{
		return( PTR_CAST(CCharsList,GetParent()) == NULL );
	}

	ASSERT(IsItem());
	if ( PTR_CAST(CItemsList,GetParent()) != NULL )
		return true;
#endif

	return( false );
}

bool CObjBaseTemplate::IsItemEquipped() const
{
#ifdef _DEBUG
	if ( GetParent())
	{
		ASSERT(IsValidContainer());
	}
#endif
	if ( ! IsItem())
	{
		return( false );
	}
	if ( PTR_CAST(CChar,GetParent()) != NULL )
		return true;
	return( false );
}

#if defined(SPHERE_CLIENT) || defined(SPHERE_SVR)

bool CObjBaseTemplate::IsItemInContainer() const
{
#ifdef _DEBUG
	if ( GetParent())
	{
		ASSERT(IsValidContainer());
	}
#endif
	if ( ! IsItem())
	{
		return( false );
	}
	if ( PTR_CAST(CItemContainer,GetParent()) != NULL )
		return true;
	return( false );
}

#endif

#if defined(_DEBUG) 

bool CObjBaseTemplate::IsValidContainer() const
{
	// Is the item lost ?
	if ( GetParent() == NULL )	// we must always have a parent.
	{
		return( false );
	}

#if defined(SPHERE_SVR)
	if ( GetParent() == &g_World.m_ObjNew )
		return true;
	if ( GetParent() == &g_World.m_ObjDelete )
		return true;

	if ( IsChar())
	{
		if ( PTR_CAST(CCharsList,GetParent()) != NULL )
			return true;
		return( PTR_CAST(CCharsActiveList,GetParent()) != NULL );
	}
	ASSERT( IsItem());
	if ( PTR_CAST(CItemsList,GetParent()) != NULL )
		return true;
	if ( PTR_CAST(CChar,GetParent()) != NULL )
		return true;
	if ( PTR_CAST(CItemContainer,GetParent()) != NULL )
		return true;
	return( false );
#endif
	return( true );
}

#endif

int CObjBaseTemplate::IsWeird() const
{
	ASSERT( IsValidDynamic());

	if ( GetParent() == NULL )
	{
		return( 0x3102 );
	}
#if defined(_DEBUG)
	if ( ! IsValidContainer())
	{
		return( 0x3103 );
	}
#endif
	if ( ! IsValidUID())
	{
		return( 0x3104 );
	}
	return( 0 );
}

int CObjBaseTemplate::GetDist( const CObjBaseTemplate* pObj ) const
{
	// logged out chars have infinite distance
	// Assume i am already at top level.
	// DEBUG_CHECK( IsTopLevel());
	if ( pObj == NULL )
		return( SHRT_MAX );
	CObjBasePtr pObjTop = pObj->GetTopLevelObj();
	if ( pObjTop->IsDisconnected())
		return( SHRT_MAX );
	return( GetTopDist( pObjTop ));
}

DIR_TYPE CObjBaseTemplate::GetDir( const CObjBaseTemplate* pObj, DIR_TYPE DirDefault ) const
{
	ASSERT( pObj );
	CObjBasePtr pObjTop = pObj->GetTopLevelObj();
	return( GetTopDir( pObjTop, DirDefault ));
}


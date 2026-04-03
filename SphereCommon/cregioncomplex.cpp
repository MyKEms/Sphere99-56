//
// CRegionComplex.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
// Common for client and server.
//

#include "stdafx.h"
#include "spherecommon.h"
#include "spheremul.h"
#include "cregionmap.h"
#include "cpointmap.h"

//*************************************************************************
// -CRegionComplex

const CScriptProp CRegionComplex::sm_Props[CRegionComplex::P_QTY+1] =	// static
{
#define CREGIONCOMPLEXPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "cregioncomplexprops.tbl"
#undef CREGIONCOMPLEXPROP
	NULL,
};

#ifdef USE_JSCRIPT
#define CREGIONCOMPLEXMETHOD(a,b,c) JSCRIPT_METHOD_IMP(CRegionComplex,a)
#include "cregioncomplexmethods.tbl"
#undef CREGIONCOMPLEXMETHOD
#endif

const CScriptMethod CRegionComplex::sm_Methods[CRegionComplex::M_QTY+1] =	// static
{
#define CREGIONCOMPLEXMETHOD(a,b,c) CSCRIPT_METHOD_IMP(a,b,c)
#include "cregioncomplexmethods.tbl"
#undef CREGIONCOMPLEXMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP1(RegionComplex,CRegionComplex::sm_Props,CRegionComplex::sm_Methods,NULL,RegionBasic);

CRegionComplex::CRegionComplex( CSphereUID rid, LPCTSTR pszName ) :
	CRegionBasic( rid, pszName )
{
}

CRegionComplex::~CRegionComplex()
{
}

HRESULT CRegionComplex::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CRegionBasic::s_PropGet( pszKey, vValRet, pSrc ));
	}

	switch (iProp)
	{
#ifdef SPHERE_SVR
	case P_Tag:	// "TAG" = get/set a local tag.
		return HRES_INVALID_FUNCTION;
	case P_Multi: // ref to multi (if this is one)
		if ( IsMultiRegion())
		{
			vValRet.SetRef( g_World.ObjFind( GetUIDIndex()));			
		}
		break;
	case P_Resources:
	case P_Events:
		m_Events.v_Get( vValRet );
		break;
#endif
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CRegionComplex::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	// Load the values for the region from script.

	s_FixExtendedProp( pszKey, "TAG", vVal );

	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CRegionBasic::s_PropSet( pszKey, vVal ));
	}

	switch (iProp)
	{
#ifdef SPHERE_SVR
	case P_Tag:	// get/set a local tag.
		return( m_TagDefs.s_PropSetTags( vVal ));
	case P_Multi:
		return HRES_WRITE_FAULT;
	case P_Resources:
	case P_Events:
		if ( ! m_Events.v_Set( vVal, RES_RegionType ))
			return HRES_BAD_ARGUMENTS;
		break;
#endif
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}

void CRegionComplex::s_WriteBody( CScript &s, LPCTSTR pszPrefix )
{
	CRegionBasic::s_WriteBody( s, pszPrefix );

#ifdef SPHERE_SVR
	CGString sName;

	if ( m_Events.GetSize())
	{
		CGVariant vVal;
		m_Events.v_Get( vVal );
		sName.Format( _TEXT("%sEVENTS"), (LPCTSTR) pszPrefix );
		s.WriteKey( sName, vVal );
	}

	// Write out any extra TAGS here.
	sName.Format( _TEXT("%sTAG.%%s"), (LPCTSTR) pszPrefix );
	m_TagDefs.s_WriteTags( s, sName );
#endif

}

void CRegionComplex::s_WriteProps( CScript &s )
{
	s.WriteSection( "AREA %s", GetName());
	s_WriteBase( s );
	s_WriteBody( s, "" );
}

HRESULT CRegionComplex::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp < 0 )
	{
		return( CRegionBasic::s_Method(pszKey, vArgs, vValRet, pSrc));
	}
	// CRegionComplex::M_Tag = get/set a local tag.
#ifdef SPHERE_SVR	// server only.
	return( m_TagDefs.s_MethodTags( vArgs, vValRet, pSrc ));
#else
	return HRES_UNKNOWN_PROPERTY;
#endif

}

#ifdef SPHERE_SVR	// server only.

TRIGRET_TYPE CRegionComplex::OnRegionTrigger( CScriptConsole* pSrc, CRegionType::T_TYPE_ iAction )
{
	// RETURN: true = halt processing (don't allow in this region)
	// [REGION 0]
	// ONTRIGGER=ENTER
	// ONTRIGGER=EXIT

	CSphereExpContext exec(this,pSrc);

	int iQty = m_Events.GetSize();
	for ( int i=0; i<iQty; i++ )
	{
		CResourceTrigPtr pLink = REF_CAST(CResourceTriggered,m_Events[i]);
		if ( pLink == NULL )
			continue;
		TRIGRET_TYPE iRet = pLink->OnTriggerScript( exec, iAction, CRegionType::sm_Triggers[iAction].m_pszName );
		if ( iRet != TRIGRET_RET_FALSE )
			return iRet;
	}

	return( TRIGRET_RET_DEFAULT );
}

const CRegionType* CRegionComplex::FindNaturalResource( int /* IT_TYPE*/ type ) const
{
	// Find the natural resources assinged to this region.
	// ARGS: type = IT_TYPE

	int iQty = m_Events.GetSize();
	for ( int i=0; i<iQty; i++ )
	{
		CResourceLinkPtr pLink = m_Events[i];
		ASSERT(pLink);
		if ( RES_GET_TYPE(pLink->GetUIDIndex()) != RES_RegionType )
			continue;
		if ( RES_GET_PAGE(pLink->GetUIDIndex()) != type )
			continue;
		return( REF_CAST(const CRegionType,pLink));
	}

	return( NULL );
}

#endif


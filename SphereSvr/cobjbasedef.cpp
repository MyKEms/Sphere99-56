//
// CObjBaseDef.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
// base classes.
//
#include "stdafx.h"	// predef header.

//****************************************************************
// -CObjBaseDef
//

const CScriptProp CObjBaseDef::sm_Props[CObjBaseDef::P_QTY+1] =
{
#define COBJBASEDEFPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "cobjbasedefprops.tbl"
#undef COBJBASEDEFPROP
	NULL,
};

CSCRIPT_CLASS_IMP1(ObjBaseDef,CObjBaseDef::sm_Props,NULL,NULL,ResourceTriggered);

CObjBaseDef::CObjBaseDef( CSphereUID id ) : CResourceTriggered( id )
{
	m_wDispIndex = 0;	// Assume nothing til told differently.
	m_CanFlags = CAN_C_INDOORS;	// most things can cover us from the weather.
	m_Height = PLAYER_HEIGHT;	// default height of object or creature.
}

CObjBaseDef::~CObjBaseDef()
{
}

HRESULT CObjBaseDef::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pChar )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp<0)
	{
		return( CResourceLink::s_PropGet(pszKey,vValRet,pChar));
	}
	switch (iProp)
	{
	case P_BaseId:
		vValRet = g_Cfg.ResourceGetName( GetResourceID());
		break;
	case P_Can:
		vValRet.SetDWORD( GetCanFlags());
		break;
	case P_Height:
		vValRet.SetInt( GetHeight());
		break;
	case P_Name:
		vValRet = GetName();
		break;
	case P_Resources:
		m_BaseResources.v_GetKeys( vValRet );
		break;
	case P_ResourceNames:
		m_BaseResources.v_GetNames( vValRet );
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CObjBaseDef::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp<0)
	{
		return( CResourceLink::s_PropSet(pszKey, vVal));
	}
	switch (iProp)
	{
	case P_BaseId:
		return( HRES_WRITE_FAULT );
	case P_Can:
		m_CanFlags = vVal.GetInt() | ( m_CanFlags & ( CAN_C_INDOORS|CAN_C_EQUIP|CAN_C_USEHANDS|CAN_C_NONHUMANOID ));
		break;
	case P_Name:
		SetTypeName(vVal);
		break;

	case P_Resources:
		m_BaseResources.s_LoadKeys( vVal.GetPSTR());
		break;
	case P_ResourceNames:
		return HRES_INVALID_FUNCTION;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}

void CObjBaseDef::CopyBasic( const CObjBaseDef* pBase )
{
	// do not copy CResourceLink

	if ( m_sName.IsEmpty())	// Base type name. should set this itself most times. (don't overwrite it!)
		m_sName = pBase->m_sName;

	m_wDispIndex = pBase->m_wDispIndex;

	m_Height = pBase->m_Height;
	// m_BaseResources = pBase->m_BaseResources;	// items might not want this.
	m_CanFlags = pBase->GetCanFlags();
}

void CObjBaseDef::CopyTransfer( CObjBaseDef* pBase )
{
	CopyLink( pBase );

	m_sName = pBase->m_sName;
	m_BaseResources.CopyArray( pBase->m_BaseResources);

	CopyBasic( pBase );
}


//
// CRegionType.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"
#include "spherecommon.h"

//*******************************************
// -CRegionType

const CScriptProp CRegionType::sm_Props[CRegionType::P_QTY+1] =
{
#define CREGIONTYPEPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "cregiontypeprops.tbl"
#undef CREGIONTYPEPROP
	NULL,
};

const CScriptProp CRegionType::sm_Triggers[CRegionType::T_QTY+1] =	// static
{
#define CREGIONEVENT(a,b,c) {"@" #a, b, c},
	CREGIONEVENT(AAAUNUSED,CSCRIPTPROP_NARGS|CSCRIPTPROP_RETNUL,NULL)	// reserved for XTRIG_UNKNOWN
#include "cregionevents.tbl"
#undef CREGIONEVENT
	NULL,
};

CSCRIPT_CLASS_IMP1(RegionType,CRegionType::sm_Props,NULL,CRegionType::sm_Triggers,ResourceLink);

int CRegionType::CalcTotalWeight()
{
	int iTotal = 0;
	int iQty = m_Members.GetSize();
	for ( int i=0; i<iQty; i++ )
	{
		iTotal += m_Members[i].GetResQty();
	}
	return( m_iTotalWeight = iTotal );
}

HRESULT CRegionType::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	// RES_Spawn or RES_RegionType
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp<0)
	{
		return( CResourceDef::s_PropSet(pszKey, vVal));
	}
	switch ( iProp )
	{
	case P_Id:	// "ID"
		{
			int iArgs = vVal.MakeArraySize();
			CResourceQty rec;
			rec.SetResourceID(
				g_Cfg.ResourceGetIDByName( RES_CharDef, vVal.GetArrayPSTR(0)),
				( iArgs > 1 ) ? vVal.GetArrayInt(1) : 1 );
			m_iTotalWeight += rec.GetResQty();
			m_Members.Add(rec);
		}
		break;

	case P_Resources:
		m_Members.s_LoadKeys( vVal.GetPSTR());
		CalcTotalWeight();
		break;

	case P_Weight: // Modify the weight of the last item.
		if ( ! m_Members.GetSize())
			return HRES_INVALID_HANDLE;
		{
			int iWeight = vVal.GetInt();
			m_Members.ElementAt( m_Members.GetSize()-1 ).SetResQty( iWeight );
			CalcTotalWeight();
		}
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

int CRegionType::GetRandMemberIndex() const
{
	int iCount = m_Members.GetSize();
	if ( ! iCount )
		return( -1 );
	int iWeight = Calc_GetRandVal( m_iTotalWeight ) + 1;
	int i=0;
	for ( ; iWeight > 0 && i<iCount; i++ )
	{
		iWeight -= m_Members[i].GetResQty();
	}
	return( i - 1);
}

//*******************************************
// -CRegionResourceDef

const CScriptProp CRegionResourceDef::sm_Props[CRegionResourceDef::P_QTY+1] =
{
#define CREGIONRESOURCEPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "cregionresourceprops.tbl"
#undef CREGIONRESOURCEPROP
	NULL,
};

CSCRIPT_CLASS_IMP1(RegionResourceDef,CRegionResourceDef::sm_Props,NULL,NULL,ResourceDef);

HRESULT CRegionResourceDef::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	// RES_RegionResource
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp<0)
	{
		return( CResourceDef::s_PropSet(pszKey, vVal));
	}
	switch (iProp)
	{
	case P_Amount:
		m_Amount.v_Set( vVal );
		break;
	case P_Reap:
		m_ReapItem = (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, vVal.GetPSTR());
		break;
	case P_ReapAmount:
		m_ReapAmount.v_Set(vVal);
		break;
	case P_Regen:
		m_iRegenerateTime = vVal.GetInt();	// TICKS_PER_SEC once found how long to regen this type.
		break;
	case P_Skill:
		m_Skill.v_Set(vVal);
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CRegionResourceDef::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	// RES_RegionResource
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp<0)
	{
		return( CResourceDef::s_PropGet( pszKey, vValRet, pSrc ));
	}
	switch (iProp)
	{
	case P_Amount:
		m_Amount.v_Get(vValRet);
		break;
		// case P_REAP:
	case P_ReapAmount:
		m_ReapAmount.v_Get(vValRet);
		break;
	case P_Regen:
		vValRet.SetInt( m_iRegenerateTime );
		break;
	case P_Skill:
		m_Skill.v_Get(vValRet);
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

CRegionResourceDef::CRegionResourceDef( CSphereUID rid ) :
	CResourceDef( rid )
{
	// set defaults first.
	m_iRegenerateTime = 0;	// TICKS_PER_SEC once found how long to regen this type.
}

CRegionResourceDef::~CRegionResourceDef()
{
}


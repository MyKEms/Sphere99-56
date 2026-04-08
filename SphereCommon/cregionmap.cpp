//
// CRegionBasic.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
// Common for client and server.
//

#include "stdafx.h"
#include "spherecommon.h"
#include "spheremul.h"
#include "cregionmap.h"

// Stub for missing function
static void s_CombineKeys(TCHAR* pszOut, LPCTSTR pszKey, LPCTSTR pszArg) {
	sprintf(pszOut, "%s.%s", pszKey, pszArg ? pszArg : "");
}
static bool HasSymbolicResourceName() { return false; }
#include "cpointmap.h"

//*************************************************************************
// -CRegionBasic

const CScriptProp CRegionBasic::sm_Props[CRegionBasic::P_QTY+1] =	// static
{
#define CREGIONPROP(a,b,c)  CSCRIPT_PROP_IMP(a,b,c)
#include "cregionprops.tbl"
#undef CREGIONPROP
	NULL,
};

#ifdef USE_JSCRIPT
#define CREGIONMETHOD(a,b,c) JSCRIPT_METHOD_IMP(CRegionBasic,a)
#include "cregionmethods.tbl"
#undef CREGIONMETHOD
#endif

const CScriptMethod CRegionBasic::sm_Methods[CRegionBasic::M_QTY+1] =
{
#define CREGIONMETHOD(a,b,c)  CSCRIPT_METHOD_IMP(a,b,c)
#include "cregionmethods.tbl"
#undef CREGIONMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP1(RegionBasic,CRegionBasic::sm_Props,CRegionBasic::sm_Methods,NULL,ResourceDef);

CRegionBasic::CRegionBasic( CSphereUID rid, LPCTSTR pszName ) :
	CResourceDef( rid )
{
	m_dwFlags = 0;
	m_RegionClass = RegionClass_Unspecified;
	if ( pszName )
		SetName( pszName );
}

CRegionBasic::~CRegionBasic()
{
	UnRealizeRegion(true);
}

void CRegionBasic::UnRealizeRegion( bool fRetestChars )
{
	// remove myself from the world. UnLink()
	// used in the case of a ship where the region will move.

	for ( int i=0;; i++ )
	{
		CSectorPtr pSector = GetSector(i);
		if ( pSector == NULL )
			break;
		if ( ! pSector->UnLinkRegion( this, fRetestChars ))
			continue;
	}
}

bool CRegionBasic::RealizeRegion()
{
	// Link the region to the world.
	// RETURN: false = not valid.

	if ( IsRegionEmpty())
	{
		return( false );
	}

#ifdef _DEBUG
	if ( m_pt.m_mapplane == 2 )
	{
		ASSERT( m_pt.m_mapplane == 2 );
	}
#endif

	if ( ! m_pt.IsValidPoint())
	{
		m_pt = GetRegionCorner( DIR_QTY );	// center
	}

	// Attach to all sectors that i overlap.
	for ( int i=0;; i++ )
	{
		CSectorPtr pSector = GetSector(i);
		if ( pSector == NULL )
			break;
		// Add it to the sector list.
		if ( ! pSector->LinkRegion( this ))
			return( false );
	}

	ASSERT(GetRefCount());

	return( true );
}

bool CRegionBasic::AddRegionRect( const CRectMap& rect )
{
	// Add an included rectangle to this region.
	if ( ! rect.IsValid())
	{
		return( false );
	}
	if ( ! CGRegion::AddRegionRect( rect ))
		return( false );

	// Need to call RealizeRegion now.?
	return( true );
}

bool CRegionBasic::IsPointInside( const CPointMap& pt ) const
{
	if ( ! PtInRegion( pt ))
		return false;
	// mapplane 255 = region applies to all map planes (Sphere 0.99 convention)
	if ( m_pt.m_mapplane != 255 && ! m_pt.IsSameMapPlane( pt.m_mapplane ))
		return false;
	// Have a z component ?
	if ( IsMatchType(REGION_TYPE_MULTI))
	{
		int zdiff = pt.m_z - m_pt.m_z;
		if ( zdiff < -3 || zdiff > 70 )	// NOTE: Tower is 66 high ?!
			return false;
	}
	return true;
}

bool CRegionBasic::IsMatchType( DWORD dwTypeMask ) const
{
#ifdef SPHERE_SVR
	// ASSERT( GetResourceID().IsValidUID());
	if ( IsMultiRegion())
	{
		if ( ! ( dwTypeMask& REGION_TYPE_MULTI ))
			return false;
	}
	else if ( RES_GET_TYPE(GetUIDIndex()) == RES_Area )
	{
		if ( ! ( dwTypeMask& REGION_TYPE_AREA ))
			return false;
	}
	else
	{
		if ( ! ( dwTypeMask & REGION_TYPE_ROOM ))
			return false;
	}
	return true;
#elif defined(SPHERE_MAP)
	// ASSERT( GetUIDIndex() == REGION_TYPE_AREA || GetUIDIndex() == REGION_TYPE_ROOM );
	return(( GetUIDIndex() & dwTypeMask ) ? true : false );
#else
	return false;
#endif
}

void CRegionBasic::SetName( LPCTSTR pszName )
{
#if defined(SPHERE_SVR)
	if ( pszName == NULL || pszName[0] == '%' )
	{
		m_sName = g_Serv.GetName();
		return;
	}
#endif
	if ( pszName == NULL || pszName[0] == '\0' )
	{
		m_sName = "Unnamed";
		return;
	}
#ifdef _DEBUG
	if ( ! m_sName.IsEmpty() && strcmp( m_sName, pszName ))
	{
		ASSERT(pszName[0]);
	}
#endif
	m_sName = pszName;
}

HRESULT CRegionBasic::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole * pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceDef::s_PropGet( pszKey, vValRet, pSrc ));
	}

	switch ( iProp )
	{
	case P_Group:	// ignore this.
	case P_ColdChance:
	case P_RainChance:
	case P_Rect:
		return( HRES_INVALID_FUNCTION );
	case P_Announce:
		vValRet.SetBool( IsFlag(REGION_FLAG_ANNOUNCE));
		break;
	case P_Arena:
		vValRet.SetBool( IsFlag(REGION_FLAG_ARENA));
		break;
	case P_Buildable:
		vValRet.SetBool( ! IsFlag(REGION_FLAG_NOBUILDING));
		break;
	case P_Class:
		vValRet.SetInt( m_RegionClass );
		break;
	case P_Flags:
		vValRet.SetDWORD( GetRegionFlags() );
		break;
	case P_FlagSafe:
		vValRet.SetBool( IsFlag(REGION_FLAG_SAFE));
		break;
	case P_Gate:
		vValRet.SetBool( ! IsFlag(REGION_ANTIMAGIC_GATE));
		break;
	case P_Guarded:
		vValRet.SetBool( IsFlag(REGION_FLAG_GUARDED));
		break;
	case P_Magic:
		vValRet.SetBool( ! IsFlag(REGION_ANTIMAGIC_ALL));
		break;
	case P_MagicDamage:
		vValRet.SetBool( ! IsFlag(REGION_ANTIMAGIC_DAMAGE));
		break;
	case P_MapPlane:
		vValRet.SetInt( m_pt.m_mapplane );
		break;
	case P_Mark:
	case P_RecallIn:
		vValRet.SetBool( ! IsFlag(REGION_ANTIMAGIC_RECALL_IN));
		break;
	case P_Name:
		// The previous name was really the DEFNAME ???
		vValRet = GetName();
		break;
	case P_NoBuild:
		vValRet.SetBool( IsFlag(REGION_FLAG_NOBUILDING));
		break;
	case P_NoDecay:
		vValRet.SetBool( IsFlag(REGION_FLAG_NODECAY));
		break;
	case P_NoPVP:
		vValRet.SetBool( IsFlag(REGION_FLAG_NO_PVP));
		break;
	case P_P:
		vValRet = CGVariant(m_pt.v_Get());
		break;
	case P_Recall:
	case P_RecallOut:
		vValRet.SetBool( ! IsFlag(REGION_ANTIMAGIC_RECALL_OUT));
		break;
	case P_Underground:
		vValRet.SetBool( IsFlag(REGION_FLAG_UNDERGROUND));
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}

HRESULT CRegionBasic::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceDef::s_PropSet( pszKey, vVal ));
	}
	switch ( iProp )
	{
	case P_Announce:
		TogRegionFlags( REGION_FLAG_ANNOUNCE, vVal.IsEmpty() || vVal.GetBool());
		break;
	case P_Arena:
		TogRegionFlags( REGION_FLAG_ARENA, vVal.GetBool());
		break;
	case P_Buildable:
		TogRegionFlags( REGION_FLAG_NOBUILDING, ! vVal.GetBool());
		break;
	case P_Class:
		m_RegionClass = (RegionClass_Type) vVal.GetInt();
		break;
#ifdef SPHERE_SVR
	case P_ColdChance:
	case P_RainChance:
		{
			TCHAR szCmd[128];
			s_CombineKeys(szCmd,pszKey,vVal.GetPSTR());
			SendSectorsCommand( szCmd, &g_Serv );
		}
		break;
#endif
	case P_Flags:
		m_dwFlags = ( vVal.GetInt() &~REGION_FLAG_SHIP ) | ( m_dwFlags & REGION_FLAG_SHIP );
		break;
	case P_Gate:
		TogRegionFlags( REGION_ANTIMAGIC_GATE, ! vVal.GetBool());
		break;
	case P_Group:	// ignore this.
		break;
	case P_Guarded:
		TogRegionFlags( REGION_FLAG_GUARDED, vVal.IsEmpty() || vVal.GetBool());
		break;
	case P_Magic:
		TogRegionFlags( REGION_ANTIMAGIC_ALL, ! vVal.GetBool());
		break;
	case P_MagicDamage:
		TogRegionFlags( REGION_ANTIMAGIC_DAMAGE, ! vVal.GetBool());
		break;
	case P_MapPlane:
		m_pt.m_mapplane = vVal.GetInt();
		break;
	case P_Mark:
	case P_RecallIn:
		TogRegionFlags( REGION_ANTIMAGIC_RECALL_IN, ! vVal.GetBool());
		break;
	case P_Name:
		SetName( vVal.GetPSTR());
		break;
	case P_NoDecay:
		TogRegionFlags( REGION_FLAG_NODECAY, vVal.GetBool());
		break;
	case P_NoBuild:
		TogRegionFlags( REGION_FLAG_NOBUILDING, vVal.GetBool());
		break;
	case P_NoPVP:
		TogRegionFlags( REGION_FLAG_NO_PVP, vVal.GetBool());
		break;
	case P_P:
		m_pt.v_Set( vVal );
		break;
	case P_Recall:
	case P_RecallOut:
		TogRegionFlags( REGION_ANTIMAGIC_RECALL_OUT, ! vVal.GetBool());
		break;
	case P_Rect:
		{
			CRectMap rect;
			rect.v_Set( vVal );
			if ( ! AddRegionRect( rect ))
				return HRES_BAD_ARGUMENTS;
		}
		break;
	case P_FlagSafe:
		TogRegionFlags( REGION_FLAG_SAFE, vVal.GetBool());
		break;
	case P_Underground:
		TogRegionFlags( REGION_FLAG_UNDERGROUND, vVal.GetBool());
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

void CRegionBasic::s_WriteBody( CScript& s, LPCTSTR pszPrefix )
{
	CGString sName;
	if ( m_RegionClass != RegionClass_Unspecified )
	{
		sName.Format( _TEXT("%sCLASS"), (LPCTSTR) pszPrefix );
		s.WriteKeyInt( sName, m_RegionClass);
	}
	if ( GetRegionFlags())
	{
		sName.Format( _TEXT("%sFLAGS"), (LPCTSTR) pszPrefix );

#ifdef SPHERE_MAP
		// try to write flags out as: 
		// FLAGS=region_flag_guarded|region_flag_globalname|region_flag_nobuilding
		s.WriteKey( sName, g_Cfg.GetRegionFlagsDesc( GetRegionFlags() ));
#else
		s.WriteKeyDWORD( sName, GetRegionFlags());
#endif
	}
}

void CRegionBasic::s_WriteBase( CScript& s )
{
	if ( HasSymbolicResourceName())
	{
		s.WriteKey( _TEXT("DEFNAME"), GetResourceName() );
	}
	if ( m_pt.IsValidPoint())
	{
		s.WriteKey( _TEXT("P"), m_pt.v_Get());
	}
	else if ( m_pt.m_mapplane )
	{
		s.WriteKeyInt( _TEXT("MAPPLANE"), m_pt.m_mapplane );
	}
	int iQty = GetRegionRectCount();
	for ( int i=0; i<iQty; i++ )
	{
		s.WriteKey( _TEXT("RECT"), GetRegionRect(i).WriteRectStr());
	}
}

void CRegionBasic::s_WriteProps( CScript& s )
{
	s.WriteSection( "ROOM %s", GetName());
	s_WriteBase( s );
	s_WriteBody( s, "" );
}

bool CRegionBasic::CheckAntiMagic( SPELL_TYPE spell ) const
{
	// return: true = blocked.
	if ( ! IsFlag( REGION_FLAG_SHIP |
		REGION_ANTIMAGIC_ALL |
		REGION_ANTIMAGIC_RECALL_IN |
		REGION_ANTIMAGIC_RECALL_OUT |
		REGION_ANTIMAGIC_GATE |
		REGION_ANTIMAGIC_TELEPORT |
		REGION_ANTIMAGIC_DAMAGE ))	// no effects on magic anyhow.
		return( false );

	if ( IsFlag( REGION_ANTIMAGIC_ALL ))
		return( true );

	if ( IsFlag( REGION_ANTIMAGIC_RECALL_IN | REGION_FLAG_SHIP ))
	{
		if ( spell == SPELL_Mark || spell == SPELL_Gate_Travel )
			return( true );
	}
	if ( IsFlag( REGION_ANTIMAGIC_RECALL_OUT ))
	{
		if ( spell == SPELL_Recall || spell == SPELL_Gate_Travel || spell == SPELL_Mark )
			return( true );
	}
	if ( IsFlag( REGION_ANTIMAGIC_GATE ))
	{
		if ( spell == SPELL_Gate_Travel )
			return( true );
	}
	if ( IsFlag( REGION_ANTIMAGIC_TELEPORT ))
	{
		if ( spell == SPELL_Teleport )
			return( true );
	}
#ifdef SPHERE_SVR
	if ( IsFlag( REGION_ANTIMAGIC_DAMAGE ))
	{
		// Just no harmfull spells.
		const CSpellDef* pSpellDef = g_Cfg.GetSpellDef(spell);
		ASSERT(pSpellDef);
		if ( pSpellDef->IsSpellType( SPELLFLAG_HARM ))
			return( true );
	}
#endif
	return( false );
}

HRESULT CRegionBasic::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceDef::s_Method(pszKey, vArgs, vValRet, pSrc));
	}

#ifdef SPHERE_SVR	// server only.
	switch (iProp)
	{
	case M_IsMulti:
		vValRet.SetBool( IsMultiRegion());
		break;
	case M_IsComplex:
		vValRet.SetBool( PTR_CAST(CRegionComplex,this) ? true : false );
		break;
	case M_IsSimple:
		vValRet.SetBool( PTR_CAST(CRegionComplex,this) ? false : true );
		break;
	case M_AllClients:	
		{
			CSphereExpContext exec( NULL, pSrc );
			for ( CClientPtr pClient = g_Serv.GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
			{
				CCharPtr pChar = pClient->GetChar();
				if ( pChar == NULL )
					continue;
				if ( pChar->GetArea() != this )
					continue;
				exec.SetBaseObject( REF_CAST(CChar,pChar));
				exec.ExecuteCommand( vArgs );
			}
		}
		break;
	case M_Sectors:
		SendSectorsCommand(vArgs,pSrc);
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
#endif

	return NO_ERROR;
}

void CRegionBasic::SendSectorsCommand( LPCTSTR pszCommand, CScriptConsole* pSrc )
{
	// Send a command to all the CSectors in this region.
#ifdef SPHERE_SVR	// server only.
	CSphereExpContext exec( NULL, pSrc );
	for ( int i=0;; i++ )
	{
		CSectorPtr pSector = GetSector(i);
		if ( pSector == NULL )
			break;
		exec.SetBaseObject( pSector );
		exec.ExecuteCommand( pszCommand );
	}
#endif // SPHERE_SVR
}

CSectorPtr CRegionBasic::GetSector( int& i ) const	
{
	// itterate all the sectors that overlap this region.
	CSectorPtr pSector;
	for(;;)
	{
		// This is really cheating i know. But it's safe enough 
		pSector = (static_cast<const CRectMap*>(&m_rectUnion))->GetSector(i);
		if ( pSector == NULL )
			break;
		// Does the sector rect really overlap ?
		if ( IsOverlapped( pSector->GetRect()))
			break;
		i++;	// skip this one,
	}
	return( pSector );
}


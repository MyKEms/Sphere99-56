//
// CCharDef.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
#include "stdafx.h"	// predef header.

/////////////////////////////////////////////////////////////////
// -CCharDef

const CScriptProp CCharDef::sm_Props[CCharDef::P_QTY+1] =
{
#define CCHARDEFPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "cchardefprops.tbl"
#undef CCHARDEFPROP
	NULL,
};

const CScriptProp CCharDef::sm_Triggers[CCharDef::T_QTY+1] =	// static
{
#define CCHAREVENT(a,b,c) {"@" #a,b,c},	// just use the body here
#define CITEMEVENT(a,b,c)  {"@Item" #a,b,c},
#define CSKILLDEFEVENT(a,b,c)  {"@Skill" #a,b,c},
	CCHAREVENT(AAAUNUSED,CSCRIPTPROP_UNUSED,NULL)	// reserved for XTRIG_UNKNOWN
#include "ccharevents.tbl"
#undef CCHAREVENT
#undef CITEMEVENT
#undef CSKILLDEFEVENT
	NULL,
};

CSCRIPT_CLASS_IMP1(CharDef,CCharDef::sm_Props,NULL,CCharDef::sm_Triggers,ObjBaseDef);

CCharDef::CCharDef( CREID_TYPE id ) :
	CObjBaseDef( CSphereUID( RES_CharDef, id ))
{
	m_ResDispId = CREID_INVALID;	// no alternate ?
	m_iResourceLevel = 0;
	m_trackID = ITEMID_TRACK_WISP;
	m_soundbase = SOUND_UNUSED;	
	m_Anims = 0xFFFFFF;		// allow all.
	m_MaxFood = 15;			// Default value
	m_wBloodHue = HUE_DEFAULT;
	m_MountID = ITEMID_NOTHING;
	m_Str = 0;
	m_Dex = 0;

	m_armor.SetRange(0,0);
	m_attack.SetRange(0,1);

	// Default = humanoid body class.
	m_pRaceGroup = g_Cfg.m_DefaultRaceClass;
	m_pRaceClass = g_Cfg.m_DefaultRaceClass;

	if ( IsValidDispID(id))
	{
		// in display range.
		m_wDispIndex = id;
	}
	else
	{
		m_wDispIndex = CREID_INVALID;	// must read from SCP file later
	}
}

LPCTSTR CCharDef::GetTradeName() const
{
	// From "Bill the carpenter" or "#HUMANMALE the Carpenter",
	// Get "Carpenter"

	LPCTSTR pName = CObjBaseDef::GetTypeName();
	if ( pName[0] != '#' )
		return( pName );

	LPCTSTR pSpace = pName;
	while ( *pSpace && *pSpace != ' ' )
		pSpace++;
	while ( *pSpace == ' ' )
		pSpace++;

	if ( *pSpace == '\0' )
		return( pName ); // no trade, just use the name

	if ( ! _strnicmp( pSpace, "the ", 4 ))
		pSpace += 4;

	return( pSpace );
}

void CCharDef::CopyBasic( const CCharDef* pCharDef )
{
	// Dont copy NAME
	m_trackID = pCharDef->m_trackID;
	m_soundbase = pCharDef->m_soundbase;

	m_wBloodHue = pCharDef->m_wBloodHue;
	m_MaxFood = pCharDef->m_MaxFood;
	m_FoodType.CopyArray( pCharDef->m_FoodType );
	m_Desires.CopyArray( pCharDef->m_Desires );

	m_armor = pCharDef->m_armor;
	m_attack = pCharDef->m_attack;
	m_Anims = pCharDef->m_Anims;

	m_BaseResources.CopyArray( pCharDef->m_BaseResources );

	CObjBaseDef::CopyBasic( pCharDef );	// This will overwrite the CResourceLink!!
}

bool CCharDef::SetDispID( CREID_TYPE idArt )
{
	// Setting the artwork "ID" for this.
	// NOTE: This will set all props to default values for the base type !
	// Execpt the NAME=

	if ( idArt == GetDispID())	// no change.
		return true;

	if ( ! IsValidDispID( idArt ))
	{
		DEBUG_ERR(( "Creating char SetDispID(0%x) > %d" LOG_CR, idArt, CREID_QTY ));
		return false; // must be in display range.
	}

	if ( idArt == GetID())
	{
		// Set back to base artwork. odd?
		m_wDispIndex = idArt;
		return true;
	}

	// Copy the rest of the stuff from the display base.
	// NOTE: This can mess up g_Serv.m_scpChar offset!!
	CCharDefPtr pCharDef = g_Cfg.FindCharDef( idArt );
	if ( pCharDef == NULL )
	{
		DEBUG_ERR(( "Creating char SetDispID(0%x) BAD" LOG_CR, idArt ));
		return( false );
	}

	CopyBasic( pCharDef );
	return( true );
}

void CCharDef::SetFoodType( LPCTSTR pszFood )
{
	m_FoodType.s_LoadKeys( pszFood );

	// Try to determine the real value
	m_MaxFood = 0;
	for ( int i=0; i<m_FoodType.GetSize(); i++ )
	{
		if ( m_MaxFood < m_FoodType[i].GetResQty())
			m_MaxFood = m_FoodType[i].GetResQty();
	}
}

HRESULT CCharDef::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CObjBaseDef::s_PropGet( pszKey, vValRet, pSrc ));
	}
	switch (iProp)
	{
	case P_Anim:
		vValRet.SetDWORD( m_Anims );
		break;
	case P_Aversions:
		// Ignore this for now.
		break;
	case P_Shelter:
		// Ingore this for now.
		break;

	case P_BloodColor:
		vValRet.SetDWORD( m_wBloodHue );
		break;

	case P_Armor:
	// case P_Defense:
		m_armor.v_Get( vValRet );
		break;
	case P_Dam:
	case P_Attack:
		m_attack.v_Get( vValRet );
		break;

	case P_Desires:
		m_Desires.v_GetKeys(vValRet);
		break;
	case P_Dex:
		vValRet.SetInt( m_Dex );
		break;
	case P_DispIdDec:	// for properties dialog.
		vValRet.SetInt( m_trackID );
		break;
	case P_DispId:
		vValRet = g_Cfg.ResourceGetName( CSphereUID( RES_CharDef, GetDispID()));
		break;
	case P_FoodType:
		m_FoodType.v_GetKeys(vValRet);
		break;
	case P_HireDayWage:
		m_TagDefs.FindKeyVar( "HIREDAYWAGE", vValRet );
		break;
	case P_Icon:
		vValRet.SetDWORD( m_trackID );
		break;
	case P_MountId:
		vValRet.SetDWORD( m_MountID );
		break;
	case P_Job:
		vValRet = GetTradeName();
		break;
	case P_MaxFood:
		vValRet.SetInt( m_MaxFood );
		break;
	case P_Race:
		vValRet.SetRef( m_pRaceClass );
		break;
	case P_RaceGroup:
		vValRet.SetRef( m_pRaceGroup );
		break;
	case P_ResDispId:
		vValRet.SetInt( m_ResDispId );
		break;
	case P_ResLevel:
		vValRet.SetInt( m_iResourceLevel );
		break;
	case P_Sound:
		vValRet.SetDWORD( m_soundbase );
		break;
	case P_Str:
		vValRet.SetInt( m_Str );
		break;
	case P_TEvents:
		m_Events.v_Get( vValRet );
		break;
	case P_TSpeech:
		m_Speech.v_Get( vValRet );
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CCharDef::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CObjBaseDef::s_PropSet(pszKey,vVal));
	}

	switch (iProp)
	{
	case P_Anim:
		m_Anims = vVal.GetDWORD();
		break;
	case P_Aversions:
		// Ignore this for now. = negative desires ?
#ifdef _DEBUG
		{
			CResourceQtyArray Aversions;
			Aversions.s_LoadKeys( vVal.GetPSTR());
		}
#endif
		break;
	case P_Shelter:
		// Ignore this for now.
#ifdef _DEBUG
		{
			CResourceQtyArray Shelter;
			Shelter.s_LoadKeys( vVal.GetPSTR());
		}
#endif
		break;
	case P_BloodColor:
		m_wBloodHue = vVal.GetInt();
		break;
	case P_Armor:
		m_armor.v_Set( vVal);
		break;
	case P_Dam:
	case P_Attack:
		m_attack.v_Set( vVal);
		return 0;
	case P_Desires:
		m_Desires.s_LoadKeys( vVal.GetPSTR());
		break;
	case P_MountId:
		m_MountID = (ITEMID_TYPE) vVal.GetInt();
		break;
	case P_Dex:
		m_Dex = vVal.GetInt();
		break;
	case P_FoodType:
		SetFoodType( vVal.GetPSTR());
		break;
	case P_HireDayWage:
		m_TagDefs.SetKeyInt( "HIREDAYWAGE", vVal.GetInt());
		break;
	case P_DispIdDec:
	case P_Icon:
		{
			ITEMID_TYPE id = (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, vVal.GetPSTR());
			if ( id < 0 || id >= ITEMID_MULTI )
			{
				return( HRES_INVALID_INDEX );
			}
			m_trackID = id;
		}
		break;
	case P_DispId:
	case P_Id:
		// NOTE: This will set all props to default values for the base type !
		if ( ! SetDispID( (CREID_TYPE) g_Cfg.ResourceGetIndexType( RES_CharDef, vVal.GetPSTR())))
			return HRES_BAD_ARGUMENTS;
		break;

	case P_MaxFood:
		m_MaxFood = vVal.GetInt();
		break;
	case P_Race:
		m_pRaceClass = REF_CAST(CRaceClassDef,g_Cfg.ResourceGetDefByName( RES_RaceClass, vVal.GetPSTR()));
		break;
	case P_RaceGroup:
		m_pRaceGroup = REF_CAST(CRaceClassDef,g_Cfg.ResourceGetDefByName( RES_RaceClass, vVal.GetPSTR()));
		break;
	case P_ResDispId:
		m_ResDispId = (CREID_TYPE) vVal.GetInt();
		break;
	case P_ResLevel:
		m_iResourceLevel = vVal.GetInt();
		break;
	case P_Sound:
		m_soundbase = vVal.GetInt();
		break;
	case P_Str:
		m_Str = vVal.GetInt();
		break;
	case P_TEvents:
		if ( ! m_Events.v_Set( vVal, RES_Events ))
			return HRES_BAD_ARGUMENTS;
		break;
	case P_TSpeech:
		if ( ! m_Speech.v_Set( vVal, RES_Events ))
			return HRES_BAD_ARGUMENTS;
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return NO_ERROR;
}

bool CCharDef::s_LoadProps( CScript& s )
{
	CResourceObj::s_LoadProps(s);

	if ( ! IsValidDispID( GetDispID()))
	{
		// Bad display id ? could be an advance gate.
		DEBUG_WARN(( "Char Script '%s' has bad dispid 0%x" LOG_CR, (LPCTSTR) GetResourceName(), GetDispID()));
		m_wDispIndex = CREID_MAN;
	}
	if ( m_sName.IsEmpty())
	{
		DEBUG_ERR(( "Char Script '%s' has no name!" LOG_CR, (LPCTSTR) GetResourceName()));
		return( false );
	}
	return( true );
}

////////////////////////////////////////////

void CCharDef::UnLink()
{
	// We are being reloaded .
	m_Speech.RemoveAll();
	m_Events.RemoveAll();
	m_FoodType.RemoveAll();
	m_Desires.RemoveAll();
	m_pRaceClass = g_Cfg.m_DefaultRaceClass;
	m_pRaceGroup = g_Cfg.m_DefaultRaceClass;
	CObjBaseDef::UnLink();
}

CCharDefPtr CCharDef::TranslateBase( CResourceDef* pResDef ) // static
{
	if ( pResDef == NULL )
		return NULL;

	// if already loaded.
	CCharDefPtr pBase = PTR_CAST(CCharDef,pResDef);
	if ( pBase )
	{
		return( pBase );	// already loaded.
	}

	// create a new base.
	CREID_TYPE id = (CREID_TYPE) RES_GET_INDEX(pResDef->GetUIDIndex());
	if ( id == CREID_INVALID )
		return NULL;

	CCharEvents* pBaseLink = PTR_CAST(CCharEvents,pResDef);
	ASSERT(pBaseLink);

	pBase = new CCharDef( id );
	ASSERT(pBase);
	pBase->CopyLink( pBaseLink );

	// replace existing one
	g_Cfg.m_ResHash.AddSortKey( pResDef->GetHashCode(), pBase );

	// load it's data on demand.
	CResourceLock s(pBase);
	if ( ! s.IsFileOpen())
	{
		return( NULL );
	}
	if ( ! pBase->s_LoadProps(s))
	{
		return( NULL );
	}

	return( pBase );
}


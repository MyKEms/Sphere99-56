//
// CResourceDef.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
// A variety of resource blocks.
//
#include "stdafx.h"	// predef header.
#include "CAssoc.h"

//*******************************************
// -CItemTypeDef - RES_TypeDef

CSCRIPT_CLASS_IMP1(ItemTypeDef,NULL,NULL,CItemDef::sm_Triggers,ResourceTriggered);

//*******************************************
// -CCharEvents - RES_Events

CSCRIPT_CLASS_IMP1(CharEvents,NULL,NULL,CCharDef::sm_Triggers,ResourceTriggered);

//*******************************************
// -CSkillDef

const CScriptProp CSkillDef::sm_Triggers[CSkillDef::T_QTY+1] =
{
#define CSKILLDEFEVENT(a,b,c) {"@" #a,b,c},
	CSKILLDEFEVENT(AAAUNUSED,CSCRIPTPROP_UNUSED,NULL)	// reserved for XTRIG_UNKNOWN
#include "cskilldefevents.tbl"
#undef CSKILLDEFEVENT
	NULL,
};

const CScriptProp CSkillDef::sm_Props[CSkillDef::P_QTY+1] =
{
#define CSKILLDEFPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "cskilldefprops.tbl"
#undef CSKILLDEFPROP
	NULL,
};

CSCRIPT_CLASS_IMP1(SkillDef,CSkillDef::sm_Props,NULL,CSkillDef::sm_Triggers,ResourceTriggered);

CSkillDef::CSkillDef( SKILL_TYPE skill ) :
	CResourceTriggered( CSphereUID( RES_Skill, skill ))
{
	DEBUG_CHECK( CChar::IsSkillBase(skill));
	m_StatPercent = 0;
}

HRESULT CSkillDef::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceLink::s_PropGet( pszKey, vValRet, pSrc ));
	}
	switch (iProp)
	{
	case P_AdvRate:	// ADV_RATE=Chance at 100, Chance at 50, chance at 0
		m_AdvRate.v_Get(vValRet);
		break;
	case P_BonusStats: // "BONUS_STATS"
		vValRet.SetInt( m_StatPercent );
		break;
	case P_Bonus_Dex:
		vValRet.SetInt( m_StatBonus[STAT_Dex] );
		break;
	case P_Bonus_Int:
		vValRet.SetInt( m_StatBonus[STAT_Int] );
		break;
	case P_Bonus_Str:
		vValRet.SetInt( m_StatBonus[STAT_Str] );
		break;
	case P_Stat_Dex:
		vValRet.SetInt( m_Stat[STAT_Dex] );
		break;
	case P_Stat_Int:
		vValRet.SetInt( m_Stat[STAT_Int] );
		break;
	case P_Stat_Str:
		vValRet.SetInt( m_Stat[STAT_Str] );
		break;
	case P_Delay:
		m_Delay.v_Get(vValRet);
		break;
	case P_Effect:
		m_Effect.v_Get(vValRet);
		break;
	case P_Key:
		vValRet = m_sKey;
		break;
	case P_PromptMsg:
		vValRet = m_sTargetPrompt;
		break;
	case P_Title: 
		vValRet = m_sTitle;
		break;
	case P_Values: // VALUES = 100,50,0 price levels.
		m_Values.v_Get(vValRet);
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CSkillDef::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceLink::s_PropSet( pszKey, vVal ));
	}
	switch (iProp)
	{
	case P_AdvRate:	// ADV_RATE=Chance at 100, Chance at 50, chance at 0
		m_AdvRate.v_Set( vVal );
		break;
	case P_Bonus_Dex:
		m_StatBonus[STAT_Dex] = vVal.GetInt();
		break;
	case P_Bonus_Int:
		m_StatBonus[STAT_Int] = vVal.GetInt();
		break;
	case P_Bonus_Str:
		m_StatBonus[STAT_Str] = vVal.GetInt();
		break;
	case P_Delay:
		m_Delay.v_Set(vVal);
		break;
	case P_Effect:
		m_Effect.v_Set(vVal);
		break;
	case P_Key: // not the same as DEFNAME but similar.
		m_sKey = vVal.GetStr();
		SetResourceName( m_sKey );
		return 0;
	case P_PromptMsg:
		m_sTargetPrompt = vVal.GetStr();
		break;
	case P_BonusStats:
		m_StatPercent = vVal.GetInt();
		break;
	case P_Stat_Dex:
		m_Stat[STAT_Dex] = vVal.GetInt();
		break;
	case P_Stat_Int:
		m_Stat[STAT_Int] = vVal.GetInt();
		break;
	case P_Stat_Str:
		m_Stat[STAT_Str] = vVal.GetInt();
		break;
	case P_Title:
		m_sTitle = vVal.GetStr();
		break;
	case P_Values: // VALUES = 100,50,0 price levels.
		m_Values.v_Set(vVal);
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

//*******************************************
// -CProfessionDef

const CScriptProp CProfessionDef::sm_Props[CProfessionDef::P_QTY+1] =
{
#define CPROFESSIONPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "cprofessionprops.tbl"
#undef CPROFESSIONPROP
	NULL,
};

CSCRIPT_CLASS_IMP2(ProfessionDef,CProfessionDef::sm_Props,NULL,NULL,ResourceLink);

template<>
void CScriptClassTemplate<CProfessionDef>::InitScriptClass()
{
	// Add skills list to the class def.
	if ( IsInit())
		return;

	CScriptClass::InitScriptClass();
}

void CProfessionDef::Init()
{
	m_SkillSumMax = 10*1000;
	m_StatSumMax = 300;
	int i;
	for ( i=0; i<COUNTOF(m_StatMax); i++ )
	{
		m_StatMax[i] = 100;
	}
	for ( i=0; i<COUNTOF(m_SkillLevelMax); i++ )
	{
		m_SkillLevelMax[i] = 1000;
	}
}

HRESULT CProfessionDef::s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		int i = g_Cfg.FindSkillKey( pszKey, false );
		if ( i != SKILL_NONE )
		{
			ASSERT( i<COUNTOF(m_SkillLevelMax));
			vVal.SetInt( m_SkillLevelMax[i] );
			return NO_ERROR;
		}
		i = g_Cfg.FindStatKey( pszKey, false );
		if ( i >= 0 )
		{
			ASSERT( i<COUNTOF(m_StatMax));
			vVal.SetInt( m_StatMax[i] );
			return NO_ERROR;
		}
		return( CResourceDef::s_PropGet( pszKey, vVal, pSrc ));
	}

	switch ( iProp )
	{
		//	case P_EVENTS: // "EVENTS"
		//		break;
	case P_Name: 
		vVal = GetName();
		break;
	case P_SkillSum:
		vVal.SetInt( m_SkillSumMax );
		break;
	case P_StatSum:
		vVal.SetInt( m_StatSumMax );
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CProfessionDef::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		int i = g_Cfg.FindSkillKey( pszKey, false );
		if ( i != SKILL_NONE )
		{
			ASSERT( i<COUNTOF(m_SkillLevelMax));
			m_SkillLevelMax[i] = vVal.GetInt();
			return NO_ERROR;
		}
		i = g_Cfg.FindStatKey( pszKey, false );
		if ( i >= 0 )
		{
			ASSERT( i<COUNTOF(m_StatMax));
			m_StatMax[i] = vVal.GetInt();
			return NO_ERROR;
		}
		return( CResourceDef::s_PropSet(pszKey, vVal));
	}
	switch (iProp)
	{
		//	case P_EVENTS: // "EVENTS"
		//		break;
	case P_Name:
		m_sName = vVal.GetStr();
		break;
	case P_SkillSum:
		m_SkillSumMax = vVal.GetInt();
		break;
	case P_StatSum:
		m_StatSumMax = vVal.GetInt();
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

//*******************************************
// -CSpellDef

const CScriptProp CSpellDef::sm_Props[CSpellDef::P_QTY+1] =
{
#define CSPELLDEFPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "cspelldefprops.tbl"
#undef CSPELLDEFPROP
	NULL,
};

CSCRIPT_CLASS_IMP1(SpellDef,CSpellDef::sm_Props,NULL,NULL,ResourceLink);

CSpellDef::CSpellDef( SPELL_TYPE id ) : CResourceDef( CSphereUID( RES_Spell, id ))
{
	m_wFlags = SPELLFLAG_DISABLED;
	m_idEffect = ITEMID_NOTHING;
	m_idSpell = ITEMID_NOTHING;		// The rune graphic for this.
	m_idScroll = ITEMID_NOTHING;	// The scroll graphic item for this.
	m_iCastTime = 0;
}

HRESULT CSpellDef::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	// RES_Spell
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp<0)
	{
		return( CResourceDef::s_PropSet(pszKey, vVal));
	}

	switch (iProp)
	{
	case P_CastTime:	// HAD _
		m_iCastTime = vVal.GetInt();	// In tenths.
		break;
	case P_Duration:
		m_Duration.v_Set(vVal);
		break;
	case P_Effect:
		m_Effect.v_Set(vVal);
		break;
	case P_EffectId:	// HAD _
		m_idEffect = (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, vVal.GetPSTR());
		break;
	case P_Flags:
		m_wFlags = vVal.GetInt();
		break;
	case P_ManaUse:
		m_wManaUse = vVal.GetInt();
		break;
	case P_Name:
		m_sName = vVal.GetStr();
		break;
	case P_PromptMsg:	// HAD _
		m_sTargetPrompt = vVal.GetStr();
		break;
	case P_Resources:
		m_Reags.s_LoadKeys( vVal.GetPSTR());
		break;
	case P_RuneItem:	// HAD _
		m_idSpell = (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, vVal.GetPSTR());
		break;
	case P_Runes:
		// This may only be basic chars !
		m_sRunes = vVal.GetStr();
		break;
	case P_ScrollItem:	// HAD _
		m_idScroll = (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, vVal.GetPSTR());
		break;
	case P_SkillReq:
		m_SkillReq.s_LoadKeys( vVal.GetPSTR());
		break;
	case P_Sound:
		m_sound = (SOUND_TYPE) vVal.GetInt();
		break;
	case P_Value:
		m_Value.v_Set(vVal);
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CSpellDef::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	// RES_Spell
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp<0)
	{
		return( CResourceDef::s_PropGet(pszKey, vValRet, pSrc));
	}

	switch (iProp)
	{
	case P_CastTime:
		vValRet = m_iCastTime;	// In tenths.
		break;
	case P_Flags:
		vValRet = m_wFlags;
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return NO_ERROR;
}

//*******************************************
// -CRaceClassDef

const CScriptProp CRaceClassDef::sm_Props[CRaceClassDef::P_QTY+1] =
{
#define CRACECLASSPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "craceclassprops.tbl"
#undef CRACECLASSPROP
	NULL,
};

#ifdef USE_JSCRIPT
#define CRACECLASSMETHOD(a,b,c) JSCRIPT_METHOD_IMP(CRaceClassDef,a)
#include "CRaceClassMethods.tbl"
#undef CRACECLASSMETHOD
#endif

const CScriptMethod CRaceClassDef::sm_Methods[ CRaceClassDef::M_QTY+1 ] =
{
#define CRACECLASSMETHOD(a,b,c) CSCRIPT_METHOD_IMP(a,b,c)
#include "CRaceClassMethods.tbl"
#undef CRACECLASSMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP1(RaceClassDef,CRaceClassDef::sm_Props,NULL,NULL,ResourceDef);

// What armor layers cover the body part?
static const LAYER_TYPE sm_BodyPartHead[] = { LAYER_HELM, LAYER_NONE };		// BODYPART_HEAD,
static const LAYER_TYPE sm_BodyPartNeck[] = { LAYER_COLLAR, LAYER_NONE };		// BODYPART_NECK,
static const LAYER_TYPE sm_BodyPartBack[] = { LAYER_SHIRT, LAYER_CHEST, LAYER_TUNIC, LAYER_CAPE, LAYER_ROBE, LAYER_NONE };	// BODYPART_BACK,
static const LAYER_TYPE sm_BodyPartChest[] = { LAYER_SHIRT, LAYER_CHEST, LAYER_TUNIC, LAYER_ROBE, LAYER_NONE };		// BODYPART_CHEST
static const LAYER_TYPE sm_BodyPartArms[] = { LAYER_ARMS, LAYER_CAPE, LAYER_ROBE, LAYER_NONE };		// BODYPART_ARMS,
static const LAYER_TYPE sm_BodyPartHands[] = { LAYER_GLOVES,	LAYER_NONE };	// BODYPART_HANDS
static const LAYER_TYPE sm_BodyPartLegs[] = { LAYER_PANTS, LAYER_SKIRT, LAYER_HALF_APRON, LAYER_ROBE, LAYER_LEGS, LAYER_NONE };	// BODYPART_LEGS,
static const LAYER_TYPE sm_BodyPartFeet[] = { LAYER_SHOES, LAYER_LEGS, LAYER_NONE };	// BODYPART_FEET,

static const CRaceBodyPartType sm_BodyPartLayers[BODYPART_ARMOR_QTY] =	// layers covering the armor zone.
{
	// Percent coverage.
	{ 14,	sm_BodyPartHead },		// BODYPART_HEAD,
	{ 6,	sm_BodyPartNeck },		// BODYPART_NECK,
	{ 18,	sm_BodyPartBack },		// BODYPART_BACK,
	{ 18,	sm_BodyPartChest },	// BODYPART_CHEST
	{ 14,	sm_BodyPartArms },		// BODYPART_ARMS,
	{ 7,	sm_BodyPartHands },	// BODYPART_HANDS
	{ 17,	sm_BodyPartLegs },		// BODYPART_LEGS,
	{ 6,	sm_BodyPartFeet },		// BODYPART_FEET,
};

CRaceClassDef::CRaceClassDef( CSphereUID rid ) :
	CResourceDef( rid )
{
	// RES_RaceClass
	// Get rid of CAN_C_NONHUMANOID flag in favor of this !
	// Put blood wHue in here as well.

	m_dwBodyPartFlags = 0xFFFFFFFF;	// What body parts do we have ? BODYPART_TYPE

	memset( m_iRegenRate,0,sizeof(m_iRegenRate));
	m_iRegenRate[STAT_Health] = 6*TICKS_PER_SEC; // Seconds to heal ONE hp (base before stam and food adjust)
	m_iRegenRate[STAT_Mana] = 5*TICKS_PER_SEC; // Seconds to heal ONE mn
	m_iRegenRate[STAT_Stam] = 3*TICKS_PER_SEC; // Seconds to heal ONE stm
	m_iRegenRate[STAT_Food] = 30*60*TICKS_PER_SEC; // Food usage (1 time per 30 minutes)
	m_iRegenRate[STAT_Fame] = 12*60*60*TICKS_PER_SEC; // Fame drop (1 time per x minutes)
}

const CRaceBodyPartType* CRaceClassDef::GetBodyPart( BODYPART_TYPE part ) const
{
	return &sm_BodyPartLayers[part];
}

HRESULT CRaceClassDef::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	// m_MountID
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceDef::s_PropGet( pszKey, vValRet, pSrc ));
	}
	switch (iProp)
	{
	case P_Regen:
		return( HRES_INVALID_FUNCTION );
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return NO_ERROR;
}

HRESULT CRaceClassDef::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceDef::s_PropSet(pszKey, vVal));
	}
	switch (iProp)
	{
	case P_Regen:
		if ( vVal.MakeArraySize() < 2 )
			return HRES_BAD_ARG_QTY;
		{
		STAT_TYPE iStat = g_Cfg.FindStatKey( vVal.GetArrayPSTR(0), true );
		if ( iStat < 0 )
			return( HRES_INVALID_INDEX );
		SetRegenRate( iStat, vVal.GetArrayInt(1));
		}
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return NO_ERROR;
}

HRESULT CRaceClassDef::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceDef::s_Method( pszKey, vArgs, vValRet, pSrc ));
	}

	switch (iProp)
	{
	case M_Regen:
		{
		int iArgQty = vArgs.MakeArraySize();
		if ( iArgQty < 1 )
			return HRES_BAD_ARG_QTY;
		STAT_TYPE iStat = g_Cfg.FindStatKey( vArgs.GetArrayPSTR(0), true );
		if ( iStat < 0 )
			return( HRES_INVALID_INDEX );
		if ( iArgQty == 1 )
		{
			vValRet = GetRegenRate( iStat );
		}
		else
		{
			SetRegenRate( iStat, vArgs.GetArrayInt(1));
		}
		}
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return NO_ERROR;
}


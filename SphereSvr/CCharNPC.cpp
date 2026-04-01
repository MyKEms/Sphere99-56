//
// CCharNPC.CPP
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
// Actions specific to an NPC.
//

#include "stdafx.h"	// predef header.

const CScriptProp CCharPlayer::sm_Props[CCharPlayer::P_QTY+1] =
{
#define CCHARPLAYERPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "ccharplayerprops.tbl"
#undef CCHARPLAYERPROP
	NULL,
};

const CScriptProp CCharNPC::sm_Props[CCharNPC::P_QTY+1] =
{
#define CCHARNPCPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "ccharnpcprops.tbl"
#undef CCHARNPCPROP
	NULL,
};

#ifdef USE_JSCRIPT
#define CCHARPLAYERMETHOD(a,b,c) JSCRIPT_METHOD_IMP(CCharPlayer,a)
#include "ccharplayermethods.tbl"
#undef CCHARPLAYERMETHOD
#endif

const CScriptMethod CCharPlayer::sm_Methods[CCharPlayer::M_QTY+1] =
{
#define CCHARPLAYERMETHOD(a,b,c) CSCRIPT_METHOD_IMP(a,b,c)
#include "ccharplayermethods.tbl"
#undef CCHARPLAYERMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP0(CharPlayer,CCharPlayer::sm_Props,CCharPlayer::sm_Methods);
CSCRIPT_CLASS_IMP0(CharNPC,CCharNPC::sm_Props,CCharNPC::sm_Methods);

void CChar::NPC_Clear()
{
	m_pNPC.Free();
}

void CChar::Player_Clear()
{
	if ( ! m_pPlayer)
		return;

	// unlink me from my account.
	if ( g_Serv.m_iModeCode != SERVMODE_Exiting )
	{
		DEBUG_WARN(( "Player delete '%s' name '%s'" LOG_CR,
			(LPCTSTR) GetAccount()->GetName(), (LPCTSTR) GetName()));
	}

	// Is this valid ?
	m_pAccount->DetachChar( this );
	m_pPlayer.Free();
	m_pAccount = NULL;
}

HRESULT CChar::Player_SetAccount( CAccount* pAccount )
{
	// Set up the char as a Player.
	if ( pAccount == NULL )
	{
		DEBUG_ERR(( "Player_SetAccount '%s' NULL" LOG_CR, (LPCTSTR) GetName()));
		return HRES_BAD_ARGUMENTS;
	}

	// NPC_Clear(); // allow to be both NPC and player ?
	if ( m_pPlayer)
	{
		if ( GetAccount() == pAccount )
			return( NO_ERROR );
		// ??? Move to another account ?
		DEBUG_ERR(( "Player_SetAccount '%s' already set '%s' != '%s' !" LOG_CR, (LPCTSTR) GetName(), (LPCTSTR) GetAccount()->GetName(), (LPCTSTR) pAccount->GetName()));
		return( HRES_INVALID_HANDLE );
	}

	m_pPlayer = new CCharPlayer( this );
	m_pAccount = pAccount;

	ASSERT(m_pPlayer);
	pAccount->AttachChar( this );
	return( NO_ERROR );
}

HRESULT CChar::Player_SetAccount( LPCTSTR pszAccName )
{
	CAccountLock pAccount = g_Accounts.Account_FindNameCheck( pszAccName );
	if ( ! pAccount)
	{
		if ( g_Serv.m_eAccApp != ACCAPP_Free )
		{
			DEBUG_ERR(( "Player_SetAccount '%s' can't find '%s'!" LOG_CR, (LPCTSTR) GetName(), pszAccName ));
			return HRES_INVALID_HANDLE;
		}

		// it's a free server so just create one.
		pAccount = g_Accounts.Account_Create( pszAccName );
		ASSERT( pAccount );
	}
	return( Player_SetAccount( pAccount ));
}

HRESULT CChar::NPC_SetBrain( NPCBRAIN_TYPE NPCBrain )
{
	// Set up the char as an NPC
	if ( NPCBrain == NPCBRAIN_NONE || IsClient())
	{
		DEBUG_ERR(( "NPC_SetBrain NULL" LOG_CR ));
		return HRES_INVALID_HANDLE;
	}
	if ( m_pPlayer)
	{
		DEBUG_ERR(( "NPC_SetBrain to Player '%s'" LOG_CR, (LPCTSTR) GetAccount()->GetName()));
		return HRES_INVALID_HANDLE;
	}
	if ( ! m_pNPC)
	{
		m_pNPC = new CCharNPC( this, NPCBrain );
	}
	else
	{
		// just replace existing brain.
		m_pNPC->m_Brain = NPCBrain;
	}
	return( NO_ERROR );
}

//////////////////////////
// -CCharPlayer

CCharPlayer::CCharPlayer( CChar* pChar )
{
	m_wDeaths = 0;
	m_wMurders = 0;
	memset( m_SkillLock, 0, sizeof( m_SkillLock ));
	SetProfession( pChar, CSphereUID( RES_Profession ));
}

CCharPlayer::~CCharPlayer()
{
	ASSERT( IsValidHeap());
}

HRESULT CCharPlayer::SetProfession( CChar* pChar, CSphereUID rid )
{
	ASSERT(pChar);
	CResourceDefPtr pResDef = g_Cfg.ResourceGetDef(rid);
	CProfessionPtr pLink = REF_CAST(CProfessionDef,pResDef);
	if ( pLink == NULL )
		return( HRES_BAD_ARGUMENTS );

	if ( pLink == m_Profession )
		return( NO_ERROR );

	// Remove any previous RES_Profession from the Events block.
	for (;;)
	{
		int i = pChar->m_Events.FindResourceType( RES_Profession );
		if ( i < 0 )
			break;
		pChar->m_Events.RemoveAt(i);
	}

	m_Profession.SetRefObj( pLink );

	// set it as m_Events block as well.
	pChar->m_Events.Add( pLink );
	return( NO_ERROR );
}

CProfessionPtr CCharPlayer::GetProfession() const
{
	// This should always return NON-NULL.
	DEBUG_CHECK( m_Profession );
	return( m_Profession );
}

HRESULT CCharPlayer::s_PropGetPlayer( CChar* pChar, int iProp, CGVariant& vValRet )
{
	ASSERT(pChar);
	switch ( iProp )
	{
	case P_SkillLock:	// "SkillLock[alchemy]"
		return HRES_INVALID_FUNCTION;
	case P_Deaths:
		vValRet.SetInt( m_wDeaths );
		break;
	case P_Kills:
		vValRet.SetInt( m_wMurders );
		break;
	case P_LastUsed:
		vValRet.SetInt( m_timeLastUsed.GetCacheAge() / TICKS_PER_SEC );
		break;
	case P_Profession:
		vValRet.SetRef( GetProfession());
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CCharPlayer::s_PropSetPlayer( CChar* pChar, int iProp, CGVariant& vVal )
{
	static LPCTSTR const sm_szLockStates[] =
	{
		"UP",
		"DOWN",
		"LOCK",
		NULL,
	};

	ASSERT(pChar);
	switch ( iProp )
	{
	case P_SkillLock:	// "SkillLock[alchemy]"
		if ( vVal.MakeArraySize() < 2 )
			return HRES_BAD_ARG_QTY;
		{
			// SKILLLOCK[x]=
			SKILL_TYPE skill = g_Cfg.FindSkillKey( vVal.GetArrayPSTR(0), true );
			if ( skill <= SKILL_NONE )
				return( HRES_INVALID_INDEX );
			int bState;
			if ( vVal.GetArrayElement(1).IsNumeric())
			{
				bState = vVal.GetArrayInt(1);
			}
			else
			{
				bState = FindTable( vVal.GetArrayElement(1), sm_szLockStates );
			}
			if ( bState < SKILLLOCK_UP || bState > SKILLLOCK_LOCK )
				return( HRES_BAD_ARGUMENTS );
			Skill_SetLock( skill, (SKILLLOCK_TYPE) bState );
		}
		break;
	case P_Deaths:
		m_wDeaths = vVal.GetInt();
		break;
	case P_Kills:
		m_wMurders = vVal.GetInt();
		break;
	case P_LastUsed:
		m_timeLastUsed.InitTimeCurrent( - ( vVal.GetInt() * TICKS_PER_SEC ));
		break;

	case P_Plot1:	// phase these out!
		// Just change these to TAG_PLOT1
		pChar->m_TagDefs.SetKeyVar( "PLOT1", vVal );
		break;
	case P_Plot2:	// phase these out!
		// Just change these to TAG_PLOT2
		pChar->m_TagDefs.SetKeyVar( "PLOT2", vVal );
		break;

	case P_Profession:
		return SetProfession( pChar, g_Cfg.ResourceGetIDType( RES_Profession, vVal.GetPSTR()));
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

void CCharPlayer::s_SerializePlayer( CChar* pChar, CGFile& a ) // const
{
}

void CCharPlayer::s_WritePlayer( CChar* pChar, CScript& s )
{
	ASSERT(pChar);

	if ( pChar->IsClient())
	{
		// periodically update this. (just in case we are logged in forever hehe.)
		//
		m_timeLastUsed.InitTimeCurrent();
	}

	if ( m_wDeaths )
		s.WriteKeyInt( "DEATHS", m_wDeaths );
	if ( m_wMurders )
		s.WriteKeyInt( "KILLS", m_wMurders );
	if ( RES_GET_INDEX(GetProfession()->GetUIDIndex()))
		s.WriteKey( "PROFESSION", GetProfession()->GetResourceName());

	for ( int j=SKILL_First;j<SKILL_QTY;j++)	// Don't write all lock states!
	{
		if ( ! m_SkillLock[j] )
			continue;
		TCHAR szTemp[128];
#if 1
		sprintf( szTemp, "SkillLock.%d", j );	// smaller storage space.
		s.WriteKeyInt( szTemp, m_SkillLock[j] );
#else
		sprintf( szTemp, "SkillLock.%s", (LPCTSTR) g_Cfg.ResourceGetName( RES_Skill, j ));
		s.WriteKey( szTemp, (( m_SkillLock[j] == 2 ) ? "Lock" :
			(( m_SkillLock[j] == 1) ? "Down" : "Up")));
#endif
	}
}

HRESULT CChar::s_MethodPlayer( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	ASSERT(m_pPlayer);

	int iProp = s_FindKeyInTable( pszKey, CCharPlayer::sm_Methods );
	if ( iProp < 0 )
		return HRES_UNKNOWN_PROPERTY;

	if ( iProp == CCharPlayer::M_SkillLock )
	{
		switch ( vArgs.MakeArraySize())
		{
		case 0:
			return HRES_BAD_ARG_QTY;
		case 1:	// Get
			{
				SKILL_TYPE skill = g_Cfg.FindSkillKey( vArgs, true );
				if ( skill <= SKILL_NONE )
					return( HRES_INVALID_INDEX );
				vValRet.SetInt( Skill_GetLock( skill ));
			}
			break;
		case 2: // Set
			return m_pPlayer->s_PropSetPlayer( this, CCharPlayer::P_SkillLock, vArgs );
		}
		return NO_ERROR;
	}

	// Same as the CAccount Method
	return GetAccount()->s_Method( pszKey, vArgs, vValRet, pSrc );
}

//////////////////////////
// -CCharNPC

CCharNPC::CCharNPC( CChar* pChar, NPCBRAIN_TYPE NPCBrain )
{
	m_Brain = NPCBrain;
	m_Home_Dist_Wander = SHRT_MAX;	// as far as i want.
	m_Act_Motivation = 0;
}

CCharNPC::~CCharNPC()
{
	ASSERT( IsValidHeap());
}

HRESULT CCharNPC::s_PropSetNPC( CChar* pChar, int iProp, CGVariant& vVal )
{
	ASSERT(pChar);
	switch ( iProp )
	{
	case P_ActPri:
		m_Act_Motivation = vVal.GetInt();
		break;
	case P_Brain:
	case P_NPC:
		m_Brain = (NPCBRAIN_TYPE) vVal.GetInt();
		break;
	case P_HomeDist:
		if ( ! pChar->m_ptHome.IsValidPoint())
		{
			pChar->m_ptHome = pChar->GetTopPoint();
		}
		m_Home_Dist_Wander = vVal.GetInt();
		break;
	case P_Knowledge:
	case P_Speech:
		// ??? Check that it is not already part of Char_GetDef()->m_Speech
		if ( g_World.m_iLoadVersion < 53 )
		{
			break;
		}
		return( m_Speech.v_Set( vVal, RES_Speech ));
	case P_Need:
	case P_NeedName:
		if ( g_World.m_iLoadVersion < 53 )
		{
			break;
		}
		{
			LPCTSTR pTmp = vVal.GetPSTR();
			m_Need.LoadResQty(pTmp);
		}
		break;

	case P_VendCap:
		{
			CItemContainerPtr pBank = pChar->GetBank();
			ASSERT(pBank);
			pBank->m_itEqBankBox.m_Check_Restock = vVal.GetInt();
		}
		break;
	case P_VendGold:
		{
			CItemContainerPtr pBank = pChar->GetBank();
			ASSERT(pBank);
			pBank->m_itEqBankBox.m_Check_Amount = vVal.GetInt();
		}
		break;

	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CCharNPC::s_PropGetNPC( CChar* pChar, int iProp, CGVariant& vValRet )
{
	ASSERT(pChar);
	switch ( iProp )
	{
	case P_ActPri:
		vValRet.SetInt( m_Act_Motivation );
		break;
	case P_Brain:
	case P_NPC:
		vValRet.SetInt( m_Brain );
		break;
	case P_HomeDist:
		vValRet.SetInt( m_Home_Dist_Wander );
		break;
	case P_Knowledge:
	case P_Speech:
		// Write out all the speech blocks.
		m_Speech.v_Get( vValRet );
		break;
	case P_Need:
		{
			TCHAR szTmp[ CSTRING_MAX_LEN ];
			m_Need.WriteKey( szTmp );
			vValRet = szTmp;
		}
		break;
	case P_NeedName:
		{
			TCHAR szTmp[ CSTRING_MAX_LEN ];
			m_Need.WriteNameSingle( szTmp );
			vValRet = szTmp;
		}
		break;
	case P_VendCap:
		{
			CItemContainerPtr pBank = pChar->GetBank();
			ASSERT(pBank);
			vValRet.SetInt( pBank->m_itEqBankBox.m_Check_Restock );
		}
		break;
	case P_VendGold:
		{
			CItemContainerPtr pBank = pChar->GetBank();
			ASSERT(pBank);
			vValRet.SetInt( pBank->m_itEqBankBox.m_Check_Amount );
		}
		break;
	default:
		DEBUG_CHECK(0);
		return(HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}

void CCharNPC::s_SerializeNPC( CChar* pChar, CGFile& a ) // const
{
}

void CCharNPC::s_WriteNPC( CChar* pChar, CScript& s )
{
	ASSERT(pChar);
	// This says we are an NPC.
	s.WriteKeyInt( "NPC", m_Brain );

	if ( m_Home_Dist_Wander < SHRT_MAX )
		s.WriteKeyInt( "HOMEDIST", m_Home_Dist_Wander );
	if ( m_Act_Motivation )
		s.WriteKeyDWORD( "ACTPRI", m_Act_Motivation );

	m_Speech.s_WriteProps( s, "SPEECH" );

	if ( m_Need.GetResourceID().IsValidRID())
	{
		TCHAR szTmp[ CSTRING_MAX_LEN ];
		m_Need.WriteKey( szTmp );
		s.WriteKey( "NEED", szTmp );
	}
}


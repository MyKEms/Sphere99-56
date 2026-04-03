//
// CChar.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//  CChar is either an NPC or a Player.
//

#include "stdafx.h"	// predef header.

/////////////////////////////////////////////////////////////////
// -CChar

const CScriptProp CChar::sm_Props[ CChar::P_QTY+1] =
{
#define CCHARPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "ccharprops.tbl"
#undef CCHARPROP
	NULL,
};

#ifdef USE_JSCRIPT
#define CCHARMETHOD(a,b,c,d) JSCRIPT_METHOD_IMP(CChar,a)
#include "ccharmethods.tbl"
#undef CCHARMETHOD
#endif

const CScriptMethod CChar::sm_Methods[CChar::M_QTY+1] =
{
#define CCHARMETHOD(a,b,c,d) CSCRIPT_METHOD_IMP(a,b,c d)
#include "ccharmethods.tbl"
#undef CCHARMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP2(Char,CChar::sm_Props,CChar::sm_Methods,NULL,ObjBase);

template<>
void CScriptClassTemplate<CChar>::InitScriptClass()
{
	// Add skills and stats list to the class def.
	if ( IsInit())
		return;
	AddSubClass( &CContainer::sm_ScriptClass );
	AddSubClass( &CCharNPC::sm_ScriptClass );
	AddSubClass( &CCharPlayer::sm_ScriptClass );
	AddSubClass( &CClient::sm_ScriptClass );

	// StatKeys
	// SkillKeys

	CScriptClass::InitScriptClass();
}

CChar::CChar( CREID_TYPE baseID ) : CObjBase(UID_INDEX_CLEAR)
{
	g_Serv.StatInc( SERV_STAT_CHARS );	// Count created CChars.

	m_StatFlag = 0;
	if ( g_World.m_fSaveParity )
	{
		StatFlag_Set( STATF_SaveParity );	// It will get saved next time.
	}

	m_dirFace = DIR_SE;
	m_fonttype = FONT_NORMAL;
	m_ArmorDisplay = 0;
	m_SpeechHue = HUE_TEXT_DEF;
	m_zClimbHeight = SPHEREMAP_SIZE_Z;
	m_dirClimb = DIR_INVALID;
	m_Act.Init();

	memset( m_Stat, 0, sizeof(m_Stat));
	memset( m_Skill, 0, sizeof(m_Skill));

	int i=0;
	for ( ;i<STAT_BASE_QTY;i++)
	{
		m_Stat[i] = 1;
	}

	m_timeLastStatRegen = m_timeCreate;

	m_prev_Hue = HUE_DEFAULT;
	m_prev_id = CREID_INVALID;

	CCharDefPtr pCharDef = SetID( baseID );
	ASSERT(pCharDef);

	Stat_Set( STAT_Food, pCharDef->m_MaxFood );

	Skill_Cleanup();

	CSphereThread::GetCurrentThread()->m_uidLastNewChar = GetUID();	// for script access.
}

CChar::~CChar() // Delete character
{
	NPC_Clear();
	Player_Clear();
	g_Serv.StatDec( SERV_STAT_CHARS );
}

CCharPtr CChar::CreateBasic( CREID_TYPE baseID ) // static
{
	// Create the "basic" NPC. Not NPC or player yet.
	// NOTE: NEVER return NULL
	return( new CChar( baseID ));
}

CCharPtr CChar::CreateNPC( CREID_TYPE baseID )	// static
{
	// Create an NPC
	// NOTE: NEVER return NULL
	CCharPtr pChar = CreateBasic( baseID );
	ASSERT(pChar);
	pChar->NPC_LoadScript(true);
	return( pChar );
}

void CChar::DeleteThis()
{
	if ( IsStatFlag( STATF_Ridden ))
	{
		CItemPtr pItem = Horse_GetMountItem();
		if ( pItem )
		{
			pItem->m_itFigurine.m_uidChar.InitUID();	// unlink it first.
			pItem->DeleteThis();
		}
	}

	if ( IsClient())	// this should never happen.
	{
		m_pClient->m_Socket.Close();
		m_pClient->CharDisconnect();
	}

	if ( m_pParty )
	{
		m_pParty->RemoveChar( GetUID(), GetUID());
		m_pParty.ReleaseRefObj();
	}

	DeleteAll();		// remove contents early so virtuals will work.

	CObjBase::DeleteThis();
}

void CChar::ClientDetach()
{
	// Client is detaching from this CChar.
	// But the CChar may not disconnect right away !

	if ( ! IsClient())
		return;

	CancelAllTrades();

	if ( m_pParty && m_pParty->IsPartyMaster( this ))
	{
		// Party must disband if the master is logged out.
		m_pParty->Disband(GetUID());
		m_pParty.ReleaseRefObj();
	}

	// remove standard client linkage
	{ CGVariant vTmp1("-e_AllPlayers"); m_Events.v_Set( vTmp1, RES_Events ); }
	{ CGVariant vTmp2("-spk_AllPlayers"); m_Events.v_Set( vTmp2, RES_Speech ); }

	CSectorPtr pSector = GetTopSector();
	pSector->ClientDetach( this );

	m_pClient.ReleaseRefObj();
}

void CChar::ClientAttach( CClient* pClient )
{
	// Client is Attaching to this CChar.
	if ( GetClient() == pClient )
		return;

	DEBUG_CHECK( ! IsClient());
	CAccountPtr pAccount = pClient->GetAccount();
	DEBUG_CHECK(pAccount);
	HRESULT hRes = Player_SetAccount( pAccount );
	if ( IS_ERROR(hRes))	// i now own this char.
		return;

	ASSERT(m_pPlayer);
	m_pPlayer->m_timeLastUsed.InitTimeCurrent();

	// Attach standard event handlers for all clients.
	{ CGVariant vTmp1("+e_AllPlayers"); m_Events.v_Set( vTmp1, RES_Events ); }
	{ CGVariant vTmp2("+spk_AllPlayers"); m_Events.v_Set( vTmp2, RES_Speech ); }

	m_pClient = pClient;
	GetTopSector()->ClientAttach( this );
}

void CChar::SetDisconnected()
{
	// Client logged out or NPC is dead.
	if ( IsClient())
	{
		GetClient()->m_Socket.Close();
		return;
	}
	if ( m_pParty )
	{
		m_pParty->RemoveChar( GetUID(), GetUID());
		m_pParty.ReleaseRefObj();
	}
	if ( IsDisconnected())
		return;

	// If this char is on a IT_SHIP then we need to stop the ship !
	if ( m_pArea && m_pArea->IsFlag( REGION_FLAG_SHIP ))
	{
		CItemPtr pItem = g_World.ItemFind( m_pArea->GetUIDIndex());
		CItemMultiPtr pShipItem = REF_CAST(CItemMulti,pItem);
		if ( pShipItem )
		{
			pShipItem->Ship_Stop();
		}
	}

	if ( IsStatFlag( STATF_Sleeping ))
	{
		// We must recover our body before we log out.
		SleepWake();
		// !? We really should come back sleeping.
	}

	DEBUG_CHECK( GetParent());
	RemoveFromView();	// Remove from views.
	// DEBUG_MSG(( "Disconnect '%s'" LOG_CR, (LPCTSTR) GetName()));
	MoveToRegion(NULL,false);
	GetTopSector()->m_Chars_Disconnect.InsertHead( this );
	DEBUG_CHECK( IsDisconnected());
}

int CChar::IsWeird() const
{
	// RETURN: invalid code.

	int iResultCode = CObjBase::IsWeird();
	if ( iResultCode )
	{
	bailout:
		return( iResultCode );
	}

	if ( IsDisconnected())
	{
		if ( ! GetTopSector()->IsCharDisconnectedIn( this ))
		{
			iResultCode = 0x1102;
			goto bailout;
		}
		if ( m_pNPC )
		{
			if ( IsStatFlag( STATF_Ridden ))
			{
				if ( Skill_GetActive() != NPCACT_RIDDEN )
				{
					iResultCode = 0x1103;
					goto bailout;
				}
			}
			else
			{
				if ( ! IsStatFlag( STATF_DEAD ))
				{
					iResultCode = 0x1106;
					goto bailout;
				}
			}
		}
	}

	if ( ! m_pPlayer && ! m_pNPC )
	{
		iResultCode = 0x1107;
		goto bailout;
	}

	if ( ! GetTopPoint().IsValidPoint())
	{
		iResultCode = 0x1108;
		goto bailout;
	}

	return( 0 );
}

int CChar::FixWeirdness()
{
	// Clean up weird flags.
	// fix Weirdness.
	// NOTE:
	//  Deleting a player char is VERY BAD ! Be careful !
	//
	// RETURN: false = i can't fix this.

	int iResultCode = CObjBase::IsWeird();
	if ( iResultCode )
	{
	bailout:
		// Not recoverable - must try to delete the object.
		return( iResultCode );
	}

	// NOTE: Stats and skills may go negative temporarily.

	CCharDefPtr pCharDef = Char_GetDef();

	if ( g_World.m_iLoadVersion <= 35 )
	{
		if ( IsStatFlag( STATF_Ridden ))
		{
			m_Act.m_atRidden.m_FigurineUID.InitUID();
			Skill_Start( NPCACT_RIDDEN );
		}
	}
	if ( g_World.m_iLoadVersion < 54 )
	{
		StatFlag_Clear( STATF_RespawnNPC );	// meant nothing then
	}

	// Make sure my flags are good.

	if ( IsStatFlag( STATF_Insubstantial ))
	{
		if ( ! IsStatFlag( STATF_DEAD ) && ! IsGM() && GetPrivLevel() < PLEVEL_Seer )
		{
			StatFlag_Clear( STATF_Insubstantial );
		}
	}
	if ( IsStatFlag( STATF_HasShield ))
	{
		CItemPtr pShield = LayerFind( LAYER_HAND2 );
		if ( pShield == NULL )
		{
			StatFlag_Clear( STATF_HasShield );
		}
	}
	if ( IsStatFlag( STATF_OnHorse ))
	{
		CItemPtr pHorse = LayerFind( LAYER_HORSE );
		if ( pHorse == NULL )
		{
			StatFlag_Clear( STATF_OnHorse );
		}
	}
	if ( IsStatFlag( STATF_Spawned ))
	{
		CItemMemoryPtr pMemory = Memory_FindTypes( MEMORY_ISPAWNED );
		if ( pMemory == NULL )
		{
			StatFlag_Clear( STATF_Spawned );
		}
	}
	if ( IsStatFlag( STATF_Pet ))
	{
		CItemMemoryPtr pMemory = Memory_FindTypes( MEMORY_IPET );
		if ( pMemory == NULL )
		{
			StatFlag_Clear( STATF_Pet );
		}
	}
	if ( IsStatFlag( STATF_Ridden ))
	{
		// Move the ridden creature to the same location as it's rider.
		if ( m_pPlayer || ! IsDisconnected())
		{
			StatFlag_Clear( STATF_Ridden );
		}
		else
		{
			if ( Skill_GetActive() != NPCACT_RIDDEN )
			{
				iResultCode = 0x1203;
				goto bailout;
			}

			m_Act.m_atRidden.m_FigurineUID = ((UID_INDEX) m_Act.m_atRidden.m_FigurineUID ) &~ RID_F_RESOURCE;	// this gets set sometimes for some reason !?

				// Make sure we are still linked back to the world.

			CItemPtr pFigurine = Horse_GetMountItem();
			if ( pFigurine == NULL )
			{
				iResultCode = 0x1204;
				goto bailout;
			}
			if ( pFigurine->m_itFigurine.m_uidChar != GetUID())
			{
				iResultCode = 0x1205;
				goto bailout;
			}
			CPointMap pt = pFigurine->GetTopLevelObj()->GetTopPoint();
			if ( pt != GetTopPoint())
			{
				MoveToChar( pt );
				SetDisconnected();
			}
		}
	}
	if ( IsStatFlag( STATF_Criminal ))
	{
		// make sure we have a criminal flag timer ?
	}

	if ( ! IsIndividualName() && pCharDef->GetTypeName()[0] == '#' )
	{
		SetName( pCharDef->GetTypeName());
	}
	if ( ! CCharDef::IsValidDispID( GetID()) && CCharDef::IsHumanID( m_prev_id ))
	{
		// This is strange. (has human body)
		m_prev_id = GetID();
	}

	if ( m_StatMaxHealth <= 1 )
	{
		m_StatMaxHealth = m_StatStr;
		m_StatHealth = m_StatMaxHealth;
	}
	if ( m_StatMaxMana <= 1 )
	{
		m_StatMaxMana = m_StatInt;
		m_StatMana = m_StatMaxMana;
	}
	if ( m_StatMaxStam <= 1 )
	{
		m_StatMaxStam = m_StatDex;
		m_StatStam = m_StatMaxStam;
	}

	if ( m_pPlayer )	// Player char.
	{
		// DEBUG_CHECK( ! IsStatFlag( STATF_Pet | STATF_Ridden | STATF_Spawned ));
		Memory_ClearTypes(MEMORY_ISPAWNED|MEMORY_IPET);
		StatFlag_Clear( STATF_Ridden | STATF_Pet | STATF_Spawned );

		if ( m_pPlayer->GetProfession() == NULL )	// this should never happen.
		{
			m_pPlayer->SetProfession( this, CSphereUID( RES_Profession ));
			ASSERT(m_pPlayer->GetProfession());
		}

		// Make sure players don't get ridiculous stats.
		if ( GetPrivLevel() <= PLEVEL_Player )
		{
			for ( int i=SKILL_First; i<=SKILL_Last; i++ )
			{
				int iSkillMax = Skill_GetMax( (SKILL_TYPE)i );
				int iSkillVal = Skill_GetBase( (SKILL_TYPE)i );
				if ( iSkillVal < 0 )
					Skill_SetBase( (SKILL_TYPE)i, 0 );
				if ( iSkillVal > iSkillMax* 2 )
					Skill_SetBase( (SKILL_TYPE)i, iSkillMax );
			}

			// ??? What if magically enhanced !!!
			if ( IsHuman() &&
				GetPrivLevel() < PLEVEL_Counsel &&
				! IsStatFlag( STATF_Polymorph ))
			{
				for ( int j=STAT_Str; j<STAT_BASE_QTY; j++ )
				{
					int iStatMax = Stat_GetMax((STAT_TYPE)j);
					if ( Stat_Get((STAT_TYPE)j) > iStatMax*2 )
					{
						Stat_Set((STAT_TYPE)j, iStatMax );
					}
				}
			}
		}
	}
	else
	{
		if ( ! m_pNPC )
		{
			// Make it into an NPC ???
			iResultCode = 0x1210;
			goto bailout;
		}

		if ( ! _stricmp( GetName(), "ship" ))
		{
			// version .37 cleanup code.
			iResultCode = 0x1211;
			goto bailout;
		}

		// An NPC. Don't keep track of unused skills.
		for ( int i=SKILL_First; i<SKILL_QTY; i++ )
		{
			if ( m_Skill[i] && m_Skill[i] <= 10 )
				Skill_SetBase( (SKILL_TYPE)i, 0 );
		}
	}

	if ( GetTimerAdjusted() > 60*60 )
	{
		// unreasonably long for a char?
		SetTimeout(1);
	}

	// FixWeight();

	return( IsWeird());
}

void CChar::CreateNewCharCheck()
{
	// Creating a new char. (Not loading from save file)
	// Make sure things are set to reasonable values.
	m_prev_id = GetID();
	m_prev_Hue = GetHue();

	m_StatHealth = m_StatMaxHealth;
	m_StatStam = m_StatMaxStam;
	m_StatMana = m_StatMaxMana;

	if ( ! m_pPlayer )	// need a starting brain tick.
	{
		SetTimeout(1);
	}

	// Name was a template/pool name?
	if ( ! IsIndividualName())
	{
		CCharDefPtr pCharDef = Char_GetDef();
		ASSERT(pCharDef);
		LPCSTR pszName = pCharDef->GetTypeName();
		if ( pszName[0] == '#' )	// Create the pool individual name
		{
			SetNamePool(pszName);
		}
	}

	FixWeirdness(); // test any other stuff.
}

bool CChar::ReadScriptTrig( CCharDef* pCharDef, CCharDef::T_TYPE_ trig )
{
	if ( pCharDef == NULL )
		return( false );

	// NOTE: Use OnTriggerScript instead ?

	if ( ! pCharDef->HasTrigger( trig ))
		return( false );
	CResourceLock sLock(pCharDef);
	if ( ! sLock.IsFileOpen())
		return( false );
	if ( ! sLock.FindTriggerName( CCharDef::sm_Triggers[trig].m_pszName ))
		return( false );
	return( ReadScript( sLock ));
}

bool CChar::ReadScript( CResourceLock& s )
{
	// Read scripts in a special way.
	// If this is a regen they will have a pack already.
	// fRestock = this is a vendor restock.
	// fNewbie = newbiize all the stuff we get.
	// NOTE:
	//  It would be nice to know the total $ value of items created here !
	// RETURN:
	//  true = default return. (mostly ignored).

	bool fIgnoreAttributes = false;
	CSphereExpContext exec(this,&g_Serv);
	CItemPtr pItem;

	while ( s.ReadKeyParse())
	{
		if ( s.IsLineTrigger())
			break;

		ITC_TYPE iProp = (ITC_TYPE) s_FindKeyInTable( s.GetKey(), CItem::sm_szTemplateTable );
		switch ( iProp )
		{
		case ITC_ITEM:
		case ITC_CONTAINER:
		case ITC_ITEMNEWBIE:
			{
				// Possible loot/equipped item.
				fIgnoreAttributes = true;

				if ( IsStatFlag( STATF_Conjured ) && iProp != ITC_ITEMNEWBIE ) // This check is not needed.
					break; // conjured creates have no loot.

				pItem = CItem::CreateHeader( s.GetArgVar(), this, this, iProp == ITC_ITEMNEWBIE );
				if ( pItem == NULL )
					continue;

				if ( iProp == ITC_ITEMNEWBIE )
				{
					pItem->SetAttr(ATTR_NEWBIE);
				}

				if ( pItem->IsItemInContainer() || pItem->IsItemEquipped())
					fIgnoreAttributes = false;
			}
			continue;
		case ITC_BUY:
		case ITC_SELL:
			{
				fIgnoreAttributes = true;

				CItemContainerPtr pCont = GetBank((iProp == ITC_SELL) ? LAYER_VENDOR_STOCK : LAYER_VENDOR_BUYS );
				if ( pCont == NULL )
				{
					DEBUG_ERR(( "NPC '%s', is not a vendor!" LOG_CR, (LPCTSTR) GetResourceName()));
					continue;
				}
				pItem = CItem::CreateHeader( s.GetArgVar(), pCont, this, false );
				if ( pItem == NULL )
					continue;

				if ( pItem->IsItemInContainer())
				{
					fIgnoreAttributes = false;
					pItem->SetContainedLayer( pItem->GetAmount());	// set the Restock amount.
				}
			}
			continue;
		}

		if ( fIgnoreAttributes )	// some item creation failure.
			continue;

		if ( pItem)
		{
			// Modify the item.
			pItem->s_PropSet( s.GetKey(), s.GetArgVar());
		}
		else
		{
			// Modify the cchar. Allow full scripts to run.
			TRIGRET_TYPE tRet = exec.ExecuteScript( s, TRIGRUN_SINGLE_EXEC );
			if ( tRet != TRIGRET_RET_DEFAULT )
			{
				return( tRet == TRIGRET_RET_FALSE );
			}
		}
	}

	return( true );
}

void CChar::NPC_LoadScript( bool fRestock )
{
	// Create an NPC from script.
	if ( ! m_pNPC)
	{
		// Set a default brian type til we get the real one from scripts.
		NPC_SetBrain( GetCreatureType());	// should have a default brain. watch out for override vendor.
	}

	CCharDefPtr pCharDef = Char_GetDef();
	ReadScriptTrig( pCharDef, CCharDef::T_Create );

	if ( fRestock )
	{
		ReadScriptTrig( pCharDef, CCharDef::T_NPCRestock );
	}

	if ( NPC_IsVendor())
	{
		// Restock it now so it can buy/sell stuff immediately
		NPC_Vendor_Restock( 15*60 );
	}

	CreateNewCharCheck();
}

void CChar::OnWeightChange( int iChange )
{
	CContainer::OnWeightChange( iChange );
	UpdateStatsFlag();
}

CGString CChar::GetName() const // virtual
{
	if ( ! IsIndividualName())			// allow some creatures to go unnamed.
	{
		CCharDefPtr pCharDef = Char_GetDef();
		ASSERT(pCharDef);
		return( pCharDef->GetTypeName());	// Just use it's type name instead.
	}

	return( CObjBase::GetName());
}

bool CChar::SetName( LPCTSTR pszName )
{
	return SetNamePool( pszName );
}

CCharDefPtr CChar::SetID( CREID_TYPE id )
{
	// Set body display type.
	// NEVER FAIL!

	CCharDefPtr pCharDef = g_Cfg.FindCharDef( id );
	if ( pCharDef == NULL )
	{
		// Unknown id type. try to find a default type?
		if ( id != -1 && id != CREID_INVALID )
		{
			DEBUG_ERR(( "Create Invalid Char 0%x" LOG_CR, id ));
		}
		pCharDef = Char_GetDef(); // just keep the old type.
		if ( pCharDef == NULL )
		{
			id = (CREID_TYPE) g_Cfg.ResourceGetIndexType( RES_CharDef, "DEFAULTCHAR" );
			if ( id < 0 )
			{
				id = CREID_OGRE;
			}
			pCharDef = g_Cfg.FindCharDef(id);
			ASSERT(pCharDef);
		}
	}

	if ( pCharDef == Char_GetDef())	// already set.
		return pCharDef;

	m_BaseRef = pCharDef;

	if ( m_prev_id == CREID_INVALID )
	{
		m_prev_id = GetID();
	}

	if ( GetCreatureType() != NPCBRAIN_HUMAN )
	{
		// Transform to non-human (if they ever where human)
		// can't ride a horse in this form.
		Horse_UnMount();
		UnEquipAllItems(); 		// unequip all items.
	}

	UpdateX();
	return pCharDef;
}

void CChar::InitPlayer( const CUOEvent* pBin, CClient* pClient )
{
	// XCMD_Create
	// Create a brand new Player char.
	ASSERT(pClient);
	ASSERT(pBin);

	fprintf(stderr, "[NET] InitPlayer: SetID...\n"); fflush(stderr);
	SetID( (CREID_TYPE) g_Cfg.ResourceGetIndexType( RES_CharDef, ( pBin->Create.m_sex == 0 ) ? "c_MAN" : "c_WOMAN" ));
	fprintf(stderr, "[NET] InitPlayer: SetID done\n"); fflush(stderr);

	TCHAR szName[ MAX_NAME_SIZE+1 ];
	int ilen = NameStrip( szName, pBin->Create.m_charname, MAX_NAME_SIZE );
	if ( ! ilen )	// Is the name unacceptable?
	{
		g_Log.Event( LOG_GROUP_ACCOUNTS, LOGL_WARN,
			"%x:Unacceptable name '%s'" LOG_CR,
			pClient->m_Socket.GetSocket(), (LPCTSTR) szName );
		SetNamePool( ( pBin->Create.m_sex ) ? "#HUMANFEMALE" : "#HUMANMALE" );
	}
	else
	{
		SetName( szName );
	}

	HUE_TYPE wHue;
	wHue = pBin->Create.m_wSkinHue | HUE_UNDERWEAR;
	if ( wHue < (HUE_UNDERWEAR|HUE_SKIN_LOW) || wHue > (HUE_UNDERWEAR|HUE_SKIN_HIGH))
	{
		wHue = HUE_UNDERWEAR|HUE_SKIN_LOW;
	}
	SetHue( wHue );
	m_fonttype = FONT_NORMAL;

	int iStartLoc = pBin->Create.m_startloc-1;
	if ( ! g_Cfg.m_StartDefs.IsValidIndex( iStartLoc ))
		iStartLoc = 0;
	m_ptHome = g_Cfg.m_StartDefs[iStartLoc]->m_pt;

	if ( ! m_ptHome.IsValidPoint())
	{
		if ( g_Cfg.m_StartDefs.GetSize())
		{
			m_ptHome = g_Cfg.m_StartDefs[0]->m_pt;
		}
		DEBUG_ERR(( "Invalid start location for character!" LOG_CR ));
	}

	fprintf(stderr, "[NET] InitPlayer: home=%d,%d startloc=%d\n", m_ptHome.m_x, m_ptHome.m_y, iStartLoc); fflush(stderr);
	SetUnkPoint( m_ptHome );	// Don't actaully put me in the world yet.

	// randomize the skills first.
	int i = SKILL_First;
	for ( ; i < SKILL_QTY; i++ )
	{
		Skill_SetBase( (SKILL_TYPE)i, Calc_GetRandVal( g_Cfg.m_iMaxBaseSkill ));
	}

	if ( pBin->Create.m_str + pBin->Create.m_dex + pBin->Create.m_int > 80 )
	{
		// ! Cheater !
		Stat_Set( STAT_Str, 10 );
		Stat_Set( STAT_Dex, 10 );
		Stat_Set( STAT_Int, 10 );
		g_Log.Event( LOG_GROUP_CHEAT, LOGL_WARN,
			"%x:Cheater is submitting hacked char stats" LOG_CR,
			pClient->m_Socket.GetSocket());
	}
	else
	{
		Stat_Set( STAT_Str, pBin->Create.m_str + 1 );
		Stat_Set( STAT_Dex, pBin->Create.m_dex + 1 );
		Stat_Set( STAT_Int, pBin->Create.m_int + 1 );
	}

	if ( pBin->Create.m_val1 > 50 ||
		pBin->Create.m_val2 > 50 ||
		pBin->Create.m_val3 > 50 ||
		pBin->Create.m_val1 + pBin->Create.m_val2 + pBin->Create.m_val3 > 101 )
	{
		// ! Cheater !
		g_Log.Event( LOG_GROUP_CHEAT, LOGL_WARN,
			"%x:Cheater is submitting hacked char skills" LOG_CR,
			pClient->m_Socket.GetSocket());
	}
	else
	{
		if ( IsSkillBase((SKILL_TYPE) pBin->Create.m_skill1))
			Skill_SetBase( (SKILL_TYPE) pBin->Create.m_skill1, pBin->Create.m_val1*10 );
		if ( IsSkillBase((SKILL_TYPE) pBin->Create.m_skill2))
			Skill_SetBase( (SKILL_TYPE) pBin->Create.m_skill2, pBin->Create.m_val2*10 );
		if ( IsSkillBase((SKILL_TYPE) pBin->Create.m_skill3))
			Skill_SetBase( (SKILL_TYPE) pBin->Create.m_skill3, pBin->Create.m_val3*10 );
	}

	ITEMID_TYPE id = (ITEMID_TYPE)(WORD) pBin->Create.m_hairid;
	if ( id )
	{
		CItemPtr pHair = CItem::CreateScript( id, this );
		ASSERT(pHair);
		if ( ! pHair->IsType(IT_HAIR))
		{
			// Cheater !
			pHair->DeleteThis();
		}
		else
		{
			wHue = pBin->Create.m_hairHue;
			if ( wHue<HUE_HAIR_LOW || wHue > HUE_HAIR_HIGH )
			{
				wHue = HUE_HAIR_LOW;
			}
			pHair->SetHue( wHue );
			pHair->SetAttr(ATTR_NEWBIE|ATTR_MOVE_NEVER);
			LayerAdd( pHair, LAYER_HAIR );	// add content
		}
	}

	id = (ITEMID_TYPE)(WORD) pBin->Create.m_beardid;
	if ( id )
	{
		CItemPtr pBeard = CItem::CreateScript( id, this );
		ASSERT(pBeard);
		if ( ! pBeard->IsType(IT_BEARD))
		{
			// Cheater !
			pBeard->DeleteThis();
		}
		else
		{
			wHue = pBin->Create.m_beardHue;
			if ( wHue < HUE_HAIR_LOW || wHue > HUE_HAIR_HIGH )
			{
				wHue = HUE_HAIR_LOW;
			}
			pBeard->SetHue( wHue );
			pBeard->SetAttr(ATTR_NEWBIE|ATTR_MOVE_NEVER);
			LayerAdd( pBeard, LAYER_BEARD );	// add content
		}
	}

	fprintf(stderr, "[NET] InitPlayer: stats done, creating bank/pack...\n"); fflush(stderr);
	// Create the bank box.
	CItemContainerPtr pBankBox = GetBank( LAYER_BANKBOX );
	// Create the pack.
	CItemContainerPtr pPack = GetPackSafe();

	fprintf(stderr, "[NET] InitPlayer: bank/pack done, loading newbie items...\n"); fflush(stderr);
	// Get special equip for the starting skills.
	for ( i=0; i<4; i++ )
	{
		int iSkill;
		if (i)
		{
			switch ( i )
			{
			case 1: iSkill = pBin->Create.m_skill1; break;
			case 2: iSkill = pBin->Create.m_skill2; break;
			case 3: iSkill = pBin->Create.m_skill3; break;
			}
		}
		else
		{
			iSkill = ( pBin->Create.m_sex == 0 ) ? RES_NEWBIE_MALE_DEFAULT : RES_NEWBIE_FEMALE_DEFAULT;
		}

		CResourceLock s( g_Cfg.ResourceGetDef( CSphereUID( RES_Newbie, iSkill )));
		if ( ! s.IsFileOpen())
			continue;
		ReadScript( s );
	}

	if ( pClient->m_ProtoVer.GetCryptVer() >= 0x126000 )
	{
		HUE_TYPE wHue = pBin->Create.m_shirtHue;
		CItemPtr pLayer = LayerFind( LAYER_SHIRT );
		if ( pLayer )
		{
			pLayer->SetHue( wHue );
		}
		wHue = pBin->Create.m_pantsHue;
		pLayer = LayerFind( LAYER_PANTS );
		if ( pLayer )
		{
			pLayer->SetHue( wHue );
		}
	}

	CreateNewCharCheck();
}

HRESULT CChar::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	// ARGS:
	//  vValRet = return the value here.

	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		int iProp = s_FindKeyInTable( pszKey, CClient::sm_Props );
		if ( iProp >= 0 )
		{
			if ( ! IsClient())	// just ignore client commands.
				return( NO_ERROR );
			return GetClient()->s_PropGet( iProp, vValRet, pSrc );
		}
		iProp = s_FindKeyInTable( pszKey, CCharPlayer::sm_Props );
		if ( iProp >= 0 )
		{
			if ( m_pPlayer == NULL ) // ignore non players.
				return NO_ERROR;
			return m_pPlayer->s_PropGetPlayer( this, iProp, vValRet );
		}
		iProp = s_FindKeyInTable( pszKey, CCharNPC::sm_Props );
		if ( iProp >= 0 )
		{
			if ( m_pNPC == NULL ) // ignore non players.
				return NO_ERROR;
			return m_pNPC->s_PropGetNPC( this, iProp, vValRet );
		}

		// Special write values. NOTE: Skills/Stats/Spells should all be the same!?
		STAT_TYPE iStat = g_Cfg.FindStatKey( pszKey, false );
		if ( iStat >= 0 )
		{
			vValRet.SetInt( Stat_Get( iStat ));
			return( NO_ERROR );
		}

		SKILL_TYPE iSkill = g_Cfg.FindSkillKey( pszKey, false );
		if ( iSkill >= 0 && IsSkillBase( iSkill ))
		{
			// Check some skill name.
			vValRet.SetInt( Skill_GetBase( iSkill ));
			return( NO_ERROR );
		}

		return( CObjBase::s_PropGet( pszKey, vValRet, pSrc ));
	}

	CCharDefPtr pCharDef = Char_GetDef();
	ASSERT(pCharDef); ASSERT(pSrc);
	CCharPtr pCharSrc = GET_ATTACHED_CCHAR(pSrc);

	switch ( iProp )
	{

	case P_Region:
		vValRet.SetRef( m_pArea );
		break;
	case P_Account:
		// May be an NPC?
		vValRet.SetRef( GetAccount());
		break;
	case P_AccountName:
		// May be an NPC?
		if ( GetAccount() == NULL )
			return HRES_PRIVILEGE_NOT_HELD;
		vValRet = GetAccount()->GetName();
		break;
	case P_NPC:
		if ( m_pNPC == NULL )
		{
			vValRet.SetInt(NPCBRAIN_NONE);
		}
		else
		{
			vValRet.SetInt( m_pNPC->m_Brain );
		}
		break;

	case P_AC:
		vValRet.SetInt( m_ArmorDisplay );
		break;
	case P_BankBalance:
		vValRet.SetInt( GetBank()->ContentCount( CSphereUID(RES_TypeDef,IT_GOLD)));
		break;
	case P_GuildAbbrev:
		{
			LPCTSTR pszAbbrev = Guild_Abbrev(MEMORY_GUILD);
			vValRet = ( pszAbbrev ) ? pszAbbrev : "";
		}
		break;
	case P_GuildMem:
		vValRet.SetRef( Memory_FindTypes(MEMORY_GUILD));
		break;
	case P_Body:
		// The artwork id.
		vValRet = g_Cfg.ResourceGetName( CSphereUID( RES_CharDef, GetDispID()));
		break;
	case P_Id:
		// The type definition 
		vValRet = g_Cfg.ResourceGetName( pCharDef->GetUIDIndex());
		break;
	case P_SpeechColor:
		vValRet.SetDWORD( m_SpeechHue );
		break;
	case P_TownAbbrev:
		{
			LPCTSTR pszAbbrev = Guild_Abbrev(MEMORY_TOWN);
			vValRet = ( pszAbbrev ) ? pszAbbrev : "";
		}
		break;
	case P_TownMem:
		vValRet.SetRef( Memory_FindTypes(MEMORY_TOWN));
		break;

	case P_WeightMax:	// use WEIGHT_UNITS ?
		vValRet.SetInt( g_Cfg.Calc_MaxCarryWeight(this));
		break;

	case P_Act:
		vValRet.SetRef( g_World.ObjFind(m_Act.m_Targ));
		break;
	case P_Action:
		vValRet = g_Cfg.ResourceGetName( CSphereUID( RES_Skill, Skill_GetActive()));
		break;
	case P_ActArg1:
		vValRet.SetDWORD( m_Act.m_atUnk.m_Arg1); // may be uid ?
		break;
	case P_ActArg2:
		vValRet.SetDWORD( m_Act.m_atUnk.m_Arg2 );
		break;
	case P_ActArg3:
		vValRet.SetDWORD( m_Act.m_atUnk.m_Arg3 );
		break;
	case P_Dir:
		vValRet.SetInt( m_dirFace );
		break;
	case P_Flags:
		// Extended for STATF_Stone etc.
		vValRet.SetDWORD( m_StatFlag );
		break;
	case P_Font:
		vValRet.SetInt( m_fonttype );
		break;
	case P_Home:
		m_ptHome.v_Get(vValRet);
		break;
	case P_OBody:
		vValRet = g_Cfg.ResourceGetName( CSphereUID( RES_CharDef, m_prev_id ));
		break;
	case P_OSkin:
		vValRet.SetDWORD( m_prev_Hue );
		break;
	case P_P:
		GetUnkPoint().v_Get(vValRet);
		break;
	case P_Profile:
		m_TagDefs.FindKeyVar( "Profile", vValRet );
		break;
	case P_Title:
		m_TagDefs.FindKeyVar( "Title", vValRet );
		break;
	case P_Nightsight:
		vValRet.SetBool( IsStatFlag( STATF_NightSight ));
		break;
	case P_Stone:
		vValRet.SetBool( IsStatFlag( STATF_Stone ));
		break;

	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CChar::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	// Handle P= property for position
	if ( ! _stricmp(pszKey, "P") )
	{
		CPointMap pt;
		pt.v_Set( vVal );
		MoveToChar( pt );
		return NO_ERROR;
	}

	// Handle Account= directly (critical for world loading)
	if ( ! _stricmp(pszKey, "Account") || ! _stricmp(pszKey, "AccountName") )
	{
		return Player_SetAccount( vVal.GetPSTR() );
	}

	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		int iProp = s_FindKeyInTable( pszKey, CClient::sm_Props );
		if ( iProp >= 0 )
		{
			if ( ! IsClient())	// just ignore client commands.
				return HRES_INVALID_HANDLE;
			return GetClient()->s_PropSet( iProp, vVal );
		}

		s_FixExtendedProp( pszKey, "SkillLock", vVal );
		iProp = s_FindKeyInTable( pszKey, CCharPlayer::sm_Props );
		if ( iProp >= 0 )
		{
			if ( m_pPlayer == NULL )	// ignore this.
				return HRES_INVALID_HANDLE;
			return m_pPlayer->s_PropSetPlayer( this, iProp, vVal );
		}
		iProp = s_FindKeyInTable( pszKey, CCharNPC::sm_Props );
		if ( iProp >= 0 )
		{
			if ( m_pNPC == NULL )	// ignore this
				return HRES_INVALID_HANDLE;
			return m_pNPC->s_PropSetNPC( this, iProp, vVal );
		}

		// Skills and stats.
		STAT_TYPE iStat = g_Cfg.FindStatKey( pszKey, false );
		if ( iStat >= 0 )
		{
			Stat_Set( iStat, vVal.GetInt());
			return NO_ERROR;
		}

		SKILL_TYPE iSkill = g_Cfg.FindSkillKey( pszKey, false );
		if ( iSkill != SKILL_NONE )
		{
			// Check some skill name.
			Skill_SetBase( iSkill, vVal.GetInt());
			return NO_ERROR;
		}

		return( CObjBase::s_PropSet( pszKey, vVal ));
	}

	switch (iProp)
	{
	case P_Account:
	case P_AccountName:
		return Player_SetAccount( vVal );
	case P_Act:
		m_Act.m_Targ = vVal.GetInt();
		break;
	case P_ActArg1:
		m_Act.m_atUnk.m_Arg1 = vVal.GetInt();
		break;
	case P_ActArg2:
		m_Act.m_atUnk.m_Arg2 = vVal.GetInt();
		break;
	case P_ActArg3:
		m_Act.m_atUnk.m_Arg3 = vVal.GetInt();
		break;
	case P_Action:
		return Skill_Start( g_Cfg.FindSkillKey( vVal.GetPSTR(), true ));
	case P_Body:
		SetID( (CREID_TYPE) g_Cfg.ResourceGetIndexType( RES_CharDef, vVal.GetStr()));
		break;
	case P_Dir:
		m_dirFace = (DIR_TYPE) vVal.GetInt();
		if ( m_dirFace < 0 || m_dirFace >= DIR_QTY )
			m_dirFace = DIR_SE;
		break;
	case P_Flags:
		// DO NOT MODIFY STATF_SaveParity, STATF_Spawned, STATF_Pet
		m_StatFlag = ( vVal.GetInt() &~ (STATF_SaveParity|STATF_Pet|STATF_Spawned)) | ( m_StatFlag& (STATF_SaveParity|STATF_Pet|STATF_Spawned) );
		break;
	case P_Font:
		m_fonttype = (FONT_TYPE) vVal.GetInt();
		if ( m_fonttype < 0 || m_fonttype >= FONT_QTY )
			m_fonttype = FONT_NORMAL;
		break;
	case P_Home:
		if ( vVal.IsEmpty())
			m_ptHome = GetTopPoint();
		else
			m_ptHome.v_Set( vVal );
		break;
	case P_Nightsight:
		{
			bool fNightsight;
			if ( vVal.IsEmpty())
			{
				fNightsight = ! IsStatFlag(STATF_NightSight);
			}
			else
			{
				fNightsight = vVal.GetBool();
			}
			StatFlag_Mod( STATF_NightSight, fNightsight );
			Update();
		}
		break;
	case P_NPC:
		return NPC_SetBrain( (NPCBRAIN_TYPE) vVal.GetInt());
	case P_OBody:
		{
			CREID_TYPE id = (CREID_TYPE) g_Cfg.ResourceGetIndexType( RES_CharDef, vVal.GetStr());
			if ( ! g_Cfg.FindCharDef( id ))
			{
				DEBUG_ERR(( "OBODY Invalid Char 0%x" LOG_CR, id ));
				return( HRES_BAD_ARGUMENTS );
			}
			m_prev_id = id;
		}
		break;
	case P_OSkin:
		m_prev_Hue = vVal.GetInt();
		break;
	case P_P:
		{
			CPointMap pt;
			pt.v_Set(vVal);
			MoveToChar(pt);
		}
		break;
	case P_SpeechColor:
		m_SpeechHue = vVal.GetDWORD();
		break;
	case P_Stone:
		{
			bool fSet;
			bool fChange = IsStatFlag(STATF_Stone);
			if ( vVal.IsEmpty())
			{
				fSet = ! fChange;
				fChange = true;
			}
			else
			{
				fSet = vVal.GetBool();
				fChange = ( fSet != fChange );
			}
			StatFlag_Mod(STATF_Stone,fSet);
			if ( fChange )
			{
				RemoveFromView();
				Update();
			}
		}
		break;
	case P_Profile:
		m_TagDefs.SetKeyVar( "Profile", vVal );
		break;
	case P_Title:
		m_TagDefs.SetKeyVar( "TITLE", vVal );
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}

HRESULT CChar::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	// Execute command from script

	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp < 0 )
	{
		HRESULT hRes;
		int iProp = s_FindKeyInTable( pszKey, CClient::sm_Methods );
		if ( iProp >= 0 )
		{
			if ( ! IsClient())	// I am a client so i get an expanded verb set. Things only clients can do.
			{
				// ignore these ?
				return( NO_ERROR );
			}
			return( GetClient()->s_Method( iProp, vArgs, vValRet, pSrc ));
		}
		if ( m_pNPC )
		{
			hRes = s_MethodNPC( pszKey, vArgs, vValRet, pSrc );
			if ( hRes != HRES_UNKNOWN_PROPERTY )
				return(hRes);
		}
		if ( m_pPlayer )
		{
			hRes = s_MethodPlayer( pszKey, vArgs, vValRet, pSrc );
			if ( hRes != HRES_UNKNOWN_PROPERTY )
				return(hRes);
		}

		hRes = s_MethodContainer(pszKey, vArgs, vValRet, pSrc);
		if ( hRes != HRES_UNKNOWN_PROPERTY )
			return hRes;

		return( CObjBase::s_Method( pszKey, vArgs, vValRet, pSrc  ));
	}

	if ( pSrc == NULL )
	{
		return( HRES_PRIVILEGE_NOT_HELD );
	}

	static LPCTSTR const sm_szFameGroups[] =	// display only type stuff.
	{
		"ANONYMOUS",
		"FAMOUS",
		"INFAMOUS",	// get rid of this in the future.
		"KNOWN",
		"OUTLAW",
		NULL,
	};
	static const CAssocStrVal sm_KarmaTitles[] =
	{
		"WICKED", INT_MIN,				// -10000 to -6001
		"BELLIGERENT", -6000,			// -6000 to -2001
		"NEUTRAL", -2000,				// -2000 to  2000
		"KINDLY", 2001,				// 2001 to  6000
		"GOODHEARTED", 6001,		// 6001 to 10000
		NULL, INT_MAX,
	};

	CCharPtr pCharSrc = GET_ATTACHED_CCHAR(pSrc);

	switch ( iProp )
	{
	case M_GuardCheck:
		// uid = some specific target to call on.
		CallGuards(NULL);
		break;
	case M_Karma:
		// Get/Set/Catagorize Karma
		if ( vArgs.IsEmpty())
		{
			vValRet = Stat_Get(STAT_Karma);
		}
		else
		{
			int i = FindTableHead( vArgs, sm_KarmaTitles, sizeof(sm_KarmaTitles[0]));
			if ( i < 0 )
			{
				Stat_Set( STAT_Karma, vArgs.GetInt());
			}
			else
			{
				// What do i think of this person.
				vValRet.SetBool( ! _stricmp( vArgs, sm_KarmaTitles->FindValSorted( Stat_Get(STAT_Karma))));
			}
		}
		break;
	case M_Fame:
		// Get/Set/Catagorize Fame
		if ( vArgs.IsEmpty())
		{
			vValRet = Stat_Get(STAT_Fame);
		}
		else
		{
			int i = FindTableSorted( vArgs, sm_szFameGroups, COUNTOF( sm_szFameGroups )-1 );
			if ( i < 0 )
			{
				Stat_Set( STAT_Fame, vArgs.GetInt());
			}
			else
			{
				// How much respect do i give this person ?
				// Fame is never negative !
				int iFame = Stat_Get(STAT_Fame);
				int iKarma = Stat_Get(STAT_Karma);
				switch (i)
				{
				case 0: // "ANONYMOUS"
					i = ( iFame < 2000 );
					break;
				case 1: // "FAMOUS"
					i = ( iFame > 6000 );
					break;
				case 2: // "INFAMOUS"
					i = ( iFame > 6000 && iKarma <= -6000 );
					break;
				case 3: // "KNOWN"
					i = ( iFame > 2000 );
					break;
				case 4: // "OUTLAW"
					i = ( iFame > 2000 && iKarma <= -2000 );
					break;
				}
				vValRet.SetBool(i);
			}
		}
		break;
	case M_CanCast:
		{
			SPELL_TYPE spell = (SPELL_TYPE) g_Cfg.ResourceGetIndexType( RES_Spell, vArgs );
			if ( spell<0)
				return( HRES_BAD_ARGUMENTS );
			vValRet.SetBool( Spell_CanCast( spell, true, this, false ));
		}
		break;
	case M_CanMake:
		{
			ITEMID_TYPE id = (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, vArgs );
			if ( id<0)
				return( HRES_BAD_ARGUMENTS );
			vValRet.SetBool( Skill_MakeItem( id, UID_INDEX_CLEAR, CSkillDef::T_Select ));
		}
		break;
	case M_IsGM:
		vValRet.SetBool( IsGM());
		break;
	case M_IsPlayer:
		vValRet.SetBool( m_pPlayer ? true : false );
		break;
	case M_IsMyPet:
		vValRet.SetBool( NPC_IsOwnedBy( pCharSrc, true ));
		break;
	case M_IsVendor:
		vValRet.SetBool( NPC_IsVendor());
		break;

	case M_SkillBest:
		// Get the top skill.
		// SKILLBEST(RANKNUM)
		vValRet.SetInt( Skill_GetBest( vArgs.GetInt()));
		break;
	case M_SkillCheck:	
		// check skill in scripts
		// SKILLCHECK(SKILL_ID_NAME,VAL)
		{
			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty < 1 )
				return HRES_BAD_ARG_QTY;
			SKILL_TYPE iSkill = g_Cfg.FindSkillKey( vArgs.GetArrayPSTR(0), true );
			if ( iSkill == SKILL_NONE )
				return( HRES_INVALID_INDEX );
			vValRet.SetBool( Skill_CheckSuccess( iSkill, vArgs.GetArrayInt(1)));
		}
		break;

	case M_Sex:	// <SEX milord/milady>	sep chars are :,/
		{
			bool fFemale = Char_GetDef()->IsFemale(); 
			if ( vArgs.IsEmpty())
			{
				vValRet.SetBool( fFemale );
			}
			else
			{
				vArgs.MakeArraySize();
				vValRet = vArgs.GetArrayPSTR( (fFemale) ? 1 : 0 );
			}
		}
		break;

	case M_FindEquip:
	case M_FindLayer:	// Find equipped layers.
		if ( vArgs.IsEmpty())
			return HRES_BAD_ARG_QTY;
		vValRet.SetRef( LayerFind( (LAYER_TYPE) vArgs.GetInt()));
		break;

	case M_MemoryFindType:	// Find a type of memory.
		if ( vArgs.IsEmpty())
			return HRES_BAD_ARG_QTY;
		vValRet.SetRef( Memory_FindTypes( vArgs.GetDWORD()));
		break;

	case M_Memory:		// Do something with the memory of this pSrc.
	case M_MemoryFind:	// Find a memory of a UID
		if ( vArgs.IsEmpty())
		{
			vValRet.SetRef( Memory_FindObj( pCharSrc ));
		}
		else
		{
			vValRet.SetRef( Memory_FindObj( vArgs.GetUID()));
		}
		break;

	case M_AFK:
		// toggle ?
		{
			bool fAFK = ( Skill_GetActive() == NPCACT_Napping );
			bool fMode;
			if ( !vArgs.IsEmpty())
			{
				fMode = vArgs.GetInt();
			}
			else
			{
				fMode = ! fAFK;
			}
			if ( fMode != fAFK )
			{
				if ( fMode )
				{
					WriteString( "You go into AFK mode" );
					m_Act.m_pt = GetTopPoint();
					Skill_Start( NPCACT_Napping );
				}
				else
				{
					WriteString( "You leave AFK mode" );
					Skill_Start( SKILL_NONE );
				}
			}
		}
		break;

	case M_SkillTotal:
	case M_AllSkills:
		if ( vArgs.IsEmpty())
		{
			int iTotal = 0;
			for ( int i=SKILL_First; i<SKILL_QTY; i++ )
			{
				iTotal += Skill_GetBase((SKILL_TYPE)i);
			}
			vValRet.SetInt( iTotal );
		}
		else
		{
			int iVal = vArgs;
			for ( int i=SKILL_First; i<SKILL_QTY; i++ )
			{
				Skill_SetBase( (SKILL_TYPE)i, iVal );
			}
		}
		break;
	case M_Anim:
		// ANIM, ANIM_TYPE action, bool fBackward = false, BYTE iFrameDelay = 1
		{
			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty < 1 )
				return HRES_BAD_ARG_QTY;

			if ( ! UpdateAnimate( (ANIM_TYPE) vArgs.GetArrayInt(0), false,
				( iArgQty > 1 )	? vArgs.GetArrayInt(1) : false,
				( iArgQty > 2 )	? vArgs.GetArrayInt(2) : 1 ))
			{
				return(HRES_BAD_ARGUMENTS);
			}
		}
		break;
	case M_Attack:
		Fight_Attack( vArgs.IsEmpty() ? pCharSrc : g_World.CharFind( vArgs.GetUID()));
		break;
	case M_Bark:
		SoundChar( (CRESND_TYPE) ( (!vArgs.IsEmpty()) ? vArgs.GetInt() : ( Calc_GetRandVal(2) ? CRESND_RAND1 : CRESND_RAND2 )));
		break;
	case M_Bounce: // 
		return ItemBounce( g_World.ItemFind( vArgs.GetUID()));
	case M_Bow:
		UpdateDir( pCharSrc );
		UpdateAnimate( ANIM_BOW, false );
		break;

	case M_Control: // Possess
		if ( pCharSrc == NULL || ! pCharSrc->IsClient())
			return( HRES_INVALID_HANDLE );
		return( pCharSrc->GetClient()->Cmd_Control( this ));

	case M_Consume:
		{
			CResourceQtyArray Resources;
			Resources.s_LoadKeys( vArgs );
			ResourceConsume( &Resources, 1, false );
		}
		break;
	case M_Criminal:
		if ( !vArgs.IsEmpty() && ! vArgs.GetInt())
		{
			StatFlag_Clear( STATF_Criminal );
		}
		else
		{
			Noto_Criminal();
		}
		break;
	case M_Disconnect:
		// Push a player char off line. CLIENTLINGER thing
		if ( IsClient())
		{
			return GetClient()->addKick( pSrc, false );
		}
		SetDisconnected();
		break;
	case M_Dismount:
		vValRet.SetRef( Horse_UnMount());
		break;
	case M_DrawMap:
		// Use the cartography skill to draw a map.
		// Already did the skill check.
		m_Act.m_atCartography.m_Dist = vArgs.GetInt();
		Skill_Start( SKILL_CARTOGRAPHY );
		break;
	case M_Drop:	// uid
		return ItemDrop( g_World.ItemFind( vArgs.GetUID()), GetTopPoint());
	case M_Equip:	// uid
		return ItemEquip( g_World.ItemFind( vArgs.GetUID()));
	case M_EquipHalo:
		{
			// equip a halo light
			CItemPtr pItem = CItem::CreateScript(ITEMID_LIGHT_SRC,this);
			ASSERT( pItem);
			vValRet.SetRef(pItem);
			if ( !vArgs.IsEmpty())	// how long to last ?
			{

			}
			LayerAdd( pItem, LAYER_LIGHT );
		}
		break;
#if 0
	case M_EquipLight:
		{
			// equip a light from my pack.
			// find a light in my pack.

		}
		break;
#endif
	case M_EquipArmor:
		return ItemEquipArmor(false);
	case M_EquipWeapon:
		// find my best waepon for my skill and equip it.
		return ItemEquipWeapon(false);
	case M_Face:
		UpdateDir( pCharSrc );
		break;
	case M_FixWeight:
		FixWeight();
		break;
	case M_GoChar:	// uid
		return TeleportToObj( 1, vArgs );
	case M_GoCharId:
		return TeleportToObj( 3, vArgs );
	case M_GoCli:	// enum clients
		return TeleportToCli( 1, vArgs.GetInt());
	case M_GoItemId:
		return TeleportToObj( 4, vArgs );
	case M_GoName:
		return TeleportToObj( 0, vArgs );
	case M_Go:
	case M_GoPlace:
		if ( ! Spell_Effect_Teleport( g_Cfg.GetRegionPoint( vArgs.GetStr()), true, false ))
			return( HRES_BAD_ARGUMENTS );
		break;

	case M_GoSock:	// sockid
		return TeleportToCli( 0, vArgs.GetInt());
	case M_GoType:
		return TeleportToObj( 2, vArgs );
	case M_GoUID:	// uid
		if ( vArgs.IsEmpty())
			return( HRES_BAD_ARG_QTY );
		{
			CSphereUID uid( vArgs.GetInt());
			CObjBasePtr pObj = g_World.ObjFind(uid);
			if ( pObj == NULL )
				return( HRES_INVALID_HANDLE );
			Spell_Effect_Teleport( pObj->GetTopLevelObj()->GetTopPoint(), true, false );
		}
		break;
	case M_Hear:
		// NPC will hear this command but no-one else.
		if ( m_pPlayer)
		{
			WriteString( vArgs );
		}
		else
		{
			ASSERT( m_pNPC );
			NPC_OnHear( vArgs, pCharSrc );
		}
		break;
	case M_Hungry:	// How hungry are we ?
		if ( pCharSrc )
		{
			CGString sMsg;
			sMsg.Format( "%s %s %s",
				( pCharSrc == this ) ? "You" : (LPCTSTR) GetName(),
				( pCharSrc == this ) ? "are" : "looks",
				Food_GetLevelMessage( false, false ));
			pCharSrc->ObjMessage( sMsg, this );
		}
		break;
	case M_Invis:
	case M_Invisible:
		if ( pSrc )
		{
			m_StatFlag = vArgs.GetDWORDMask( m_StatFlag, STATF_Insubstantial );
			pSrc->Printf( "Invis is now %s.", ( IsStatFlag( STATF_Insubstantial )) ? "on" : "off" );
			UpdateMode( NULL, true );	// invis used by GM bug requires this
		}
		break;
	case M_Invul:
	case M_Invulnerable:
		if ( pSrc )
		{
			m_StatFlag = vArgs.GetDWORDMask( m_StatFlag, STATF_INVUL );
			pSrc->Printf( "Invulnerability %s", ( IsStatFlag( STATF_INVUL )) ? "ON" : "OFF" );
		}
		break;
	case M_Kill:
		{
			Effect( EFFECT_LIGHTNING, ITEMID_NOTHING, pCharSrc );
			OnTakeDamage( SHRT_MAX, pCharSrc, DAMAGE_GOD );
			Stat_Set( STAT_Health, 0 );
			g_Log.Event( LOG_GROUP_KILLS|LOG_GROUP_GM_CMDS, LOGL_EVENT, "'%s' was KILLed by '%s'" LOG_CR, (LPCTSTR) GetName(), (LPCTSTR) pSrc->GetName());
		}
		break;

	case M_MakeItem:
		if ( ! Skill_MakeItem(
			(ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, vArgs ),
			m_Act.m_Targ, CSkillDef::T_Start ))
			return HRES_BAD_ARGUMENTS;
		break;

	case M_NewbieSkill:
		// Reload the newbie stuff.
		{
			CResourceLock s( g_Cfg.ResourceGetDefByName( RES_Newbie, vArgs ));
			if ( ! s.IsFileOpen())
				return( HRES_BAD_ARGUMENTS );
			ReadScript( s );
		}
		break;

	case M_NewDupe:	// uid
		if ( vArgs.IsEmpty())
			return HRES_BAD_ARG_QTY;
		{
			CSphereUID uid( vArgs.GetUID());
			CObjBasePtr pObj = g_World.ObjFind(uid);
			if ( pObj == NULL )
				return HRES_INVALID_HANDLE;
			vValRet.SetRef(pObj);
			m_Act.m_Targ = pObj->GetUID();	// for last target stuff. (trigger stuff)
			return pObj->s_Method( "DUPE", vArgs, vValRet, pSrc );
		}
		break;

	case M_NewItem:	// just add an item but don't put it anyplace yet..
		{
			CItemPtr pItem = CItem::CreateHeader( vArgs, NULL, pCharSrc, false );
			if ( pItem == NULL )
				return( HRES_BAD_ARGUMENTS );
			// CCharDef::T_itemCreate
			vValRet.SetRef(pItem);
			m_Act.m_Targ = pItem->GetUID();	// for last target stuff. (trigger stuff)
		}
		break;

	case M_Dupe:	// = dupe a creature !
		{
			CCharPtr pChar = CChar::CreateNPC( GetID());
			ASSERT(pChar);
			vValRet.SetRef(pChar);

			// What about all the stuff it's wearing ???s
			pChar->MoveNearObj( pCharSrc ? (CChar*)pCharSrc : this, 1 );
		}
		break;
	case M_NewNPC:
		{
			CREID_TYPE id = (CREID_TYPE) g_Cfg.ResourceGetIndexType( RES_CharDef, vArgs.GetStr());
			if ( id == CREID_INVALID )
				return HRES_BAD_ARGUMENTS;
			CCharPtr pChar = CChar::CreateNPC( id );
			ASSERT(pChar);
			pChar->MoveNearObj( this, 1 );
			pChar->Update();
			vValRet.SetRef(pChar);
			m_Act.m_Targ = pChar->GetUID();	// for last target stuff. (trigger stuff)
		}
		break;

	case M_Bank:
	case M_OpenBank:
		// Open the bank box for this person
		if ( pCharSrc == NULL || ! pCharSrc->IsClient())
			return( HRES_INVALID_HANDLE );
		pCharSrc->GetClient()->addBankOpen( this, (LAYER_TYPE)((vArgs.IsEmpty()) ? LAYER_BANKBOX : vArgs.GetInt() ));
		break;
	case M_OpenPack:
		if ( pCharSrc == NULL || ! pCharSrc->IsClient())
			return( HRES_INVALID_HANDLE );
		pCharSrc->m_pClient->addBankOpen( this, LAYER_PACK ); // Send Backpack (with items)
		break;

	case M_Poison:
		{
			int iSkill = vArgs.GetInt();
			Spell_Effect_Poison( iSkill, iSkill/50, GET_ATTACHED_CCHAR(pSrc));
		}
		break;

	case M_Jail:	// i am being jailed (iTime in Minutes)
		Jail( pSrc, vArgs.IsEmpty() ? (4*365*24*60) : vArgs.GetInt());
		break;
	case M_Pardon:
	case M_Forgive:
		Jail( pSrc, 0 );
		break;

	case M_Poly:	// result of poly spell script choice. (casting a spell)

		m_Act.m_atMagery.m_Spell = SPELL_Polymorph;
		m_Act.m_atMagery.m_SummonID = (CREID_TYPE) g_Cfg.ResourceGetIndexType( RES_CharDef, vArgs.GetStr());

		if ( m_pClient != NULL )
		{
			m_Act.m_Targ = m_pClient->m_Targ.m_UID;
			m_Act.m_TargPrv = m_pClient->m_Targ.m_PrvUID;
		}
		else
		{
			m_Act.m_Targ = GetUID();
			m_Act.m_TargPrv = GetUID();
		}
		Skill_Start( SKILL_MAGERY );
		break;

	case M_PrivSet:
		if ( ! SetPrivLevel( pSrc, vArgs.GetStr()))
			return HRES_BAD_ARGUMENTS;
		break;

	case M_Remove:	// remove this char from the world instantly.
		if ( m_pPlayer )
		{
			if ( vArgs.GetStr()[0] != '1' ||
				pSrc->GetPrivLevel() < PLEVEL_Admin )
			{
				pSrc->WriteString( "Can't remove players this way, Try 'kill','kick' or 'remove 1'" );
				return( HRES_PRIVILEGE_NOT_HELD );
			}
		}
		DeleteThis();
		break;
	case M_Resurrect:
		if ( ! OnSpellEffect( SPELL_Resurrection, pCharSrc, 1000, NULL ))
			return HRES_INVALID_HANDLE;
		break;
	case M_Salute:	//	salute to player
		UpdateDir( pCharSrc );
		UpdateAnimate( ANIM_SALUTE, false );
		break;
	case M_Skill:
		Skill_Start( g_Cfg.FindSkillKey( vArgs.GetStr(), true));
		break;
	case M_Sleep:
		vValRet.SetRef( SleepStart( vArgs.GetInt()));
		break;
	case M_Suicide:
		Memory_ClearTypes( MEMORY_FIGHT ); // Clear the list of people who get credit for your death
		UpdateAnimate( ANIM_SALUTE );
		m_StatHealth = 0;
		break;

	case M_SummonCage: // i just got summoned. freeze me.
		if ( pCharSrc == NULL )
			return HRES_INVALID_HANDLE;
		StatFlag_Set(STATF_Immobile);
		m_TagDefs.SetKeyVar( "SUMMON_RETURNPOINT", GetTopPoint().v_Get());
		Spell_Effect_Teleport( pCharSrc->GetTopPoint(), true, false );
		break;
	case M_SummonDismiss: // i just got summoned. unfreeze me.
		{
		StatFlag_Clear(STATF_Immobile);
		if ( ! m_TagDefs.FindKeyVar( "SUMMON_RETURNPOINT", vValRet ))
			return HRES_INVALID_HANDLE;
		CPointMap pt;
		pt.v_Set(vValRet); 
		Spell_Effect_Teleport( pt, true, false );
		}
		break;
	case M_SummonTo:	// i just got summoned
		if ( pCharSrc == NULL )
			return HRES_INVALID_HANDLE;
		m_TagDefs.SetKeyVar( "SUMMON_RETURNPOINT", GetTopPoint().v_Get());
		Spell_Effect_Teleport( pCharSrc->GetTopPoint(), true, false );
		break;

	case M_Underwear:
		if ( ! IsHuman())
			return( HRES_INVALID_HANDLE );
		SetHue( GetHue() ^ HUE_UNDERWEAR );
		RemoveFromView();
		Update();
		break;
	case M_Unequip:	// uid
		return ItemBounce( g_World.ItemFind( vArgs.GetUID()));
	case M_Where:
		if ( pCharSrc )
		{
			// pCharSrc->Skill_UseQuick( SKILL_CARTOGRAPHY, 10 );

			CGString sMsg;
			if ( m_pArea )
			{
				if ( m_pArea->IsMultiRegion())
				{
					// house region.
				basicform:
					sMsg.Format( "I am in %s (%s)",
						(LPCTSTR) m_pArea->GetName(), (LPCTSTR) GetTopPoint().v_Get());
				}
				else
				{
					CRegionPtr pRoom = GetTopRegion( REGION_TYPE_ROOM );
					if ( ! pRoom )
						goto basicform;

					sMsg.Format( "I am in %s in %s (%s)",
						(LPCTSTR) m_pArea->GetName(), (LPCTSTR) pRoom->GetName(), (LPCTSTR) GetTopPoint().v_Get());
				}
			}
			else
			{
				// This should not happen.
				sMsg.Format( "I am at %s.", (LPCTSTR) GetTopPoint().v_Get());
			}
			pCharSrc->ObjMessage( sMsg, this );
		}
		break;

	case M_ScriptLoad:
		// Load me a script book and run it.
		return ScriptBook_Command( vArgs, true );

	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

void CChar::s_Serialize( CGFile& a )
{
	// Read and write binary.
	CObjBase::s_Serialize(a);

	if (m_pPlayer)
	{
		m_pPlayer->s_SerializePlayer(this, a);
	}
	if (m_pNPC)
	{
		m_pNPC->s_SerializeNPC(this, a);
	}

	// ...

	s_SerializeContent(a);
}

void CChar::s_WriteProps( CScript& s )
{
	s.WriteSection( "WORLDCHAR %s", (LPCTSTR) GetResourceName());

	CObjBase::s_WriteProps( s );

	if ( m_pPlayer)
	{
		s.WriteKey( "ACCOUNT", GetAccount()->GetName());
		m_pPlayer->s_WritePlayer(this, s);
	}
	if ( m_pNPC)
	{
		if ( m_SpeechHue != HUE_TEXT_DEF )
		{
			s.WriteKeyDWORD( "SPEECHCOLOR", m_SpeechHue );
		}
		m_pNPC->s_WriteNPC(this, s);
	}

	if ( GetTopPoint().IsValidPoint())
	{
		s.WriteKey( "P", GetTopPoint().v_Get());
	}
	if ( m_fonttype != FONT_NORMAL )
	{
		s.WriteKeyInt( "FONT", m_fonttype );
	}
	if ( m_dirFace != DIR_SE )
	{
		s.WriteKeyInt( "DIR", m_dirFace );
	}
	if ( m_prev_id != GetID())
	{
		s.WriteKey( "OBODY", g_Cfg.ResourceGetName( CSphereUID( RES_CharDef, m_prev_id )));
	}
	if ( m_prev_Hue != HUE_DEFAULT )
	{
		s.WriteKeyDWORD( "OSKIN", m_prev_Hue );
	}
	if ( m_StatFlag )
	{
		s.WriteKeyDWORD( "FLAGS", m_StatFlag );
	}
	if ( Skill_GetActive() != SKILL_NONE )
	{
		if ( CChar::IsSkillNPC( Skill_GetActive()))
			s.WriteKeyInt( "ACTION", Skill_GetActive());
		else
			s.WriteKey( "ACTION", g_Cfg.ResourceGetName( CSphereUID( RES_Skill, Skill_GetActive())));

		if ( m_Act.m_atUnk.m_Arg1 )
		{
			s.WriteKeyDWORD( "ACTARG1", m_Act.m_atUnk.m_Arg1 );
		}
		if ( m_Act.m_atUnk.m_Arg2 )
		{
			s.WriteKeyDWORD( "ACTARG2", m_Act.m_atUnk.m_Arg2 );
		}
		if ( m_Act.m_atUnk.m_Arg3 )
		{
			s.WriteKeyDWORD( "ACTARG3", m_Act.m_atUnk.m_Arg3 );
		}
	}

	if ( m_ptHome.IsValidPoint())
	{
		s.WriteKey( "HOME", m_ptHome.v_Get());
	}

	int j=0;
	for ( ;j<STAT_QTY;j++)
	{
		if ( ! m_Stat[j] )
			continue;
		s.WriteKeyInt( g_Stat_Name[j], m_Stat[j] );
	}
	for ( j=SKILL_First;j<SKILL_QTY;j++)
	{
		if ( ! m_Skill[j] )
			continue;
		s.WriteKeyInt( g_Cfg.GetSkillDef( (SKILL_TYPE) j )->GetSkillKey(), Skill_GetBase( (SKILL_TYPE) j ));
	}

	s_WriteContent(s);
}

void CChar::s_WriteParity( CScript& s )
{
	// overload virtual for world save.

	// if ( GetPrivLevel() <= PLEVEL_Guest ) return;

	if ( g_World.m_fSaveParity == IsStatFlag(STATF_SaveParity))
	{
		return; // already saved.
	}

	StatFlag_Mod( STATF_SaveParity, g_World.m_fSaveParity );

	int iRet = IsWeird();
	if ( iRet )
	{
		DEBUG_MSG(( "Weird char %x,'%s' rejected from save %d." LOG_CR, GetUID(), (LPCTSTR) GetName(), iRet ));
		return;
	}

	s_WriteSafe(s);
}

bool CChar::s_LoadProps( CScript& s ) // Load a character from script
{
	// Read all properties directly through CChar's virtual s_PropSet.
	while (s.ReadKeyParse())
	{
		CGVariant vArg;
		vArg = s.GetArgRaw();
		s_PropSet(s.GetKey(), vArg);
	}

	// Init the STATF_SaveParity flag.
	// StatFlag_Mod( STATF_SaveParity, g_World.m_fSaveParity );

	// Make sure everything is ok.
	if (( m_pPlayer && ! IsClient()) ||
		( m_pNPC && IsStatFlag( STATF_DEAD | STATF_Ridden )))	// dead npc
	{
		DEBUG_CHECK( ! IsClient());
		SetDisconnected();
	}

	int iResultCode = CObjBase::IsWeird();
	if ( iResultCode )
	{
		DEBUG_ERR(( "Char 0%x Invalid, id='%s', code=0%x" LOG_CR, GetUID(), (LPCTSTR) GetResourceName(), iResultCode ));
		DeleteThis();
	}

	return( true );
}


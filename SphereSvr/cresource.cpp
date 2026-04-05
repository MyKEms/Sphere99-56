//
// CSphereResourceMgr.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.
#include "spherelog.h"
#ifndef _WIN32
#include <dirent.h>
#include <signal.h>
#include <setjmp.h>
#include <execinfo.h>
#endif

// Signal handler: if siglongjmp recovery is enabled, use it.
// Otherwise print backtrace and crash.
#ifndef _WIN32
#include <setjmp.h>
extern volatile sig_atomic_t g_fSEGV_catch;
extern sigjmp_buf g_SEGV_jmpbuf;

static void CrashHandler(int sig)
{
	if ( g_fSEGV_catch )
	{
		// Recovery enabled — jump back to safe point.
		g_fSEGV_catch = 0;
		signal(sig, CrashHandler); // re-install (SA_RESETHAND cleared it)
		siglongjmp(g_SEGV_jmpbuf, 1);
		return; // not reached
	}
	// No recovery — print backtrace and crash.
	fprintf(stderr, "\n=== CRASH: signal %d ===\n", sig);
	void* bt[30];
	int n = backtrace(bt, 30);
	backtrace_symbols_fd(bt, n, 2);
	fflush(stderr);
	signal(sig, SIG_DFL);
	raise(sig);
}
#endif

const CAssocReg CSphereResourceMgr::sm_PropsAssoc[CSphereResourceMgr::P_QTY+1] =
{
#define CRESOURCEPROP(a,b,c,d,e) { #a, {c, offsetof(CSphereResourceMgr,d) }},
#include "cresourceprops.tbl"
#undef CRESOURCEPROP
	NULL,
};

const CScriptProp CSphereResourceMgr::sm_Props[CSphereResourceMgr::P_QTY+1] =
{
#define CRESOURCEPROP(a,b,c,d,e) CSCRIPT_PROP_IMP(a,b,e)
#include "cresourceprops.tbl"
#undef CRESOURCEPROP
	NULL,
};

CSCRIPT_CLASS_IMP1(SphereResourceMgr,CSphereResourceMgr::sm_Props,NULL,NULL,ResourceMgr);

CSphereResourceMgr::CSphereResourceMgr()
{
	// RES_Sphere

	// m_Servers.IncRefCount(); // Static item (not applicable)
	// m_scpIni.IncRefCount();	// Static item (not applicable)

	m_scpIni.SetFilePath( SPHERE_FILE ".ini" ); // Open script file

	m_RegisterServer.SetHostPortStr( SPHERE_MAIN_SERVER );
	m_RegisterServer.SetPort( SPHERE_DEF_PORT );
	m_RegisterServer2.SetPort( SPHERE_DEF_PORT );

	m_fAllowKeyLockdown = true;
	m_iPollServers = 15*60*TICKS_PER_SEC;
	m_iMapCacheTime = 2*60*TICKS_PER_SEC;
	m_iSectorSleepMask = _1BITMASK(10)-1;
	m_iServerValidListTime = 8*60*60*TICKS_PER_SEC;

	m_wDebugFlags = 0; // DEBUGF_NPC_EMOTE
	m_fSecure = true;
	m_iFreezeRestartTime = 10;

	// Magic
	m_iPreCastTime = 0;
	m_fReagentsRequired = true;
	m_fReagentLossFail = true;
	m_fWordsOfPowerPlayer = true;
	m_fWordsOfPowerStaff = false;
	m_fEquippedCast = true;
	m_iMagicUnlockDoor = 1000;

	m_iSpell_Teleport_Effect_Staff = ITEMID_FX_FLAMESTRIKE;	// drama
	m_iSpell_Teleport_Sound_Staff = 0x1f3;
	// m_iSpell_Teleport_Effect_Player = ;
	// m_iSpell_Teleport_Sound_Player;

	// Decay
	m_iDecay_Item = 30*60*TICKS_PER_SEC;
	m_iDecay_CorpsePlayer = 45*60*TICKS_PER_SEC;
	m_iDecay_CorpseNPC = 15*60*TICKS_PER_SEC;

	// Accounts
	m_iMaxCharsPerAccount = UO_MAX_CHARS_PER_ACCT;
	m_fRequireEmail = false;
	m_iGuestsMax = 0;
	m_fArriveDepartMsg = true;
	m_iMinCharDeleteTime = 3*24*60*60*TICKS_PER_SEC;

	m_iClientsPerIPMax = 4;
	m_iClientsMax = FD_SETSIZE-1;
	m_iClientLingerTime = 60* TICKS_PER_SEC;
	m_iDeadSocketTime = 5*60*TICKS_PER_SEC;
	m_fLocalIPAdmin = false;
	m_ClientVerMin.SetCryptVerEnum(0);	// allow any.

	// Save
	m_iSaveBackupLevels = 3;
	m_iSaveBackgroundTime = 5* 60* TICKS_PER_SEC;	// Use the new background save.
	m_fSaveGarbageCollect = false;	// Always force a full garbage collection.
	m_iSavePeriod = 15*60*TICKS_PER_SEC;

	// In game effects.
	m_fMonsterFight = true;
	m_fMonsterFear = true;
	m_iLightDungeon = 17;
	m_iLightNight = 17;	// dark before t2a.
	m_iLightDay = LIGHT_BRIGHT;
	m_iBankIMax = 1000;
	m_iBankWMax = 400* WEIGHT_UNITS;
	m_fGuardsInstantKill = true;
	m_iSnoopCriminal = 500;
	m_iTrainSkillPercent = 50;
	m_iTrainSkillMax = 500;
	m_fCharTags = true;
	m_iVendorMaxSell = 30;
	m_iGameMinuteLength = 8* TICKS_PER_SEC;
	m_fNoWeather = false;
	m_fFlipDroppedItems = true;
	m_iMurderMinCount = 5;
	m_iMurderDecayTime = 8*60*60* TICKS_PER_SEC;
	m_iMaxCharComplexity = 16;
	m_iMaxItemComplexity = 25;
	m_iPlayerKarmaNeutral = -2000; // How much bad karma makes a player neutral?
	m_iPlayerKarmaEvil = -8000;
	m_iGuardLingerTime = 1*60*TICKS_PER_SEC; // "GUARDLINGER",
	m_iCriminalTimer = 3*60*TICKS_PER_SEC;
	m_iHitpointPercentOnRez = 10;
	m_fLootingIsACrime = true;
	m_fHelpingCriminalsIsACrime = true;
	m_fPlayerGhostSounds = true;
	m_fAutoNewbieKeys = true;
	m_iMaxBaseSkill = 250;
	m_iStamRunningPenalty = 50;
	m_iStaminaLossAtWeight = 100;
	m_iMountHeight = PLAYER_HEIGHT + 5;
	m_iArcheryMinDist = 2;
	m_fCharTitles = true;
}

CSphereResourceMgr::~CSphereResourceMgr()
{
	Unload(false);
	// m_Servers.StaticDestruct(); // Static item (not applicable)
	// m_scpIni.StaticDestruct();	// static item (not applicable)
}

HRESULT CSphereResourceMgr::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	RES_TYPE iResType = (RES_TYPE) s_FindKeyInTable( pszKey, sm_szResourceBlocks );
	if ( iResType <= 0 )	// RES_UNKNOWN
	{
		return( HRES_UNKNOWN_PROPERTY );
	}

	// Now get the index.
	if ( iResType == RES_Servers )
	{
		vValRet.SetRef( g_Cfg.Server_GetDef( vArgs.GetInt()));
	}
	else
	{
		vValRet.SetRef( ResourceGetDef( ResourceGetIDByName( iResType, vArgs )));
	}

	return( NO_ERROR );
}

HRESULT CSphereResourceMgr::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	s_FixExtendedProp( pszKey, "Advance", vVal );

	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return(HRES_UNKNOWN_PROPERTY);
	}

	switch (iProp)
	{
	case P_Advance:
		// Stat advance rates.
		if ( vVal.MakeArraySize() < 2 )
			return HRES_BAD_ARG_QTY;
		{
			STAT_TYPE i = FindStatKey( vVal.GetArrayPSTR(0), true );
			if ( i < 0 || i >= STAT_BASE_QTY )
				return HRES_INVALID_INDEX;
			vVal.RemoveArrayElement(0);
			m_StatAdv[i].v_Set( vVal );
		}
		break;
	case P_Regen:
		if ( vVal.MakeArraySize() < 2 )
			return HRES_BAD_ARG_QTY;
		{
			STAT_TYPE iStat = FindStatKey( vVal.GetArrayPSTR(0), true );
			if ( iStat < 0 )
				return( HRES_INVALID_INDEX );
			g_Cfg.m_DefaultRaceClass->SetRegenRate( iStat, vVal.GetArrayInt(1));
		}
		break;

	case P_MulFile:
		if ( vVal.MakeArraySize() < 2 )
			return HRES_BAD_ARG_QTY;
		return g_MulInstall.SetMulFile((VERFILE_TYPE)( atoi(pszKey+7)), vVal.GetPSTR());

#if 0
	case P_AcctCryptKey:
		{
			int iArgQty = vVal.MakeArraySize();
			g_Accounts.m_Crypt.SetCryptMasterVer( vVal.GetArrayInt(0), vVal.GetArrayInt(1), vVal.GetArrayInt(2));
		}
		break;
	case P_EmailGateway:
		m_EMailGateway.SetHostPortStr( vVal.GetPSTR());
		break;
#endif

	case P_LangDef:
		m_LangDef.Set( vVal.GetPSTR());
		break;

	case P_MasterKey:
		{
			int iArgQty = vVal.MakeArraySize();
			if ( iArgQty < 1 )
				return HRES_BAD_ARG_QTY;
			CCryptBase::SetDefaultMasterVer( vVal.GetArrayInt(0), vVal.GetArrayInt(1), vVal.GetArrayInt(2));
		}
		break;
	case P_AcctFiles:	// Put acct files here.
		m_sAcctBaseDir = CGFile::GetMergedFileName( vVal.GetPSTR(), "" );
		break;
	case P_BankMaxWeight:
		m_iBankWMax = vVal.GetInt()* WEIGHT_UNITS;
		break;
	case P_ClientLinger:
		m_iClientLingerTime = vVal.GetInt()* TICKS_PER_SEC;
		break;
	case P_ClientMax:
	case P_Clients:
		m_iClientsMax = vVal.GetInt();
		if ( m_iClientsMax > FD_SETSIZE-1 )	// Max number we can deal with. compile time thing.
		{
			m_iClientsMax = FD_SETSIZE-1;
		}
		break;
	case P_CorpseNPCDecay:
		m_iDecay_CorpseNPC = vVal.GetInt()*60*TICKS_PER_SEC;
		break;
	case P_CorpsePlayerDecay:
		m_iDecay_CorpsePlayer = vVal.GetInt()*60*TICKS_PER_SEC ;
		break;
	case P_CriminalTimer:
		m_iCriminalTimer = vVal.GetInt()* 60* TICKS_PER_SEC;
		break;
	case P_DeadSocketTime:
		m_iDeadSocketTime = vVal.GetInt()*60*TICKS_PER_SEC;
		break;
	case P_DecayTimer:
		m_iDecay_Item = vVal.GetInt() *60*TICKS_PER_SEC;
		break;
	case P_GuardLinger:
		m_iGuardLingerTime = vVal.GetInt()* 60* TICKS_PER_SEC;
		break;
	case P_ClientVerMin:
		m_ClientVerMin.SetCryptVer( vVal.GetPSTR());
		break;
	case P_HearAll:
		g_Log.SetLogGroupMask( vVal.GetDWORDMask( g_Log.GetLogGroupMask(), LOG_GROUP_PLAYER_SPEAK ));
		break;
	case P_DebugLevel:
		{
			extern int g_iDebugLevel;
			g_iDebugLevel = vVal.GetInt();
			SPHERE_LOG_ERR("DebugLevel set to %d", g_iDebugLevel);
		}
		break;
	case P_Log:
		g_Log.OpenLog( vVal.GetPSTR());
		break;
	case P_LogMask:
		g_Log.SetLogGroupMask( vVal.GetInt());
		break;
	case P_MulFiles:
	case P_Files: // Get data files from here.
		{
			CGString sMul = CGFile::GetMergedFileName( vVal.GetPSTR(), "" );
			g_MulInstall.SetPreferPath( sMul );
		}
		break;
	case P_MapCacheTime:
		m_iMapCacheTime = vVal.GetInt()* TICKS_PER_SEC;
		break;
	case P_ServerValidListTime:
		m_iServerValidListTime = vVal.GetInt()* TICKS_PER_SEC;
		break;
	case P_MaxCharsPerAccount:
		m_iMaxCharsPerAccount = vVal.GetInt();
		if ( m_iMaxCharsPerAccount > UO_MAX_CHARS_PER_ACCT )
			m_iMaxCharsPerAccount = UO_MAX_CHARS_PER_ACCT;
		break;
	case P_MainLogServer:
		m_sMainLogServerDir = vVal.GetPSTR();
		if ( m_sMainLogServerDir[0] == '0' )
			m_sMainLogServerDir.Empty();
		break;
	case P_MinCharDeleteTime:
		m_iMinCharDeleteTime = vVal.GetInt()*60*TICKS_PER_SEC;
		break;
	case P_MurderDecayTime:
		m_iMurderDecayTime = vVal.GetInt()* TICKS_PER_SEC;
		break;
	case P_Profile:
		g_Serv.m_Profile.SetSampleWindowLen( vVal.GetInt());
		break;
	case P_PollServers:
		m_iPollServers = vVal.GetInt() *60*TICKS_PER_SEC;
		g_BackTask.CreateThread();
		break;
	case P_PlayerNeutral:	// How much bad karma makes a player neutral?
		m_iPlayerKarmaNeutral = vVal.GetInt();
		if ( m_iPlayerKarmaEvil > m_iPlayerKarmaNeutral )
			m_iPlayerKarmaEvil = m_iPlayerKarmaNeutral;
		break;
	case P_RegisterFlag:
	case P_RegisterServer:
		if ( ! strcmp( vVal.GetPSTR(), "0" ))
		{
			m_RegisterServer.EmptyHost();
		}
		else if ( ! strcmp( vVal.GetPSTR(), "1" ))
		{
			m_RegisterServer.SetHostStr( SPHERE_MAIN_SERVER );
		}
		else
		{
			m_RegisterServer.SetHostStr( vVal.GetPSTR());
		}
		g_BackTask.CreateThread();
		break;
	case P_RegisterServer2:
		m_RegisterServer2.SetHostStr( vVal.GetPSTR());
		g_BackTask.CreateThread();
		break;
	case P_Resources:
		AddResourceFile( vVal.GetPSTR());
		break;
	case P_ScpFiles: // Get SCP files from here.
		m_sSCPBaseDir = CGFile::GetMergedFileName( vVal.GetPSTR(), "" );
		break;
	case P_Secure:
		m_fSecure = vVal.GetBool();
		g_Serv.SetSignals();
		break;
	case P_SavePeriod:
		m_iSavePeriod = vVal.GetInt()*60*TICKS_PER_SEC;
		break;
	case P_SectorSleep:
		m_iSectorSleepMask = _1BITMASK(vVal.GetInt()) - 1;
		break;
	case P_SaveBackground:
		m_iSaveBackgroundTime = vVal.GetInt() * 60 * TICKS_PER_SEC;
		break;
	case P_Verbose:
		g_Log.SetLogLevel( vVal.GetBool() ? LOGL_TRACE : LOGL_EVENT );
		break;

	case P_WorldSave: // Put save files here.
		m_sWorldBaseDir = CGFile::GetMergedFileName( vVal.GetPSTR(), "" );
		break;

	case P_WorldStatics:
		SetWorldStatics(vVal.GetPSTR());
		break;

	case P_InstCDPath:
		g_MulInstall.SetInstCDPath( vVal.GetPSTR());
		break;
	case P_ExePath:
		g_MulInstall.SetExePath( vVal.GetPSTR());
		break;

	default:
		if ( ! sm_PropsAssoc[iProp].m_elem.SetVal( this, vVal ))
			return HRES_BAD_ARGUMENTS;
		break;
	}

	return( NO_ERROR );
}

HRESULT CSphereResourceMgr::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	// Just do stats values for now.

	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp<0 )
	{
		return( HRES_UNKNOWN_PROPERTY );
	}

	switch (iProp)
	{
	case P_Advance:
		return HRES_INVALID_FUNCTION;
#if 0
		// Stat advance rates.
		{
			STAT_TYPE i = FindStatKey( pszKey+8, true );
			if ( i < 0 || i >= STAT_BASE_QTY )
				return HRES_INVALID_INDEX;
			m_StatAdv[i].v_Get( vValRet );
		}
		break;
#endif
	case P_Regen:
		return HRES_INVALID_FUNCTION;
	case P_ScpFiles:
		vValRet = m_sSCPBaseDir;
		break;
	case P_LangDef:
		vValRet = m_LangDef.GetStr();
		break;
	case P_BankMaxWeight:
		vValRet.SetInt( m_iBankWMax / WEIGHT_UNITS );
		break;
	case P_ClientLinger:
		vValRet.SetInt( m_iClientLingerTime / TICKS_PER_SEC );
		break;
	case P_CorpseNPCDecay:
		vValRet.SetInt( m_iDecay_CorpseNPC / (60*TICKS_PER_SEC));
		break;
	case P_CorpsePlayerDecay:
		vValRet.SetInt( m_iDecay_CorpsePlayer / (60*TICKS_PER_SEC));
		break;
	case P_CriminalTimer:
		vValRet.SetInt( m_iCriminalTimer / (60*TICKS_PER_SEC));
		break;
	case P_DeadSocketTime:
		vValRet.SetInt( m_iDeadSocketTime / (60*TICKS_PER_SEC));
		break;
	case P_DecayTimer:
		vValRet.SetInt( m_iDecay_Item / (60*TICKS_PER_SEC));
		break;
	case P_GuardLinger:
		vValRet.SetInt( m_iGuardLingerTime / (60*TICKS_PER_SEC));
		break;
	case P_HearAll:
		vValRet.SetBool( g_Log.IsLoggedGroupMask( LOG_GROUP_PLAYER_SPEAK ));
		break;
	case P_DebugLevel:
		{
			extern int g_iDebugLevel;
			vValRet.SetInt( g_iDebugLevel );
		}
		break;
	case P_Log:
		vValRet = g_Log.GetLogDir();
		break;
	case P_LogMask:
		vValRet.SetDWORD( g_Log.GetLogGroupMask());
		break;
	case P_MulFiles:
	case P_Files: // Get data files from here.
		vValRet = g_MulInstall.GetPreferPath( NULL );
		break;
	case P_MapCacheTime:
		vValRet.SetInt( m_iMapCacheTime / TICKS_PER_SEC );
		break;
	case P_ServerValidListTime:
		vValRet.SetInt( m_iServerValidListTime / TICKS_PER_SEC );
		break;
	case P_MinCharDeleteTime:
		vValRet.SetInt( m_iMinCharDeleteTime /( 60*TICKS_PER_SEC ));
		break;
	case P_MurderDecayTime:
		vValRet.SetInt( m_iMurderDecayTime / (TICKS_PER_SEC ));
		break;
	case P_Profile:
		vValRet.SetInt( g_Serv.m_Profile.GetSampleWindowLen());
		break;
	case P_PollServers:
		vValRet.SetInt( m_iPollServers / (60*TICKS_PER_SEC));
		break;
	case P_RClock:
#ifdef _WIN32
		vValRet.SetDWORD( GetTickCount());
#else
		vValRet.SetDWORD( (DWORD)time(NULL) );
#endif
		break;
	case P_RegisterFlag:
		vValRet.SetBool( m_RegisterServer.IsValidAddr());
		break;
	case P_RegisterServer:
		vValRet = m_RegisterServer.GetHostName();
		break;
	case P_RegisterServer2:
		vValRet = m_RegisterServer2.GetHostName();
		break;
	case P_RTime:	// the current real world time.
	case P_RTimeText: // the current real world time.
		{
			CGTime datetime;
			datetime.InitTimeCurrent();
			vValRet = datetime.Format(CTIME_FORMAT_DEFAULT);
		}
		break;
	case P_SavePeriod:
		vValRet.SetInt( m_iSavePeriod / (60*TICKS_PER_SEC));
		break;
	case P_SectorSleep:
		vValRet.SetInt( Calc_GetLog2( m_iSectorSleepMask+1 )-1 );
		break;
	case P_SaveBackground:
		vValRet.SetInt( m_iSaveBackgroundTime / (60* TICKS_PER_SEC));
		break;
	// case P_Guilds:
	case P_StatGuilds:
		vValRet.SetInt( g_World.m_GuildStones.GetSize());
		break;
	case P_StatTowns:
		vValRet.SetInt( g_World.m_TownStones.GetSize());
		break;
	case P_TimeUp:
		vValRet.SetInt( g_World.m_timeStartup.GetCacheAge() / TICKS_PER_SEC );
		break;
	case P_TotalPolledAccounts:
		vValRet.SetInt( g_BackTask.m_iTotalPolledAccounts );
		break;
	case P_TotalPolledClients:
		vValRet.SetInt( g_BackTask.m_iTotalPolledClients );
		break;
	case P_TotalPolledServers:
		vValRet.SetInt( m_Servers.GetSize() + 1 );
		break;
	case P_Verbose:
		vValRet.SetBool(( g_Log.GetLogLevel() >= LOGL_TRACE ) ? 1 : 0 );
		break;
	case P_Version:
		vValRet = g_szServerDescription;
		break;

	default:
		if ( ! sm_PropsAssoc[iProp].m_elem.GetVal( this, vValRet ))
			return HRES_BAD_ARGUMENTS;
		break;
	}

	return( NO_ERROR );
}

void CSphereResourceMgr::s_WriteProps( CScript& s )
{
	// CResourceMgr:s_WriteProps(s);

	CSphereResourceMgr defaults;	// compare against the defaults.

	// Now write all the stuff in it.
	for ( int i=0; i<CSphereResourceMgr::P_QTY; i++ )
	{
		if ( sm_PropsAssoc[i].m_elem.m_type != VARTYPE_VOID )
		{
			if ( ! sm_PropsAssoc[i].m_elem.CompareVal( this, &defaults ))
				continue;
		}
		else switch ( i )	// handle the special cases here.
		{
		case P_ScpFiles:
			break;
		case P_MulFiles:
			if ( ! g_MulInstall.GetPreferPath().IsEmpty())
				break;
			continue;
		case P_LogMask:
			if ( g_Log.GetLogGroupMask() != (LOG_GROUP_INIT | LOG_GROUP_CLIENTS | LOG_GROUP_GM_PAGE ))
				break;
			continue;
		case P_Log:
			if ( g_Log.GetLogDir()[0] != '\0' )
				break;
			continue;
		case P_Profile:
			if ( g_Serv.m_Profile.GetSampleWindowLen() != 10 )
				break;
			continue;
		case P_Verbose:
			if ( g_Log.GetLogLevel() != LOGL_EVENT )
				break;
			continue;
#if 0
		case P_MulFile:
			{
				return g_MulInstall.SetMulFile((VERFILE_TYPE)( atoi(s.GetKey()+7)), vVal.GetPSTR());
			}
		case P_Regen:
			{
				int index = atoi(s.GetKey()+5);
				return g_Cfg.m_DefaultRaceClass->SetRegenRate( (STAT_TYPE) index, vVal.GetInt()* TICKS_PER_SEC );
			}
#endif
		default:
			// Warn cant write this?
			continue;
		}

		CGVariant vVal;
		HRESULT hRes = s_PropGet( sm_Props[i].m_pszName, vVal, &g_Serv );
		if ( IS_ERROR(hRes))
			continue;

		s.WriteKey( sm_PropsAssoc[i].m_pszName, vVal );
	}
}

//*************************************************************

void CSphereResourceMgr::SetWorldStatics( LPCTSTR pszFileName )
{
	if ( CGFile::GetFileNameTitle( pszFileName ) == pszFileName )
	{
		m_sWorldStatics = CGFile::GetMergedFileName( m_sSCPBaseDir, pszFileName );
	}
	else
	{
		m_sWorldStatics = pszFileName;
	}
}

bool CSphereResourceMgr::IsConsoleCmd( TCHAR ch ) const
{
	// in evil client press . gets u the = prefix.
	return( ch == '/' || ch == '.' || ch == '=' );
}

bool CSphereResourceMgr::CanRunBackTask() const
{
	// Run the background task or not ?

	if ( g_Serv.IsLoading() || g_Serv.m_iExitFlag )
		return( false );
	if ( m_iPollServers && m_Servers.GetSize())
		return( true );
	if ( ! m_RegisterServer.IsEmptyHost())
		return( true );
	if ( ! m_RegisterServer2.IsEmptyHost())
		return( true );

	return( false );
}

SKILL_TYPE CSphereResourceMgr::FindSkillKey( LPCTSTR pszKey, bool fAllowDigit ) const
{
	// Find the skill name in the alpha sorted list.
	// RETURN: SKILL_NONE = error.

	if ( fAllowDigit && isdigit( pszKey[0] ))
	{
		SKILL_TYPE skill = (SKILL_TYPE) RES_GET_INDEX( Exp_GetComplex(pszKey));
		if ( ! CChar::IsSkillBase(skill) &&
			! CChar::IsSkillNPC(skill))
		{
			return( SKILL_NONE );
		}
		return( skill );
	}

	CSkillDefPtr pSkillDef = FindSkillDef( pszKey );
	if ( pSkillDef == NULL )
		return( SKILL_NONE );
	return( (SKILL_TYPE) RES_GET_INDEX( pSkillDef->GetUIDIndex()));
}

STAT_TYPE CSphereResourceMgr::FindStatKey( LPCTSTR pszKey, bool fAllowDigit ) // static
{
	// RETURN: -1 = no match
	if ( fAllowDigit && isdigit(pszKey[0]))
	{
		int i = atoi(pszKey);
		if ( i >= STAT_QTY )
			return( (STAT_TYPE) -1 );
		return( (STAT_TYPE) i );
	}
	if ( ! _stricmp( pszKey, "STAMINA" ))	// alternate name.
		return STAT_Stam;
	if ( ! _stricmp( pszKey, "HITPOINTS" ))	// alternate name.
		return STAT_Health;
	return (STAT_TYPE) FindTable( pszKey, g_Stat_Name );
}

int CSphereResourceMgr::GetSpellEffect( SPELL_TYPE spell, int iSkillVal ) const
{
	// NOTE: Any randomizing of the effect must be done by varying the skill level .
	// iSkillVal = 0-1000
	DEBUG_CHECK(spell);
	if ( spell<=0 )
		return( 0 );
	CSpellDefPtr pSpellDef = g_Cfg.GetSpellDef( spell );
	if ( pSpellDef == NULL )
		return( 0 );
	return( pSpellDef->m_Effect.GetLinear( iSkillVal ));
}

CServerPtr CSphereResourceMgr::Server_GetDef( int index )
{
	CThreadLockPtr lock( &m_Servers );
	if ( ! m_Servers.IsValidIndex(index))
		return( NULL );
	return( CServerPtr( m_Servers[index] ));
}

CWebPagePtr CSphereResourceMgr::FindWebPage( const char* pszPath ) const
{
	// "--WEBBOT-SELF--" == the Referer.

	if ( pszPath == NULL )
	{
	takedefault:
		if ( ! m_WebPages.GetSize())
			return( NULL );
		// Take this as the default page.
		return( m_WebPages[0] );
	}

	LPCTSTR pszTitle = CGFile::GetFileNameTitle(pszPath);

	if ( pszTitle == NULL || pszTitle[0] == '\0' )
	{
		// This is just the root index.
		goto takedefault;
	}

	for ( int i=0; i<m_WebPages.GetSize(); i++ )
	{
		CWebPagePtr pWeb = m_WebPages[i];
		ASSERT(pWeb);
		if ( pWeb->IsMatch(pszTitle))
			return( pWeb );
	}

	return( NULL );
}

bool CSphereResourceMgr::IsObscene( LPCTSTR pszText ) const
{
	// does this text contain obscene content?
	// NOTE: allow partial match syntax *fuck* or ass (alone)

	for ( int i=0; i<m_Obscene.GetSize(); i++ )
	{
		MATCH_TYPE ematch = Str_Match( m_Obscene[i], pszText );
		if ( ematch == MATCH_VALID )
			return( true );
	}
	return( false );
}

const CMulMultiType* CSphereResourceMgr::GetMultiItemDefs( ITEMID_TYPE itemid )
{
	if ( ! CItemDef::IsID_Multi(itemid))
		return( NULL );

	CMulMultiType* pMulti;
	HASH_INDEX hashindex = VERDATA_MAKE_HASH( VERFILE_MULTI, itemid - ITEMID_MULTI );
	int index = m_MultiDefs.FindKey( hashindex );
	if ( index < 0 )
	{
		pMulti = new CMulMultiType( hashindex );
		ASSERT(pMulti);
		index = m_MultiDefs.Add(pMulti);
		ASSERT(index>=0);
	}
	else
	{
		pMulti = m_MultiDefs[index];
		ASSERT(pMulti);
		pMulti->m_timeCache.InitTimeCurrent();
	}
	return( pMulti );
}

CCharDefPtr CSphereResourceMgr::FindCharDef( CREID_TYPE id )
{
	return( CCharDef::TranslateBase( CResourceMgr::ResourceGetDef( CSphereUID( RES_CharDef, id ))));
}

CItemDefPtr CSphereResourceMgr::FindItemDef( ITEMID_TYPE id )
{
	return( CItemDef::TranslateBase( CResourceMgr::ResourceGetDef( CSphereUID( RES_ItemDef, id ))));
}

PLEVEL_TYPE CSphereResourceMgr::GetPrivCommandLevel( LPCTSTR pszCmd ) const
{
	// What is this commands plevel ?
	// NOTE: This doe snot attempt to parse anything.

	char szTmp[EXPRESSION_MAX_KEY_LEN+1];
	int i;
	for ( i=0; isalnum(pszCmd[i]); i++ )
	{
		szTmp[i] = pszCmd[i];
	}
	szTmp[i] = '\0';

	// Is this command avail for your priv level (or lower) ?
	int ilevel = PLEVEL_QTY-1;
	for ( ; ilevel >= 0; ilevel-- )
	{
		if ( m_PrivCommands[ilevel].FindKey(szTmp) >= 0 )
			return( (PLEVEL_TYPE)ilevel );
	}

	// A GM will default to use all commands.
	// xcept those that are specifically named that i can't use.
	return( PLEVEL_GM );	// default level.
}

bool CSphereResourceMgr::CanUsePrivVerb( const CScriptObj* pObjTarg, LPCTSTR pszCmd, CScriptConsole* pSrc ) const
{
	// RES_PLevel
	// can i use this verb on this object ?
	// Check just at entry points where commands are entered or targetted.
	// NOTE:
	//  Call this each time we change pObjTarg such as s_GetRef()
	// RETURN:
	//  true = i am ok to use this command.

	if ( pSrc == NULL )
	{
		DEBUG_CHECK(pSrc);
		return( false );
	}
	ASSERT(pObjTarg);

	// Are they more privleged than me ?
	CCharPtr pCharSrc = GET_ATTACHED_CCHAR(pSrc);
	CCharPtr pChar = PTR_CAST(CChar,const_cast<CScriptObj*>(pObjTarg));
	if ( pChar )
	{
		if ( pCharSrc != pChar &&
			pSrc->GetPrivLevel() < pChar->GetPrivLevel())
		{
			pSrc->WriteString( "Target is more privileged than you" LOG_CR );
			return false;
		}
	}

	// can i use this verb on this object ?

	if ( pCharSrc == NULL )
	{
		// I'm not a cchar. what am i ?
		CClientPtr pClient = PTR_CAST(CClient,pSrc);
		if ( pClient )
		{
			// We are not logged in as a player char ? so we cannot do much !
			if ( pClient->GetAccount() == NULL )
			{
				// must be a console or web page ?
				// I guess we can just login !
				if ( ! _strnicmp( pszCmd, "LOGIN", 5 ))
					return( true );
				return( false );
			}
		}
		else
		{
			// we might be the g_Serv or web page ?
		}
	}

	// Is this command avail for your priv level (or lower) ?

	PLEVEL_TYPE ilevel = GetPrivCommandLevel( pszCmd );
	if ( ilevel > pSrc->GetPrivLevel())
		return( false );

	return( true );
}

//*************************************************************

CPointMap CSphereResourceMgr::GetRegionPoint( LPCTSTR pCmd ) const // Decode a teleport location number into X/Y/Z
{
	// get a point from a name. (probably the name of a region)
	// Might just be a point coord number ?
	// RES_Teleporters ?

	GETNONWHITESPACE( pCmd );
	if ( pCmd[0] == '-' )	// Get location from start list.
	{
		// RES_Starts
		int i = ( - atoi(pCmd)) - 1;
		if ( ! m_StartDefs.IsValidIndex( i ))
			i = 0;
		return( m_StartDefs[i]->m_pt );
	}

	CPointMap pt;	// invalid point
	if ( isdigit( pCmd[0] ))
	{
		{ CGVariant vTmp(pCmd); pt.v_Set( vTmp ); }
	}
	else
	{
		// Match the region name with global regions.

		for ( int _hi = 0; _hi < (int)m_ResHash.GetCount(); _hi++ )
		{
			CResourceDefPtr pResDef = m_ResHash.GetAt(_hi);
			if ( !pResDef )
				continue;

			// RES_Room
			// RES_Area

			CRegionPtr pRegion = REF_CAST(CRegionBasic,pResDef);
			if ( pRegion == NULL )
				continue;

			if ( ! pRegion->GetName().CompareNoCase( pCmd ) ||
				! _stricmp( pRegion->GetResourceName(), pCmd ))
			{
				return( pRegion->m_pt );
			}
		}
	}
	// no match.
	return( pt );
}

//*************************************************************

bool CSphereResourceMgr::LoadScriptSection( CScript& s )
{
	// Index or read any resource blocks we know how to handle.
	CSphereScriptContext FileContext(&s);	// set this as the context.
	CVarDefPtr pVarNum;
	CSphereUID rid;

	RES_TYPE restype = (RES_TYPE) FindTableSorted( s.GetSection(), sm_szResourceBlocks, RES_QTY );
	if ( restype <= 0 )
	{
		// support old versions that just had special files with [xxxx] entries
		// DEFS.SCP - has no section headers.
		// MENU.SCP changes headers for ITEMMENU GMMENU, GUILDMENU and ADMIN
		// SKILL.SCP needs new SKILLMENU header
		// HELP.SCP needs HELP header.
		// NEWB.SCP needs NEWBIE header
		// NAME.SCP needs NAMES header.
		//

		if ( isdigit( s.GetSection()[0] ))
		{
			CString sTitle = s.GetFileTitle();
			int ilen = strlen( SPHERE_FILE );
			if ( sTitle.GetLength() > ilen )
			{
				LPCTSTR pszTitle = ((LPCTSTR)sTitle) + ilen;
				if ( ! _strnicmp( pszTitle, "item", 4 ))
				{
					restype = RES_ItemDef;
					rid = CSphereUID( RES_ItemDef, Str_ahextou( s.GetSection()));
				}
				else if ( ! _strnicmp( pszTitle, "char", 4 ))
				{
					restype = RES_CharDef;
					rid = CSphereUID( RES_CharDef, Str_ahextou( s.GetSection()));
				}
				else if ( ! _strnicmp( pszTitle, "template", 8 ))
				{
					restype = RES_Template;
					rid = CSphereUID( RES_Template, atoi( s.GetSection()));
				}
				else if ( ! _strnicmp( pszTitle, "spee", 4 ))
				{
					restype = RES_Speech;
					rid = CSphereUID( RES_Speech, Str_ahextou( s.GetSection()));
				}
			}
		}
		else
		{
			// _stricmp( "ITEMMENU"
		}

		if ( restype <= 0 )
		{
			g_Log.Event( LOG_GROUP_INIT, LOGL_WARN, "Unknown section '%s' in '%s'" LOG_CR, (LPCTSTR) s.GetKey(), (LPCTSTR) s.GetFileTitle());
			return( false );
		}
	}
	else
	{
		// Create a new index for the block.
		// NOTE: rid is not created for all types.
		// NOTE: GetArgRaw() is not always the DEFNAME
		LPCTSTR pszArg = s.GetArgRaw();
		if ( pszArg == NULL ) pszArg = "";
		rid = ResourceGetNewID( restype, pszArg, pVarNum );
	}

	if ( ! rid.IsValidRID())
	{
		DEBUG_ERR(( "Invalid %s block index '%s'" LOG_CR, (LPCTSTR) s.GetSection(), (LPCTSTR) s.GetArgRaw()));
		return( false );
	}

	// NOTE: It is possible to be replacing an existing entry !!! Check for this.
	// a previous object with the same id
	// Though it may not apply.
	CResourceDefPtr pPrvDef = ResourceGetDef( rid );

	CResourceObjPtr pNewObj;
	CResourceDefPtr pNewDef;
	CResourceLinkPtr pNewLink;

	switch ( restype )
	{
		// Handle the single instance stuff first.

	case RES_Sphere:
		// Define main global config info.
		g_Serv.s_LoadProps(s);
		return( true );

	case RES_BlockIP:
		while ( s.ReadKeyParse())
		{
			SetLogIPBlock( s.GetKey(), s.GetArgStr(), NULL );
		}
		return( true );

	case RES_Comment: // Just toss this.
		return( true );

	case RES_DefNames:
		// just get a block of defs.
		while ( s.ReadKeyParse())
		{
			g_Cfg.m_Const.SetKeyVar( s.GetKey(), s.GetArgVar());
		}
		return( true );
	case RES_VarNames:
		// just get a block of defs.
		while ( s.ReadKeyParse())
		{
			g_Cfg.m_Var.SetKeyVar( s.GetKey(), s.GetArgVar());
		}
		return( true );

	case RES_NotoTitles:
		while ( s.ReadLine())
		{
			TCHAR* pName = s.GetLineBuffer();
			if ( *pName == '<' )
				pName = "";
			m_NotoTitles.Add( pName );
		}
		return( true );
	case RES_Obscene:
		while ( s.ReadLine())
		{
			m_Obscene.AddSortString( s.GetKey());
		}
		return( true );
	case RES_Resources:
		// Add these all to the list of files we need to include.
		while ( s.ReadLine())
		{
			AddResourceFile( s.GetKey());
		}
		return( true );
	case RES_Runes:
		// The full names of the magic runes.
		m_Runes.RemoveAll();
		while ( s.ReadLine())
		{
			m_Runes.Add( s.GetKey());
		}
		return( true );
	case RES_Servers:	// Old way to define a block of servers.
		{
			bool fReadSelf = false;

			CThreadLockPtr lock( &m_Servers );
			while ( s.ReadLine())
			{
				// Does the name already exist ?
				bool fAddNew = false;
				CServerPtr pServ;
				int i = m_Servers.FindKey( s.GetKey());
				if ( i < 0 )
				{
					pServ = new CServerDef( UID_INDEX_CLEAR, s.GetKey(), CSocketAddressIP( SOCKET_LOCAL_ADDRESS ));
					fAddNew = true;
				}
				else
				{
					pServ = Server_GetDef(i);
				}
				if ( s.ReadLine())
				{
					pServ->m_ip.SetHostPortStr( s.GetKey());
					if ( s.ReadLine())
					{
						pServ->m_ip.SetPort( (WORD)atoi( s.GetKey()));
					}
				}
				if ( ! _stricmp( pServ->GetName(), g_Serv.GetName()))
				{
					fReadSelf = true;
				}
				if ( g_Serv.m_ip == pServ->m_ip )
				{
					fReadSelf = true;
				}
				if ( fReadSelf )
				{
					// I can be listed first here. (old way)
					g_Serv.SetName( pServ->GetName());
					g_Serv.m_ip = pServ->m_ip;
					fReadSelf = false;
					continue;
				}
				if ( fAddNew )
				{
					m_Servers.AddSortKey( pServ, pServ->GetName());
				}
			}
		}
		return( true );

	case RES_TypeDefs:
		// just get a block of defs.
		while ( s.ReadKeyParse())
		{
			CSphereUID ridnew( RES_TypeDef, s.GetArgInt());
			CResourceDefPtr pResDef = new CResourceDef( ridnew, s.GetKey());
			ASSERT(pResDef);
			m_ResHash.AddSortKey( pResDef, ridnew );
		}
		return( true );

	case RES_Starts:
		// Weird multi line format.
		m_StartDefs.RemoveAll();
		while ( s.ReadLine())
		{
			CStartLoc* pStart = new CStartLoc( s.GetLineBuffer());
			if ( s.ReadLine())
			{
				pStart->m_sName = s.GetLineBuffer();
				if ( s.ReadLine())
				{
					{ CGVariant vTmp( s.GetLineBuffer()); pStart->m_pt.v_Set( vTmp ); }
				}
			}
			m_StartDefs.Add( pStart );
		}
		return( true );

	case RES_MoonGates:
//		m_MoonGates.RemoveAll();
		while ( s.ReadLine())
		{
			char* pszName = strchr( s.GetLineBuffer(), '=' );
			if ( pszName ) *pszName = '\0';
			CPointMap pt = GetRegionPoint( s.GetLineBuffer());
			m_MoonGates.Add( pt );
		}
		return( true );

	case RES_Teleporters:
		while ( s.ReadLine())
		{
			// Add the teleporter to the CSector.
			CTeleportPtr pTeleport = new CTeleport( s.GetLineBuffer());
			ASSERT(pTeleport);
			// make sure this is not a dupe.
			pTeleport->RealizeTeleport();
		}
		return( true );

	//*************************************************************
	// Non linked (multi instance) stuff.

	case RES_PLevel:
		{
			if ( rid.GetResIndex() >= COUNTOF(m_PrivCommands))
				return( false );
			while ( s.ReadLine())
			{
				m_PrivCommands[rid.GetResIndex()].AddSortString( s.GetKey());
			}
		}
		return( true );

	case RES_Map:
		// Just toss this for now. CMulMap wrapper
		return( true );
	case RES_Location:
		// ignore this. (Axis stuff)
		return( true );

	case RES_Account:	// NOTE: ArgStr is not the DEFNAME
		// Load the account now. Not normal method !
		return g_Accounts.Account_Load( s, false );

	case RES_Sector: // saved in world file.
		{
			CPointMap pt = GetRegionPoint( s.GetArgRaw()); // Decode X/Y/Z
			if ( ! pt.IsValidXY())
				return( false );
			pNewObj = pt.GetSector();
			return( pNewObj->s_LoadProps(s));
		}
		return( true );

	case RES_Spell:
		if ( pPrvDef )
		{
			pNewDef = REF_CAST(CSpellDef,pPrvDef );
			ASSERT(pNewDef);
		}
		else
		{
			CSpellDefWPtr pSpellDef = new CSpellDef( (SPELL_TYPE) rid.GetResIndex());
			m_SpellDefs.SetAtGrow( rid.GetResIndex(), pSpellDef );
			pNewDef = pSpellDef;
		}
		pNewDef->s_LoadProps(s);
		break;

	case RES_RegionResource:
		if ( pPrvDef )
		{
			pNewDef = REF_CAST(CRegionResourceDef,pPrvDef );
			ASSERT(pNewDef);
		}
		else
		{
			pNewDef = new CRegionResourceDef( rid );
			ASSERT(pNewDef);
			m_ResHash.AddSortKey( pNewDef, rid );
		}
		pNewDef->s_LoadProps(s);
		break;

	case RES_Server:	// saved in world file.
		{
			CThreadLockPtr lock( &m_Servers );
			CServerPtr pServ;
			int i = m_Servers.FindKey( s.GetKey());
			if ( i < 0 )
			{
				pServ = new CServerDef( rid, s.GetArgStr(), CSocketAddressIP( SOCKET_LOCAL_ADDRESS ));
			}
			else
			{
				pServ = Server_GetDef(i);
			}
			pNewDef = pServ;
			pNewDef->s_LoadProps(s);
			if ( pNewDef->GetName()[0] == '\0' )
			{
				return( false );
			}
			if ( i < 0 )
			{
				m_Servers.AddSortKey( pServ, pServ->GetName() );
			}
		}
		return( true );

	case RES_Area:
		try {
			// Map stuff that could be duplicated !!!
			// NOTE: ArgStr is NOT the DEFNAME in this case
			CRegionComplexPtr pRegion = new CRegionComplex( rid, s.GetArgStr());
			ASSERT(pRegion);
			pNewDef = pRegion;
			pRegion->s_LoadProps(s);
			if ( pRegion->RealizeRegion())
			{
				// might be a dupe ?
				m_ResHash.AddSortKey( pRegion, rid );
			}
		} catch (...) {
			g_Log.Event( LOG_GROUP_INIT, LOGL_WARN, "Failed to load AREA '%s'" LOG_CR, (LPCTSTR) s.GetArgStr());
		}
		return( true );
	case RES_Room:
		{
			// NOTE: ArgStr is not the DEFNAME
			// ??? duplicate rooms ?
			// ??? Insert with new id number.
			CRegionPtr pRegion = new CRegionBasic( rid, s.GetArgStr());
			ASSERT(pRegion);
			pNewDef = pRegion;
			pRegion->s_LoadProps(s);
			if ( pRegion->RealizeRegion())
			{
				// might be a dupe ?
				m_ResHash.AddSortKey( pRegion, rid );
			}
		}
		return( true );
		// Saved in the world file.

	case RES_GMPage:	// saved in world file. (Name is NOT DEFNAME)
		{
			CAccountPtr pAccount = g_Accounts.Account_FindNameCheck( s.GetArgStr());
			if ( ! pAccount )
				return false;
			CGMPage* pGMPage = new CGMPage( pAccount );
			return( pGMPage->s_LoadProps(s));
		}
		return false;

	case RES_WorldChar:	// saved in world file.
		if ( ! rid.IsValidRID())
		{
			g_Log.Event( LOG_GROUP_INIT, LOGL_ERROR, "Undefined char type '%s'" LOG_CR, (LPCTSTR) s.GetArgRaw());
			return( false );
		}
		try {
			pNewObj = CChar::CreateBasic((CREID_TYPE)rid.GetResIndex());
			if ( pNewObj == NULL )
				return false;
			return( pNewObj->s_LoadProps(s));
		} catch (...) {
			return false;
		}

	case RES_WorldItem:	// saved in world file.
		if ( ! rid.IsValidRID())
		{
			g_Log.Event( LOG_GROUP_INIT, LOGL_ERROR, "Undefined item type '%s'" LOG_CR, (LPCTSTR) s.GetArgRaw());
			return( false );
		}
		try {
			pNewObj = CItem::CreateBase((ITEMID_TYPE)rid.GetResIndex());
			if ( pNewObj == NULL )
				return false;
			return( pNewObj->s_LoadProps(s));
		} catch (...) {
			return false;
		}

	//*******************************************************************
	// Linked Class objects.
	// Might need to check if the link already exists ?

	case RES_Skill:
		{
		CSkillDefWPtr pSkill;
		if ( pPrvDef )
		{
			pNewLink = pSkill = REF_CAST(CSkillDef,pPrvDef );
			ASSERT(pSkill);
		}
		else
		{
			pSkill = new CSkillDef( (SKILL_TYPE) rid.GetResIndex());
			ASSERT(pSkill);
			pNewLink = pSkill;
		}
		CScriptLineContext LineContext = s.GetContext();
		pSkill->s_LoadProps(s);
		s.SeekContext( LineContext );

		// build a name sorted list.
		m_SkillNameDefs.AddSortKey( pSkill, pSkill->GetSkillKey());
		// Hard coded value for skill index.
		m_SkillIndexDefs.SetAtGrow( rid.GetResIndex(), pSkill );
		// m_ResHash.AddSortKey( pSkill, rid );
		}
		break;

	case RES_Events:
		if ( pPrvDef )
		{
			pNewLink = REF_CAST(CCharEvents,pPrvDef );
		}
		if ( pNewLink == NULL )
		{
			pNewLink = new CCharEvents( rid );
		}
		ASSERT(pNewLink);
		m_ResHash.AddSortKey( pNewLink, rid );
		break;

	case RES_TypeDef:
		if ( pPrvDef )
		{
			pNewLink = REF_CAST(CItemTypeDef,pPrvDef );
		}
		if ( pNewLink == NULL )
		{
			pNewLink = new CItemTypeDef( rid );
		}
		ASSERT(pNewLink);
		m_ResHash.AddSortKey( pNewLink, rid );
		break;

	case RES_BlockEMail:	// single instance link?

	case RES_Dialog:
	case RES_Menu:
	case RES_Speech:
	case RES_SkillMenu:

	case RES_Book:
	case RES_EMailMsg:
	case RES_Names:
	case RES_Newbie:
	case RES_Scroll:
	case RES_Template:
	case RES_Function:
	case RES_RaceClass:
		// Just index this for access later.

		if ( pPrvDef )
		{
			pNewLink = REF_CAST(CResourceLink,pPrvDef);
			ASSERT(pNewLink);
		}
		else
		{
			pNewLink = new CResourceLink( rid );
			ASSERT(pNewLink);
			m_ResHash.AddSortKey( pNewLink, rid );
		}
		break;

	case RES_RegionType:
	case RES_Spawn:
		if ( pPrvDef )
		{
			pNewLink = REF_CAST(CRegionType,pPrvDef);
			ASSERT(pNewLink);
		}
		else
		{
			pNewLink = new CRegionType( rid );
			ASSERT(pNewLink);
			m_ResHash.AddSortKey( pNewLink, rid );
		}
		{
			CScriptLineContext LineContext = s.GetContext();
			pNewLink->s_LoadProps( s );
			s.SeekContext( LineContext );
		}
		break;

	case RES_Profession:
		if ( pPrvDef )
		{
			pNewLink = REF_CAST(CProfessionDef,pPrvDef);
			ASSERT(pNewLink);
		}
		else
		{
			pNewLink = new CProfessionDef( rid );
			ASSERT(pNewLink);
			m_ResHash.AddSortKey( pNewLink, rid );
		}
		{
			CScriptLineContext LineContext = s.GetContext();
			pNewLink->s_LoadProps( s );
			s.SeekContext( LineContext );
		}
		break;

	case RES_CharDef:
		if ( pPrvDef )
		{
			pNewLink = REF_CAST(CResourceLink,pPrvDef);
			ASSERT(pNewLink);
			pNewLink->UnLink();
			CCharDefPtr pBaseDef = REF_CAST(CCharDef,pNewLink);
			if ( pBaseDef )
			{
				CScriptLineContext LineContext = s.GetContext();
				pBaseDef->s_LoadProps(s);
				s.SeekContext( LineContext );
			}
		}
		else
		{
			pNewLink = new CCharEvents( rid );
			ASSERT(pNewLink);
			m_ResHash.AddSortKey( pNewLink, rid );
		}
		break;

	case RES_ItemDef:
		// ??? existing smart pointers to RES_ItemDef ?
		if ( pPrvDef )
		{
			pNewLink = REF_CAST(CResourceLink,pPrvDef);
			if ( pNewLink == NULL )
			{
				ASSERT( REF_CAST(CItemDefDupe,pPrvDef));
				return( true );
			}
			pNewLink->UnLink();
			CItemDefPtr pBaseDef = REF_CAST(CItemDef,pNewLink);
			if ( pBaseDef )
			{
				CScriptLineContext LineContext = s.GetContext();
				pBaseDef->s_LoadProps(s);
				s.SeekContext( LineContext );
			}
		}
		else
		{
			pNewLink = new CItemTypeDef( rid );
			ASSERT(pNewLink);
			m_ResHash.AddSortKey( pNewLink, rid );
		}
		break;

	case RES_WebPage:
		// Read a web page entry.
		if ( pPrvDef )
		{
			pNewLink = REF_CAST(CWebPageDef,pPrvDef);
			ASSERT(pNewLink);
		}
		else
		{
			CWebPagePtr pWebPage = new CWebPageDef( rid );
			pNewLink = pWebPage;
			ASSERT(pWebPage);
			m_WebPages.AddSortKey( pWebPage, rid );
		}
		{
			CScriptLineContext LineContext = s.GetContext();
			pNewLink->s_LoadProps(s);
			s.SeekContext( LineContext ); // set the pos back so ScanSection will work.
		}
		break;

	case RES_Help:	// (Name is NOT DEFNAME)
		{
			CResourceNamed *pHelpRef = new CResourceNamed( rid, s.GetArgStr());
			pNewLink = pHelpRef;
			m_HelpDefs.AddSortKey( pHelpRef, pHelpRef->GetName());
		}
		break;

		//**************************************************

	default:
		ASSERT(0);
		return( false );
	}

	if ( pNewLink )
	{
		pNewLink->SetResourceVar( pVarNum );

		// NOTE: we should not be linking to stuff in the *WORLD.SCP file.
		CResourceScriptPtr pResScript = PTR_CAST(CResourceScript,&s);
		if ( pResScript == NULL )	// can only link to it if it's a CResourceScript !
		{
			DEBUG_ERR(( "Can't link resources in the world save file" LOG_CR ));
			return( false );
		}

		// Save the section context BEFORE scanning for DEFNAME properties.
		// This context is the file offset pointing to the start of the section body.
		CScriptLineContext LinkContext = s.GetContext();

		// Scan section for DEFNAME= and DEFNAME2= properties to register them in m_Const.
		// This is needed because numeric [ITEMDEF 0e75] sections define their DEFNAME inside.
		{
			while ( s.ReadKeyParse())
			{
				if ( ! _stricmp( s.GetKey(), "DEFNAME" ) || ! _stricmp( s.GetKey(), "DEFNAME2" ))
				{
					LPCTSTR pszDefName = s.GetArgStr();
					if ( pszDefName && pszDefName[0] )
					{
						g_Cfg.m_Const.SetKeyVar( pszDefName, CGVariant( VARTYPE_UID, &rid ));
					}
				}
			}
			s.SeekContext( LinkContext );
		}

		pNewLink->SetLinkSection(pResScript, LinkContext);
	}
	else if ( pNewDef && pVarNum )
	{
		// Not linked but still may have a var name
		pNewDef->SetResourceVar( pVarNum );
	}

	return( true );
}

//*************************************************************

CSphereUID CSphereResourceMgr::ResourceGetNewID( RES_TYPE restype, LPCTSTR pszName, CVarDefPtr& pVarNum )
{
	// We are reading in a script block.
	// We may be creating a new id or replacing an old one.
	// ARGS:
	//	restype = the data type we are reading in.
	//  pszName = MAy or may not be the DEFNAME depending on type.

	const CSphereUID ridinvalid;	// LINUX wants this for some reason.
	if ( pszName == NULL || pszName[0] == '\0' )
	{
		return ridinvalid;
	}

	CSphereUID rid;
	int iPage = 0;	// sub page

	// Some types don't use named indexes at all. (Single instance)
	switch ( restype )
	{
	case RES_UNKNOWN:
		return( ridinvalid);

	case RES_BlockEMail:
		// (single instance) m_ResourceLinks linked stuff.
		return( CSphereUID( restype ));

	case RES_BlockIP:
	case RES_Comment:
	case RES_DefNames:
	case RES_MoonGates:
	case RES_NotoTitles:
	case RES_Obscene:
	case RES_Resources:
	case RES_Runes:
	case RES_Servers:
	case RES_Sphere:
	case RES_Starts:
	case RES_Teleporters:
	case RES_TypeDefs:
	case RES_VarNames:
		// Single instance stuff. (fully read in)
		// Ignore any resource name.
		return( CSphereUID( restype ));

	case RES_Help:
		// Private name range.

	case RES_Account:
	case RES_GMPage:
	case RES_Server:
	case RES_Sector:
		// These must have a resource name but do not use true CSphereUID format.
		// These are multiple instance but name is not a CSphereUID
		if ( pszName[0] == '\0' )
			return( ridinvalid );	// invalid
		return( CSphereUID( restype ));

		// Extra args are allowed.
	case RES_Book:	// BOOK BookName page
	case RES_Dialog:	// DIALOG GumpName ./TEXT/BUTTON
	case RES_RegionType:
		{
			if ( pszName[0] == '\0' )
				return( ridinvalid );

			TCHAR* pArg1 = Str_GetTemp();
			strcpy( pArg1, pszName );
			pszName = pArg1;
			TCHAR* pArg2;
			Str_Parse( pArg1, pArg2 );

			if ( ! _stricmp( pArg2, "TEXT" ))
				iPage = RES_DIALOG_TEXT;
			else if ( ! _stricmp( pArg2, "BUTTON" ))
				iPage = RES_DIALOG_BUTTON;
			else if (pArg2[0])
			{
				iPage = RES_GET_INDEX( Exp_GetValue( pArg2 ));
			}
			if ( iPage > 255 )
			{
				DEBUG_ERR(( "Bad resource index page %d" LOG_CR, iPage ));
			}
		}
		break;

	case RES_Newbie:	// MALE_DEFAULT, FEMALE_DEFAULT, Skill
		if ( ! _stricmp( pszName, "MALE_DEFAULT" ))
			return( CSphereUID( RES_Newbie, RES_NEWBIE_MALE_DEFAULT ));
		if ( ! _stricmp( pszName, "FEMALE_DEFAULT" ))
			return( CSphereUID( RES_Newbie, RES_NEWBIE_FEMALE_DEFAULT ));
		break;

	case RES_Area:
	case RES_Room:
		// Name is not the defname or id, just find a free id.
		pszName = NULL;	// fake it out for now.
		break;

	case RES_Events:		// An Event handler block with the trigger type in it. ON=@Death etc.
	case RES_TypeDef:			// Define a trigger block for a RES_WorldItem m_type.
		break;
	default:
		// The name is a DEFNAME or id number
		ASSERT( restype < RES_QTY );
		break;
	}

	int index;
	if ( pszName )
	{
		if ( pszName[0] == '\0' )	// absense of resourceid = index 0
		{
			// This might be ok.
			return( CSphereUID( restype, 0, iPage ) );
		}

		if ( isdigit(pszName[0]))	// Its just an index.
		{
			index = Exp_GetValue(pszName);

			rid = CSphereUID( restype, index );

			switch ( restype )
			{
			case RES_Book:			// A book or a page from a book.
			case RES_Dialog:			// A scriptable gump dialog: text or handler block.
			case RES_RegionType:	// Triggers etc. that can be assinged to a RES_Area
				rid = CSphereUID( restype, index, iPage );
				break;

			case RES_Events:		// An Event handler block with the trigger type in it. ON=@Death etc.
			case RES_TypeDef:			// Define a trigger block for a RES_WorldItem m_type.
				break;

			case RES_SkillMenu:
			case RES_Menu:			// General scriptable menus.
			case RES_EMailMsg:		// define an email msg that could be sent to an account.
			case RES_Speech:		// A speech block with ON=*blah* in it.
			case RES_Names:			// A block of possible names for a NPC type. (read as needed)
			case RES_Scroll:		// SCROLL_GUEST=message scroll sent to player at guest login. SCROLL_MOTD: SCROLL_NEWBIE
			case RES_CharDef:		// Define a char type.
			case RES_ItemDef:		// Define an item type
			case RES_Template:		// Define lists of items. (for filling loot etc)
				break;

			default:
				return( rid );
			}

#ifdef _DEBUG
			if ( g_Serv.m_iModeCode != SERVMODE_ResyncLoad )	// this really is ok.
			{
				// Warn of  duplicates.
				index = m_ResHash.FindKey( rid );
				if ( index >= 0 )	// i found it. So i have to find something else.
				{
					CResourceDefPtr pDef = m_ResHash.GetAt( rid, index );
					ASSERT(pDef);
					DEBUG_WARN(( "redefinition of '%s' ('%s')" LOG_CR, (LPCTSTR) GetResourceBlockName(restype), (LPCTSTR) pDef->GetName() ));
				}
			}
#endif

			return( rid );
		}

		pVarNum = g_Cfg.m_Const.FindKeyPtr( pszName );
		if ( pVarNum )
		{
			// An existing VarDef with the same name ?
			// We are creating a new Block but using an old name ? weird.
			// just check to see if this is a strange type conflict ?

			rid = pVarNum->GetDWORD();
			if ( restype != rid.GetResType())
			{
				switch ( restype )
				{
				case RES_WorldChar:
				case RES_WorldItem:
				case RES_Newbie:
				case RES_PLevel:
					// These are not truly defining a new DEFNAME
					break;
				default:
					DEBUG_ERR(( "Redefined name '%s' from %s to %s" LOG_CR, (LPCTSTR) pszName, (LPCTSTR) GetResourceBlockName(rid.GetResType()), (LPCTSTR) GetResourceBlockName(restype) ));
					return( ridinvalid );
				}
			}
			else if ( iPage == rid.GetResPage())	// Books and dialogs have pages.
			{
				// We are redefining an item we have already read in ?
				// Why do this unless it's a Resync ?
				if ( g_Serv.m_iModeCode != SERVMODE_ResyncLoad )
				{
					DEBUG_WARN(( "Redef resource '%s'" LOG_CR, (LPCTSTR) pszName ));
				}
			}

			return( CSphereUID( restype, rid.GetResIndex(), iPage ));
		}
	}

	// we must define this as a new unique entry.
	// Find a new free entry.

	int iHashRange = 0;
	switch ( restype )
	{

		// Some cannot create new id's
		// Do not allow NEW named indexs for some types for now.

	case RES_Skill:			// Define attributes for a skill (how fast it raises etc)
		// rid = m_SkillDefs.GetCount();
	case RES_Spell:			// Define a magic spell. (0-64 are reserved)
		// rid = m_SpellDefs.GetCount();
		return( ridinvalid );

		// These MUST exist !

	case RES_Newbie:	// MALE_DEFAULT, FEMALE_DEFAULT, Skill
		return( ridinvalid );
	case RES_PLevel:	// 0-7
		return( ridinvalid );
	case RES_WorldChar:
	case RES_WorldItem:
		return( ridinvalid );

		// Just find a free entry in proper range.

	case RES_CharDef:		// Define a char type.
		iHashRange = 2000;
		index = NPCID_SCRIPT2 + 0x2000;	// add another offset to avoid Sphere ranges.
		break;
	case RES_ItemDef:		// Define an item type
		iHashRange = 2000;
		index = ITEMID_SCRIPT2 + 0x4000;	// add another offset to avoid Sphere ranges.
		break;
	case RES_Template:		// Define lists of items. (for filling loot etc)
		iHashRange = 2000;
		index = ITEMID_TEMPLATE + 100000;
		break;

	case RES_Book:			// A book or a page from a book.
	case RES_Dialog:			// A scriptable gump dialog: text or handler block.
		if ( iPage )	// We MUST define the main section FIRST !
			return( ridinvalid );

	case RES_RegionType:	// Triggers etc. that can be assinged to a RES_Area
		iHashRange = 100;
		index = 1000;
		break;

	case RES_Area:
	case RES_Room:
	case RES_SkillMenu:
	case RES_Menu:			// General scriptable menus.
	case RES_EMailMsg:		// define an email msg that could be sent to an account.
	case RES_Events:			// An Event handler block with the trigger type in it. ON=@Death etc.
	case RES_Speech:			// A speech block with ON=*blah* in it.
	case RES_Names:			// A block of possible names for a NPC type. (read as needed)
	case RES_Scroll:		// SCROLL_GUEST=message scroll sent to player at guest login. SCROLL_MOTD: SCROLL_NEWBIE
	case RES_TypeDef:			// Define a trigger block for a RES_WorldItem m_type.
	case RES_Profession:		// Define specifics for a char with this skill class. (ex. skill caps)
	case RES_RegionResource:
	case RES_Function:
	case RES_RaceClass:
		iHashRange = 1000;
		index = 10000;
		break;
	case RES_Spawn:			// Define a list of NPC's and how often they may spawn.
		iHashRange = 1000;
		index = SPAWNTYPE_START + 100000;
		break;

	case RES_WebPage:		// Define a web page template.
		index = m_WebPages.GetSize() + 1;
		break;

	default:
		ASSERT(0);
		return( ridinvalid );
	}

	if ( iPage )
	{
		rid = CSphereUID( restype, index, iPage );
	}
	else
	{
		rid = CSphereUID( restype, index );
	}

	if ( iHashRange )
	{
		// find a new FREE entry starting here

		rid = m_ResHash.FindKeyFree( rid + Calc_GetRandVal( iHashRange ));
	}
	else
	{
		// RES_WebPage - find a new FREE entry starting here
		while ( ResourceGetDef(rid) || ! index )
		{
			rid = rid+1;
		}
	}

	if ( pszName )
	{
		// We know this value and key does not already exist.
		int iVarNum = g_Cfg.m_Const.SetKeyVar( pszName, CGVariant( VARTYPE_UID, &rid ));
		if ( iVarNum >= 0 )
		{
			pVarNum = g_Cfg.m_Const.GetAt(iVarNum);
		}
		else
		{
			ASSERT(iVarNum >= 0);
		}
	}

	return( rid );
}

CResourceDefPtr CSphereResourceMgr::ResourceGetDef( UID_INDEX uid )
{
	// Get a CResourceDef from the CSphereUID.
	// NOTE:
	//  this does not load CCharDef and CItemDef if not already loaded !
	// ARGS:
	//	restype = id must be this type.

	CSphereUID rid = uid;
	if ( ! rid.IsValidRID())
		return( NULL );

	int index = rid.GetResIndex();
	RES_TYPE restype = rid.GetResType();
	switch ( restype )
	{
		// Some things cannot be looked up by UID or RID.
	case RES_VarNames:
	case RES_MoonGates:
	case RES_PLevel:
	case RES_TypeDefs:
	case RES_DefNames:
	case RES_Runes:
	case RES_NotoTitles:
	case RES_Obscene:
	case RES_Resources:
	case RES_Comment:
	case RES_BlockIP:
		return( NULL );

	case RES_Starts:
	case RES_Map:
	case RES_Sphere:
	case RES_Teleporters:
	case RES_Sector:
		return( NULL );

	case RES_WorldItem:
	case RES_WorldChar:
		// ASSERT(0);	// ! IsValidRID
		return( NULL );

	// case RES_Account:
		// these can be enumerated this way !

	case RES_WebPage:
		index = m_WebPages.FindKey( rid );
		if ( index < 0 )
			return( NULL );
		return( m_WebPages.GetAt( index ));

	case RES_Skill:
		if ( ! m_SkillIndexDefs.IsValidIndex(index))
			return( NULL );
		return( (CResourceDef*)m_SkillIndexDefs[ index ] );

	case RES_Spell:
		if ( ! m_SpellDefs.IsValidIndex(index))
			return( NULL );
		return( (CResourceDef*)m_SpellDefs[ index ] );

	case RES_Server:
#if 0
		if ( ! m_Servers.IsValidIndex(index))
			return( NULL );
		return( PTR_CAST(CServerDef,m_Servers[ index ] ));
#endif
	case RES_UNKNOWN:	// legal to use this as a ref but it is unknown
		return( NULL );

#ifdef _DEBUG
	case RES_BlockEMail:
	case RES_Book:			// A book or a page from a book.
	case RES_EMailMsg:		// define an email msg that could be sent to an account.
	case RES_Events:
	case RES_Dialog:			// A scriptable gump dialog: text or handler block.
	case RES_Menu:
	case RES_Names:			// A block of possible names for a NPC type. (read as needed)
	case RES_Newbie:	// MALE_DEFAULT, FEMALE_DEFAULT, Skill
	case RES_RegionResource:
	case RES_RegionType:		// Triggers etc. that can be assinged to a RES_Area
	case RES_Scroll:		// SCROLL_GUEST=message scroll sent to player at guest login. SCROLL_MOTD: SCROLL_NEWBIE
	case RES_Speech:
	case RES_TypeDef:			// Define a trigger block for a RES_WorldItem m_type.
	case RES_Template:
	case RES_SkillMenu:
	case RES_Spawn:	// the char spawn tables
	case RES_Profession:
	case RES_Function:
	case RES_RaceClass:
		break;
#endif

	case RES_ItemDef:
		//return FindItemDef( (ITEMID_TYPE) index);
	case RES_CharDef:
		//return FindCharDef( (CREID_TYPE) index);
		break;

	case RES_Area:	// ??? can these be enum this way
	case RES_Room:
	case RES_GMPage:
	case RES_Servers:
	case RES_Stat:
		break;
	default:
		DEBUG_CHECK(0);
		break;
	}

	return CResourceMgr::ResourceGetDef( rid );
}

CResourceObjPtr CSphereResourceMgr::FindUID( UID_INDEX uid ) // virtual
{
	// Can it be resolved to any uid ref.
	CSphereUID uid2 = uid;
	if ( ! uid2.IsValidRID())
	{
		return g_World.ObjFind(uid);
	}

	// Get ref to resource id !?
	// NOTE: watch security hole for CAccount?
	int index = uid2.GetResIndex();
	RES_TYPE restype = uid2.GetResType();
	switch ( restype )
	{
	case RES_ItemDef:
		return FindItemDef( (ITEMID_TYPE) index);
	case RES_CharDef:
		return FindCharDef( (CREID_TYPE) index);
	}

	return( ResourceGetDef(uid));
}

////////////////////////////////////////////////////////////////////////////////

void CSphereResourceMgr::LoadChangedFiles()
{
	// Look for files in the changed directory.
	// *ACCT.SCP
	// m_ResourceFiles
	// m_WebPages

	if ( m_sSCPInBoxDir.IsEmpty())
		return;
	if ( ! m_sSCPInBoxDir.CompareNoCase( m_sSCPBaseDir ))
		return;

	// If there is a file here and i can open it exlusively then do a resync on this file.
	CFileDir filedir;
	int iRet = filedir.ReadDir( m_sSCPInBoxDir );
	if ( iRet < 0 )
	{
		DEBUG_MSG(( "DirList=%d for '%s'" LOG_CR, iRet, (LPCTSTR) m_sSCPInBoxDir ));
		return;
	}
	if ( iRet <= 0 )	// no files here.
		return;

	bool fSetResync = false;
	TCHAR szTmpDir[ 260 ];
	strcpy( szTmpDir, m_sSCPInBoxDir );
	CGFile::GetStrippedDirName( szTmpDir );

	for ( int i=0; i<filedir.GetSize(); i++ )
	{
		CGString sPathName = CGFile::GetMergedFileName( szTmpDir, (LPCTSTR) filedir[i] );
		DEBUG_MSG(( "LoadChange to '%s'='%s'" LOG_CR, (LPCTSTR) filedir[i], (LPCTSTR) sPathName ));

		// Is the file matching one of my system files ?

#if 0
		if ( ! g_Serv.m_fResyncPause )
		{
			fSetResync = true;
			g_Serv.SetResyncPause( true, this );
		}

		// Copy it into the new scripts dir. FIRST !
		CGString sNewPathName = CGFile::GetMergedFileName( m_sSCPBaseDir, (LPCTSTR) filedir[i].m_sFileName );

		// Copy to m_sSCPBaseDir

		// Now load it. (because we are going to store it's name here !)
		LoadResourcesAdd( sNewPathName );

		// now delete the old file. we are done with it.
		// remove( sPathName );
#endif

	}

	if ( fSetResync )
	{
		g_Serv.SetResyncPause( false, &g_Serv );
	}
}

bool CSphereResourceMgr::SetLogIPBlock( LPCTSTR pszIP, LPCTSTR pszBlockReason, CScriptConsole* pSrc )
{
	// NOTE: pSrc = NULL so we dont write as we read the INI file.
	if ( pSrc )
	{
		if ( pSrc->GetPrivLevel() < PLEVEL_Admin )
			return( false );
	}

	if ( ! m_LogIP.SetLogIPBlock( pszIP, pszBlockReason ))
	{
		if ( pSrc )
		{
			pSrc->Printf( "No change in IP status for '%s'.", pszIP );
		}
		return false;
	}

	// Write change to *.INI now.
	if ( pSrc )
	{
		g_Log.Event( LOG_GROUP_ACCOUNTS, LOGL_EVENT, "%sBlock IP %s by %s for '%s'" LOG_CR, 
			pszBlockReason ? "" : "UN-", pszIP, pSrc->GetName(), pszBlockReason );

		m_scpIni.WriteProfileStringSec( "BLOCKIP", pszIP, pszBlockReason ? "" : NULL );
	}
	return true;
}

void CSphereResourceMgr::OnTick( bool fNow )
{
	// Give a tick to the less critical stuff.

	if ( ! fNow )
	{
		if ( g_Serv.IsLoading())
			return;
		if ( m_timePeriodic > CServTime::GetCurrentTime())
			return;
	}

	// Do periodic resource stuff.

	m_LogIP.OnTick();

	for ( int i=0; i<m_WebPages.GetSize(); i++ )
	{
		CWebPagePtr pWeb = m_WebPages[i];
		ASSERT(pWeb);
		pWeb->OnTick(fNow);
	}

	// Check to see if the resource files have not been acccessed in a while.
	for ( int k=0;; k++ )
	{
		CResourceFilePtr pResFile = GetResourceFile(k);
		if ( pResFile == NULL )
			break;
		pResFile->OnTick(fNow);
	}
	m_scpIni.OnTick(fNow);

	LoadChangedFiles();

	m_timePeriodic.InitTimeCurrent( 60* TICKS_PER_SEC );
}

//*************************************************************
// CResourceMgr base methods

LPCTSTR CResourceMgr::ResourceGetName(CSphereUID rid) const
{
	// Format the RID as a hex string, or look up the resource name.
	int index = m_ResHash.FindKey(rid);
	if (index >= 0)
	{
		CResourceDef* pDef = m_ResHash.GetAt(index);
		if (pDef)
		{
			LPCTSTR pszName = pDef->GetResourceName();
			if (pszName && pszName[0])
				return pszName;
		}
	}
	return "?";
}

CResourceFilePtr CResourceMgr::FindResourceFile(LPCTSTR pszName)
{
	if (!pszName || !pszName[0])
		return NULL;

	LPCTSTR pszTitle = CGFile::GetFileNameTitle(pszName);

	for (int i = 0; i < m_ResourceFiles.GetSize(); i++)
	{
		CResourceScript* pScript = m_ResourceFiles[i];
		if (!pScript)
			continue;
		LPCTSTR pszExistingTitle = CGFile::GetFileNameTitle(pScript->GetFilePath());
		if (!_stricmp(pszTitle, pszExistingTitle))
			return static_cast<CResourceFile*>(pScript);
	}
	return NULL;
}

void CResourceMgr::AddResourceFile(LPCTSTR pszFile)
{
	if (!pszFile || !pszFile[0])
		return;

	// Normalize backslashes to forward slashes on Linux
	TCHAR szPath[1024];
	strncpy(szPath, pszFile, sizeof(szPath) - 1);
	szPath[sizeof(szPath) - 1] = '\0';
	for (TCHAR* p = szPath; *p; p++)
	{
		if (*p == '\\')
			*p = '/';
	}

	// Append .scp extension if no extension present
	LPCTSTR pszExt = CGFile::GetFileNameExt(szPath);
	if (!pszExt || !pszExt[0])
	{
		strncat(szPath, ".scp", sizeof(szPath) - strlen(szPath) - 1);
	}

	// Check for duplicate
	if (FindResourceFile(szPath) != NULL)
		return;

	// Build full path if relative
	CGString sFullPath;
	// Try the path as-is first (it may already be relative to CWD)
	{
		FILE* fTest = fopen(szPath, "r");
		if (fTest)
		{
			fclose(fTest);
			sFullPath = szPath;
		}
		else if (szPath[0] != '/' && m_sSCPBaseDir.GetLength() > 0)
		{
			sFullPath = CGFile::GetMergedFileName(m_sSCPBaseDir, szPath);
		}
		else
		{
			sFullPath = szPath;
		}
	}

	CResourceScript* pNewScript = new CResourceScript;
	ASSERT(pNewScript);
	pNewScript->SetFilePath(sFullPath);
	m_ResourceFiles.Add(pNewScript);

	g_Log.Event(LOG_GROUP_INIT, LOGL_TRACE, "Indexing resource '%s'" LOG_CR, (LPCTSTR)sFullPath);
}

bool CResourceMgr::LoadResources(CResourceScript* pResScript)
{
	if (!pResScript)
		return false;
	if (!pResScript->Open())
		return false;
	LoadResourcesOpen(*pResScript);
	pResScript->Close();
	return true;
}

void CResourceMgr::LoadResourcesOpen(CResourceScript &script)
{
	while (script.FindNextSection())
	{
		CSphereResourceMgr* pMgr = static_cast<CSphereResourceMgr*>(this);
		pMgr->LoadScriptSection(script);
	}
}

bool CResourceMgr::OpenScriptFind(CScript& s, LPCTSTR pszName)
{
	// If no name provided, use the file's existing path
	if (!pszName || !pszName[0])
		pszName = s.GetFilePath();

	if (!pszName || !pszName[0])
		return false;

	// Try opening the file directly
	if (s.Open(pszName))
		return true;

	// Try with the base directory prefix
	if (m_sSCPBaseDir.GetLength() > 0)
	{
		CGString sPath = CGFile::GetMergedFileName(m_sSCPBaseDir, pszName);
		if (s.Open(sPath))
			return true;
	}

	// If pszName is NULL, try opening based on the file's already-set path
	if (!pszName)
	{
		return s.Open();
	}

	return false;
}

CResourceFilePtr CResourceMgr::GetResourceFile(int iIndex)
{
	if (iIndex < 0 || iIndex >= m_ResourceFiles.GetSize())
		return NULL;
	return static_cast<CResourceFile*>(m_ResourceFiles[iIndex]);
}

CResourceDefPtr CResourceMgr::ResourceGetDef(UID_INDEX rid)
{
	int index = m_ResHash.FindKey(rid);
	if (index < 0)
		return NULL;
	return m_ResHash.GetAt(index);
}

void CResourceMgr::AddResourceDir(LPCTSTR pszDirName)
{
#ifndef _WIN32
	// Use opendir/readdir on Linux
	if (!pszDirName || !pszDirName[0])
		return;

	DIR* pDir = opendir(pszDirName);
	if (!pDir)
	{
		g_Log.Event(LOG_GROUP_INIT, LOGL_WARN, "Can't open resource directory '%s'" LOG_CR, pszDirName);
		return;
	}

	struct dirent* pEntry;
	while ((pEntry = readdir(pDir)) != NULL)
	{
		// Check for .scp extension
		LPCTSTR pszExt = CGFile::GetFileNameExt(pEntry->d_name);
		if (!pszExt || _stricmp(pszExt, ".scp"))
			continue;

		CGString sFullPath = CGFile::GetMergedFileName(pszDirName, pEntry->d_name);
		AddResourceFile(sFullPath);
	}
	closedir(pDir);
#endif
}

CResourceFilePtr CResourceMgr::LoadResourcesAdd(LPCTSTR pszNewName)
{
	if (!pszNewName || !pszNewName[0])
		return NULL;

	AddResourceFile(pszNewName);
	CResourceFilePtr pFile = FindResourceFile(pszNewName);
	if (pFile)
	{
		LoadResources(pFile);
	}
	return pFile;
}

void CResourceMgr::DeleteResourceFile(LPCTSTR pszFile)
{
	if (!pszFile || !pszFile[0])
		return;

	LPCTSTR pszTitle = CGFile::GetFileNameTitle(pszFile);
	for (int i = 0; i < m_ResourceFiles.GetSize(); i++)
	{
		CResourceScript* pScript = m_ResourceFiles[i];
		if (!pScript)
			continue;
		LPCTSTR pszExistingTitle = CGFile::GetFileNameTitle(pScript->GetFilePath());
		if (!_stricmp(pszTitle, pszExistingTitle))
		{
			m_ResourceFiles.RemoveAt(i);
			return;
		}
	}
}

//*************************************************************

bool CSphereResourceMgr::LoadIni( bool fTest )
{
	// Load my INI file first.
	// ARGS:
	//  fTest = do not display errors.

	if ( ! OpenScriptFind( m_scpIni, NULL )) // Open script file
	{
#ifdef _WIN32
		if ( fTest )
		{
			// Get the path from the registry?

			// try setting the path to the path of the EXE file ?
			char szPath[_MAX_PATH];
			::GetModuleFileName( NULL, szPath, sizeof(szPath));
			if (szPath[0] == 0)
			{
				return( false );
			}

			CGFile::ExtractPath(szPath);
			_chdir(szPath);

			return( LoadIni(false));
		}
#endif
		g_Log.Event( LOG_GROUP_INIT, LOGL_FATAL, "Can't open %s" LOG_CR, 	(LPCTSTR) m_scpIni.GetFilePath());
		return( false );
	}

	LoadResourcesOpen(m_scpIni);
	m_scpIni.Close();
	return( true );
}

bool CSphereResourceMgr::SaveIni()
{
	// Save out stuff in the INI file.
	// Delete the previous [SPHERE] section.
	m_scpIni.WriteProfileStringSec( GetResourceBlockName(RES_Sphere), NULL, NULL );

	// Now write a new one at the end of the file.
	if ( ! m_scpIni.Open( NULL, OF_WRITE|OF_READWRITE|OF_TEXT )) // Open script file for append write
	{
		return( false );
	}

	g_Serv.s_WriteProps( m_scpIni );

	m_scpIni.Close();

	// Resync this file (in case there are linked things in it.
	SERVMODE_TYPE iModePrv = g_Serv.SetServerMode( SERVMODE_ResyncLoad );
	g_Cfg.LoadResources( &m_scpIni );
	g_Serv.SetServerMode( iModePrv );

	return true;
}

void CSphereResourceMgr::Unload( bool fResync )
{
	if ( fResync )
	{
		// Unlock all the SCP and MUL files.
		g_MulInstall.CloseFiles();
		for ( int j=0;; j++ )
		{
			CResourceFilePtr pResFile = GetResourceFile(j);
			if ( pResFile == NULL )
				break;
			pResFile->OnTick(true);
		}
		m_scpIni.OnTick(true);
		return;
	}

	for ( int _hi = (int)m_ResHash.GetCount() - 1; _hi >= 0; _hi-- )
	{
		CResourceDefPtr pResDef = m_ResHash.GetAt(_hi);
		if ( pResDef )
			pResDef->UnLink();
	}

	// m_LogIP
	m_Obscene.RemoveAll();
	m_NotoTitles.RemoveAll();
	m_Runes.RemoveAll();	// Words of power. (A-Z)
	// m_MultiDefs
	m_SkillNameDefs.RemoveAll();	// Defined Skills
	m_SkillIndexDefs.RemoveAll();
	// m_Servers
	m_HelpDefs.RemoveAll();
	m_StartDefs.RemoveAll(); // Start points list
	// m_StatAdv
	for ( int j=0; j<PLEVEL_QTY; j++ )
	{
		m_PrivCommands[j].RemoveAll();
	}
	m_MoonGates.RemoveAll();
	m_SpellDefs.RemoveAll();	// Defined Spells

	m_WebPages.RemoveAll();

	m_ResHash.Empty();
	m_ResourceFiles.RemoveAll();
	m_Var.RemoveAll();
}

bool CSphereResourceMgr::Load( bool fResync )
{
	// ARGS:
	//  fResync = just look for changes.

#ifndef _WIN32
	// Install crash handler for backtrace on segfault
	struct sigaction sa;
	sa.sa_handler = CrashHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0; // persistent — CrashHandler checks g_fSEGV_catch for recovery
	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGBUS, &sa, NULL);
	sigaction(SIGABRT, &sa, NULL);
#endif

	if ( ! fResync )
	{
		LoadIni(true);

		for ( int i=0; i<STAT_QTY; i++ )
		{
			CResourceDefPtr pRes = new CResourceDef( CSphereUID( RES_Stat, i ), g_Stat_Name[i] );
			ASSERT(pRes);
			m_ResHash.AddSortKey( pRes, CSphereUID( RES_Stat, i ) );
		}

		m_DefaultRaceClass = new CRaceClassDef(RES_RaceClass);
	}
	else
	{
		LoadResources( &m_scpIni );
	}

#ifdef _WIN32
	// Load from the registry.
	// Changed = 1 to mark as changed?
	CGRegKey reg( HKEY_LOCAL_MACHINE );
	LONG lRet = reg.Open( SPHERE_REGKEY, KEY_READ );
	if ( lRet == NO_ERROR )
	{
		g_Serv.s_LoadPropsRegistry( reg, fResync );
	}
#endif

	// Open the MUL files I need.
	VERFILE_TYPE i = g_MulInstall.OpenFiles(
		_1BITMASK(VERFILE_MAP0)|
		_1BITMASK(VERFILE_STAIDX0 )|
		_1BITMASK(VERFILE_STATICS0 )|
		_1BITMASK(VERFILE_TILEDATA )|
		_1BITMASK(VERFILE_MULTIIDX )|
		_1BITMASK(VERFILE_MULTI ),
		0
		);
	if ( i != VERFILE_QTY )
	{
		g_Log.Event( LOG_GROUP_INIT, LOGL_FATAL, "MUL File '%s' not found..." LOG_CR, (LPCTSTR) g_MulInstall.GetBaseFileName(i));
		return( false );
	}

	Debug_CheckPoint();

	// Optional files.
	i = g_MulInstall.OpenFiles(
		_1BITMASK(VERFILE_VERDATA )|	// some install have none.
		_1BITMASK(VERFILE_MAP2)|
		_1BITMASK(VERFILE_STAIDX2)|
		_1BITMASK(VERFILE_STATICS2),

		_1BITMASK(VERFILEX_MAPDIF0)|
		_1BITMASK(VERFILEX_MAPDIFL0)|
		_1BITMASK(VERFILEX_STADIFL0)|
		_1BITMASK(VERFILEX_STADIFI0)|
		_1BITMASK(VERFILEX_STADIF0)|

		_1BITMASK(VERFILEX_MAPDIF1)|
		_1BITMASK(VERFILEX_MAPDIFL1)|
		_1BITMASK(VERFILEX_STADIFL1)|
		_1BITMASK(VERFILEX_STADIFI1)|
		_1BITMASK(VERFILEX_STADIF1)|

		_1BITMASK(VERFILEX_MAPDIF2)|
		_1BITMASK(VERFILEX_MAPDIFL2)|
		_1BITMASK(VERFILEX_STADIFL2)|
		_1BITMASK(VERFILEX_STADIFI2)|
		_1BITMASK(VERFILEX_STADIF2)
		);

	// Load the optional verdata cache. (modifies MUL stuff)
	bool fRet = false;
	try
	{
		fRet = g_MulVerData.Load( g_MulInstall.m_File[VERFILE_VERDATA] );
	}
	SPHERE_LOG_TRY_CATCH( "g_MulVerData.Load" )
	if ( ! fRet )
	{
		return( false );
	}

	Debug_CheckPoint();

	// Now load the *TABLES.SCP file.
	if ( m_ResourceFiles.GetSize() == 0 )
	{
		AddResourceFile( SPHERE_FILE "tables" );
	}

	// open and index all my script files i'm going to use.

	for ( int j=0;; j++ )
	{
		CResourceFilePtr pResFile = GetResourceFile(j);
		if ( pResFile == NULL )
			break;

		// Debug_CheckPoint();
		bool fRet;
		try {
			fRet = LoadResources( pResFile );	// load or resync
		}
		catch (...)
		{
			fRet = false;
		}
		if (!fRet)
		{
			continue;
		}

		if ( j == 0 )
		{
			AddResourceDir( m_sSCPBaseDir );		// if we want to get *.SCP files from elsewhere.
			DeleteResourceFile( m_sWorldStatics );	// don't read this way
			g_Log.Event( LOG_GROUP_INIT, LOGL_TRACE, "Indexing %d scripts..." LOG_CR, m_ResourceFiles.GetSize());
		}
		else
		{
			g_Serv.Event_PrintPercent( SERVTRIG_TestStatus, j+1, m_ResourceFiles.GetSize());
		}
	}
	// Make sure we have the basics.

	Debug_CheckPoint();
	// ASSERT( m_ResourceLinks.TestSort());

	if ( g_Serv.GetName()[0] == '\0' )	// make sure we have a set name
	{
		TCHAR szName[ UO_MAX_SERVER_NAME_SIZE ];
		int iRet = gethostname( szName, sizeof( szName ));
		g_Serv.SetName(( ! iRet && szName[0] ) ? szName : SPHERE_TITLE );
	}

	if ( ! g_Cfg.ResourceGetDef( CSphereUID( RES_Profession, 0 )))
	{
		// must have at least 1 Profession.
		CProfessionPtr pProfession = new CProfessionDef( CSphereUID( RES_Profession ));
		ASSERT(pProfession);
		m_ResHash.AddSortKey( pProfession, CSphereUID( RES_Profession, 0 ) );
	}
	if ( ! m_StartDefs.GetSize())	// must have 1 start location
	{
		CRefPtr<CStartLoc> pStart = new CStartLoc( SPHERE_TITLE );
		ASSERT(pStart);
		pStart->m_sName = "The Throne Room";
		pStart->m_pt = g_pntLBThrone;
		m_StartDefs.Add( pStart );
	}

#if 0
	// Default Region covering all. 
	if ( ! g_World.GetSector(0)->GetRegionCount())
	{
		CRefPtr<CRegionComplex> pRegionAll = new CRegionComplex( CSphereUID(RES_Area,RID_INDEX_MASK-1), GetName());
		if ( pRegionAll->RealizeRegion())
		{
			m_ResHash.AddSortKey( pRegionAll, rid );
		}
	}
#endif

	g_Serv.WriteString( LOG_CR );
	Debug_CheckPoint();
	return true;
}


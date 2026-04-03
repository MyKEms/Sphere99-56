//
// CSector.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.

//////////////////////////////////////////////////////////////////
// -CSector

const CScriptProp CSector::sm_Props[CSector::P_QTY+1] =
{
#define CSECTORPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "csectorprops.tbl"
#undef CSECTORPROP
	NULL,
};

#ifdef USE_JSCRIPT
#define CSECTORMETHOD(a,b,c) JSCRIPT_METHOD_IMP(CSector,a)
#include "csectormethods.tbl"
#undef CSECTORMETHOD
#endif

const CScriptMethod CSector::sm_Methods[CSector::M_QTY+1] =
{
#define CSECTORMETHOD(a,b,c) CSCRIPT_METHOD_IMP(a,b,c)
#include "csectormethods.tbl"
#undef CSECTORMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP1(Sector,CSector::sm_Props,CSector::sm_Methods,NULL,ResourceObj);

CSector::CSector()
{
	IncRefCount();		// static part of another.
	m_ListenItems = 0;
	m_RainChance = 0;		// 0 to 100%
	m_ColdChance = 0;		// Will be snow if rain chance success.
	SetDefaultWeatherChance();
}

CSector::~CSector()
{
	StaticDestruct();
	ASSERT( ! HasClients());
}

HRESULT CSector::s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( HRES_UNKNOWN_PROPERTY );
	}
	switch (iProp)
	{
	case P_ColdChance:
		vVal.SetInt( GetColdChance());
		break;
	case P_Light:
	case P_LocalLight:
		vVal.SetInt( GetLight());
		break;
	case P_RainChance:
		vVal.SetInt( GetRainChance());
		break;
	case P_Season:
		vVal.SetInt( GetSeason());
		break;
	case P_Weather:
		vVal.SetInt( GetWeather());
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CSector::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp<0)
	{
		return(HRES_UNKNOWN_PROPERTY);
	}
	switch (iProp)
	{
	case P_ColdChance:
		SetWeatherChance( false, vVal.IsEmpty() ? -1 : vVal.GetInt());
		break;
	case P_Light:
	case P_LocalLight:
		SetLight( vVal.IsEmpty() ? -1 : vVal.GetInt());
		break;
	case P_RainChance:
		SetWeatherChance( true, vVal.IsEmpty() ? -1 : vVal.GetInt());
		break;
	case P_Season:
		SetSeason( (SEASON_TYPE) ( vVal.IsEmpty()? SEASON_QTY : vVal.GetInt()));
		break;
	case P_Weather:
		SetWeather( (WEATHER_TYPE) ( vVal.IsEmpty()? WEATHER_DEFAULT : vVal.GetInt()));
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CSector::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	ASSERT(pSrc);
	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceObj::s_Method( pszKey, vArgs, vValRet, pSrc ));
	}

	static const CAssocStrVal sm_ComplexityTitles[] =
	{
		{"HIGH", INT_MIN},	// speech can be very complex if low char count
		{"MEDIUM", 5},
		{"LOW", 10},
		{NULL, INT_MAX},
	};

	switch (iProp)
	{
	case M_LocalTime:
		vValRet = GetLocalGameTime();
		break;
	case M_LocalTOD:
		vValRet.SetInt( GetLocalTime());
		break;
	case M_MoonLight:
		vValRet.SetBool( IsMoonVisible(0));
		break;
	case M_MoonPhase:
		vValRet.SetInt( g_World.Moon_GetPhase( vArgs.GetInt()));
		break;
	case M_Complexity:
		// Query complexity
		if ( vArgs.IsEmpty())
		{
			vValRet.SetInt( GetCharComplexity());
		}
		else
		{
			vValRet.SetBool( ! _stricmp( vArgs, sm_ComplexityTitles->FindValSorted( GetCharComplexity())));
		}
		break;
	case M_IsDark:
		vValRet.SetBool( IsDark());
		break;
	case M_IsNightTime:
		{
			int iMinutes = GetLocalTime();
			vValRet.SetBool( iMinutes < 7*60 || iMinutes > (9+12)*60 );
		}
		break;
	case M_AllItems:
		{
			// Send a command to all items in the sector
			CSphereExpContext exec( NULL, pSrc );
			CItemPtr pItem = m_Items_Timer.GetHead();
			for ( ; pItem; pItem=pItem->GetNext())
			{
				exec.SetBaseObject( REF_CAST(CItem,pItem));
				exec.ExecuteCommand( vArgs );
			}
			pItem = m_Items_Inert.GetHead();
			for ( ; pItem; pItem=pItem->GetNext())
			{
				exec.SetBaseObject( REF_CAST(CItem,pItem));
				exec.ExecuteCommand( vArgs );
			}
		}
		break;
	case M_AllChars:
	case M_AllClients:
		{
			// Send a command to all clients in the sector
			CSphereExpContext exec( NULL, pSrc );
			CCharPtr pChar = m_Chars_Active.GetHead();
			for ( ; pChar; pChar=pChar->GetNext())
			{
				if ( iProp == M_AllClients && ! pChar->IsClient())
					continue;
				exec.SetBaseObject( REF_CAST(CChar,pChar));
				exec.ExecuteCommand( vArgs );
			}
		}
		break;
	case M_Respawn:
		if ( toupper( vArgs.GetPSTR()[0] ) == 'A' )
		{
			g_World.RespawnDeadNPCs();
		}
		else
		{
			RespawnDeadNPCs();
		}
		break;
	case M_Restock:
		// set restock time of all vendors in World.
		// set the respawn time of all spawns in World.
		if ( toupper( vArgs.GetPSTR()[0] ) == 'A' )
		{
			g_World.Restock();
		}
		else
		{
			Restock( vArgs.GetInt());
		}
		break;
	case M_Dry:	
		SetWeather( WEATHER_DRY );
		break;
	case M_Rain:
		SetWeather( (vArgs.IsEmpty()||vArgs.GetBool()) ? WEATHER_RAIN : WEATHER_DRY );
		break;
	case M_Snow:
		SetWeather( (vArgs.IsEmpty()||vArgs.GetBool()) ? WEATHER_SNOW : WEATHER_DRY );
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

void CSector::s_WriteStatics( CScript& s )
{
	// ATTR_STATIC Items on the ground.
	CItemPtr pItemNext;
	CItemPtr pItem = m_Items_Inert.GetHead();
	for ( ; pItem; pItem = pItemNext )
	{
		pItemNext = pItem->GetNext();
		if ( ! pItem->IsAttr(ATTR_STATIC))
			continue;
		pItem->s_WriteSafe( s );
	}
	pItem = m_Items_Timer.GetHead();
	for ( ; pItem; pItem = pItemNext )
	{
		pItemNext = pItem->GetNext();
		if ( ! pItem->IsAttr(ATTR_STATIC))
			continue;
		pItem->s_WriteSafe( s );
	}
}

void CSector::s_WriteProps()
{
	if ( m_fSaveParity == g_World.m_fSaveParity )
		return;	// already saved.

	m_fSaveParity = g_World.m_fSaveParity;

	if ( IsLightOverriden() || IsRainOverriden() || IsColdOverriden())
	{
		CPointMap pt = GetBasePoint();
		g_World.m_FileWorld.WriteSection( "SECTOR %d,%d", pt.m_x, pt.m_y );
	}
	if ( IsLightOverriden())
	{
		g_World.m_FileWorld.WriteKeyInt( "LOCALLIGHT", GetLight());
	}
	if ( IsRainOverriden())
	{
		g_World.m_FileWorld.WriteKeyInt( "RAINCHANCE", GetRainChance());
	}
	if ( IsColdOverriden())
	{
		g_World.m_FileWorld.WriteKeyInt( "COLDCHANCE", GetColdChance());
	}

	// Chars in the sector.
	CCharPtr pCharNext;
	CCharPtr pChar = m_Chars_Active.GetHead();
	for ( ; pChar != NULL; pChar = pCharNext )
	{
		pCharNext = pChar->GetNext();
		if ( pChar->m_pPlayer.IsValidNewObj())
		{
			pChar->s_WriteParity( g_World.m_FilePlayers );
		}
		else
		{
			pChar->s_WriteParity( g_World.m_FileWorld );
		}
	}
	// Inactive Client Chars, ridden horses and dead npcs
	// NOTE: ??? Push inactive player chars out to the account files here ?
	pChar = m_Chars_Disconnect.GetHead();
	for ( ; pChar != NULL; pChar = pCharNext )
	{
		pCharNext = pChar->GetNext();
		if ( pChar->m_pPlayer.IsValidNewObj())
		{
			pChar->s_WriteParity( g_World.m_FilePlayers );
		}
		else
		{
			pChar->s_WriteParity( g_World.m_FileWorld );
		}
	}

	// Items on the ground.
	CItemPtr pItemNext;
	CItemPtr pItem = m_Items_Inert.GetHead();
	for ( ; pItem != NULL; pItem = pItemNext )
	{
		pItemNext = pItem->GetNext();
		if ( pItem->IsAttr(ATTR_STATIC))
			continue;
		pItem->s_WriteSafe( g_World.m_FileWorld );
	}
	pItem = m_Items_Timer.GetHead();
	for ( ; pItem != NULL; pItem = pItemNext )
	{
		pItemNext = pItem->GetNext();
		if ( pItem->IsAttr(ATTR_STATIC))
			continue;
		pItem->s_WriteSafe( g_World.m_FileWorld );
	}
}

int CSector::GetLocalTime() const
{
	// The local time of day (in minutes) is based on Global Time and latitude
	//
	CPointMap pt = GetBasePoint();

	// Time difference between adjacent sectors in minutes
	int iSectorTimeDiff = (24*60) / SECTOR_COLS;

	// Calculate the # of columns between here and Castle Britannia ( x = 1400 )
	int iSectorOffset = ( pt.m_x / SECTOR_SIZE_X ) - ( (24*60) / SECTOR_SIZE_X );

	// Calculate the time offset from global time
	int iTimeOffset = iSectorOffset* iSectorTimeDiff;

	// Calculate the local time
	int iLocalTime = ( g_World.GetGameWorldTime() + iTimeOffset ) % (24*60);

	return ( iLocalTime );
}

LPCTSTR CSector::GetLocalGameTime() const
{
	return( GetTimeDescFromMinutes( GetLocalTime()));
}

bool CSector::IsMoonVisible(int iPhase, int iLocalTime) const
{
	// When is moon rise and moon set ?
	iLocalTime %= (24*60);		// just to make sure (day time in minutes) 1440
	switch (iPhase)
	{
	case 0:	// new moon
		return( (iLocalTime > 360) && (iLocalTime < 1080));
	case 1:	// waxing crescent
		return( (iLocalTime > 540) && (iLocalTime < 1270));
	case 2:	// first quarter
		return( iLocalTime > 720 );
	case 3:	// waxing gibbous
		return( (iLocalTime < 180) || (iLocalTime > 900));
	case 4:	// full moon
		return( (iLocalTime < 360) || (iLocalTime > 1080));
	case 5:	// waning gibbous
		return( (iLocalTime < 540) || (iLocalTime > 1270));
	case 6:	// third quarter
		return( iLocalTime < 720 );
	case 7:	// waning crescent
		return( (iLocalTime > 180) && (iLocalTime < 900));
	default: // How'd we get here?
		ASSERT(0);
		return (false);
	}
}

int CSector::IsMoonVisible( int iMoonIndex ) const
{
	int iPhase = g_World.Moon_GetPhase( iMoonIndex );
	if ( IsMoonVisible( iPhase, GetLocalTime()))
	{
		return g_World.Moon_GetBright( iMoonIndex, iPhase );
	}
	return 0;
}

BYTE CSector::GetLightCalc() const
{
	// What is the ambient light level default here in this sector.
	// not effected by point light sources.
	// higher value = darker.

	if ( IsLightOverriden())
		return( m_Env.m_Light );

	if ( IsInDungeonRegion())
		return( g_Cfg.m_iLightDungeon );

	// Get light level based on Time of day.
	// Assume the transition from full light to full dark takes 2 hours.

	int localtime = GetLocalTime();

#define TIME_DAWN	 (6*60)		// sun rise.
#define TIME_DUSK	 ((8+12)*60) // sun set.

	// AM          111		   111 PM
	//    123456789012123456789012
	//		   +-			-+

	int iLight = 0;

	if ( localtime < TIME_DAWN )
		iLight = g_Cfg.m_iLightNight;

	else if ( localtime < TIME_DAWN + 2*60 )
		iLight = g_Cfg.m_iLightDay - IMULDIV( g_Cfg.m_iLightNight - g_Cfg.m_iLightDay, (TIME_DAWN + 2*60) - localtime, 2*60 );

	else if ( localtime < TIME_DUSK - 2*60 )
		iLight = g_Cfg.m_iLightDay;

	else if ( localtime < TIME_DUSK )
		iLight = g_Cfg.m_iLightDay + IMULDIV( g_Cfg.m_iLightDay - g_Cfg.m_iLightNight, TIME_DUSK - localtime, 2*60 );

	else
		iLight = g_Cfg.m_iLightNight;

	// Check for clouds...if it is cloudy, then we don't even need to check for the effects of the moons...
	if ( GetWeather())
	{
		// Clouds of some sort...
		if (iLight < g_Cfg.m_iLightNight )
			iLight += ( Calc_GetRandVal( 2 ) + 1 );	// 1-2 light levels darker if cloudy at night
	}

	// Factor in the effects of the moons
	// Trammel
	int iTrammelPhase = g_World.Moon_GetPhase( MOON_TRAMMEL );
	// Check to see if Trammel is up here...
	if ( IsMoonVisible( iTrammelPhase, localtime ))
	{
		iLight -= g_World.Moon_GetBright( MOON_TRAMMEL, iTrammelPhase );
	}

	// Felucca
	int iFeluccaPhase = g_World.Moon_GetPhase( MOON_FELUCCA );
	if ( IsMoonVisible( iFeluccaPhase, localtime ))
	{
		iLight -= g_World.Moon_GetBright( MOON_FELUCCA, iTrammelPhase );
	}

	if ( iLight < LIGHT_BRIGHT )
		iLight = LIGHT_BRIGHT;
	if ( iLight > LIGHT_DARK ) 
		iLight = LIGHT_DARK;

	return( iLight );
}

void CSector::SetLightNow( bool fFlash )
{
	// Set the light level for all the CClients here.

	CCharPtr pChar = m_Chars_Active.GetHead();
	for ( ; pChar != NULL; pChar = pChar->GetNext())
	{
		if ( pChar->IsStatFlag( STATF_DEAD | STATF_NightSight ))
			continue;

		if ( pChar->IsClient())
		{
			CClientPtr pClient = pChar->GetClient();
			ASSERT(pClient);

			if ( fFlash )	// This does not seem to work predicably !?
			{
				BYTE bPrvLight = m_Env.m_Light;
				m_Env.m_Light = LIGHT_BRIGHT;	// full bright.
				pClient->addLight();
				pClient->xFlush();
				pClient->addPause( false );	// show the new light level.
				m_Env.m_Light = bPrvLight;	// back to previous.
			}
			pClient->addLight();
		}

		if ( ! g_Serv.IsLoading())
		{
			CSphereExpContext se(pChar, pChar);
			pChar->OnTrigger( CCharDef::T_EnvironChange, se);
		}
	}
}

void CSector::SetLight( int light )
{
	// GM set light level command
	// light =LIGHT_BRIGHT, LIGHT_DARK=dark

	// DEBUG_MSG(( "CSector.SetLight(%d)" LOG_CR, light ));

	if ( light < LIGHT_BRIGHT || light > LIGHT_DARK )
	{
		m_Env.m_Light &= ~LIGHT_OVERRIDE;
		m_Env.m_Light = (BYTE) GetLightCalc();
	}
	else
	{
		m_Env.m_Light = (BYTE) ( light | LIGHT_OVERRIDE );
	}
	SetLightNow(false);
}

void CSector::SetDefaultWeatherChance()
{
	// IsInDungeonRegion() ?

	CPointMap pt = GetBasePoint();
	int iPercent = IMULDIV( pt.m_y, 100, UO_SIZE0_Y );	// 100 = south
	if ( iPercent < 50 )
	{
		// Anywhere north of the Britain Moongate is a good candidate for snow
		m_ColdChance = 1 + ( 49 - iPercent ) * 2;
		m_RainChance = 15;
	}
	else
	{
		// warmer down south
		m_ColdChance = 1;
		// rain more likely down south.
		m_RainChance = 15 + ( iPercent - 50 ) / 10;
	}
}

WEATHER_TYPE CSector::GetWeatherCalc() const
{
	// (1 in x) chance of some kind of weather change at any given time
	if ( IsInDungeonRegion() || g_Cfg.m_fNoWeather )
		return( WEATHER_DRY );

	int iRainRoll = Calc_GetRandVal( 100 );
	if ( ( GetRainChance() * 2) < iRainRoll )
		return( WEATHER_DRY );

	// Is it just cloudy?
	if ( iRainRoll > GetRainChance())
		return WEATHER_CLOUDY;

	// It is snowing
	if ( Calc_GetRandVal(100) <= GetColdChance()) // Can it actually snow here?
		return WEATHER_SNOW;

	// It is raining
	return WEATHER_RAIN;
}

void CSector::SetWeather( WEATHER_TYPE w )
{
	// Set the immediate weather type.
	// 0=dry, 1=rain or 2=snow.

	if ( w == m_Env.m_Weather )
		return;

	m_Env.m_Weather = w;

	CCharPtr pChar = m_Chars_Active.GetHead();
	for ( ; pChar != NULL; pChar = pChar->GetNext())
	{
		if ( pChar->IsClient())
		{
			pChar->GetClient()->addWeather( w );
		}
		CSphereExpContext se(pChar, pChar);
		pChar->OnTrigger( CCharDef::T_EnvironChange, se);
	}
}

void CSector::SetSeason( SEASON_TYPE season )
{
	// Set the season type.

	if ( season == m_Env.m_Season )
		return;
	if ( season < 0 || season >= SEASON_QTY )	// default for map?
		season = SEASON_Spring;

	m_Env.m_Season = season;

	CCharPtr pChar = m_Chars_Active.GetHead();
	for ( ; pChar != NULL; pChar = pChar->GetNext())
	{
		if ( pChar->IsClient())
		{
			pChar->GetClient()->addSeason( season );
		}
		CSphereExpContext se(pChar, pChar);
		pChar->OnTrigger( CCharDef::T_EnvironChange, se);
	}
}

void CSector::SetWeatherChance( bool fRain, int iChance )
{
	// Set via the client.
	// Transfer from snow to rain does not work ! must be DRY first.

	if ( iChance > 100 )
		iChance = 100;
	if ( iChance < 0 )
	{
		// just set back to defaults.
		SetDefaultWeatherChance();
	}
	else if ( fRain )
	{
		m_RainChance = iChance | LIGHT_OVERRIDE;
	}
	else
	{
		m_ColdChance = iChance | LIGHT_OVERRIDE;
	}

	// Recalc the weather immediatly.
	if ( ! g_Serv.IsLoading())
	{
		SetWeather( GetWeatherCalc());
	}
}

void CSector::OnHearItem( CChar* pChar, TCHAR* szText )
{
	// report to any of the items that something was said.

	ASSERT(m_ListenItems);

	CItemPtr pItemNext;
	CItemPtr pItem = m_Items_Timer.GetHead();
	for ( ; pItem != NULL; pItem = pItemNext )
	{
		pItemNext = pItem->GetNext();
		pItem->OnHear( szText, pChar );
	}
	pItem = m_Items_Inert.GetHead();
	for ( ; pItem != NULL; pItem = pItemNext )
	{
		pItemNext = pItem->GetNext();
		pItem->OnHear( szText, pChar );
	}
}

void CSector::MoveItemToSector( CItemPtr pItem, bool fActive )
{
	// remove from previous list and put in new.
	// May just be setting a timer. SetTimer or MoveTo()
	ASSERT( pItem );
	if ( fActive )
	{
		m_Items_Timer.AddItemToSector( pItem );
	}
	else
	{
		m_Items_Inert.AddItemToSector( pItem );
	}
}

bool CSector::MoveCharToSector( CCharPtr pChar )
{
	// Move a CChar into this CSector.
	// NOTE:
	//   m_pt is still the old location. Not moved yet!
	// ASSUME: called from CChar.MoveToChar() assume ACtive char.

	if ( IsCharActiveIn( pChar ))
		return false;	// already here

	// Check my save parity vs. this sector's
	if ( pChar->IsStatFlag( STATF_SaveParity ) != m_fSaveParity )
	{
		// DEBUG_MSG(( "Movement based save uid 0%x" LOG_CR, pChar->GetUID()));
		if ( g_World.IsSaving())
		{
			if ( m_fSaveParity == g_World.m_fSaveParity )
			{
				// Save out the CChar now. the sector has already been saved.
				if ( pChar->m_pPlayer.IsValidNewObj())
				{
					pChar->s_WriteParity( g_World.m_FilePlayers );
				}
				else
				{
					pChar->s_WriteParity( g_World.m_FileWorld );
				}
			}
			else
			{
				// We need to write out this CSector now. (before adding client)
				DEBUG_CHECK( pChar->IsStatFlag( STATF_SaveParity ) == g_World.m_fSaveParity );
				s_WriteProps();
			}
		}
		else
		{
			DEBUG_CHECK( g_World.IsSaving());
		}
	}

	if ( pChar->IsClient())
	{
		// Send new weather and light for this sector
		// Only send if different than last ???

		CClientPtr pClient = pChar->GetClient();
		ASSERT( pClient );

		pClient->addSound( SOUND_CLEAR, NULL, 0 );	// clear recurring sounds based on distance.

		if ( ! pChar->IsStatFlag( STATF_DEAD | STATF_NightSight | STATF_Sleeping ))
		{
			pClient->addLight(GetLight());
		}
		// Provide the weather as an arg as we are not in the new location yet.
		pClient->addWeather( GetWeather());
		pClient->addSeason( GetSeason());
	}

	m_Chars_Active.AddCharToSector( pChar );	// remove from previous spot.
	return( true );
}

inline bool CSector::IsSectorSleeping() const
{
	long iAge = GetLastClientTime().GetCacheAge();
	return( iAge > 10*60*TICKS_PER_SEC );
}

void CSector::Close( bool fResources )
{
	// Clear up all dynamic data for this sector.
	CObjBase::sm_fDeleteReal = true;
	m_Items_Timer.DeleteAll();
	m_Items_Inert.DeleteAll();
	m_Chars_Active.DeleteAll();
	m_Chars_Disconnect.DeleteAll();
	CObjBase::sm_fDeleteReal = false;

	if ( fResources )
	{
		// These are resource type things.
		UnloadRegions();
	}
}

void CSector::RespawnDeadNPCs()
{
	// Respawn dead NPC's
	CCharPtr pCharNext;
	CCharPtr pChar = m_Chars_Disconnect.GetHead();
	for ( ; pChar != NULL; pChar = pCharNext )
	{
		pCharNext = pChar->GetNext();
		if ( ! pChar->m_pNPC.IsValidNewObj())
			continue;
		if ( ! pChar->IsStatFlag( STATF_DEAD ))
			continue;
		if ( ! pChar->m_ptHome.IsValidPoint())
			continue;

		DEBUG_CHECK( pChar->IsStatFlag( STATF_RespawnNPC ));

		// Res them back to their "home".
		int iDist = pChar->m_pNPC->m_Home_Dist_Wander;
		pChar->MoveNear( pChar->m_ptHome, ( iDist < SHRT_MAX ) ? iDist : 4 );
		pChar->Spell_Effect_Resurrection(-1,NULL);

		// Restock them with npc stuff.
		pChar->NPC_LoadScript(true);	// NPC_Vendor_Restock
	}
}

void CSector::Restock( long iTime )
{
	// ARGS: iTime = time in seconds
	// set restock time of all vendors in Sector.
	// set the respawn time of all spawns in Sector.

	CCharPtr pCharNext;
	CCharPtr pChar = m_Chars_Active.GetHead();
	for ( ; pChar != NULL; pChar = pCharNext )
	{
		pCharNext = pChar->GetNext();
		pChar->NPC_Vendor_Restock( iTime );
	}
}

void CSector::OnTick( int iPulseCount )
{
	// CWorld gives OnTick() to all CSectors.

	// Check for light change before putting the sector to sleep
	// If we don't, sectors that are asleep will end up with wacked-out
	// light levels due to the gradual light level changes not getting
	// applied fast enough.

	bool fEnvironChange = false;
	bool fLightChange = false;
	bool fPeriodic = ! ( iPulseCount & 0x7f ); // 30 seconds or so.

	if ( fPeriodic )
	{
		// check for local light level change ?
		BYTE blightprv = m_Env.m_Light;
		m_Env.m_Light = GetLightCalc();
		if ( m_Env.m_Light != blightprv )
		{
			fEnvironChange = true;
			fLightChange = true;
		}
	}

	bool fSleeping = false;
	if ( ! HasClients())
	{
		// Put the sector to sleep if no clients been here in a while.
		fSleeping = IsSectorSleeping();
		if ( fSleeping && ! IsMapCacheActive())	// Close down the cache?
		{
			// Stagger sectors a bit. so it is not jerky.
			// 15 seconds or so.
			// Tuned to the CWorld OnTick of SECTOR_TICK_PERIOD !!!
			if ( ! g_Cfg.m_iSectorSleepMask )
				return;
			if (( iPulseCount & g_Cfg.m_iSectorSleepMask ) != ( GetIndex() & g_Cfg.m_iSectorSleepMask ))
				return;

			m_Chars_Active.m_dwMapPlaneClients = 0;
			fPeriodic = true;
		}
	}

	// random weather noises and effects.
	SOUND_TYPE sound = 0;
	bool fWeatherChange = false;
	int iRegionPeriodic = 0;

	if ( fPeriodic )	// 30 seconds or so.
	{
		// Only do this every x minutes or so (TICKS_PER_SEC)
		// check for local weather change ?
		WEATHER_TYPE weatherprv = m_Env.m_Weather;
		if ( ! Calc_GetRandVal( 30 ))	// change less often
		{
			m_Env.m_Weather = GetWeatherCalc();
			if ( weatherprv != m_Env.m_Weather )
			{
				fWeatherChange = true;
				fEnvironChange = true;
			}
		}

		// Random area noises. Only do if clients about.
		if ( HasClients())
		{
			iRegionPeriodic = 2;

			static const SOUND_TYPE sm_SfxRain[] = { 0x10, 0x11 };
			static const SOUND_TYPE sm_SfxWind[] = { 0x14, 0x15, 0x16 };
			static const SOUND_TYPE sm_SfxThunder[] = { 0x28, 0x29 , 0x206 };

			// Lightning ?	// wind, rain,
			switch ( GetWeather())
			{
			case WEATHER_CLOUDY:
				break;

			case WEATHER_SNOW:
				if ( ! Calc_GetRandVal(5))
				{
					sound = sm_SfxWind[ Calc_GetRandVal( COUNTOF( sm_SfxWind )) ];
				}
				break;

			case WEATHER_RAIN:
				{
					int iVal = Calc_GetRandVal(30);
					if ( iVal < 5 )
					{
						// Mess up the light levels for a sec..
						LightFlash();
						sound = sm_SfxThunder[ Calc_GetRandVal( COUNTOF( sm_SfxThunder )) ];
					}
					else if ( iVal < 10 )
					{
						sound = sm_SfxRain[ Calc_GetRandVal( COUNTOF( sm_SfxRain )) ];
					}
					else if ( iVal < 15 )
					{
						sound = sm_SfxWind[ Calc_GetRandVal( COUNTOF( sm_SfxWind )) ];
					}
				}
				break;
			}
		}
	}

	// regen all creatures and do AI

	g_Serv.m_Profile.SwitchTask( PROFILE_Chars );

	CCharPtr pCharNext;
	CCharPtr pChar = m_Chars_Active.GetHead();
	for ( ; pChar != NULL; pChar = pCharNext )
	{
		DEBUG_CHECK( ! pChar->IsWeird());
		DEBUG_CHECK( pChar->IsTopLevel());	// not deleted or off line.
		pCharNext = pChar->GetNext();

		if ( ! fPeriodic && ! m_Chars_Active.IsAwakePlane( pChar->GetTopMap()))
			continue;

		if ( fEnvironChange )	// even npc's react to the environment.
		{
			CSphereExpContext se(pChar, pChar);
			pChar->OnTrigger( CCharDef::T_EnvironChange, se);
		}

		if ( pChar->IsClient())
		{
			CClientPtr pClient = pChar->GetClient();
			ASSERT( pClient );
			if ( sound )
			{
				pClient->addSound( sound, pChar );
			}
			if ( fLightChange && ! pChar->IsStatFlag( STATF_DEAD | STATF_NightSight ))
			{
				pClient->addLight();
			}
			if ( fWeatherChange )
			{
				pClient->addWeather( GetWeather());
			}
			if ( iRegionPeriodic && pChar->m_pArea )
			{
				if ( iRegionPeriodic == 2 )
				{
					pChar->m_pArea->OnRegionTrigger( pChar, CRegionType::T_RegPeriodic );
					iRegionPeriodic--;
				}
				pChar->m_pArea->OnRegionTrigger( pChar, CRegionType::T_CliPeriodic );
			}
		}
		// Can only die on your own tick.
		if ( ! pChar->OnTick())
		{
			pChar->DeleteThis();
		}
	}

	// decay items on ground = time out spells / gates etc.. etc..
	// No need to check these so often !

	g_Serv.m_Profile.SwitchTask( PROFILE_Items );

	CItemPtr pItemNext;
	CItemPtr pItem = m_Items_Timer.GetHead();
	for ( ; pItem != NULL; pItem = pItemNext )
	{
		DEBUG_CHECK( ! pItem->IsWeird());
		DEBUG_CHECK( pItem->IsTimerSet());
		pItemNext = pItem->GetNext();

		if ( ! pItem->IsTimerExpired())
			continue;	// not ready yet.
		if ( ! pItem->OnTick())
		{
			pItem->DeleteThis();
		}
		else
		{
			if ( pItem->IsTimerExpired())	// forgot to clear the timer.? strange.
			{
				pItem->SetTimeout(-1);
			}
		}
	}

	g_Serv.m_Profile.SwitchTask( PROFILE_Overhead );

	if ( fSleeping || fPeriodic )	// 30 seconds or so.
	{
		// delete the static CMulMapBlock items that have not been used recently.
		CheckMapBlockCache( fSleeping ? 0 : g_Cfg.m_iMapCacheTime );
	}
}


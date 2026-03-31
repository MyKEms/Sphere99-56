//
// CWorld.h
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#ifndef _INC_CWORLD_H
#define _INC_CWORLD_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "sphereproto.h"
#include "csectortemplate.h"
#include "common.h"
#include "cObjBase.h"

struct CSectorEnviron	// When these change it is an CCharDef::T_EnvironChange,
{
#define LIGHT_OVERRIDE 0x80
	// Temperature from season and weather ?
public:
	BYTE m_Light;		// the calculated light level in this area. |0x80 = override.
	SEASON_TYPE m_Season;		// What is the season for this sector.
	WEATHER_TYPE m_Weather;		// the weather in this area now.
public:
	CSectorEnviron()
	{
		m_Light = LIGHT_BRIGHT;	// set based on time later.
		m_Season = SEASON_Summer;
		m_Weather = WEATHER_DRY;
	}
	void SetInvalid()
	{
		// Force a resync of all this. we changed location by teleport etc.
		m_Light = -1;	// set based on time later.
		m_Season = SEASON_QTY;
		m_Weather = WEATHER_DEFAULT;
	}
};

class CSector : public CSectorTemplate	// square region of the world.
{
	// A square region of the world. ex: MAP0.MUL Dungeon Sectors are 256 by 256 meters
#define SECTOR_TICK_PERIOD (TICKS_PER_SEC/4) // length of a pulse.

public:
	CSector();
	~CSector();

	void OnTick( int iPulse );

	// Time
	int GetLocalTime() const;
	LPCTSTR GetLocalGameTime() const;

	SEASON_TYPE GetSeason() const
	{
		return m_Env.m_Season;
	}
	void SetSeason( SEASON_TYPE season );

	// Weather
	WEATHER_TYPE GetWeather() const	// current weather.
	{
		return m_Env.m_Weather;
	}
	bool IsRainOverriden() const
	{
		return(( m_RainChance& LIGHT_OVERRIDE ) ? true : false );
	}
	BYTE GetRainChance() const
	{
		return( m_RainChance &~ LIGHT_OVERRIDE );
	}
	bool IsColdOverriden() const
	{
		return(( m_ColdChance& LIGHT_OVERRIDE ) ? true : false );
	}
	BYTE GetColdChance() const
	{
		return( m_ColdChance &~ LIGHT_OVERRIDE );
	}
	void SetWeather( WEATHER_TYPE w );
	void SetWeatherChance( bool fRain, int iChance );

	// Light
	bool IsLightOverriden() const
	{
		return(( m_Env.m_Light& LIGHT_OVERRIDE ) ? true : false );
	}
	BYTE GetLight() const
	{
		return( m_Env.m_Light &~ LIGHT_OVERRIDE );
	}
	bool IsDark() const
	{
		return( GetLight() > 6 );
	}
	void LightFlash()
	{
		SetLightNow( true );
	}
	void SetLight( int light );
	int  IsMoonVisible( int iMoonIndex ) const;

	// Items in the sector

	int GetItemComplexity() const
	{
		return m_Items_Timer.GetCount() + m_Items_Inert.GetCount();
	}
	bool IsItemInSector( const CItem* pItem ) const
	{
		ASSERT(pItem);
		return( const_cast <const CGObList *>( pItem->GetParent()) == &(m_Items_Inert) ||
			const_cast <const CGObList *>( pItem->GetParent()) == &(m_Items_Timer));
	}
	void MoveItemToSector( CItemPtr pItem, bool fActive );

	void AddListenItem()
	{
		m_ListenItems++;
	}
	void RemoveListenItem()
	{
		m_ListenItems--;
	}
	bool HasListenItems() const
	{
		return m_ListenItems ? true : false;
	}
	void OnHearItem( CChar* pChar, TCHAR* szText );

	// Chars in the sector.
	bool IsCharActiveIn( const CChar* pChar ) // const
	{
		// assume the char is active (not disconnected)
		return( pChar->GetParent() == &m_Chars_Active );
	}
	bool IsCharDisconnectedIn( const CChar* pChar ) // const
	{
		// assume the char is active (not disconnected)
		return( pChar->GetParent() == &m_Chars_Disconnect );
	}
	int GetCharComplexity() const
	{
		return( m_Chars_Active.GetCount());
	}
	int HasClients() const
	{
		return( m_Chars_Active.HasClients());
	}
	CServTimeBase GetLastClientTime() const
	{
		return( m_Chars_Active.m_timeLastClient );
	}
	bool IsSectorSleeping() const;
	void SetSectorWakeStatus( int iMapPlane )
	{
		// Ships may enter a sector before it's riders ! ships need working timers to move !
		m_Chars_Active.SetWakeStatus( iMapPlane );
	}
	void ClientAttach( CChar* pChar )
	{
		if ( ! IsCharActiveIn( pChar ))
			return;
		m_Chars_Active.ClientAttach();
		SetSectorWakeStatus( pChar->GetTopMap());
	}
	void ClientDetach( CChar* pChar )
	{
		if ( ! IsCharActiveIn( pChar ))
			return;
		m_Chars_Active.ClientDetach();
	}
	bool MoveCharToSector( CCharPtr pChar );

	// General scripting
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script
	virtual void s_WriteProps();

	// Other resources.
	void Restock( long iTime );
	void RespawnDeadNPCs();
	void s_WriteStatics( CScript& s );

	void Close( bool fResources );
	virtual CGString GetName() const { return( "Sector" ); }

private:
	WEATHER_TYPE GetWeatherCalc() const;
	BYTE GetLightCalc() const;
	void SetLightNow( bool fFlash = false );
	bool IsMoonVisible( int iPhase, int iLocalTime ) const;
	void SetDefaultWeatherChance();

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CSECTORPROP(a,b,c) P_##a,
#include "csectorprops.tbl"
#undef CSECTORPROP
		P_QTY,
	};
	enum M_TYPE_
	{
#define CSECTORMETHOD(a,b,c) M_##a,
#include "csectormethods.tbl"
#undef CSECTORMETHOD
		M_QTY,
	};
	static const CScriptMethod sm_Methods[M_QTY+1];
	static const CScriptProp sm_Props[P_QTY+1];
#ifdef USE_JSCRIPT
#define CSECTORMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "csectormethods.tbl"
#undef CSECTORMETHOD
#endif

private:
	bool   m_fSaveParity;		// has the sector been saved relative to the char entering it ?
	CSectorEnviron m_Env;		// Current Environment

	BYTE m_RainChance;		// 0 to 100%
	BYTE m_ColdChance;		// Will be snow if rain chance success.
	BYTE m_ListenItems;		// Items on the ground that listen ?
};

enum IMPFLAGS_TYPE	// IMPORT and EXPORT flags.
{
	IMPFLAGS_NOTHING = 0,
	IMPFLAGS_ITEMS = 0x01,	// 0x01 = items,
	IMPFLAGS_CHARS = 0x02,  // 0x02 = characters
	IMPFLAGS_BOTH  = 0x03,	// 0x03 = both
	IMPFLAGS_PLAYERS = 0x04,
	IMPFLAGS_RELATIVE = 0x10, // 0x10 = distance relative. dx, dy to you
	IMPFLAGS_ACCOUNT = 0x20,  // 0x20 = recover just this account/char	(and all it is carrying)
};

class CWorldThread : public CUIDArray
{
	// A regional thread of execution. hold all the objects in my region.
	// as well as those just created here. (but may not be here anymore)

public:
	// int m_iThreadNum;	// Thread number to id what range of UID's i own.
	// CRectMap m_ThreadRect;	// the World area that this thread owns.

	CGObListType<CObjBase> m_ObjNew;		// CObjBase created but not yet placed in the world.
	CGObListType<CObjBase> m_ObjDelete;		// CObjBase to be deleted.

	// Background save. Does this belong here ?
	CScript m_FileWorld;		// Save or Load file.
	CScript m_FilePlayers;		// Save of the players chars.
	bool	m_fSaveParity;		// has the sector been saved relative to the char entering it ?

public:

	// Backgound Save
	bool IsSaving() const
	{
		return( m_FileWorld.IsFileOpen() && m_FileWorld.IsModeWrite());
	}

	// UID Managenent
	void SetPreventUIDReuse() { /* STUB */ }
	void SetAllowUIDReuse() { /* STUB */ }

	int FixObjTry( CObjBase* pObj, int iUID = 0 );
	int  FixObj( CObjBase* pObj, int iUID = 0 );

	void GarbageCollection_UIDs();
	void GarbageCollection_New();

	void CloseAllUIDs();

#if 0
	// A uid i allocated has been freed.
	void OnOutsideUIDFree( CSphereUID uid );

	// Transfer these objects to another thread.
	void TransferObjectToThread( CObjBase* pObj, CWorldThread* pNewThread );
#endif

	CWorldThread();
	~CWorldThread();
};

class CGMPage;

extern class CWorld : public CWorldThread
{
	// the world. Stuff saved in *World.SCP
private:
	// Clock stuff. how long have we been running ? all i care about.
	CServTimeMaster m_Clock;		// the current relative tick time  (in TICKS_PER_SEC)

	// Special purpose timers.
	CServTime	m_timeSector;		// next time to do sector stuff.
	CServTime	m_timeSave;		// when to auto save ?
	CServTime	m_timeRespawn;	// when to res dead NPC's ?
	int		m_Sector_Pulse;		// Slow some stuff down that doesn't need constant processing.

	int		m_iSaveStage;	// Current stage of the background save.

	// World data.
	CSector m_Sectors[ SECTOR_QTY ];

public:
	int		m_iSaveCountID;		// Current archival backup id. Whole World must have this same stage id
	int		m_iLoadVersion;		// Previous load version. (only used during load of course)
	CServTime m_timeStartup;	// When did the system restore load/save ?

	CResNameSortArray<CItemStone> m_GuildStones;	// links to guild stones. (not saved array)
	CResNameSortArray<CItemStone> m_TownStones;	// links to town stones. (not saved array)

	CGObListType<CGMPage> m_GMPages;		// Current outstanding GM pages. (CGMPage)
	CGObListType<CPartyDef> m_Parties;	// links to all active parties. CPartyDef

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CWORLDPROP(a,b,c) P_##a,
#include "cworldprops.tbl"
#undef CWORLDPROP
		P_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];

private:
	bool LoadFile( LPCTSTR pszName );
	bool LoadWorld();

	bool SaveTry(bool fForceImmediate); // Save world state
	void GarbageCollection_GMPages();
	bool SaveStage();
	static void GetBackupName( CGString& sArchive, LPCTSTR pszBaseDir, TCHAR chType, int savecount );
	void SaveForce(); // Save world state

public:
	HRESULT SaveWorldStatics();

	CSectorPtr GetSector( int i )
	{
		if ( ((WORD)i) >= SECTOR_QTY )
			return NULL;
		return( &m_Sectors[i] );
	}
	CSectorPtr GetSector( POINT pt )
	{
		return( GetSector( (( pt.y / SECTOR_SIZE_Y )* SECTOR_COLS ) + ( pt.x / SECTOR_SIZE_X ) ));
	}

	// Time

	CServTimeBase GetCurrentTime() const
	{
		return m_Clock;  // Time in TICKS_PER_SEC
	}

#define MOON_TRAMMEL	0
#define MOON_FELUCCA	1
#define MOON_PHASES		8
#define MOON_TRAMMEL_SYNODIC_PERIOD 105 // in game world minutes
#define MOON_FELUCCA_SYNODIC_PERIOD 840 // in game world minutes
#define MOON_TRAMMEL_FULL_BRIGHTNESS 2 // light units LIGHT_BRIGHT
#define MOON_FELUCCA_FULL_BRIGHTNESS 6 // light units LIGHT_BRIGHT

	int Moon_GetPhase( int iMoonIndex = MOON_TRAMMEL ) const;
	int Moon_GetBright( int iMoonIndex, int iPhase ) const;
	CServTime Moon_GetNextNew( int iMoonIndex = MOON_TRAMMEL ) const;

	DWORD GetGameWorldTime( CServTime basetime ) const;
	DWORD GetGameWorldTime() const	// return game world minutes
	{
		return( GetGameWorldTime( GetCurrentTime()));
	}

	// CSector World Map stuff.

	void GetHeightPoint( const CPointMap& pt, CMulMapBlockState& block, const CRegionBasic* pRegion = NULL );
	// bool GetHeightNear( CPointMap& pt, CMulMapBlockState& block, const CRegionBasic* pRegion = NULL );

	const CMulMapBlock* GetMapBlock( const CPointMap& pt )
	{
		return( pt.GetSector()->GetMapBlock(pt));
	}
	const CMulMapMeter* GetMapMeter( const CPointMap& pt ) const // Height of MAP0.MUL at given coordinates
	{
		const CMulMapBlock* pMapBlock = pt.GetSector()->GetMapBlock(pt);
		ASSERT(pMapBlock);
		return( pMapBlock->GetTerrain( SPHEREMAP_BLOCK_OFFSET(pt.m_x), SPHEREMAP_BLOCK_OFFSET(pt.m_y)));
	}

	CPointMap FindItemTypeNearby( const CPointMap& pt, IT_TYPE iType, int iDistance = 0 );
	bool IsItemTypeNear( const CPointMap& pt, IT_TYPE iType, int iDistance = 0 );
	CItemPtr CheckNaturalResource( const CPointMap& pt, IT_TYPE Type, bool fTest = true );

	static bool OpenScriptBackup( CScript& s, LPCTSTR pszBaseDir, LPCTSTR pszBaseName, int savecount );
	void ReSyncLoad();
	void ReSyncUnload();

	virtual void s_WriteProps( CScript& s );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal ) ;

	void OnTick();

	CObjBasePtr ObjFind( CSphereUID uid ) const
	{
		if ( ! uid.IsValidObjUID())
			return( NULL );
		return( PTR_CAST(CObjBase,FindUIDObj( uid& UID_INDEX_MASK )));
	}
	CItemPtr ItemFind( CSphereUID uid ) const
	{
		// Does item still exist or has it been deleted
		// IsItem() may be faster ?
		if ( ! uid.IsItem())
			return( NULL );
		return( PTR_CAST(CItem,FindUIDObj( uid& UID_INDEX_MASK )));
	}
	CCharPtr CharFind( CSphereUID uid ) const
	{
		// Does character still exist
		if ( ! uid.IsChar())
			return( NULL );
		return( PTR_CAST(CChar,FindUIDObj( uid& UID_INDEX_MASK )));
	}

	void GarbageCollection();
	void Restock();
	void RespawnDeadNPCs();

	void Speak( const CObjBaseTemplate* pSrc, LPCTSTR pText, HUE_TYPE wHue = HUE_TEXT_DEF, TALKMODE_TYPE mode = TALKMODE_SAY, FONT_TYPE font = FONT_BOLD );
	void SpeakUNICODE( const CObjBaseTemplate* pSrc, const NCHAR* pText, HUE_TYPE wHue, TALKMODE_TYPE mode, FONT_TYPE font, CLanguageID lang );

	void Broadcast( LPCTSTR pMsg );
	CItemPtr Explode( CChar* pSrc, CPointMap pt, int iDist, int iDamage, WORD wFlags = DAMAGE_GENERAL | DAMAGE_HIT_BLUNT );

	LPCTSTR GetGameTime() const;

	HRESULT Export( LPCTSTR pszFilename, const CChar* pSrc, WORD iModeFlags = IMPFLAGS_ITEMS, int iDist = SHRT_MAX, int dx = 0, int dy = 0 );
	HRESULT Import( LPCTSTR pszFilename, const CChar* pSrc, WORD iModeFlags = IMPFLAGS_ITEMS, int iDist = SHRT_MAX, LPCSTR pszAgs1 = NULL, LPCSTR pszAgs2 = NULL );
	void Save( bool fForceImmediate ); // Save world state
	bool LoadAll( LPCTSTR pszLoadName = NULL );
	void Close( bool fResources );

	DWORD LoadUID( DWORD dwUID, CResourceObj* pObj ) { return AllocUID(pObj, dwUID); }

	virtual CGString GetName() const { return( "World" ); }

	CWorld();
	~CWorld();

} g_World;

class CWorldSearch	// define a search of the world.
{
	// Search for CChars and CItem
	// Should we just make a list???
private:
	// Search params
	const CPointMap m_pt;		// Base point of our search.
	const int m_iDist;			// How far from the point are we interested in
	bool m_fAllShow;			// Include Even inert items.

	// Current search status.
	CObjBasePtr m_pObj;			// The current object of interest.
	CObjBasePtr m_pObjNext;		// In case the object get deleted.
	bool m_fInertToggle;		// We are now doing the inert items

	CSectorPtr m_pSectorBase;	// Don't search the center sector 2 times.
	CSectorPtr m_pSector;		// current Sector
	CRectMap m_rectSector;		// A rectangle containing our sectors we can search.
	int		m_iSectorCur;		// What is the current Sector index in m_rectSector
private:
	bool GetNextSector();
public:
	void SetFilterAllShow( bool fView ) { m_fAllShow = fView; }
	// void SetFilterRegion( const CRegionBasic* pRegion );
	// void SetFilter( LPCTSTR pszFilter );

	// Only use one of these per instance.
	CCharPtr GetNextChar();
	CItemPtr GetNextItem();

	void ReInit();
	CWorldSearch( const CPointMap& pt, int iDist = 0 );
	~CWorldSearch() {}
};

inline CServTimeBase CServTimeBase::GetCurrentTime()	// static
{
	return( g_World.GetCurrentTime());
}

#endif

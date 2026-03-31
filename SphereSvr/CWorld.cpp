//
// CWorld.CPP
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.

static const SOUND_TYPE sm_Sounds_Ghost[] =
{
	SOUND_GHOST_1,
	SOUND_GHOST_2,
	SOUND_GHOST_3,
	SOUND_GHOST_4,
	SOUND_GHOST_5,
};

//////////////////////////////////////////////////////////////////
// -CWorldThread

CWorldThread::CWorldThread()
{
	m_fSaveParity = false;		// has the sector been saved relative to the char entering it ?
}

CWorldThread::~CWorldThread()
{
	CloseAllUIDs();
}

void CWorldThread::CloseAllUIDs()
{
	GarbageCollection_New();
	DeleteAllUIDs();
	DEBUG_CHECK( m_ObjDelete.IsEmpty());
}

int CWorldThread::FixObjTry( CObjBase* pObj, int iUID )
{
	// RETURN: 0 = success.
	if ( iUID )
	{
		if (( pObj->GetUID() & UID_INDEX_MASK ) != iUID )
		{
			// Miss linked in the UID table !!! BAD
			// Hopefully it was just not linked at all. else no way to clean this up ???
			DEBUG_ERR(( "UID 0%x, '%s', Mislinked" LOG_CR, iUID, (LPCTSTR) pObj->GetName()));
			return( 0x7101 );
		}
	}
	return pObj->FixWeirdness();
}

int CWorldThread::FixObj( CObjBase* pObj, int iUID )
{
	// Attempt to fix problems with this item.
	// Ignore any children it may have for now.
	// RETURN: 0 = success.
	//

	int iResultCode = 0xFFFF;	// bad mem ?;
	try
	{
		iResultCode = FixObjTry(pObj,iUID);
	}
	SPHERE_LOG_TRY_CATCH1( "FixObj", iUID )

	if ( ! iResultCode )
		return( 0 );

	try
	{
		iUID = pObj->GetUID();

		// is it a real error ?
		if ( pObj->IsItem())
		{
			CItemPtr pItem = PTR_CAST(CItem,pObj);
			if ( pItem && pItem->IsType(IT_EQ_MEMORY_OBJ))
			{
				// Skip display of message for unlinked memories.
				pObj->DeleteThis();
				return iResultCode;
			}
		}
		DEBUG_ERR(( "UID=0%x, id=%s '%s', Invalid code=0%0x" LOG_CR, iUID, (LPCTSTR) pObj->GetResourceName(), (LPCTSTR) pObj->GetName(), iResultCode ));
		pObj->DeleteThis();
	}
	SPHERE_LOG_TRY_CATCH1( "UID=0%x, Asserted cleanup", iUID )

	return( iResultCode );
}

void CWorldThread::GarbageCollection_New()
{
	// Clean up new objects that are never placed.
	// NOTE: _CrtCheckMemory() is very time expensive ! 
#if defined(_WIN32) && defined(_DEBUG)
	//ASSERT( _CrtCheckMemory());
#endif

	CObjBase::sm_fDeleteReal = true;
	try
	{
		if ( m_ObjNew.GetCount())
		{
			g_Log.Event( LOG_GROUP_DEBUG, LOGL_ERROR, "%d Lost object deleted" LOG_CR, m_ObjNew.GetCount());
			m_ObjNew.DeleteAll();
		}
		m_ObjDelete.DeleteAll();	// clean up our delete list.
	}
	SPHERE_LOG_TRY_CATCH( "GarbageCollection_New" )
	CObjBase::sm_fDeleteReal = false;

#if 0//defined(_WIN32) && defined(_DEBUG)
	ASSERT( _CrtCheckMemory());
#endif
}

void CWorldThread::GarbageCollection_UIDs()
{
	// Go through the m_ppUIDs looking for Objects without links to reality.
	// This can take a while.

	SERVMODE_TYPE iModeCode = g_Serv.m_iModeCode;
	g_Serv.SetServerMode(SERVMODE_Test8);

	GarbageCollection_New();

	int iCount = 0;
	for ( int i=1; i<GetUIDCount(); i++ )
	{
		CObjBasePtr pObj = STATIC_CAST(CObjBase,FindUIDObj(i));
		if ( pObj == NULL )
			continue;	// not used.

		// Look for anomolies and fix them (that might mean delete it.)
		FixObj( pObj, i );

		if (! (iCount & 0x1FF ))
		{
			g_Serv.Event_PrintPercent( SERVTRIG_GarbageStatus, iCount, GetUIDCount());
		}
		iCount ++;
	}

	GarbageCollection_New();

	if ( iCount != CObjBase::sm_iCount )	// All objects must be accounted for.
	{
		g_Log.Event( LOG_GROUP_DEBUG, LOGL_ERROR, "Object memory leak %d!=%d" LOG_CR, iCount, CObjBase::sm_iCount );
	}
	else
	{
		g_Log.Event( LOG_GROUP_DEBUG, LOGL_EVENT, "%d Objects accounted for" LOG_CR, iCount );
	}

	g_Serv.SetServerMode(iModeCode);
}

//////////////////////////////////////////////////////////////////
// -CWorld

const CScriptProp CWorld::sm_Props[CWorld::P_QTY+1] =	// static
{
#define CWORLDPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "cworldprops.tbl"
#undef CWORLDPROP
	NULL,
};

CSCRIPT_CLASS_IMP0(World,CWorld::sm_Props,NULL);

CWorld::CWorld()
{
	m_iSaveCountID = 0;
	m_iSaveStage = 0;
}

CWorld::~CWorld()
{
	Close(true);
}

///////////////////////////////////////////////
// Loading and Saving.

void CWorld::GetBackupName( CGString& sArchive, LPCTSTR pszBaseDir, TCHAR chType, int iSaveCount ) // static
{
	if ( chType == 's' )
	{
		// Archive forever. archive will have date stamp.
		CGTime datetime;
		datetime.InitTimeCurrent();

		sArchive.Format( "%s" SPHERE_FILE "%c%d%02d%02d%s",
			pszBaseDir,
			chType,
			datetime.GetYear(),
			datetime.GetMonth(),
			datetime.GetDay(),
			SCRIPT_EXT );

		return;
	}

	int iCount = iSaveCount;
	int iGroup = 0;
	for ( ; iGroup<g_Cfg.m_iSaveBackupLevels; iGroup++ )
	{
		if ( iCount & 0x7 )
			break;
		iCount >>= 3;
	}
	sArchive.Format( "%s" SPHERE_FILE "b%d%d%c%s",
		pszBaseDir,
		iGroup, iCount&0x07,
		chType,
		SCRIPT_EXT );
}

bool CWorld::OpenScriptBackup( CScript& s, LPCTSTR pszBaseDir, LPCTSTR pszBaseName, int iSaveCount ) // static
{
	ASSERT(pszBaseName);

	CGString sArchive;
	GetBackupName( sArchive, pszBaseDir, pszBaseName[0], iSaveCount );

	// remove possible previous archive of same name
	remove( sArchive );

	// rename previous save to archive name.
	CGString sSaveName;
	sSaveName.Format( "%s" SPHERE_FILE "%s" SCRIPT_EXT, pszBaseDir, pszBaseName );

	if ( rename( sSaveName, sArchive ))
	{
		// May not exist if this is the first time.
		g_Log.Event( LOG_GROUP_SAVE, LOGL_WARN, "Rename %s to '%s' FAILED code %d?" LOG_CR, (LPCTSTR) sSaveName, (const TCHAR*) sArchive, CGFile::GetLastError() );
	}

	if ( ! s.Open( sSaveName, OF_WRITE|OF_TEXT))
	{
		g_Log.Event( LOG_GROUP_SAVE, LOGL_CRIT, "Save '%s' FAILED" LOG_CR, (LPCTSTR) sSaveName );
		return( false );
	}

	return( true );
}

HRESULT CWorld::SaveWorldStatics()
{
	// Save just the world statics out to the statics file.
	//

	// It name not set then set it.
	if ( g_Cfg.m_sWorldStatics.IsEmpty())
	{
		g_Cfg.SetWorldStatics( SPHERE_FILE "Statics" SCRIPT_EXT );
		// Must call SaveINI after this.
		g_Cfg.SaveIni();
	}

	// Now open the file to write  and write out all the sectors.
	CScript s;
	if ( ! s.Open( g_Cfg.m_sWorldStatics, OF_WRITE|OF_TEXT))
	{
		return( HRES_BAD_ARGUMENTS );
	}

	for ( int i=0; i<SECTOR_QTY; i++ )
	{
		m_Sectors[i].s_WriteStatics(s);
	}

	s.WriteSection( "EOF" );
	return NO_ERROR;
}

bool CWorld::SaveStage() // Save world state in stages.
{
	// Do the next stage of the save.
	// RETURN: true = continue;
	//  false = done.

	ASSERT( IsSaving());

	switch ( m_iSaveStage )
	{
	case -1:

		SetPreventUIDReuse();

		if ( ! g_Cfg.m_fSaveGarbageCollect )
		{
			GarbageCollection_New();
			GarbageCollection_GMPages();
		}
		// Save global variables 
		m_FileWorld.WriteSection( "VARNAMES" );
		g_Cfg.m_Var.s_WriteTags( m_FileWorld, "%s" );
		break;

	default:
		ASSERT( m_iSaveStage >= 0 && m_iSaveStage < SECTOR_QTY );
		// NPC Chars in the world sectors and the stuff they are carrying.
		// Sector lighting info.
		m_Sectors[m_iSaveStage].s_WriteProps();
		break;

	case SECTOR_QTY:
		{
			// GM_Pages.
			CGMPagePtr pPage = m_GMPages.GetHead();
			for ( ; pPage!= NULL; pPage = pPage->GetNext())
			{
				pPage->s_WriteProps( m_FilePlayers );
			}
		}
		break;

	case SECTOR_QTY+1:
		// Save all my servers some place.
		if ( ! g_Cfg.m_sMainLogServerDir.IsEmpty())
		{
			CScript s;
			if ( ! OpenScriptBackup( s, g_Cfg.m_sMainLogServerDir, "serv", m_iSaveCountID ))
				break;
			// s_WriteProps( s );
			CThreadLockPtr lock( &(g_Cfg.m_Servers));
			for ( int i=0; true; i++ )
			{
				CServerPtr pServ = g_Cfg.Server_GetDef(i);
				if ( pServ == NULL )
					break;
				// Only those dynamically created.
				pServ->s_WriteCreated( s );
			}
			s.WriteSection( "EOF" );
		}
		break;

	case SECTOR_QTY+2:
		// Now make a backup of the account file.
		g_Accounts.Account_SaveAll();
		break;

	case SECTOR_QTY+3:
		// EOF marker to show we reached the end.
		m_FileWorld.WriteSection( "EOF" );
		m_FilePlayers.WriteSection( "EOF" );

		m_iSaveCountID++;	// Save only counts if we get to the end winout trapping.
		m_timeSave.InitTimeCurrent( g_Cfg.m_iSavePeriod );	// next save time.

		g_Log.Event( LOG_GROUP_SAVE, LOGL_TRACE, "World data saved (%s)." LOG_CR, (LPCTSTR) m_FileWorld.GetFilePath());

		// Now clean up all the held over UIDs
		SetAllowUIDReuse();
		m_FileWorld.Close();
		m_FilePlayers.Close();
		m_iSaveStage = INT_MAX;
		return( false );	// done.
	}

	if ( g_Cfg.m_iSaveBackgroundTime )
	{
		int iNextTime = g_Cfg.m_iSaveBackgroundTime / SECTOR_QTY;
		if ( iNextTime > TICKS_PER_SEC/2 )
			iNextTime = TICKS_PER_SEC/2;	// max out at 30 minutes or so.
		m_timeSave.InitTimeCurrent( iNextTime );
	}
	m_iSaveStage ++;
	return( true );
}

void CWorld::SaveForce() // Save world state
{
	Broadcast( "World save has been initiated." );

	SERVMODE_TYPE iModePrv = g_Serv.SetServerMode( SERVMODE_Saving );	// Forced save freezes the system.

	while ( SaveStage())
	{
		if (! ( m_iSaveStage & 0x1FF ))
		{
			g_Serv.Event_PrintPercent( SERVTRIG_SaveStatus, m_iSaveStage, SECTOR_QTY+3 );
		}
	}

	g_Serv.SetServerMode( iModePrv );			// Game is up and running

	DEBUG_MSG(( "Save Done" LOG_CR ));
}

bool CWorld::SaveTry( bool fForceImmediate ) // Save world state
{
	if ( m_FileWorld.IsFileOpen())
	{
		// Save is already active !
		ASSERT( IsSaving());
		if ( fForceImmediate )	// finish it now !
		{
			SaveForce();
		}
		else if ( g_Cfg.m_iSaveBackgroundTime )
		{
			SaveStage();
		}
		return true;
	}

	// Do the write async from here in the future.
	if ( g_Cfg.m_fSaveGarbageCollect )
	{
		GarbageCollection();
	}

	// Determine the save name based on the time.
	// exponentially degrade the saves over time.
	if ( ! OpenScriptBackup( m_FileWorld, g_Cfg.m_sWorldBaseDir, "world", m_iSaveCountID ))
	{
		return false;
	}
	if ( ! OpenScriptBackup( m_FilePlayers, g_Cfg.m_sWorldBaseDir, "chars", m_iSaveCountID ))
	{
		return false;
	}

	m_fSaveParity = ! m_fSaveParity; // Flip the parity of the save.
	m_iSaveStage = -1;
	m_timeSave.InitTime();

	// Write the file headers.
	s_WriteProps( m_FileWorld );
	s_WriteProps( m_FilePlayers );

	if ( fForceImmediate || ! g_Cfg.m_iSaveBackgroundTime )	// Save now !
	{
		SaveForce();
	}

	return true;
}

void CWorld::Save( bool fForceImmediate ) // Save world state
{
	bool fRet = false;
	try
	{
		fRet = SaveTry(fForceImmediate);
	}
	SPHERE_LOG_TRY_CATCH( "Save FAILED." )

	if ( ! fRet )
	{
		Broadcast( "Save FAILED. " SPHERE_TITLE " is UNSTABLE!" );
		m_FileWorld.Close();	// close if not already closed.
		m_FilePlayers.Close();	// close if not already closed.
		// We should probably shut down the server if the save failed
		// so that we don't destroy all of the good saves we have
		g_Serv.SetExitFlag(SPHEREERR_INTERNAL);
	}
}

/////////////////////////////////////////////////////////////////////

bool CWorld::LoadFile( LPCTSTR pszLoadName ) // Load world from script
{
	fprintf(stderr, "DBG: LoadFile('%s')...\n", pszLoadName ? pszLoadName : "NULL"); fflush(stderr);
	CScript s;
	if ( ! s.Open( pszLoadName ))
	{
		fprintf(stderr, "DBG: LoadFile: can't open '%s'\n", pszLoadName ? pszLoadName : "NULL"); fflush(stderr);
		g_Log.Event( LOG_GROUP_INIT, LOGL_ERROR, "Can't Load %s" LOG_CR, (LPCTSTR) pszLoadName );
		return( false );
	}

	g_Log.Event( LOG_GROUP_INIT, LOGL_TRACE, "Loading %s..." LOG_CR, (LPCTSTR) pszLoadName );

	// Find the size of the file.
	FILE_POS_TYPE lLoadSize = s.GetLength();
	fprintf(stderr, "DBG: LoadFile: opened '%s', size=%ld\n", (LPCTSTR)s.GetFilePath(), (long)lLoadSize); fflush(stderr);
	int iLoadStage = 0;

	CSphereScriptContext ScriptContext( &s );

	// Read the header stuff first.
	fprintf(stderr, "DBG: LoadFile: reading header...\n"); fflush(stderr);
	while ( s.ReadKeyParse())
	{
		fprintf(stderr, "DBG: LoadFile: header key='%s'\n", s.GetKey()); fflush(stderr);
		s_PropSet(s.GetKey(),s.GetArgVar());
	}
	fprintf(stderr, "DBG: LoadFile: header done, reading sections...\n"); fflush(stderr);

	while ( s.FindNextSection())
	{
		if (! ( ++iLoadStage & 0x1FF ))	// don't update too often
		{
			fprintf(stderr, "DBG: LoadFile: section %d, section='%s'\n", iLoadStage, s.GetSection()); fflush(stderr);
			g_Serv.Event_PrintPercent( SERVTRIG_LoadStatus, s.GetPosition(), lLoadSize );
		}

		try
		{
			g_Cfg.LoadScriptSection(s);
		}
		SPHERE_LOG_TRY_CATCH1( "Load Exception line %d " SPHERE_TITLE " is UNSTABLE!", s.GetContext().m_iLineNum )
	}
	fprintf(stderr, "DBG: LoadFile: all sections loaded (%d total)\n", iLoadStage); fflush(stderr);

	if ( s.IsSectionType( "EOF" ))
	{
		// The only valid way to end.
		s.Close();
		fprintf(stderr, "DBG: LoadFile: success (EOF found)\n"); fflush(stderr);
		return( true );
	}

	g_Log.Event( LOG_GROUP_INIT, LOGL_CRIT, "No [EOF] marker. '%s' is corrupt!" LOG_CR, (LPCTSTR) s.GetFilePath());
	return( false );
}

bool CWorld::LoadWorld() // Load world from script
{
	// Auto change to the most recent previous backup !
	// Try to load a backup file instead ?
	// NOTE: WE MUST Sync these files ! CHAR and WORLD !!!

	CGString sWorldName;
	sWorldName.Format( "%s" SPHERE_FILE "world", (LPCTSTR) g_Cfg.m_sWorldBaseDir );
	CGString sCharsName;
	sCharsName.Format( "%s" SPHERE_FILE "chars", (LPCTSTR) g_Cfg.m_sWorldBaseDir );

	fprintf(stderr, "DBG: LoadWorld: world='%s' chars='%s'\n", (LPCTSTR)sWorldName, (LPCTSTR)sCharsName); fflush(stderr);

	int iPrevSaveCount = m_iSaveCountID;
	for(;;)
	{
		fprintf(stderr, "DBG: LoadWorld: trying LoadFile(world)...\n"); fflush(stderr);
		if ( LoadFile( sWorldName ))
		{
			fprintf(stderr, "DBG: LoadWorld: world loaded, m_iLoadVersion=%d\n", (int)m_iLoadVersion); fflush(stderr);
			if ( m_iLoadVersion < 53 )
				return( true );
			fprintf(stderr, "DBG: LoadWorld: trying LoadFile(chars)...\n"); fflush(stderr);
			if ( LoadFile( sCharsName ))
			{
				fprintf(stderr, "DBG: LoadWorld: chars loaded OK\n"); fflush(stderr);
				return( true );
			}
			fprintf(stderr, "DBG: LoadWorld: chars load FAILED\n"); fflush(stderr);
		}
		else
		{
			fprintf(stderr, "DBG: LoadWorld: world load FAILED\n"); fflush(stderr);
		}

		// If we could not open the file at all then it was a bust!
		if ( m_iSaveCountID == iPrevSaveCount )
		{
			break;
		}

		// Erase all the stuff in the failed world/chars load.
		Close(false);

		// Get the name of the previous backups.
		CGString sArchive;
		GetBackupName( sArchive, g_Cfg.m_sWorldBaseDir, 'w', m_iSaveCountID );
		if ( ! sArchive.CompareNoCase( sWorldName ))	// ! same file ? break endless loop.
		{
			break;
		}
		sWorldName = sArchive;

		GetBackupName( sArchive, g_Cfg.m_sWorldBaseDir, 'c', m_iSaveCountID );
		if ( ! sArchive.CompareNoCase( sCharsName ))	// ! same file ? break endless loop.
		{
			break;
		}
		sCharsName = sArchive;
	}

	g_Log.Event( LOG_GROUP_INIT, LOGL_FATAL, "No previous backup available ?" LOG_CR );
	return( false );
}

bool CWorld::LoadAll( LPCTSTR pszLoadName ) // Load world from script
{
	fprintf(stderr, "DBG: CWorld::LoadAll() start\n"); fflush(stderr);
	if ( GetUIDCount())	// we already loaded?
		return( true );

	g_Serv.OnTriggerEvent( SERVTRIG_LoadBegin );
	DEBUG_CHECK( g_Serv.IsLoading());

	// The world has just started.
	m_Clock.InitTime();		// will be loaded from the world file.

	// Load all the accounts.
	fprintf(stderr, "DBG: loading accounts...\n"); fflush(stderr);
	if ( ! g_Accounts.Account_LoadAll( false ))
	{
		fprintf(stderr, "DBG: Account_LoadAll returned false\n"); fflush(stderr);
		return( false );
	}
	fprintf(stderr, "DBG: accounts loaded OK\n"); fflush(stderr);

	// If we are the master list. Then read the list from a sep file.
	if ( ! g_Cfg.m_sMainLogServerDir.IsEmpty())
	{
		// NOTE: Load this last because the timers are relative to the timers in the world file.
		CGString sLoadName;
		sLoadName.Format( "%s" SPHERE_FILE "serv", (LPCTSTR) g_Cfg.m_sMainLogServerDir );
		LoadFile( sLoadName );
	}

	// Try to load the world and chars files .
	fprintf(stderr, "DBG: loading world files (pszLoadName=%s)...\n", pszLoadName ? pszLoadName : "NULL"); fflush(stderr);
	if ( pszLoadName )
	{
		// Command line load this file. g_Cfg.m_sWorldBaseDir
		if ( ! LoadFile( pszLoadName ))
			return( false );
	}
	else
	{
		if ( ! LoadWorld())
		{
			fprintf(stderr, "DBG: LoadWorld() returned false\n"); fflush(stderr);
			return( false );
		}
	}
	fprintf(stderr, "DBG: world files loaded OK\n"); fflush(stderr);

	// Load static items into the world. WORLDSTATICS=
	fprintf(stderr, "DBG: loading statics (m_sWorldStatics='%s')...\n", (LPCTSTR)g_Cfg.m_sWorldStatics); fflush(stderr);
	if ( ! g_Cfg.m_sWorldStatics.IsEmpty())
	{
		if ( ! LoadFile( g_Cfg.m_sWorldStatics ))
		{
			fprintf(stderr, "DBG: statics load failed (non-fatal)\n"); fflush(stderr);
		}
	}
	fprintf(stderr, "DBG: statics done\n"); fflush(stderr);

	m_timeStartup.InitTimeCurrent();
	m_timeSave.InitTimeCurrent( g_Cfg.m_iSavePeriod );	// next save time.

	fprintf(stderr, "DBG: setting sector light levels (SECTOR_QTY=%d)...\n", SECTOR_QTY); fflush(stderr);
	// Set all the sector light levels now that we know the time.
	// This should not look like part of the load. (CCharDef::T_EnvironChange triggers should run)
	for ( int j=0; j<SECTOR_QTY; j++ )
	{
		if ( j % 1000 == 0 ) { fprintf(stderr, "DBG: sector %d/%d\n", j, SECTOR_QTY); fflush(stderr); }
		if ( ! m_Sectors[j].IsLightOverriden())
		{
			m_Sectors[j].SetLight(-1);
		}

		// Is this area too complex ?
		int iCount = m_Sectors[j].GetItemComplexity();
		if ( iCount > g_Cfg.m_iMaxItemComplexity*SECTOR_SIZE_X )
		{
			DEBUG_ERR(( "Warning: %d items at %s, Sector too complex!" LOG_CR, iCount, (LPCTSTR) m_Sectors[j].GetBasePoint().v_Get()));
		}
	}
	fprintf(stderr, "DBG: sector lights done\n"); fflush(stderr);

	fprintf(stderr, "DBG: running GarbageCollection...\n"); fflush(stderr);
	GarbageCollection();
	fprintf(stderr, "DBG: GarbageCollection done\n"); fflush(stderr);

	// Set the current version now.
	const TCHAR* pszVersion = SPHERE_VERSION;
	fprintf(stderr, "DBG: calling Exp_GetComplex for version '%s'...\n", pszVersion); fflush(stderr);
	m_iLoadVersion = Exp_GetComplex( pszVersion );	// Set m_iLoadVersion
	fprintf(stderr, "DBG: calling OnTriggerEvent(SERVTRIG_LoadDone)...\n"); fflush(stderr);
	g_Serv.OnTriggerEvent( SERVTRIG_LoadDone );

	return( true );
}

#if 0

void CWorld::ReSyncUnload()
{
	// for resync.
	// Any CObjects that have direct pointers to resources must be disconnected.

	for ( int k=0; k<SECTOR_QTY; k++ )
	{
		// kill any refs to the regions we are about to unload.
		m_Sectors[k].UnloadRegions();
	}
}

void CWorld::ReSyncLoad()
{
	// After a pause/resync all the items need to resync their m_pDef pointers. (maybe)

	for ( int i=1; i<GetUIDCount(); i++ )
	{
		CObjBasePtr pObj = STATIC_CAST(CObjBase,FindUIDObj(i));
		if ( pObj == NULL )
			continue;	// not used.

		if ( pObj->IsItem())
		{
			CItemPtr pItem = REF_CAST(CItem,pObj);
			ASSERT(pItem);
			pItem->SetBaseID( pItem->GetID()); // re-eval the m_pDef stuff.
		}
		else
		{
			CCharPtr pChar = REF_CAST(CChar,pObj);
			ASSERT(pChar);
			pChar->SetID( pChar->GetID());	// re-eval the m_pDef stuff.
		}
	}

	// If this is a resync then we must put all the on-line chars in regions.
	for ( i=0; i<SECTOR_QTY; i++ )
	{
		CCharPtr pChar = m_Sectors[i].m_Chars.GetHead();
		for ( ; pChar != NULL; pChar = pChar->GetNext())
		{
			pChar->MoveToRegionReTest( REGION_TYPE_MULTI | REGION_TYPE_AREA );
		}
	}
}

#endif

/////////////////////////////////////////////////////////////////

void CWorld::s_WriteProps( CScript& s )
{
	// Write out the safe header.
	s.WriteKey( "TITLE", SPHERE_TITLE " World Script" );
	s.WriteKey( "VERSION", SPHERE_VERSION );
	s.WriteKeyInt( "TIME", GetCurrentTime().GetTimeRaw() );
	s.WriteKeyInt( "SAVECOUNT", m_iSaveCountID );
	s.Flush();	// Force this out to the file now.
}

HRESULT CWorld::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp<0)
	{
		return( HRES_UNKNOWN_PROPERTY );
	}
	switch (iProp)
	{
	case P_LastNew:
	case P_LastNewItem:
		{
			CSphereThread* pTask = CSphereThread::GetCurrentThread();
			vValRet.SetRef( g_World.ItemFind(pTask->m_uidLastNewItem));
		}
		break;
	case P_LastNewChar:
		{
			CSphereThread* pTask = CSphereThread::GetCurrentThread();
			vValRet.SetRef( g_World.CharFind(pTask->m_uidLastNewChar));
		}
		break;
	case P_RegStatus:
		vValRet = g_BackTask.m_sRegisterResult;
		break;
	case P_SaveCount:
		vValRet.SetInt( m_iSaveCountID );
		break;
	case P_Time:	// "TIME" = get time in TICKS_PER_SEC
		vValRet.SetInt( GetCurrentTime().GetTimeRaw() );
		break;
	case P_Title: // 	"TITLE",
		vValRet = SPHERE_TITLE " World Script";
		break;
	case P_Version: // "VERSION"
		vValRet = SPHERE_VERSION;
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CWorld::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp<0)
	{
		return( HRES_UNKNOWN_PROPERTY );
	}
	switch ( iProp )
	{
	case P_SaveCount:
		m_iSaveCountID = vVal.GetInt();
		break;
	case P_Time:
		if ( ! g_Serv.IsLoading() )
		{
			DEBUG_WARN(( "Setting TIME while running is BAD!" LOG_CR ));
		}
		m_Clock.InitTime( vVal.GetInt());
		break;
	case P_Version:
		m_iLoadVersion = vVal.GetInt();
		break;
	case P_Title: // ignore this
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}

void CWorld::RespawnDeadNPCs()
{
	// Respawn dead story NPC's
	SERVMODE_TYPE iModePrv = g_Serv.SetServerMode( SERVMODE_RestockAll );
	for ( int i = 0; i<SECTOR_QTY; i++ )
	{
		m_Sectors[i].RespawnDeadNPCs();
	}
	g_Serv.SetServerMode( iModePrv );
}

void CWorld::Restock()
{
	// Recalc all the base items as well.

	SERVMODE_TYPE iModePrv = g_Serv.SetServerMode( SERVMODE_RestockAll );

	for ( int _hi = 0; _hi < (int)g_Cfg.m_ResHash.GetCount(); _hi++ )
	{
		CResourceDefPtr pResDef = g_Cfg.m_ResHash.GetAt(_hi);
		if ( !pResDef )
			continue;
		if ( RES_GET_TYPE(pResDef->GetUIDIndex()) != RES_ItemDef )
			continue;
		CItemDefPtr pBase = REF_CAST(CItemDef,pResDef);
		if ( pBase )
		{
			pBase->Restock();
		}
	}

	for ( int k = 0; k<SECTOR_QTY; k++ )
	{
		m_Sectors[k].Restock(0);
	}

	g_Serv.SetServerMode( iModePrv );
}

void CWorld::Close( bool fResources )
{
	if ( IsSaving())	// Must complete save now !
	{
		Save( true );
	}
	m_GuildStones.RemoveAll();
	m_TownStones.RemoveAll();
	m_Parties.DeleteAll();
	m_GMPages.DeleteAll();

	for ( int i = 0; i<SECTOR_QTY; i++ )
	{
		m_Sectors[i].Close(fResources);
	}

	CloseAllUIDs();

	m_Clock.InitTime();	// no more sense of time.
}

void CWorld::GarbageCollection_GMPages()
{
	// Make sure all GM pages have accounts.
	CGMPagePtr pPage = m_GMPages.GetHead();
	while ( pPage!= NULL )
	{
		CGMPagePtr pPageNext = static_cast<CGMPage*>(pPage->GetNext());
		if ( ! pPage->GetAccount()) // Open script file
		{
			DEBUG_ERR(( "GM Page has invalid account '%s'" LOG_CR, (LPCTSTR) pPage->GetName()));
			pPage->RemoveSelf();
			delete (CGMPage*)pPage;
		}
		pPage = pPageNext;
	}
}

void CWorld::GarbageCollection()
{
	g_Log.Flush();
	GarbageCollection_GMPages();
	GarbageCollection_UIDs();
	g_Log.Flush();
}

void CWorld::Speak( const CObjBaseTemplate* pSrc, LPCTSTR pszText, HUE_TYPE wHue, TALKMODE_TYPE mode, FONT_TYPE font )
{
	// ISINTRESOURCE might be SPKTAB_TYPE ?
	ASSERT(pszText);

	// if ( ISINTRESOURCE(pszText))

	CCharPtr pCharSrc;
	bool fSpeakAsGhost = false;	// I am a ghost ?
	if ( pSrc )
	{
		if ( pSrc->IsChar())
		{
			// Are they dead ? Garble the text. unless we have SpiritSpeak
			pCharSrc = PTR_CAST(CChar,const_cast<CObjBaseTemplate*>(pSrc));
			ASSERT(pCharSrc);
			fSpeakAsGhost = pCharSrc->IsSpeakAsGhost();
		}
	}
	else
	{
		mode = TALKMODE_BROADCAST;
	}

	CGString sTextUID;
	CGString sTextName;	// name labelled text.
	CGString sTextGhost; // ghost speak.

	for ( CClientPtr pClient = g_Serv.GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
	{
		if ( ! pClient->CanHear( pSrc, mode ))
			continue;

		LPCTSTR pszSpeak = pszText;
		bool fCanSee = false;
		CCharPtr pChar = pClient->GetChar();
		if ( pChar != NULL )
		{
			if ( fSpeakAsGhost && ! pChar->CanUnderstandGhost())
			{
				if ( sTextGhost.IsEmpty())	// Garble ghost.
				{
					sTextGhost = pszText;
					for ( int i=0; i<sTextGhost.GetLength(); i++ )
					{
						if ( sTextGhost[i] != ' ' &&  sTextGhost[i] != '\t' )
						{
							sTextGhost.SetAt( i, Calc_GetRandVal(2) ? 'O' : 'o' );
						}
					}
				}
				pszSpeak = sTextGhost;
				pClient->addSound( sm_Sounds_Ghost[ Calc_GetRandVal( COUNTOF( sm_Sounds_Ghost )) ], pSrc );
			}
			fCanSee = pChar->CanSee( pSrc );	// Must label the text.
			if ( ! fCanSee && pSrc )
			{
				if ( sTextName.IsEmpty())
				{
					if ( pCharSrc && ! pChar->CanDisturb(pCharSrc))
						sTextName.Format( "<System>%s", (LPCTSTR) pszText );
					else
						sTextName.Format( "<%s>%s", (LPCTSTR) pSrc->GetName(), (LPCTSTR) pszText );
				}
				pszSpeak = sTextName;
			}
		}

		if ( ! fCanSee && pSrc && pClient->IsPrivFlag( PRIV_HEARALL|PRIV_DEBUG ))
		{
			if ( sTextUID.IsEmpty())
			{
				if ( pCharSrc && ! pChar->CanDisturb(pCharSrc))
					sTextUID.Format( "<System [%lx]>%s", pSrc->GetUID(), (LPCTSTR) pszText );
				else
					sTextUID.Format( "<%s [%lx]>%s", (LPCTSTR) pSrc->GetName(), pSrc->GetUID(), (LPCTSTR) pszText );
			}
			pszSpeak = sTextUID;
		}

		pClient->addBark( pszSpeak, pSrc, wHue, mode, font );
	}
}

void CWorld::SpeakUNICODE( const CObjBaseTemplate* pSrc, const NCHAR* pwText, HUE_TYPE wHue, TALKMODE_TYPE mode, FONT_TYPE font, CLanguageID lang )
{
	ASSERT(pwText);

	CCharPtr pCharSrc;
	bool fSpeakAsGhost = false;	// I am a ghost ?
	if ( pSrc != NULL )
	{
		if ( pSrc->IsChar())
		{
			// Are they dead ? Garble the text. unless we have SpiritSpeak
			pCharSrc = PTR_CAST(CChar,const_cast<CObjBaseTemplate*>( pSrc ));
			ASSERT(pCharSrc);
			fSpeakAsGhost = pCharSrc->IsSpeakAsGhost();
		}
	}
	else
	{
		mode = TALKMODE_BROADCAST;
	}

	NCHAR wTextUID[MAX_TALK_BUFFER];	// uid labelled text.
	wTextUID[0] = '\0';
	NCHAR wTextName[MAX_TALK_BUFFER];	// name labelled text.
	wTextName[0] = '\0';
	NCHAR wTextGhost[MAX_TALK_BUFFER]; // ghost speak.
	wTextGhost[0] = '\0';

	for ( CClientPtr pClient = g_Serv.GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
	{
		if ( ! pClient->CanHear( pSrc, mode ))
			continue;

		const NCHAR* pwSpeak = pwText;
		bool fCanSee = false;
		CCharPtr pChar = pClient->GetChar();
		if ( pChar != NULL )
		{
			if ( fSpeakAsGhost && ! pChar->CanUnderstandGhost())
			{
				if ( wTextGhost[0] == '\0' )	// Garble ghost.
				{
					int i;
					for ( i=0; pwText[i] && i < MAX_TALK_BUFFER; i++ )
					{
						if ( pwText[i] != ' ' && pwText[i] != '\t' )
							wTextGhost[i] = Calc_GetRandVal(2) ? 'O' : 'o';
						else
							wTextGhost[i] = pwText[i];
					}
					wTextGhost[i] = '\0';
				}
				pwSpeak = wTextGhost;
				pClient->addSound( sm_Sounds_Ghost[ Calc_GetRandVal( COUNTOF( sm_Sounds_Ghost )) ], pSrc );
			}

			fCanSee = pChar->CanSee( pSrc );	// Must label the text.
			if ( ! fCanSee && pSrc )
			{
				if ( wTextName[0] == '\0' )
				{
					CGString sTextName;
					if ( pCharSrc && ! pChar->CanDisturb(pCharSrc))
						sTextName = _TEXT("<System>");
					else
						sTextName.Format( _TEXT("<%s>"), (LPCTSTR) pSrc->GetName());
					int iLen = CvtSystemToNUNICODE( wTextName, COUNTOF(wTextName), sTextName );
					for ( int i=0; pwText[i] && iLen < MAX_TALK_BUFFER; i++, iLen++ )
					{
						wTextName[iLen] = pwText[i];
					}
					wTextName[iLen] = '\0';
				}
				pwSpeak = wTextName;
			}
		}

		if ( ! fCanSee && pSrc && pClient->IsPrivFlag( PRIV_HEARALL|PRIV_DEBUG ))
		{
			if ( wTextUID[0] == '\0' )
			{
				CGString sTextName;
				if ( pCharSrc && ! pChar->CanDisturb(pCharSrc))
					sTextName = _TEXT("<System>");
				else
					sTextName.Format( _TEXT("<%s [%lx]>"), (LPCTSTR) pSrc->GetName(), pSrc->GetUID());
				int iLen = CvtSystemToNUNICODE( wTextUID, COUNTOF(wTextUID), sTextName );
				for ( int i=0; pwText[i] && iLen < MAX_TALK_BUFFER; i++, iLen++ )
				{
					wTextUID[iLen] = pwText[i];
				}
				wTextUID[iLen] = '\0';
			}
			pwSpeak = wTextUID;
		}

		pClient->addBarkUNICODE( pwSpeak, pSrc, wHue, mode, font, lang );
	}
}

void CWorld::Broadcast( LPCTSTR pMsg ) // System broadcast in bold text
{
	Speak( NULL, pMsg, HUE_TEXT_DEF, TALKMODE_BROADCAST, FONT_BOLD );
	g_Serv.SocketsFlush();
}

CItemPtr CWorld::Explode( CChar* pSrc, CPointMap pt, int iDist, int iDamage, WORD wFlags )
{
	// Purple potions and dragons fire.
	// degrade damage the farther away we are. ???

	CItemPtr pItem = CItem::CreateBase( ITEMID_FX_EXPLODE_3 );
	ASSERT(pItem);

	pItem->SetAttr(ATTR_MOVE_NEVER | ATTR_CAN_DECAY);
	pItem->SetType(IT_EXPLOSION);
	pItem->m_uidLink = pSrc ? (DWORD) pSrc->GetUID() : UID_INDEX_CLEAR;
	pItem->m_itExplode.m_iDamage = iDamage;
	pItem->m_itExplode.m_wFlags = wFlags | DAMAGE_GENERAL | DAMAGE_HIT_BLUNT;
	pItem->m_itExplode.m_iDist = iDist;
	pItem->MoveToDecay( pt, 1 );	// almost Immediate Decay

	pItem->Sound( 0x207 );	// sound is attached to the object so put the sound before the explosion.

	return( pItem );
}

//////////////////////////////////////////////////////////////////
// Game time.

DWORD CWorld::GetGameWorldTime( CServTime basetime ) const
{
	// basetime = TICKS_PER_SEC time.
	// Get the time of the day in GameWorld minutes
	// 8 real world seconds = 1 game minute.
	// 1 real minute = 7.5 game minutes
	// 3.2 hours = 1 game day.
	return( g_Cfg.m_iGameTimeOffset + ( basetime.GetTimeRaw() / g_Cfg.m_iGameMinuteLength ));
}

CServTime CWorld::Moon_GetNextNew( int iMoonIndex ) const
{
	// "Predict" the next new moon for this moon
	// Get the period
	DWORD iSynodic = iMoonIndex ? MOON_FELUCCA_SYNODIC_PERIOD : MOON_TRAMMEL_SYNODIC_PERIOD;

	// Add a "month" to the current game time
	DWORD iNextMonth = GetGameWorldTime() + iSynodic;

	// Get the game time when this cycle will start
	DWORD iNewStart = (DWORD) (iNextMonth -
		(double) (iNextMonth % iSynodic));

	// Convert to TICKS_PER_SEC ticks
	CServTime time;
	time.InitTime( iNewStart* g_Cfg.m_iGameMinuteLength );
	return(time);
}

int CWorld::Moon_GetPhase( int iMoonIndex ) const
{
	// bMoonIndex is FALSE if we are looking for the phase of Trammel,
	// TRUE if we are looking for the phase of Felucca.

	// There are 8 distinct moon phases:  New, Waxing Crescent, First Quarter, Waxing Gibbous,
	// Full, Waning Gibbous, Third Quarter, and Waning Crescent

	// To calculate the phase, we use the following formula:
	//				CurrentTime % SynodicPeriod
	//	Phase = 	-----------------------------------------    * 8
	//			              SynodicPeriod
	//

	DWORD dwCurrentTime = GetGameWorldTime();	// game world time in minutes

	if ( iMoonIndex == MOON_TRAMMEL )
	{
		return( IMULDIV( dwCurrentTime % MOON_TRAMMEL_SYNODIC_PERIOD, MOON_PHASES, MOON_TRAMMEL_SYNODIC_PERIOD ));
	}
	else
	{
		return( IMULDIV( dwCurrentTime % MOON_FELUCCA_SYNODIC_PERIOD, MOON_PHASES, MOON_FELUCCA_SYNODIC_PERIOD ));
	}
}

int CWorld::Moon_GetBright( int iMoonIndex, int iPhase ) const
{
	// If the moon can be seen, what is it's brightness.

	static const BYTE sm_PhaseBrightness[MOON_PHASES] =
	{
		0, // New Moon
		1, // Crescent Moon
		2, // Quarter Moon
		3, // Gibbous Moon
		4, // Full Moon
		3, // Gibbous Moon
		2, // Quarter Moon
		1, // Crescent Moon
	};

	int iFullBright;
	if ( iMoonIndex == MOON_TRAMMEL )
		iFullBright = MOON_TRAMMEL_FULL_BRIGHTNESS;
	else if ( iMoonIndex == MOON_FELUCCA )
		iFullBright = MOON_FELUCCA_FULL_BRIGHTNESS;
	else 
		return 0;

	return IMULDIV( iFullBright, sm_PhaseBrightness[ iPhase % MOON_PHASES ], 4 );
}

LPCTSTR GetTimeDescFromMinutes( int minutes )
{
	// Get Time of day from minutes.
	if ( minutes < 0 )
	{
		DEBUG_CHECK(0);
		return( "?" );
	}
	int minute = minutes % 60;
	int hour = ( minutes / 60 ) % 24;

	LPCTSTR pMinDif;
	if (minute<=14) pMinDif = "";
	else if ((minute>=15)&&(minute<=30))
		pMinDif = " a quarter past";
	else if ((minute>=30)&&(minute<=45))
		pMinDif = " half past";
	else
	{
		pMinDif = " a quarter till";
		hour = ( hour + 1 ) % 24;
	}

	static LPCTSTR const sm_ClockHour[] =
	{
		"midnight",
		"one",
		"two",
		"three",
		"four",
		"five",
		"six",
		"seven",
		"eight",
		"nine",
		"ten",
		"eleven",
		"noon",
	};

	LPCTSTR pTail;
	if ( hour == 0 || hour==12 )
		pTail = "";
	else if ( hour > 12 )
	{
		hour -= 12;
		if ((hour>=1)&&(hour<6))
			pTail = " o'clock in the afternoon";
		else if ((hour>=6)&&(hour<9))
			pTail = " o'clock in the evening.";
		else
			pTail = " o'clock at night";
	}
	else
	{
		pTail = " o'clock in the morning";
	}

	TCHAR* pTime = Str_GetTemp();
	sprintf( pTime, "%s %s%s.", pMinDif, sm_ClockHour[hour], pTail );
	return( pTime );
}

LPCTSTR CWorld::GetGameTime() const
{
	return( GetTimeDescFromMinutes( GetGameWorldTime()));
}

void CWorld::OnTick()
{
	// Do this once per tick.
	// 256 real secs = 1 SPHEREhour.

	if ( g_Serv.IsLoading())
		return;

	// Set the game time from the real world clock.
#ifdef _DEBUG
	long lTimePrev = m_Clock.GetTimeRaw();
#endif
	if ( ! m_Clock.AdvanceTime())
		return;

#ifdef _DEBUG
	ASSERT( lTimePrev != m_Clock.GetTimeRaw());
#endif
	g_Serv.m_Profile.SwitchTask( PROFILE_Overhead );	// PROFILE_Overhead

	if ( m_timeSector <= GetCurrentTime())
	{
		// Only need a SECTOR_TICK_PERIOD tick to do world stuff.
		m_timeSector.InitTimeCurrent( SECTOR_TICK_PERIOD );	// Next hit time.
		m_Sector_Pulse ++;

		for ( int i=0; i<SECTOR_QTY; i++ )
		{
			m_Sectors[i].OnTick( m_Sector_Pulse );
		}

		g_Serv.m_Profile.SwitchTask( PROFILE_Debug );
		GarbageCollection_New();	// clean up our delete list.
		g_Serv.m_Profile.SwitchTask( PROFILE_Overhead );
	}
	if ( m_timeSave <= GetCurrentTime())
	{
		// Auto save world
		m_timeSave.InitTimeCurrent( g_Cfg.m_iSavePeriod );
		g_Log.Flush();
		Save( false );
	}
	if ( m_timeRespawn <= GetCurrentTime())
	{
		// Time to regen all the dead NPC's in the world.
		m_timeRespawn.InitTimeCurrent( 20*60*TICKS_PER_SEC );
		RespawnDeadNPCs();
	}
}


// Disposable regression test for the world-load save guard.

#include "SphereSvr/stdafx.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <dirent.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

static int CountDirectoryEntries( const char* pszDir )
{
	DIR* pDir = opendir( pszDir );
	if ( pDir == NULL )
		return -1;

	int iCount = 0;
	struct dirent* pEntry;
	while ( ( pEntry = readdir( pDir )) != NULL )
	{
		if ( !std::strcmp( pEntry->d_name, "." ) || !std::strcmp( pEntry->d_name, ".." ))
			continue;
		iCount++;
	}
	closedir( pDir );
	return iCount;
}

static bool TestUIDReset()
{
	CUIDArray uids;
	CResourceObj first( 1 );
	CResourceObj second( 2 );

	if ( uids.AllocUID( &first, 0 ) != 1 || uids.FindUIDObj( 0 ) != NULL )
		return false;

	uids.DeleteAllUIDs();
	if ( uids.GetUIDCount() != 1 || uids.FindUIDObj( 0 ) != NULL )
		return false;

	return uids.AllocUID( &second, 0 ) == 1;
}

static bool TestLoadDetailBudgetScope()
{
	// Consume one category while the server is loading.  The ninth detail is
	// suppressed by the bounded load budget, but the same category must be
	// visible again once the server is running.
	g_Serv.SetServerMode( SERVMODE_Loading );
	for ( int i = 0; i < 8; ++i )
	{
		if ( !g_World.ShouldLogLoadDetail( LOAD_LOG_WORLDCHAR_FAILURE ))
			return false;
	}
	if ( g_World.ShouldLogLoadDetail( LOAD_LOG_WORLDCHAR_FAILURE ))
		return false;

	g_Serv.SetServerMode( SERVMODE_Run );
	const bool fRuntimeVisible =
		g_World.ShouldLogLoadDetail( LOAD_LOG_WORLDCHAR_FAILURE ) &&
		g_World.ShouldLogLoadDetail( LOAD_LOG_WORLDCHAR_FAILURE );
	g_Serv.SetServerMode( SERVMODE_Loading );
	return fRuntimeVisible;
}

static bool TestBackupFallbackRequiresOptIn()
{
	char szTempDir[] = "/tmp/sphere-save-fallback-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;
	const std::string sBaseDir = std::string( szTempDir ) + "/";
	const std::string sWorld = sBaseDir + "sphereworld.scp";
	const std::string sChars = sBaseDir + "spherechars.scp";
	const std::string sWorldBackup = sBaseDir + "sphereb02w.scp";
	const std::string sCharsBackup = sBaseDir + "sphereb02c.scp";
	{
		std::ofstream currentWorld( sWorld.c_str());
		currentWorld << "TITLE=Broken current save\nVERSION=0.99\nSAVECOUNT=2\n";
		std::ofstream currentChars( sChars.c_str());
		currentChars << "TITLE=Current chars\nVERSION=0.99\nSAVECOUNT=2\n[EOF]\n";
		std::ofstream backupWorld( sWorldBackup.c_str());
		backupWorld << "TITLE=Backup world\nVERSION=0.99\nSAVECOUNT=1\n[EOF]\n";
		std::ofstream backupChars( sCharsBackup.c_str());
		backupChars << "TITLE=Backup chars\nVERSION=0.99\nSAVECOUNT=1\n[EOF]\n";
		if ( !currentWorld || !currentChars || !backupWorld || !backupChars )
			return false;
	}

	g_Cfg.m_sWorldBaseDir = sBaseDir.c_str();
	g_Cfg.m_fSaveBackupFallback = false;
	g_World.m_iSaveCountID = 0;
	const bool fImplicitFallback = g_World.LoadWorldForTest();
	if ( fImplicitFallback )
	{
		std::fprintf( stderr, "corrupt current save was silently replaced by a backup\n" );
		return false;
	}

	g_Cfg.m_fSaveBackupFallback = true;
	g_World.m_iSaveCountID = 0;
	const bool fExplicitFallback = g_World.LoadWorldForTest();
	g_World.Close( false );
	g_Cfg.m_fSaveBackupFallback = false;
	g_World.m_iSaveCountID = 0;
	unlink( sWorld.c_str());
	unlink( sChars.c_str());
	unlink( sWorldBackup.c_str());
	unlink( sCharsBackup.c_str());
	rmdir( szTempDir );
	return fExplicitFallback;
}

static bool TestMismatchedPairRecovery()
{
	char szTempDir[] = "/tmp/sphere-save-pair-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;
	const std::string sBaseDir = std::string( szTempDir ) + "/";
	const std::string sWorld = sBaseDir + "sphereworld.scp";
	const std::string sChars = sBaseDir + "spherechars.scp";
	const std::string sWorldBackup = sBaseDir + "sphereb02w.scp";
	const std::string sCharsBackup = sBaseDir + "sphereb02c.scp";
	const std::string sManifest = sBaseDir + "sphere.save.pending";

	const char* pszWorldTwo = "TITLE=Current world\nVERSION=0.99\nSAVECOUNT=2\n[EOF]\n";
	const char* pszCharsOne = "TITLE=Current chars\nVERSION=0.99\nSAVECOUNT=1\n[EOF]\n";
	const char* pszWorldOne = "TITLE=Archived world\nVERSION=0.99\nSAVECOUNT=1\n[EOF]\n";
	const char* pszCharsOneArchive = "TITLE=Archived chars\nVERSION=0.99\nSAVECOUNT=1\n[EOF]\n";
	{
		std::ofstream world( sWorld.c_str());
		std::ofstream chars( sChars.c_str());
		world << pszWorldTwo;
		chars << pszCharsOne;
		if ( !world || !chars )
			return false;
	}

	g_Cfg.m_sWorldBaseDir = sBaseDir.c_str();
	g_Cfg.m_fSaveBackupFallback = false;
	g_World.m_iSaveCountID = 0;
	const bool fMismatchAccepted = g_World.LoadWorldForTest();
	g_World.Close( false );
	if ( fMismatchAccepted )
	{
		std::fprintf( stderr, "mismatched current world/chars pair was accepted\n" );
		unlink( sWorld.c_str());
		unlink( sChars.c_str());
		rmdir( szTempDir );
		return false;
	}

	{
		std::ofstream worldBackup( sWorldBackup.c_str());
		std::ofstream charsBackup( sCharsBackup.c_str());
		std::ofstream manifest( sManifest.c_str());
		worldBackup << pszWorldOne;
		charsBackup << pszCharsOneArchive;
		manifest << "SAVECOUNT=2\nSTATE=PENDING\nROTATED=3\n[EOF]\n";
		if ( !worldBackup || !charsBackup || !manifest )
			return false;
	}
	g_World.m_iSaveCountID = 0;
	const bool fRecovered = g_World.LoadWorldForTest() && g_World.m_iSaveCountID == 2;
	g_World.Close( false );
	unlink( sWorld.c_str());
	unlink( sChars.c_str());
	unlink( sWorldBackup.c_str());
	unlink( sCharsBackup.c_str());
	unlink( sManifest.c_str());
	rmdir( szTempDir );
	g_World.m_iSaveCountID = 0;
	return fRecovered;
}

int main()
{
	if ( !TestUIDReset() )
	{
		std::fprintf( stderr, "UID reset did not preserve the reserved slot 0\n" );
		return 1;
	}
	std::printf( "UID reset: reserved slot 0 preserved\n" );
	if ( !TestLoadDetailBudgetScope() )
	{
		std::fprintf( stderr, "load detail budget leaked into runtime diagnostics\n" );
		return 1;
	}
	std::printf( "load detail budget: bounded during load and visible at runtime\n" );
	if ( !TestBackupFallbackRequiresOptIn() )
	{
		std::fprintf( stderr, "backup fallback was not restricted to the explicit option\n" );
		return 1;
	}
	std::printf( "backup fallback: corrupt current save is fatal unless explicitly enabled\n" );
	if ( !TestMismatchedPairRecovery() )
	{
		std::fprintf( stderr, "mismatched save pair was not rejected and recovered only from a pending pair\n" );
		return 1;
	}
	std::printf( "paired save load: mismatched current pair rejected; pending archives recover atomically\n" );

	char szTempDir[] = "/tmp/sphere-load-safety-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
	{
		std::fprintf( stderr, "could not create disposable test directory\n" );
		return 1;
	}

	const std::string sWorldPath = std::string( szTempDir ) + "/broken-world.scp";
	const std::string sWorldBaseDir = std::string( szTempDir ) + "/";
	const ITEMID_TYPE testItemID = ITEMID_GOLD_C1;
	if ( !g_Cfg.FindItemDef( testItemID ))
	{
		CItemDef* pTestItemDef = new CItemDef( testItemID );
		if ( g_Cfg.m_ResHash.AddSortKey( pTestItemDef,
			CSphereUID( RES_ItemDef, testItemID )) < 0 )
		{
			std::fprintf( stderr, "could not register the synthetic item definition\n" );
			return 1;
		}
	}
	{
		std::ofstream world( sWorldPath.c_str() );
		world << "TITLE=Sphere Test World\n"
			"VERSION=0.99\n"
			"SAVECOUNT=0\n"
			"[WORLDITEM]\n"
			"NAME=deliberately-broken-object\n";
		world << "[WORLDITEM " << testItemID << "]\n"
			"SERIAL=42\n"
			"P=128,128,0\n"
			"[WORLDITEM " << testItemID << "]\n"
			"SERIAL=43\n"
			"P=129,128,0\n"
			"[WORLDITEM " << testItemID << "]\n"
			"SERIAL=44\n"
			"P=130,128,0\n"
			"[EOF]\n";
		if ( !world )
		{
			std::fprintf( stderr, "could not write disposable world fixture\n" );
			unlink( sWorldPath.c_str() );
			rmdir( szTempDir );
			return 1;
		}
	}

	g_Cfg.m_sWorldBaseDir = sWorldBaseDir.c_str();
	const int iSaveCountBefore = g_World.m_iSaveCountID;
	CItemPtr pRuntimeOrphan = CItem::CreateBase( testItemID );
	if ( pRuntimeOrphan == NULL )
	{
		std::fprintf( stderr, "could not create disposable orphan for cleanup coverage\n" );
		unlink( sWorldPath.c_str() );
		rmdir( szTempDir );
		return 1;
	}
	const bool fLoaded = g_World.LoadFileForTest( sWorldPath.c_str() );
	if ( !fLoaded || !g_World.IsSaveBlockedByLoad() ||
		g_World.GetLoadSkippedSections() != 3 ||
		g_World.GetLoadSkippedObjects() != 3 ||
		g_World.GetLoadFailedParses() != 3 ||
		g_World.GetLoadAccepted() != 1 ||
		g_World.GetLoadToleratedLegacy() != 0 ||
		g_World.GetLoadRejected() != 3 ||
		g_World.GetLoadDefaulted() != 0 ||
		g_World.GetLoadDeleted() != 0 )
	{
		std::fprintf( stderr,
			"load guard did not record the failed object sections: loaded=%d sections=%d objects=%d parses=%d accepted=%d tolerated=%d rejected=%d defaulted=%d deleted=%d blocked=%d\n",
			fLoaded ? 1 : 0,
			g_World.GetLoadSkippedSections(),
			g_World.GetLoadSkippedObjects(),
			g_World.GetLoadFailedParses(),
			g_World.GetLoadAccepted(),
			g_World.GetLoadToleratedLegacy(),
			g_World.GetLoadRejected(),
			g_World.GetLoadDefaulted(),
			g_World.GetLoadDeleted(),
			g_World.IsSaveBlockedByLoad() ? 1 : 0 );
		unlink( sWorldPath.c_str() );
		rmdir( szTempDir );
		return 1;
	}
	if ( g_World.ItemFind( CSphereUID( UID_F_ITEM | 42 )) != NULL ||
		g_World.ItemFind( CSphereUID( UID_F_ITEM | 43 )) != NULL ||
		g_World.ItemFind( CSphereUID( UID_F_ITEM | 44 )) == NULL ||
		g_World.m_ObjNew.GetCount() != 0 )
	{
		std::fprintf( stderr,
			"failed or orphaned items remained addressable after cleanup\n" );
		unlink( sWorldPath.c_str() );
		rmdir( szTempDir );
		return 1;
	}
	CGString sLoadCounts;
	g_World.FormatLoadCounts( sLoadCounts );
	std::string sLoadCountsText = (LPCTSTR) sLoadCounts;
	if ( sLoadCountsText.find(
		"created_items=1 created_chars=0 read_items=4 read_chars=0" ) == std::string::npos ||
		sLoadCountsText.find( "allocated_items=4 allocated_chars=0" ) == std::string::npos )
	{
		std::fprintf( stderr, "load summary did not separate created and read sections: %s\n",
			sLoadCountsText.c_str());
		unlink( sWorldPath.c_str() );
		rmdir( szTempDir );
		return 1;
	}
	g_World.GarbageCollection_New();
	if ( g_Serv.StatGet( SERV_STAT_ITEMS ) != 1 )
	{
		std::fprintf( stderr, "failed items remained allocated after world garbage collection\n" );
		unlink( sWorldPath.c_str() );
		rmdir( szTempDir );
		return 1;
	}

	// This must return before opening or renaming any world file, including
	// when an internal caller has no console/source object to notify.
	CGVariant vArgs;
	CGVariant vValRet;
	const HRESULT hSave = g_Serv.s_Method( "SAVE", vArgs, vValRet, NULL );
	const int iRemainingEntries = CountDirectoryEntries( szTempDir );
	const bool fSafe = hSave != NO_ERROR &&
		g_World.m_iSaveCountID == iSaveCountBefore && iRemainingEntries == 1;

	unlink( sWorldPath.c_str() );
	rmdir( szTempDir );
	if ( !fSafe )
	{
		std::fprintf( stderr,
			"NULL-source SAVE was not safely refused: hresult=%ld savecount=%d entries=%d\n",
			(long) hSave, g_World.m_iSaveCountID, iRemainingEntries );
		return 1;
	}

	std::printf( "load safety: failed and orphaned objects cleaned, counts separated, and NULL-source SAVE refused\n" );
	return 0;
}

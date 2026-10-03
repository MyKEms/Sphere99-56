// Disposable regression test for the world-load save guard.

#include "SphereSvr/stdafx.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

#include <cstdio>
#include <cstring>
#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

class CLoadSafetyExecContext : public CSphereExpContext
{
public:
	CLoadSafetyExecContext() : CSphereExpContext( NULL, NULL ) {}
	using CSphereExpContext::ValidateUIDReference;
};

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

static bool TestGetItemDataReadFailureIsInvalid()
{
	char szTileData[] = "/tmp/sphere-tiledata-truncated-XXXXXX";
	const int iTempFD = mkstemp( szTileData );
	if ( iTempFD < 0 )
		return false;
	const ITEMID_TYPE testItemID = static_cast<ITEMID_TYPE>( 0x3FFF );
	const off_t iReadOffset = static_cast<off_t>( UOTILE_TERRAIN_SIZE ) + 4 +
		static_cast<off_t>( testItemID / UOTILE_BLOCK_QTY ) * 4 +
		static_cast<off_t>( testItemID ) * sizeof(CUOItemTypeRec);
	const unsigned char marker = 0x7F;
	const bool fSized = ftruncate( iTempFD, iReadOffset + 1 ) == 0 &&
		lseek( iTempFD, iReadOffset, SEEK_SET ) == iReadOffset &&
		write( iTempFD, &marker, sizeof(marker)) == sizeof(marker);
	close( iTempFD );
	if ( !fSized )
	{
		unlink( szTileData );
		return false;
	}

	g_MulInstall.m_File[VERFILE_TILEDATA].Close();
	const bool fOpened = g_MulInstall.m_File[VERFILE_TILEDATA].Open(
		szTileData, OF_READ | OF_SHARE_DENY_NONE );
	CUOItemTypeRec data;
	memset( &data, 0xA5, sizeof(data));
	const bool fValid = fOpened && CItemDef::GetItemData( testItemID, &data );
	const unsigned char* pBytes = reinterpret_cast<const unsigned char*>( &data );
	bool fZeroed = true;
	for ( size_t i = 0; i < sizeof(data); ++i )
	{
		if ( pBytes[i] != 0 )
		{
			fZeroed = false;
			break;
		}
	}
	g_MulInstall.m_File[VERFILE_TILEDATA].Close();
	unlink( szTileData );
	return fOpened && !fValid && fZeroed;
}

static bool TestUIDReuseIsQuarantined()
{
	CUIDArray uids;
	CResourceObj first( 1 );
	CResourceObj second( 2 );
	CResourceObj third( 3 );
	const DWORD dwFirst = uids.AllocUID( &first, 0 );
	const DWORD dwFirstGeneration = first.GetUIDGeneration();
	if ( dwFirst == 0 || dwFirstGeneration == 0 )
		return false;
	CGVariant savedReference;
	savedReference.SetRef( &first );
	if ( savedReference.GetUIDGeneration() != dwFirstGeneration )
		return false;
	uids.FreeUID( &first );
	// A zero delay still observes the completed-save barrier.
	uids.SetUIDReuseDelaySeconds( 0 );
	const DWORD dwSecond = uids.AllocUID( &second, 0 );
	// A freed slot must not be reused before its quarantine expires.
	if ( dwSecond == dwFirst )
		return false;
	// A completed save and an explicitly configured zero-second delay release
	// the slot; its generation must advance before it can be reused.
	uids.SetAllowUIDReuse();
	const DWORD dwThird = uids.AllocUID( &third, 0 );
	return dwThird == dwFirst && third.GetUIDGeneration() != dwFirstGeneration &&
		savedReference.GetUIDGeneration() != third.GetUIDGeneration();
}

static bool TestUIDQuarantineReleaseScales()
{
	// A large deferred-destruction batch must not scan every prior quarantine
	// entry.  Use explicit slots so this isolates release bookkeeping from the
	// normal first-free UID search.
	const size_t iObjectCount = 50000;
	CUIDArray uids;
	std::vector<CResourceObj*> objects;
	objects.reserve( iObjectCount );
	for ( size_t i = 0; i < iObjectCount; ++i )
	{
		CResourceObj* pObject = new CResourceObj( static_cast<HASH_INDEX>( i + 1 ));
		if ( uids.AllocUID( pObject, static_cast<DWORD>( i + 1 )) != i + 1 )
		{
			delete pObject;
			for ( size_t j = 0; j < objects.size(); ++j )
				delete objects[j];
			return false;
		}
		objects.push_back( pObject );
	}

	const std::chrono::steady_clock::time_point timeStart = std::chrono::steady_clock::now();
	for ( size_t i = 0; i < objects.size(); ++i )
		uids.FreeUID( objects[i] );
	const std::chrono::steady_clock::duration timeElapsed =
		std::chrono::steady_clock::now() - timeStart;
	for ( size_t i = 0; i < objects.size(); ++i )
		delete objects[i];

	const long long iElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>( timeElapsed ).count();
	std::printf( "UID quarantine release: %zu objects in %lld ms\n", iObjectCount, iElapsedMs );
	return iElapsedMs < 5000;
}

static bool TestStaleUIDReferenceIsRejected()
{
	CResourceObj oldObject( 1 );
	CResourceObj newObject( 2 );
	const DWORD dwOldUID = g_World.AllocUID( &oldObject, 0 );
	if ( dwOldUID == 0 )
		return false;
	CGVariant savedReference;
	savedReference.SetRef( &oldObject );
	g_World.FreeUID( &oldObject );
	g_World.SetUIDReuseDelaySeconds( 0 );
	g_World.SetAllowUIDReuse();
	if ( g_World.AllocUID( &newObject, dwOldUID ) != dwOldUID )
		return false;
	newObject.SetUIDIndex( dwOldUID );
	CLoadSafetyExecContext context;
	const bool fRejected = !context.ValidateUIDReference( savedReference, &newObject, "TAG.TEST" );
	g_World.FreeUID( &newObject );
	g_World.SetAllowUIDReuse();
	return fRejected;
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

static bool TestForwardResourceAlias()
{
	// DEFNAME blocks are allowed to refer to a TYPEDEF declared later in the
	// script tree.  The parser stores that forward reference as a string until
	// the target exists; resource-list lookup must resolve it at use time.
	const char* pszAlias = "LOAD_SAFETY_FORWARD_RESOURCE";
	const char* pszTarget = "LOAD_SAFETY_RESOURCE_TARGET";
	const CSphereUID targetRID( RES_TypeDef, 10862 );
	UID_INDEX targetValue = targetRID;
	g_Cfg.m_Const.SetKeyVar( pszAlias, CGVariant( pszTarget ));
	g_Cfg.m_Const.SetKeyVar( pszTarget, CGVariant( VARTYPE_UID, &targetValue ));
	LPCTSTR pszExpr = pszAlias;
	CResourceQty resource;
	const bool fLoaded = resource.LoadResQty( pszExpr );
	const CSphereUID resolved = resource.GetResourceID();
	g_Cfg.m_Const.RemoveKey( pszAlias );
	g_Cfg.m_Const.RemoveKey( pszTarget );
	if ( !fLoaded || resolved != targetRID || resource.GetResQty() != 1 )
	{
		std::fprintf( stderr, "forward resource alias did not resolve to its target\n" );
		return false;
	}
	return true;
}


// Concatenate the daily log files written into pszDir, then remove them.
static std::string TakeDailyLogs( const std::string& sDir )
{
	std::string sContents;
	DIR* pDir = opendir( sDir.c_str());
	if ( pDir == NULL )
		return sContents;
	struct dirent* pEntry;
	while ( ( pEntry = readdir( pDir )) != NULL )
	{
		const std::string sName = pEntry->d_name;
		if ( sName.size() < 4 || sName.compare( sName.size() - 4, 4, ".log" ) != 0 )
			continue;
		const std::string sPath = sDir + "/" + sName;
		std::ifstream file( sPath.c_str());
		sContents.append( std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>());
		unlink( sPath.c_str());
	}
	closedir( pDir );
	return sContents;
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
	// Give the backups a known save time so the fallback report can be checked.
	const time_t tBackupSaved = 1000000000;
	struct utimbuf backupTimes;
	backupTimes.actime = tBackupSaved;
	backupTimes.modtime = tBackupSaved;
	if ( utime( sWorldBackup.c_str(), &backupTimes ) != 0 ||
		utime( sCharsBackup.c_str(), &backupTimes ) != 0 )
		return false;
	const std::string sBackupSaved = CGTime( tBackupSaved ).Format( NULL );

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
	const std::string sLogDir = sBaseDir + "logs";
	const bool fLogOpened = mkdir( sLogDir.c_str(), 0700 ) == 0 && g_Log.OpenLog( sLogDir.c_str());
	const bool fExplicitFallback = g_World.LoadWorldForTest();
	g_Log.Close();
	const std::string sLog = TakeDailyLogs( sLogDir );
	g_World.Close( false );
	g_Cfg.m_fSaveBackupFallback = false;
	g_World.m_iSaveCountID = 0;
	unlink( sWorld.c_str());
	unlink( sChars.c_str());
	unlink( sWorldBackup.c_str());
	unlink( sCharsBackup.c_str());
	rmdir( sLogDir.c_str());
	rmdir( szTempDir );

	// The fallback names each selected backup with its own SAVECOUNT and save time.
	const bool fWorldReported = sLog.find( "Loading save backup '" + sWorldBackup +
		"' SaveCount=1 saved=" + sBackupSaved + " time=" ) != std::string::npos;
	const bool fCharsReported = sLog.find( "Loading save backup '" + sCharsBackup +
		"' SaveCount=1 saved=" + sBackupSaved + " time=" ) != std::string::npos;
	if ( !fLogOpened || !fWorldReported || !fCharsReported )
	{
		std::fprintf( stderr,
			"backup fallback report: log=%d world=%d chars=%d\n%s\n",
			fLogOpened ? 1 : 0, fWorldReported ? 1 : 0, fCharsReported ? 1 : 0, sLog.c_str());
		return false;
	}
	return fExplicitFallback;
}

static bool WriteSavePair( const std::string& sWorld, const char* pszWorld,
	const std::string& sChars, const char* pszChars )
{
	std::ofstream world( sWorld.c_str(), std::ios::out | std::ios::trunc );
	std::ofstream chars( sChars.c_str(), std::ios::out | std::ios::trunc );
	world << pszWorld;
	chars << pszChars;
	return static_cast<bool>( world ) && static_cast<bool>( chars );
}

static bool LoadPairForTest()
{
	g_Cfg.m_fSaveBackupFallback = false;
	g_World.m_iSaveCountID = 0;
	const bool fLoaded = g_World.LoadWorldForTest();
	g_World.Close( false );
	return fLoaded;
}

// Every save the server writes carries the same SAVECOUNT header in both files,
// so only a pair in which neither file carries one (legacy [EOF]-only
// placeholders) is accepted without a count.  A pair in which only one file
// carries a count cannot be proven to be one generation and is rejected.
static bool TestLegacyPairRequiresBothUncounted()
{
	char szTempDir[] = "/tmp/sphere-save-legacy-pair-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;
	const std::string sBaseDir = std::string( szTempDir ) + "/";
	const std::string sWorld = sBaseDir + "sphereworld.scp";
	const std::string sChars = sBaseDir + "spherechars.scp";
	const char* pszLegacy = "[EOF]\n";
	const char* pszCounted = "TITLE=Counted save\nVERSION=0.99\nSAVECOUNT=0\n[EOF]\n";
	g_Cfg.m_sWorldBaseDir = sBaseDir.c_str();

	const bool fLegacyAccepted = WriteSavePair( sWorld, pszLegacy, sChars, pszLegacy ) &&
		LoadPairForTest();
	const bool fCountedAccepted = WriteSavePair( sWorld, pszCounted, sChars, pszCounted ) &&
		LoadPairForTest();
	const bool fCountedCharsRejected = WriteSavePair( sWorld, pszLegacy, sChars, pszCounted ) &&
		!LoadPairForTest();
	const bool fCountedWorldRejected = WriteSavePair( sWorld, pszCounted, sChars, pszLegacy ) &&
		!LoadPairForTest();

	unlink( sWorld.c_str());
	unlink( sChars.c_str());
	rmdir( szTempDir );
	g_World.m_iSaveCountID = 0;
	if ( !fLegacyAccepted || !fCountedAccepted || !fCountedCharsRejected || !fCountedWorldRejected )
	{
		std::fprintf( stderr,
			"save pair header rule: legacy_accepted=%d counted_accepted=%d "
			"uncounted_world_rejected=%d uncounted_chars_rejected=%d\n",
			fLegacyAccepted ? 1 : 0, fCountedAccepted ? 1 : 0,
			fCountedCharsRejected ? 1 : 0, fCountedWorldRejected ? 1 : 0 );
		return false;
	}
	return true;
}

// The pair check reads SAVECOUNT only from a file's header, matching the key
// case-insensitively.  The header is the key block before the first section;
// older writers put the same keys, as "SaveCount=", into a leading [SPHERE]
// section.  A later section, such as a global variable named SAVECOUNT in
// [VARNAMES], must never be taken for the header count.
static bool TestSaveCountHeaderOnly()
{
	char szTempDir[] = "/tmp/sphere-save-header-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;
	const std::string sBaseDir = std::string( szTempDir ) + "/";
	const std::string sWorld = sBaseDir + "sphereworld.scp";
	const std::string sChars = sBaseDir + "spherechars.scp";
	g_Cfg.m_sWorldBaseDir = sBaseDir.c_str();

	const char* pszMixedThree = "TITLE=Mixed case\nVERSION=0.99\nSaveCount=3\n[EOF]\n";
	const char* pszMixedFour = "TITLE=Mixed case\nVERSION=0.99\nSaveCount=4\n[EOF]\n";
	const char* pszSphereThree = "[SPHERE]\nSaveCount=3\n[EOF]\n";
	const char* pszSphereFour = "[SPHERE]\nSaveCount=4\n[EOF]\n";
	const char* pszLegacy = "[EOF]\n";
	const char* pszCountedTwo = "TITLE=Counted\nVERSION=0.99\nSAVECOUNT=2\n[EOF]\n";
	const char* pszCountedTwoWithVar =
		"TITLE=Counted\nVERSION=0.99\nSAVECOUNT=2\n[VARNAMES]\nSAVECOUNT=7\n[EOF]\n";
	const char* pszLegacyWithVar = "[VARNAMES]\nSAVECOUNT=7\n[EOF]\n";

	const bool fMixedMatchAccepted = WriteSavePair( sWorld, pszMixedThree, sChars, pszMixedThree ) &&
		LoadPairForTest();
	const bool fMixedMismatchRejected = WriteSavePair( sWorld, pszMixedThree, sChars, pszMixedFour ) &&
		!LoadPairForTest();
	const bool fMixedHalfRejected = WriteSavePair( sWorld, pszMixedThree, sChars, pszLegacy ) &&
		!LoadPairForTest();
	const bool fSphereMatchAccepted = WriteSavePair( sWorld, pszSphereThree, sChars, pszSphereThree ) &&
		LoadPairForTest();
	const bool fSphereMismatchRejected = WriteSavePair( sWorld, pszSphereThree, sChars, pszSphereFour ) &&
		!LoadPairForTest();
	const bool fVarIgnoredCounted = WriteSavePair( sWorld, pszCountedTwoWithVar, sChars, pszCountedTwo ) &&
		LoadPairForTest();
	const bool fVarIgnoredLegacy = WriteSavePair( sWorld, pszLegacyWithVar, sChars, pszLegacy ) &&
		LoadPairForTest();
	g_Cfg.m_Var.RemoveKey( "SAVECOUNT" );

	unlink( sWorld.c_str());
	unlink( sChars.c_str());
	rmdir( szTempDir );
	g_World.m_iSaveCountID = 0;
	if ( !fMixedMatchAccepted || !fMixedMismatchRejected || !fMixedHalfRejected ||
		!fSphereMatchAccepted || !fSphereMismatchRejected ||
		!fVarIgnoredCounted || !fVarIgnoredLegacy )
	{
		std::fprintf( stderr,
			"save pair header count: mixed_match=%d mixed_mismatch_rejected=%d mixed_half_rejected=%d "
			"sphere_match=%d sphere_mismatch_rejected=%d var_ignored_counted=%d var_ignored_legacy=%d\n",
			fMixedMatchAccepted ? 1 : 0, fMixedMismatchRejected ? 1 : 0, fMixedHalfRejected ? 1 : 0,
			fSphereMatchAccepted ? 1 : 0, fSphereMismatchRejected ? 1 : 0,
			fVarIgnoredCounted ? 1 : 0, fVarIgnoredLegacy ? 1 : 0 );
		return false;
	}
	return true;
}

// A TYPEDEF is a resource index, not a closed member of IT_TYPE.  Saved items
// may therefore carry a script-defined value such as 10861.  Keep that value
// usable while still routing malformed (negative/unknown) TYPE= values
// through the normal-item fallback.
static bool TestCustomTypeSavedItem()
{
	ITEMID_TYPE testItemID = ITEMID_NOTHING;
	for ( int i = static_cast<int>( ITEMID_MULTI_MAX ); i > 0; --i )
	{
		if ( !g_Cfg.FindItemDef( static_cast<ITEMID_TYPE>( i )))
		{
			testItemID = static_cast<ITEMID_TYPE>( i );
			break;
		}
	}
	if ( testItemID == ITEMID_NOTHING )
	{
		std::fprintf( stderr, "could not find a synthetic ITEMDEF id\n" );
		return false;
	}

	const int customType = 10861;
	const char* pszTypeName = "LOAD_SAFETY_CUSTOM_TYPE";
	CSphereUID typeRID( RES_TypeDef, customType );
	CResourceDefPtr pTypeDef = g_Cfg.ResourceGetDef( typeRID );
	if ( !pTypeDef )
	{
		pTypeDef = new CItemTypeDef( typeRID );
		if ( g_Cfg.m_ResHash.AddSortKey( pTypeDef, typeRID ) < 0 )
		{
			std::fprintf( stderr, "could not register the synthetic TYPEDEF\n" );
			return false;
		}
	}
	g_Cfg.m_Const.SetKeyVar( pszTypeName, CGVariant( VARTYPE_UID, &typeRID ));

	CItemDefPtr pItemDef = new CItemDef( testItemID );
	if ( g_Cfg.m_ResHash.AddSortKey( pItemDef, CSphereUID( RES_ItemDef, testItemID )) < 0 )
	{
		std::fprintf( stderr, "could not register the synthetic ITEMDEF\n" );
		return false;
	}
	CGVariant typeName;
	typeName.SetStr( pszTypeName );
	if ( pItemDef->s_PropSet( "TYPE", typeName ) != NO_ERROR ||
		static_cast<int>( pItemDef->GetType()) != customType )
	{
		std::fprintf( stderr, "TYPEDEF did not retain its script-defined index\n" );
		return false;
	}

	CItemDef invalidDef( ITEMID_MULTI_MAX );
	CGVariant invalidType;
	invalidType.SetInt( -1 );
	if ( invalidDef.s_PropSet( "TYPE", invalidType ) != NO_ERROR ||
		invalidDef.GetType() != IT_NORMAL )
	{
		std::fprintf( stderr, "invalid TYPE= did not fall back to IT_NORMAL\n" );
		return false;
	}

	char szTempDir[] = "/tmp/sphere-custom-type-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;
	const std::string sWorldPath = std::string( szTempDir ) + "/custom-type-world.scp";
	const std::string sOldWorldBaseDir = (LPCTSTR) g_Cfg.m_sWorldBaseDir;
	{
		std::ofstream world( sWorldPath.c_str());
		world << "TITLE=Sphere custom TYPEDEF fixture\n"
			"VERSION=0.99\n"
			"SAVECOUNT=0\n"
			"[WORLDITEM " << testItemID << "]\n"
			"SERIAL=45123\n"
			"P=128,128,0\n"
			"[EOF]\n";
		if ( !world )
		{
			unlink( sWorldPath.c_str());
			rmdir( szTempDir );
			return false;
		}
	}

	g_Cfg.m_sWorldBaseDir = (std::string( szTempDir ) + "/").c_str();
	const SERVMODE_TYPE iModePrev = g_Serv.SetServerMode( SERVMODE_Loading );
	const bool fLoaded = g_World.LoadFileForTest( sWorldPath.c_str());
	g_Serv.SetServerMode( iModePrev );
	CItem* pLoaded = g_World.ItemFind( CSphereUID( UID_F_ITEM | 45123 ));
	const bool fCustomTypeLoaded = fLoaded && pLoaded != NULL &&
		static_cast<int>( pLoaded->GetType()) == customType;
	if ( pLoaded )
	{
		// This fixture owns the only loaded object.  Detach and destroy it
		// synchronously so the test does not leave a sector item for static
		// destruction after g_Serv.
		pLoaded->RemoveSelf();
		delete pLoaded;
	}
	g_World.GarbageCollection_New();
	g_Cfg.m_sWorldBaseDir = sOldWorldBaseDir.c_str();
	unlink( sWorldPath.c_str());
	rmdir( szTempDir );
	if ( !fCustomTypeLoaded )
	{
		std::fprintf( stderr, "saved item did not retain its custom TYPEDEF: loaded=%d\n",
			fLoaded ? 1 : 0 );
		return false;
	}
	return true;
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

static bool WriteTextFile( const std::string& sPath, const char* pszText )
{
	std::ofstream file( sPath.c_str(), std::ios::out | std::ios::trunc );
	file << pszText;
	return static_cast<bool>( file );
}

// Load with a pending manifest for generation 1 that recorded its world and
// character backups; report the loaded markers and whether the manifest is
// still pending afterwards.
static bool LoadPendingGeneration( const std::string& sBaseDir, std::string& sWorldMarker,
	std::string& sCharsMarker, bool& fManifestKept )
{
	const std::string sManifest = sBaseDir + "sphere.save.pending";
	const std::string sText = "SAVECOUNT=1\nSTATE=PENDING\nROTATED=3\nARCHIVE_W=" + sBaseDir +
		"sphereb01w.scp\nARCHIVE_C=" + sBaseDir + "sphereb01c.scp\n[EOF]\n";
	if ( !WriteTextFile( sManifest, sText.c_str()))
		return false;
	g_Cfg.m_Var.RemoveKey( "PAIR_WORLD" );
	g_Cfg.m_Var.RemoveKey( "PAIR_CHARS" );
	g_Cfg.m_fSaveBackupFallback = false;
	g_World.m_iSaveCountID = 0;
	const bool fLoaded = g_World.LoadWorldForTest() && g_World.m_iSaveCountID == 1;
	sWorldMarker = (LPCTSTR) g_Cfg.m_Var.FindKeyStr( "PAIR_WORLD" );
	sCharsMarker = (LPCTSTR) g_Cfg.m_Var.FindKeyStr( "PAIR_CHARS" );
	g_World.Close( false );
	g_Cfg.m_Var.RemoveKey( "PAIR_WORLD" );
	g_Cfg.m_Var.RemoveKey( "PAIR_CHARS" );
	fManifestKept = access( sManifest.c_str(), F_OK ) == 0;
	unlink( sManifest.c_str());
	return fLoaded;
}

// The first save after a start-up writes the SAVECOUNT that the loaded pair
// (and so each backup it takes) already carries.  A pending generation whose
// live files both carry its count is therefore only proven published when each
// live file differs from its recorded backup: a hard-linked backup that is
// still the live file, or a copied backup with the same contents, means that
// file was never replaced, and the start-up must load the backups instead of a
// new world with old characters.
static bool TestPendingSameCountPair()
{
	char szTempDir[] = "/tmp/sphere-save-same-count-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;
	const std::string sBaseDir = std::string( szTempDir ) + "/";
	const std::string sWorld = sBaseDir + "sphereworld.scp";
	const std::string sChars = sBaseDir + "spherechars.scp";
	const std::string sWorldBackup = sBaseDir + "sphereb01w.scp";
	const std::string sCharsBackup = sBaseDir + "sphereb01c.scp";
	g_Cfg.m_sWorldBaseDir = sBaseDir.c_str();

	const char* pszLiveWorld = "TITLE=World\nVERSION=0.99\nSAVECOUNT=1\n[VARNAMES]\nPAIR_WORLD=live\n[EOF]\n";
	const char* pszBackupWorld = "TITLE=World\nVERSION=0.99\nSAVECOUNT=1\n[VARNAMES]\nPAIR_WORLD=backup\n[EOF]\n";
	const char* pszLiveChars = "TITLE=Chars\nVERSION=0.99\nSAVECOUNT=1\n[VARNAMES]\nPAIR_CHARS=live\n[EOF]\n";
	const char* pszBackupChars = "TITLE=Chars\nVERSION=0.99\nSAVECOUNT=1\n[VARNAMES]\nPAIR_CHARS=backup\n[EOF]\n";
	// The world file was published; its backup is the previous world.
	const bool fWorldWritten = WriteTextFile( sWorld, pszLiveWorld ) &&
		WriteTextFile( sWorldBackup, pszBackupWorld );

	// The character backup is a hard link that is still the live file.
	std::string sLinkedWorld, sLinkedChars;
	bool fLinkedKept = false;
	const bool fLinked = fWorldWritten && WriteTextFile( sCharsBackup, pszBackupChars ) &&
		link( sCharsBackup.c_str(), sChars.c_str()) == 0 &&
		LoadPendingGeneration( sBaseDir, sLinkedWorld, sLinkedChars, fLinkedKept ) &&
		sLinkedWorld == "backup" && sLinkedChars == "backup" && fLinkedKept;

	// The character backup is a copy with the live file's contents.
	std::string sCopiedWorld, sCopiedChars;
	bool fCopiedKept = false;
	const bool fCopied = unlink( sChars.c_str()) == 0 && WriteTextFile( sChars, pszBackupChars ) &&
		LoadPendingGeneration( sBaseDir, sCopiedWorld, sCopiedChars, fCopiedKept ) &&
		sCopiedWorld == "backup" && sCopiedChars == "backup" && fCopiedKept;

	// Both live files were replaced: the published pair is loaded and the
	// stale pending record dropped.
	std::string sPublishedWorld, sPublishedChars;
	bool fPublishedKept = true;
	const bool fPublished = WriteTextFile( sChars, pszLiveChars ) &&
		LoadPendingGeneration( sBaseDir, sPublishedWorld, sPublishedChars, fPublishedKept ) &&
		sPublishedWorld == "live" && sPublishedChars == "live" && !fPublishedKept;

	unlink( sWorld.c_str());
	unlink( sChars.c_str());
	unlink( sWorldBackup.c_str());
	unlink( sCharsBackup.c_str());
	rmdir( szTempDir );
	g_World.m_iSaveCountID = 0;
	if ( !fLinked || !fCopied || !fPublished )
	{
		std::fprintf( stderr,
			"pending pair with an unchanged count: linked=%d (world=%s chars=%s kept=%d) "
			"copied=%d (world=%s chars=%s kept=%d) published=%d (world=%s chars=%s kept=%d)\n",
			fLinked ? 1 : 0, sLinkedWorld.c_str(), sLinkedChars.c_str(), fLinkedKept ? 1 : 0,
			fCopied ? 1 : 0, sCopiedWorld.c_str(), sCopiedChars.c_str(), fCopiedKept ? 1 : 0,
			fPublished ? 1 : 0, sPublishedWorld.c_str(), sPublishedChars.c_str(), fPublishedKept ? 1 : 0 );
		return false;
	}
	return true;
}

int main()
{
	if ( !TestGetItemDataReadFailureIsInvalid() )
	{
		std::fprintf( stderr, "truncated tiledata was not reported as invalid\n" );
		return 1;
	}
	std::printf( "tiledata reads: truncated records are invalid and zeroed\n" );
	if ( !TestForwardResourceAlias() )
	{
		std::fprintf( stderr, "forward resource alias was not resolved\n" );
		return 1;
	}
	std::printf( "forward resource alias retained its TYPEDEF target\n" );
	if ( !TestCustomTypeSavedItem() )
	{
		std::fprintf( stderr, "saved custom TYPEDEF item was not loaded safely\n" );
		return 1;
	}
	std::printf( "saved custom TYPEDEF item retained its resource index\n" );
	if ( !TestUIDReset() )
	{
		std::fprintf( stderr, "UID reset did not preserve the reserved slot 0\n" );
		return 1;
	}
	std::printf( "UID reset: reserved slot 0 preserved\n" );
	if ( !TestUIDReuseIsQuarantined() )
	{
		std::fprintf( stderr, "freed UID was reused before quarantine expired\n" );
		return 1;
	}
	std::printf( "UID reuse: freed slot stayed quarantined\n" );
	if ( !TestUIDQuarantineReleaseScales() )
	{
		std::fprintf( stderr, "UID quarantine release scanned too much state\n" );
		return 1;
	}
	if ( !TestStaleUIDReferenceIsRejected() )
	{
		std::fprintf( stderr, "stale UID reference was not rejected after slot reuse\n" );
		return 1;
	}
	std::printf( "UID generations: stale property write was rejected\n" );
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
	if ( !TestPendingSameCountPair() )
	{
		std::fprintf( stderr, "pending pair with an unchanged count was loaded without proof of publication\n" );
		return 1;
	}
	std::printf( "paired save load: unchanged-count pending pair loads backups unless both files were replaced\n" );
	if ( !TestLegacyPairRequiresBothUncounted() )
	{
		std::fprintf( stderr, "save pair accepted a file without SAVECOUNT next to a counted file\n" );
		return 1;
	}
	std::printf( "paired save load: uncounted legacy pair accepted; half-counted pair rejected\n" );
	if ( !TestSaveCountHeaderOnly() )
	{
		std::fprintf( stderr, "save pair count was not read from the file header only\n" );
		return 1;
	}
	std::printf( "paired save load: header SAVECOUNT matched in any case; later sections ignored\n" );

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

	// The accepted item is intentionally kept alive for the load-count checks;
	// remove it before process teardown so sanitizer shutdown does not call a
	// CItem destructor after the singleton server has entered base destruction.
	CItem* pAccepted = g_World.ItemFind( CSphereUID( UID_F_ITEM | 44 ));
	if ( pAccepted )
	{
		pAccepted->RemoveSelf();
		delete pAccepted;
	}

	std::printf( "load safety: failed and orphaned objects cleaned, counts separated, and NULL-source SAVE refused\n" );
	return 0;
}

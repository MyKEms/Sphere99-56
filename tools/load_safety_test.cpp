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

int main()
{
	if ( !TestUIDReset() )
	{
		std::fprintf( stderr, "UID reset did not preserve the reserved slot 0\n" );
		return 1;
	}
	std::printf( "UID reset: reserved slot 0 preserved\n" );

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
	const bool fSafe = hSave == NO_ERROR &&
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

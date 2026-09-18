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
	{
		std::ofstream world( sWorldPath.c_str() );
		world << "TITLE=Sphere Test World\n"
			"VERSION=0.99\n"
			"SAVECOUNT=0\n"
			"[WORLDITEM]\n"
			"NAME=deliberately-broken-object\n"
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
	const bool fLoaded = g_World.LoadFileForTest( sWorldPath.c_str() );
	if ( !fLoaded || !g_World.IsSaveBlockedByLoad() ||
		g_World.GetLoadSkippedSections() != 1 ||
		g_World.GetLoadSkippedObjects() != 1 ||
		g_World.GetLoadFailedParses() != 1 )
	{
		std::fprintf( stderr,
			"load guard did not record the broken object: loaded=%d sections=%d objects=%d parses=%d blocked=%d\n",
			fLoaded ? 1 : 0,
			g_World.GetLoadSkippedSections(),
			g_World.GetLoadSkippedObjects(),
			g_World.GetLoadFailedParses(),
			g_World.IsSaveBlockedByLoad() ? 1 : 0 );
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

	std::printf( "load safety: broken object counted and NULL-source save refused\n" );
	return 0;
}

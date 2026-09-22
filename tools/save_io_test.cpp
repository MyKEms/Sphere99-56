// Disposable regression test for checked save writes and close/flush status.

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
#include <string>

static void RemoveDirectoryContents( const char* pszDir )
{
	DIR* pDir = opendir( pszDir );
	if ( pDir == NULL )
		return;
	struct dirent* pEntry;
	while ( ( pEntry = readdir( pDir )) != NULL )
	{
		if ( !std::strcmp( pEntry->d_name, "." ) || !std::strcmp( pEntry->d_name, ".." ))
			continue;
		std::string sPath = std::string( pszDir ) + "/" + pEntry->d_name;
		unlink( sPath.c_str());
	}
	closedir( pDir );
}

static bool RunFaultCase( CFileText::TEST_FAULT fault, const char* pszName )
{
	char szTempDir[] = "/tmp/sphere-save-io-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
	{
		std::fprintf( stderr, "%s: could not create disposable directory\n", pszName );
		return false;
	}

	g_Cfg.m_sWorldBaseDir.Format( "%s/", szTempDir );
	g_Cfg.m_sAcctBaseDir.Empty();
	const int iSaveCountBefore = g_World.m_iSaveCountID;
	CFileText::SetTestFault( fault );
	CGVariant vArgs;
	vArgs.SetInt( 1 );
	CGVariant vValRet;
	const HRESULT hRes = g_Serv.s_Method( "SAVE", vArgs, vValRet, NULL );
	const bool fTriggered = CFileText::WasTestFaultTriggered();
	CFileText::ClearTestFault();
	g_World.Close( false );

	const bool fFailed = hRes != NO_ERROR &&
		iSaveCountBefore == g_World.m_iSaveCountID && fTriggered;
	if ( !fFailed )
	{
		std::fprintf( stderr,
			"%s: save unexpectedly succeeded or advanced generation: hresult=%ld before=%d after=%d triggered=%d\n",
			pszName, (long)hRes, iSaveCountBefore, g_World.m_iSaveCountID,
			fTriggered ? 1 : 0 );
	}
	RemoveDirectoryContents( szTempDir );
	rmdir( szTempDir );
	return fFailed;
}

int main()
{
	if ( !RunFaultCase( CFileText::TEST_FAULT_SHORT_WRITE, "short write" ))
		return 1;
	if ( !RunFaultCase( CFileText::TEST_FAULT_FLUSH, "flush failure" ))
		return 1;
	if ( !RunFaultCase( CFileText::TEST_FAULT_CLOSE, "close failure" ))
		return 1;
	std::printf( "save I/O: short-write, flush, and close failures rejected without generation advance\n" );
	return 0;
}

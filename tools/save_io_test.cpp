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
#include <fstream>
#include <sstream>
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

static std::string ReadFile( const std::string& sPath )
{
	std::ifstream file( sPath.c_str(), std::ios::in | std::ios::binary );
	std::ostringstream contents;
	contents << file.rdbuf();
	return contents.str();
}

static bool RunEmptyWriteCase()
{
	char szTempDir[] = "/tmp/sphere-save-empty-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;

	std::string sPath = std::string( szTempDir ) + "/empty.scp";
	CFileText file;
	const bool fOpened = file.Open( sPath.c_str(), OF_WRITE|OF_CREATE|OF_TEXT );
	const bool fWrite = fOpened && file.WriteString( "" );
	const bool fClose = file.CloseChecked();
	const bool fPassed = fOpened && fWrite && fClose && !file.HasIOError();
	if ( !fPassed )
	{
		std::fprintf( stderr, "zero-length write was treated as an I/O error: opened=%d write=%d close=%d error=%d\n",
			fOpened ? 1 : 0, fWrite ? 1 : 0, fClose ? 1 : 0,
			file.HasIOError() ? 1 : 0 );
	}
	RemoveDirectoryContents( szTempDir );
	rmdir( szTempDir );
	return fPassed;
}

static bool RunDirectorySyncCase()
{
	char szTempDir[] = "/tmp/sphere-save-directory-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;

	const bool fDirectory = CWorld::SyncSaveDirectory( szTempDir );
	const bool fMissing = CWorld::SyncSaveDirectory( "/tmp/sphere-save-directory-missing" );
	if ( !fDirectory || fMissing )
	{
		std::fprintf( stderr,
			"directory durability check failed: existing=%d missing=%d\n",
			fDirectory ? 1 : 0, fMissing ? 1 : 0 );
	}
	RemoveDirectoryContents( szTempDir );
	rmdir( szTempDir );
	return fDirectory && !fMissing;
}

static bool RunHealthyCase()
{
	char szTempDir[] = "/tmp/sphere-save-healthy-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;

	g_Cfg.m_sWorldBaseDir.Format( "%s/", szTempDir );
	g_Cfg.m_sAcctBaseDir.Empty();
	g_World.m_iSaveCountID = 0;
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	CFileText::SetTestFault( CFileText::TEST_FAULT_CLOSE, "does-not-exist.scp" );
	CGVariant vArgs;
	vArgs.SetInt( 1 );
	CGVariant vValRet;
	const HRESULT hRes = g_Serv.s_Method( "SAVE", vArgs, vValRet, NULL );
	const bool fTriggered = CFileText::WasTestFaultTriggered();
	CFileText::ClearTestFault();
	const int iGeneration = g_World.m_iSaveCountID;
	const std::string sWorld = ReadFile( std::string( szTempDir ) + "/sphereworld.scp" );
	const std::string sChars = ReadFile( std::string( szTempDir ) + "/spherechars.scp" );
	const std::string sManifest = ReadFile( std::string( szTempDir ) + "/sphere.save.pending" );
	const bool fPassed = hRes == NO_ERROR && !fTriggered && iGeneration == 1 &&
		sWorld.find( "[EOF]" ) != std::string::npos &&
		sChars.find( "[EOF]" ) != std::string::npos && sManifest.empty();
	if ( !fPassed )
	{
		std::fprintf( stderr, "healthy save control failed: hresult=%ld generation=%d world_eof=%d chars_eof=%d\n",
			(long)hRes, iGeneration,
			sWorld.find( "[EOF]" ) != std::string::npos ? 1 : 0,
			sChars.find( "[EOF]" ) != std::string::npos ? 1 : 0 );
		std::fprintf( stderr, "manifest contents: %s\n", sManifest.c_str() );
	}
	g_World.Close( false );
	RemoveDirectoryContents( szTempDir );
	rmdir( szTempDir );
	return fPassed;
}

static bool RunRepeatedFailureCase()
{
	char szTempDir[] = "/tmp/sphere-save-retry-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;

	g_Cfg.m_sWorldBaseDir.Format( "%s/", szTempDir );
	g_Cfg.m_sAcctBaseDir.Empty();
	g_World.m_iSaveCountID = 0;
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	CFileText::ClearTestFault();
	CGVariant vArgs;
	vArgs.SetInt( 1 );
	CGVariant vValRet;
	const bool fInitial = g_Serv.s_Method( "SAVE", vArgs, vValRet, NULL ) == NO_ERROR &&
		g_World.m_iSaveCountID == 1;

	CFileText::SetTestFault( CFileText::TEST_FAULT_SHORT_WRITE, "sphereworld.scp" );
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	const bool fFirstFailed = g_Serv.s_Method( "SAVE", vArgs, vValRet, NULL ) != NO_ERROR &&
		g_World.m_iSaveCountID == 1;
	const std::string sArchivePath = std::string( szTempDir ) + "/sphereb01w.scp";
	const std::string sArchiveBefore = ReadFile( sArchivePath );
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	g_World.Close( false );
	const bool fReloaded = g_World.LoadAll() && g_World.m_iSaveCountID == 1;

	CFileText::SetTestFault( CFileText::TEST_FAULT_SHORT_WRITE, "sphereworld.scp" );
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	const bool fSecondFailed = g_Serv.s_Method( "SAVE", vArgs, vValRet, NULL ) != NO_ERROR &&
		g_World.m_iSaveCountID == 1;
	const std::string sArchiveAfter = ReadFile( sArchivePath );
	const std::string sPendingManifest = ReadFile( std::string( szTempDir ) + "/sphere.save.pending" );
	CFileText::ClearTestFault();
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	const bool fRecovered = g_Serv.s_Method( "SAVE", vArgs, vValRet, NULL ) == NO_ERROR &&
		g_World.m_iSaveCountID == 2;
	const std::string sArchiveRecovered = ReadFile( sArchivePath );
	const std::string sActiveWorld = ReadFile( std::string( szTempDir ) + "/sphereworld.scp" );
	const bool fPassed = fInitial && fFirstFailed && fReloaded && fSecondFailed &&
		!sArchiveBefore.empty() && sArchiveBefore == sArchiveAfter &&
		sArchiveAfter.find( "[EOF]" ) != std::string::npos &&
		sPendingManifest.find( "STATE=PENDING" ) != std::string::npos &&
		fRecovered && sArchiveRecovered == sArchiveBefore &&
		sActiveWorld.find( "[EOF]" ) != std::string::npos &&
		ReadFile( std::string( szTempDir ) + "/sphere.save.pending" ).empty();
	if ( !fPassed )
	{
		std::fprintf( stderr, "repeated failed save replaced the last good archive: initial=%d first=%d reloaded=%d second=%d recovered=%d archive_before=%zu archive_after=%zu archive_recovered=%zu eof=%d pending=%d active_eof=%d\n",
			fInitial ? 1 : 0, fFirstFailed ? 1 : 0, fReloaded ? 1 : 0, fSecondFailed ? 1 : 0,
			fRecovered ? 1 : 0, sArchiveBefore.size(), sArchiveAfter.size(),
			sArchiveRecovered.size(),
			sArchiveAfter.find( "[EOF]" ) != std::string::npos ? 1 : 0,
			sPendingManifest.find( "STATE=PENDING" ) != std::string::npos ? 1 : 0,
			sActiveWorld.find( "[EOF]" ) != std::string::npos ? 1 : 0 );
	}
	g_World.Close( false );
	RemoveDirectoryContents( szTempDir );
	rmdir( szTempDir );
	return fPassed;
}

static bool RunFaultCase( CFileText::TEST_FAULT fault, const char* pszName, const char* pszTargetFile )
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
	CFileText::SetTestFault( fault, pszTargetFile );
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
	if ( !RunEmptyWriteCase() )
		return 1;
	if ( !RunDirectorySyncCase() )
		return 1;
	if ( !RunHealthyCase() )
		return 1;
	if ( !RunFaultCase( CFileText::TEST_FAULT_SHORT_WRITE, "short write", "spherechars.scp" ))
		return 1;
	if ( !RunFaultCase( CFileText::TEST_FAULT_FLUSH, "flush failure", "spherechars.scp" ))
		return 1;
	if ( !RunFaultCase( CFileText::TEST_FAULT_CLOSE, "close failure", "sphereworld.scp" ))
		return 1;
	if ( !RunRepeatedFailureCase() )
		return 1;
	std::printf( "save I/O: short-write, flush, and close failures rejected without generation advance\n" );
	return 0;
}

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

// A global variable named SAVECOUNT is written into [VARNAMES] as
// "SAVECOUNT=<value>".  The validation of the temporary files reads the count
// only from the file header, so such a variable must not fail every save.
static bool RunSaveCountVariableCase()
{
	char szTempDir[] = "/tmp/sphere-save-var-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;

	g_Cfg.m_sWorldBaseDir.Format( "%s/", szTempDir );
	g_Cfg.m_sAcctBaseDir.Empty();
	g_World.m_iSaveCountID = 0;
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	CFileText::ClearTestFault();
	g_Cfg.m_Var.SetKeyInt( "SAVECOUNT", 99 );
	CGVariant vArgs;
	vArgs.SetInt( 1 );
	CGVariant vValRet;
	const bool fFirst = g_Serv.s_Method( "SAVE", vArgs, vValRet, NULL ) == NO_ERROR &&
		g_World.m_iSaveCountID == 1;
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	const bool fSecond = g_Serv.s_Method( "SAVE", vArgs, vValRet, NULL ) == NO_ERROR &&
		g_World.m_iSaveCountID == 2;
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	const std::string sWorld = ReadFile( std::string( szTempDir ) + "/sphereworld.scp" );
	g_Cfg.m_Var.RemoveKey( "SAVECOUNT" );
	const bool fVarWritten = sWorld.find( "[VARNAMES]\nSAVECOUNT=" ) != std::string::npos;
	const bool fPassed = fFirst && fSecond && fVarWritten;
	if ( !fPassed )
	{
		std::fprintf( stderr,
			"global SAVECOUNT variable broke save validation: first=%d second=%d var_written=%d\n",
			fFirst ? 1 : 0, fSecond ? 1 : 0, fVarWritten ? 1 : 0 );
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
	const std::string sWorldBefore = ReadFile( std::string( szTempDir ) + "/sphereworld.scp" );
	const std::string sCharsBefore = ReadFile( std::string( szTempDir ) + "/spherechars.scp" );

	CFileText::SetTestFault( CFileText::TEST_FAULT_SHORT_WRITE, "sphereworld.scp.tmp" );
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	const bool fFirstFailed = g_Serv.s_Method( "SAVE", vArgs, vValRet, NULL ) != NO_ERROR &&
		g_World.m_iSaveCountID == 1;
	const std::string sArchivePath = std::string( szTempDir ) + "/sphereb01w.scp";
	const std::string sArchiveBefore = ReadFile( sArchivePath );
	const std::string sWorldAfterFirstFailure = ReadFile( std::string( szTempDir ) + "/sphereworld.scp" );
	const std::string sCharsAfterFirstFailure = ReadFile( std::string( szTempDir ) + "/spherechars.scp" );
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	g_World.Close( false );
	const bool fReloaded = g_World.LoadAll() && g_World.m_iSaveCountID == 1;

	CFileText::SetTestFault( CFileText::TEST_FAULT_SHORT_WRITE, "sphereworld.scp.tmp" );
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
		sWorldBefore == sWorldAfterFirstFailure && sCharsBefore == sCharsAfterFirstFailure &&
		sArchiveBefore.empty() && sArchiveAfter.empty() &&
		sPendingManifest.find( "STATE=PENDING" ) != std::string::npos &&
		fRecovered && !sArchiveRecovered.empty() && sArchiveRecovered == sWorldBefore &&
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

// Both files of a generation are published, then the save fails before its
// commit is recorded (here: closing the COMMITTED manifest).  The published
// pair is complete, so a restart must load it instead of the previous
// generation's backups, and the following save must back it up before
// replacing it.
static bool RunPublishedBeforeCommitCase()
{
	char szTempDir[] = "/tmp/sphere-save-commit-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;
	const std::string sBaseDir( szTempDir );
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

	// Mark the next generation so the test can tell which pair was loaded.
	g_Cfg.m_Var.SetKeyStr( "PUBLISHED_MARKER", "generation-1" );
	// Generation 1 writes the manifest when it starts and after each backup
	// (accounts, world, characters); the fifth write records the commit.
	CFileText::SetTestFault( CFileText::TEST_FAULT_CLOSE, "sphere.save.pending.tmp", 4 );
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	const bool fCommitFailed = g_Serv.s_Method( "SAVE", vArgs, vValRet, NULL ) != NO_ERROR &&
		CFileText::WasTestFaultTriggered() && g_World.m_iSaveCountID == 1;
	CFileText::ClearTestFault();
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	const std::string sPublishedWorld = ReadFile( sBaseDir + "/sphereworld.scp" );
	const std::string sPublishedChars = ReadFile( sBaseDir + "/spherechars.scp" );
	const std::string sPending = ReadFile( sBaseDir + "/sphere.save.pending" );
	const bool fPublished = sPublishedWorld.find( "SAVECOUNT=1\n" ) != std::string::npos &&
		sPublishedWorld.find( "PUBLISHED_MARKER=generation-1" ) != std::string::npos &&
		sPublishedChars.find( "SAVECOUNT=1\n" ) != std::string::npos &&
		sPending.find( "STATE=PENDING" ) != std::string::npos;

	// Restart: forget the marker, then load whatever the start-up picks.
	g_World.Close( false );
	g_Cfg.m_Var.RemoveKey( "PUBLISHED_MARKER" );
	const bool fReloaded = g_World.LoadAll();
	const bool fLoadedPublished =
		!g_Cfg.m_Var.FindKeyStr( "PUBLISHED_MARKER" ).CompareNoCase( "generation-1" );

	// The following save must back up the published pair before replacing it.
	g_Cfg.m_Var.RemoveKey( "PUBLISHED_MARKER" );
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	const bool fFollowing = g_Serv.s_Method( "SAVE", vArgs, vValRet, NULL ) == NO_ERROR;
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	const bool fBackedUp = ReadFile( sBaseDir + "/sphereb01w.scp" ) == sPublishedWorld &&
		ReadFile( sBaseDir + "/sphereb01c.scp" ) == sPublishedChars;
	const bool fPassed = fInitial && fCommitFailed && fPublished && fReloaded &&
		fLoadedPublished && fFollowing && fBackedUp;
	if ( !fPassed )
	{
		std::fprintf( stderr,
			"published pair discarded after a failed commit: initial=%d commit_failed=%d published=%d "
			"reloaded=%d loaded_published=%d following=%d backed_up=%d\n",
			fInitial ? 1 : 0, fCommitFailed ? 1 : 0, fPublished ? 1 : 0, fReloaded ? 1 : 0,
			fLoadedPublished ? 1 : 0, fFollowing ? 1 : 0, fBackedUp ? 1 : 0 );
	}
	g_World.Close( false );
	RemoveDirectoryContents( szTempDir );
	rmdir( szTempDir );
	return fPassed;
}

static const char* const ACCOUNT_SENTINEL = "RETRY_SENTINEL=preserve-first-archive";

// Save generation 0, then append a sentinel to the live account file so the
// test can tell the previous generation's account file from any file that a
// later attempt publishes (the server serializes accounts from memory).
static bool PrepareAccountRetryCase( const char* pszTempDir )
{
	g_Cfg.m_sWorldBaseDir.Format( "%s/", pszTempDir );
	g_Cfg.m_sAcctBaseDir.Empty();
	g_World.m_iSaveCountID = 0;
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	CFileText::ClearTestFault();
	CGVariant vArgs;
	vArgs.SetInt( 1 );
	CGVariant vValRet;
	if ( g_Serv.s_Method( "SAVE", vArgs, vValRet, NULL ) != NO_ERROR ||
		g_World.m_iSaveCountID != 1 )
		return false;
	std::ofstream account( ( std::string( pszTempDir ) + "/sphereaccu.scp" ).c_str(),
		std::ios::out | std::ios::app );
	account << ACCOUNT_SENTINEL << "\n";
	return static_cast<bool>( account );
}

static bool RunAccountSave( CFileText::TEST_FAULT fault, const char* pszTarget )
{
	if ( fault == CFileText::TEST_FAULT_NONE )
		CFileText::ClearTestFault();
	else
		CFileText::SetTestFault( fault, pszTarget );
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	CGVariant vArgs;
	vArgs.SetInt( 1 );
	CGVariant vValRet;
	const bool fSaved = g_Serv.s_Method( "SAVE", vArgs, vValRet, NULL ) == NO_ERROR;
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	return fSaved;
}

static void FinishAccountRetryCase( char* pszTempDir )
{
	g_World.Close( false );
	CFileText::ClearTestFault();
	RemoveDirectoryContents( pszTempDir );
	rmdir( pszTempDir );
}

// The account archive for a generation is taken once.  A failure after it was
// taken (here: closing the world file, after the accounts were published)
// leaves the pending manifest recording it, and the retry reuses it instead of
// archiving the account file that the failed attempt published.
static bool RunRetryPreservesAccountArchiveCase()
{
	char szTempDir[] = "/tmp/sphere-save-account-retry-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;
	const bool fInitial = PrepareAccountRetryCase( szTempDir );
	const std::string sArchivePath = std::string( szTempDir ) + "/sphereb01a.scp";

	const bool fFailed = !RunAccountSave( CFileText::TEST_FAULT_CLOSE, "sphereworld.scp.tmp" ) &&
		g_World.m_iSaveCountID == 1 && CFileText::WasTestFaultTriggered();
	const std::string sArchiveBefore = ReadFile( sArchivePath );
	const std::string sPending = ReadFile( std::string( szTempDir ) + "/sphere.save.pending" );

	const bool fRecovered = RunAccountSave( CFileText::TEST_FAULT_NONE, NULL ) &&
		g_World.m_iSaveCountID == 2;
	const std::string sArchiveAfter = ReadFile( sArchivePath );
	const bool fRotated = sPending.find( "ROTATED=4" ) != std::string::npos;
	const bool fPassed = fInitial && fFailed &&
		sArchiveBefore.find( ACCOUNT_SENTINEL ) != std::string::npos &&
		fRotated && fRecovered && sArchiveAfter == sArchiveBefore;
	if ( !fPassed )
	{
		std::fprintf( stderr,
			"retry re-rotated account archive: initial=%d failed=%d recovered=%d archive_before=%zu archive_after=%zu rotated=%d\n",
			fInitial ? 1 : 0, fFailed ? 1 : 0, fRecovered ? 1 : 0,
			sArchiveBefore.size(), sArchiveAfter.size(), fRotated ? 1 : 0 );
	}
	FinishAccountRetryCase( szTempDir );
	return fPassed;
}

// A failure before the account archive was taken (here: closing the account
// temporary file) must not leave the generation without an account backup:
// the retry takes it from the still-unchanged live account file.
static bool RunRetryTakesMissingAccountArchiveCase()
{
	char szTempDir[] = "/tmp/sphere-save-account-late-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;
	const bool fInitial = PrepareAccountRetryCase( szTempDir );
	const std::string sArchivePath = std::string( szTempDir ) + "/sphereb01a.scp";

	const bool fFailed = !RunAccountSave( CFileText::TEST_FAULT_CLOSE, "sphereaccu.scp.tmp" ) &&
		g_World.m_iSaveCountID == 1 && CFileText::WasTestFaultTriggered();
	const std::string sArchiveBefore = ReadFile( sArchivePath );
	const std::string sPending = ReadFile( std::string( szTempDir ) + "/sphere.save.pending" );
	const std::string sLiveAfterFailure = ReadFile( std::string( szTempDir ) + "/sphereaccu.scp" );

	const bool fRecovered = RunAccountSave( CFileText::TEST_FAULT_NONE, NULL ) &&
		g_World.m_iSaveCountID == 2;
	const std::string sArchiveAfter = ReadFile( sArchivePath );
	const bool fPassed = fInitial && fFailed && sArchiveBefore.empty() &&
		sPending.find( "ROTATED=0" ) != std::string::npos &&
		sLiveAfterFailure.find( ACCOUNT_SENTINEL ) != std::string::npos &&
		fRecovered && sArchiveAfter == sLiveAfterFailure;
	if ( !fPassed )
	{
		std::fprintf( stderr,
			"retry skipped the account archive: initial=%d failed=%d recovered=%d archive_before=%zu archive_after=%zu live=%zu\n",
			fInitial ? 1 : 0, fFailed ? 1 : 0, fRecovered ? 1 : 0,
			sArchiveBefore.size(), sArchiveAfter.size(), sLiveAfterFailure.size());
	}
	FinishAccountRetryCase( szTempDir );
	return fPassed;
}

// If the account archive recorded by the pending manifest disappears, the
// retry must stop instead of archiving the file the failed attempt published.
static bool RunRetryLostAccountArchiveFailsClosedCase()
{
	char szTempDir[] = "/tmp/sphere-save-account-lost-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;
	const bool fInitial = PrepareAccountRetryCase( szTempDir );
	const std::string sArchivePath = std::string( szTempDir ) + "/sphereb01a.scp";

	const bool fFailed = !RunAccountSave( CFileText::TEST_FAULT_CLOSE, "sphereworld.scp.tmp" ) &&
		g_World.m_iSaveCountID == 1 && CFileText::WasTestFaultTriggered();
	const bool fArchived = ReadFile( sArchivePath ).find( ACCOUNT_SENTINEL ) != std::string::npos;
	unlink( sArchivePath.c_str());

	const bool fRetryRefused = !RunAccountSave( CFileText::TEST_FAULT_NONE, NULL ) &&
		g_World.m_iSaveCountID == 1;
	const std::string sPending = ReadFile( std::string( szTempDir ) + "/sphere.save.pending" );
	const bool fPassed = fInitial && fFailed && fArchived && fRetryRefused &&
		ReadFile( sArchivePath ).empty() &&
		sPending.find( "STATE=PENDING" ) != std::string::npos &&
		sPending.find( "ROTATED=4" ) != std::string::npos;
	if ( !fPassed )
	{
		std::fprintf( stderr,
			"retry re-rotated a lost account archive: initial=%d failed=%d archived=%d refused=%d archive_after=%zu\n",
			fInitial ? 1 : 0, fFailed ? 1 : 0, fArchived ? 1 : 0, fRetryRefused ? 1 : 0,
			ReadFile( sArchivePath ).size());
	}
	FinishAccountRetryCase( szTempDir );
	return fPassed;
}

// The very first generation has no live account file to archive.  A failure
// after the accounts were published must not record an archive that was never
// taken, or every retry would fail closed.
static bool RunFirstSaveRetryWithoutArchiveCase()
{
	char szTempDir[] = "/tmp/sphere-save-first-retry-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;

	g_Cfg.m_sWorldBaseDir.Format( "%s/", szTempDir );
	g_Cfg.m_sAcctBaseDir.Empty();
	g_World.m_iSaveCountID = 0;
	const bool fFailed = !RunAccountSave( CFileText::TEST_FAULT_CLOSE, "sphereworld.scp.tmp" ) &&
		g_World.m_iSaveCountID == 0 && CFileText::WasTestFaultTriggered();
	const std::string sPending = ReadFile( std::string( szTempDir ) + "/sphere.save.pending" );
	const bool fRecovered = RunAccountSave( CFileText::TEST_FAULT_NONE, NULL ) &&
		g_World.m_iSaveCountID == 1;
	const bool fPassed = fFailed && fRecovered &&
		sPending.find( "STATE=PENDING" ) != std::string::npos &&
		sPending.find( "ROTATED=4" ) == std::string::npos;
	if ( !fPassed )
	{
		std::fprintf( stderr,
			"first save retry incorrectly required absent archive: failed=%d recovered=%d rotated=%d\n",
			fFailed ? 1 : 0, fRecovered ? 1 : 0,
			sPending.find( "ROTATED=4" ) != std::string::npos ? 1 : 0 );
	}
	FinishAccountRetryCase( szTempDir );
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

static bool RunShutdownCompletionCase()
{
	char szTempDir[] = "/tmp/sphere-save-shutdown-XXXXXX";
	if ( mkdtemp( szTempDir ) == NULL )
		return false;
	const int iBackgroundBefore = g_Cfg.m_iSaveBackgroundTime;
	g_Cfg.m_sWorldBaseDir.Format( "%s/", szTempDir );
	g_Cfg.m_sAcctBaseDir.Empty();
	g_Cfg.m_iSaveBackgroundTime = TICKS_PER_SEC;
	g_World.m_iSaveCountID = 0;
	g_Serv.m_iExitFlag = SPHEREERR_OK;
	CGVariant vArgs;
	vArgs.SetInt( 1 );
	CGVariant vValRet;
	const bool fInitial = g_Serv.s_Method( "SAVE", vArgs, vValRet, NULL ) == NO_ERROR &&
		g_World.m_iSaveCountID == 1;
	const bool fStarted = fInitial && g_World.Save( false ) && g_World.IsSaving();
	// SIGTERM only sets the orderly exit flag.  Close() must finish the
	// already-open generation before the process exits, and a second request
	// must not start a second generation.
	g_Serv.SetExitFlag( SPHEREERR_TIMED_CLOSE );
	g_Serv.SetExitFlag( SPHEREERR_TIMED_CLOSE );
	g_World.Close( false );
	const int iAfterFirstClose = g_World.m_iSaveCountID;
	g_World.Close( false );
	const std::string sWorld = ReadFile( std::string( szTempDir ) + "/sphereworld.scp" );
	const std::string sChars = ReadFile( std::string( szTempDir ) + "/spherechars.scp" );
	const bool fCompleted = fStarted && iAfterFirstClose == 2 &&
		g_World.m_iSaveCountID == iAfterFirstClose &&
		sWorld.find( "[EOF]" ) != std::string::npos &&
		sChars.find( "[EOF]" ) != std::string::npos &&
		ReadFile( std::string( szTempDir ) + "/sphere.save.pending" ).empty();
	g_Cfg.m_iSaveBackgroundTime = iBackgroundBefore;
	RemoveDirectoryContents( szTempDir );
	rmdir( szTempDir );
	return fCompleted;
}

int main()
{
	if ( !RunEmptyWriteCase() )
		return 1;
	if ( !RunDirectorySyncCase() )
		return 1;
	if ( !RunHealthyCase() )
		return 1;
	if ( !RunSaveCountVariableCase() )
		return 1;
	if ( !RunFaultCase( CFileText::TEST_FAULT_SHORT_WRITE, "short write", "spherechars.scp.tmp" ))
		return 1;
	if ( !RunFaultCase( CFileText::TEST_FAULT_FLUSH, "flush failure", "spherechars.scp.tmp" ))
		return 1;
	if ( !RunFaultCase( CFileText::TEST_FAULT_CLOSE, "close failure", "sphereworld.scp.tmp" ))
		return 1;
	if ( !RunRepeatedFailureCase() )
		return 1;
	if ( !RunPublishedBeforeCommitCase() )
		return 1;
	// Run every account retry case so one run reports each failing case.
	const bool fAccountReuse = RunRetryPreservesAccountArchiveCase();
	const bool fAccountLate = RunRetryTakesMissingAccountArchiveCase();
	const bool fAccountLost = RunRetryLostAccountArchiveFailsClosedCase();
	const bool fFirstRetry = RunFirstSaveRetryWithoutArchiveCase();
	if ( !fAccountReuse || !fAccountLate || !fAccountLost || !fFirstRetry )
		return 1;
	if ( !RunShutdownCompletionCase() )
	{
		std::fprintf( stderr, "orderly shutdown did not finish the active save exactly once\n" );
		return 1;
	}
	std::printf( "save I/O: short-write, flush, and close failures rejected without generation advance\n" );
	return 0;
}

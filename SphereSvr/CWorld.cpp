//
// CWorld.CPP
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#else
#include <windows.h>
#include <io.h>
#endif
#include <chrono>
#include <set>
#if defined(SPHERE_CRASH_RECOVERY_ENABLED)
#include <setjmp.h>
#include <signal.h>
#endif

static unsigned long long SaveClockMillis()
{
#ifdef _WIN32
	return (unsigned long long)GetTickCount();
#else
	struct timespec ts;
	if ( clock_gettime( CLOCK_MONOTONIC, &ts ) != 0 )
		return 0;
	return (unsigned long long)ts.tv_sec * 1000ULL +
		(unsigned long long)ts.tv_nsec / 1000000ULL;
#endif
}

static unsigned long long SaveFileSize( LPCTSTR pszPath )
{
	if ( !pszPath || !pszPath[0] )
		return 0;
	struct stat st;
	if ( stat( pszPath, &st ) != 0 )
		return 0;
	return (unsigned long long)st.st_size;
}

static bool SaveSyncStream( FILE* pFile )
{
	if ( !pFile || fflush( pFile ) != 0 )
		return false;
#ifdef _WIN32
	const int iFD = _fileno( pFile );
	if ( iFD < 0 )
		return false;
	const intptr_t iHandle = _get_osfhandle( iFD );
	return iHandle != -1 && FlushFileBuffers((HANDLE)iHandle) != 0;
#else
	return fsync( fileno( pFile )) == 0;
#endif
}

static bool SaveCopyFile( LPCTSTR pszSource, LPCTSTR pszTarget )
{
	FILE* pSource = fopen( pszSource, "rb" );
	if ( !pSource )
		return false;
	FILE* pTarget = fopen( pszTarget, "wb" );
	if ( !pTarget )
	{
		fclose( pSource );
		return false;
	}
	char szBuffer[64 * 1024];
	bool fOK = true;
	for (;;)
	{
		size_t iRead = fread( szBuffer, 1, sizeof(szBuffer), pSource );
		if ( iRead > 0 && fwrite( szBuffer, 1, iRead, pTarget ) != iRead )
		{
			fOK = false;
			break;
		}
		if ( iRead < sizeof(szBuffer) )
		{
			if ( ferror( pSource ))
				fOK = false;
			break;
		}
	}
	if ( fOK && !SaveSyncStream( pTarget ))
		fOK = false;
	if ( fclose( pTarget ) != 0 )
		fOK = false;
	if ( fclose( pSource ) != 0 )
		fOK = false;
	if ( !fOK )
		remove( pszTarget );
	return fOK;
}

static const SOUND_TYPE sm_Sounds_Ghost[] =
{
	SOUND_GHOST_1,
	SOUND_GHOST_2,
	SOUND_GHOST_3,
	SOUND_GHOST_4,
	SOUND_GHOST_5,
};

namespace
{
	static const int INTEGRITY_LOG_BUDGET = 16;

	static long long IntegrityNowMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	static void IntegrityAppend( char* pszOut, size_t iOutLen, const char* pszPart )
	{
		if ( pszOut == NULL || pszPart == NULL || iOutLen == 0 )
			return;
		const size_t iUsed = strlen( pszOut );
		if ( iUsed + 1 < iOutLen )
			strncat( pszOut, pszPart, iOutLen - iUsed - 1 );
	}

	static int IntegrityViolation( const char* pszRule, const CObjBase* pObj,
		const char* pszChain, int& iLogBudget )
	{
		if ( iLogBudget <= 0 )
			return 1;
		--iLogBudget;
		const unsigned dwUID = pObj ? static_cast<unsigned>(pObj->GetUID()) : 0;
		g_Log.Event( LOG_GROUP_INIT, LOGL_CRIT,
			"integrity watchdog rule=%s uid=0x%x chain=%s" LOG_CR,
			pszRule, dwUID, pszChain ? pszChain : "-" );
		return 1;
	}

	static void IntegrityChain( const CObjBase* pObj, char* pszOut, size_t iOutLen )
	{
		if ( pszOut == NULL || iOutLen == 0 )
			return;
		pszOut[0] = '\0';
		std::set<const CObjBase*> seen;
		const CObjBase* pCurrent = pObj;
		for ( int i = 0; pCurrent != NULL && i < 16; ++i )
		{
			if ( ! seen.insert( pCurrent ).second )
			{
				IntegrityAppend( pszOut, iOutLen, "->cycle" );
				break;
			}
			char szUID[32];
			snprintf( szUID, sizeof(szUID), "%s0x%x", i ? "->" : "",
				static_cast<unsigned>(pCurrent->GetUID()) );
			IntegrityAppend( pszOut, iOutLen, szUID );
			pCurrent = dynamic_cast<const CObjBase*>(pCurrent->GetParent());
		}
		if ( pszOut[0] == '\0' )
			strncpy( pszOut, "-", iOutLen - 1 );
		pszOut[iOutLen - 1] = '\0';
	}

	static bool IntegrityParentAllowed( const CObjBase* pObj, const CGObList* pParent )
	{
		if ( pParent == NULL || pParent == &g_World.m_ObjNew ||
			pParent == &g_World.m_ObjDelete )
			return pParent != NULL;
		if ( pObj->IsChar())
			return dynamic_cast<const CCharsList*>(pParent) != NULL ||
				dynamic_cast<const CCharsActiveList*>(pParent) != NULL;
		if ( pObj->IsItem())
			return dynamic_cast<const CItemsList*>(pParent) != NULL ||
				dynamic_cast<const CChar*>(pParent) != NULL ||
				dynamic_cast<const CItemContainer*>(pParent) != NULL;
		return false;
	}

	static int CheckIntegrityObject( CWorld* pWorld, CObjBase* pObj,
		DWORD dwUIDIndex, int& iWorkBudget, int& iLogBudget )
	{
		if ( pObj == NULL )
			return 0;
		int iViolations = 0;
		char szChain[256];
		IntegrityChain( pObj, szChain, sizeof(szChain));

		CGObList* pParent = pObj->GetParent();
		if ( ! IntegrityParentAllowed( pObj, pParent ))
			iViolations += IntegrityViolation( pParent ? "invalid_parent" : "parent_missing",
				pObj, szChain, iLogBudget );
		else if ( pParent != NULL && ! pParent->IsMyChild( pObj ))
			iViolations += IntegrityViolation( "parent_missing_link", pObj, szChain, iLogBudget );

		if (( pObj->GetUIDIndex() & UID_INDEX_MASK ) != dwUIDIndex )
			iViolations += IntegrityViolation( "uid_slot_mismatch", pObj, szChain, iLogBudget );

		if ( pObj->IsItem())
		{
			CContainer* pParentContainer = dynamic_cast<CContainer*>(pParent);
			if ( pParentContainer != NULL && ! pParentContainer->IsMyChild(pObj))
				iViolations += IntegrityViolation( "container_missing_child", pObj, szChain, iLogBudget );

			std::set<const CObjBase*> seen;
			const CObjBase* pCurrent = pObj;
			while ( pCurrent != NULL )
			{
				CGObList* pCurrentParent = pCurrent->GetParent();
				CObjBase* pContainer = dynamic_cast<CObjBase*>(pCurrentParent);
				if ( pContainer == NULL )
					break;
				if ( ! seen.insert( pContainer ).second )
				{
					iViolations += IntegrityViolation( "container_cycle", pObj, szChain, iLogBudget );
					break;
				}
				pCurrent = pContainer;
			}
		}

		// Only a top-level item or active character has world coordinates. A
		// disconnected object may retain an uninitialised point during teardown.
		const bool fTopItem = pObj->IsItem() && dynamic_cast<CItemsList*>(pParent) != NULL;
		const bool fActiveChar = pObj->IsChar() && dynamic_cast<CCharsActiveList*>(pParent) != NULL;
		if (( fTopItem || fActiveChar ) && ! pObj->GetTopPoint().IsValidPoint())
			iViolations += IntegrityViolation( "invalid_top_point", pObj, szChain, iLogBudget );

		CContainer* pContainer = dynamic_cast<CContainer*>(pObj);
		if ( pContainer != NULL && iWorkBudget != 0 )
		{
			for ( CItemPtr pChild = pContainer->GetHead(); pChild != NULL; pChild = pChild->GetNext())
			{
				if ( iWorkBudget > 0 )
					--iWorkBudget;
				if ( pChild->GetParent() != pContainer )
					iViolations += IntegrityViolation( "child_parent_backlink", pChild, szChain, iLogBudget );
				CObjBase* pLinked = pWorld->ObjFind( pChild->GetUID());
				if ( pLinked != pChild )
					iViolations += IntegrityViolation( "child_uid_link", pChild, szChain, iLogBudget );
				if ( iWorkBudget == 0 )
					break;
			}
		}
		return iViolations;
	}
}

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
	if ( pObj == NULL )
		return 0x7100;
	if ( ! pObj->IsValidUID())
		return 0x7102;
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
			// During initial load, don't delete objects — they may be items in containers
			// that haven't been placed yet. Just clear the list without deleting.
			if ( g_Serv.IsLoading() || g_Serv.m_iModeCode >= 0x10 )
			{
				// Just detach objects from the list without deleting them.
				// They should be referenced from sectors, containers, or UID table.
				while (m_ObjNew.GetHead())
				{
					m_ObjNew.GetHead()->RemoveSelf();
				}
			}
			else
			{
				g_Log.Event( LOG_GROUP_DEBUG, LOGL_ERROR, "%d Lost object deleted" LOG_CR, m_ObjNew.GetCount());
				m_ObjNew.DeleteAll();
			}
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
		CResourceObj* pRaw = FindUIDObj(i);
		if ( pRaw == NULL )
			continue;	// not used.
		CObjBasePtr pObj = STATIC_CAST(CObjBase, pRaw);

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
	m_fSaveRetry = false;
	m_ullSaveStartMillis = 0;
	m_iSaveStartItems = 0;
	m_iSaveStartChars = 0;
	m_iIntegrityCursor = 1;
	m_iIntegrityCycleObjects = 0;
	m_iIntegrityCycleSlots = 0;
	m_iIntegrityCycleViolations = 0;
	m_iIntegrityLogBudget = INTEGRITY_LOG_BUDGET;
	m_iIntegrityLastLogs = 0;
	m_iIntegrityCycleChanges = CObjBase::sm_iChangeCount;
	m_iIntegrityCycleStartMs = 0;
	m_fIntegrityResourceBaseline = false;
	m_iIntegrityResourceDefNames = 0;
	m_iIntegrityResourceDialogs = 0;
	m_iIntegrityResourceFunctions = 0;
	m_ridIntegrityResourceDefName.InitUID();
	m_ridIntegrityResourceDialog.InitUID();
	m_ridIntegrityResourceFunction.InitUID();
	m_sIntegrityResourceDefName.Empty();
	m_sIntegrityResourceDialog.Empty();
	m_sIntegrityResourceFunction.Empty();
	ResetLoadIntegrity();
}

CWorld::~CWorld()
{
	Close(true);
}

///////////////////////////////////////////////
// Loading and Saving.

void CWorld::ResetLoadIntegrity()
{
	m_iLoadSkippedSections = 0;
	m_iLoadSkippedObjects = 0;
	m_iLoadFailedParses = 0;
	m_iLoadReadItems = 0;
	m_iLoadReadChars = 0;
	m_iLoadItems = 0;
	m_iLoadChars = 0;
	m_iLoadAllocatedItems = 0;
	m_iLoadAllocatedChars = 0;
	m_iLoadAccepted = 0;
	m_iLoadToleratedLegacy = 0;
	m_iLoadRejected = 0;
	m_iLoadDefaulted = 0;
	m_iLoadDeleted = 0;
	m_iLoadDuplicateSerials = 0;
	for ( int i = 0; i < LOAD_LOG_QTY; ++i )
	{
		m_iLoadLogEmitted[i] = 0;
		m_iLoadLogSuppressed[i] = 0;
	}
	m_fSaveBlockedByLoad = false;
	m_fSaveFailed = false;
	m_fSaveRetry = false;
	m_fLoadIntegrityReported = false;
	m_fLoadCountsCaptured = false;
}

bool CWorld::ShouldLogLoadDetail( LOAD_LOG_CATEGORY category )
{
	if ( category < 0 || category >= LOAD_LOG_QTY )
		return true;
	// The counters bound noisy world-load diagnostics.  Runtime property and
	// placement failures must remain visible after loading, even if the load
	// phase exhausted a category's budget.
	if ( !g_Serv.IsLoading())
		return true;
	static const int sm_iLoadLogLimit = 8;
	if ( m_iLoadLogEmitted[category] < sm_iLoadLogLimit )
	{
		m_iLoadLogEmitted[category]++;
		return true;
	}
	m_iLoadLogSuppressed[category]++;
	return false;
}

void CWorld::MarkLoadIssue( bool fObjectSection )
{
	m_iLoadSkippedSections++;
	if ( fObjectSection )
		m_iLoadSkippedObjects++;
	m_iLoadFailedParses++;
	m_fSaveBlockedByLoad = true;
}

void CWorld::ReportLoadIntegrity()
{
	if ( !m_fSaveBlockedByLoad || m_fLoadIntegrityReported )
		return;

	g_Log.Event( LOG_GROUP_INIT, LOGL_CRIT,
		"CRITICAL: world load skipped %d sections (%d objects) with %d failed parses; "
		"autosave and plain SAVE are disabled. Review the source and use explicit admin "
		"SAVE FORCE only if accepting the loss is intentional." LOG_CR,
		m_iLoadSkippedSections, m_iLoadSkippedObjects, m_iLoadFailedParses );
	m_fLoadIntegrityReported = true;
}

void CWorld::FormatLoadCounts( CGString& s ) const
{
	s.Format( "world load: created_items=%d created_chars=%d read_items=%d read_chars=%d "
		"allocated_items=%d allocated_chars=%d",
		m_iLoadItems, m_iLoadChars, m_iLoadReadItems, m_iLoadReadChars,
		m_iLoadAllocatedItems, m_iLoadAllocatedChars );
}

void CWorld::LogLoadCounts() const
{
	CGString sCounts;
	FormatLoadCounts( sCounts );
	g_Log.Event( LOG_GROUP_INIT, LOGL_EVENT, "%s" LOG_CR, (LPCTSTR) sCounts );
#ifndef _WIN32
	// Linux's legacy logger only writes errors to the captured console stream.
	fprintf( stderr, "[INFO] %s\n", (LPCTSTR) sCounts );
	fflush( stderr );
#endif
}

void CWorld::CaptureLoadCounts()
{
	// Created counts are recorded per successfully loaded object section. Do not
	// infer them from live UID slots here: that table also reflects later deletion
	// and placement side effects, not just section-load success.
	m_iLoadAllocatedItems = g_Serv.StatGet( SERV_STAT_ITEMS );
	m_iLoadAllocatedChars = g_Serv.StatGet( SERV_STAT_CHARS );
	m_fLoadCountsCaptured = true;
	LogLoadCounts();
}

void CWorld::RecordLoadDiagnostic( LOAD_DIAGNOSTIC_TYPE type )
{
	switch ( type )
	{
	case LOAD_DIAG_ACCEPTED:
		m_iLoadAccepted++;
		break;
	case LOAD_DIAG_TOLERATED_LEGACY:
		m_iLoadToleratedLegacy++;
		break;
	case LOAD_DIAG_REJECTED:
		m_iLoadRejected++;
		break;
	case LOAD_DIAG_DEFAULTED:
		m_iLoadDefaulted++;
		break;
	case LOAD_DIAG_DELETED:
		m_iLoadDeleted++;
		break;
	}
}

void CWorld::RecordLoadDuplicateSerial( UID_INDEX dwSerial, bool fWorldItem )
{
	m_iLoadDuplicateSerials++;
	RecordLoadDiagnostic( LOAD_DIAG_REJECTED );
	if ( ShouldLogLoadDetail( LOAD_LOG_DUPLICATE_SERIAL ))
	{
		g_Log.Event( LOG_GROUP_INIT, LOGL_ERROR,
			"%s load duplicate serial: uid=0x%x keeping first object" LOG_CR,
			fWorldItem ? "WORLDITEM" : "WORLDCHAR", dwSerial );
	}
}

void CWorld::FormatLoadDiagnostics( CGString& s ) const
{
	s.Format( "world load diagnostics: accepted=%d tolerated_legacy=%d rejected=%d "
		"defaulted=%d deleted=%d",
		m_iLoadAccepted, m_iLoadToleratedLegacy, m_iLoadRejected,
		m_iLoadDefaulted, m_iLoadDeleted );
}

void CWorld::LogLoadDiagnostics() const
{
	CGString sDiagnostics;
	FormatLoadDiagnostics( sDiagnostics );
	g_Log.Event( LOG_GROUP_INIT, LOGL_EVENT, "%s" LOG_CR, (LPCTSTR) sDiagnostics );
	if ( m_iLoadDuplicateSerials )
	{
		g_Log.Event( LOG_GROUP_INIT, LOGL_EVENT,
			"world load duplicate serials: count=%d (first object kept)" LOG_CR,
			m_iLoadDuplicateSerials );
#ifndef _WIN32
		fprintf( stderr, "[INFO] world load duplicate serials: count=%d (first object kept)\n",
			m_iLoadDuplicateSerials );
#endif
	}
	static LPCTSTR const sm_szLoadLogCategories[LOAD_LOG_QTY] =
	{
		"WORLDCHAR property rejected",
		"WORLDITEM property rejected",
		"WORLDCHAR property detail",
		"WORLDITEM property detail",
		"WORLDCHAR load failed",
		"WORLDITEM load failed",
		"duplicate serial",
	};
	for ( int i = 0; i < LOAD_LOG_QTY; ++i )
	{
		if ( m_iLoadLogSuppressed[i] )
		{
			g_Log.Event( LOG_GROUP_INIT, LOGL_EVENT,
				"world load diagnostics: %s %d more suppressed" LOG_CR,
				sm_szLoadLogCategories[i], m_iLoadLogSuppressed[i] );
#ifndef _WIN32
			fprintf( stderr, "[INFO] world load diagnostics: %s %d more suppressed\n",
				sm_szLoadLogCategories[i], m_iLoadLogSuppressed[i] );
#endif
		}
	}
#ifndef _WIN32
	fprintf( stderr, "[INFO] %s\n", (LPCTSTR) sDiagnostics );
	fflush( stderr );
#endif
}

void CWorld::CleanupLoadOrphans()
{
	// Anything still in m_ObjNew was never placed in a sector or container.
	// Remove its UID and queue it for normal deletion before load counts are captured.
	const bool fDeleteRealPrev = CObjBase::sm_fDeleteReal;
	CObjBase::sm_fDeleteReal = false;
	int iOrphaned = 0;
	while ( m_ObjNew.GetHead() )
	{
		CObjBase* pObj = STATIC_CAST(CObjBase, m_ObjNew.GetHead());
		if ( pObj )
		{
			try
			{
				// Load-time orphans have no active runtime callback. Remove their
				// contents synchronously before queuing the owner so load counts
				// exclude nested objects that could not be attached.
				CContainer* pContainer = dynamic_cast<CContainer*>(pObj);
				if ( pContainer )
					pContainer->DeleteAll();
				pObj->DeleteThis();
				iOrphaned++;
			}
			catch (...)
			{
				// Preserve the orphan in the world's deletion queue if a virtual
				// cleanup hook throws, but never free a UID slot now owned by another
				// object.
				if ( pObj->GetParent() != &m_ObjDelete )
				{
					DWORD dwUIDIndex = pObj->GetUIDIndex() & UID_INDEX_MASK;
					if ( dwUIDIndex && FindUIDObj( dwUIDIndex ) == pObj )
						FreeUID( pObj );
					pObj->RemoveSelf();
					m_ObjDelete.InsertHead( pObj );
				}
				iOrphaned++;
			}
		}
		else
		{
			m_ObjNew.GetHead()->RemoveSelf();
		}
	}
	CObjBase::sm_fDeleteReal = fDeleteRealPrev;
	if ( iOrphaned )
	{
		g_Log.Event( LOG_GROUP_INIT, LOGL_WARN, "%d orphaned objects queued for deletion during load cleanup" LOG_CR, iOrphaned );
	}
}

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

// The manifest component of a save base name ("world", "chars", "accu",
// "serv"), or -1.  Its bit in ROTATED is 1 << component.
static int SaveManifestComponent( LPCTSTR pszBaseName )
{
	if ( !pszBaseName || !pszBaseName[0] )
		return -1;
	switch ( tolower( (unsigned char) pszBaseName[0] ))
	{
	case 'w': return CSaveManifest::COMPONENT_WORLD;
	case 'c': return CSaveManifest::COMPONENT_CHARS;
	case 'a': return CSaveManifest::COMPONENT_ACCOUNTS;
	case 's': return CSaveManifest::COMPONENT_SERVERS;
	default: return -1;
	}
}

// Manifest key suffix of each component's recorded backup path (ARCHIVE_W=...).
static const char sm_chSaveManifestKind[CSaveManifest::COMPONENT_QTY] = { 'W', 'C', 'A', 'S' };

static void GetSaveManifestName( CGString& sManifest, LPCTSTR pszBaseDir )
{
	sManifest.Format( "%s" SPHERE_FILE ".save.pending", pszBaseDir ? pszBaseDir : "" );
}

// The backup of a component for generation iSaveCount: the path the manifest
// recorded when the backup was taken.  A manifest written before paths were
// recorded falls back to the computed name.
static void GetManifestArchive( const CSaveManifest& manifest, int iComponent,
	LPCTSTR pszBaseDir, TCHAR chType, int iSaveCount, CGString& sArchive )
{
	if ( iComponent >= 0 && iComponent < CSaveManifest::COMPONENT_QTY &&
		!manifest.m_sArchive[iComponent].IsEmpty())
	{
		sArchive = manifest.m_sArchive[iComponent];
		return;
	}
	CWorld::GetBackupName( sArchive, pszBaseDir ? pszBaseDir : "", chType, iSaveCount );
}

// A backup the pending manifest recorded is gone: the save stops instead of
// backing up the live file again (it may already hold the failed attempt's
// output).  Name the missing file and the two ways to recover.
static void LogMissingSaveArchive( int iSaveCount, LPCTSTR pszArchive, LPCTSTR pszCurrent )
{
	CGString sManifest;
	GetSaveManifestName( sManifest, g_Cfg.m_sWorldBaseDir );
	CGString sMsg;
	sMsg.Format( "Save generation %d cannot find its recorded backup '%s' of '%s'; refusing to back it up again. "
		"Restore that backup, or remove '%s' to abandon the interrupted generation "
		"(the next save then backs up the current files)." LOG_CR,
		iSaveCount, pszArchive, pszCurrent, (LPCTSTR)sManifest );
	g_Log.EventStr( LOG_GROUP_SAVE, LOGL_CRIT, sMsg );
}

bool CWorld::ReadSaveManifest( LPCTSTR pszBaseDir, CSaveManifest& manifest )
{
	CGString sManifest;
	GetSaveManifestName( sManifest, pszBaseDir );
	FILE* pFile = fopen( sManifest, "rb" );
	if ( !pFile )
		return false;

	char szLine[SCRIPT_MAX_LINE_LEN];
	bool fSaveCount = false;
	bool fEOF = false;
	manifest = CSaveManifest( 0, true );
	while ( fgets( szLine, sizeof(szLine), pFile ) != NULL )
	{
		size_t iLen = strlen( szLine );
		while ( iLen > 0 && ( szLine[iLen - 1] == '\n' || szLine[iLen - 1] == '\r' ))
			szLine[--iLen] = '\0';
		if ( !strncmp( szLine, "SAVECOUNT=", 10 ))
		{
			if ( sscanf( szLine + 10, "%d", &manifest.m_iSaveCount ) == 1 )
				fSaveCount = true;
		}
		else if ( !strncmp( szLine, "STATE=", 6 ))
		{
			manifest.m_fPending = strncmp( szLine + 6, "COMMITTED", 9 ) != 0;
		}
		else if ( !strncmp( szLine, "ROTATED=", 8 ))
		{
			unsigned iRotated = 0;
			if ( sscanf( szLine + 8, "%u", &iRotated ) == 1 )
				manifest.m_dwRotated = iRotated;
		}
		else if ( !strncmp( szLine, "ARCHIVE_", 8 ) && szLine[8] && szLine[9] == '=' )
		{
			const int iComponent = SaveManifestComponent( szLine + 8 );
			if ( iComponent >= 0 )
				manifest.m_sArchive[iComponent] = szLine + 10;
		}
		else if ( !strncmp( szLine, "[EOF]", 5 ))
		{
			fEOF = true;
		}
	}
	fclose( pFile );
	if ( !fEOF )
		manifest.m_fPending = true;
	return fSaveCount;
}

bool CWorld::WriteSaveManifest( LPCTSTR pszBaseDir, const CSaveManifest& manifest )
{
	CGString sManifest;
	GetSaveManifestName( sManifest, pszBaseDir );
	CGString sManifestTemp;
	sManifestTemp.Format( "%s.tmp", (LPCTSTR)sManifest );
	CScript s;
	remove( sManifestTemp );
	if ( !s.Open( sManifestTemp, OF_WRITE|OF_CREATE|OF_TEXT ))
		return false;
	s.WriteKeyInt( "SAVECOUNT", manifest.m_iSaveCount );
	s.WriteKey( "STATE", manifest.m_fPending ? "PENDING" : "COMMITTED" );
	s.WriteKeyInt( "ROTATED", (int) manifest.m_dwRotated );
	for ( int i = 0; i < CSaveManifest::COMPONENT_QTY; i++ )
	{
		if ( manifest.m_sArchive[i].IsEmpty())
			continue;
		CGString sKey;
		sKey.Format( "ARCHIVE_%c", sm_chSaveManifestKind[i] );
		s.WriteKey( sKey, manifest.m_sArchive[i] );
	}
	s.WriteSection( "EOF" );
	const bool fWriteOK = !s.HasIOError() && s.Sync();
	const bool fCloseOK = s.CloseChecked();
	if ( !fWriteOK || !fCloseOK )
	{
		remove( sManifestTemp );
		return false;
	}
	// Replace the manifest atomically, then synchronize its directory so the
	// recorded state survives a sudden restart.
	if ( !PublishSaveFile( sManifestTemp, sManifest ))
	{
		remove( sManifestTemp );
		return false;
	}
	return true;
}

void CWorld::RemoveSaveManifest( LPCTSTR pszBaseDir )
{
	CGString sManifest;
	GetSaveManifestName( sManifest, pszBaseDir );
	remove( sManifest );
}

static bool SaveFileExists( LPCTSTR pszPath )
{
	if ( !pszPath || !pszPath[0] )
		return false;
	FILE* pFile = fopen( pszPath, "rb" );
	if ( !pFile )
		return false;
	fclose( pFile );
	return true;
}

bool CWorld::PublishSaveFile( LPCTSTR pszTemp, LPCTSTR pszCurrent )
{
	if ( !pszTemp || !pszCurrent )
		return false;
	bool fPublished = false;
#ifdef _WIN32
	fPublished = MoveFileEx( pszTemp, pszCurrent,
		MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH ) != 0;
#else
	fPublished = rename( pszTemp, pszCurrent ) == 0;
#endif
	if ( !fPublished )
		return false;

	const char* pszSlash = strrchr( pszCurrent, '/' );
#ifdef _WIN32
	const char* pszBackslash = strrchr( pszCurrent, '\\' );
	if ( pszBackslash && ( !pszSlash || pszBackslash > pszSlash ))
		pszSlash = pszBackslash;
#endif
	CGString sDirectory;
	if ( pszSlash )
	{
		if ( pszSlash == pszCurrent )
			sDirectory.Copy( "/" );
		else
		{
			sDirectory.Copy( pszCurrent );
			sDirectory.SetLength( (int)( pszSlash - pszCurrent ));
		}
	}
	else
		sDirectory.Copy( "." );
	return SyncSaveDirectory( sDirectory );
}

bool CWorld::SyncSaveDirectory( LPCTSTR pszBaseDir )
{
	const LPCTSTR pszDir = ( pszBaseDir && pszBaseDir[0] ) ? pszBaseDir : ".";
#ifdef _WIN32
	HANDLE hDir = CreateFile( pszDir, GENERIC_READ,
		FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS, NULL );
	if ( hDir == INVALID_HANDLE_VALUE )
		return false;
	const BOOL fOK = FlushFileBuffers( hDir );
	CloseHandle( hDir );
	return fOK != FALSE;
#else
	const int iFD = open( pszDir, O_RDONLY
#ifdef O_DIRECTORY
		| O_DIRECTORY
#endif
#ifdef O_CLOEXEC
		| O_CLOEXEC
#endif
	);
	if ( iFD < 0 )
		return false;
	const bool fOK = fsync( iFD ) == 0;
	close( iFD );
	return fOK;
#endif
}

void CWorld::GetSaveTempName( CGString& sTemp, LPCTSTR pszBaseDir, LPCTSTR pszBaseName )
{
	sTemp.Format( "%s" SPHERE_FILE "%s" SCRIPT_EXT ".tmp",
		pszBaseDir ? pszBaseDir : "", pszBaseName ? pszBaseName : "" );
}

// remove() for save backups; the save I/O test can make it fail.
static int SaveRemoveFile( LPCTSTR pszPath )
{
#ifdef SPHERE_SAVE_IO_TEST
	if ( CFileText::ShouldFailTestPath( CFileText::TEST_FAULT_REMOVE, pszPath ))
	{
		errno = EACCES;
		return -1;
	}
#endif
	return remove( pszPath );
}

// Hard-link a save backup to its source; the save I/O test can make it fail.
static bool SaveLinkFile( LPCTSTR pszSource, LPCTSTR pszArchive )
{
#ifdef SPHERE_SAVE_IO_TEST
	if ( CFileText::ShouldFailTestPath( CFileText::TEST_FAULT_LINK, pszArchive ))
		return false;
#endif
#ifdef _WIN32
	return CreateHardLink( pszArchive, pszSource, NULL ) != FALSE;
#else
	return link( pszSource, pszArchive ) == 0;
#endif
}

bool CWorld::PreserveSaveFile( LPCTSTR pszSource, LPCTSTR pszArchive )
{
	if ( !SaveFileExists( pszSource ))
		return true;
	// The old backup can still be a hard link to the live file (an attempt
	// that failed before publishing).  If it cannot be removed, stop: writing
	// through that name would overwrite the live file.
	if ( SaveRemoveFile( pszArchive ) != 0 && errno != ENOENT )
	{
		g_Log.Event( LOG_GROUP_SAVE, LOGL_CRIT,
			"Save could not remove the old backup '%s' (error %d)" LOG_CR, pszArchive, errno );
		return false;
	}
	if ( SaveLinkFile( pszSource, pszArchive ))
		return true;
	// Without a hard link, copy to a temporary name and rename it into place,
	// so the backup name is never opened for writing.
	CGString sTemp;
	sTemp.Format( "%s.tmp", pszArchive );
	if ( SaveRemoveFile( sTemp ) != 0 && errno != ENOENT )
	{
		g_Log.Event( LOG_GROUP_SAVE, LOGL_CRIT,
			"Save could not remove the stale backup copy '%s' (error %d)" LOG_CR, (LPCTSTR)sTemp, errno );
		return false;
	}
	if ( !SaveCopyFile( pszSource, sTemp ))
		return false;
	if ( !PublishSaveFile( sTemp, pszArchive ))
	{
		remove( sTemp );
		return false;
	}
	return true;
}

bool CWorld::PreserveSaveComponent( LPCTSTR pszBaseDir, LPCTSTR pszBaseName, int iSaveCount, bool fStartManifest )
{
	// Preserve the live component as this generation's backup exactly once,
	// without ever removing the live name.  The pending manifest records the
	// path of each backup that has been taken: a retry reuses it instead of
	// archiving the file that the failed attempt already published, and a
	// recorded backup that has disappeared stops the save instead of rotating
	// again.  Without a pending manifest for this generation the backup is
	// recorded only when fStartManifest asks to start one.
	ASSERT( pszBaseName && pszBaseName[0] );
	if ( !pszBaseDir )
		pszBaseDir = "";
	CGString sCurrent;
	sCurrent.Format( "%s" SPHERE_FILE "%s" SCRIPT_EXT, pszBaseDir, pszBaseName );
	const int iComponent = SaveManifestComponent( pszBaseName );
	CSaveManifest manifest;
	const bool fPendingManifest = iComponent >= 0 &&
		ReadSaveManifest( g_Cfg.m_sWorldBaseDir, manifest ) &&
		manifest.m_fPending && manifest.m_iSaveCount == iSaveCount;
	const unsigned dwBit = iComponent >= 0 ? ( 1u << iComponent ) : 0;
	if ( fPendingManifest && ( manifest.m_dwRotated & dwBit ))
	{
		CGString sArchive;
		GetManifestArchive( manifest, iComponent, pszBaseDir, pszBaseName[0], iSaveCount, sArchive );
		if ( SaveFileExists( sArchive ))
			return true;
		LogMissingSaveArchive( iSaveCount, sArchive, sCurrent );
		return false;
	}
	CGString sArchive;
	GetBackupName( sArchive, pszBaseDir, pszBaseName[0], iSaveCount );
	if ( !PreserveSaveFile( sCurrent, sArchive ))
	{
		g_Log.Event( LOG_GROUP_SAVE, LOGL_CRIT,
			"Save could not preserve '%s' as '%s'" LOG_CR,
			(LPCTSTR)sCurrent, (LPCTSTR)sArchive );
		return false;
	}
	// Record only a backup that was actually taken; the first generation has
	// no live file to preserve.
	if ( iComponent < 0 || !SaveFileExists( sCurrent ))
		return true;
	if ( !fPendingManifest )
	{
		if ( !fStartManifest )
			return true;
		manifest = CSaveManifest( iSaveCount, true );
	}
	// The backup must be durable in its own directory (the account directory
	// can differ from the world directory) before the manifest names it.
	if ( !SyncSaveDirectory( pszBaseDir ))
	{
		g_Log.Event( LOG_GROUP_SAVE, LOGL_CRIT,
			"Save directory sync FAILED after preserving '%s'" LOG_CR, (LPCTSTR)sCurrent );
		return false;
	}
	manifest.m_dwRotated |= dwBit;
	manifest.m_sArchive[iComponent] = sArchive;
	if ( !WriteSaveManifest( g_Cfg.m_sWorldBaseDir, manifest ))
	{
		g_Log.Event( LOG_GROUP_SAVE, LOGL_CRIT,
			"Save manifest update FAILED after preserving '%s'" LOG_CR, (LPCTSTR)sCurrent );
		return false;
	}
	return true;
}

// Parse a "SAVECOUNT=<n>" header line, matching the key in any case (older
// writers use "SaveCount=").  Like the script parser, the key ends at '=' or
// whitespace.
static bool ParseSaveCountLine( const char* pszLine, int& iSaveCount )
{
	while ( *pszLine == ' ' || *pszLine == '\t' )
		pszLine++;
	static const char sm_szKey[] = "SAVECOUNT";
	const size_t iKeyLen = sizeof(sm_szKey) - 1;
	if ( _strnicmp( pszLine, sm_szKey, iKeyLen ))
		return false;
	pszLine += iKeyLen;
	if ( *pszLine != '=' && *pszLine != ' ' && *pszLine != '\t' )
		return false;
	while ( *pszLine == ' ' || *pszLine == '\t' )
		pszLine++;
	if ( *pszLine == '=' )
		pszLine++;
	return sscanf( pszLine, "%d", &iSaveCount ) == 1;
}

// Read the SAVECOUNT from a save file's header and check that the file ends
// with its [EOF] section.  The header is the key block the writer puts before
// the first section (see s_WriteProps); older writers put the same keys into a
// leading [SPHERE] section instead.  Any later section ends the header, so a
// global variable named SAVECOUNT in [VARNAMES] is never read as the count.
static bool ReadSaveFileCount( LPCTSTR pszPath, int& iSaveCount )
{
	iSaveCount = INT_MIN;
	FILE* pFile = fopen( pszPath, "rb" );
	if ( !pFile )
		return false;
	char szLine[512];
	bool fHeader = true;
	bool fSectionSeen = false;
	bool fEOF = false;
	bool fAfterEOF = false;
	bool fLineStart = true;
	while ( fgets( szLine, sizeof(szLine), pFile ) != NULL )
	{
		// A line longer than the buffer arrives in several pieces; only the
		// first piece starts a line.
		const bool fContinuation = !fLineStart;
		fLineStart = strchr( szLine, '\n' ) != NULL;
		if ( fEOF )
		{
			if ( szLine[0] != '\n' && szLine[0] != '\r' && szLine[0] != '\0' )
				fAfterEOF = true;
			continue;
		}
		if ( fContinuation )
			continue;
		if ( !strncmp( szLine, "[EOF]", 5 ))
		{
			fEOF = true;
			continue;
		}
		if ( szLine[0] == '[' )
		{
			fHeader = !fSectionSeen && !_strnicmp( szLine, "[SPHERE]", 8 );
			fSectionSeen = true;
			continue;
		}
		int iHeaderCount = 0;
		if ( fHeader && ParseSaveCountLine( szLine, iHeaderCount ))
			iSaveCount = iHeaderCount;
	}
	fclose( pFile );
	return fEOF && !fAfterEOF;
}

// Name a selected save backup with its own SAVECOUNT and save time (the file's
// modification time) next to the wall-clock time of the selection; start-up
// log lines carry no timestamp of their own.
static void LogSaveBackupSelected( LPCTSTR pszPath )
{
	int iSaveCount = INT_MIN;
	ReadSaveFileCount( pszPath, iSaveCount );
	CGString sSaved;
	struct stat st;
	if ( pszPath && stat( pszPath, &st ) == 0 )
		sSaved.Copy( CGTime( st.st_mtime ).Format( NULL ));
	else
		sSaved.Copy( "unknown" );
	g_Log.Event( LOG_GROUP_INIT, LOGL_WARN,
		"Loading save backup '%s' SaveCount=%d%s saved=%s time=%s" LOG_CR,
		pszPath, iSaveCount, iSaveCount == INT_MIN ? "(missing)" : "",
		(LPCTSTR)sSaved, (LPCTSTR)CGTime::GetCurrentTime().Format( NULL ));
}

bool CWorld::VerifySaveFile( LPCTSTR pszPath, int iSaveCount )
{
	int iFound = 0;
	return ReadSaveFileCount( pszPath, iFound ) && iFound != INT_MIN &&
		iFound == iSaveCount;
}

bool CWorld::PublishSavePair()
{
	CGString sWorldTemp;
	CGString sCharsTemp;
	GetSaveTempName( sWorldTemp, g_Cfg.m_sWorldBaseDir, "world" );
	GetSaveTempName( sCharsTemp, g_Cfg.m_sWorldBaseDir, "chars" );
	CGString sWorldCurrent;
	CGString sCharsCurrent;
	sWorldCurrent.Format( "%s" SPHERE_FILE "world" SCRIPT_EXT, (LPCTSTR)g_Cfg.m_sWorldBaseDir );
	sCharsCurrent.Format( "%s" SPHERE_FILE "chars" SCRIPT_EXT, (LPCTSTR)g_Cfg.m_sWorldBaseDir );

	if ( !VerifySaveFile( sWorldTemp, m_iSaveCountID ) ||
		!VerifySaveFile( sCharsTemp, m_iSaveCountID ))
	{
		g_Log.Event( LOG_GROUP_SAVE, LOGL_CRIT,
			"Save generation %d failed validation: both files require matching SAVECOUNT and [EOF]" LOG_CR,
			m_iSaveCountID );
		return false;
	}

	// Back up the live pair once per generation (a retry reuses the recorded
	// backups), recording each backup even if the pending manifest is gone.
	if ( !PreserveSaveComponent( g_Cfg.m_sWorldBaseDir, "world", m_iSaveCountID, true ) ||
		!PreserveSaveComponent( g_Cfg.m_sWorldBaseDir, "chars", m_iSaveCountID, true ))
		return false;

	// Each publication syncs the directory, so once both return the pair is
	// the durable current generation and the caller records the commit.
	return PublishSaveFile( sWorldTemp, sWorldCurrent ) &&
		PublishSaveFile( sCharsTemp, sCharsCurrent );
}

bool CWorld::OpenScriptBackup( CScript& s, LPCTSTR pszBaseDir, LPCTSTR pszBaseName, int iSaveCount, bool fRetry )
{
	ASSERT(pszBaseName);

	CGString sSaveName;
	sSaveName.Format( "%s" SPHERE_FILE "%s" SCRIPT_EXT, pszBaseDir, pszBaseName );
	const int iComponent = SaveManifestComponent( pszBaseName );
	CSaveManifest manifest;
	const bool fHaveManifest = iComponent >= 0 &&
		ReadSaveManifest( g_Cfg.m_sWorldBaseDir, manifest ) &&
		manifest.m_fPending && manifest.m_iSaveCount == iSaveCount;
	const unsigned dwBit = iComponent >= 0 ? ( 1u << iComponent ) : 0;

	if ( fRetry && fHaveManifest && ( manifest.m_dwRotated & dwBit ))
	{
		// A retry keeps the backup the failed generation already took, at the
		// path it recorded (a recomputed name can carry a later date).
		CGString sArchive;
		GetManifestArchive( manifest, iComponent, pszBaseDir, pszBaseName[0], iSaveCount, sArchive );
		if ( !SaveFileExists( sArchive ))
		{
			LogMissingSaveArchive( iSaveCount, sArchive, sSaveName );
			return false;
		}
	}
	else
	{
		// If this component was not reached before a failure, rotate it now.
		CGString sArchive;
		GetBackupName( sArchive, pszBaseDir, pszBaseName[0], iSaveCount );
		remove( sArchive );
		const bool fRotated = rename( sSaveName, sArchive ) == 0;
		if ( !fRotated )
		{
			// May not exist if this is the first time.
			g_Log.Event( LOG_GROUP_SAVE, LOGL_WARN, "Rename %s to '%s' FAILED code %d?" LOG_CR, (LPCTSTR) sSaveName, (const TCHAR*) sArchive, CGFile::GetLastError() );
		}
		else if ( !SyncSaveDirectory( pszBaseDir ))
		{
			g_Log.Event( LOG_GROUP_SAVE, LOGL_CRIT,
				"Save directory sync FAILED after rotating '%s'" LOG_CR, (LPCTSTR) pszBaseName );
			return false;
		}
		if ( fHaveManifest && fRotated )
		{
			manifest.m_dwRotated |= dwBit;
			manifest.m_sArchive[iComponent] = sArchive;
			if ( !WriteSaveManifest( g_Cfg.m_sWorldBaseDir, manifest ))
			{
				g_Log.Event( LOG_GROUP_SAVE, LOGL_CRIT, "Save manifest update FAILED after rotating '%s'" LOG_CR, (LPCTSTR) pszBaseName );
				return false;
			}
		}
	}

	if ( ! s.Open( sSaveName, OF_WRITE|OF_TEXT))
	{
		g_Log.Event( LOG_GROUP_SAVE, LOGL_CRIT, "Save '%s' FAILED" LOG_CR, (LPCTSTR) sSaveName );
		return( false );
	}

	return( true );
}

bool CWorld::FailSave( LPCTSTR pszReason )
{
	m_fSaveFailed = true;
	g_Log.Event( LOG_GROUP_SAVE, LOGL_CRIT,
		"Save failed during %s; committed generation remains %d" LOG_CR,
		pszReason ? pszReason : "file I/O", m_iSaveCountID );
	SetAllowUIDReuse();
	m_FileWorld.CloseChecked();
	m_FilePlayers.CloseChecked();
	CGString sTemp;
	GetSaveTempName( sTemp, g_Cfg.m_sWorldBaseDir, "world" );
	remove( sTemp );
	GetSaveTempName( sTemp, g_Cfg.m_sWorldBaseDir, "chars" );
	remove( sTemp );
	m_iSaveStage = INT_MAX;
	return false;
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
	const bool fWriteOK = !s.HasIOError();
	const bool fCloseOK = s.CloseChecked();
	if ( !fWriteOK || !fCloseOK )
		return HRES_INTERNAL_ERROR;
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
			if ( ! OpenScriptBackup( s, g_Cfg.m_sMainLogServerDir, "serv", m_iSaveCountID, IsSaveRetry() ))
				return FailSave( "opening server save" );
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
			if ( !s.CloseChecked())
				return FailSave( "closing server save" );
		}
		break;

	case SECTOR_QTY+2:
		// Now make a backup of the account file.
		if ( !g_Accounts.Account_SaveAll())
			return FailSave( "saving accounts" );
		break;

	case SECTOR_QTY+3:
		// EOF marker to show we reached the end.
		m_FileWorld.WriteSection( "EOF" );
		m_FilePlayers.WriteSection( "EOF" );
		if ( m_FileWorld.HasIOError() || m_FilePlayers.HasIOError())
			return FailSave( "writing EOF" );
		if ( !m_FileWorld.Sync() || !m_FilePlayers.Sync())
			return FailSave( "syncing world files" );
		const bool fWorldCloseOK = m_FileWorld.CloseChecked();
		const bool fPlayersCloseOK = m_FilePlayers.CloseChecked();
		if ( !fWorldCloseOK || !fPlayersCloseOK )
			return FailSave( "closing world files" );

		if ( !PublishSavePair())
			return FailSave( "publishing paired world files" );
		// Record the commit as soon as both files are published: a pending
		// manifest would make the next start prefer the previous backups.
		if ( !WriteSaveManifest( g_Cfg.m_sWorldBaseDir, CSaveManifest( m_iSaveCountID, false )))
			return FailSave( "committing save manifest" );

		CGString sWorldCurrent;
		CGString sCharsCurrent;
		sWorldCurrent.Format( "%s" SPHERE_FILE "world" SCRIPT_EXT, (LPCTSTR)g_Cfg.m_sWorldBaseDir );
		sCharsCurrent.Format( "%s" SPHERE_FILE "chars" SCRIPT_EXT, (LPCTSTR)g_Cfg.m_sWorldBaseDir );
		// Re-read the published pair as a diagnostic only.  The generation is
		// already committed, so a mismatch is reported but neither discards
		// it nor fails the save.
		if ( !VerifySaveFile( sWorldCurrent, m_iSaveCountID ) ||
			!VerifySaveFile( sCharsCurrent, m_iSaveCountID ))
		{
			g_Log.Event( LOG_GROUP_SAVE, LOGL_CRIT,
				"Save generation %d was committed, but re-reading '%s' and '%s' did not find matching SAVECOUNT and [EOF]" LOG_CR,
				m_iSaveCountID, (LPCTSTR)sWorldCurrent, (LPCTSTR)sCharsCurrent );
		}

		const int iCommittedSaveCount = m_iSaveCountID;
		m_iSaveCountID++;	// Save only counts if we get to the end without trapping.
		m_fSaveRetry = false;
		m_timeSave.InitTimeCurrent( g_Cfg.m_iSavePeriod );	// next save time.
		RemoveSaveManifest( g_Cfg.m_sWorldBaseDir );

		const unsigned long long iNowMillis = SaveClockMillis();
		const unsigned long long iDuration = iNowMillis >= m_ullSaveStartMillis ?
			iNowMillis - m_ullSaveStartMillis : 0;
		g_Log.Event( LOG_GROUP_SAVE, LOGL_EVENT,
			"World save ended: SaveCount=%d duration_ms=%llu objects=%d/%d files=%llu/%llu" LOG_CR,
			iCommittedSaveCount, iDuration,
			g_Serv.StatGet( SERV_STAT_ITEMS ), g_Serv.StatGet( SERV_STAT_CHARS ),
			SaveFileSize( sWorldCurrent ), SaveFileSize( sCharsCurrent ));
#ifndef _WIN32
		fprintf( stderr,
			"[INFO] World save ended: SaveCount=%d duration_ms=%llu objects=%d/%d files=%llu/%llu\n",
			iCommittedSaveCount, iDuration,
			g_Serv.StatGet( SERV_STAT_ITEMS ), g_Serv.StatGet( SERV_STAT_CHARS ),
			SaveFileSize( sWorldCurrent ), SaveFileSize( sCharsCurrent ));
#endif
		// The stock event line names the published live file, never the
		// temporary file the generation was written to.
		g_Log.Event( LOG_GROUP_SAVE, LOGL_EVENT, "World data saved (%s)." LOG_CR, (LPCTSTR) sWorldCurrent);

		// Now clean up all the held over UIDs
		SetAllowUIDReuse();
		m_iSaveStage = INT_MAX;
		return( false );	// done.
	}

	if ( m_FileWorld.HasIOError() || m_FilePlayers.HasIOError())
		return FailSave( "writing world files" );

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

	if ( !m_fSaveFailed )
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
		return !m_fSaveFailed;
	}
	CSaveManifest manifest;
	const bool fHasManifest = ReadSaveManifest( g_Cfg.m_sWorldBaseDir, manifest );
	if ( fHasManifest && !manifest.m_fPending )
		RemoveSaveManifest( g_Cfg.m_sWorldBaseDir );
	const bool fRetry = fHasManifest && manifest.m_fPending &&
		manifest.m_iSaveCount == m_iSaveCountID;
	m_fSaveRetry = fRetry;
	m_fSaveFailed = false;
	m_ullSaveStartMillis = SaveClockMillis();
	m_iSaveStartItems = g_Serv.StatGet( SERV_STAT_ITEMS );
	m_iSaveStartChars = g_Serv.StatGet( SERV_STAT_CHARS );
	g_Log.Event( LOG_GROUP_SAVE, LOGL_EVENT,
		"World save started: SaveCount=%d objects=%d/%d" LOG_CR,
		m_iSaveCountID, m_iSaveStartItems, m_iSaveStartChars );
#ifndef _WIN32
	fprintf( stderr, "[INFO] World save started: SaveCount=%d objects=%d/%d\n",
		m_iSaveCountID, m_iSaveStartItems, m_iSaveStartChars );
#endif
	if ( !fRetry && !WriteSaveManifest( g_Cfg.m_sWorldBaseDir,
		CSaveManifest( m_iSaveCountID, true )))
		return FailSave( "opening save manifest" );

	// Do the write async from here in the future.
	if ( g_Cfg.m_fSaveGarbageCollect )
	{
		GarbageCollection();
	}

	// Write both components beside the live pair.  Their current paths remain
	// untouched until both temporary files have reached EOF and passed the
	// matching SAVECOUNT validation in PublishSavePair().
	CGString sWorldTemp;
	CGString sCharsTemp;
	GetSaveTempName( sWorldTemp, g_Cfg.m_sWorldBaseDir, "world" );
	GetSaveTempName( sCharsTemp, g_Cfg.m_sWorldBaseDir, "chars" );
	remove( sWorldTemp );
	remove( sCharsTemp );
	if ( !m_FileWorld.Open( sWorldTemp, OF_WRITE|OF_CREATE|OF_TEXT ))
	{
		return FailSave( "opening world save" );
	}
	if ( !m_FilePlayers.Open( sCharsTemp, OF_WRITE|OF_CREATE|OF_TEXT ))
	{
		return FailSave( "opening character save" );
	}

	m_fSaveParity = ! m_fSaveParity; // Flip the parity of the save.
	m_iSaveStage = -1;
	m_timeSave.InitTime();

	// Write the file headers.
	s_WriteProps( m_FileWorld );
	s_WriteProps( m_FilePlayers );
	if ( m_FileWorld.HasIOError() || m_FilePlayers.HasIOError())
		return FailSave( "writing save headers" );

	if ( fForceImmediate || ! g_Cfg.m_iSaveBackgroundTime )	// Save now !
	{
		SaveForce();
		return !m_fSaveFailed;
	}

	return true;
}

bool CWorld::Save( bool fForceImmediate ) // Save world state
{
	return Save( fForceImmediate, false );
}

bool CWorld::Save( bool fForceImmediate, bool fAllowDamagedWorld ) // Save world state
{
	if ( m_fSaveBlockedByLoad && !fAllowDamagedWorld )
	{
		g_Log.Event( LOG_GROUP_SAVE, LOGL_CRIT,
			"Save refused: the last world load skipped %d sections (%d objects) and had %d failed parses. "
			"Use explicit admin SAVE FORCE only after reviewing the load failure." LOG_CR,
			m_iLoadSkippedSections, m_iLoadSkippedObjects, m_iLoadFailedParses );
		Broadcast( "Save refused: world load was incomplete; use admin SAVE FORCE only after review." );
		return false;
	}
	if ( m_fSaveBlockedByLoad && fAllowDamagedWorld )
	{
		g_Log.Event( LOG_GROUP_SAVE, LOGL_WARN,
			"WARNING: forcing a save after an incomplete world load (%d skipped sections, %d objects, %d failed parses)." LOG_CR,
			m_iLoadSkippedSections, m_iLoadSkippedObjects, m_iLoadFailedParses );
	}

	bool fRet = false;
	try
	{
		fRet = SaveTry(fForceImmediate);
	}
	SPHERE_LOG_TRY_CATCH( "Save FAILED." )

	if ( ! fRet )
	{
		Broadcast( "Save FAILED. " SPHERE_TITLE " is UNSTABLE!" );
		m_FileWorld.CloseChecked();	// close if not already closed.
		m_FilePlayers.CloseChecked();	// close if not already closed.
		// We should probably shut down the server if the save failed
		// so that we don't destroy all of the good saves we have
		g_Serv.SetExitFlag(SPHEREERR_INTERNAL);
	}
	return fRet;
}

/////////////////////////////////////////////////////////////////////

static bool WorldReadSectionSerial( LPCTSTR pszFilePath, CScriptLineContext sectionContext, CGString& sSerial )
{
	CScript serialScript;
	if ( !serialScript.Open( pszFilePath ))
		return( false );

	serialScript.SeekContext( sectionContext );
	bool fFound = serialScript.FindKey( "SERIAL" );
	if ( fFound )
		sSerial.Copy( serialScript.GetArgRaw());
	serialScript.Close();
	return( fFound );
}

bool CWorld::LoadFile( LPCTSTR pszLoadName ) // Load world from script
{
	CScript s;
	if ( ! s.Open( pszLoadName ))
	{
		g_Log.Event( LOG_GROUP_INIT, LOGL_ERROR, "Can't Load %s" LOG_CR, (LPCTSTR) pszLoadName );
		// A missing world/chars/statics file is also unsafe to follow with a
		// save: the missing data would be replaced by an incomplete snapshot.
		MarkLoadIssue( false );
		return( false );
	}

	g_Log.Event( LOG_GROUP_INIT, LOGL_TRACE, "Loading %s..." LOG_CR, (LPCTSTR) pszLoadName );

	// Find the size of the file.
	FILE_POS_TYPE lLoadSize = s.GetLength();
	int iLoadStage = 0;

	CSphereScriptContext ScriptContext( &s );

	// Read the header stuff first.
	while ( s.ReadKeyParse())
	{
		s_PropSet(s.GetKey(),s.GetArgVar());
	}

	while ( s.FindNextSection())
	{
		bool fWorldItem = s.IsSectionType( "WORLDITEM" );
		bool fWorldChar = s.IsSectionType( "WORLDCHAR" );
		bool fObjectSection = fWorldItem || fWorldChar;
		CScriptLineContext sectionContext = s.GetContext();
		CGString sWorldCharType( s.GetArgRaw());
		if ( fWorldItem )
			m_iLoadReadItems++;
		else if ( fWorldChar )
			m_iLoadReadChars++;

		if (! ( ++iLoadStage & 0x1FF ))	// don't update too often
		{
			g_Serv.Event_PrintPercent( SERVTRIG_LoadStatus, s.GetPosition(), lLoadSize );
		}

		bool fSectionLoaded = false;
		bool fWorldCharDefaulted = false;
		CGString sFailureReason;
#if defined(SPHERE_CRASH_RECOVERY_ENABLED)
		bool fRecoveredFault = false;
#endif
		try
		{
#if defined(SPHERE_CRASH_RECOVERY_ENABLED)
			// Set up SEGV recovery point — if a section causes a segfault,
			// we skip it and continue loading the next section.
			extern volatile sig_atomic_t g_fSEGV_catch;
			extern sigjmp_buf g_SEGV_jmpbuf;
			g_fSEGV_catch = 1;
			if ( sigsetjmp(g_SEGV_jmpbuf, 1) != 0 )
			{
				// Returned here from SEGV handler via siglongjmp.
				g_fSEGV_catch = 0;
				fRecoveredFault = true;
			}
			else
#endif
			{
				fSectionLoaded = g_Cfg.LoadScriptSection(
					s,
					fObjectSection ? &sFailureReason : NULL,
					fWorldChar ? &fWorldCharDefaulted : NULL );
			}
#if defined(SPHERE_CRASH_RECOVERY_ENABLED)
			g_fSEGV_catch = 0;
#endif
		}
		catch ( CGException &e )
		{
			if ( fObjectSection )
				sFailureReason.Copy( fWorldItem ? "exception while loading item section" : "exception while loading character section" );
			g_Log.CatchEvent( &e, "Load Exception line %d " SPHERE_TITLE " is UNSTABLE!", s.GetContext().m_iLineNum );
		}
		catch (...)
		{
			if ( fObjectSection )
				sFailureReason.Copy( fWorldItem ? "exception while loading item section" : "exception while loading character section" );
			g_Log.CatchEvent( NULL, "Load Exception line %d " SPHERE_TITLE " is UNSTABLE!", s.GetContext().m_iLineNum );
		}

#if defined(SPHERE_CRASH_RECOVERY_ENABLED)
		if ( fRecoveredFault )
		{
			if ( fObjectSection )
				sFailureReason.Copy( fWorldItem ? "recoverable fault while loading item section" : "recoverable fault while loading character section" );
			if ( fObjectSection )
			{
				CGString sSerial( "unknown" );
				WorldReadSectionSerial( s.GetFilePath(), sectionContext, sSerial );
				if ( ShouldLogLoadDetail( fWorldItem ? LOAD_LOG_WORLDITEM_FAILURE : LOAD_LOG_WORLDCHAR_FAILURE ))
				{
					g_Log.Event( LOG_GROUP_INIT, LOGL_ERROR,
						"%s load failed: uid=%s type='%s' reason=%s" LOG_CR,
						fWorldItem ? "WORLDITEM" : "WORLDCHAR",
						(LPCTSTR) sSerial, (LPCTSTR) sWorldCharType, (LPCTSTR) sFailureReason );
				}
			}
			MarkLoadIssue( fObjectSection );
			continue;
		}
#endif
		if ( fSectionLoaded )
		{
			if ( fWorldItem )
				m_iLoadItems++;
			else if ( fWorldChar )
			{
				m_iLoadChars++;
				if ( fWorldCharDefaulted )
				{
					CGString sSerial( "unknown" );
					WorldReadSectionSerial( s.GetFilePath(), sectionContext, sSerial );
					g_Log.Event( LOG_GROUP_INIT, LOGL_ERROR,
						"WORLDCHAR load fallback: uid=%s type='%s' reason=character type does not resolve to a resource index default=DEFAULTCHAR" LOG_CR,
						(LPCTSTR) sSerial, (LPCTSTR) sWorldCharType );
				}
			}
		}
		else
		{
			if ( fObjectSection )
			{
				if ( sFailureReason.IsEmpty())
					sFailureReason.Copy( fWorldItem ? "item section could not be loaded" : "character section could not be loaded" );
				CGString sSerial( "unknown" );
				WorldReadSectionSerial( s.GetFilePath(), sectionContext, sSerial );
				if ( ShouldLogLoadDetail( fWorldItem ? LOAD_LOG_WORLDITEM_FAILURE : LOAD_LOG_WORLDCHAR_FAILURE ))
				{
					g_Log.Event( LOG_GROUP_INIT, LOGL_ERROR,
						"%s load failed: uid=%s type='%s' reason=%s" LOG_CR,
						fWorldItem ? "WORLDITEM" : "WORLDCHAR",
						(LPCTSTR) sSerial, (LPCTSTR) sWorldCharType, (LPCTSTR) sFailureReason );
				}
			}
			MarkLoadIssue( fObjectSection );
		}
	}

	if ( s.IsSectionType( "EOF" ))
	{
		// The only valid way to end.
		s.Close();
		return( true );
	}

	g_Log.Event( LOG_GROUP_INIT, LOGL_CRIT, "No [EOF] marker. '%s' is corrupt!" LOG_CR, (LPCTSTR) s.GetFilePath());
	MarkLoadIssue( false );
	return( false );
}

#ifdef SPHERE_LOAD_SAFETY_TEST
bool CWorld::LoadFileForTest( LPCTSTR pszName )
{
	ResetLoadIntegrity();
	bool fLoaded = LoadFile( pszName );
	CleanupLoadOrphans();
	CaptureLoadCounts();
	LogLoadDiagnostics();
	ReportLoadIntegrity();
	return fLoaded;
}

bool CWorld::LoadWorldForTest()
{
	Close( false );
	ResetLoadIntegrity();
	SERVMODE_TYPE iModePrv = g_Serv.SetServerMode( SERVMODE_Loading );
	const bool fLoaded = LoadWorld();
	CleanupLoadOrphans();
	g_Serv.SetServerMode( iModePrv );
	return fLoaded;
}
#endif

bool CWorld::LoadWorld() // Load world from script
{
	// Current files are authoritative.  A previous backup is considered only
	// for an interrupted transaction (the pending manifest) or when the
	// operator explicitly enables SaveBackupFallback in the INI.

	CGString sWorldName;
	sWorldName.Format( "%s" SPHERE_FILE "world" SCRIPT_EXT, (LPCTSTR) g_Cfg.m_sWorldBaseDir );
	CGString sCharsName;
	sCharsName.Format( "%s" SPHERE_FILE "chars" SCRIPT_EXT, (LPCTSTR) g_Cfg.m_sWorldBaseDir );
	CSaveManifest manifest;
	const bool fHaveManifest = ReadSaveManifest( g_Cfg.m_sWorldBaseDir, manifest );
	const bool fPendingManifest = fHaveManifest && manifest.m_fPending;
	const int iPendingSaveCount = manifest.m_iSaveCount;
	int iLiveWorldCount = INT_MIN;
	int iLiveCharsCount = INT_MIN;
	// Both live files carrying the pending generation's SAVECOUNT means both
	// were published (each was validated before its rename); only the commit
	// record is missing.  That pair is the newest complete generation.
	const bool fPublishedPending = fPendingManifest &&
		ReadSaveFileCount( sWorldName, iLiveWorldCount ) &&
		ReadSaveFileCount( sCharsName, iLiveCharsCount ) &&
		iLiveWorldCount == iPendingSaveCount && iLiveCharsCount == iPendingSaveCount;
	if ( fHaveManifest && !fPendingManifest )
	{
		RemoveSaveManifest( g_Cfg.m_sWorldBaseDir );
	}
	else if ( fPublishedPending )
	{
		m_iSaveCountID = iPendingSaveCount;
		g_Log.Event( LOG_GROUP_INIT, LOGL_WARN,
			"Save generation %d was published before its commit was recorded; loading the published pair" LOG_CR,
			iPendingSaveCount );
	}
	else if ( fPendingManifest )
	{
		// A pending marker means the active files may be incomplete. Prefer the
		// matching archives, falling back to an active component that was not
		// reached before the failed save.
		m_iSaveCountID = iPendingSaveCount;
		CGString sArchive;
		GetManifestArchive( manifest, CSaveManifest::COMPONENT_WORLD, g_Cfg.m_sWorldBaseDir,
			'w', iPendingSaveCount, sArchive );
		if (( manifest.m_dwRotated & ( 1u << CSaveManifest::COMPONENT_WORLD )) && SaveFileExists( sArchive ))
		{
			sWorldName = sArchive;
			LogSaveBackupSelected( sWorldName );
		}
		GetManifestArchive( manifest, CSaveManifest::COMPONENT_CHARS, g_Cfg.m_sWorldBaseDir,
			'c', iPendingSaveCount, sArchive );
		if (( manifest.m_dwRotated & ( 1u << CSaveManifest::COMPONENT_CHARS )) && SaveFileExists( sArchive ))
		{
			sCharsName = sArchive;
			LogSaveBackupSelected( sCharsName );
		}
	}

	const bool fAllowBackupFallback = fPendingManifest || g_Cfg.m_fSaveBackupFallback;
	int iPrevSaveCount = m_iSaveCountID;
	for(;;)
	{
		// If this save needs to fall back to an older backup, report only the
		// sections read from the save that actually loaded.
		m_iLoadReadItems = 0;
		m_iLoadReadChars = 0;
		m_iLoadItems = 0;
		m_iLoadChars = 0;
		int iWorldSaveCount = 0;
		int iCharsSaveCount = 0;
		const bool fWorldReadable = ReadSaveFileCount( sWorldName, iWorldSaveCount );
		const bool fCharsReadable = ReadSaveFileCount( sCharsName, iCharsSaveCount );
		const bool fWorldHasCount = iWorldSaveCount != INT_MIN;
		const bool fCharsHasCount = iCharsSaveCount != INT_MIN;
		const bool fMatchingPair = ! ( fWorldHasCount || fCharsHasCount ) ||
			( fWorldReadable && fCharsReadable && fWorldHasCount && fCharsHasCount &&
				iWorldSaveCount == iCharsSaveCount );
		if ( !fMatchingPair )
		{
			g_Log.Event( LOG_GROUP_INIT, LOGL_FATAL,
				"World/chars save pair mismatch: world=%d%s chars=%d%s" LOG_CR,
				iWorldSaveCount, iWorldSaveCount == INT_MIN ? "(missing)" : "",
				iCharsSaveCount, iCharsSaveCount == INT_MIN ? "(missing)" : "" );
			if ( !fPendingManifest && iWorldSaveCount != INT_MIN )
				m_iSaveCountID = iWorldSaveCount;
		}
		const bool fWorldLoaded = fMatchingPair && LoadFile( sWorldName );
		const bool fCharsLoaded = fWorldLoaded && LoadFile( sCharsName );
		if ( fWorldLoaded && fCharsLoaded )
		{
			if ( fPendingManifest )
				m_iSaveCountID = iPendingSaveCount;
			// The published generation is now the loaded one: drop its stale
			// pending record, so the next save backs this pair up as a new
			// generation instead of retrying over it.
			if ( fPublishedPending )
				RemoveSaveManifest( g_Cfg.m_sWorldBaseDir );
			return( true );
		}

		if ( !fAllowBackupFallback )
			break;

		// If we could not open the file at all then it was a bust.  Do not walk
		// backups unless the manifest or the explicit INI option authorizes it.
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
		LogSaveBackupSelected( sWorldName );

		GetBackupName( sArchive, g_Cfg.m_sWorldBaseDir, 'c', m_iSaveCountID );
		if ( ! sArchive.CompareNoCase( sCharsName ))	// ! same file ? break endless loop.
		{
			break;
		}
		sCharsName = sArchive;
		LogSaveBackupSelected( sCharsName );
	}

	g_Log.Event( LOG_GROUP_INIT, LOGL_FATAL, "No previous backup available ?" LOG_CR );
	return( false );
}

bool CWorld::LoadAll( LPCTSTR pszLoadName ) // Load world from script
{
	// The UID table reserves slot zero, so its size does not mean the world is loaded.
	if ( m_fLoadCountsCaptured )	// a successful load already completed?
		return( true );

	ResetLoadIntegrity();

	g_Serv.OnTriggerEvent( SERVTRIG_LoadBegin );
	DEBUG_CHECK( g_Serv.IsLoading());

	// The world has just started.
	m_Clock.InitTime();		// will be loaded from the world file.

	// Load all the accounts.
	if ( ! g_Accounts.Account_LoadAll( false ))
	{
		return( false );
	}

	// If we are the master list. Then read the list from a sep file.
	if ( ! g_Cfg.m_sMainLogServerDir.IsEmpty())
	{
		// NOTE: Load this last because the timers are relative to the timers in the world file.
		CGString sLoadName;
		sLoadName.Format( "%s" SPHERE_FILE "serv", (LPCTSTR) g_Cfg.m_sMainLogServerDir );
		LoadFile( sLoadName );
	}

	// Try to load the world and chars files .
	if ( pszLoadName )
	{
		// Command line load this file. g_Cfg.m_sWorldBaseDir
		m_iLoadReadItems = 0;
		m_iLoadReadChars = 0;
		if ( ! LoadFile( pszLoadName ))
		{
			ReportLoadIntegrity();
			return( false );
		}
	}
	else
	{
		if ( ! LoadWorld())
		{
			ReportLoadIntegrity();
			return( false );
		}
	}

	// Load static items into the world. WORLDSTATICS=
	if ( ! g_Cfg.m_sWorldStatics.IsEmpty())
	{
		LoadFile( g_Cfg.m_sWorldStatics );
	}

	ResolveLoadContainers();

	ReportLoadIntegrity();

	m_timeStartup.InitTimeCurrent();
	m_timeSave.InitTimeCurrent( g_Cfg.m_iSavePeriod );	// next save time.

	// Set all the sector light levels now that we know the time.
	// This should not look like part of the load. (CCharDef::T_EnvironChange triggers should run)
	for ( int j=0; j<SECTOR_QTY; j++ )
	{
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

	CleanupLoadOrphans();

	// Set the current version now.
	const TCHAR* pszVersion = SPHERE_VERSION;
	m_iLoadVersion = Exp_GetComplex( pszVersion );	// Set m_iLoadVersion
	CaptureLoadCounts();
	LogLoadDiagnostics();
	g_Serv.OnTriggerEvent( SERVTRIG_LoadDone );

	return( true );
}

void CWorld::ResolveLoadContainers()
{
	// A save normally writes parents before children, but older or interrupted
	// saves can reverse that order. Retry deferred CONT links after every section
	// has been constructed, allowing nested chains to settle in bounded passes.
	const int iMaxPasses = static_cast<int>(GetUIDCount());
	for ( int iPass = 0; iPass < iMaxPasses; ++iPass )
	{
		bool fProgress = false;
		for ( int i = 1; i < static_cast<int>(GetUIDCount()); ++i )
		{
			CItem* pItem = PTR_CAST(CItem, FindUIDObj(i));
			if ( pItem == NULL || ! pItem->HasPendingLoadContainer())
				continue;
			if ( pItem->ResolveLoadContainer())
				fProgress = true;
		}
		if ( ! fProgress )
			break;
	}
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
	{
		// Save headers use the 0.99 dotted version form (for example
		// "0.99u"), sometimes wrapped in quotes.  GetInt() stops at the dot
		// and turns that into zero,
		// which incorrectly enables every pre-0.99 migration while loading.
		// Use the Sphere expression parser so dotted 0.99 versions retain
		// their numeric 99 value and current-format properties are accepted.
		LPCTSTR pszVersion = vVal.GetPSTR();
		while ( pszVersion && isspace( (unsigned char)*pszVersion ))
			pszVersion++;
		if ( pszVersion && *pszVersion == '"' )
			pszVersion++;
		m_iLoadVersion = Exp_GetComplex( pszVersion );
		break;
	}
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
		g_Log.Event( LOG_GROUP_SAVE, LOGL_EVENT,
			"Shutdown is completing active save SaveCount=%d" LOG_CR, m_iSaveCountID );
		Save( true );
	}
	m_GuildStones.RemoveAll();
	m_TownStones.RemoveAll();
	m_Parties.DeleteAll();
	m_PartiesPendingDelete.DeleteAll();
	m_GMPages.DeleteAll();

	for ( int i = 0; i<SECTOR_QTY; i++ )
	{
		m_Sectors[i].Close(fResources);
	}

	CloseAllUIDs();
	m_fLoadCountsCaptured = false;

	m_Clock.InitTime();	// no more sense of time.
}

void CWorld::QueuePartyForDelete( CPartyDef* pParty )
{
	if ( pParty == NULL || m_PartiesPendingDelete.IsMyChild( pParty ))
		return;

	// A party can only belong to one intrusive list.  Detach it from the
	// active registry before retaining it for the end-of-tick deletion pass.
	pParty->RemoveSelf();
	m_PartiesPendingDelete.InsertTail( pParty );
}

void CWorld::DestroyPendingParties()
{
	// DeleteAll() runs the complete CPartyDef destructor while no client or
	// sector callback from the current tick can still hold its raw pointer.
	m_PartiesPendingDelete.DeleteAll();
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

void CWorld::CaptureResourceIntegrityBaseline()
{
	m_iIntegrityResourceDefNames = static_cast<int>(g_Cfg.m_Const.GetCount());
	m_iIntegrityResourceDialogs = 0;
	m_iIntegrityResourceFunctions = 0;
	m_ridIntegrityResourceDefName.InitUID();
	m_ridIntegrityResourceDialog.InitUID();
	m_ridIntegrityResourceFunction.InitUID();
	m_sIntegrityResourceDefName.Empty();
	m_sIntegrityResourceDialog.Empty();
	m_sIntegrityResourceFunction.Empty();

	for ( size_t i = 0; i < g_Cfg.m_Const.GetCount(); ++i )
	{
		CVarDef* pVar = g_Cfg.m_Const.GetAt(i);
		if ( pVar == NULL )
			continue;
		CSphereUID rid( pVar->GetDWORD());
		if ( rid.IsValidRID() && g_Cfg.ResourceGetDef( rid ) != NULL )
		{
			m_ridIntegrityResourceDefName = rid;
			m_sIntegrityResourceDefName = pVar->GetKey();
			break;
		}
	}

	for ( int i = 0; i < static_cast<int>(g_Cfg.m_ResHash.GetCount()); ++i )
	{
		CResourceDefPtr pResDef = g_Cfg.m_ResHash.GetAt(i);
		if ( pResDef == NULL )
			continue;
		CSphereUID rid = pResDef->GetUIDIndex();
		if ( rid.GetResType() == RES_Dialog && rid.GetResPage() == 0 )
		{
			++m_iIntegrityResourceDialogs;
			if ( !m_ridIntegrityResourceDialog.IsValidRID())
			{
				m_ridIntegrityResourceDialog = rid;
				m_sIntegrityResourceDialog = pResDef->GetResourceName();
			}
		}
		else if ( rid.GetResType() == RES_Function && rid.GetResPage() == 0 )
		{
			++m_iIntegrityResourceFunctions;
			if ( !m_ridIntegrityResourceFunction.IsValidRID())
			{
				m_ridIntegrityResourceFunction = rid;
				m_sIntegrityResourceFunction = pResDef->GetResourceName();
			}
		}
	}
	m_fIntegrityResourceBaseline = true;
}

int CWorld::CheckResourceIntegrity( int& iLogBudget )
{
	if ( !m_fIntegrityResourceBaseline )
		CaptureResourceIntegrityBaseline();

	int iDialogs = 0;
	int iFunctions = 0;
	for ( int i = 0; i < static_cast<int>(g_Cfg.m_ResHash.GetCount()); ++i )
	{
		CResourceDefPtr pResDef = g_Cfg.m_ResHash.GetAt(i);
		if ( pResDef == NULL )
			continue;
		CSphereUID rid = pResDef->GetUIDIndex();
		if ( rid.GetResPage() != 0 )
			continue;
		if ( rid.GetResType() == RES_Dialog )
			++iDialogs;
		else if ( rid.GetResType() == RES_Function )
			++iFunctions;
	}

	int iViolations = 0;
	if ( static_cast<int>(g_Cfg.m_Const.GetCount()) < m_iIntegrityResourceDefNames )
	{
		char szDetail[96];
		snprintf( szDetail, sizeof(szDetail), "count=%d baseline=%d",
			static_cast<int>(g_Cfg.m_Const.GetCount()), m_iIntegrityResourceDefNames );
		iViolations += IntegrityViolation( "resource_defnames", NULL, szDetail, iLogBudget );
	}
	if ( iDialogs < m_iIntegrityResourceDialogs )
	{
		char szDetail[96];
		snprintf( szDetail, sizeof(szDetail), "count=%d baseline=%d", iDialogs, m_iIntegrityResourceDialogs );
		iViolations += IntegrityViolation( "resource_dialogs", NULL, szDetail, iLogBudget );
	}
	if ( iFunctions < m_iIntegrityResourceFunctions )
	{
		char szDetail[96];
		snprintf( szDetail, sizeof(szDetail), "count=%d baseline=%d", iFunctions, m_iIntegrityResourceFunctions );
		iViolations += IntegrityViolation( "resource_functions", NULL, szDetail, iLogBudget );
	}

	const CSphereUID aKnown[] = {
		m_ridIntegrityResourceDefName,
		m_ridIntegrityResourceDialog,
		m_ridIntegrityResourceFunction,
	};
	const CGString* aKnownNames[] = {
		&m_sIntegrityResourceDefName,
		&m_sIntegrityResourceDialog,
		&m_sIntegrityResourceFunction,
	};
	for ( size_t i = 0; i < sizeof(aKnown) / sizeof(aKnown[0]); ++i )
	{
		const CSphereUID& rid = aKnown[i];
		if ( rid.IsValidRID() && g_Cfg.ResourceGetDef( rid ) == NULL )
		{
			char szDetail[128];
			snprintf( szDetail, sizeof(szDetail), "rid=0x%x name=%s",
				static_cast<unsigned>(rid), (LPCTSTR)*aKnownNames[i] );
			iViolations += IntegrityViolation( "resource_unresolved", NULL, szDetail, iLogBudget );
		}
	}
	return iViolations;
}

int CWorld::CheckIntegrity( int iBudget )
{
	const DWORD dwUIDCount = GetUIDCount();
	const bool fComplete = iBudget <= 0;
	const bool fNewCycle = fComplete || m_iIntegrityCycleStartMs == 0;
	if ( fNewCycle )
	{
		m_iIntegrityCycleChanges = CObjBase::sm_iChangeCount;
		m_iIntegrityCycleObjects = 0;
		m_iIntegrityCycleSlots = 0;
		m_iIntegrityCycleViolations = 0;
		m_iIntegrityLogBudget = INTEGRITY_LOG_BUDGET;
		m_iIntegrityCycleStartMs = IntegrityNowMs();
		if ( !m_fIntegrityResourceBaseline )
			CaptureResourceIntegrityBaseline();
	}
	if ( dwUIDCount <= 1 )
	{
		const int iResourceViolations = CheckResourceIntegrity( m_iIntegrityLogBudget );
		m_iIntegrityCycleViolations += iResourceViolations;
		m_iIntegrityLastLogs = INTEGRITY_LOG_BUDGET - m_iIntegrityLogBudget;
		g_Log.Event( LOG_GROUP_INIT, LOGL_EVENT,
			"integrity watchdog cycle slots=0 objects=0 violations=%d elapsed_ms=0 logs=%d" LOG_CR,
			m_iIntegrityCycleViolations,
			m_iIntegrityLastLogs );
		m_iIntegrityCursor = 1;
		m_iIntegrityCycleObjects = 0;
		m_iIntegrityCycleSlots = 0;
		m_iIntegrityCycleViolations = 0;
		m_iIntegrityCycleStartMs = 0;
		return iResourceViolations;
	}

	const DWORD dwStart = fComplete ? 1 :
		( m_iIntegrityCursor > 0 && static_cast<DWORD>(m_iIntegrityCursor) < dwUIDCount ?
			static_cast<DWORD>(m_iIntegrityCursor) : 1 );
	const DWORD dwRequestedEnd = dwStart + static_cast<DWORD>(iBudget);
	const DWORD dwEnd = fComplete || dwRequestedEnd > dwUIDCount ? dwUIDCount : dwRequestedEnd;
	int iWorkBudget = fComplete ? -1 : iBudget;
	int iViolations = 0;
	m_iIntegrityCycleSlots += static_cast<int>(dwEnd - dwStart);
	for ( DWORD i = dwStart; i < dwEnd; ++i )
	{
		CResourceObj* pRaw = FindUIDObj(i);
		if ( pRaw != NULL )
		{
			CObjBase* pObj = dynamic_cast<CObjBase*>(pRaw);
			if ( pObj == NULL )
			{
				char szChain[32];
				snprintf( szChain, sizeof(szChain), "slot=0x%x", static_cast<unsigned>(i));
				iViolations += IntegrityViolation( "uid_slot_type", NULL, szChain, m_iIntegrityLogBudget );
			}
			else
			{
				if ( ! fComplete )
				{
					if ( iWorkBudget > 0 )
						--iWorkBudget;
				}
				iViolations += CheckIntegrityObject( this, pObj, i, iWorkBudget, m_iIntegrityLogBudget );
				m_iIntegrityCycleObjects++;
			}
		}
	}

	m_iIntegrityCycleViolations += iViolations;
	if ( fComplete || dwEnd >= dwUIDCount )
	{
		// A live world can create and destroy objects while the UID table is
		// being scanned.  Only compare the accumulated slot count when the
		// creation/destruction counter stayed unchanged for the whole cycle.
		if ( CObjBase::sm_iChangeCount == m_iIntegrityCycleChanges )
		{
			const int iExpectedObjects = m_iIntegrityCycleObjects + m_ObjDelete.GetCount();
			if ( iExpectedObjects != CObjBase::sm_iCount )
			{
				char szCount[64];
				snprintf( szCount, sizeof(szCount), "uid=%d objects=%d", iExpectedObjects, CObjBase::sm_iCount );
				const int iCountViolation = IntegrityViolation( "count_drift", NULL, szCount, m_iIntegrityLogBudget );
				iViolations += iCountViolation;
				m_iIntegrityCycleViolations += iCountViolation;
			}
		}
		const int iResourceViolations = CheckResourceIntegrity( m_iIntegrityLogBudget );
		iViolations += iResourceViolations;
		m_iIntegrityCycleViolations += iResourceViolations;
		const long long iElapsedMs = IntegrityNowMs() - m_iIntegrityCycleStartMs;
		m_iIntegrityLastLogs = INTEGRITY_LOG_BUDGET - m_iIntegrityLogBudget;
		g_Log.Event( LOG_GROUP_INIT, LOGL_EVENT,
			"integrity watchdog cycle slots=%d objects=%d violations=%d elapsed_ms=%lld logs=%d" LOG_CR,
			m_iIntegrityCycleSlots, m_iIntegrityCycleObjects,
			m_iIntegrityCycleViolations, iElapsedMs >= 0 ? iElapsedMs : 0,
			m_iIntegrityLastLogs );
		m_iIntegrityCursor = 1;
		m_iIntegrityCycleObjects = 0;
		m_iIntegrityCycleSlots = 0;
		m_iIntegrityCycleViolations = 0;
		m_iIntegrityCycleStartMs = 0;
	}
	else
	{
		m_iIntegrityCursor = static_cast<int>(dwEnd);
	}
	return iViolations;
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
			try
			{
				m_Sectors[i].OnTick( m_Sector_Pulse );
			}
			catch (...)
			{
				g_Log.Event( LOG_GROUP_DEBUG, LOGL_ERROR, "Exception in Sector %d OnTick" LOG_CR, i );
			}
		}

		g_Serv.m_Profile.SwitchTask( PROFILE_Debug );
		GarbageCollection_New();	// clean up our delete list.
		CheckIntegrity( 256 );
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

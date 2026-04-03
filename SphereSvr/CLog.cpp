//
// CLog.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"
// #include "clogbase.h"

///////////////////////////////////////////////////////////////
// -CLog

CLog::CLog()
{
	// IncRefCount();	// static singleton (not applicable here)

	m_fLockOpen = false;
	SetLogLevel( LOGL_EVENT );
	SetLogGroupMask( LOG_GROUP_INIT | LOG_GROUP_CLIENTS | LOG_GROUP_GM_PAGE );
	SetFilePath( SPHERE_FILE "log.log" );	// default name to go to.
}

CLog::~CLog()
{
	// StaticDestruct(); // not applicable here
}

bool CLog::OpenLog( LPCTSTR pszBaseDirName )	// name set previously.
{
	if ( m_fLockOpen )	// the log is already locked open
		return( false );

	if ( pszBaseDirName != NULL )
	{
		if ( pszBaseDirName[0] == '\0' )
		{
			Close();
			return false;
		}
		else
		{
			m_sBaseDir = pszBaseDirName;
		}
	}

	// Get the new name based on date.
	CGTime datetime;
	datetime.InitTimeCurrent();
	struct tm* pDateTm = datetime.GetLocalTm();

	// Watch for day change (so we can change the name)
	m_iDayStamp = pDateTm->tm_mday;	

	CGString sName;
	sName.Format( SPHERE_FILE "%d-%02d-%02d.log",
		pDateTm->tm_year+1900, pDateTm->tm_mon+1, m_iDayStamp );

	CGString sFileName = GetMergedFileName( m_sBaseDir, sName );

	// Use the OF_READWRITE to append to an existing file.
	return( CFileText::Open( sFileName, OF_SHARE_DENY_NONE|OF_READWRITE|OF_TEXT ));
}

void CLog::EventStrPrint( int iColorType, LPCTSTR pszMsg )
{
	// Print to the main console and all admin telnets
	// ARGS:
	//  iColorType = 0 = body of text.
	//  

#ifdef _DEBUG
#ifdef _WIN32
	OutputDebugString(pszMsg );
#endif
#endif

	// Write out to log file.
#ifdef _WIN32
	WriteString( pszMsg );
#endif

	// print to all client consoles.
	g_Serv.Event_PrintClient( pszMsg );	// echo out to admin telnets.

	// Send event to the external monitors.
	g_Serv.OnTriggerEvent( SERVTRIG_ServerMsg, (DWORD) pszMsg, iColorType );
}

int CLog::EventStr( LOG_GROUP_TYPE dwGroupMask, LOGL_TYPE level, LPCTSTR pszMsg )
{
	if ( ! IsLogged( dwGroupMask, level ))
		return( 0 );
	if ( pszMsg == NULL || *pszMsg == '\0' )
		return( 0 );

#ifndef _WIN32
	// Linux: write to stderr. The CFileText open/close per-line cycle is
	// broken on Linux (causes SEGV). All critical messages also go through
	// SPHERE_LOG_* macros to stderr.
	fprintf(stderr, "%s", pszMsg);
	fflush(stderr);
	try { g_Serv.Event_PrintClient( pszMsg ); } catch (...) {}
	return 1;
#else
	int iRet = 0;
	try
	{
		CThreadLockPtr lock(this);
		ASSERT( pszMsg && *pszMsg );

		CGTime datetime;
		datetime.InitTimeCurrent();
		struct tm* pDateTm = datetime.GetLocalTm();

		if ( pDateTm->tm_mday != m_iDayStamp )
		{
			Close();
			OpenLog(NULL);
			WriteString( datetime.Format(CTIME_FORMAT_DEFAULT));
			WriteString( LOG_CR );
			m_iDayStamp = pDateTm->tm_mday;
		}

		TCHAR szTime[ 32 ];
		sprintf( szTime, "%02d:%02d:", pDateTm->tm_hour, pDateTm->tm_min );

		LPCTSTR pszLabel = NULL;
		switch (level)
		{
		case LOGL_FATAL:  pszLabel = "FATAL:"; break;
		case LOGL_CRIT:   pszLabel = "CRITICAL:"; break;
		case LOGL_ERROR:  pszLabel = "ERROR:"; break;
		case LOGL_WARN:   pszLabel = "WARNING:"; break;
		}

		TCHAR szScriptContext[ 260 + 16 ];
		CSphereThread* pThread = CSphereThread::GetCurrentThread();
		if ( pThread && pThread->m_pScriptContext )
		{
			CScriptLineContext LineContext = pThread->m_pScriptContext->GetContext();
			sprintf( szScriptContext, "(%s,%d)", (LPCTSTR) pThread->m_pScriptContext->GetFileTitle(), LineContext.m_iLineNum );
		}
		else
		{
			szScriptContext[0] = '\0';
		}

		if ( ! ( dwGroupMask & LOG_GROUP_INIT ) && ! g_Serv.IsLoading())
			EventStrPrint( 1, szTime );
		if ( pszLabel )
			EventStrPrint( 2, pszLabel );
		if ( szScriptContext[0] )
			EventStrPrint( 3, szScriptContext );
		EventStrPrint( 0, pszMsg );
		Flush();
		iRet = 1;
	}
	catch (...)
	{
		iRet = 0;
	}
	return( iRet );
#endif
}

CGTime CLog::sm_prevCatchTick;

void _cdecl CLog::CatchEvent( CGException * pErr, LPCTSTR pszCatchContext, ... )
{
	CGTime datetime;
	datetime.InitTimeCurrent();
	if ( sm_prevCatchTick.GetTime() == datetime.GetTime() )	// prevent message floods.
		return;
	// Keep a record of what we catch.
	try
	{
		TCHAR szMsg[512];
		LOGL_TYPE eSeverity;
		int iLen = 0;
		if ( pErr )
		{
			eSeverity = pErr->GetSeverity();
			pErr->GetErrorMessage( szMsg, sizeof(szMsg));
			iLen = strlen(szMsg);
		}
		else
		{
			eSeverity = LOGL_CRIT;
			iLen = sprintf( szMsg, "Unknown Exception", CServTime::GetCurrentTime());
		}

		iLen += sprintf( szMsg+iLen, ", in " );

		va_list vargs;
		va_start(vargs, pszCatchContext);

		iLen += vsprintf( szMsg+iLen, pszCatchContext, vargs );
		iLen += sprintf( szMsg+iLen, LOG_CR );

		Event( LOG_GROUP_DEBUG, eSeverity, szMsg );
		va_end(vargs);
	}
	catch(...)
	{
		// Not much we can do about this.
		pErr = NULL;
	}
	sm_prevCatchTick = datetime;
}

#if 0
void CLog::Dump( const BYTE * pData, int len )
{
	// Just dump a bunch of bytes. 16 bytes per line.
	while ( len )
	{
		TCHAR szTmp[16*3+10];
		int j=0;
		for ( ; j < 16; j++ )
		{
			if ( ! len )
				break;
			sprintf( szTmp+(j*3), "%02x ", * pData );
			len --;
			pData ++;
		}
		strcpy( szTmp+(j*3), LOG_CR );
		// g_Serv.Event_PrintStr( 4, szTmp );
		WriteString( szTmp );	// Print to log file.
	}
}
#endif


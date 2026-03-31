//
// CBackTask.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
// Background task stuff.
//

#include "stdafx.h"	// predef header.

///////////////////////////////////////////////////////////////////
// -CBackTask

bool CBackTask::RegisterServer( bool fFirst, CSocketNamedAddr& addr, CGString& sResult )
{
	// NOTE: This is a synchronous function that could take a while.
	// Register myself with the central login server.

	// don't register to myself.
	if ( ! g_Cfg.m_sMainLogServerDir.IsEmpty() )
	{
		sResult = _TEXT( "This is the list server" );
		return( false );
	}

	// Find the address for the main login server and register.
	if ( addr.IsEmptyHost())
	{
		sResult = _TEXT( "No list server configured" );
		return false;
	}

	if ( fFirst || ! addr.IsValidAddr() || ! Calc_GetRandVal( 20 ))
	{
		// Only do the host lookup one time.
		bool fRet = addr.UpdateFromHostName(); 
		if ( ! fRet )	// can't resolve the name to an address.
		{
			// Might just try the address we already have ?
			sResult.Format( _TEXT( "Can't resolve list server '%s' DNS entry. Code=%d"), (LPCTSTR) addr.GetHostName(), CGSocket::GetLastError());
			addr.EmptyAddr();
			return false;
		}

		fFirst = true;
	}

	// Tell the registration server that we exist.

	CGSocket sock;
	if ( ! sock.Socket())
	{
		sResult = _TEXT( "Failed Create Socket" );
		return( false );
	}
	if ( ! sock.ConnectAddr( addr ))
	{
		sResult.Format( _TEXT( "Failed Connect Code %d"), sock.GetLastError());
		addr.EmptyAddr();	// try to look it up again later.
		return false;
	}

	// ? need to wait a certain amount of time for the connection to setle out here.

	// got a connection. Send reg command.
	TCHAR szData[ CSTRING_MAX_LEN ];
	memset( szData, 0xFF, 4 );	// special registration header.
	szData[4] = 0;
	int iLen = strcpylen( &szData[5], g_Serv.GetStatusStringRegister(fFirst), sizeof( szData )-5);
	iLen += 5;
	int iLenRet = sock.Send( szData, iLen );
	if ( iLenRet != iLen )
	{
		// WSAENOTSOCK
		sResult.Format( _TEXT( "Failed Send Code %d"), sock.GetLastError());
		DEBUG_ERR(( "%d, can't send to reg server '%s'" LOG_CR, sock.GetLastError(), (LPCTSTR) addr.GetHostName() ));
		return false;
	}

	sResult.Format( _TEXT( "OK" ));

#if 0
	// Get something back from the list server ?
	// get back the REGRES_TYPE code.
#endif

	// reset the max clients.
	g_Serv.ClientsAvgReset();
	return( true );
}

void CBackTask::PollServers()
{
	// Poll all the servers in my list to see if they are alive.
	// knock them off the list if they have not been alive for a while.

	int iTotalAccounts = 0;
	int iTotalClients = 0;
	for ( int i=0; ! g_Serv.m_iExitFlag; i++ )
	{
		CServerLock pServ = g_Cfg.Server_GetDef(i);
		if ( pServ == NULL )
			break;

		if ( g_Cfg.m_sMainLogServerDir.IsEmpty())	// the main server does not do this because too many people use dial ups .
		{
			if ( pServ->PollStatus())
				continue;
		}
		if ( pServ->GetConnectStatus() >= 3 )
		{
			g_Log.Event( LOG_GROUP_ACCOUNTS, LOGL_EVENT, "Dropping Server '%s', ip=%s, age=%d, from the list." LOG_CR,
				(LPCTSTR) pServ->GetName(),
				(LPCTSTR) pServ->m_ip.GetAddrStr(), pServ->GetAgeHours());
			g_Cfg.m_Servers.RemoveArg(pServ);
			i--;
			continue;
		}

		pServ->QueueCharToServer();	// Check if the mover task is running ? or needs to.

		if ( pServ->GetConnectStatus() <= 2 )
		{
			// Only count this if it has responded recently.
			DEBUG_CHECK(pServ->GetClientsAvg()>=0);
			iTotalClients += pServ->GetClientsAvg();
			iTotalAccounts += pServ->StatGet(SERV_STAT_ACCOUNTS);
		}
	}

	m_iTotalPolledClients = iTotalClients;
	m_iTotalPolledAccounts = iTotalAccounts;
}

void CBackTask::EntryTask()
{
	// Do all the background stuff we don't want to bog down the game server thread.
	// None of this is time critical.

	InitInstance();
	while ( g_Cfg.CanRunBackTask())
	{
		try
		{
			// register the server every few hours or so.
			if ( ! g_Cfg.m_RegisterServer.IsEmptyHost() &&
				CServTime::GetCurrentTime() >= m_timeNextRegister )
			{
				// g_Log.Event( LOG_GROUP_DEBUG, LOGL_EVENT, "Attempting to register server" LOG_CR);
				RegisterServer( ! m_timeNextRegister.IsTimeValid(), g_Cfg.m_RegisterServer, m_sRegisterResult );
				m_timeNextRegister.InitTimeCurrent( 60*60*TICKS_PER_SEC );
			}

			if ( ! g_Cfg.m_RegisterServer2.IsEmptyHost() &&
				CServTime::GetCurrentTime() >= m_timeNextRegister2 )
			{
				// g_Log.Event( LOG_GROUP_DEBUG, LOGL_EVENT, "Attempting to register server 2" LOG_CR);
				RegisterServer( ! m_timeNextRegister2.IsTimeValid(), g_Cfg.m_RegisterServer2, m_sRegisterResult2 );
				m_timeNextRegister2.InitTimeCurrent( 60*60*TICKS_PER_SEC );
			}

			// ping the monitored servers.
			if ( g_Cfg.m_iPollServers &&
				CServTime::GetCurrentTime() >= m_timeNextPoll )
			{
				// g_Log.Event( LOG_GROUP_DEBUG, LOGL_EVENT, "Attempting to poll servers" LOG_CR);
				PollServers();
				m_timeNextPoll.InitTimeCurrent( g_Cfg.m_iPollServers );
			}

			// Check for outgoing mail.
			if ( CServTime::GetCurrentTime() >= m_timeNextMail )
			{
				// g_Log.Event( LOG_GROUP_DEBUG, LOGL_EVENT, "Attempting to send mail" LOG_CR);
				g_Accounts.OnTick();
				m_timeNextMail.InitTimeCurrent( 60 * TICKS_PER_SEC );
			}
		}
		SPHERE_LOG_TRY_CATCH( "Back Task" )

		// ? watch for incoming connections and data ???.
		Sleep( 5 * 1000 );
	}

	// Tell the world we are closed. CloseHandle() required ?
	g_Log.Event(LOG_GROUP_INIT, LOGL_EVENT, "Closing background threads" LOG_CR);

#ifdef _WIN32
	ExitInstance();
#else
	return; // thread exits naturally
#endif
}

THREAD_ENTRY_RET _cdecl CBackTask::EntryProc( void* lpThreadParameter ) // static
{
	g_BackTask.EntryTask();
}

void CBackTask::CreateThread()
{
	// beginthread() and endthread() are the portable versions of this !

#ifdef _WIN32
	if ( ! g_Cfg.CanRunBackTask())
	{
		// don't bother with the background stuff.
		return;
	}

	g_Log.Event( LOG_GROUP_DEBUG, LOGL_EVENT, "Starting background thread" LOG_CR);
	CThread::CreateThread( EntryProc );
#endif
	// Linux: skip background thread to avoid threading issues under QEMU user-mode.
}

void CBackTask::CheckStuckThread()
{
	// Periodically called to check if this thread is stuck.

	if ( IsActive())
		return;

	CreateThread();
}


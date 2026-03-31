//
// CClientLog.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
// Login and low level stuff for the client.
//

#include "stdafx.h"	// predef header.

BYTE CClient::sm_xCompress_Buffer[UO_MAX_EVENT_BUFFER];	// static
CCompressTree CClient::sm_xComp;

/////////////////////////////////////////////////////////////////
// -CClient stuff.

int CClient::xDeCompress( BYTE* pOutput, const BYTE* pInput, int iLen ) // static
{
	if ( ! sm_xComp.IsLoaded())
	{
		if ( ! sm_xComp.Load())
			return( -1 );
	}
	return( sm_xComp.Decode( pOutput, pInput, iLen ));
}

int CClient::xCompress( BYTE* pOutput, const BYTE* pInput, int iLen ) // static
{
	// The game server will compress the outgoing data to the clients.
	return sm_xComp.Encode( pOutput, pInput, iLen );
}

bool CClient::CheckProtoVersion()
{
	// Is the protocol version they want allowed ?

	if ( m_ProtoVer.GetCryptVer() == g_Serv.m_ClientVersion.GetCryptVer())
		return true;
	if ( m_ProtoVer.GetCryptVer() >= g_Cfg.m_ClientVerMin.GetCryptVer())
		return true;

	// Seems the protocol they want is not allowed !
	TCHAR szVersion[128];
	Printf( "Client protocol '%s' not allowed!", m_ProtoVer.WriteCryptVer( szVersion ));

	// Is it a GM or admin ?
	if ( GetPrivLevel() >= PLEVEL_Dev )
		return true;
	return false;
}

bool CClient::CheckLogIP()
{
	// Is this Ip already logged in?
	// true = we are ok.

	CLogIPPtr pLogIP = g_Cfg.FindLogIP(m_Socket);
	if ( pLogIP == NULL )
		return( false );
	if ( pLogIP->IncPingBlock( false ))
		return false;

	// We might have an existing account connection ?
	if ( ! m_pAccount && m_ConnectType >= CONNECT_CONSOLE )
	{
		m_pAccount = REF_CAST(CAccount,pLogIP->GetAccount());
	}
	return true;
}

//---------------------------------------------------------------------
// Push world display data to this client only.

bool CClient::addLoginErr( LOGIN_ERR_TYPE code )
{
	// code
	// 0 = no account
	// 1 = account used.
	// 2 = blocked.
	// 3 = no password
	// LOGIN_ERR_OTHER

	if ( code == LOGIN_SUCCESS )
		return true;

	DEBUG_ERR(( "%x:Bad Login %d" LOG_CR, m_Socket.GetSocket(), code ));

	CUOCommand cmd;
	cmd.LogBad.m_Cmd = XCMD_LogBad;
	cmd.LogBad.m_code = code;
	xSendPkt( &cmd, sizeof( cmd.LogBad ));
	xFlush();
	return( false );
}

void CClient::addWebLaunch( LPCTSTR pPage )
{
	// Direct game client to a web page
	ASSERT(pPage);
	if ( pPage[0] == '\0' )
		return;

	WriteString( "Launching your web browser. Please wait...");

	CUOCommand cmd;
	cmd.Web.m_Cmd = XCMD_Web;
	int iLen = sizeof(cmd.Web) + strlen(pPage);
	cmd.Web.m_len = iLen;
	strcpy( cmd.Web.m_page, pPage );
	xSendPkt( &cmd, iLen );
}

///////////////////////////////////////////////////////////////
// Login server.

bool CClient::addRelay( const CServerDef* pServ )
{
	// Tell the client to play on this server.

	ASSERT(pServ);
	CSocketAddressIP ipAddr = pServ->m_ip;

	if ( ipAddr.IsLocalAddr())	// local server address not yet filled in.
	{
		ipAddr = m_Socket.GetSockName();
		DEBUG_MSG(( "%x:Login_Relay to %s" LOG_CR, m_Socket.GetSocket(), ipAddr.GetAddrStr() ));
	}

	CSocketAddress PeerName = m_Socket.GetPeerName();
	if ( PeerName.IsLocalAddr() || PeerName.IsSameIP( ipAddr ))	// weird problem with client relaying back to self.
	{
		DEBUG_MSG(( "%x:Login_Relay loopback to server %s" LOG_CR, m_Socket.GetSocket(), ipAddr.GetAddrStr() ));
		ipAddr.SetAddrIP( SOCKET_LOCAL_ADDRESS );
	}

	DWORD dwAddr = ipAddr.GetAddrIP();

	CUOCommand cmd;
	cmd.Relay.m_Cmd = XCMD_Relay;
	cmd.Relay.m_ip[3] = ( dwAddr >> 24 ) & 0xFF;
	cmd.Relay.m_ip[2] = ( dwAddr >> 16 ) & 0xFF;
	cmd.Relay.m_ip[1] = ( dwAddr >> 8  ) & 0xFF;
	cmd.Relay.m_ip[0] = ( dwAddr	   ) & 0xFF;
	cmd.Relay.m_port = pServ->m_ip.GetPort();
	cmd.Relay.m_Account = 0x7f000001; // dwAddr = customer account handshake. (don't bother to check this.)

	xSendPkt( &cmd, sizeof(cmd.Relay));
	xFlush();	// flush b4 we turn into a game server.

	m_Targ.m_Mode = CLIMODE_SETUP_RELAY;

	// just in case they are on the same machine, change over to the new game encrypt
	m_Crypt.SetCryptType( UNPACKDWORD( cmd.Relay.m_ip ), CONNECT_GAME ); // Init decryption table
	m_CompressXOR.InitTable( 0x7f000001 );
	m_ConnectType = CONNECT_GAME;
	return( true );
}

bool CClient::Login_Relay( int iRelay ) // Relay player to a selected IP
{
	// Client wants to be relayed to another server. XCMD_ServerSelect
	// DEBUG_MSG(( "%x:Login_Relay" LOG_CR, GetSocket() ));
	// iRelay = 0 = this local server.

	if ( !m_ProtoVer.GetCryptVer() || m_ProtoVer.GetCryptVer() >= 0x126000 )
	{
		// client list Gives us a 1 based index for some reason.
		iRelay --;
	}

	CServerPtr pServ;
	if ( iRelay <= 0 )
	{
		pServ = &g_Serv;	// we always list ourself first.
	}
	else
	{
		iRelay --;
		pServ = g_Cfg.Server_GetDef(iRelay);
		if ( pServ == NULL )
		{
			DEBUG_ERR(( "%x:Login_Relay BAD index! %d" LOG_CR, m_Socket.GetSocket(), iRelay+1 ));
			pServ = &g_Serv;	// we always list ourself first.
		}
	}

	return addRelay( pServ );
}

LOGIN_ERR_TYPE CClient::Login_ServerList( const char* pszAccount, const char* pszPassword )
{
	// XCMD_ServersReq
	// Initial login (Login on "loginserver", new format)
	// If the messages are garbled make sure they are terminated to correct length.

	TCHAR szAccount[MAX_ACCOUNT_NAME_SIZE+3];
	int iLenAccount = Str_GetBare( szAccount, pszAccount, sizeof(szAccount)-1 );
	if ( iLenAccount > MAX_ACCOUNT_NAME_SIZE )
		return( LOGIN_ERR_OTHER );
	if ( iLenAccount != strlen(pszAccount))
		return( LOGIN_ERR_OTHER );

	TCHAR szPassword[MAX_NAME_SIZE+3];
	int iLenPassword = Str_GetBare( szPassword, pszPassword, sizeof( szPassword )-1 );
	if ( iLenPassword > MAX_NAME_SIZE )
		return( LOGIN_ERR_OTHER );
	if ( iLenPassword != strlen(pszPassword))
		return( LOGIN_ERR_OTHER );

	// Make sure the first server matches the GetSockName here
	if ( g_Log.IsLogged( LOGL_TRACE ))
	{
		DEBUG_MSG(( "%x:Login_ServerList to '%s','%s'" LOG_CR, m_Socket.GetSocket(), pszAccount, pszPassword ));
	}

	// don't bother logging in yet.
	// Give the server list to everyone.
	// if ( ! addLoginErr( LogIn( pszAccount, pszPassword )))
	// { return; }

	CUOCommand cmd;
	cmd.ServerList.m_Cmd = XCMD_ServerList;

	int indexoffset = 1;
	if ( m_ProtoVer.GetCryptVer() >= 0x126000 )
	{
		indexoffset = 2;
	}

	// always list myself first here.
	{ CUOCommandServer tmpSrv = cmd.ServerList.m_serv[0]; g_Serv.addToServersList( tmpSrv, indexoffset-1 ); cmd.ServerList.m_serv[0] = tmpSrv; }

	int iQtyMax = 32; // UO_MAX_SERVERS;
	int j = 1;
	for ( int i=0; j < iQtyMax; i++ )
	{
		CServerLock pServ = g_Cfg.Server_GetDef(i);
		if ( pServ == NULL )
			break;
		if ( pServ->GetConnectStatus() > 1 )
			continue;
		{ CUOCommandServer tmpSrv = cmd.ServerList.m_serv[j]; pServ->addToServersList( tmpSrv, i+indexoffset ); cmd.ServerList.m_serv[j] = tmpSrv; }
		j++;
	}

	int iLen = sizeof(cmd.ServerList) - sizeof(cmd.ServerList.m_serv) + ( j* sizeof(cmd.ServerList.m_serv[0]));
	cmd.ServerList.m_len = iLen;
	cmd.ServerList.m_count = j;
	cmd.ServerList.m_VerCode = 0x3d; // 0xFF;
	xSendPkt( &cmd, iLen );

	m_Targ.m_Mode = CLIMODE_SETUP_SERVERS;
	return( LOGIN_SUCCESS );
}

//*****************************************

REGRES_TYPE CClient::OnRxAutoServerRegister( const BYTE* pData, int iLen )
{
	// I got the 0xFFFFFFFF header plus a 0 byte.
	// This means it is an auto server registration packet.

	ASSERT( m_ConnectType == CONNECT_AUTO_SERVER );

	// dump the client on return.

	if ( iLen <= 0  || iLen >= CSTRING_MAX_LEN-1 )
		return REGRES_RET_INVALID;
	((TCHAR *)(pData))[iLen]='\0';

	// Server registration message.
	// Check to see if this server is already here.
	CSocketAddress PeerName = m_Socket.GetPeerName();
	if ( ! PeerName.IsValidAddr() )
		return REGRES_RET_FAILURE;

	// Create a new entry for this. (so we can get it's name etc.)
	CServerPtr pServNew = new CServerDef( UID_INDEX_CLEAR, NULL, PeerName );
	ASSERT( pServNew );
	pServNew->ParseStatus( (LPCTSTR)(pData), true );

	if ( pServNew->GetName()[0] == '\0' )
	{
		// There is no name here ?
		// Might we try to match by IP address ?

		for ( int i=0;; i++ )
		{
			CServerPtr pServTest = g_Cfg.Server_GetDef(i);
			if ( pServTest == NULL )
				break;
			if ( PeerName.IsSameIP( pServTest->m_ip ))
			{
				pServTest->ParseStatus( (char*)pData, true );
				g_Cfg.m_Servers.RemoveArg( pServNew );
				return REGRES_RET_OK;
			}
		}

		g_Log.Event( LOG_GROUP_ACCOUNTS, LOGL_WARN, "%x:Bad reg info, No server name '%s'" LOG_CR, m_Socket.GetSocket(), (LPCTSTR)pData );
		g_Cfg.m_Servers.RemoveArg( pServNew );
		return REGRES_RET_IP_UNK;
	}

	// Look up it's name.
	CThreadLockPtr lock( &g_Cfg.m_Servers );
	int index = g_Cfg.m_Servers.FindKey( pServNew->GetName());
	if ( index < 0 )
	{
		// No server by this name
		if ( g_Cfg.m_sMainLogServerDir.IsEmpty())
		{
			g_Cfg.m_Servers.RemoveArg( pServNew );
			return( REGRES_RET_NAME_UNK );
		}

		g_Log.Event( LOG_GROUP_ACCOUNTS, LOGL_EVENT, "%x:Adding Server '%s' to list." LOG_CR,
			m_Socket.GetSocket(), (LPCTSTR) pServNew->GetName());

		// only the main login server will add automatically.
		g_Cfg.m_Servers.AddSortKey( pServNew, pServNew->GetName());
		return REGRES_RET_OK;
	}

	CServerPtr pServName = g_Cfg.Server_GetDef(index);
	ASSERT( pServName );

	if ( ! pServName->IsSame( pServNew ) &&
		! PeerName.IsSameIP( pServName->m_ip ))
	{
		g_Log.Event( LOG_GROUP_ACCOUNTS, LOGL_WARN, "%x:Bad regpass '%s'!='%s' for server '%s'" LOG_CR, m_Socket.GetSocket(), (LPCTSTR) pServNew->m_sRegisterPassword, (LPCTSTR) pServName->m_sRegisterPassword, (LPCTSTR) pServName->GetName());
		g_Cfg.m_Servers.RemoveArg( pServNew );
		return REGRES_RET_BAD_PASS;
	}

	// It looks like our IP just changed thats all. RegPass checks out.
	pServName->ParseStatus( (LPCTSTR)pData, true );
	return REGRES_RET_OK;
}

bool CClient::OnRxPeerServer( const BYTE* pData, int iLen )
{
	// Answer a question for this peer server.
	// This is on my m_Servers list or is the auto registration server.

	ASSERT( m_ConnectType == CONNECT_PEER_SERVER );

	bool fSuccess = false;
	if ( iLen <= 128 )
	{
		TCHAR szRequest[ 128 ];
		strcpylen( szRequest, (LPCTSTR) pData, sizeof(szRequest) );
		TCHAR* pszCmd = Str_TrimWhitespace( szRequest );

		// ??? this is a bit too powerful. SECURITY PROBLEM?!
		CGVariant vVal;
		HRESULT hRes = g_Serv.s_PropGet( pszCmd, vVal, this );
		if ( !IS_ERROR(hRes))
		{
			CGString sTmp = (CGString) vVal;
			xSendReady( sTmp, sTmp.GetLength()+1 );
		}
	}
	return( true );	// don't dump the connection.
}

bool CClient::OnRxConsoleLoginComplete()
{
	// We have logged in and are aready to be telnet admin

	ASSERT( m_ConnectType == CONNECT_TELNET );
	if ( GetPrivLevel() < PLEVEL_Admin )	// this really should not happen.
	{
		WriteString( "Sorry you don't have telnet permission" LOG_CR );
		return( false );
	}

	CSocketAddress PeerName = m_Socket.GetPeerName();
	if ( ! PeerName.IsValidAddr())
		return( false );

	m_Targ.m_Mode = CLIMODE_TELNET_READY;
	Printf( "Welcome to the " SPHERE_TITLE " console '%s','%s'" LOG_CR, (LPCTSTR) GetName(), PeerName.GetAddrStr());
	return( true );
}

bool CClient::OnRxConsoleLogin()
{
	// RETURN: true = success.

	ASSERT( m_ConnectType == CONNECT_TELNET );
	ASSERT( m_Targ.m_Mode == CLIMODE_TELNET_PASS );

	const TCHAR* pszUser = m_Targ.m_sText;
	const TCHAR* pszPass = strchr( pszUser, ':' );
	if ( pszPass == NULL )
		return false;

	TCHAR szUser[ MAX_NAME_SIZE ];
	TCHAR szPass[ MAX_NAME_SIZE ];
	strcpylen( szUser, pszUser, MIN( sizeof(szUser), ( pszPass - pszUser ) + 1 ));
	strcpylen( szPass, pszPass+1, sizeof(szPass));

	CGString sMsg;
	if ( LogIn( szUser, szPass ) != LOGIN_SUCCESS )
	{
		return( false );
	}
	m_Targ.m_sText.Empty();	// WriteString goes here.

	return OnRxConsoleLoginComplete();
}

bool CClient::OnRxConsole( const BYTE* pData, int iLen )
{
	// CONNECT_TELNET
	// A special console version of the client. (Not game protocol)
	// RETURN: true = keep processing.

	ASSERT( iLen );
	ASSERT( m_ConnectType == CONNECT_TELNET );

	while ( iLen -- )
	{
		int iRet = AddConsoleKey( m_Targ.m_sText, *pData++, false ); // never echo?
		if ( ! iRet )
			return( false );

		if ( iRet != 2 )
			continue;

		switch ( m_Targ.m_Mode )
		{
		case CLIMODE_TELNET_USERNAME:
			// Just save the username for now.
			// Put the username and password in format http://user:password@host:port/url-path
			m_Targ.m_sText += ':';
			m_Targ.m_Mode = CLIMODE_TELNET_PASS;
			WriteString( "Password?:" LOG_CR );
			return( true );

		case CLIMODE_TELNET_PASS:
			return( OnRxConsoleLogin() ? true : false );

		case CLIMODE_TELNET_READY:
			g_Serv.OnConsoleCmd( m_Targ.m_sText, this );
			break;
		}
	}

	return( true );
}

bool CClient::OnRxPing( const BYTE* pData, int iLen )
{
	// packet iLen < 4
	// UOMon should work like this.
	// RETURN: true = keep the connection open.

	ASSERT( m_ConnectType == CONNECT_UNK );
	ASSERT( iLen < 4 );
	// The linux telnet client also sends a CRLF pair at the end of any 
	// text so we need to check for that as well.
	if ( iLen > 3 )	// this is not a ping. don't know what it is.
		return( false );

	if ( pData[0] == ' ' )
	{
		// enter into remote admin mode. (look for password).
		m_ConnectType = CONNECT_TELNET;
		Printf( "Welcome to %s Telnet" LOG_CR, (LPCTSTR) g_Serv.GetName());

		CSocketAddress PeerName = m_Socket.GetPeerName();
		if ( ! PeerName.IsValidAddr())
			return( false );

		if ( g_Cfg.m_fLocalIPAdmin && PeerName.IsLocalAddr() )
		{
			// don't bother logging in if local.

			CAccountPtr pAccount = g_Accounts.Account_FindNameChecked( "Administrator" );

			LOGIN_ERR_TYPE lErr = LogIn( pAccount );
			if ( lErr != LOGIN_SUCCESS )
			{
				return( false );
			}
			return OnRxConsoleLoginComplete();
		}

		m_Targ.m_Mode = CLIMODE_TELNET_USERNAME;
		WriteString( "Username?:" LOG_CR );
		return( true );
	}

	// Answer the ping and drop.
	m_ConnectType = CONNECT_PING;
	CString sTemp = g_Serv.GetStatusString( pData[0] );
	xSendReady( sTemp, sTemp.GetLength()+1 );
	return( false );
}

bool CClient::OnRxWebPageRequest( BYTE* pRequest, int iLen )
{
	// a web browser is requesting a page?
	ASSERT( m_ConnectType == CONNECT_HTTP );
	m_Targ.m_sText.Empty();	// kill any previous text sent.

	pRequest[iLen] = '\0';

	TCHAR* ppLines[64];
	int iQtyLines = Str_ParseCmds( (TCHAR*) pRequest, ppLines, COUNTOF(ppLines), "\r\n" );
	if ( iQtyLines < 1 )
		return( false );

	static const TCHAR* sm_szTags[] =
	{
		// "Accept:",
		// "Accept-Encoding:",
		// "Accept-Language:",
		// "User-Agent:",
		// "Content-Type:",
		// "Cache-Control:",
		// "Cache-Control:",
		// "Host:",
		// "VTI-GROUP=",
		"Connection:",
		"Content-Length:",
		"If-Modified-Since:", 
		"Referer:",
		NULL,
	};

	// Look for what they want to do with the connection.
	bool fKeepAlive = false;
	int iContentLength = 0;
	CGTime dateIfModifiedSince;
	const TCHAR* pszReferer = NULL;

	for ( int j=1; j<iQtyLines; j++ )
	{
		TCHAR* pszArgs = ppLines[j];
		int iTag = FindTableHeadSorted( pszArgs, sm_szTags, COUNTOF(sm_szTags)-1);
		if ( iTag < 0 )
			continue;
		pszArgs += strlen(sm_szTags[iTag]);
		GETNONWHITESPACE(pszArgs);
		switch (iTag)
		{
		case 0:	// Connection:
			if ( strstr( pszArgs, "Keep-Alive" ))
			{
				fKeepAlive = true;
			}
			break;
		case 1:	// Content-Length:
			iContentLength = Exp_GetValue(pszArgs);
			break;
		case 2:	// If-Modified-Since:
			// If-Modified-Since: Fri, 17 Dec 1999 14:59:20 GMT\r\n
			dateIfModifiedSince.Read( pszArgs );
			break;
		case 3: // "Referer:"
			pszReferer = pszArgs;
			break;
		}
	}

	TCHAR* ppRequest[4];
	int iQtyArgs = Str_ParseCmds( (TCHAR*) ppLines[0], ppRequest, COUNTOF(ppRequest), " " );
	if ( iQtyArgs < 2 )
		return( false );

	//linger llinger;
	//llinger.l_onoff = 1;
	//llinger.l_linger = 500;	// in mSec
	//m_Socket.SetSockOpt( SO_LINGER, (char*)&llinger, sizeof(struct linger));
	//BOOL nbool = true;
	//m_Socket.SetSockOpt( SO_KEEPALIVE, &nbool, sizeof(BOOL));

	// disable NAGLE algorythm for data compression
	//nbool=true;
	//m_Socket.SetSockOpt( TCP_NODELAY,&nbool,sizeof(BOOL),IPPROTO_TCP);

	if ( ! memcmp( ppLines[0], "POST", 4 ))
	{
		// web browser is posting data back to the server.
		// IT must be one of our defined pages to handled posted info.
		// Is it one of our defined pages ?

		g_Log.Event( LOG_GROUP_HTTP, LOGL_EVENT, "%x:HTTP Page Post '%s'" LOG_CR, m_Socket.GetSocket(), (LPCTSTR) ppRequest[1] );

		// POST /--WEBBOT-SELF-- HTTP/1.1
		// Accept: image/gif, image/x-xbitmap, image/jpeg, */*
		// Referer: http://127.0.0.1:2593/spherestatus.htm
		// Accept-Language: en-us
		// Content-Type: application/x-www-form-urlencoded
		// Accept-Encoding: gzip, deflate
		// User-Agent: Mozilla/4.0 (compatible; MSIE 5.0; Windows NT; DigExt)
		// Host: 127.0.0.1:2593
		// Content-Length: 29
		// Connection: Keep-Alive
		// T1=stuff1&B1=Submit&T2=stuff2

		// Is it one of our defined pages ?
		CWebPagePtr pWebPage;
		if ( pszReferer )
		{
			pWebPage = g_Cfg.FindWebPage(pszReferer);
		}
		if ( pWebPage == NULL )
		{
			pWebPage = g_Cfg.FindWebPage(ppRequest[1]);
		}
		if ( pWebPage )
		{
			if ( pWebPage->ServePagePost( this, ppRequest[1], ppLines[iQtyLines-1], iContentLength ))
				return fKeepAlive;
		}

		// return( false );
		// just fall through and serv up the error page.
	}
	else
	{
		// Request for a page. try to respond.
		// GET /pagename.htm HTTP/1.1\r\n
		// Accept: image/gif, */*\r\n
		// Accept-Language: en-us\r\n
		// Accept-Encoding: gzip, deflate\r\n
		// If-Modified-Since: Fri, 17 Dec 1999 14:59:20 GMT\r\n
		// User-Agent: Mozilla/4.0 (compatible; MSIE 5.0; Windows NT; DigExt)\r\n
		// Host: localhost:2593\r\n
		// Connection: Keep-Alive\r\n	// keep the connection open ? or "close"
		// \r\n
		//

		g_Log.Event( LOG_GROUP_HTTP, LOGL_EVENT, "%x:HTTP Page Request '%s', alive=%d" LOG_CR, m_Socket.GetSocket(), (LPCTSTR) ppRequest[1], fKeepAlive );
	}

	HRESULT hRes = CWebPageDef::ServePage( this, ppRequest[1], &dateIfModifiedSince );
	if ( IS_ERROR(hRes))
		return( false );

	return fKeepAlive;
}

void CClient::xFinishProcessMsg( bool fGood )
{
	// Done with the current packet.
	// m_bin_msg_len = size of the current packet we are processing.

	if ( ! m_bin_msg_len )	// hmm, nothing to do !
		return;

	const CUOEvent* pEvent = (const CUOEvent *) m_bin.RemoveDataLock();

	if ( ! fGood )	// toss all.
	{
		DEBUG_ERR(( "%x:Bad Msg %x Eat %d bytes, prv=0%x" LOG_CR, m_Socket.GetSocket(), pEvent->Default.m_Cmd, m_bin.GetDataQty(), m_bin_PrvMsg ));
		m_bin.RemoveDataAmount( m_bin_msg_len );	// eat the buffer.
		if ( m_ConnectType == CONNECT_LOGIN )	// tell them about it.
		{
			addLoginErr( LOGIN_ERR_OTHER );
		}
	}
	else
	{
		// This is the last message we are pretty sure we got correctly.
		m_bin_PrvMsg = (XCMD_TYPE)( pEvent->Default.m_Cmd );
		m_bin.RemoveDataAmount( m_bin_msg_len );
	}

	m_bin_msg_len = 0;	// done with this packet.
}

bool CClient::xProcessClientSetup( CUOEvent* pEvent, int iLen )
{
	// If this is a login then try to process the data and figure out what client it is.
	// try to figure out which client version we are talking to.
	// (CUOEvent::ServersReq) or (CUOEvent::CharListReq)
	// NOTE: Anything else we get at this point is tossed !

	ASSERT( m_ConnectType == CONNECT_CRYPT );
	// ASSERT( !m_Crypt.IsInitCrypt());
	ASSERT( iLen );

	if ( iLen == sizeof(pEvent->ServersReq) ) // SERVER_Login 1.26.0
	{
		m_Crypt.SetCryptType( m_Targ.m_tmSetup.m_dwCryptKey, CONNECT_LOGIN ); // Init decryption table
		m_ConnectType = CONNECT_LOGIN;
	}
	else if ( iLen == sizeof(pEvent->CharListReq))	// Auto-registering server sending us info.
	{
		m_Crypt.SetCryptType( m_Targ.m_tmSetup.m_dwCryptKey, CONNECT_GAME ); // Init decryption table
		if ( ! m_CompressXOR.InitTable( m_Targ.m_tmSetup.m_dwCryptKey ))
		{
			addLoginErr( LOGIN_ERR_OTHER );
			return false;
		}
		m_ConnectType = CONNECT_GAME;
	}
	else
	{
		DEBUG_MSG(( "%x:Odd login message length %d?" LOG_CR, m_Socket.GetSocket(), iLen ));
		m_Crypt.SetCryptType( m_Targ.m_tmSetup.m_dwCryptKey, CONNECT_LOGIN ); // Init decryption table
		m_ConnectType = CONNECT_LOGIN;
	}

	if ( ! CheckLogIP())	// we are a blocked ip so i guess it does not matter.
	{
		addLoginErr( LOGIN_ERR_BLOCKED );
		return( false );
	}

	// Try all client versions on the msg.
	CUOEvent bincopy;		// in buffer. (from client)
	ASSERT( iLen <= sizeof(bincopy));
	memcpy( bincopy.m_Raw, pEvent->m_Raw, iLen );

	int iVer = -1;	// just use whatever it was do default.
	LOGIN_ERR_TYPE lErr = LOGIN_ERR_OTHER;
	for(;;)
	{
		m_Crypt.Decrypt( pEvent->m_Raw, bincopy.m_Raw, iLen );

		if ( pEvent->Default.m_Cmd == XCMD_ServersReq )
		{
			if ( iLen < sizeof( pEvent->ServersReq ))
				return(false);
			lErr = Login_ServerList( pEvent->ServersReq.m_acctname, pEvent->ServersReq.m_acctpass );
		}
		else if ( pEvent->Default.m_Cmd == XCMD_CharListReq )
		{
			if ( iLen < sizeof( pEvent->CharListReq ))
				return(false);
			lErr = Setup_CharListReq( pEvent->CharListReq.m_acctname, pEvent->CharListReq.m_acctpass, 0 );
		}
		if ( lErr != LOGIN_ERR_OTHER )
			break;
		iVer ++;
		if ( iVer == 0 && m_Crypt.GetCryptVer() == 0 )	// we already tried this.
			iVer ++;
		if ( ! m_Crypt.SetCryptVerEnum( iVer ))
			break;
	}

	m_ProtoVer = m_Crypt;	// default to start.

	if ( ! CheckProtoVersion())
	{
		lErr = LOGIN_ERR_OTHER;
	}

	if ( lErr == LOGIN_ERR_OTHER )	// it never matched any crypt format.
	{
		addLoginErr( lErr );
	}

	return( lErr == LOGIN_SUCCESS );
}

void CClient::xFlush()
{
	// Sends buffered data at once
	// NOTE:
	// Make sure we do not overflow the Sockets Tx buffers!
	//

	int iLen = m_bout.GetDataQty();
	if ( ! iLen || ! m_Socket.IsOpen())
		return;

	m_timeLastSend.InitTimeCurrent();

	int iLenRet;
	if ( m_ConnectType != CONNECT_GAME /* || m_Crypt.GetCryptVer() >= 0x200040 */ )	// acting as a login server to this client.
	{
		iLenRet = m_Socket.Send( m_bout.RemoveDataLock(), iLen );
		if ( iLenRet != SOCKET_ERROR )
		{
			// Tx overflow may be handled gracefully.
			g_Serv.m_Profile.IncTaskCount( PROFILE_DataTx, iLenRet );
			m_bout.RemoveDataAmount(iLenRet);
		}
		else
		{
			// Assume nothing was transmitted.
		Do_Handle_Error:
			int iErrCode = CGSocket::GetLastError();
#ifdef _WIN32
			if ( iErrCode == WSAECONNRESET || iErrCode == WSAECONNABORTED )
			{
				// WSAECONNRESET = 10054 = far end decided to bail.
				m_Socket.Close();
				return;
			}
			if ( iErrCode == WSAEWOULDBLOCK )
			{
				// just try back later. or select() will close it for us later.
				return;
			}
#endif
			DEBUG_ERR(( "%x:Tx Error %d" LOG_CR, m_Socket.GetSocket(), iErrCode ));
			return;
		}
	}
#ifdef SPHERE_GAME_SERVER
	else
	{
		// Only the game server does this.
		// This acts as a compression alg. tho it may expand the data some times.

		int iLenComp = xCompress( sm_xCompress_Buffer, m_bout.RemoveDataLock(), iLen );
		ASSERT( iLenComp <= sizeof(sm_xCompress_Buffer));

		if ( m_Crypt.GetCryptVer() >= 0x200040 )
		{
			m_CompressXOR.CompressXOR( sm_xCompress_Buffer, iLenComp );
		}

		// DEBUG_MSG(( "%x:Send %d bytes as %d" LOG_CR, m_Socket.GetSocket(), m_bout.GetDataQty(), iLen ));

		iLenRet = m_Socket.Send( sm_xCompress_Buffer, iLenComp );
		if ( iLenRet != SOCKET_ERROR )
		{
			g_Serv.m_Profile.IncTaskCount( PROFILE_DataTx, iLen );
			m_bout.RemoveDataAmount(iLen);	// must use all of it since we have no idea what was really sent.
		}

		if ( iLenRet != iLenComp )
		{
			// Tx overflow is not allowed here !
			// no idea what effect this would have. assume nothing is sent.
			goto Do_Handle_Error;
		}
	}
#endif
}

void CClient::xSend( const void *pData, int length )
{
	// buffer a packet to client.
	if ( length <= 0 )
		return;
	if ( ! m_Socket.IsOpen())
		return;

	if ( m_ConnectType != CONNECT_HTTP )	// acting as a login server to this client.
	{
		if ( length > UO_MAX_EVENT_BUFFER )
		{
			g_Log.Event( LOG_GROUP_CLIENTS, LOGL_ERROR, "%x:Client out TO BIG %d!" LOG_CR, m_Socket.GetSocket(), length );
			m_Socket.Close();
			return;
		}
		if ( m_bout.GetDataQty() + length > UO_MAX_EVENT_BUFFER )
		{
			g_Log.Event( LOG_GROUP_CLIENTS, LOGL_ERROR, "%x:Client out overflow %d+%d!" LOG_CR, m_Socket.GetSocket(), m_bout.GetDataQty(), length );
			m_Socket.Close();
			return;
		}
	}

	m_bout.AddNewData( (const BYTE*) pData, length );
}

void CClient::xSendReady( const void *pData, int length ) // We could send the packet now if we wanted to but wait til we have more.
{
	// We could send the packet now if we wanted to but wait til we have more.
	if ( m_bout.GetDataQty() + length >= UO_MAX_EVENT_BUFFER )
	{
		xFlush();
	}
	xSend( pData, length );
	if ( m_bout.GetDataQty() >= UO_MAX_EVENT_BUFFER / 2 )	// send only if we have a bunch.
	{
		xFlush();
	}
}

bool CClient::OnRxUnk( BYTE* pData, int iLen ) // Receive message from client
{
	// This is the first data we get on a new connection.
	// Figure out what the other side wants.

	// DEBUG_CHECK( ! m_Crypt.IsInitCrypt());

	if ( iLen < 4 )	// just a ping for server info. (maybe, or CONNECT_TELNET?)
	{
		if ( ! CheckLogIP())
			return( false );
		return( OnRxPing( pData, iLen ));
	}

	if ( iLen > 5 )
	{
		if ( *((DWORD*)pData) == 0xFFFFFFFF )
		{
			// special inter-server type message.
			m_ConnectType = CONNECT_AUTO_SERVER;

			if ( ! CheckLogIP())
				return( false );
			if ( pData[4] == 0 )
			{
				// Server Registration message
				REGRES_TYPE retcode = OnRxAutoServerRegister( &pData[5], iLen-5 );

				// Send the code back.
				return( false );
			}
		}

		// Is it a HTTP request ?
		// Is it HTTP post ?
		if ( ! memcmp( pData, "POST /", 6 ) ||
			! memcmp( pData, "GET /", 5 ))
		{
			m_ConnectType = CONNECT_HTTP;	// we are serving web pages to this client.

			if ( ! CheckLogIP())
				return( false );

			return( OnRxWebPageRequest( pData, iLen ));
		}
	}

	// Assume it's a normal client log in.
	m_Targ.m_tmSetup.m_dwCryptKey = UNPACKDWORD(pData);
	iLen -= sizeof( DWORD );
	m_ConnectType = CONNECT_CRYPT;
	if ( iLen <= 0 )
	{
		return( true );
	}

	return( xProcessClientSetup( (CUOEvent*)(pData+4), iLen ));
}

bool CClient::xRecvData() // Receive message from client
{
	// High level Rx from Client.
	// RETURN: false = dump the client.

	CUOEvent Event;
	int iCountNew = m_Socket.Receive( &Event, sizeof(Event), 0 );
	if ( iCountNew <= 0 )	// I should always get data here.
	{
		return( false ); // this means that the client is gone.
	}
	if ( iCountNew > 0 )
	{
		g_Serv.m_Profile.IncTaskCount( PROFILE_DataRx, iCountNew );
	}

	switch ( m_ConnectType )
	{
	case CONNECT_UNK: // first thing
		return OnRxUnk( Event.m_Raw, iCountNew );

	case CONNECT_LOGIN:
	case CONNECT_GAME:
		// Decrypt the client data.
		// TCP = no missed packets ! If we miss a packet we are screwed !
		ASSERT( m_Crypt.IsInitCrypt());
		if ( m_bin.GetDataQty() + iCountNew >= UO_MAX_EVENT_BUFFER )
			return false;
		m_Crypt.Decrypt( m_bin.AddNewDataLock(iCountNew), Event.m_Raw, iCountNew );
		m_bin.AddNewDataFinish(iCountNew);
		return true;

	case CONNECT_CRYPT:
		// try to figure out which client version we are talking to.
		// (CUOEvent::ServersReq) or (CUOEvent::CharListReq)
		return( xProcessClientSetup( &Event, iCountNew ));

#ifdef SPHERE_GAME_SERVER
	case CONNECT_HTTP:
		// we are serving web pages to this client.
		return( OnRxWebPageRequest( Event.m_Raw, iCountNew ));
	case CONNECT_PEER_SERVER:
		return( OnRxPeerServer( Event.m_Raw, iCountNew ));
	case CONNECT_TELNET:
		// We already logged in or are in the process of logging in.
		return( OnRxConsole( Event.m_Raw, iCountNew ));
#endif
	}

	return( false );	// No idea what this junk is.
}


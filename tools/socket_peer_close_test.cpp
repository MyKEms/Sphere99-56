// Regression test for a buffered send after the peer has closed its socket.

#include "SphereSvr/stdafx.h"

#ifndef _WIN32
#include <csignal>
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
	std::signal( SIGPIPE, SIG_IGN );
	int sockets[2] = { -1, -1 };
	if ( socketpair( AF_UNIX, SOCK_STREAM, 0, sockets ) != 0 )
	{
		std::perror( "socketpair" );
		return 1;
	}

	bool fClosed = false;
	{
		CClient client( sockets[0] );
		sockets[0] = -1;
		client.m_ConnectType = CONNECT_LOGIN;
		close( sockets[1] );
		sockets[1] = -1;

		static const char szPayload[] = "peer-closed";
		client.xSend( szPayload, sizeof(szPayload) );
		client.xFlush();
		fClosed = !client.m_Socket.IsOpen();
	}

	if ( sockets[0] >= 0 )
		close( sockets[0] );
	if ( sockets[1] >= 0 )
		close( sockets[1] );
	if ( !fClosed )
	{
		std::fprintf( stderr, "a send to a closed peer left the client socket open\n" );
		return 1;
	}
	return 0;
}
#else
int main()
{
	return 0;
}
#endif

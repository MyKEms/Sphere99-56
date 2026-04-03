//
// SphereLog.CPP
//
// Login server for SphereWorld.
// Some have expressed the need to have a spererate login server app
// to take the load off Main SphereWorld server while people fail/retry logins.
// Also act as Accounts server.
//
// NOTE: This uses the same port as SphereWorld.
// It MUST BE RUN ON A SEPERATE MACHINE
//
// see http://www.menasoft.com/sphere for more details.

#include "spherecom.h"

#define SPHERE_TITLE		"SphereLog"
#define SPHERE_VERSION	"0.01"

class CLogServer : public CSockets
{

};

#include "cclientlog.cpp"

int main( int argc, char *argv[] )
{
#ifdef _WIN32
	SetConsoleTitle( SPHERE_TITLE " V" SPHERE_VERSION );
#endif
	printf( SPHERE_TITLE " V" SPHERE_VERSION
#ifdef _WIN32
		" for Win32" LOG_CR
#else
		" for Linux" LOG_CR
#endif
		"Client Version: " SPHERE_CLIENT_STR LOG_CR
		"Compiled on " __DATE__ " (" __TIME__ " " SPHERE_TIMEZONE ")" LOG_CR
		"Compiled by " SPHERE_FILE " <" SPHERE_EMAIL ">" LOG_CR
		LOG_CR );

	g_Log.SocketsInit();

	for(;;)
	{
		if ( kbhit())
		{
			// Send the char over to the server.
			char ch = getch();
			if ( ch == 0x1b ) break;	// ESCAPE KEY
		}

		// Look for incoming data.
		if ( ! g_Log.SocketsReceive())
			break;
	}

	return( 0 );
}


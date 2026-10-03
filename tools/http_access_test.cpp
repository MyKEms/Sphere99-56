// Regression test for the localhost-only HTTP access policy.

#include "spherelib/spherelib.h"
#include "SphereSvr/httpaccess.h"

#include <cstdio>

static bool Expect( bool fCondition, const char* pszDescription )
{
	if ( fCondition )
		return true;
	std::fprintf( stderr, "FAIL: %s\n", pszDescription );
	return false;
}

int main()
{
	CSocketAddress loopback;
	loopback.SetAddrStr( "127.0.0.1" );
	CSocketAddress remote;
	remote.SetAddrStr( "203.0.113.9" );
	CSocketAddress invalid;

	return Expect( IsHTTPPeerLocal( loopback ),
		"loopback peer remains eligible for HTTP" ) &&
		Expect( !IsHTTPPeerLocal( remote ),
			"non-loopback peer cannot enter the HTTP handler" ) &&
		Expect( !IsHTTPPeerLocal( invalid ),
			"invalid peer cannot enter the HTTP handler" ) ? 0 : 1;
}

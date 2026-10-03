// HTTP transport access policy.

#ifndef _INC_SPHERE_HTTPACCESS_H
#define _INC_SPHERE_HTTPACCESS_H

#include "CSocket.h"

// The legacy HTTP handler is an administrative surface.  Keep it available
// for local tooling while rejecting peers that are not an unambiguous local
// loopback connection before request parsing or script evaluation begins.
inline bool IsHTTPPeerLocal( const CSocketAddress& peer )
{
	return peer.IsValidAddr() && peer.IsLocalAddr();
}

#endif // _INC_SPHERE_HTTPACCESS_H

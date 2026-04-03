//
// stdafx.H
// Copyright Menace Software (www.menasoft.com).
// Precompiled header
//

#ifndef _INC_STDAFX_H_
#define _INC_STDAFX_H_
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef SPHERE_SVR
#error SPHERE_SVR must be -defined on compiler command line for common code to work!
#endif

// SPHERE_VERSION
#define SPHERE_GAME_SERVER
#define SPHERE_LOG_SERVER
//#define USE_JSCRIPT		// incorporate the Jscript code or not. // D:\samples\js\src

#ifdef _DEBUG
#define DEBUG_VALIDATE_ALLOC	// slows us down but checks memory often
#endif

#ifdef _WIN32
// NOTE: If we want a max number of sockets we must compile for it !
#undef FD_SETSIZE		// This shuts off a warning
#define FD_SETSIZE 1024 // for max of n users ! default = 64
#endif

#ifdef _AFXDLL
// Windows with MFC
#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC OLE automation classes
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT
#include <afxtempl.h>		// MFC Templates
#endif

#include "spheresvr.h"

#endif	// _INC_STDAFX_H_

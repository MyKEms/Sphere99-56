//
// spherecommon.h
// Copyright Menace Software (www.menasoft.com).
// common header file.
//

#ifndef _INC_SPHERECOMMON_H
#define _INC_SPHERECOMMON_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

//#define _DEBUG	// compile a debug version with more verbose comments
#define SPHERE_VERSION		"0.99u"		// share version with all files.
#define SPHERE_DEF_PORT		2593
#define SPHERE_FILE			"sphere"	// file name prefix
#define SPHERE_TITLE		"Sphere"	// "Sphere"
#define SPHERE_MAIN_SERVER	"list.sphereserver.net"
#define SPHERE_URL			"www.sphereserver.net"	// default url.
#define SPHERE_REGKEY		"Software\\Menasoft\\" SPHERE_FILE

//---------------------------SYSTEM DEFINITIONS---------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

#ifdef _WIN32

#ifndef STRICT
#define STRICT			// strict conversion of handles and pointers.
#endif	// STRICT

#include <direct.h>	// _chdir
#include <io.h>
#include <windows.h>
#include <winsock.h>
#include <dos.h>
#include <limits.h>
#include <conio.h>
#include <sys/timeb.h>

#else	// _WIN32 else assume LINUX

#include <sys/types.h>
#include <sys/timeb.h>
#include <unistd.h>
#include <limits.h>

// Type definitions are now in spherelib/common.h - no duplicates here.
#ifndef PUINT
#define PUINT			unsigned int *
#endif

#endif // !_WIN32

#ifdef _DEBUG
#ifdef SPHERE_SVR
#ifndef ASSERT
extern void Assert_CheckFail( const char * pExp, const char *pFile, unsigned uLine );
#define ASSERT(exp)			(void)( (exp) || (Assert_CheckFail(#exp, __FILE__, __LINE__), 0) )
#endif	// ASSERT
#else	// SPHERE_SVR
#ifndef ASSERT
#define ASSERT			assert
#endif
#endif	// ! SPHERE_SVR
#ifndef DEBUG_CHECK
extern void Debug_CheckFail( const char * pExp, const char *pFile, unsigned uLine );
#define DEBUG_CHECK(exp)	(void)( (exp) || (Debug_CheckFail(#exp, __FILE__, __LINE__), 0) )
#endif	// DEBUG_CHECK

extern void Debug_CheckPoint();

#else	// _DEBUG

#ifndef ASSERT
#ifndef _WIN32
// In linux, if we get an access violation, an exception isn't thrown.  Instead, we get a SIG_SEGV, and
// the process cores.  The following code takes care of this for us
extern void Assert_CheckFail( const char * pExp, const char *pFile, unsigned uLine );
#define ASSERT(exp)			(void)( (exp) || (Assert_CheckFail(#exp, __FILE__, __LINE__), 0) )
#else
#define ASSERT(exp)
#endif
#endif	// ASSERT

#ifndef DEBUG_CHECK
#define DEBUG_CHECK(exp)
#endif

#define DEBUG_ASSERT ASSERT
#define Debug_CheckPoint()

#endif	// ! _DEBUG

#include "spherelib.h"
#include "cresourcebase.h"
#include "spheremul.h"
#include "sphereproto.h"
#include "cregionmap.h"
#include "cMulInst.h"
#include "cMulMap.h"
#include "cmulmulti.h"
#include "cMulTile.h"
#include "ccrypt.h"

#ifdef SPHERE_SVR
#define TICKS_PER_SEC 10	// CServTime based on CServTimeMaster
#else
#define TICKS_PER_SEC 1000	// CServTime
inline CServTimeBase CServTimeBase::GetCurrentTime()	// static
{
	CServTimeBase timeVar;
	timeVar.InitTime( ::GetTickCount());
	return( timeVar );
}
#endif	// ! SPHERE_SVR

#ifdef WM_USER
#define WM_SPHERE_CLIENT_COMMAND	(WM_USER+123)	// command the client to do something.

enum SPHERECLIENTMSG_TYPE
{
	SPHERECLIENTMSG_LINK = 0,		// LPARAM = hWnd
	SPHERECLIENTMSG_UNLINK = 1,	// LPARAM = hWnd
	SPHERECLIENTMSG_KEY = 2,		// act as if a key is pressed. // LPARAM = key
	SPHERECLIENTMSG_MOVE = 3,		// movement notification. (LPARAM=x,y)
	SPHERECLIENTMSG_RESYNC = 4,	// resync with the mul files. (probably been modified)
};

#endif

#ifdef SPHERE_SVR
#include "spheresvr.h"
#endif

#endif	// _INC_SPHERECOMMON_H

#ifndef _INC_COMMON_H
#define _INC_COMMON_H

#include <cstdio>
#include <cstdarg>
#include <climits>

#ifndef _WIN32
// Windows type definitions for Linux
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <strings.h>

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int UINT;
typedef unsigned int DWORD;
typedef int INT;
typedef long LONG;
typedef long long LONGLONG;
typedef const char* LPCSTR;
typedef const char* LPCTSTR;
typedef char* LPSTR;
typedef char* LPTSTR;
typedef int HRESULT;
typedef unsigned short WCHAR;
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* HINSTANCE;
typedef void* HWND;

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif
typedef int BOOL;

#define E_FAIL ((HRESULT)0x80004005L)
#define NO_ERROR 0
#define S_OK 0
#define S_FALSE 1
#define IS_ERROR(hr) ((hr) < 0)
#define FAILED(hr) ((hr) < 0)
#define SUCCEEDED(hr) ((hr) >= 0)
#define HRESULT_CODE(hr) ((hr) & 0xFFFF)
#define HRES_BAD_ARG_QTY E_FAIL
#define HRES_INVALID_HANDLE ((HRESULT)0x80070006L)
#define HRES_UNKNOWN_PROPERTY ((HRESULT)0x80020006L)
#define HRES_INTERNAL_ERROR ((HRESULT)0x80004005L)
#define FAR
#define _cdecl
#define __cdecl

// Win32 struct
struct POINT { long x; long y; };
struct POINTS { short x; short y; };
struct RECT { long left; long top; long right; long bottom; };

// Win32 macros
#define MAKEWORD(a, b)   ((WORD)(((BYTE)((DWORD)(a) & 0xff)) | ((WORD)((BYTE)((DWORD)(b) & 0xff))) << 8))
#define LOBYTE(w)        ((BYTE)((DWORD)(w) & 0xff))
#define HIBYTE(w)        ((BYTE)(((DWORD)(w) >> 8) & 0xff))
#define LOWORD(l)        ((WORD)((DWORD)(l) & 0xffff))
#define HIWORD(l)        ((WORD)(((DWORD)(l) >> 16) & 0xffff))
#define TEXT(x)          x
#define _TEXT(x)         (char*)x

#define IsBadReadPtr(p, len)    ((p) == NULL)
#define IsBadStringPtr(p, len)  ((p) == NULL)

// MSVC -> POSIX string function mappings
#define _stricmp  strcasecmp
#define _strnicmp strncasecmp
#define _strlwr(s) ({ char* _p = (s); while (*_p) { *_p = tolower(*_p); _p++; } (s); })
#define _strupr(s) ({ char* _p = (s); while (*_p) { *_p = toupper(*_p); _p++; } (s); })
#define _vsnprintf vsnprintf
#define _snprintf  snprintf

#define Sleep(mSec) usleep((mSec) * 1000)
#include <unistd.h>

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#endif // !_WIN32

#define MIN min
#define MAX max
#define IMULDIV(a,b,c) (((a)*(b))/(c))	// windows MulDiv will round ! 

#define ABS(n) (((n) < 0) ? (-(n)) : (n))

#ifndef _UNICODE		// _WIN32
#define TCHAR			char
#define LPCTSTR			LPCSTR
#define LPCWSTR			LPCSTR
#endif  // _UNICODE _WIN32
#ifndef _TEXT
#define _TEXT(x)		(TCHAR *)x
#endif	// _TEXT

// use to indicate that a function uses printf-style arguments, allowing GCC
// to validate the format string and arguments:
// a = 1-based index of format string
// b = 1-based index of arguments
// (note: add 1 to index for non-static class methods because 'this' argument
// is inserted in position 1)
#ifdef __GNUC__
#define __printfargs(a,b) __attribute__ ((format(printf, a, b)))
#else
#define __printfargs(a,b)
#endif

// Fallback ASSERT if not yet defined by spherecommon.h
#ifndef ASSERT
#include <cassert>
#define ASSERT assert
#endif

#ifndef MAKEDWORD
#define MAKEDWORD(low, high) ((DWORD)(((WORD)(low)) | (((DWORD)((WORD)(high))) << 16)))
#endif	// MAKEDWORD

#ifndef COUNTOF
#define COUNTOF(a)	(sizeof(a)/sizeof((a)[0]))
#endif

#define UID_INDEX DWORD
#define HASH_INDEX DWORD
#define HASH_COMPARE(a, b) (a>b)

#ifndef _1BITMASK
#define _1BITMASK(b)    (((size_t)1) << (b))
#endif

#ifndef _BITMASK // xstddef.h?
#define _BITMASK(b) 	(1<<(b))
#define _ISSET(w,b) 	((w)&_BITMASK(b))
#define _ISCLR(w,b) 	(!_ISSET(w,b))
#endif

#define _IS_SWITCH(c)    ((c) == '-' || (c) == '/' )	// command line switch.

template <int size>
class CBitArray
{
	BYTE m_bits[ (size + 7) / 8 ];
public:
	CBitArray() { memset(m_bits, 0, sizeof(m_bits)); }
	bool IsSet(int iBit) { return( iBit >= 0 && iBit < size && (m_bits[iBit >> 3] & (1 << (iBit & 7)))); }
	void SetBit(int iBit) { if ( iBit >= 0 && iBit < size ) m_bits[iBit >> 3] |= (1 << (iBit & 7)); }
};

#define ISWHITESPACE(ch)		 (isspace(ch)||(ch)==0xa0)	// isspace
#define GETNONWHITESPACE( pStr ) while ( ISWHITESPACE( (pStr)[0] )) { (pStr)++; }

enum LOGL_TYPE
{
	// critical level.
	LOGL_FATAL = 1, 	// fatal error ! cannot continue.
	LOGL_CRIT = 2, 	// critical. might not continue.
	LOGL_ERROR = 3, 	// non-fatal errors. can continue.
	LOGL_WARN = 4,	// strange.
	LOGL_EVENT = 5,	// Misc major events.
	LOGL_TRACE = 6,	// low level debug trace.
};

#define LOG_GROUP_TYPE DWORD

#define HRES_INVALID_HANDLE -1
#define HRES_PRIVILEGE_NOT_HELD -2
#define HRES_BAD_ARGUMENTS -3
#define HRES_INVALID_INDEX -4
#define HRES_INTERNAL_ERROR -5
#define HRES_BAD_ARG_QTY -5
#define HRES_UNKNOWN_PROPERTY -6
#define HRES_WRITE_FAULT -7
#define HRES_INVALID_FUNCTION -8

#define LOG_CR "\n"

#define DEBUG_MSG(_x_)		g_pLog->EventEvent _x_
#define DEBUG_TRACE(_x_)	g_pLog->EventTrace _x_
#define DEBUG_ERR(_x_)		g_pLog->EventError _x_
#define DEBUG_WARN(_x_)		g_pLog->EventWarn _x_

class CLogBase
{
public:
	virtual int EventStr(LOG_GROUP_TYPE dwGroupMask, LOGL_TYPE level, const char* pszMsg) = 0;

	void Event(LOG_GROUP_TYPE dwGroupMask, LOGL_TYPE level, const char* pszMsg, ...)
	{
		va_list vargs;
		va_start(vargs, pszMsg);
		TCHAR szBuf[1024];
		vsprintf(szBuf, pszMsg, vargs);
		va_end(vargs);
		EventStr(dwGroupMask, level, szBuf);
	}
	void EventEvent(LPCTSTR pszFormat, ...)
	{
		va_list vargs;
		va_start(vargs, pszFormat);
		TCHAR szBuf[1024];
		vsprintf(szBuf, pszFormat, vargs);
		va_end(vargs);
		EventStr(0, LOGL_EVENT, szBuf);
	}
	void EventTrace(LPCTSTR pszFormat, ...)
	{
		va_list vargs;
		va_start(vargs, pszFormat);
		TCHAR szBuf[1024];
		vsprintf(szBuf, pszFormat, vargs);
		va_end(vargs);
		EventStr(0, LOGL_TRACE, szBuf);
	}
	void EventError(LPCTSTR pszFormat, ...)
	{
		va_list vargs;
		va_start(vargs, pszFormat);
		TCHAR szBuf[1024];
		vsprintf(szBuf, pszFormat, vargs);
		va_end(vargs);
		EventStr(0, LOGL_ERROR, szBuf);
	}
	void EventWarn(LPCTSTR pszFormat, ...)
	{
		va_list vargs;
		va_start(vargs, pszFormat);
		TCHAR szBuf[1024];
		vsprintf(szBuf, pszFormat, vargs);
		va_end(vargs);
		EventStr(0, LOGL_WARN, szBuf);
	}
};

extern CLogBase* g_pLog;

class CGException
{
public:
	LOGL_TYPE m_eSeverity;
	CGException() : m_eSeverity(LOGL_EVENT) {}
	CGException(LOGL_TYPE level, int lastError, LPCTSTR pszMessage) : m_eSeverity(level) {}

	LOGL_TYPE GetSeverity() const { return m_eSeverity; }
	void GetErrorMessage(char* pBuf, int iBufLen) { /* STUB */ }
};

class CGSystemInfo
{
public:
	static bool IsNt()
	{
#ifdef _WIN32
		return true;	// Assume NT-based on modern Windows
#else
		return false;	// Not Windows
#endif
	}
};

class CAssocStrVal
{
public:
	LPCTSTR m_pszName;
	int m_iVal;

public:
	LPCTSTR FindValSorted(int iVal) const
	{
		// Walk the sorted table to find the name for a given value.
		// The table must be sorted by m_iVal and terminated with m_pszName==NULL.
		const CAssocStrVal* p = this;
		for ( ; p->m_pszName != NULL; p++ )
		{
			if ( iVal < p[1].m_iVal || p[1].m_pszName == NULL )
				return p->m_pszName;
		}
		return "";
	}
};

inline int CvtUNICODEToSystem(TCHAR* pOut, int iSizeOutBytes, WCHAR* pwChar, int iSizeInChars)
{
	// Convert UNICODE (wide) string to system (ASCII/Latin-1) string.
	int iLen = iSizeInChars;
	int iOutMax = iSizeOutBytes - 1;
	int i;
	for ( i = 0; i < iLen && i < iOutMax; i++ )
	{
		WCHAR wch = pwChar[i];
		if ( wch == 0 )
			break;
		pOut[i] = (TCHAR)( wch > 0xFF ? '?' : wch );
	}
	pOut[i] = '\0';
	return i;
}
inline int CvtSystemToUNICODE(WCHAR* wChar, int iSizeInBytes, LPCTSTR pInp)
{
	// Convert system (ASCII/Latin-1) string to UNICODE (wide) string.
	int iOutMax = (iSizeInBytes / sizeof(WCHAR)) - 1;
	int i;
	for ( i = 0; pInp[i] && i < iOutMax; i++ )
	{
		wChar[i] = (WCHAR)(unsigned char)pInp[i];
	}
	wChar[i] = 0;
	return i;
}

#endif // _INC_COMMON_H

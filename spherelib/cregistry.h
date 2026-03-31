#ifndef _INC_CREGISTRY_H
#define _INC_CREGISTRY_H

#ifdef _WIN32
class CGRegKey
{
public:
	CGRegKey() { throw "not implemented"; }
	CGRegKey(HKEY hKey) { throw "not implemented"; }

	LONG Open(LPCTSTR pszName, DWORD mode) { throw "not implemented"; }
	void Attach(HKEY hKey) { throw "not implemented"; }
	LONG QueryValue(LPCTSTR pszName, DWORD dwType, TCHAR *szValue, DWORD lSize) { throw "not implemented"; }
};
#endif // _WIN32

#endif // _INC_CREGISTRY_H

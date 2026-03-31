#include "stdafx.h"
#include <cstring>

#ifndef minimum
#define minimum(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef STRREV
static void _strrev(char* s) {
	int len = strlen(s);
	for (int i = 0; i < len / 2; i++) {
		char tmp = s[i]; s[i] = s[len-1-i]; s[len-1-i] = tmp;
	}
}
#define STRREV(s) _strrev(s)
#endif

#ifndef _WIN32
#define strcmpi _stricmp
#endif

/**
* @brief Default memory alloc size for CGString.
*
* - Empty World! (total strings on start =480,154)
*  - 16 bytes : memory:  8,265,143 [Mem=42,516 K] [reallocations=12,853]
*  - 32 bytes : memory: 16,235,008 [Mem=50,916 K] [reallocations=11,232]
*  - 36 bytes : memory: 17,868,026 [Mem=50,592 K] [reallocations=11,144]
*  - 40 bytes : memory: 19,627,696 [Mem=50,660 K] [reallocations=11,108]
*  - 42 bytes : memory: 20,813,400 [Mem=50,240 K] [reallocations=11,081] BEST
*  - 48 bytes : memory: 23,759,788 [Mem=58,704 K] [reallocations=11,048]
*  - 56 bytes : memory: 27,689,157 [Mem=57,924 K] [reallocations=11,043]
*  - 64 bytes : memory: 31,618,882 [Mem=66,260 K] [reallocations=11,043]
*  - 128 bytes : memory: 62,405,304 [Mem=98,128 K] [reallocations=11,042] <- was in [0.55R4.0.2 - 0.56a]
* - Full World! (total strings on start ~1,388,081)
*  - 16 bytes : memory:  29,839,759 [Mem=227,232 K] [reallocations=269,442]
*  - 32 bytes : memory:  53,335,580 [Mem=250,568 K] [reallocations=224,023]
*  - 40 bytes : memory:  63,365,178 [Mem=249,978 K] [reallocations=160,987]
*  - 42 bytes : memory:  66,120,092 [Mem=249,896 K] [reallocations=153,181] BEST
*  - 48 bytes : memory:  74,865,847 [Mem=272,016 K] [reallocations=142,813]
*  - 56 bytes : memory:  87,050,665 [Mem=273,108 K] [reallocations=141,507]
*  - 64 bytes : memory:  99,278,582 [Mem=295,932 K] [reallocations=141,388]
*  - 128 bytes : memory: 197,114,039 [Mem=392,056 K] [reallocations=141,234] <- was in [0.55R4.0.2 - 0.56a]
*/
#define	STRING_DEFAULT_SIZE	42

//#define DEBUG_STRINGS
#ifdef DEBUG_STRINGS
int gAmount = 0;  ///< Current amount of CGString.
ULONG gMemAmount = 0; ///< Total mem allocated by CGStrings.
int gReallocs = 0; ///< Total reallocs caused by CGString resizing.
#endif

//see os_unix.h
// #ifndef _WIN32
// void _strupr( TCHAR * pszStr )
// {
// 	// No portable UNIX/LINUX equiv to this.
// 	for ( ;pszStr[0] != '\0'; pszStr++ )
// 	{
// 		*pszStr = toupper( *pszStr );
// 	}
// }
//
// void _strlwr( TCHAR * pszStr )
// {
// 	// No portable UNIX/LINUX equiv to this.
// 	for ( ;pszStr[0] != '\0'; pszStr++ )
// 	{
// 		*pszStr = tolower( *pszStr );
// 	}
// }
// #endif


int strcpylen(TCHAR* pDst, LPCTSTR pSrc)
{
	strcpy(pDst, pSrc);
	return(strlen(pDst));
}


int strcpylen(TCHAR* pDst, LPCTSTR pSrc, int iMaxSize)
{
	// it does NOT include the iMaxSize element! (just like memcpy)
	// so iMaxSize=sizeof() is ok !
	ASSERT(iMaxSize);
	strncpy(pDst, pSrc, iMaxSize - 1);
	pDst[iMaxSize - 1] = '\0';	// always terminate.
	return(strlen(pDst));
}

//***************************************************************************
// -CGString

void CGString::Empty(bool bTotal)
{
	if (bTotal)
	{
		if (m_iMaxLength && m_pchData)
		{
#ifdef DEBUG_STRINGS
			gMemAmount -= m_iMaxLength;
#endif
			delete[] m_pchData;
			m_pchData = NULL;
			m_iMaxLength = 0;
		}
	}
	else m_iLength = 0;
}

int CGString::SetLength(int iNewLength)
{
	if (iNewLength >= m_iMaxLength)
	{
#ifdef DEBUG_STRINGS
		gMemAmount -= m_iMaxLength;
#endif
		m_iMaxLength = iNewLength + (STRING_DEFAULT_SIZE >> 1);	// allow grow, and always expand only
#ifdef DEBUG_STRINGS
		gMemAmount += m_iMaxLength;
		gReallocs++;
#endif
		TCHAR* pNewData = new TCHAR[m_iMaxLength + 1];
		ASSERT(pNewData);

		int iMinLength = minimum(iNewLength, m_iLength);
		strncpy(pNewData, m_pchData, iMinLength);
		pNewData[m_iLength] = 0;

		if (m_pchData) delete[] m_pchData;
		m_pchData = pNewData;
	}
	m_iLength = iNewLength;
	m_pchData[m_iLength] = 0;
	return m_iLength;
}

void CGString::Copy(LPCTSTR pszStr)
{
	if ((pszStr != m_pchData) && pszStr)
	{
		SetLength(strlen(pszStr));
		strcpy(m_pchData, pszStr);
	}
}

void CGString::FormatV(LPCTSTR pszFormat, va_list args)
{
	TCHAR pszTemp[CSTRING_MAX_LEN];
	_vsnprintf(pszTemp, sizeof(pszTemp) - 1, pszFormat, args);
	pszTemp[sizeof(pszTemp) - 1] = '\0';
	Copy(pszTemp);
}

void CGString::Add(TCHAR ch)
{
	int iLen = m_iLength;
	SetLength(iLen + 1);
	SetAt(iLen, ch);
}

void CGString::Add(LPCTSTR pszStr)
{
	int iLenCat = strlen(pszStr);
	if (iLenCat)
	{
		SetLength(iLenCat + m_iLength);
		strcat(m_pchData, pszStr);
		m_iLength = strlen(m_pchData);
	}
}

void CGString::Reverse()
{
	STRREV(m_pchData);
}

CGString::~CGString()
{
#ifdef DEBUG_STRINGS
	gAmount--;
#endif
	Empty(true);
}

CGString::CGString()
{
#ifdef DEBUG_STRINGS
	gAmount++;
#endif
	Init();
}

CGString::CGString(LPCTSTR pStr)
{
	m_iMaxLength = m_iLength = 0;
	m_pchData = NULL;
	Copy(pStr);
}

CGString::CGString(const CGString& s)
{
	m_iMaxLength = m_iLength = 0;
	m_pchData = NULL;
	Copy(s.GetPtr());
}

bool CGString::IsValid() const
{
	if (!m_iMaxLength) return false;
	return(m_pchData[m_iLength] == '\0');
}

int CGString::GetLength() const
{
	return (m_iLength);
}
bool CGString::IsEmpty() const
{
	return(!m_iLength);
}
TCHAR& CGString::ReferenceAt(int nIndex)       // 0 based
{
	ASSERT(nIndex < m_iLength);
	return m_pchData[nIndex];
}
TCHAR CGString::GetAt(int nIndex) const      // 0 based
{
	ASSERT(nIndex <= m_iLength);	// allow to get the null char
	return(m_pchData[nIndex]);
}
void CGString::SetAt(int nIndex, TCHAR ch)
{
	ASSERT(nIndex < m_iLength);
	m_pchData[nIndex] = ch;
	if (!ch) m_iLength = strlen(m_pchData);	// \0 inserted. line truncated
}
LPCTSTR CGString::GetPtr() const
{
	return(m_pchData);
}
void _cdecl CGString::Format(LPCTSTR pStr, ...)
{
	va_list vargs;
	va_start(vargs, pStr);
	FormatV(pStr, vargs);
	va_end(vargs);
}

void CGString::FormatVal(long iVal)
{
	Format("%ld", iVal);
}

void CGString::FormatLLVal(long long iVal)
{
	Format("%lld", iVal);
}

void CGString::FormatULLVal(unsigned long long iVal)
{
	Format("%llu", iVal);
}

void CGString::FormatUVal(unsigned long iVal)
{
	Format("%lu", iVal);
}

void CGString::FormatHex(DWORD dwVal)
{
	// In principle, all values in sphere logic are
	// signed.. 
	// If iVal is negative we MUST hexformat it as
	// 64 bit int or reinterpreting it in a 
	// script might completely mess up
	long long dwVal64 = ((int)dwVal);
	if (dwVal64 < 0)
		return FormatLLHex(dwVal64);
	Format("0%lx", dwVal);
}

void CGString::FormatLLHex(unsigned long long dwVal)
{
	Format("0%llx", dwVal);
}

int CGString::Compare(LPCTSTR pStr) const
{
	return (strcmp(m_pchData, pStr));
}
int CGString::CompareNoCase(LPCTSTR pStr) const
{
	return (strcmpi(m_pchData, pStr));
}

int CGString::indexOf(TCHAR c)
{
	return indexOf(c, 0);
}

int CGString::indexOf(TCHAR c, int offset)
{
	if (offset < 0)
		return -1;

	int len = strlen(m_pchData);
	if (offset >= len)
		return -1;

	for (int i = offset; i < len; i++)
	{
		if (m_pchData[i] == c)
		{
			return i;
		}
	}
	return -1;
}

int CGString::indexOf(CGString str, int offset)
{
	if (offset < 0)
		return -1;

	int len = strlen(m_pchData);
	if (offset >= len)
		return -1;

	int slen = str.GetLength();
	if (slen > len)
		return -1;

	TCHAR* str_value = new TCHAR[slen + 1];
	strcpy(str_value, str.GetPtr());
	TCHAR firstChar = str_value[0];

	for (int i = offset; i < len; i++)
	{
		TCHAR c = m_pchData[i];
		if (c == firstChar)
		{
			int rem = len - i;
			if (rem >= slen)
			{
				int j = i;
				int k = 0;
				bool found = true;
				while (k < slen)
				{
					if (m_pchData[j] != str_value[k])
					{
						found = false;
						break;
					}
					j++; k++;
				}
				if (found)
				{
					delete[] str_value;
					return i;
				}
			}
		}
	}

	delete[] str_value;
	return -1;
}

int CGString::indexOf(CGString str)
{
	return indexOf(str, 0);
}

int CGString::lastIndexOf(TCHAR c)
{
	return lastIndexOf(c, 0);
}

int CGString::lastIndexOf(TCHAR c, int from)
{
	if (from < 0)
		return -1;

	int len = strlen(m_pchData);
	if (from > len)
		return -1;

	for (int i = (len - 1); i >= from; i--)
	{
		if (m_pchData[i] == c)
		{
			return i;
		}
	}
	return -1;
}

int CGString::lastIndexOf(CGString str, int from)
{
	if (from < 0)
		return -1;

	int len = strlen(m_pchData);
	if (from >= len)
		return -1;
	int slen = str.GetLength();
	if (slen > len)
		return -1;

	TCHAR* str_value = new TCHAR[slen + 1];
	strcpy(str_value, str.GetPtr());
	TCHAR firstChar = str_value[0];
	for (int i = (len - 1); i >= from; i--)
	{
		TCHAR c = m_pchData[i];
		if (c == firstChar)
		{
			int rem = i;
			if (rem >= slen)
			{
				int j = i;
				int k = 0;
				bool found = true;
				while (k < slen)
				{
					if (m_pchData[j] != str_value[k])
					{
						found = false;
						break;
					}
					j++; k++;
				}
				if (found)
				{
					delete[] str_value;
					return i;
				}
			}
		}
	}

	delete[] str_value;
	return -1;
}

int CGString::lastIndexOf(CGString str)
{
	return lastIndexOf(str, 0);
}

void CGString::Init()
{
	m_iMaxLength = STRING_DEFAULT_SIZE;
#ifdef DEBUG_STRINGS
	gMemAmount += m_iMaxLength;
#endif
	m_iLength = 0;
	m_pchData = new TCHAR[m_iMaxLength + 1];
	m_pchData[m_iLength] = 0;
}

//***************************************************************************
// String global functions.

static int		Str_iTemp = 0;
static TCHAR	Str_szTemp[8][CSTRING_MAX_LEN];

TCHAR* Str_GetTemp(void)
{
	// Some scratch string space, random uses
	if (++Str_iTemp >= 8)
		Str_iTemp = 0;
	return(Str_szTemp[Str_iTemp]);
}
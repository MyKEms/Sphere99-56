/**
* @file CString.
* @brief Custom String implementation.
*/

#ifndef _INC_CSTRING_H
#define _INC_CSTRING_H

#include "common.h"

#define CSTRING_MAX_LEN 8*1024

#define CString CGString

/**
* @brief Custom String implementation.
*/
class CGString
{
private:
	TCHAR* m_pchData; ///< Data pointer.
	int		m_iLength; ///< Length of string.
	int		m_iMaxLength; ///< Size of memory allocated pointed by m_pchData.

public:
	static const char* m_sClassName;

private:
	/**
	* @brief Initializes internal data.
	*
	* Allocs STRING_DEFAULT_SIZE by default. If DEBUG_STRINGS setted, updates statistical information (total memory allocated).
	*/
	void Init();

public:
	/**
	* @brief CGString destructor.
	*
	* If DEBUG_STRINGS setted, updates statistical information (total CGString instantiated).
	*/
	~CGString();
	/**
	* @brief Default constructor.
	*
	* Initializes string. If DEBUG_STRINGS setted, update statistical information (total CGString instantiated).
	* @see Init()
	*/
	CGString();
	/**
	* @brief Copy constructor.
	*
	* @see Copy()
	* @param pStr string to copy.
	*/
	CGString(LPCTSTR pStr);
	/**
	* @brief Copy constructor.
	*
	* @see Copy()
	* @param pStr string to copy.
	*/
	CGString(const CGString& s);

	void FormatErrorMessage(const HRESULT hRes)
	{
		Format("Error code: %d", hRes);
	}

	/**
	* @brief Check if there is data allocated and if the string is zero ended.
	* @return true if is valid, false otherwise.
	*/
	bool IsValid() const;
	/**
	* @brief Change the length of the CGString.
	*
	* If the new length is lesser than the current lenght, only set a zero at the end of the string.
	* If the new length is bigger than the current length, alloc memory for the string and copy.
	* If DEBUG_STRINGS setted, update statistical information (reallocs count, total memory allocated).
	* @param iLen new length of the string.
	* @return the new length of the CGString.
	*/
	int SetLength(int iLen);
	/**
	* @brief Get the length of the CGString.
	* @return the length of the CGString.
	*/
	int GetLength() const;
	/**
	* @brief Check the length of the CGString.
	* @return true if length is 0, false otherwise.
	*/
	bool IsEmpty() const;
	/**
	* @brief Sets length to zero.
	*
	* If bTotal is true, then free the memory allocated. If DEBUG_STRINGS setted, update statistical information (total memory allocated).
	* @param bTotal true for free the allocated memory.
	*/
	void Empty(bool bTotal = false);
	/**
	* @brief Copy a string into the CGString.
	* @see SetLength()
	* @see strcpylen()
	* @param pStr string to copy.
	*/
	void Copy(LPCTSTR pStr);

	/**
	* @brief Gets the reference to character a specified position (0 based).
	* @param nIndex position of the character.
	* @return reference to character in position nIndex.
	*/
	TCHAR& ReferenceAt(int nIndex);
	/**
	* @brief Gets the caracter in a specified position (0 based).
	* @param nIndex position of the character.
	* @return character in position nIndex.
	*/
	TCHAR GetAt(int nIndex) const;
	/**
	* @brief Puts a character in a specified position (0 based).
	*
	* If character is 0, updates the length of the string (truncated).
	* @param nIndex position to put the character.
	* @param ch character to put.
	*/
	void SetAt(int nIndex, TCHAR ch);
	/**
	* @brief Gets the internal pointer.
	* @return Pointer to internal data.
	*/
	LPCTSTR GetPtr() const;

	/**
	* @brief Join a formated string (printf like) with values and copy into this.
	* @param pStr formated string.
	* @param args list of values.
	*/
	void FormatV(LPCTSTR pStr, va_list args);
	/**
	* @brief Join a formated string (printf like) with values and copy into this.
	* @see FormatV()
	* @param pStr formated string.
	* @param ... list of values.
	*/
	void _cdecl Format(LPCTSTR pStr, ...) __printfargs(2, 3);
	/**
	* @brief Print a long value into the string.
	* @see Format()
	* @param iVal value to print.
	*/
	void FormatVal(long iVal);
	/**
	* @brief Print a long long value into the string.
	* @see Format()
	* @param iVal value to print.
	*/
	void FormatLLVal(long long iVal);
	/**
	* @brief Print a unsigned long long value into the string.
	* @see Format()
	* @param iVal value to print.
	*/
	void FormatULLVal(unsigned long long iVal);
	/**
	* @brief Print a unsigned long value into the string.
	* @see Format()
	* @param iVal value to print.
	*/
	void FormatUVal(unsigned long iVal);
	/**
	* @brief Print a DWORD value into the string (hex format).
	* @see Format()
	* @param iVal value to print.
	*/
	void FormatHex(DWORD dwVal);
	/**
	* @brief Print a unsigned long long value into the string (hex format).
	* @see Format()
	* @param iVal value to print.
	*/
	void FormatLLHex(unsigned long long dwVal);

	/**
	* @brief Compares the CGString to string pStr (strcmp wrapper).
	*
	* This function starts comparing the first character of CGString and the string.
	* If they are equal to each other, it continues with the following
	* pairs until the characters differ or until a terminating null-character
	* is reached. This function performs a binary comparison of the characters.
	* @param pStr string to compare.
	* @return <0 if te first character that not match has lower value in CGString than in pStr. 0 if hte contents of both are equal. >0 if the first character that does not match has greater value in CGString than pStr.
	*/
	int Compare(LPCTSTR pStr) const;
	/**
	* @brief Compares the CGString to string pStr (case insensitive) (_strcmpi wrapper).
	*
	* This function starts comparing the first character of CGString and the string.
	* If they are equal to each other, it continues with the following
	* pairs until the characters differ or until a terminating null-character
	* is reached. This function performs a case insensitive comparison of the characters.
	* @param pStr string to compare.
	* @return <0 if te first character that not match has lower value in CGString than in pStr. 0 if hte contents of both are equal. >0 if the first character that does not match has greater value in CGString than pStr.
	*/
	int CompareNoCase(LPCTSTR pStr) const;
	/**
	* @brief Look for the first occurence of c in CGString.
	* @param c character to look for.
	* @return position of the character in CGString if any, -1 otherwise.
	*/
	int indexOf(TCHAR c);
	/**
	* @brief Look for the first occurence of c in CGString from a position.
	* @param c character to look for.
	* @param offset position from start the search.
	* @return position of the character in CGString if any, -1 otherwise.
	*/
	int indexOf(TCHAR c, int offset);
	/**
	* @brief Look for the first occurence of a substring in CGString from a position.
	* @param str substring to look for.
	* @param offset position from start the search.
	* @return position of the substring in CGString if any, -1 otherwise.
	*/
	int indexOf(CGString str, int offset);
	/**
	* @brief Look for the first occurence of a substring in CGString.
	* @param str substring to look for.
	* @return position of the substring in CGString if any, -1 otherwise.
	*/
	int indexOf(CGString str);
	/**
	* @brief Look for the last occurence of c in CGString.
	* @param c character to look for.
	* @return position of the character in CGString if any, -1 otherwise.
	*/
	int lastIndexOf(TCHAR c);
	/**
	* @brief Look for the last occurence of c in CGString from a position to the end.
	* @param c character to look for.
	* @param from position where stop the search.
	* @return position of the character in CGString if any, -1 otherwise.
	*/
	int lastIndexOf(TCHAR c, int from);
	/**
	* @brief Look for the last occurence of a substring in CGString from a position to the end.
	* @param str substring to look for.
	* @param from position where stop the search.
	* @return position of the substring in CGString if any, -1 otherwise.
	*/
	int lastIndexOf(CGString str, int from);
	/**
	* @brief Look for the last occurence of a substring in CGString.
	* @param str substring to look for.
	* @return position of the substring in CGString if any, -1 otherwise.
	*/
	int lastIndexOf(CGString str);
	/**
	* @brief Adds a char at the end of the CGString.
	* @param ch character to add.
	*/
	void Add(TCHAR ch);
	/**
	* @brief Adds a string at the end of the CGString.
	* @parampszStrh string to add.
	*/
	void Add(LPCTSTR pszStr);
	/**
	* @brief Reverses the CGString.
	*/
	void Reverse();
	/**
	* @brief Changes the capitalization of CGString to upper.
	*/
	void MakeUpper() { _strupr(m_pchData); }
	/**
	* @brief Changes the capitalization of CGString to lower.
	*/
	void MakeLower() { _strlwr(m_pchData); }

	/**
	* @brief Gets the caracter in a specified position (0 based).
	* @see GetAt()
	* @param nIndex position of the character.
	* @return character in position nIndex.
	*/
	TCHAR operator[](int nIndex) const
	{
		return GetAt(nIndex);
	}
	/**
	* @brief Gets the reference to character a specified position (0 based).
	* @see ReferenceAt()
	* @param nIndex position of the character.
	* @return reference to character in position nIndex.
	*/
	TCHAR& operator[](int nIndex)
	{
		return ReferenceAt(nIndex);
	}
	/**
	* @brief cast as const LPCSTR.
	* @return internal data pointer.
	*/
	operator LPCTSTR() const
	{
		return(GetPtr());
	}
	// operator CString&() removed - returning reference to temporary is invalid
	/**
	* @brief Concatenate CGString with a string.
	* @param psz string to concatenate with.
	* @return The result of concatenate the CGString with psz.
	*/
	const CGString& operator+=(LPCTSTR psz)	// like strcat
	{
		Add(psz);
		return(*this);
	}
	/**
	* @brief Concatenate CGString with a character.
	* @param ch character to concatenate with.
	* @return The result of concatenate the CGString with ch.
	*/
	const CGString& operator+=(TCHAR ch)
	{
		Add(ch);
		return(*this);
	}
	/**
	* @brief Copy supplied string into the CGString.
	* @param pStr string to copy.
	* @return the CGString.
	*/
	const CGString& operator=(LPCTSTR pStr)
	{
		Copy(pStr);
		return(*this);
	}
	/**
	* @brief Copy supplied CGString into the CGString.
	* @param s CGString to copy.
	* @return the CGString.
	*/
	const CGString& operator=(const CGString& s)
	{
		Copy(s.GetPtr());
		return(*this);
	}
};

/**
* match result defines
*/
enum MATCH_TYPE
{
	MATCH_INVALID = 0,
	MATCH_VALID,		///< valid match
	MATCH_END,			///< premature end of pattern string
	MATCH_ABORT,		///< premature end of text string
	MATCH_RANGE,		///< match failure on [..] construct
	MATCH_LITERAL,		///< match failure on literal match
	MATCH_PATTERN		///< bad pattern
};

extern int strcpylen(TCHAR* pDst, LPCTSTR pSrc);
extern int strcpylen(TCHAR* pDst, LPCTSTR pSrc, int imaxlen);

// extern TCHAR * Str_GetTemporary(int amount = 1);
inline LPCTSTR Str_GetArticleAndSpace(LPCTSTR pszWords) { return pszWords; /* STUB */ }
extern TCHAR* Str_GetTemp();
inline int Str_GetBare(TCHAR* pszOut, LPCTSTR pszInp, int iMaxSize, LPCTSTR pszStrip = NULL)
{
	// Copy the string removing/replacing any special characters in it.
	// pszStrip = characters to strip (NULL = use defaults: "{}[]|~")
	static const TCHAR szDefaultStrip[] = "{}[]|~";
	if ( pszStrip == NULL )
		pszStrip = szDefaultStrip;
	int j = 0;
	for ( int i = 0; pszInp[i] && j < iMaxSize - 1; i++ )
	{
		TCHAR ch = pszInp[i];
		if ( ch < ' ' ) // control chars
		{
			if ( ch == '\t' )
			{
				pszOut[j++] = ' ';
			}
			continue;
		}
		// Check strip list
		bool fStrip = false;
		for ( LPCTSTR p = pszStrip; *p; p++ )
		{
			if ( ch == *p )
			{
				fStrip = true;
				break;
			}
		}
		if ( ! fStrip )
		{
			pszOut[j++] = ch;
		}
	}
	pszOut[j] = '\0';
	return j;
}
inline TCHAR* Str_TrimWhitespace(TCHAR* pStr)
{
	// Skip leading whitespace and trim trailing whitespace.
	TCHAR* p = pStr;
	GETNONWHITESPACE(p);
	// Move string to start if needed
	if ( p != pStr )
	{
		int len = strlen(p);
		memmove(pStr, p, len + 1);
	}
	// Trim trailing whitespace
	int len = strlen(pStr);
	while ( len > 0 && ISWHITESPACE(pStr[len - 1]) )
	{
		pStr[--len] = '\0';
	}
	return pStr;
}
inline TCHAR* Str_GetNonWhitespace(LPCTSTR pStr)
{
	// Skip whitespace, return pointer to first non-whitespace char.
	TCHAR* p = (TCHAR*)pStr;
	GETNONWHITESPACE(p);
	return p;
}
inline bool Str_Parse(TCHAR* pLine, TCHAR** ppArg, LPCTSTR pSep)
{
	// Split line at separator into key + arg
	TCHAR* p = pLine;
	while (*p)
	{
		if (*p == '=')
		{
			*p = '\0';
			*ppArg = p + 1;
			GETNONWHITESPACE(*ppArg);
			return true;
		}
		if (pSep)
		{
			for (LPCTSTR s = pSep; *s; s++)
			{
				if (*p == *s)
				{
					*p = '\0';
					*ppArg = p + 1;
					GETNONWHITESPACE(*ppArg);
					return true;
				}
			}
		}
		else if (ISWHITESPACE(*p))
		{
			*p = '\0';
			*ppArg = p + 1;
			GETNONWHITESPACE(*ppArg);
			return true;
		}
		p++;
	}
	*ppArg = p; // empty string at end
	return false;
}
inline bool Str_Parse(TCHAR* pLine, TCHAR** ppArg)
{
	return Str_Parse(pLine, ppArg, (LPCTSTR)NULL);
}
// Legacy overloads for callers that pass TCHAR* instead of TCHAR**
// These callers are broken (can't modify caller's pointer by value) but must compile.
inline bool Str_Parse(TCHAR* pLine, TCHAR* pArg2, LPCTSTR pSep)
{
	return false; // STUB - caller needs migration to TCHAR** form
}
inline bool Str_Parse(TCHAR* pLine, TCHAR* pArg2)
{
	return false; // STUB - caller needs migration to TCHAR** form
}
inline int Str_GetEndWhitespace(LPCTSTR pStr, int iLen)
{
	while (iLen > 0 && ISWHITESPACE(pStr[iLen - 1]))
		iLen--;
	return iLen;
}
inline int Str_ParseCmds(LPCTSTR pStr, TCHAR** ppCmds, int iCmdCount, LPCTSTR lpcSeparators = NULL)
{
	// Parse a string into multiple commands by separators.
	// pStr must be a modifiable string (despite LPCTSTR).
	// Returns the number of parts found.
	static const TCHAR szDefaultSep[] = ",";
	if ( lpcSeparators == NULL || *lpcSeparators == '\0' )
		lpcSeparators = szDefaultSep;

	TCHAR* pLine = (TCHAR*) pStr;
	int iCount = 0;

	while ( iCount < iCmdCount )
	{
		// skip leading whitespace
		while ( *pLine && ISWHITESPACE(*pLine) )
			pLine++;
		ppCmds[iCount++] = pLine;

		// find next separator
		bool fFound = false;
		for ( ; *pLine; pLine++ )
		{
			for ( LPCTSTR p = lpcSeparators; *p; p++ )
			{
				if ( *pLine == *p )
				{
					*pLine++ = '\0';
					fFound = true;
					break;
				}
			}
			if ( fFound )
				break;
		}
		if ( ! *pLine && ! fFound )
			break;
	}
	// fill remaining slots with empty strings
	for ( int i = iCount; i < iCmdCount; i++ )
	{
		ppCmds[i] = pLine; // points to '\0' at end
	}
	return iCount;
}
inline int Str_ParseCmdsStr(LPCTSTR pStr, TCHAR** ppCmds, int iCmdCount, LPCTSTR lpcSeparators)
{
	return Str_ParseCmds(pStr, ppCmds, iCmdCount, lpcSeparators);
}
inline MATCH_TYPE Str_Match(LPCTSTR pStr, LPCTSTR pPattern)
{
	// Simple wildcard match. * matches any sequence, ? matches single char.
	if ( pPattern == NULL || pStr == NULL )
		return MATCH_ABORT;
	while ( *pPattern )
	{
		if ( *pPattern == '*' )
		{
			pPattern++;
			if ( *pPattern == '\0' )
				return MATCH_VALID; // trailing * matches everything
			while ( *pStr )
			{
				if ( Str_Match(pStr, pPattern) == MATCH_VALID )
					return MATCH_VALID;
				pStr++;
			}
			return MATCH_ABORT;
		}
		if ( *pStr == '\0' )
			return MATCH_ABORT;
		if ( *pPattern == '?' || toupper(*pPattern) == toupper(*pStr) )
		{
			pPattern++;
			pStr++;
		}
		else
		{
			return MATCH_ABORT;
		}
	}
	return( *pStr == '\0' ? MATCH_VALID : MATCH_ABORT );
}
inline int Str_FindWord(LPCTSTR pStr, LPCTSTR pWord)
{
	// Find a word in a comma-separated list. Return the index. -1 = not found.
	if ( pStr == NULL || pWord == NULL )
		return -1;
	int iWordLen = strlen(pWord);
	int iIndex = 0;
	while ( *pStr )
	{
		// skip whitespace
		while ( *pStr && ISWHITESPACE(*pStr) )
			pStr++;
		// compare
		if ( ! _strnicmp(pStr, pWord, iWordLen) )
		{
			TCHAR ch = pStr[iWordLen];
			if ( ch == '\0' || ch == ',' || ISWHITESPACE(ch) )
				return iIndex;
		}
		// skip to next word
		while ( *pStr && *pStr != ',' )
			pStr++;
		if ( *pStr == ',' )
			pStr++;
		iIndex++;
	}
	return -1;
}
inline void Str_EscSeqAdd(LPCTSTR pStr1, LPCTSTR pStr2, int iSize) { /* STUB */ }
inline void Str_EscSeqRemove(LPCTSTR pStr1, LPCTSTR pStr2, int iSize) { /* STUB */ }

inline UINT Str_ahextou(LPCTSTR pszStr)
{
	// Convert hex string to unsigned int. Supports optional "0x" or "0" prefix.
	if ( pszStr == NULL )
		return 0;
	// skip whitespace
	while ( ISWHITESPACE(*pszStr) )
		pszStr++;
	// skip optional "0x" or "0X" prefix
	if ( pszStr[0] == '0' && ( pszStr[1] == 'x' || pszStr[1] == 'X' ) )
		pszStr += 2;
	UINT val = 0;
	for ( ; ; pszStr++ )
	{
		TCHAR ch = *pszStr;
		if ( ch >= '0' && ch <= '9' )
			val = (val << 4) | (ch - '0');
		else if ( ch >= 'a' && ch <= 'f' )
			val = (val << 4) | (ch - 'a' + 10);
		else if ( ch >= 'A' && ch <= 'F' )
			val = (val << 4) | (ch - 'A' + 10);
		else
			break;
	}
	return val;
}

class CScriptProp;
class CScriptMethod;

// Case-insensitive key lookup in a NULL-terminated table of LPCTSTR.
inline int s_FindKeyInTable(LPCTSTR pszKey, const LPCTSTR pTable[])
{
	if ( !pszKey || !pTable )
		return -1;
	for ( int i = 0; pTable[i]; i++ )
	{
		if ( !_stricmp(pszKey, pTable[i]) )
			return i;
	}
	return -1;
}

// Forward declarations -- full implementations are in CScript.h after classes are defined.
int s_FindKeyInTable(LPCTSTR pszKey, const CScriptProp pTable[]);
int s_FindKeyInTable(LPCTSTR pszKey, const CScriptMethod pTable[]);

// Case-insensitive exact match in a NULL-terminated LPCTSTR table.
inline int FindTable(LPCTSTR pFind, LPCTSTR const* ppTable)
{
	if ( !pFind || !ppTable )
		return -1;
	for ( int i = 0; ppTable[i]; i++ )
	{
		if ( !_stricmp(pFind, ppTable[i]) )
			return i;
	}
	return -1;
}

// Case-insensitive exact match in a sorted NULL-terminated LPCTSTR table.
inline int FindTableSorted(LPCTSTR pFind, LPCTSTR const* ppTable, int count = -1)
{
	if ( !pFind || !ppTable )
		return -1;
	for ( int i = 0; (count < 0) ? (ppTable[i] != NULL) : (i < count); i++ )
	{
		if ( !ppTable[i] )
			break;
		if ( !_stricmp(pFind, ppTable[i]) )
			return i;
	}
	return -1;
}

// Case-insensitive prefix match in a NULL-terminated LPCTSTR table.
inline int FindTableHead(LPCTSTR pFind, LPCTSTR const* ppTable, int count = -1)
{
	if ( !pFind || !ppTable )
		return -1;
	for ( int i = 0; (count < 0) ? (ppTable[i] != NULL) : (i < count); i++ )
	{
		if ( !ppTable[i] )
			break;
		size_t len = strlen(ppTable[i]);
		if ( !_strnicmp(pFind, ppTable[i], len) )
			return i;
	}
	return -1;
}

// Case-insensitive prefix match in a CAssocStrVal table (NULL m_pszName terminates).
inline int FindTableHead(LPCTSTR pFind, CAssocStrVal const* ppTable, int count = -1)
{
	if ( !pFind || !ppTable )
		return -1;
	for ( int i = 0; (count < 0) ? (ppTable[i].m_pszName != NULL) : (i < count); i++ )
	{
		if ( !ppTable[i].m_pszName )
			break;
		size_t len = strlen(ppTable[i].m_pszName);
		if ( !_strnicmp(pFind, ppTable[i].m_pszName, len) )
			return i;
	}
	return -1;
}

// Case-insensitive prefix match in a sorted NULL-terminated LPCTSTR table.
inline int FindTableHeadSorted(LPCTSTR pFind, LPCTSTR const* ppTable, int count = -1)
{
	// Same as FindTableHead for now (linear scan).
	return FindTableHead(pFind, ppTable, count);
}

#endif // _INC_CSTRING_H
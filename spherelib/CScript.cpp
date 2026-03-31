//
// CScript.cpp
// Script file parser for SphereServer 0.99
// Parses .scp files with [SECTION] headers and KEY=VALUE pairs
//

#include "spherecommon.h"

///////////////////////////////////////////////////////////////
// CScript

CScript::CScript()
{
	m_iLineNum = 0;
	m_fSectionHead = false;
	m_lSectionData = 0;
	m_pszKey = m_szLine;
	m_pszArg = m_szLine;
	m_szLine[0] = '\0';
	m_szSection[0] = '\0';
}

///////////////////////////////////////////////////////////////
// Parsing helpers

size_t CScript::ParseKeyEnd()
{
	// Remove trailing whitespace and comments from the line buffer
	ASSERT(m_pszKey);

	size_t iLen = 0;
	for (; iLen < SCRIPT_MAX_LINE_LEN; iLen++)
	{
		TCHAR ch = m_pszKey[iLen];
		if (ch == '\0')
			break;
		if ((ch == '/') && (m_pszKey[iLen + 1] == '/'))	// remove // comment
			break;
	}

	// Trim trailing whitespace
	while (iLen > 0 && ISWHITESPACE(m_pszKey[iLen - 1]))
		iLen--;

	m_pszKey[iLen] = '\0';
	return iLen;
}

void CScript::ParseKey()
{
	// Split line at '=' or first whitespace into key and arg
	ASSERT(m_pszKey);

	// Skip leading whitespace
	GETNONWHITESPACE(m_pszKey);

	m_pszArg = m_pszKey;

	// Find the separator: '=' or whitespace
	while (*m_pszArg)
	{
		if (*m_pszArg == '=')
		{
			*m_pszArg = '\0';
			m_pszArg++;
			// Skip whitespace after '='
			GETNONWHITESPACE(m_pszArg);
			return;
		}
		if (ISWHITESPACE(*m_pszArg))
		{
			*m_pszArg = '\0';
			m_pszArg++;
			GETNONWHITESPACE(m_pszArg);
			return;
		}
		m_pszArg++;
	}
	// No separator found - arg is empty string at end
}

///////////////////////////////////////////////////////////////
// Reading

bool CScript::ReadTextLine(bool fRemoveBlanks)
{
	// Read a line from the file into line buffer
	// fRemoveBlanks: skip blank/comment-only lines
	while (ReadString(m_szLine, sizeof(m_szLine)))
	{
		m_iLineNum++;
		m_pszKey = m_szLine;

		// Remove CR/LF
		size_t len = strlen(m_szLine);
		while (len > 0 && (m_szLine[len - 1] == '\n' || m_szLine[len - 1] == '\r'))
		{
			m_szLine[--len] = '\0';
		}

		if (fRemoveBlanks)
		{
			if (ParseKeyEnd() <= 0)
				continue;
		}
		return true;
	}

	m_szLine[0] = '\0';
	m_pszKey = m_szLine;
	return false;
}

bool CScript::ReadLine(bool fRemoveBlanks)
{
	// Read next line within current section
	if (!ReadTextLine(fRemoveBlanks))
		return false;
	if (m_pszKey[0] == '[')	// hit next section
	{
		m_fSectionHead = true;
		return false;
	}
	return true;
}

bool CScript::ReadKeyParse()
{
	// Read next key=value line in current section, parse into key and arg
	if (!ReadLine())
		return false;

	ASSERT(m_pszKey);
	GETNONWHITESPACE(m_pszKey);
	ParseKey();
	return true;
}

///////////////////////////////////////////////////////////////
// Section navigation

bool CScript::FindTextHeader(LPCTSTR pszName)
{
	// Search for a text header from the beginning of file
	ASSERT(pszName);

	SeekToBegin();
	m_iLineNum = 0;

	size_t iLen = strlen(pszName);
	ASSERT(iLen);
	do
	{
		if (!ReadTextLine(false))
			return false;
		if (IsKeyHead("[EOF]", 5))
			return false;
	} while (!IsKeyHead(pszName, iLen));
	return true;
}

bool CScript::FindNextSection()
{
	// Find the next [SECTION] in the file
	// Returns false on EOF

	if (m_fSectionHead)
	{
		// Previous ReadLine already found a section header
		m_pszKey = m_szLine;
		ASSERT(m_pszKey);
		m_fSectionHead = false;
		if (m_pszKey[0] == '[')
			goto foundit;
	}

	for (;;)
	{
		if (!ReadTextLine(true))
		{
			m_lSectionData = GetPosition();
			return false;
		}
		if (m_pszKey[0] == '[')
			break;
	}

foundit:
	// Parse section name: remove [ and ]
	m_pszKey++;
	size_t iLen = strlen(m_pszKey);
	for (size_t i = 0; i < iLen; i++)
	{
		if (m_pszKey[i] == ']')
		{
			m_pszKey[i] = '\0';
			break;
		}
	}

	// Store section name
	strncpy(m_szSection, m_pszKey, sizeof(m_szSection) - 1);
	m_szSection[sizeof(m_szSection) - 1] = '\0';

	m_lSectionData = GetPosition();
	if (IsSectionType("EOF"))
		return false;

	// Split section type from section args
	ParseKey();
	return true;
}

bool CScript::FindSection(LPCTSTR pszName, UINT uModeFlags)
{
	// Find a specific [SECTION NAME] from beginning of file
	ASSERT(pszName);

	SeekToBegin();
	m_iLineNum = 0;

	while (FindNextSection())
	{
		if (_stricmp(GetKey(), pszName) == 0)
			return true;
	}
	return false;
}

bool CScript::FindKey(LPCTSTR pszName)
{
	// Find a key in the current section
	ASSERT(pszName);

	while (ReadKeyParse())
	{
		if (IsKey(pszName))
			return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////
// Key comparison

bool CScript::IsKeyHead(LPCTSTR lpszKey, int iLen)
{
	return (_strnicmp(m_pszKey, lpszKey, iLen) == 0);
}

bool CScript::IsKey(LPCTSTR lpszKey)
{
	return (_stricmp(GetKey(), lpszKey) == 0);
}

bool CScript::IsSectionType(LPCTSTR lpszSectionType)
{
	return (_stricmp(GetKey(), lpszSectionType) == 0);
}

///////////////////////////////////////////////////////////////
// Key/value access

LPCTSTR CScript::GetArgStr()
{
	// Get the argument, stripping surrounding quotes if present
	LPCTSTR pszStr = GetArgRaw();
	if (*pszStr != '"')
		return pszStr;

	pszStr++;
	// Find and remove closing quote
	size_t len = strlen(pszStr);
	if (len > 0 && pszStr[len - 1] == '"')
	{
		const_cast<TCHAR*>(pszStr)[len - 1] = '\0';
	}
	return pszStr;
}

int CScript::GetArgInt()
{
	LPCTSTR pszArg = GetArgRaw();
	if (!pszArg || !*pszArg)
		return 0;

	// Handle hex values (0x prefix or plain hex)
	if (pszArg[0] == '0' && (pszArg[1] == 'x' || pszArg[1] == 'X'))
		return (int)strtol(pszArg, NULL, 16);

	return atoi(pszArg);
}

CGVariant& CScript::GetArgVar()
{
	// Return a variant wrapping the current arg
	static CGVariant s_vArg;
	// TODO: proper variant initialization when CGVariant is implemented
	return s_vArg;
}

///////////////////////////////////////////////////////////////
// Context save/restore

CScriptLineContext CScript::GetContext() const
{
	CScriptLineContext ctx;
	ctx.m_iLineNum = m_iLineNum;
	ctx.m_lOffset = const_cast<CScript*>(this)->GetPosition();
	return ctx;
}

void CScript::SeekContext(CScriptLineContext& context)
{
	Seek(context.m_lOffset, SEEK_SET);
	m_iLineNum = context.m_iLineNum;
	m_fSectionHead = false;
}

///////////////////////////////////////////////////////////////
// Writing

void CScript::WriteSection(LPCTSTR pszSection, ...)
{
	va_list vargs;
	va_start(vargs, pszSection);

	Printf("\n[");
	VPrintf(pszSection, vargs);
	Printf("]\n");
	va_end(vargs);
}

void CScript::WriteKey(LPCTSTR pszKey, LPCTSTR lpszVal)
{
	if (!pszKey || !pszKey[0])
		return;

	if (lpszVal && lpszVal[0])
		Printf("%s=%s\n", pszKey, lpszVal);
	else
		Printf("%s\n", pszKey);
}

void CScript::WriteKeyInt(LPCTSTR pszKey, int iValue)
{
	TCHAR szVal[32];
	snprintf(szVal, sizeof(szVal), "%d", iValue);
	WriteKey(pszKey, szVal);
}

void CScript::WriteKeyDWORD(LPCTSTR pszKey, DWORD iValue)
{
	TCHAR szVal[32];
	snprintf(szVal, sizeof(szVal), "0%x", iValue);
	WriteKey(pszKey, szVal);
}

bool CScript::WriteProfileStringSec(LPCTSTR pszSection, LPCTSTR pszKey, LPCTSTR pszVal)
{
	if (!FindSection(pszSection, 0))
		return false;
	WriteKey(pszKey, pszVal);
	return true;
}

bool CScript::WriteProfileStringOffset(long lSectionOffset, LPCTSTR pszKey, LPCTSTR pszVal)
{
	Seek(lSectionOffset, SEEK_SET);
	WriteKey(pszKey, pszVal);
	return true;
}

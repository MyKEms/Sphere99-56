// stubs.cpp - Minimal implementations for remaining undefined symbols
// These provide just enough to link the binary.

#include "stdafx.h"
#include "CMemBlock.h"
#include "CExpression.h"
#include "CValueRange.h"
#include "CSocket.h"
#include "CScriptExecContext.h"

// CMemBlockBase
DWORD CMemBlockBase::sm_dwAllocTotal = 0;

// Parse the scalar and two-endpoint forms used by resource properties:
//   7,12
//   {7 12}
// A scalar is represented as an equal-endpoint range. Keep the current value
// unchanged when the input is malformed; callers cannot report a parse error.
static void SkipRangeWhitespace(LPCTSTR& pszValue)
{
	while ( pszValue && *pszValue && ISWHITESPACE(*pszValue) )
		++pszValue;
}

static bool ParseValueRange(CGVariant& vVal, int& iLo, int& iHi)
{
	LPCTSTR pszValue = vVal.GetPSTR();
	if ( pszValue == NULL )
		return false;

	SkipRangeWhitespace(pszValue);
	const bool fBraced = (*pszValue == '{');
	if ( fBraced )
		++pszValue;
	SkipRangeWhitespace(pszValue);
	if ( !*pszValue || ( fBraced && *pszValue == '}' ))
		return false;

	LPCTSTR pszFirst = pszValue;
	const int iParsedLo = Exp_GetValueRef(pszValue);
	if ( pszValue == pszFirst )
		return false;

	// GetValueRef skips whitespace before stopping at the next token. Remember
	// whether that token was separated by whitespace so `{lo hi}` is distinct
	// from an accidental suffix such as `12junk`.
	const bool fWhitespaceSeparator = pszValue > pszFirst && ISWHITESPACE(pszValue[-1]);
	SkipRangeWhitespace(pszValue);

	int iParsedHi = iParsedLo;
	bool fHasSecondValue = false;
	if ( *pszValue == ',' )
	{
		++pszValue;
		SkipRangeWhitespace(pszValue);
		if ( !*pszValue || *pszValue == '}' || *pszValue == ',' )
			return false;
		const LPCTSTR pszSecond = pszValue;
		iParsedHi = Exp_GetValueRef(pszValue);
		if ( pszValue == pszSecond )
			return false;
		fHasSecondValue = true;
	}
	else if ( fWhitespaceSeparator && *pszValue && *pszValue != '}' && *pszValue != ';' )
	{
		const LPCTSTR pszSecond = pszValue;
		iParsedHi = Exp_GetValueRef(pszValue);
		if ( pszValue == pszSecond )
			return false;
		fHasSecondValue = true;
	}

	SkipRangeWhitespace(pszValue);
	if ( fBraced )
	{
		if ( *pszValue != '}' )
			return false;
		++pszValue;
		SkipRangeWhitespace(pszValue);
	}
	if ( *pszValue && *pszValue != ';' )
		return false;

	iLo = iParsedLo;
	iHi = fHasSecondValue ? iParsedHi : iParsedLo;
	return true;
}

static BYTE ClampRangeByte(int iValue)
{
	if ( iValue < 0 )
		return 0;
	if ( iValue > 255 )
		return 255;
	return static_cast<BYTE>(iValue);
}

// CValueRangeInt
void CValueRangeInt::v_Set(CGVariant& vVal)
{
	int iLo, iHi;
	if ( ParseValueRange(vVal, iLo, iHi) && iLo <= iHi )
		SetRange(iLo, iHi);
}

void CValueRangeInt::v_Get(CGVariant& vVal)
{
	if ( IsInvalid() )
	{
		vVal.SetVoid();
		return;
	}
	if ( m_iLo == m_iHi )
		vVal.SetInt(m_iLo);
	else
		vVal.SetStrFormat("%d,%d", m_iLo, m_iHi);
}

// CValueRangeByte
void CValueRangeByte::v_Set(CGVariant& vVal)
{
	int iLo, iHi;
	if ( ParseValueRange(vVal, iLo, iHi) )
	{
		const BYTE bLo = ClampRangeByte(iLo);
		const BYTE bHi = ClampRangeByte(iHi);
		if ( bLo <= bHi )
			SetRange(bLo, bHi);
	}
}

void CValueRangeByte::v_Get(CGVariant& vVal)
{
	if ( m_iLo == m_iHi )
		vVal.SetInt(m_iLo);
	else
		vVal.SetStrFormat("%u,%u", static_cast<unsigned int>(m_iLo), static_cast<unsigned int>(m_iHi));
}

// CValueCurveDef
int CValueCurveDef::GetLinear(int iSkillPercent) const
{
	int iQty = m_aiValues.GetCount();
	if (iQty <= 0)
		return 0;
	if (iQty == 1)
		return m_aiValues[0];

	// Interpolate
	int iSegSize = 1000 / (iQty - 1);
	if (iSegSize <= 0)
		iSegSize = 1;

	int iSeg = iSkillPercent / iSegSize;
	if (iSeg < 0)
		iSeg = 0;
	if (iSeg >= iQty - 1)
		return m_aiValues[iQty - 1];

	int iRemainder = iSkillPercent - (iSeg * iSegSize);
	int iDiff = m_aiValues[iSeg + 1] - m_aiValues[iSeg];
	return m_aiValues[iSeg] + IMULDIV(iDiff, iRemainder, iSegSize);
}

int CValueCurveDef::GetChancePercent(int iSkillPercent) const
{
	return GetLinear(iSkillPercent);
}

// CVarDef
int CVarDef::GetInt()
{
	return GetValNum();
}

DWORD CVarDef::GetDWORD()
{
	return (DWORD)GetValNum();
}

// CStringSortArray
void CStringSortArray::AddSortString(LPCTSTR pszStr)
{
	if (!pszStr)
		return;
	Add(CGString(pszStr));
}

// CSocketAddressIP
bool CSocketAddressIP::IsSameIP(const CSocketAddressIP& ip) const
{
	return (s_addr == ip.s_addr);
}

// CScriptExecContext
CScriptPropArray CScriptExecContext::sm_FunctionsAll;

// g_pLog is defined in SphereSvr/spheresvr.cpp

// CVarDefArray::s_WriteTags - must be in .cpp because CScript is incomplete in CExpression.h
#include "CScript.h"
void CVarDefArray::s_WriteTags(CScript& script, LPCTSTR pszName)
{
	// Write all the tags to the script file.
	// pszName = format string for the key name (e.g. "TAG.%s" or "%s" or NULL for "Tag.%s")
	for (int i = 0; i < (int)this->GetSize(); i++)
	{
		CVarDef* pVar = this->GetAt(i);
		if ( pVar == NULL )
			continue;
		LPCTSTR pszKey = pVar->GetKey();
		LPCTSTR pszVal = pVar->GetValStr();
		if ( pszKey == NULL || pszKey[0] == '\0' )
			continue;
		TCHAR szKeyFull[EXPRESSION_MAX_KEY_LEN];
		if ( pszName )
		{
			snprintf(szKeyFull, sizeof(szKeyFull), pszName, pszKey);
		}
		else
		{
			snprintf(szKeyFull, sizeof(szKeyFull), "Tag.%s", pszKey);
		}
		// Write key=value pair.
		if ( pszVal && pszVal[0] )
		{
			script.WriteKey(szKeyFull, pszVal);
		}
	}
}

// CResourceObj::s_LoadProps - read key=value pairs from script section
// NOTE: Due to multiple inheritance (CResourceObj + CResourceDef both define
// virtual s_LoadProps), this version is called via CResourceObjPtr.
// For CObjBase-derived objects, delegate to CResourceDef::s_LoadProps which
// properly dispatches through the class hierarchy (CChar/CItem overrides).
bool CResourceObj::s_LoadProps(CScript& s)
{
	// Read key=value pairs and dispatch via s_PropSet.
	while (s.ReadKeyParse())
	{
		CGVariant vArg;
		vArg = s.GetArgRaw();
		s_PropSet(s.GetKey(), vArg);
	}
	return true;
}

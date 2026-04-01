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

// CValueRangeInt
void CValueRangeInt::v_Set(CGVariant& vVal)
{
	// STUB
}

void CValueRangeInt::v_Get(CGVariant& vVal)
{
	// STUB
}

// CValueRangeByte
void CValueRangeByte::v_Set(CGVariant& vVal)
{
	// STUB
}

void CValueRangeByte::v_Get(CGVariant& vVal)
{
	// STUB
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
	// NOTE: Due to multiple inheritance, CChar/CItem override s_PropSet
	// on the CResourceDef chain, not the CResourceObj chain.
	// We must avoid calling this->s_PropSet() which goes through
	// the CResourceObj vtable. Instead, use s_LoadProps_Default.
	while (s.ReadKeyParse())
	{
		CGVariant vArg;
		vArg = s.GetArgRaw();
		s_PropSet(s.GetKey(), vArg);
	}
	return true;
}

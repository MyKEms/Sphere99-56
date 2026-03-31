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

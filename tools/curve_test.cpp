// Offline regression test for the stock per-mille skill curves.

#include "SphereSvr/stdafx.h"

#include <cstdio>

struct CurveRow
{
	int m_iDiff;
	int m_iVariance;
	int m_iBell;
	int m_iS;
};

static bool Expect(bool fCondition, const char* pszDescription)
{
	if ( fCondition )
		return true;
	std::fprintf(stderr, "FAIL: %s\n", pszDescription);
	return false;
}

static bool TestReferenceTable()
{
	// The values are the stock 0.99z8 table for variance 100. Keep both
	// positive and negative offsets so a percentage/per-mille regression or
	// an accidental left-to-right tail cannot pass by symmetry alone.
	static const CurveRow sm_Rows[] =
	{
		{ 0,   100, 500, 500 },
		{ 100, 100, 250, 750 },
		{-100, 100, 250, 250 },
		{ 200, 100, 125, 875 },
		{-200, 100, 125, 125 },
		{ 300, 100,  63, 937 },
		{-300, 100,  63,  63 },
	};
	for ( const CurveRow& row : sm_Rows )
	{
		if ( !Expect(Calc_GetBellCurve(row.m_iDiff, row.m_iVariance) == row.m_iBell,
			"bell curve reference point") ||
			!Expect(Calc_GetSCurve(row.m_iDiff, row.m_iVariance) == row.m_iS,
			"S-curve reference point") )
			return false;
	}
	return true;
}

static bool TestDegenerateVariance()
{
	return Expect(Calc_GetBellCurve(0, 0) == 500, "zero variance center") &&
		Expect(Calc_GetBellCurve(1, 0) == 0, "zero variance positive bell tail") &&
		Expect(Calc_GetSCurve(1, 0) == 1000, "zero variance positive S tail") &&
		Expect(Calc_GetSCurve(-1, 0) == 0, "zero variance negative S tail");
}

int main()
{
	if ( !TestReferenceTable() || !TestDegenerateVariance() )
		return 1;
	std::puts("skill curves: stock per-mille reference table passed");
	return 0;
}

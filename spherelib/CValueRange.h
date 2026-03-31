#ifndef _INC_CVALUERANGE_H
#define _INC_CVALUERANGE_H
struct CValueRangeInt
{
	// Simple linearity
public:
	int m_iLo;
	int m_iHi;
public:
	CValueRangeInt(int iLo, int iHi)
	{
		SetRange(iLo, iHi);
	}
	void Init()
	{
		m_iLo = INT_MIN;
		m_iHi = INT_MAX;
	}
	int GetRange() const
	{
		return( static_cast<int>(m_iHi - m_iLo) );
	}
	void SetRange(int iLo, int iHi)
	{
		m_iLo = iLo;
		m_iHi = iHi;
	}
	int GetLinear( int iPercent ) const
	{	
		// ARGS: iPercent = 0-1000
		return( static_cast<int>(m_iLo) + IMULDIV( GetRange(), iPercent, 1000 ));
	}
	int GetRandom() const
	{	
		return( static_cast<int>(m_iLo) + Calc_GetRandVal( GetRange()));
	}
	int GetMin() const { return m_iLo; }
	int GetMax() const { return m_iHi; }
	int GetRandomLinear( int iPercent ) const;
	void v_Set(CGVariant& vVal);
	void v_Get(CGVariant& vVal);
	bool IsInvalid() const { throw "not implemented"; }

public:
	CValueRangeInt()
	{
		Init();
	}
};

struct CValueRangeByte
{
	// Simple linearity
public:
	BYTE m_iLo;
	BYTE m_iHi;
public:
	void Init()
	{
		m_iLo = 0;
		m_iHi = 255;
	}
	BYTE GetRange() const
	{
		return(static_cast<BYTE>(m_iHi - m_iLo));
	}
	void SetRange(BYTE iLo, BYTE iHi)
	{
		m_iLo = iLo;
		m_iHi = iHi;
	}
	int GetLinear(int iPercent) const
	{
		// ARGS: iPercent = 0-1000
		return(static_cast<BYTE>(m_iLo) + IMULDIV(GetRange(), iPercent, 1000));
	}
	BYTE GetRandom() const
	{
		return(static_cast<BYTE>(m_iLo) + Calc_GetRandVal(GetRange()));
	}
	BYTE GetAvg() const
	{
		return (m_iHi - m_iLo) / 2;
	}
	BYTE GetMin() const { return m_iLo; }
	BYTE GetMax() const { return m_iHi; }
	void SetMax(BYTE iHi) { m_iHi = iHi; }
	int GetRandomLinear(int iPercent) const;
	void v_Set(CGVariant& vVal);
	void v_Get(CGVariant& vVal);

public:
	CValueRangeByte()
	{
		Init();
	}
};

struct CValueCurveDef
{
	// Describe an arbitrary curve.
	// for a range from 0.0 to 100.0 (1000)
	// May be a list of probabilties from 0 skill to 100.0% skill.
public:
	CGTypedArray<int, int> m_aiValues;		// 0 to 100.0 skill levels
public:
	void Init()
	{
		m_aiValues.Empty();
	}
	int GetLinear(int iSkillPercent) const;
	int GetChancePercent(int iSkillPercent) const;
	int GetRandom() const;
	int GetRandomLinear(int iPercent) const;

	void v_Get(CGVariant& val) { throw "not implemented"; }
	void v_Set(CGVariant& val) { throw "not implemented"; }
};

#endif // _INC_CVALUERANGE_H

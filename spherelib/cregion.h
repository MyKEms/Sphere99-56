#ifndef _INC_CREGION_H
#define _INC_CREGION_H

struct CGRect			// Basic rectangle. (May not be on the map)
{
public:
	int m_left;		// West	 x=0
	int m_top;		// North y=0
	int m_right;	// East	( NON INCLUSIVE !)
	int m_bottom;	// South ( NON INCLUSIVE !)
	int m_map;

public:
	int Width() const { return(m_right - m_left); }
	int Height() const { return(m_bottom - m_top); }
	bool IsRectEmpty() const { throw "not implemented"; }

	operator RECT& ()
	{
		return(*((RECT*)(&m_left)));
	}
	operator RECT* ()
	{
		return((::RECT*)(&m_left));
	}
	operator const RECT& () const
	{
		return(*((RECT*)(&m_left)));
	}
	operator const RECT* () const
	{
		return((::RECT*)(&m_left));
	}
	CGRect& operator = (RECT rect)
	{
		m_left = rect.left;
		m_top = rect.top;
		m_right = rect.right;
		m_bottom = rect.bottom;
		return(*this);
	}

	void SetRectEmpty()
	{
		m_left = m_top = 0;	// 0x7ffe
		m_right = m_bottom = 0;
		m_map = 0;
	}
	
	virtual void NormalizeRect()
	{
		if (m_bottom < m_top)
		{
			int wtmp = m_bottom;
			m_bottom = m_top;
			m_top = wtmp;
		}
		if (m_right < m_left)
		{
			int wtmp = m_right;
			m_right = m_left;
			m_left = wtmp;
		}
		if ((m_map < 0) || (m_map >= 256)) m_map = 0;
		if (!g_MapList.m_maps[m_map]) m_map = 0;
	}

	void NormalizeRectMax(int cx, int cy)
	{
		if (m_left < 0)
			m_left = 0;
		if (m_top < 0)
			m_top = 0;
		if (m_right > cx)
			m_right = cx;
		if (m_bottom > cy)
			m_bottom = cy;
	}

	void OffsetRect(int x, int y)
	{
		m_left += x;
		m_top += y;
		m_right += x;
		m_bottom += y;
	}

	void UnionPoint(int x, int y)
	{
		// Inflate this rect to include this point.
		// NON inclusive rect! 
		if (x < m_left) m_left = x;
		if (y < m_top) m_top = y;
		if (x >= m_right) m_right = x + 1;
		if (y >= m_bottom) m_bottom = y + 1;
	}

	void SetRect(int left, int top, int right, int bottom)
	{
		m_left = left;
		m_top = top;
		m_right = right;
		m_bottom = bottom;
		NormalizeRect();
	}

	bool PtInRect(CGPointBase point) { throw "not implemented"; }
	LPCTSTR WriteRectStr() { throw "not implemented"; }

	static DIR_TYPE GetDirStr(LPCTSTR pszDir) { throw "not implemented"; }
};

class CGRegion
{
public:
	CGRect m_rectUnion;	// The union rectangle.
	CGTypedArray<CGRect, const CGRect&> m_Rects;

	void EmptyRegion()
	{
		m_rectUnion.SetRectEmpty();
		m_Rects.Empty();
	}

	bool IsRegionEmpty() const { throw "not implemented"; }
	int GetRegionRectCount() const { throw "not implemented"; }
	CGRect& GetRegionRect(int i) { throw "not implemented"; }
	virtual bool AddRegionRect(const CGRect& rect) { throw "not implemented"; }
	virtual bool IsOverlapped(const CGRect& rect) const { throw "not implemented"; }
	virtual bool IsOverlapped(const CGRect* rect) const { throw "not implemented"; }
	virtual bool IsOverlapped(const CGRegion* pRect) const { throw "not implemented"; }
	bool IsEqualRegion(const CGRegion* pRegionTest) const { throw "not implemented"; }
	bool IsInside(const CGRegion* pRect) const { throw "not implemented"; }

	CGPointBase GetRegionCorner(DIR_TYPE dir) const { throw "not implemented"; }
	bool PtInRegion(const CGPointBase& pt) const { throw "not implemented"; }
};

inline DIR_TYPE GetDirTurn(DIR_TYPE dir, int offset)
{
	// Turn in a direction.
	// +1 = to the right.
	// -1 = to the left.
	offset += DIR_QTY + dir;
	offset %= DIR_QTY;
	return((DIR_TYPE)(offset));
}

#endif // _INC_CREGION_H

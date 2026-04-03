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
	bool IsRectEmpty() const { return( m_left >= m_right || m_top >= m_bottom ); }

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

	bool PtInRect(CGPointBase point)
	{
		return( point.m_x >= m_left && point.m_x < m_right &&
				point.m_y >= m_top && point.m_y < m_bottom );
	}
	LPCTSTR WriteRectStr()
	{
		TCHAR* pszTemp = Str_GetTemp();
		sprintf( pszTemp, "%d,%d,%d,%d", m_left, m_top, m_right, m_bottom );
		return pszTemp;
	}

	static DIR_TYPE GetDirStr(LPCTSTR pszDir)
	{
		for ( int i = 0; i < DIR_QTY; i++ )
		{
			if ( ! _stricmp( pszDir, CGPointBase::sm_szDirs[i] ))
				return (DIR_TYPE)i;
		}
		return DIR_QTY;
	}
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

	bool IsRegionEmpty() const
	{
		return m_rectUnion.IsRectEmpty();
	}
	int GetRegionRectCount() const
	{
		int iQty = m_Rects.GetSize();
		if ( iQty <= 0 )
		{
			if ( ! m_rectUnion.IsRectEmpty())
				return 1;
		}
		return iQty;
	}
	CGRect& GetRegionRect(int i)
	{
		if ( m_Rects.GetSize() <= 0 )
			return m_rectUnion;
		return m_Rects[i];
	}
	virtual bool AddRegionRect(const CGRect& rect)
	{
		if ( m_Rects.GetSize() <= 0 && m_rectUnion.IsRectEmpty())
		{
			m_rectUnion = rect;
		}
		else
		{
			m_Rects.Add( rect );
			m_rectUnion.UnionPoint( rect.m_left, rect.m_top );
			m_rectUnion.UnionPoint( rect.m_right - 1, rect.m_bottom - 1 );
		}
		return true;
	}
	virtual bool IsOverlapped(const CGRect& rect) const
	{
		if ( m_rectUnion.m_left >= rect.m_right || m_rectUnion.m_right <= rect.m_left )
			return false;
		if ( m_rectUnion.m_top >= rect.m_bottom || m_rectUnion.m_bottom <= rect.m_top )
			return false;
		return true;
	}
	virtual bool IsOverlapped(const CGRect* rect) const
	{
		if ( rect == NULL ) return false;
		return IsOverlapped(*rect);
	}
	virtual bool IsOverlapped(const CGRegion* pRect) const
	{
		if ( pRect == NULL ) return false;
		return IsOverlapped(pRect->m_rectUnion);
	}
	bool IsEqualRegion(const CGRegion* pRegionTest) const
	{
		if ( pRegionTest == NULL ) return false;
		if ( GetRegionRectCount() != pRegionTest->GetRegionRectCount()) return false;
		// Just compare unions for simplicity.
		return( m_rectUnion.m_left == pRegionTest->m_rectUnion.m_left &&
				m_rectUnion.m_top == pRegionTest->m_rectUnion.m_top &&
				m_rectUnion.m_right == pRegionTest->m_rectUnion.m_right &&
				m_rectUnion.m_bottom == pRegionTest->m_rectUnion.m_bottom );
	}
	bool IsInside(const CGRegion* pRect) const
	{
		if ( pRect == NULL ) return false;
		return( m_rectUnion.m_left >= pRect->m_rectUnion.m_left &&
				m_rectUnion.m_top >= pRect->m_rectUnion.m_top &&
				m_rectUnion.m_right <= pRect->m_rectUnion.m_right &&
				m_rectUnion.m_bottom <= pRect->m_rectUnion.m_bottom );
	}

	CGPointBase GetRegionCorner(DIR_TYPE dir) const
	{
		CGPointBase pt;
		pt.m_z = 0;
		pt.m_mapplane = 0;
		switch ( dir )
		{
		case DIR_N:  pt.m_x = (m_rectUnion.m_left + m_rectUnion.m_right) / 2; pt.m_y = m_rectUnion.m_top; break;
		case DIR_NE: pt.m_x = m_rectUnion.m_right - 1; pt.m_y = m_rectUnion.m_top; break;
		case DIR_E:  pt.m_x = m_rectUnion.m_right - 1; pt.m_y = (m_rectUnion.m_top + m_rectUnion.m_bottom) / 2; break;
		case DIR_SE: pt.m_x = m_rectUnion.m_right - 1; pt.m_y = m_rectUnion.m_bottom - 1; break;
		case DIR_S:  pt.m_x = (m_rectUnion.m_left + m_rectUnion.m_right) / 2; pt.m_y = m_rectUnion.m_bottom - 1; break;
		case DIR_SW: pt.m_x = m_rectUnion.m_left; pt.m_y = m_rectUnion.m_bottom - 1; break;
		case DIR_W:  pt.m_x = m_rectUnion.m_left; pt.m_y = (m_rectUnion.m_top + m_rectUnion.m_bottom) / 2; break;
		case DIR_NW: pt.m_x = m_rectUnion.m_left; pt.m_y = m_rectUnion.m_top; break;
		default:
			pt.m_x = (m_rectUnion.m_left + m_rectUnion.m_right) / 2;
			pt.m_y = (m_rectUnion.m_top + m_rectUnion.m_bottom) / 2;
			break;
		}
		return pt;
	}
	bool PtInRegion(const CGPointBase& pt) const
	{
		if ( ! ( pt.m_x >= m_rectUnion.m_left && pt.m_x < m_rectUnion.m_right &&
				 pt.m_y >= m_rectUnion.m_top && pt.m_y < m_rectUnion.m_bottom ))
			return false;
		int iQty = m_Rects.GetSize();
		if ( iQty <= 0 )
			return true;	// single rect already checked.
		for ( int i = 0; i < iQty; i++ )
		{
			if ( pt.m_x >= m_Rects[i].m_left && pt.m_x < m_Rects[i].m_right &&
				 pt.m_y >= m_Rects[i].m_top && pt.m_y < m_Rects[i].m_bottom )
				return true;
		}
		return false;
	}
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

#include "stdafx.h"
#include "cpointbase.h"
#include <cstdlib>
#include <cmath>
#include <climits>

LPCTSTR const CGPointBase::sm_szDirs[DIR_QTY + 1] =
{
	"N",
	"NE",
	"E",
	"SE",
	"S",
	"SW",
	"W",
	"NW",
	"Here"
};

void CGPointBase::InitPoint()
{
	m_x = -1;	// invalid location.
	m_y = -1;
	m_z = 0;
	m_mapplane = 0;
}

int CGPointBase::GetDistZ(const CGPointBase& pt) const
{
	return(abs(m_z - pt.m_z));
}

int CGPointBase::GetDist(const CGPointBase& pt) const
{
	// Get the basic 2d distance.
	int dx = abs(m_x - pt.m_x);
	int dy = abs(m_y - pt.m_y);
	return max(dx, dy);
}

int CGPointBase::GetDist3D(const CGPointBase& pt) const
{
	int dist = GetDist(pt);
	int dz = GetDistZ(pt) / 8; // approximate Z adjustment
	return max(dz, dist);
}

DIR_TYPE CGPointBase::GetDir(const CGPointBase& pt, DIR_TYPE DirDefault) const
{
	int dx = (m_x - pt.m_x);
	int dy = (m_y - pt.m_y);

	int ax = abs(dx);
	int ay = abs(dy);

	if (ay > ax)
	{
		if (!ax)
		{
			return (dy > 0) ? DIR_N : DIR_S;
		}
		int slope = ay / ax;
		if (slope > 2)
			return (dy > 0) ? DIR_N : DIR_S;
		if (dx > 0)
		{
			return (dy > 0) ? DIR_NW : DIR_SW;
		}
		return (dy > 0) ? DIR_NE : DIR_SE;
	}
	else
	{
		if (!ay)
		{
			if (!dx)
				return DirDefault;
			return (dx > 0) ? DIR_W : DIR_E;
		}
		int slope = ax / ay;
		if (slope > 2)
			return (dx > 0) ? DIR_W : DIR_E;
		if (dy > 0)
		{
			return (dx > 0) ? DIR_NW : DIR_NE;
		}
		return (dx > 0) ? DIR_SW : DIR_SE;
	}
}

int CGPointBase::StepLinePath(const CGPointBase& ptSrc, int iSteps)
{
	int dx = m_x - ptSrc.m_x;
	int dy = m_y - ptSrc.m_y;
	int iDist2D = GetDist(ptSrc);
	if (!iDist2D)
		return 0;

	m_x = ptSrc.m_x + (signed short)(IMULDIV(iSteps, dx, iDist2D));
	m_y = ptSrc.m_y + (signed short)(IMULDIV(iSteps, dy, iDist2D));
	return iDist2D;
}

void CGPointBase::Set(const CGPointBase& pt)
{
	m_x = pt.m_x;
	m_y = pt.m_y;
	m_z = pt.m_z;
	m_mapplane = pt.m_mapplane;
}

void CGPointBase::Set(const POINT pt)
{
	m_x = pt.x;
	m_y = pt.y;
	m_z = 0;
	m_mapplane = 0;
}

void CGPointBase::Set(const POINTS pt)
{
	m_x = pt.x;
	m_y = pt.y;
	m_z = 0;
	m_mapplane = 0;
}

void CGPointBase::v_Set(CGVariant& vVal)
{
	// STUB - parse "x,y,z,map" from variant
}

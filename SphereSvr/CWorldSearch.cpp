//
// CWorldSearch.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.

//////////////////////////////////////////////////////////////////
// -CWorldSearch

CWorldSearch::CWorldSearch( const CPointMap& pt, int iDist ) :
	m_pt( pt ),
	m_iDist( iDist )
{
	// define a search of the world.
	ASSERT(pt.IsValidXY());
	ReInit();
}

void CWorldSearch::ReInit()
{
	m_fAllShow = false;
	m_pObj = m_pObjNext = NULL;
	m_fInertToggle = false;

	m_pSectorBase = m_pSector = m_pt.GetSector();

	m_rectSector.SetRect(
		m_pt.m_x - m_iDist,
		m_pt.m_y - m_iDist,
		m_pt.m_x + m_iDist + 1,
		m_pt.m_y + m_iDist + 1 );

	// Get upper left of search rect.
	m_iSectorCur = 0;
}

bool CWorldSearch::GetNextSector()
{
	// Move search into nearby CSector(s) if necessary

	if ( ! m_iDist )
		return( false );

	for(;;)
	{
		m_pSector = m_rectSector.GetSector(m_iSectorCur++);
		if ( m_pSector == NULL )
			return( false );	// done searching.
		if ( m_pSectorBase == m_pSector )
			continue;	// same as base.
		m_pObj = NULL;	// start at head of next Sector.
		return( true );
	}

	return( false );	// done searching.
}

CItemPtr CWorldSearch::GetNextItem()
{
	for(;;)
	{
		if ( m_pObj == NULL )
		{
			m_fInertToggle = false;
			m_pObj = STATIC_CAST(CObjBase,m_pSector->m_Items_Inert.GetHead());
		}
		else
		{
			m_pObj = m_pObjNext;
		}
		if ( m_pObj == NULL )
		{
			if ( ! m_fInertToggle )
			{
				m_fInertToggle = true;
				m_pObj = STATIC_CAST(CObjBase,m_pSector->m_Items_Timer.GetHead());
				if ( m_pObj != NULL )
					goto jumpover;
			}
			if ( GetNextSector())
				continue;
			return( NULL );
		}

	jumpover:
		m_pObjNext = m_pObj->GetNext();
		if ( m_fAllShow )
		{
			if ( m_pt.GetDistBase( m_pObj->GetTopPoint()) <= m_iDist )
				return( REF_CAST(CItem,m_pObj));
		}
		else
		{
			if ( m_pt.GetDist( m_pObj->GetTopPoint()) <= m_iDist )
				return( REF_CAST(CItem,m_pObj));
		}
	}
}

CCharPtr CWorldSearch::GetNextChar()
{
	for(;;)
	{
		if ( m_pObj == NULL )
		{
			m_fInertToggle = false;
			m_pObj = STATIC_CAST(CObjBase,m_pSector->m_Chars_Active.GetHead());
		}
		else
		{
			m_pObj = m_pObjNext;
		}
		if ( m_pObj == NULL )
		{
			if ( ! m_fInertToggle && m_fAllShow )
			{
				m_fInertToggle = true;
				m_pObj = STATIC_CAST(CObjBase,m_pSector->m_Chars_Disconnect.GetHead());
				if ( m_pObj != NULL )
					goto jumpover;
			}
			if ( GetNextSector())
				continue;
			return( NULL );
		}

	jumpover:
		m_pObjNext = m_pObj->GetNext();
		if ( m_fAllShow )
		{
			if ( m_pt.GetDistBase( m_pObj->GetTopPoint()) <= m_iDist )
				return( REF_CAST(CChar,m_pObj));
		}
		else
		{
			if ( m_pt.GetDist( m_pObj->GetTopPoint()) <= m_iDist )
				return( REF_CAST(CChar,m_pObj));
		}
	}
}


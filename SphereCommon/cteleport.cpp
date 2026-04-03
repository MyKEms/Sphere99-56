//
//
// CTeleport.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"
#include "spherecommon.h"
#include "spheremul.h"
#include "cregionmap.h"
#include "cpointmap.h"

//************************************************************************
// -CTeleport

CSCRIPT_CLASS_IMP1(Teleport,NULL,NULL,NULL,ResourceDef);

CTeleport::CTeleport( const CPointMap& ptSrc ) :
	CResourceDef( RID_F_MAP | ptSrc.GetHashCode()),
	m_ptSrc(ptSrc)
{
	ASSERT( ptSrc.IsValidPoint());
	m_ptDst = ptSrc;
}

CTeleport::CTeleport( const TCHAR* pszArgs ) :
	CResourceDef( RID_F_MAP )
{
	// RES_TELEPORT
	// Assume valid iArgs >= 5

	m_DirType = TELDIR_1;

	CString sTmp = pszArgs;
	TCHAR* ppCmds[4];
	int iArgs = Str_ParseCmdsStr( sTmp, ppCmds, COUNTOF( ppCmds ), "=" );
	if ( iArgs < 2 )
	{
		DEBUG_ERR(( "Bad CTeleport Def\n" ));
		return;
	}

	CGVariant vArgsSrc(VARTYPE_LPSTR,ppCmds[0]);
	m_ptSrc.v_Set(vArgsSrc);
	CGVariant vArgsDst(VARTYPE_LPSTR,ppCmds[1]);
	m_ptDst.v_Set( vArgsDst);

	if ( iArgs >= 3 )
	{
		SetResourceName( ppCmds[2] );
	}
	if ( iArgs >= 4 )
	{
		m_DirType = (TELDIR_TYPE) atoi(ppCmds[3]);
	}
}

CTeleport::~CTeleport()
{
	UnRealizeTeleport();
	if ( m_DirType != TELDIR_1 )
	{
		CTeleportPtr pTeleport = Get2Dir();
		if ( pTeleport )
		{
			pTeleport->UnRealizeTeleport();
		}
	}
}

void CTeleport::s_WriteProps( CScript& s )
{
	if ( m_DirType == TELDIR_2_SECONDARY )
		return;
	CGString sText;
	sText.Format( _TEXT("%s=%s=%d"), 
		(LPCTSTR) m_ptDst.v_Get(),
		(LPCTSTR) GetName(),
		m_DirType );
	s.WriteKey( m_ptSrc.v_Get(), sText );
}

CTeleportPtr CTeleport::Get2Dir() const
{
	// Get the teleport going the other way.
	// If the teleport is marked as 2 Dir.

	if ( m_DirType == TELDIR_1 )	// just 1 way.
		return NULL;
	CSectorPtr pSector = m_ptDst.GetSector();
	if ( pSector == NULL )
		return NULL;
	return pSector->GetTeleport2d(m_ptDst);
}

void CTeleport::SetDst( const CPointMap& pt )
{
	// Move an existing teleport.
	if ( m_ptDst == pt )
		return;
	m_ptDst = pt;
	if ( m_DirType != TELDIR_1 )
	{
		// Move the other side.
		CTeleportPtr pTeleport = Get2Dir();
		if ( pTeleport )
		{
			pTeleport->SetSrc(pt);
		}
	}
}

void CTeleport::UnRealizeTeleport()
{
	CSectorPtr pSector = m_ptSrc.GetSector();
	if ( pSector )
	{
		pSector->m_Teleports.UnLinkArg(this);
	}
}

void CTeleport::SetSrc( const CPointMap& pt )
{
	// Move an existing teleport.
	UnRealizeTeleport();

	if ( m_ptSrc != pt )
	{
		m_ptSrc = pt;
		if ( m_DirType != TELDIR_1 )
		{
			// Move the other side.
			CTeleportPtr pTeleport = Get2Dir();
			if ( pTeleport )
			{
				pTeleport->SetDst(pt);
			}
		}
	}

	RealizeTeleport();
}

bool CTeleport::RealizeTeleport()
{
	if ( ! m_ptSrc.IsCharValid() || ! m_ptDst.IsCharValid())
	{
		DEBUG_ERR(( "CTeleport bad coords %s\n", (LPCTSTR) m_ptSrc.v_Get() ));
		return false;
	}

	if ( m_DirType == TELDIR_2_PRIMARY )
	{
		// generate the secondary if it does not already exist.
		CTeleportPtr pTeleport = Get2Dir();
		if ( pTeleport == NULL )
		{
			pTeleport = new CTeleport(m_ptDst);
			ASSERT(pTeleport);
			pTeleport->RealizeTeleport();
		}
		else if ( pTeleport->m_ptDst != m_ptSrc )
		{
			DEBUG_ERR(( "CTeleport non matching coords %s\n", (LPCTSTR) ( pTeleport->m_ptDst.v_Get()) ));
		}
		pTeleport->m_DirType = TELDIR_2_SECONDARY;
		pTeleport->m_ptDst = m_ptSrc;
	}

	CSectorPtr pSector = m_ptSrc.GetSector();
	ASSERT(pSector);
	return pSector->AddTeleport( this );
}

//************************************************************************
// -CStartLoc

CSCRIPT_CLASS_IMP1(StartLoc,NULL,NULL,NULL,ResourceDef);

CStartLoc::CStartLoc( LPCTSTR pszArea ) : 
	CResourceDef( CSphereUID(RES_Starts))
{
	m_sArea = pszArea;
}


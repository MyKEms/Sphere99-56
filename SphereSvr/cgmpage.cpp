//
// CGMPage.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.

//////////////////////////////////////////////////////////////////
// -CGMPage

const CScriptProp CGMPage::sm_Props[CGMPage::P_QTY+1] =
{
#define CGMPAGEPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "cgmpageprops.tbl"
#undef CGMPAGEPROP
	NULL,
};

CSCRIPT_CLASS_IMP1(GMPage,CGMPage::sm_Props,NULL,NULL,ResourceObj);

CGMPage::CGMPage( CAccount* pAccount ) :
	CResourceObj( CSphereUID( RES_GMPage, pAccount->GetUIDIndex() & RID_INDEX_MASK )),
	m_pAccount( pAccount )
{
	m_pGMClient = NULL;
	m_timePage.InitTimeCurrent();
	// Put at the end of the list.
	g_World.m_GMPages.InsertTail( this );
}

CGMPage::~CGMPage()
{
	if ( m_pGMClient )	// break the link to the client.
	{
		ASSERT( m_pGMClient->m_pGMPage == this );
		m_pGMClient->m_pGMPage = NULL;
		ClearGMHandler();
	}
}

int CGMPage::GetAge() const
{
	// How old in seconds.
	return( m_timePage.GetCacheAge() / TICKS_PER_SEC );
}

CGString CGMPage::GetName() const
{
	return( m_pAccount->GetName());
}

CAccountPtr CGMPage::GetAccount() const
{
	return( m_pAccount );
}

CGString CGMPage::GetAccountStatus() const
{
	CClientPtr pClient = g_Serv.FindClientAccount( m_pAccount);
	if ( pClient==NULL )
		return "OFFLINE";
	else if ( pClient->GetChar() == NULL )
		return "LOGIN";
	else
		return pClient->GetChar()->GetName();
}

void CGMPage::s_WriteProps( CScript& s ) const
{
	s.WriteSection( "GMPAGE %s", (LPCTSTR) GetName());
	s.WriteKey( "REASON", GetReason());
	s.WriteKeyDWORD( "TIME", GetAge());
	s.WriteKey( "P", m_ptOrigin.v_Get());
}

HRESULT CGMPage::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp<0)
	{
		return( CResourceObj::s_PropGet( pszKey, vValRet, pSrc ));
	}
	switch (iProp)
	{
	case P_Account:
	case P_AccountName:
		vValRet = GetName();
		break;
	case P_P:	
		vValRet = m_ptOrigin.v_Get();
		break;
	case P_Reason:
		vValRet = GetReason();
		break;
	case P_Status:
		vValRet = GetAccountStatus();
		break;
	case P_Time:
		vValRet.SetDWORD( GetAge() );
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CGMPage::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp<0)
	{
		return( CResourceObj::s_PropSet( pszKey, vVal ));
	}
	switch (iProp)
	{
	case P_P:	// "P"
		m_ptOrigin.v_Set(vVal);
		break;
	case P_Reason:
		SetReason( vVal.GetPSTR());
		break;
	case P_Time:
		m_timePage.InitTimeCurrent( - ( vVal.GetInt()* TICKS_PER_SEC ));
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}


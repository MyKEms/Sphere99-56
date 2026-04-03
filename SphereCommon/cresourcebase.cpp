//
// CResourceDef.h
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"
#include "spherecommon.h"

// Stub for missing function
static int FindArg(void* p) { return -1; }

/////////////////////////////////////////////////
// -CResourceRefArray

#ifdef USE_JSCRIPT
#define CUIDREFARRAYMETHOD(a,b,c,d) JSCRIPT_METHOD_IMP(CUIDRefArray,a)
#include "cuidrefarraymethods.tbl"
#undef CUIDREFARRAYMETHOD
#endif

const CScriptMethod CResourceRefArray::sm_Methods[CResourceRefArray::M_QTY+1] =
{
#define CUIDREFARRAYMETHOD(a,b,c,d) CSCRIPT_METHOD_IMP(a,b,c d)
#include "cuidrefarraymethods.tbl"
#undef CUIDREFARRAYMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP0(ResourceRefArray,NULL,CResourceRefArray::sm_Methods);

bool CResourceRefArray::v_Set( CGVariant& vVal, RES_TYPE restype )
{
	// A bunch of CResourceLink (CResourceDef) pointers.
	// Add or remove from the list.
	// RETURN: false = it failed.

	// ? "TOWN" and "REGION" are special ?!?

	int iArgCount = vVal.MakeArraySize();

	bool fRet = false;
	for ( int i=0; i<iArgCount; i++ )
	{
		const char*pszCmd = vVal.GetArrayStr(i);

		bool fRemove = ( pszCmd[0] == '-' );
		if ( fRemove )
		{
			// remove a ref or all refs.
			pszCmd ++;
			if ( pszCmd[0] == '0' || pszCmd[0] == '*' )
			{
				RemoveAll();
				fRet = true;
				continue;
			}
		}
		else
		{
			// Add a single knowledge fragment or appropriate group item.
			if ( pszCmd[0] == '+' ) pszCmd ++;
		}

		CResourceDefPtr pResDef = g_Cfg.ResourceGetDefByName( restype, pszCmd );
		CResourceLinkPtr pResLink = REF_CAST(CResourceLink,pResDef);
		if ( pResLink == NULL )
		{
			fRet = false;
			DEBUG_ERR(( "Unknown '%s' Resource '%s'" LOG_CR, CSphereResourceMgr::GetResourceBlockName(restype), pszCmd ));
			continue;
		}

#ifdef _DEBUG
		CString sName = pResLink->GetResourceName();
		ASSERT( sName[0] );
		ASSERT( sName[0] != '0' || sName[1] != '0' );
#endif

		// Already here ?
		int iIndex = FindArg(pResLink);

		if ( fRemove )
		{
			fRet = ( iIndex >= 0 );
			if ( ! fRet )
				continue;
			RemoveAt(iIndex);
		}
		else
		{
			// Is it already in the list ?
			fRet = true;
			if ( iIndex >= 0 )
				continue;
			Add( pResLink );
		}
	}
	return( fRet );
}

void CResourceRefArray::v_Get( CGVariant& vVal ) const
{
	// Get the whole list.
	for ( int j=0;j<GetSize(); j++ )
	{
		vVal.SetArrayElement(j, GetResourceName( j ));
	}
}

int CResourceRefArray::FindResourceType( RES_TYPE restype ) const
{
	// Is this resource type already in the list ?
	int iQty = GetSize();
	for ( int i=0; i<iQty; i++ )
	{
		CSphereUID ridtest = ConstElementAt(i)->GetUIDIndex();
		if ( ridtest.GetResType() == restype )
			return( i );
	}
	return( -1 );
}

int CResourceRefArray::FindResourceID( CSphereUID rid ) const
{
	// Is this resource already in the list ?
	int iQty = GetSize();
	for ( int i=0; i<iQty; i++ )
	{
		CSphereUID ridtest = ConstElementAt(i)->GetUIDIndex();
		if ( ridtest == rid )
			return( i );
	}
	return( -1 );
}

int CResourceRefArray::FindResourceName( RES_TYPE restype, LPCTSTR pszKey ) const
{
	// Is this resource already in the list ?
	CResourceLinkPtr pResLink = REF_CAST(CResourceLink,g_Cfg.ResourceGetDefByName( restype, pszKey ));
	if ( pResLink == NULL )
		return( false );

	return( FindArg(pResLink));
}

void CResourceRefArray::s_WriteProps( CScript& s, LPCTSTR pszKey ) const
{
	for ( int j=0;j<GetSize(); j++ )
	{
		CString sName = GetResourceName( j );
		ASSERT( sName[0] );
		ASSERT( sName[0] != '0' || sName[1] != '0' );
		s.WriteKey( pszKey, sName );
	}
}

//**********************************************
// -CResourceQty

int CResourceQty::WriteKey( TCHAR* pszArgs ) const
{
	int i=0;
	if ( GetResQty())
	{
		i = sprintf( pszArgs, "%d ", GetResQty());
	}
	i += strcpylen( pszArgs+i, g_Cfg.ResourceGetName( m_rid ));
	return( i );
}

int CResourceQty::WriteNameSingle( TCHAR* pszArgs ) const
{
	int i;
	CResourceDefPtr pResourceDef = g_Cfg.ResourceGetDef( m_rid );
	if ( pResourceDef )
	{
		i = strcpylen( pszArgs, pResourceDef->GetName());
	}
	else
	{
		i = strcpylen( pszArgs, g_Cfg.ResourceGetName( m_rid ));
	}
	return( i );
}

bool CResourceQty::LoadResQty( LPCTSTR& pszCmds )
{
	// Can be either order.:
	// "Name Qty" or "Qty Name"

	GETNONWHITESPACE( pszCmds );	// Skip leading spaces.

	m_iQty = 0;
	if ( ! isalpha( *pszCmds )) // might be { or .
	{
		// Quantity is first by default. 
		m_iQty = Exp_GetComplexRef(pszCmds);
		GETNONWHITESPACE( pszCmds );	// Skip leading spaces.
	}

	if ( *pszCmds == '\0' )
		return false;

	LPCTSTR pszPrv = pszCmds;
	m_rid = g_Cfg.ResourceGetID( RES_UNKNOWN, Exp_GetValueRef(pszCmds));

	if ( m_rid.GetResType() == RES_UNKNOWN )
	{
		// This means nothing to me!
		// STUPID HACK with skills !
		char szTmp[ EXPRESSION_MAX_KEY_LEN ];
		Exp_GetIdentifierString( szTmp, pszPrv );
		int iSkill = g_Cfg.FindSkillKey( szTmp, false );
		if ( iSkill >= 0 )
		{
			m_rid = CSphereUID( RES_Skill, iSkill );
		}
		else
		{
			DEBUG_ERR(( "Bad resource list id '%s'" LOG_CR, pszPrv ));
			return( false );
		}
	}

	GETNONWHITESPACE( pszCmds );	// Skip leading spaces.

	if ( ! m_iQty )	// trailing qty?
	{
		if ( *pszCmds == '\0' || *pszCmds == ',' )
		{
			m_iQty = 1;
		}
		else
		{
			m_iQty = Exp_GetComplexRef(pszCmds);
			GETNONWHITESPACE( pszCmds );	// Skip leading spaces.
		}
	}

	return( true );
}

//**********************************************
// -CResourceQtyArray

int CResourceQtyArray::FindResourceType( RES_TYPE type ) const
{
	// is this RES_TYPE in the array ?
	// -1 = fail
	for ( int i=0; i<GetSize(); i++ )
	{
		CSphereUID ridtest = ConstElementAt(i).GetResourceID();
		if ( type == ridtest.GetResType() )
			return( i );
	}
	return( -1 );
}

int CResourceQtyArray::FindResourceID( CSphereUID rid ) const
{
	// is this CSphereUID in the array ?
	// -1 = fail
	for ( int i=0; i<GetSize(); i++ )
	{
		CSphereUID ridtest = ConstElementAt(i).GetResourceID();
		if ( rid == ridtest )
			return( i );
	}
	return( -1 );
}

int CResourceQtyArray::FindResourceMatch( CObjBase* pObj ) const
{
	// Is there a more vague match in the array ?
	// Use to find intersection with this pOBj raw material and BaseResource creation elements.
	for ( int i=0; i<GetSize(); i++ )
	{
		CSphereUID ridtest = ConstElementAt(i).GetResourceID();
		if ( pObj->IsResourceMatch( ridtest, 0 ))
			return( i );
	}
	return( -1 );
}

bool CResourceQtyArray::IsResourceMatchAll( CObjBase* pObj ) const
{
	// Check all required skills and non-consumable items.
	// RETURN:
	//  false = failed.

	for ( int i=0; i<GetSize(); i++ )
	{
		CSphereUID ridtest = ConstElementAt(i).GetResourceID();
		if ( ! pObj->IsResourceMatch( ridtest, ConstElementAt(i).GetResQty()))
			return( false );
	}

	return( true );
}

int CResourceQtyArray::s_LoadKeys( LPCTSTR pszCmds )
{
	// 0 = clear the list.
	// RETURN:
	//  Number of entries added.

	int iValid = 0;
	ASSERT(pszCmds);
	while ( *pszCmds )
	{
		if ( *pszCmds == '0' &&
			( pszCmds[1] == '\0' || pszCmds[1] == ',' ))
		{
			RemoveAll();	// clear any previous stuff.
			pszCmds ++;
		}
		else
		{
			CResourceQty res;
			if ( ! res.LoadResQty( pszCmds ))
				break;

			if ( res.GetResourceID().IsValidRID())
			{
				// Replace any previous refs to this same entry ?
				int i = FindResourceID( res.GetResourceID() );
				if ( i >= 0 )
				{
					SetAt(i,res);
				}
				else
				{
					Add(res);
				}
				iValid++;
			}
		}

		if ( *pszCmds != ',' )
		{
			break;
		}

		pszCmds++;
	}

	return( iValid );
}

void CResourceQtyArray::v_GetKeys( CGVariant& vVal ) const
{
	TCHAR szTmp[ CSTRING_MAX_LEN ];
	int j=0;
	for ( int i=0; i<GetSize(); i++ )
	{
		if ( i )
		{
			j += sprintf( szTmp+j, "," );
		}
		j += ConstElementAt(i).WriteKey( szTmp+j );
	}
	szTmp[j] = '\0';
	vVal = CGVariant((LPCTSTR)szTmp);
}

void CResourceQtyArray::v_GetNames( CGVariant& vVal ) const
{
	TCHAR szTmp[ CSTRING_MAX_LEN ];
	int j=0;
	for ( int i=0; i<GetSize(); i++ )
	{
		if ( i )
		{
			j += sprintf( szTmp+j, ", " );
		}
		if ( ConstElementAt(i).GetResQty())
		{
			j += sprintf( szTmp+j, "%d ", ConstElementAt(i).GetResQty());
		}
		j += ConstElementAt(i).WriteNameSingle( szTmp+j );
	}
	szTmp[j] = '\0';
	vVal = CGVariant((LPCTSTR)szTmp);
}

bool CResourceQtyArray::operator == ( const CResourceQtyArray& array ) const
{
	// compare 2 arrays. assume no order.

	if ( GetSize() != array.GetSize())
		return( false );

	for ( int i=0; i<GetSize(); i++ )
	{
		for ( int j=0;; j++ )
		{
			if ( j>=array.GetSize())
				return( false );
			if ( ! ( ConstElementAt(i).GetResourceID() == array[j].GetResourceID() ))
				continue;
			if ( ConstElementAt(i).GetResQty() != array[j].GetResQty() )
				continue;
			break;
		}
	}
	return( true );
}

//***************************************************
// CSphereResourceMgr

LPCTSTR const CSphereResourceMgr::sm_szResourceBlocks[RES_QTY+1] =	// static
{
	"AAAUNUSED",	// unused / unknown.
#define CRESOURCETAG(a,b,c) #a,
#include "cresourcetag.tbl"
#undef CRESOURCETAG
	NULL,
};

CSphereUID CSphereResourceMgr::ResourceCheckIDType( RES_TYPE restype, LPCTSTR pszName )
{
	// Passively check for the id. no error if not here.
	CSphereUID rid( m_Const.FindKeyInt(pszName));	// May be some complex expression {}
	if ( rid.GetResType() != restype )
	{
		rid.InitUID();
	}
	return( rid );
}

CSphereUID CSphereResourceMgr::ResourceGetID( RES_TYPE restype, HASH_INDEX index ) // static 
{
	// index may hase type flags or not.
	CSphereUID rid( index );

	if ( restype != RES_UNKNOWN && 
		rid.IsValidUID() &&
		rid.GetResType() == RES_UNKNOWN )
	{
		// A type was not provided.
		// Label it with the type we want.
		return CSphereUID( restype, rid.GetResIndex());
	}

	// just let it pass through as is.
	return( rid );
}

CSphereUID CSphereResourceMgr::ResourceGetIDByName( RES_TYPE restype, LPCTSTR pszName )
{
	// Find the Resource ID given this name.
	// We are NOT creating a new resource. just looking up an existing one
	// NOTE: Do not enforce the restype.
	//		Just fill it in if we are not sure what the type is.
	//  ie. i could ref an item by it's item number if i want.
	// NOTE:
	//  Some restype's have private name spaces. (ie. RES_Area and RES_Server)
	// RETURN:
	//  pszName is now set to be after the expression.

	// We are NOT creating.

#if 0
	// Try to handle private name spaces.
	switch ( restype )
	{
	case RES_Account:
	case RES_Area:
	case RES_GMPage:
	case RES_Room:
	case RES_Server:
	case RES_Sector:
		break;
	}
#endif

	// May be some complex expression (1+2)
	return ResourceGetID( restype, Exp_GetValue(pszName));
}

int CSphereResourceMgr::ResourceGetIndex( RES_TYPE restype, LPCTSTR pszName )
{
	// Get a resource but do not enforce the type.
	CSphereUID rid = ResourceGetIDByName( restype, pszName );
	if ( ! rid.IsValidRID())
		return( -1 );
	return( rid.GetResIndex());
}

int CSphereResourceMgr::ResourceGetIndexType( RES_TYPE restype, LPCTSTR pszName )
{
	// Get a resource index of just this restype.
	CSphereUID rid = ResourceGetIDByName( restype, pszName );
	if ( rid.GetResType() != restype )
		return( -1 );	// not UID_INDEX_CLEAR ?
	return( rid.GetResIndex());
}

const CScript* CSphereResourceMgr::SetScriptContext( const CScript* pScriptContext )
{
	CSphereThread* pThread = CSphereThread::GetCurrentThread();
	if ( pThread )
	{
		return pThread->SetScriptContext( pScriptContext );
	}
	else
	{
		return NULL;
	}
}


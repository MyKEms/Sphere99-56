//
// CContain.CPP
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
#include "stdafx.h"	// predef header.

//***************************************************************************
// -CContainer

#ifdef USE_JSCRIPT
#define CCONTAINERMETHOD(a,b,c) JSCRIPT_METHOD_IMP(CContainer,a)
#include "ccontainermethods.tbl"
#undef CCONTAINERMETHOD
#endif

const CScriptMethod CContainer::sm_Methods[CContainer::M_QTY+1] = // static
{
#define CCONTAINERMETHOD(a,b,c) CSCRIPT_METHOD_IMP(a,b,c)
#include "ccontainermethods.tbl"
#undef CCONTAINERMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP0(Container,NULL,CContainer::sm_Methods);

void CContainer::OnWeightChange( int iChange )
{
	// Propagate the weight change up the stack if there is one.
	m_totalweight += iChange;

#ifdef _DEBUG
	if ( m_totalweight < 0 )
	{
		DEBUG_CHECK(( "Trap" ));
	}
#endif

	DEBUG_CHECK( m_totalweight >= 0 );
}

int CContainer::FixWeight()
{
	// If there is some sort of ASSERT during item add then this is used to fix it.
	// NOTE: Often the bankbox is added to your weight !
	m_totalweight = 0;

	CItemPtr pItem=GetHead();
	for ( ; pItem!=NULL; pItem=pItem->GetNext())
	{
		CItemContainerPtr pCont = REF_CAST(CItemContainer,pItem);
		if ( pCont )
		{
			pCont->FixWeight();
			if ( ! pCont->IsWeighed())
				continue;	// Bank box doesn't count for wieght.
		}
		m_totalweight += pItem->GetWeight();
	}

	return( m_totalweight );
}

void CContainer::ContentAddPrivate( CItemPtr pItem )
{
	// We are adding to a CChar or a CItemContainer
	ASSERT( pItem != NULL );
	ASSERT( pItem->IsValidUID());	// it should be valid at this point.
	if ( IsMyChild(pItem))
		return;

#if 0
	if ( g_Log.IsLogged( LOGL_TRACE ))
	{
		DEBUG_CHECK( GetCount() <= UO_MAX_ITEMS_CONT );
	}
#endif

	CGObList::InsertHead( pItem );
	OnWeightChange( pItem->GetWeight());
}

void CContainer::OnRemoveOb( CGObListRec* pObRec )	// Override this = called when removed from list.
{
	// remove this object from the container list.
	// Overload the RemoveAt for general lists to come here.
	CItemPtr pItem = STATIC_CAST(CItem,pObRec);
	ASSERT(pItem);

	OnWeightChange( -pItem->GetWeight());

	CGObList::OnRemoveOb( pItem );
	ASSERT( pItem->GetParent() == NULL );
	// It is no place for the moment.
}

void CContainer::s_SerializeContent( CGFile& a ) const
{
	// Write out all the items in me.
	CItemPtr pItemNext;
	for ( CItemPtr pItem=GetHead(); pItem!=NULL; pItem=pItemNext)
	{
		pItemNext = pItem->GetNext();
		ASSERT( IsMyChild(pItem));
		pItem->s_Serialize(a);
	}
}

void CContainer::s_WriteContent( CScript& s ) const
{
	// Write out all the items in me.
	CItemPtr pItemNext;
	for ( CItemPtr pItem=GetHead(); pItem!=NULL; pItem=pItemNext)
	{
		pItemNext = pItem->GetNext();
		ASSERT( IsMyChild(pItem));
		pItem->s_WriteSafe(s);
	}
}

long CContainer::GetMakeValue()
{
	long lPrice = 0;
	CItemPtr pItemNext;
	for ( CItemPtr pItem=GetHead(); pItem!=NULL; pItem=pItemNext)
	{
		pItemNext = pItem->GetNext();
		ASSERT( IsMyChild(pItem));
		lPrice += pItem->GetMakeValue();
	}
	return lPrice;
}

CItemPtr CContainer::ContentFind( CSphereUID rid, DWORD dwArg, int iDecendLevels ) const
{
	// send all the items in the container.

	if ( rid.GetResIndex() == 0 )
		return( NULL );

	CItemPtr pItem=GetHead();
	for ( ; pItem!=NULL; pItem=pItem->GetNext())
	{
		if ( pItem->IsResourceMatch( rid, dwArg ))
			break;
		if ( iDecendLevels <= 0 )
			continue;
		CItemContainerPtr pCont = REF_CAST(CItemContainer,pItem);
		if ( pCont != NULL )
		{
			if ( ! pCont->IsSearchable())
				continue;
			CItemPtr pItemInCont = pCont->ContentFind( rid, dwArg, iDecendLevels-1 );
			if ( pItemInCont )
				return( pItemInCont );
		}
	}
	return( pItem );
}

bool CContainer::ContentFindKeyFor( CItem* pLocked ) const
{
	// Look for the key that fits this in my possesion.
	DEBUG_CHECK( pLocked->IsTypeLockable());
	return( ContentFind( CSphereUID( RES_TypeDef, IT_KEY ), pLocked->m_itContainer.m_lockUID ) != NULL );
}

CItemPtr CContainer::ContentFindRandom( void ) const
{
	// returns Pointer of random item, NULL if player carrying none
	return static_cast<CItem*>( GetAt( Calc_GetRandVal( GetCount())));
}

int CContainer::ContentConsume( CSphereUID rid, int amount, bool fTest, DWORD dwArg )
{
	// Check this container and all sub containers for this resource.
	// ARGS:
	//  dwArg = a hack for ores.
	// RETURN:
	//  0 = all consumed ok.
	//  # = number left to be consumed. (still required)

	if ( rid.GetResIndex() == 0 )
		return( amount );	// from skills menus.

	CItemPtr pItemNext;
	for ( CItemPtr pItem=GetHead(); pItem!=NULL; pItem=pItemNext)
	{
		pItemNext = pItem->GetNext();

		if ( pItem->IsResourceMatch( rid, dwArg ))
		{
			amount -= pItem->ConsumeAmount( amount, fTest );
			if ( amount <= 0 )
				break;
		}

		CItemContainerPtr pCont = REF_CAST(CItemContainer,pItem);
		if ( pCont != NULL )	// this is a sub-container.
		{
			if ( rid == CSphereUID(RES_TypeDef,IT_GOLD))
			{
				if ( pCont->IsType(IT_CONTAINER_LOCKED))
					continue;
			}
			else
			{
				if ( ! pCont->IsSearchable())
					continue;
			}
			amount = pCont->ContentConsume( rid, amount, fTest, dwArg );
			if ( amount <= 0 )
				break;
		}
	}
	return( amount );
}

int CContainer::ContentCount( CSphereUID rid, DWORD dwArg )
{
	// Calculate total (gold or other items) in this recursed container
	return( INT_MAX - ContentConsume( rid, INT_MAX, true, dwArg ));
}

void CContainer::ContentAttrMod( WORD wAttr, bool fSet )
{
	// Mark the attr
	for ( CItemPtr pItem=GetHead(); pItem!=NULL; pItem=pItem->GetNext())
	{
		if ( fSet )
		{
			pItem->SetAttr( wAttr );
		}
		else
		{
			pItem->ClrAttr( wAttr );
		}
		CItemContainerPtr pCont = REF_CAST(CItemContainer,pItem);
		if ( pCont != NULL )	// this is a sub-container.
		{
			pCont->ContentAttrMod( wAttr, fSet );
		}
	}
}

void CContainer::ContentsDump( const CPointMap& pt, WORD wAttrLeave )
{
	// Just dump the contents onto the ground.
	DEBUG_CHECK( pt.IsValidPoint());
	wAttrLeave |= ATTR_NEWBIE|ATTR_MOVE_NEVER|ATTR_CURSED2|ATTR_BLESSED2;
	CItemPtr pItemNext;
	for ( CItemPtr pItem=GetHead(); pItem!=NULL; pItem=pItemNext)
	{
		pItemNext = pItem->GetNext();
		if ( pItem->IsAttr(wAttrLeave))
			continue;	// hair and newbie stuff.
		// ??? scatter a little ?
		pItem->MoveToCheck( pt );
	}
}

void CContainer::ContentsTransfer( CItemContainer* pCont, bool fNoNewbie )
{
	// Move all contents to another container. (pCont)
	if ( pCont == NULL )
		return;

	CItemPtr pItemNext;
	for ( CItemPtr pItem=GetHead(); pItem!=NULL; pItem=pItemNext)
	{
		pItemNext = pItem->GetNext();
		if ( fNoNewbie && pItem->IsAttr( ATTR_NEWBIE|ATTR_MOVE_NEVER|ATTR_CURSED2|ATTR_BLESSED2 ))	// keep newbie stuff.
			continue;
		pCont->ContentAdd( pItem );	// add content
	}
}

int CContainer::ResourceConsumePart( const CResourceQtyArray* pResources, int iReplicationQty, int iDamagePercent, bool fTest )
{
	// Consume just some of the resources.
	// ARGS:
	//	pResources = the resources i need to make 1 replication of this end product.
	//  iDamagePercent = 0-100
	// RETURN:
	//  -1 = all needed items where present.
	// index of the item we did not have.

	if ( iDamagePercent <= 0 )
		return( -1 );

	int iMissing = -1;

	int iQtyRes = pResources->GetSize();
	for ( int i=0; i<iQtyRes; i++ )
	{
		int iResQty = pResources->ConstElementAt(i).GetResQty();
		if (iResQty<=0)	// not sure why this would be true
			continue;

		int iQtyTotal = ( iResQty* iReplicationQty );
		if ( iQtyTotal <= 0 )
			continue;
		iQtyTotal = IMULDIV( iQtyTotal, iDamagePercent, 100 );
		if ( iQtyTotal <= 0 )
			continue;

		CSphereUID rid = pResources->ConstElementAt(i).GetResourceID();
		int iRet = ContentConsume( rid, iQtyTotal, fTest );
		if ( iRet )
		{
			iMissing = i;
		}
	}

	return( iMissing );
}

int CContainer::ResourceConsume( const CResourceQtyArray* pResources, int iReplicationQty, bool fTest, DWORD dwArg )
{
	// Consume or test all the required resources.
	// ARGS:
	//	pResources = the resources i need to make 1 replication of this end product.
	// RETURN:
	//  how many whole objects can be made. <= iReplicationQty

	if ( iReplicationQty <= 0 )
		iReplicationQty = 1;
	if ( ! fTest && iReplicationQty > 1 )
	{
		// Test what the max number we can really make is first !
		// All resources must be consumed with the same number.
		iReplicationQty = ResourceConsume( pResources, iReplicationQty, true, dwArg );
	}

	int iQtyMin = INT_MAX;
	for ( int i=0; i<pResources->GetSize(); i++ )
	{
		int iResQty = pResources->ConstElementAt(i).GetResQty();
		if (iResQty<=0)	// not sure why this would be true
			continue;

		int iQtyTotal = ( iResQty* iReplicationQty );
		CSphereUID rid = pResources->ConstElementAt(i).GetResourceID();

		int iQtyCur = iQtyTotal - ContentConsume( rid, iQtyTotal, fTest, dwArg );
		iQtyCur /= iResQty;
		if ( iQtyCur < iQtyMin )
			iQtyMin = iQtyCur;
	}

	if ( iQtyMin == INT_MAX )	// it has no resources ? So i guess we can make it from nothing ?
		return( iReplicationQty );

	return( iQtyMin );
}

int CContainer::ContentCountAll() const
{
	// RETURN:
	//  A count of all the items in this container and sub contianers.
	int iTotal = 0;
	for ( CItemPtr pItem=GetHead(); pItem!=NULL; pItem=pItem->GetNext())
	{
		iTotal++;
		CItemContainerPtr pCont = REF_CAST(CItemContainer,pItem);
		if ( pCont == NULL )
			continue;
		// if ( ! pCont->IsSearchable()) continue; // found a sub
		iTotal += pCont->ContentCountAll();
	}
	return( iTotal );
}

HRESULT CContainer::s_MethodContainer( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp < 0 )
	{
		return HRES_UNKNOWN_PROPERTY;
	}

	switch(iProp)
	{
	case M_DeleteCont:
		if ( vArgs.IsEmpty())
			return( HRES_BAD_ARG_QTY );
		{
			int index = vArgs.GetInt();
			if ( index <= GetCount())
			{
				static_cast<CItem*>(GetAt(index))->DeleteThis();
				break;
			}
		}
		break;
	case M_EmptyCont:
		DeleteAll();
		break;
	case M_FindId:
		// Get item id from the container.
		vValRet.SetRef( ContentFind( g_Cfg.ResourceGetIDByName( RES_ItemDef, vArgs.GetPSTR())));
		break;
	case M_FindCont:
		// Get enumerated item from the container.
		vValRet.SetRef( static_cast<CScriptObj*>(static_cast<CItem*>(GetAt( vArgs.GetInt()))));
		break;
	case M_FindType:
		vValRet.SetRef( ContentFind( g_Cfg.ResourceGetIDByName( RES_TypeDef, vArgs.GetPSTR())));
		break;
	case M_ResCount:
		if ( vArgs.IsEmpty())
		{
			vValRet.SetInt( GetCount());	// count all.
		}
		else
		{
			// How much of this?
			vValRet.SetInt( ContentCount( g_Cfg.ResourceGetID( RES_ItemDef, vArgs )));
		}
		break;
	case M_ResTest:
		{
		CResourceQtyArray Resources;
		Resources.s_LoadKeys( vArgs );
		vValRet.SetInt( ResourceConsume( &Resources, 1, true ));
		}
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}


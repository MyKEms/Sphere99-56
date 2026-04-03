//
// cvendoritem.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
// Implementation file for the CItemVendable class
//
// Initial version by Catharsis.  11/20/1999
//

#include "stdafx.h"	// predef header.

const CScriptProp CItemVendable::sm_Props[CItemVendable::P_QTY+1] =
{
#define CITEMVENDPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "citemvendprops.tbl"
#undef CITEMVENDPROP
	NULL,
};

CSCRIPT_CLASS_IMP1(ItemVendable,CItemVendable::sm_Props,NULL,NULL,Item);

CItemVendable::CItemVendable( ITEMID_TYPE id, CItemDef* pDef ) : CItem( id, pDef )
{
	// Constructor
	m_lPrice = 0;
	m_wQuality = 0;
}

CItemVendable::~CItemVendable()
{
	// Nothing really to do...no dynamic memory has been allocated.
}

void CItemVendable::SetQuality(WORD quality = 0)
{
	DEBUG_CHECK( quality <= 200 );
	m_wQuality = quality;
}

void CItemVendable::DupeCopy( const CItem* pItem )
{
	CItem::DupeCopy( pItem );

	const CItemVendable* pVendItem = PTR_CAST(const CItemVendable,pItem);
	DEBUG_CHECK(pVendItem);
	if ( pVendItem == NULL )
		return;

	m_lPrice = pVendItem->m_lPrice;
	m_wQuality = pVendItem->m_wQuality;
}

HRESULT CItemVendable::s_PropGet(LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole *pSrc)
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if (iProp<0)
	{
		return CItem::s_PropGet( pszKey, vValRet, pSrc );
	}
	switch (iProp)
	{
	case P_BuyPrice:
	case P_SellPrice:
		return(HRES_INVALID_FUNCTION);
	case P_Price:
		vValRet.SetInt( m_lPrice );
		break;
	case P_Quality:
		vValRet.SetInt( GetQuality());
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return NO_ERROR;
}

HRESULT CItemVendable::s_PropSet(LPCTSTR pszKey, CGVariant& vVal)
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if (iProp<0)
	{
		return( CItem::s_PropSet(pszKey, vVal));
	}
	switch (iProp)
	{
	case P_BuyPrice:
    case P_SellPrice:
	case P_Price:	
		m_lPrice = vVal.GetInt();
		break;
	case P_Quality:
		SetQuality( vVal.GetInt());
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return NO_ERROR;
}

void CItemVendable::s_Serialize( CGFile& a )
{
	// Read and write binary.
	CItem::s_Serialize(a);
}

void CItemVendable::s_WriteProps(CScript &s)
{
	CItem::s_WriteProps(s);
	if ( GetQuality() != 0 )
	{
		s.WriteKeyInt( "QUALITY", GetQuality());
	}

	// am i on a vendor right now ?
	if ( m_lPrice > 0 )
	{
		s.WriteKeyInt( "PRICE", m_lPrice );
	}
}

void CItemVendable::Restock( bool fSellToPlayers )
{
	// This is on a non-pet vendor.
	// allow prices to fluctuate randomly (per vendor) but hold the values for a bit.

	ASSERT( IsItemInContainer());
	if ( m_lPrice < 0 )
		m_lPrice = 0;	// signal to recalc this later.

	if ( fSellToPlayers )
	{
		// restock to my full amount.

		int iAmountRestock = GetContainedLayer();
		if ( ! iAmountRestock )
		{
			SetContainedLayer(1);
			iAmountRestock = 1;
		}

		SetAmount( iAmountRestock );	// restock amount
	}
	else
	{
		// Clear the amount i have bought from players.
		// GetAmount() is the max that i will buy in the next period.
		SetContainedLayer(0);
	}
}

void CItemVendable::SetPlayerVendorPrice( int lPrice )
{
	// This can only be inside a vendor container.
	// DEBUG_CHECK( g_World.IsLoading() || IsItemInContainer() );

	if ( lPrice < 0 )
		lPrice = 0;
	m_lPrice = lPrice;
}

long CItemVendable::GetBasePrice()
{
	// Get the price that the player set on his vendor
	// This will show as negative for floating prices.

	if ( m_lPrice == 0 )	// short cut.
	{
		CItemDefPtr pItemDef;
		if ( IsType( IT_DEED ))
		{
			// Deeds just represent the item they are deeding.
			pItemDef = g_Cfg.FindItemDef((ITEMID_TYPE) RES_GET_INDEX(m_itDeed.m_Type));
			if ( pItemDef == NULL )
				return( 1 );
		}
		else
		{
			pItemDef = Item_GetDef();
		}

		m_lPrice = - pItemDef->GetMakeValue( GetQuality());
	}

	return( m_lPrice );
}

long CItemVendable::GetMakeValue()
{
	// Get the actual world economy value of the item.
	long lPrice = GetBasePrice();
	if ( lPrice < 0 )
		lPrice = -lPrice;
	return lPrice * GetAmount();
}

long CItemVendable::GetVendorPrice( int iConvertFactor )
{
	// Player is buying/selling from a vendor.
	// ASSUME this item is on the vendor !
	// Consider: (if not on a player vendor)
	//  Quality of the item.
	//  rareity of the item.
	// NOTE: 
	//   DO NOT consider amount.
	// ARGS:
	// iConvertFactor will consider:
	//  Vendors Karma.
	//  Players Karma
	// -100 = player selling to the vendor (lower price)
	// +100 = vendor selling to player (higher price)

	if ( iConvertFactor <= -100 )
	{
		return 0;
	}

	long lPrice = GetBasePrice();
	if ( lPrice < 0 )	// set on player vendor.
	{
		lPrice = -lPrice;
	}

	lPrice += IMULDIV( lPrice, iConvertFactor, 100 );
	return lPrice;
}

bool CItemVendable::IsValidSaleItem( bool fBuyFromVendor ) const
{
	// Can this individual item be sold or bought ?
	if ( ! IsMovableType())
	{
		DEBUG_ERR(( "Vendor uid=0%lx selling unmovable item %s='%s'" LOG_CR, GetTopLevelObj()->GetUID(), GetResourceName(), (LPCTSTR) GetName()));
		return( false );
	}
	if ( ! fBuyFromVendor )
	{
		// cannot sell these to a vendor.
		if ( IsAttr(ATTR_NEWBIE|ATTR_MOVE_NEVER))
			return( false );	// spellbooks !
	}
	if ( IsType(IT_COIN))
		return( false );
	return( true );
}

bool CItemVendable::IsValidNPCSaleItem() const
{
	// This item is in an NPC's vendor box.
	// Is it a valid item that NPC's should be selling ?

	CItemDefPtr pItemDef = Item_GetDef();
	ASSERT(pItemDef);

	if ( m_lPrice <= 0 && pItemDef->GetMakeValue(0) <= 0 )
	{
		DEBUG_ERR(( "Vendor uid=0%lx selling unpriced item %s='%s'" LOG_CR, GetTopLevelObj()->GetUID(), GetResourceName(), (LPCTSTR) GetName()));
		return( false );
	}

	if ( ! IsValidSaleItem( true ))
	{
		DEBUG_ERR(( "Vendor uid=0%lx selling bad item %s='%s'" LOG_CR, GetTopLevelObj()->GetUID(), GetResourceName(), (LPCTSTR) GetName()));
		return( false );
	}

	return( true );
}


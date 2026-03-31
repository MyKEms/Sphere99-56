//
// CContain.CPP
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
#include "stdafx.h"	// predef header.

//***************************************************************************
// -CItemContainer

#ifdef USE_JSCRIPT
#define CITEMCONTAINERMETHOD(a,b,c) JSCRIPT_METHOD_IMP(CItemContainer,a)
#include "citemcontainermethods.tbl"
#undef CITEMCONTAINERMETHOD
#endif

const CScriptMethod CItemContainer::sm_Methods[CItemContainer::M_QTY+1] =
{
#define CITEMCONTAINERMETHOD(a,b,c)	CSCRIPT_METHOD_IMP(a,b,c)
#include "citemcontainermethods.tbl"
#undef CITEMCONTAINERMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP2(ItemContainer,CItemContainer::sm_Props,CItemContainer::sm_Methods,NULL,ItemVendable);

CItemContainer::CItemContainer( ITEMID_TYPE id, CItemDef* pItemDef ) :
	CItemVendable( id, pItemDef )
{
	// DEBUG_CHECK( pItemDef->IsTypeContainer());
	// m_fTinkerTrapped = false;
}

template<>
void CScriptClassTemplate<CItemContainer>::InitScriptClass()
{
	if ( IsInit())
		return;
	AddSubClass( &CContainer::sm_ScriptClass );
	CScriptClass::InitScriptClass();
}

void CItemContainer::Trade_Status( bool fCheck, bool fSetStabilizeTimer )
{
	// IT_EQ_TRADE_WINDOW
	// Update trade status check boxes to both sides.
	// This is called when i drop an item in the trade window.
	// ARGS:
	//
	// NOTE: If the other side takes stuff out then hits check really fast i could
	//  be agreeing to something i have not had time to see ?
	// Force minimum time delay since other side has moved stuff ?

	CItemPtr pPartner2 = g_World.ItemFind( m_uidLink );
	CItemContainerPtr pPartner = REF_CAST(CItemContainer,pPartner2);
	if ( pPartner == NULL )
		return;

	CCharPtr pChar1 = PTR_CAST(CChar,GetParent());
	if ( pChar1 == NULL )
		return;
	CCharPtr pChar2 = PTR_CAST(CChar,pPartner->GetParent());
	if ( pChar2 == NULL )
		return;

	m_itEqTradeWindow.m_fCheck = fCheck;
	if ( ! fCheck )
	{
		pPartner->m_itEqTradeWindow.m_fCheck = false;
		if ( fSetStabilizeTimer )
		{
			// If something is removed.
			SetTimeout( 2*TICKS_PER_SEC );	// just one second to stabilize.
		}
	}
	else
	{
		if ( ! pPartner->IsTimerExpired())
		{
			pChar1->WriteString( "Wait a second for the other trade window to stabilize" );
			return;
		}
		if ( ! pChar1->CanSeeLOS( pChar2->GetTopPoint(), NULL, SPHEREMAP_VIEW_SIZE ))
		{
			// To far away.
			pChar1->WriteString( "You are too far away to trade items" );
			return;
		}
	}

	CUOCommand cmd;
	cmd.SecureTrade.m_Cmd = XCMD_SecureTrade;
	cmd.SecureTrade.m_len = 0x11;
	cmd.SecureTrade.m_action = SECURE_TRADE_CHANGE;	// status
	cmd.SecureTrade.m_fname = 0;

	if ( pChar1->IsClient())
	{
		cmd.SecureTrade.m_UID = GetUID();
		cmd.SecureTrade.m_UID1 = m_itEqTradeWindow.m_fCheck;
		cmd.SecureTrade.m_UID2 = pPartner->m_itEqTradeWindow.m_fCheck;
		pChar1->GetClient()->xSendPkt( &cmd, 0x11 );
	}
	if ( pChar2->IsClient())
	{
		cmd.SecureTrade.m_UID = pPartner->GetUID();
		cmd.SecureTrade.m_UID1 = pPartner->m_itEqTradeWindow.m_fCheck;
		cmd.SecureTrade.m_UID2 = m_itEqTradeWindow.m_fCheck;
		pChar2->GetClient()->xSendPkt( &cmd, 0x11 );
	}

	// if both checked then done.
	if ( ! pPartner->m_itEqTradeWindow.m_fCheck || ! m_itEqTradeWindow.m_fCheck )
		return;

	CItemPtr pItemNext;
	CItemPtr pItem = GetHead();
	for ( ; pItem!=NULL; pItem=pItemNext)
	{
		pItemNext = pItem->GetNext();
		pChar2->ItemBounce( pItem );
	}

	pItem = pPartner->GetHead();
	for ( ; pItem!=NULL; pItem=pItemNext)
	{
		pItemNext = pItem->GetNext();
		pChar1->ItemBounce( pItem );
	}

	// done with trade.
	DeleteThis();
}

void CItemContainer::Trade_Delete()
{
	// Called when object deleted.

	ASSERT( IsType(IT_EQ_TRADE_WINDOW) );

	CCharPtr pChar = PTR_CAST(CChar,GetParent());
	if ( pChar == NULL )
		return;

	if ( pChar->IsClient())
	{
		// Send the cancel trade message.
		CUOCommand cmd;
		cmd.SecureTrade.m_Cmd = XCMD_SecureTrade;
		cmd.SecureTrade.m_len = 0x11;
		cmd.SecureTrade.m_action = SECURE_TRADE_CLOSE;
		cmd.SecureTrade.m_UID = GetUID();
		cmd.SecureTrade.m_UID1 = 0;
		cmd.SecureTrade.m_UID2 = 0;
		cmd.SecureTrade.m_fname = 0;
		pChar->GetClient()->xSendPkt( &cmd, 0x11 );
	}

	// Drop items back in my pack.
	CItemPtr pItemNext;
	for ( CItemPtr pItem = GetHead(); pItem!=NULL; pItem=pItemNext)
	{
		pItemNext = pItem->GetNext();
		pChar->ItemBounce( pItem );
	}

	// Kill my trading partner.
	CItemPtr pPartner2 = g_World.ItemFind( m_uidLink );
	CItemContainerPtr pPartner = REF_CAST(CItemContainer,pPartner2);
	if ( pPartner == NULL )
		return;

	m_uidLink.InitUID();	// unlink.
	pPartner->m_uidLink.InitUID();
	pPartner->DeleteThis();
}

void CItemContainer::OnWeightChange( int iChange )
{
	CContainer::OnWeightChange( iChange );
	if ( iChange == 0 )
		return;	// no change

	// some containers do not add weight to you.
	if ( ! IsWeighed())
		return;

	// Propagate the weight change up the stack if there is one.
	CContainer* pCont = PTR_CAST(CContainer,GetParent());
	if ( pCont == NULL )
		return;	// on ground.
	pCont->OnWeightChange( iChange );
}

POINT CItemContainer::GetRandContainerLoc() const
{
	// Max/Min Container Sizes.

	static const struct // we can probably get this from MUL file some place.
	{
		GUMP_TYPE m_gump;
		WORD m_minx;
		WORD m_miny;
		WORD m_maxx;
		WORD m_maxy;
	} sm_ContSize[] =
	{
		{ GUMP_RESERVED, 40, 50, 100, 100 },		// default.
		{ GUMP_SECURE_TRADE, 1, 1, 66, 26 },
		{ GUMP_CORPSE, 20, 85, 80, 185 },
		{ GUMP_PACK, 44, 65, 142, 150 },	// Open backpack
		{ GUMP_BAG, 29, 34, 93, 119 },		// Leather Bag
		{ GUMP_CHEST_GO_SI, 18, 105, 118, 169 },	// Gold and Silver Chest.
		{ GUMP_CHEST_WO_GO, 18, 105, 118, 169 },	// Wood with gold trim.
		{ GUMP_CHEST_SI, 18, 105, 118, 169 },	// silver chest.
		{ GUMP_BARREL, 33, 36, 98, 139 },		// Barrel
		{ GUMP_CRATE, 20, 10, 126, 91 },	// Wood Crate
		{ GUMP_BASKET_SQ, 19, 47, 138, 114 },	// Square picknick Basket
		{ GUMP_BASKET_RO, 33, 36,  98, 134 }, // Round Basket
		{ GUMP_CABINET_DK, 24, 96, 91, 143 },
		{ GUMP_CABINET_LT, 24, 96, 91, 143 },
		{ GUMP_BOOK_SHELF, 76, 12, 96, 59 },
		{ GUMP_BOX_WOOD, 16, 51, 140, 115 },	// small wood box with a lock
		{ GUMP_BOX_WOOD_OR, 16, 51, 140, 115 }, // Small wood box (ornate)(no lock)
		{ GUMP_BOX_GO_LO, 16, 51, 140, 115 }, // Gold/Brass box with a lock.
		{ GUMP_DRAWER_DK, 16, 17, 110, 85 },
		{ GUMP_DRAWER_LT, 16, 17, 110, 85 },
		{ GUMP_SHIP_HOLD, 46, 74, 152, 175 },

		{ GUMP_GAME_BOARD,	 4, 10, 220, 185 },	// Chess or checker board.
		{ GUMP_GAME_BACKGAM, 4, 10, 220, 185 },
	};

	// ??? pItemDef->m_ttContainer.m_dwMinXY to m_dwMaxXY
	// Get a random location in the container.

	CItemDefPtr pItemDef = Item_GetDef();
	GUMP_TYPE gump = pItemDef->IsTypeContainer();
	int i=0;
	for ( ;; i++ )
	{
		if (i>=COUNTOF(sm_ContSize))
		{
			i=0;
			DEBUG_ERR(( "unknown container gump id %d for 0%x" LOG_CR, gump, GetDispID()));
			break;
		}
		if ( sm_ContSize[i].m_gump == gump )
			break;
	}

	POINT pt;
	pt.x = sm_ContSize[i].m_minx + Calc_GetRandVal( sm_ContSize[i].m_maxx - sm_ContSize[i].m_minx );
	pt.y = sm_ContSize[i].m_miny + Calc_GetRandVal( sm_ContSize[i].m_maxy - sm_ContSize[i].m_miny );
	return pt;
}

void CItemContainer::ContentAdd( CItemPtr pItem, POINT pt )
{
	// Add to CItemContainer
	if ( pItem == NULL || ! pItem->IsValidUID())
		return;
	if ( pItem == this )
		return;	// infinite loop.

	if ( ! g_Serv.IsLoading())
	{
		if ( IsType(IT_EQ_TRADE_WINDOW) )
		{
			// Drop into a trade window.
			Trade_Status( false );
		}
		switch ( pItem->GetType())
		{
		case IT_LIGHT_LIT:
			// Douse the light in a pack ! (if possible)
			pItem->Use_Light();
			break;
		case IT_GAME_BOARD:
			// Can't be put into any sort of a container.
			// delete all it's pieces.
			CItemContainerPtr pCont = REF_CAST(CItemContainer,pItem);
			ASSERT(pCont);
			pCont->DeleteAll();
			break;
		}
	}

	if ( pt.x <= 0 || pt.y <= 0 ||
		pt.x > 512 || pt.y > 512 )	// invalid container location ?
	{
		// Try to stack it.
		if ( ! g_Serv.IsLoading() && pItem->Item_GetDef()->IsStackableType())
		{
			CItemPtr pItemNext;
			for ( CItemPtr pTry=GetHead(); pTry!=NULL; pTry=pItemNext)
			{
				pItemNext = pTry->GetNext();
				pt = pTry->GetContainedPoint();
				if ( pItem->Stack( pTry ))
					goto insertit;
			}
		}
		pt = GetRandContainerLoc();
	}

insertit:
	CContainer::ContentAddPrivate( pItem );
	pItem->SetContainedPoint( pt );

#ifdef _DEBUG
	if ( ! g_Serv.IsLoading())
	{
		DEBUG_CHECK( ! pItem->IsType(IT_EQ_MEMORY_OBJ));
	}
#endif

	switch ( GetType())
	{
	case IT_KEYRING: // empty key ring.
		SetKeyRing();
		break;
	case IT_EQ_VENDOR_BOX:
		if ( ! IsItemEquipped())	// vendor boxes should ALWAYS be equipped !
		{
			DEBUG_ERR(("Un-equipped vendor box uid=0%x is bad" LOG_CR, GetUID()));
			break;
		}
		{
			CItemVendablePtr pItemVend = REF_CAST(CItemVendable,pItem);
			if ( pItemVend == NULL )
			{
				g_Log.Event( LOG_GROUP_DEBUG, LOGL_WARN, "Vendor non-vendable item: %s" LOG_CR, (LPCTSTR) pItem->GetResourceName());
				pItem->DeleteThis();
				break;
			}

			pItemVend->SetPlayerVendorPrice( 0 );	// unpriced yet.
			pItemVend->SetContainedLayer( pItem->GetAmount());
		}
		break;
	}

	switch( pItem->GetID())
	{
	case ITEMID_BEDROLL_O_EW:
	case ITEMID_BEDROLL_O_NS:
		// Close the bedroll
		pItem->SetDispID( ITEMID_BEDROLL_C );
		break;
	}

	pItem->Update();
}

void CItemContainer::ContentAdd( CItemPtr pItem )
{
	if ( pItem == NULL )
		return;
	if ( IsMyChild(pItem))
		return;	// already here.
	CPointMap pt;	// invalid point.
	if ( g_Serv.IsLoading())
	{
		pt = pItem->GetUnkPoint();
	}
	ContentAdd( pItem, pt );
}

bool CItemContainer::IsItemInside(const CItem* pItem) const
{
	// Checks if a particular item is in a container or one of
	// it's subcontainers.

	for(;;)
	{
		if ( pItem == NULL )
			return( false );
		if ( pItem == this )
			return( true );
		pItem = PTR_CAST(CItemContainer,pItem->GetParent());
	}
}

void CItemContainer::OnRemoveOb( CGObListRec* pObRec )	// Override this = called when removed from list.
{
	// remove this object from the container list.
	CItemPtr pItem = STATIC_CAST(CItem,pObRec);
	ASSERT(pItem);
	if ( IsType(IT_EQ_TRADE_WINDOW))
	{
		// Remove from a trade window.
		Trade_Status( false, true );
	}
	if ( IsType(IT_EQ_VENDOR_BOX) && IsItemEquipped())	// vendor boxes should ALWAYS be equipped !
	{
		CItemVendablePtr pItemVend = REF_CAST(CItemVendable,pItem);
		if ( pItemVend != NULL )
		{
			pItemVend->SetPlayerVendorPrice( 0 );
		}
	}

	CContainer::OnRemoveOb(pObRec);

	if ( IsType(IT_KEYRING))	// key ring.
	{
		SetKeyRing();
	}
}

void CItemContainer::DupeCopy( const CItem* pItem )
{
	// Copy the contents of this item.

	CItemVendable::DupeCopy( pItem );

	CItemContainerPtr pContItem = PTR_CAST(CItemContainer,const_cast<CItem*>(pItem));
	DEBUG_CHECK(pContItem);
	if ( pContItem == NULL )
		return;

	for ( CItemPtr pContent = pContItem->GetHead(); pContent != NULL; pContent = pContent->GetNext())
	{
		ContentAdd( CreateDupeItem( pContent ), pContent->GetContainedPoint());
	}
}

void CItemContainer::MakeKey( CScriptConsole* pSrc )
{
	DEBUG_CHECK( IsType(IT_CONTAINER_LOCKED));
	SetType(IT_CONTAINER);
	m_itContainer.m_lockUID = GetUID();
	m_itContainer.m_lock_complexity = 500 + Calc_GetRandVal( 600 );

	CItemPtr pKey = CreateScript( ITEMID_KEY_COPPER, GET_ATTACHED_CCHAR(pSrc));
	ASSERT(pKey);
	pKey->m_itKey.m_lockUID = GetUID();
	ContentAdd( pKey );
}

void CItemContainer::SetKeyRing()
{
	// IT_KEYRING
	// Look of a key ring depends on how many keys it has in it.
	static const ITEMID_TYPE sm_Item_Keyrings[] =
	{
		ITEMID_KEY_RING0, // empty key ring.
		ITEMID_KEY_RING1,
		ITEMID_KEY_RING1,
		ITEMID_KEY_RING3,
		ITEMID_KEY_RING3,
		ITEMID_KEY_RING5,
	};

	int iQty = GetCount();
	if ( iQty >= COUNTOF(sm_Item_Keyrings))
		iQty = COUNTOF(sm_Item_Keyrings)-1;

	ITEMID_TYPE id = sm_Item_Keyrings[iQty];
	if ( id != GetDispID())
	{
		SetDispID(id);	// change the type as well.
		Update();
	}
}

bool CItemContainer::CanContainerHold( const CItem* pItem, CChar* pCharMsg )
{
	if ( pCharMsg == NULL || pItem == NULL )
		return( false );
	if ( pCharMsg->IsPrivFlag(PRIV_GM))	// a gm can do anything.
		return( true );

	if ( IsAttr(ATTR_MAGIC))
	{
		// Put stuff in a magic box
		pCharMsg->WriteString( "The item bounces out of the magic container" );
		return( false );
	}

	if ( GetCount() >= UO_MAX_ITEMS_CONT-1 )
	{
		pCharMsg->WriteString( "Too many items in that container" );
		return( false );
	}

	if ( ! IsItemEquipped() &&	// does not apply to my pack.
		pItem->IsContainer() &&
		pItem->Item_GetDef()->GetVolume() >= Item_GetDef()->GetVolume())
	{
		// is the container too small ? can't put barrels in barrels.
		pCharMsg->WriteString( "The container is too small for that" );
		return( false );
	}

	switch ( GetType())
	{
	case IT_EQ_BANK_BOX:
		{
			// Check if the bankbox will allow this item to be dropped into it.
			// Too many items or too much weight?

			// Check if the item dropped in the bank is a container. If it is
			// we need to calculate the number of items in that too.
			int iItemsInContainer = 0;
			CItemContainerPtr pContItem = PTR_CAST(CItemContainer,const_cast<CItem*>(pItem));
			if ( pContItem )
			{
				iItemsInContainer = pContItem->ContentCountAll();
			}

			// Check the total number of items in the bankbox and the ev.
			// container put into it.
			if (( ContentCountAll() + iItemsInContainer ) > g_Cfg.m_iBankIMax )
			{
				pCharMsg->WriteString( "Your bankbox can't hold more items." );
				return( false );
			}

			// Check the weightlimit on bankboxes.
			if (( GetTotalWeight() + pItem->GetWeight()) > g_Cfg.m_iBankWMax )
			{
				pCharMsg->WriteString( "Your bankbox can't hold more weight." );
				return( false );
			}
		}
		break;

	case IT_GAME_BOARD:
		if ( ! pItem->IsType(IT_GAME_PIECE))
		{
			pCharMsg->WriteString( "That is not a game piece" );
			return( false );
		}
		break;
	case IT_BBOARD:		// This is a trade window or a bboard.
		return( false );
	case IT_EQ_TRADE_WINDOW:
		// BBoards are containers but you can't put stuff in them this way.
		return( true );

	case IT_KEYRING: // empty key ring.
		if ( ! pItem->IsType(IT_KEY) )
		{
			pCharMsg->WriteString( "This is not a key." );
			return( false );
		}
		if ( ! pItem->m_itKey.m_lockUID )
		{
			pCharMsg->WriteString( "You can't put blank keys on a keyring." );
			return( false );
		}
		break;

	case IT_EQ_VENDOR_BOX:
		if ( pItem->IsTimerSet() && ! pItem->IsAttr(ATTR_DECAY))
		{
			pCharMsg->WriteString( "That's not a saleable item." );
			return( false );
		}
		break;

	case IT_TRASH_CAN:
		Sound( 0x235 ); // a little sound so we know it "ate" it.
		pCharMsg->WriteString( "You trash the item. It will decay in 15 seconds." );
		SetTimeout( 15*TICKS_PER_SEC );
		break;
	}

	return( true );
}

void CItemContainer::Restock()
{
	// Check for vendor type containers.

	if ( IsAttr(ATTR_MAGIC))
	{
		// Magic restocking container.
	}

	if ( IsItemEquipped())
	{
		// Part of a vendor.
		CCharPtr pChar = PTR_CAST(CChar,GetParent());
		ASSERT(pChar);
		if ( ! pChar->IsStatFlag( STATF_Pet ))	// Not on a hired vendor.
			switch ( GetEquipLayer())
		{
		case LAYER_VENDOR_STOCK:
			// Magic restock the vendors container.
			// Stuff the vendor sells
			{
				CItemPtr pItemNext;
				CItemPtr pItem = GetHead();
				for ( ; pItem!=NULL; pItem=pItemNext)
				{
					pItemNext = pItem->GetNext();
					CItemVendablePtr pVendItem = REF_CAST(CItemVendable,pItem);
					if ( pVendItem != NULL )
						pVendItem->Restock( true );
				}
			}
			break;

		case LAYER_VENDOR_EXTRA:
			// clear all this junk periodicallly.
			// sell it back for cash value ?
			DeleteAll();
			break;

		case LAYER_VENDOR_BUYS:
			{
				// Reset what we will buy from players.
				CItemPtr pItem= GetHead();
				for ( ; pItem!=NULL; pItem = pItem->GetNext())
				{
					CItemVendablePtr pVendItem = REF_CAST(CItemVendable,pItem);
					if ( pVendItem != NULL )
						pVendItem->Restock( false );
				}
			}
			break;

		case LAYER_BANKBOX:
			// Restock petty cash.
			if ( ! m_itEqBankBox.m_Check_Restock )
				m_itEqBankBox.m_Check_Restock = 10000;
			if ( m_itEqBankBox.m_Check_Amount < m_itEqBankBox.m_Check_Restock )
				m_itEqBankBox.m_Check_Amount = m_itEqBankBox.m_Check_Restock;
			return;
		}
	}

	SetTimeout( GetRestockTimeSeconds()* TICKS_PER_SEC );
}

void CItemContainer::OnOpenEvent( CChar* pCharOpener, const CObjBaseTemplate* pObjTop )
{
	// The container is being opened. Explode ? etc ?

	ASSERT(pCharOpener);

	if ( IsType(IT_EQ_BANK_BOX) ||
		IsType(IT_EQ_VENDOR_BOX))
	{
		CCharPtr pCharTop = PTR_CAST(CChar,const_cast<CObjBaseTemplate*>(pObjTop));
		if ( pCharTop == NULL )
			return;

		int iStones = GetTotalWeight()/WEIGHT_UNITS;
		if ( pCharTop == pCharOpener )
		{
			pCharOpener->Printf( "You have %d stones in your %s", iStones, (LPCTSTR) GetName() );
		}
		else
		{
			pCharOpener->Printf( "%s has %d stones in %s %s", pCharTop->GetPronoun(), iStones, (LPCTSTR) pCharTop->GetPossessPronoun(), (LPCTSTR) GetName() );
		}

		// these are special. they can only be opened near the designated opener.
		// see CanTouch
		m_itEqBankBox.m_pntOpen = pCharOpener->GetTopPoint();
	}
}

void CItemContainer::Game_Create()
{
	ASSERT( IsType(IT_GAME_BOARD) );
	if ( GetCount())
		return;	// already here.

	static const ITEMID_TYPE sm_Item_ChessPieces[] =
	{
		ITEMID_GAME1_ROOK,		// 42,4
		ITEMID_GAME1_KNIGHT,
		ITEMID_GAME1_BISHOP,
		ITEMID_GAME1_QUEEN,
		ITEMID_GAME1_KING,
		ITEMID_GAME1_BISHOP,
		ITEMID_GAME1_KNIGHT,
		ITEMID_GAME1_ROOK,		// 218,4

		ITEMID_GAME1_PAWN,		// 42,41
		ITEMID_GAME1_PAWN,
		ITEMID_GAME1_PAWN,
		ITEMID_GAME1_PAWN,
		ITEMID_GAME1_PAWN,
		ITEMID_GAME1_PAWN,
		ITEMID_GAME1_PAWN,
		ITEMID_GAME1_PAWN,		// 218, 41

		ITEMID_GAME2_PAWN,		// 44, 167
		ITEMID_GAME2_PAWN,
		ITEMID_GAME2_PAWN,
		ITEMID_GAME2_PAWN,
		ITEMID_GAME2_PAWN,
		ITEMID_GAME2_PAWN,
		ITEMID_GAME2_PAWN,
		ITEMID_GAME2_PAWN,		// 218, 164

		ITEMID_GAME2_ROOK,		// 42, 185
		ITEMID_GAME2_KNIGHT,
		ITEMID_GAME2_BISHOP,
		ITEMID_GAME2_QUEEN,
		ITEMID_GAME2_KING,
		ITEMID_GAME2_BISHOP,
		ITEMID_GAME2_KNIGHT,
		ITEMID_GAME2_ROOK,		// 218, 183
	};

	CPointMap pt;
	bool fChess = ( m_itGameBoard.m_GameType == 0 );

	static const WORD sm_ChessRow[] =
	{
		5,
		40,
		160,
		184,
	};
	static const WORD sm_CheckerRow[] =
	{
		30,
		55,
		178,
		205,
	};

	for ( int i=0; i<COUNTOF(sm_Item_ChessPieces); i++ )
	{
		// Add all it's pieces. (if not already added)
		CItemPtr pPiece = CItem::CreateBase( (fChess ) ? sm_Item_ChessPieces[i] :
			(( i >= (2*8)) ? ITEMID_GAME1_CHECKER : ITEMID_GAME2_CHECKER ));
		if ( pPiece == NULL )
			break;
		pPiece->SetType(IT_GAME_PIECE);
		if ( (i&7) == 0 )
		{
			pt.m_x = 42;
			pt.m_y = (fChess)?sm_ChessRow[ i/8 ]:sm_CheckerRow[ i/8 ];
		}
		else
		{
			pt.m_x += 25;
		}
		ContentAdd( pPiece, pt );
	}
}

void CItemContainer::s_Serialize( CGFile& a )
{
	CItemVendable::s_Serialize(a);
	s_SerializeContent(a);
}

void CItemContainer::s_WriteProps( CScript& s )
{
	CItemVendable::s_WriteProps(s);
	s_WriteContent(s);
}

HRESULT CItemContainer::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	// Execute command from script
	ASSERT(pSrc);

	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp < 0 )
	{
		HRESULT hRes = s_MethodContainer( pszKey, vArgs, vValRet, pSrc );
		if ( hRes != HRES_UNKNOWN_PROPERTY )
		{
			return(hRes);
		}
		return( CItemVendable::s_Method( pszKey, vArgs, vValRet, pSrc ));
	}

	switch ( iProp )
	{
	case M_FixWeight:
		FixWeight();
		break;

	case M_Open: // "OPEN"
		{
			CCharPtr pCharSrc = GET_ATTACHED_CCHAR(pSrc);
			if ( pCharSrc && pCharSrc->IsClient())
			{
				CClientPtr pClient = pCharSrc->GetClient();
				ASSERT(pClient);
				pClient->addItem( this );	// may crash client if we dont do this.
				pClient->addContainerSetup( this );
				OnOpenEvent( pCharSrc, GetTopLevelObj());
			}
		}
		break;

	case M_MakeKey:
		// Create a key for this locked item. Put it inside.
		MakeKey(pSrc);
		break;

	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}

bool CItemContainer::OnTick()
{
	// equipped or not.
	switch ( GetType())
	{
	case IT_TRASH_CAN:
		// Empty it !
		DeleteAll();
		return true;
	case IT_CONTAINER:
		if ( IsAttr(ATTR_MAGIC))
		{
			// Magic restocking container.
			Restock();
			return true;
		}
		break;
	case IT_EQ_BANK_BOX:
	case IT_EQ_VENDOR_BOX:
		// Restock this box.
		// Magic restocking container.
		Restock();
		return true;
	}
	return CItemVendable::OnTick();
}


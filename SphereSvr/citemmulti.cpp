//
// CItemMulti.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.

/////////////////////////////////////////////////////////////////////////////

const CScriptProp CItemMulti::sm_Props[CItemMulti::P_QTY+1] =
{
#define CITEMMULTIPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "citemmultiprops.tbl"
#undef CITEMMULTIPROP
	NULL,
};

#ifdef USE_JSCRIPT
#define CITEMMULTIMETHOD(a,b,c) JSCRIPT_METHOD_IMP(CItemMulti,a)
#include "citemmultimethods.tbl"
#undef CITEMMULTIMETHOD
#endif

const CScriptMethod CItemMulti::sm_Methods[CItemMulti::M_QTY+1] =
{
#define CITEMMULTIMETHOD(a,b,c) CSCRIPT_METHOD_IMP(a,b,c)
#include "citemmultimethods.tbl"
#undef CITEMMULTIMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP1(ItemMulti,CItemMulti::sm_Props,CItemMulti::sm_Methods,NULL,Item);

CItemMulti::CItemMulti( ITEMID_TYPE id, CItemDef* pItemDef ) :	// CItemDefMulti
	CItem( id, pItemDef )
{
	DEBUG_CHECK( PTR_CAST(const CItemDefMulti,pItemDef));
}

CItemMulti::~CItemMulti()
{
}

void CItemMulti::DeleteThis()
{
	// Must remove early because virtuals will fail in child destructor.
	// NOTE: This is dangerous to iterators. The "next" item may no longer be valid !
	Multi_Region_Delete();
	CItem::DeleteThis();	
}

int CItemMulti::Multi_GetMaxDist() const
{
	CItemDefMultiPtr pMultiDef = Multi_GetDef();
	if ( pMultiDef == NULL )
		return( 0 );
	return( pMultiDef->GetMaxDist());
}

void CItemMulti::Multi_Region_Delete()
{
	// Attempt to remove all the accessory junk.
	if ( ! m_pRegion)
		return;

	Multi_Region_UnRealize(true);	// unrealize before removed from ground.

	CWorldSearch Area( m_pRegion->m_pt, Multi_GetMaxDist());	// largest area.
	for(;;)
	{
		CItemPtr pItem = Area.GetNextItem();
		if ( pItem == NULL )
			break;
		if ( pItem == this )	// this gets deleted seperately.
			continue;
		if ( ! Multi_IsPartOf( pItem ))
			continue;
		pItem->DeleteThis();	// delete the key id for the door/key/sign.
	}

	m_pRegion.ReleaseRefObj();
}

bool CItemMulti::Multi_Region_Realize()
{
	// Add/move a region for the multi so we know when we are in it.
	// RETURN: ignored.

	DEBUG_CHECK( IsType(IT_MULTI) || IsType(IT_SHIP));
	ASSERT( IsTopLevel());

	CItemDefMultiPtr pMultiDef = Multi_GetDef();
	if ( pMultiDef == NULL )
	{
		DEBUG_ERR(( "Bad Multi type 0%x, uid=0%x" LOG_CR, GetID(), GetUID()));
		return false;
	}

	if ( ! m_pRegion)
	{
		// must create the region.
		m_pRegion = new CRegionComplex( GetUID());
	}
	else
	{
		// ASSERT( m_pRegion->GetRefCount()==0);
		m_pRegion->UnRealizeRegion(false);
	}

	// Get Background region.
	CPointMap pt = GetTopPoint();
	CRegionComplexPtr pRegionBack = REF_CAST(CRegionComplex,pt.GetRegion( REGION_TYPE_AREA ));
	ASSERT( pRegionBack );
	ASSERT( pRegionBack != m_pRegion );

	// Change the region rectangle.
	CRectMap rect;
	reinterpret_cast<CGRect&>(rect) = pMultiDef->m_rect;
	rect.OffsetRect( pt.m_x, pt.m_y );
	m_pRegion->SetRegionRect( rect );
	m_pRegion->m_pt = pt;

	DWORD dwFlags;
	if ( IsType(IT_SHIP))
	{
		dwFlags = REGION_FLAG_SHIP|REGION_FLAG_NOBUILDING;
	}
	else
	{
		// Houses get some of the attribs of the land around it.
		dwFlags = pRegionBack->GetRegionFlags() | REGION_FLAG_NOBUILDING;
	}
	dwFlags |= pMultiDef->m_dwRegionFlags;
	m_pRegion->SetRegionFlags( dwFlags );

	CGString sName;
	sName.Format( "%s (%s)", (LPCTSTR) pRegionBack->GetName(), (LPCTSTR) GetName());
	m_pRegion->SetName( sName );

	return( m_pRegion->RealizeRegion());
}

void CItemMulti::Multi_Region_UnRealize( bool fRetestChars )
{
	DEBUG_CHECK( IsType(IT_MULTI) || IsType(IT_SHIP));

	if ( ! m_pRegion.IsValidRefObj())
		return;

	m_pRegion->UnRealizeRegion( fRetestChars );
}

bool CItemMulti::Multi_CreateComponent( ITEMID_TYPE id, int dx, int dy, int dz, DWORD dwKeyCode )
{
	CItemPtr pItem = CreateTemplate( id, NULL, NULL );
	ASSERT(pItem);

	CPointMap pt = GetTopPoint();
	pt.m_x += dx;
	pt.m_y += dy;
	pt.m_z += dz;

	bool fNeedKey = false;

	switch ( pItem->GetType())
	{
	case IT_KEY:	// it will get locked down with the house ?
	case IT_SIGN_GUMP:
	case IT_SHIP_TILLER:
		pItem->m_itKey.m_lockUID = dwKeyCode;	// Set the key id for the key/sign.
		fNeedKey = true;
		break;
	case IT_DOOR:
		pItem->SetType(IT_DOOR_LOCKED);
		fNeedKey = true;
		break;
	case IT_CONTAINER:
		pItem->SetType(IT_CONTAINER_LOCKED);
		fNeedKey = true;
		break;
	case IT_SHIP_SIDE:
		pItem->SetType(IT_SHIP_SIDE_LOCKED);
		break;
	case IT_SHIP_HOLD:
		pItem->SetType(IT_SHIP_HOLD_LOCK);
		break;
	}

	pItem->SetAttr( ATTR_MOVE_NEVER | (GetAttr()&(ATTR_MAGIC|ATTR_INVIS)));
	pItem->SetHue( GetHue());
	pItem->m_uidLink = GetUID();	// lock it down with the structure.

	if ( pItem->IsTypeLockable() || pItem->IsTypeLocked())
	{
		pItem->m_itContainer.m_lockUID = dwKeyCode;	// Set the key id for the door/key/sign.
		pItem->m_itContainer.m_lock_complexity = 10000;	// never pickable.
	}

	pItem->MoveTo( pt );
	return( fNeedKey );
}

void CItemMulti::Multi_Create( CChar* pChar, DWORD dwKeyCodeOpt )
{
	// Create house or Ship extra stuff.
	// ARGS:
	//  dwKeyCode = set the key code for the doors/sides to this in case it's a drydocked ship.
	// NOTE:
	//  This can only be done after the house is given location.

	CItemDefMultiPtr pMultiDef = Multi_GetDef();
	// We are top level.
	if ( pMultiDef == NULL ||
		! IsTopLevel())
		return;

// WESTY MOD (MULTI CONFIRM)
	DWORD dwKeyCode = 0;
	if ( dwKeyCodeOpt == UID_INDEX_CLEAR || dwKeyCodeOpt == 0xffffffff )
		dwKeyCode = GetUID();
// END WESTY MOD

	// ??? SetTimeout( GetDecayTime()); house decay ?

	bool fNeedKey = false;
	int iQty = pMultiDef->m_Components.GetSize();
	for ( int i=0; i<iQty; i++ )
	{
		fNeedKey |= Multi_CreateComponent( 
			pMultiDef->m_Components[i].GetDispID(),
			pMultiDef->m_Components[i].m_dx,
			pMultiDef->m_Components[i].m_dy,
			pMultiDef->m_Components[i].m_dz,
			dwKeyCode );
	}
// WESTY MOD (MULTI CONFIRM)
	if( dwKeyCodeOpt == 0xffffffff )
		fNeedKey = false;
// END WESTY MOD
#if 0
	const CMulMultiType* pMultiMul = g_World.GetMultiItemDefs( GetDispID());
	if ( pMultiMul )
	{
		iQty = pMultiMul->GetItemCount();
		for ( int i=0; iQty--; i++ )
		{
			const CMulMultiItemRec* pMultiItem = pMultiMul->GetItem(i);
			ASSERT(pMultiItem);
			if ( pMultiItem->m_visible )	// client side.
				continue;
			fNeedKey |= Multi_CreateComponent( pMultiItem->GetDispID(),
				pMultiItem->m_dx,
				pMultiItem->m_dy,
				pMultiItem->m_dz,
				dwKeyCode );
		}
	}
#endif

	CItemPtr pKey;
	if ( fNeedKey )
	{
		// Create the key to the door.
		ITEMID_TYPE id = IsAttr(ATTR_MAGIC) ? ITEMID_KEY_MAGIC : ITEMID_KEY_COPPER ;
		pKey = CreateScript(id,pChar);
		ASSERT(pKey);
		pKey->SetType(IT_KEY);
		if ( g_Cfg.m_fAutoNewbieKeys )
			pKey->SetAttr(ATTR_NEWBIE);
		pKey->SetAttr(GetAttr()&ATTR_MAGIC);
		pKey->m_itKey.m_lockUID = dwKeyCode;
		pKey->m_uidLink = GetUID();
	}

	Multi_GetSign();	// set the m_uidLink

// WESTY MOD (MULTI CONFIRM)
	if ( pChar != NULL && dwKeyCodeOpt != 0xffffffff  )
// END WESTY MOD
	{
		m_itShip.m_uidCreator = pChar->GetUID();
		CItemMemoryPtr pMemory = pChar->Memory_AddObjTypes( this, MEMORY_GUARD );

		if ( pKey )
		{
			// Put in your pack
			pChar->GetPackSafe()->ContentAdd( pKey );

			// Put dupe key in the bank.
			pKey = CreateDupeItem( pKey );
			pChar->GetBank()->ContentAdd( pKey );
			pChar->WriteString( "The duplicate key is in your bank account" );
		}
	}
	else
	{
// WESTY MOD (MULTI CONFIRM)
		if( dwKeyCodeOpt == 0xffffffff )
		{
			pChar->m_Act.m_Targ = GetUID();
		}
		else
		// Just put the key on the front step ?
		DEBUG_CHECK( 0 );
// END WESTY MOD
	}
}

bool CItemMulti::Multi_IsPartOf( const CItem* pItem ) const
{
	// Assume it is in my area test already.
	// IT_MULTI
	// IT_SHIP
	if ( pItem == NULL )
		return( false );
	if ( pItem == this )
		return( true );
	return ( pItem->m_uidLink == GetUID());
}

int CItemMulti::Multi_IsComponentOf( const CItem* pItem, const CItemDefMulti* pMultiDef ) const
{
	// RETURN: Index of the component.
	// NOTE: Doors can actually move so this won't work for them !

	ASSERT(pMultiDef);

	if ( ! Multi_IsPartOf( pItem ))
		return( -1 );

	CPointMap pt = pItem->GetTopPoint();

	int xdiff = pt.m_x - GetTopPoint().m_x ;
	int ydiff = pt.m_y - GetTopPoint().m_y;
	int zdiff = pt.m_z - GetTopPoint().m_z;

	int iQty = pMultiDef->m_Components.GetSize();
	for ( int i=0; i<iQty; i++ )
	{
		if ( zdiff != pMultiDef->m_Components[i].m_dz )	// must always have same z (cannot change)
			continue;
		// x,y can change by 1 ? for doors !
		if ( xdiff != pMultiDef->m_Components[i].m_dx ||
			ydiff != pMultiDef->m_Components[i].m_dy )
			continue;
		return( i );
	}

	return( -1 );
}

CItemPtr CItemMulti::Multi_FindItemComponent( int iComp ) const
{
	// Enumerate the base componenets 
	// Find this components instance in the world.

	CItemDefMultiPtr pMultiDef = Multi_GetDef();
	if ( pMultiDef == NULL )
		return( NULL );
	if ( ! pMultiDef->m_Components.IsValidIndex(iComp))
		return NULL;

	const CMulMultiItemRec& Comp = pMultiDef->m_Components[iComp];

	CPointMap pt = GetTopPoint();
	pt.Move( Comp.m_dx, Comp.m_dy, Comp.m_dz );

	CWorldSearch Area( pt, 2 );
	for(;;)
	{
		CItemPtr pItem = Area.GetNextItem();
		if ( pItem == NULL )
			break;
		if ( Multi_IsComponentOf( pItem, pMultiDef ) == iComp )
			return( pItem );
	}

	return( NULL );
}

CItemPtr CItemMulti::Multi_FindItemType( IT_TYPE type ) const
{
	// Find a part of this multi nearby.
	if ( ! IsTopLevel())
		return( NULL );

	CWorldSearch Area( GetTopPoint(), Multi_GetMaxDist());
	for(;;)
	{
		CItemPtr pItem = Area.GetNextItem();
		if ( pItem == NULL )
			return( NULL );
		if ( ! Multi_IsPartOf( pItem ))
			continue;
		if ( pItem->IsType( type ))
			return( pItem );
	}
}

bool CItemMulti::OnTick()
{
	if ( IsType(IT_SHIP))
	{
		// Ships move on their tick.
		if ( Ship_OnMoveTick())
			return true;
	}

	return true;
}

void CItemMulti::OnMoveFrom()
{
	// Being removed from the top level.
	// Might just be moving.
	if ( m_pRegion == NULL )
		return;
	m_pRegion->UnRealizeRegion(false);
}

bool CItemMulti::MoveTo( CPointMap pt ) // Put item on the ground here.
{
	// Move this item to it's point in the world. (ground/top level)
	if ( ! CItem::MoveTo(pt))
		return false;

	// Multis need special region info to track when u are inside them.
	// Add new region info.
	Multi_Region_Realize();
	return( true );
}

CItemPtr CItemMulti::Multi_GetSign()
{
	// Get my sign or tiller link.
	CItemPtr pTiller = g_World.ItemFind(m_uidLink);
	if ( pTiller == NULL )
	{
		pTiller = Multi_FindItemType( IsType(IT_SHIP) ? IT_SHIP_TILLER : IT_SIGN_GUMP );
		if ( pTiller == NULL )
			return( this );
		m_uidLink = pTiller->GetUID();
	}
	return( pTiller );
}

bool CItemMulti::Multi_DeedConvert( CChar* pChar, CItemMemory* pMemory )
{
	// Turn a multi back into a deed.
	// If it has chests with stuff inside then refuse to do so ?
	// This item specifically will morph into a deed. (so keys will change!)

	CWorldSearch Area( GetTopPoint(), Multi_GetMaxDist());
	for(;;)
	{
		CItemPtr pItem = Area.GetNextItem();
		if ( pItem == NULL )
			break;
		if ( ! Multi_IsPartOf( pItem ))
			continue;
		CItemContainerPtr pCont = REF_CAST(CItemContainer,pItem);
		if ( pCont == NULL )
			continue;
		if ( pCont->IsEmpty())
			continue;
		// Can't re-deed this with the container full.
		if ( pChar )
		{
			pChar->WriteString( "Containers are not empty" );
		}
		return( false );
	}

	// Make a new deed and bounce if into their pack
	pChar->ItemBounce( CItem::CreateScript( pMemory->m_itDeed.m_Type, pChar ));
	// destroy the unwanted multi
	DeleteThis();

	return( false );
}

#ifdef COMMENT
void CItemMulti::Ship_Dock( CChar* pChar )
{
	// Attempt to dock this "ship". If it is a ship.
	// Must not have things on the deck or in the hold.
	ASSERT( pChar != NULL );

	CItemPtr pItemShip;
	if ( ! IsType(IT_SHIP))
	{

	}

	if ( ! IsType(IT_SHIP))

		pChar->GetPackSafe()->ContentAdd( this );
}
#endif

bool CItem::Ship_Plank( bool fOpen )
{
	// IT_SHIP_PLANK to IT_SHIP_SIDE and IT_SHIP_SIDE_LOCKED
	// This item is the ships plank.

	CItemDefPtr pItemDef = Item_GetDef();
	ITEMID_TYPE idState = (ITEMID_TYPE) RES_GET_INDEX( pItemDef->m_ttShipPlank.m_idState );
	if ( ! idState )
	{
		// Broken ?
		return( false );
	}
	if ( IsType(IT_SHIP_PLANK))
	{
		if ( fOpen )
			return( true );
	}
	else
	{
		DEBUG_CHECK( IsType(IT_SHIP_SIDE) || IsType(IT_SHIP_SIDE_LOCKED));
		if ( ! fOpen )
			return( true );
	}

	SetID( idState );
	Update();
	return( true );
}

void CItemMulti::Ship_Stop( void )
{
	// Make sure we have stopped.
	m_itShip.m_fSail = false;
	SetTimeout( -1 );
}

bool CItemMulti::Ship_SetMoveDir( DIR_TYPE dir )
{
	// Set the direction we will move next time we get a tick.
	int iSpeed = 1;
	if ( m_itShip.m_DirMove == dir && m_itShip.m_fSail )
	{
		if ( m_itShip.m_DirFace == m_itShip.m_DirMove &&
			m_itShip.m_fSail == 1 )
		{
			iSpeed = 2;
		}
		else return( false );
	}

	if ( ! IsAttr(ATTR_MAGIC ))	// make sound.
	{
		if ( ! Calc_GetRandVal(10))
		{
			Sound( Calc_GetRandVal(2) ? 0x12 : 0x13 );
		}
	}

	m_itShip.m_DirMove = dir;
	m_itShip.m_fSail = iSpeed;
	GetTopSector()->SetSectorWakeStatus( GetTopMap());	// may get here b4 my client does.
	SetTimeout(( m_itShip.m_fSail == 1 ) ? 1*TICKS_PER_SEC : (TICKS_PER_SEC/5));
	return( true );
}

#define MAX_MULTI_LIST_OBJS 128

int CItemMulti::Ship_ListObjs( CObjBasePtr* ppObjList )
{
	// List all the objects in the structure.
	// Move the ship and everything on the deck
	// If too much stuff. then some will fall overboard. hehe.

	if ( ! IsTopLevel())
		return 0;

	int iMaxDist = Multi_GetMaxDist();

	// always list myself first. All other items must see my new region !
	int iCount = 0;
	ppObjList[iCount++] = this;

	CWorldSearch AreaChar( GetTopPoint(), iMaxDist );
	while ( iCount < MAX_MULTI_LIST_OBJS )
	{
		CCharPtr pChar = AreaChar.GetNextChar();
		if ( pChar == NULL )
			break;
		if ( ! m_pRegion->IsPointInside( pChar->GetTopPoint()))
			continue;
		if ( pChar->IsClient())
		{
			pChar->GetClient()->addPause();	// get rid of flicker. for anyone even seeing this.
		}
		ppObjList[iCount++] = pChar;
	}

	CWorldSearch AreaItem( GetTopPoint(), iMaxDist );
	while ( iCount < MAX_MULTI_LIST_OBJS )
	{
		CItemPtr pItem = AreaItem.GetNextItem();
		if ( pItem == NULL )
			break;
		if ( pItem == this )	// already listed.
			continue;
		if ( ! Multi_IsPartOf( pItem ))
		{
			if ( ! m_pRegion->IsPointInside( pItem->GetTopPoint()))
				continue;
			if ( ! pItem->IsMovable())
				continue;
		}
		ppObjList[iCount++] = pItem;
	}
	return( iCount );
}

HRESULT CItemMulti::Ship_MoveDelta( CPointMapBase pdelta )
{
	// Move the ship one space in some direction.

	ASSERT( m_pRegion->GetRefCount());	// m_iLinkedSectors

	int znew = GetTopZ() + pdelta.m_z;
	if ( pdelta.m_z > 0 )
	{
		if ( znew >= (SPHEREMAP_SIZE_Z - PLAYER_HEIGHT )-1 )
			return( HRES_BAD_ARGUMENTS );
	}
	else if ( pdelta.m_z < 0 )
	{
		if ( znew <= (SPHEREMAP_SIZE_MIN_Z + 3 ))
			return( HRES_BAD_ARGUMENTS );
	}

	// Move the ship and everything on the deck
	CObjBasePtr ppObjs[MAX_MULTI_LIST_OBJS+1];
	int iCount = Ship_ListObjs( ppObjs );

	for ( int i=0; i <iCount; i++ )
	{
		CObjBasePtr pObj = ppObjs[i];
		ASSERT(pObj);
		CPointMap pt = pObj->GetTopPoint();
		pt += pdelta;
		if ( ! pt.IsValidPoint())  // boat goes out of bounds !
		{
			DEBUG_ERR(( "Ship uid=0%x out of bounds" LOG_CR, GetUID()));
			continue;
		}
		pObj->MoveTo( pt );
		if ( pObj->IsChar())
		{
			ASSERT( m_pRegion->GetRefCount());	// m_iLinkedSectors
			pObj->RemoveFromView(); // Get rid of the blink/walk
			pObj->Update();
		}
	}

	return( NO_ERROR );
}

bool CItemMulti::Ship_CanMoveTo( const CPointMap & pt ) const
{
	// Can we move to the new location ? all water type ?
	if ( IsAttr(ATTR_MAGIC ))
		return( true );

	// Run into other ships ? ignore my own region.
	CRegionPtr pRegionOther = pt.GetRegion( REGION_TYPE_MULTI );
	if ( pRegionOther == m_pRegion )
		pRegionOther = NULL;

	CMulMapBlockState block( CAN_C_SWIM );
	g_World.GetHeightPoint( pt, block, pRegionOther );

	if ( ! ( block.m_Bottom.m_BlockFlags & CAN_I_WATER ))
	{
		return( false );
	}

	return( true );
}

static const DIR_TYPE sm_Ship_FaceDir[] =
{
	DIR_N,
	DIR_E,
	DIR_S,
	DIR_W,
};

HRESULT CItemMulti::Ship_Face( DIR_TYPE dir )
{
	// Change the direction of the ship.

	ASSERT( IsTopLevel());
	if ( ! m_pRegion.IsValidRefObj())
	{
		DEBUG_CHECK(0);
		return HRES_INVALID_HANDLE;
	}

	int i=0;
	for (;; i++ )
	{
		if ( i >= COUNTOF(sm_Ship_FaceDir))
			return( HRES_BAD_ARGUMENTS );
		if ( dir == sm_Ship_FaceDir[i] )
			break;
	}

	int iFaceOffset = Ship_GetFaceOffset();
	ITEMID_TYPE idnew = (ITEMID_TYPE) ( GetID() - iFaceOffset + i );

	CItemDefPtr pMultiNew2 = g_Cfg.FindItemDef( idnew );
	CItemDefMultiPtr pMultiNew = REF_CAST(CItemDefMulti,pMultiNew2);
	if ( pMultiNew == NULL )
	{
		return HRES_INVALID_HANDLE;
	}

	CItemDefMultiPtr pMultiDef = Multi_GetDef();
	ASSERT( pMultiDef);

	int iTurn = dir - sm_Ship_FaceDir[ iFaceOffset ];

	// ?? Are there blocking items in the way of the turn ?
	// CRectMap

	// Reorient everything on the deck
	CObjBasePtr ppObjs[MAX_MULTI_LIST_OBJS+1];
	int iCount = Ship_ListObjs( ppObjs );

	for ( i=0; i<iCount; i++ )
	{
		CObjBasePtr pObj = ppObjs[i];
		CPointMap pt = pObj->GetTopPoint();

		if ( pObj->IsItem())
		{
			CItemPtr pItem = REF_CAST(CItem,pObj);
			ASSERT( pItem );
			if ( pItem == this )	// do this first.
			{
				m_pRegion->UnRealizeRegion(false);
				SetID(idnew);
				// Adjust the region to be the new shape/area.
				Multi_Region_Realize();
				continue;
			}

			// Is this a ship component ? transform it.
			int i = Multi_IsComponentOf( pItem, pMultiDef );
			if ( i>=0 )
			{
				pItem->SetDispID( pMultiNew->m_Components[i].GetDispID());
				pt = GetTopPoint();
				pt.m_x += pMultiNew->m_Components[i].m_dx;
				pt.m_y += pMultiNew->m_Components[i].m_dy;
				pt.m_z += pMultiNew->m_Components[i].m_dz;
				pItem->MoveTo( pt );
				continue;
			}
		}

		// -2,6 = left.
		// +2,-6 = right.
		// +-4 = flip around

		int iTmp;
		int xdiff = GetTopPoint().m_x - pt.m_x;
		int ydiff = GetTopPoint().m_y - pt.m_y;
		switch ( iTurn )
		{
		case 2: // right
		case (2-DIR_QTY):
			iTmp = xdiff;
			xdiff = ydiff;
			ydiff = -iTmp;
			break;
		case -2: // left.
		case (DIR_QTY-2):
			iTmp = xdiff;
			xdiff = -ydiff;
			ydiff = iTmp;
			break;
		default: // u turn.
			xdiff = -xdiff;
			ydiff = -ydiff;
			break;
		}
		pt.m_x = GetTopPoint().m_x + xdiff;
		pt.m_y = GetTopPoint().m_y + ydiff;
		pObj->MoveTo( pt );

		if ( pObj->IsChar())
		{
			// Turn creatures as well.
			CCharPtr pChar = REF_CAST(CChar,pObj);
			ASSERT(pChar);
			pChar->m_dirFace = GetDirTurn( pChar->m_dirFace, iTurn );
			pChar->RemoveFromView();
			pChar->Update();
		}
	}

	Update();
	m_itShip.m_DirFace = dir;
	return( NO_ERROR );
}

HRESULT CItemMulti::Ship_Move( DIR_TYPE dir )
{
	if ( dir >= DIR_QTY )
		return( HRES_BAD_ARGUMENTS );

	if ( ! m_pRegion.IsValidRefObj())
	{
		DEBUG_ERR(( "Ship bad region" LOG_CR ));
		return HRES_INVALID_HANDLE;
	}

	CPointMap pt = GetTopPoint();

	CPointMap ptDelta;
	ptDelta.ZeroPoint();
	ptDelta.Move( dir );

	CPointMap ptForePrv;
	ptForePrv = m_pRegion->GetRegionCorner(dir);

	CPointMap ptFore = ptForePrv;
	ptFore.Move( dir );
	ptFore.m_z = pt.m_z;

	CMulMap* pMap = pt.GetMulMap();

	if ( ! ptFore.IsValidPoint() ||
		( ptForePrv.m_x < pMap->m_iSizeXWrap && ptFore.m_x >= pMap->m_iSizeXWrap ))
	{
		// Circle the globe
		// Fall off edge of world ?
		CItemPtr pTiller = Multi_GetSign();
		ASSERT(pTiller);
		pTiller->Speak( "Turbulent waters Cap'n", 0, TALKMODE_SAY, FONT_NORMAL );

		// Compute the new safe place to go. (where we will not overlap)
		int iMaxDist = Multi_GetMaxDist();

		if ( ptFore.m_x <= 0 )
		{

		}
		else if ( ptFore.m_x >= pMap->m_iSizeXWrap )
		{

		}
		else if ( ptFore.m_y <= 0 )
		{

		}
		else if ( ptFore.m_y >= pMap->m_iSizeY )
		{

		}

		return HRES_INVALID_HANDLE;
	}

	// We should check all relevant corners.
	if ( ! Ship_CanMoveTo( ptFore ))
	{
	cantmove:
		CItemPtr pTiller = Multi_GetSign();
		ASSERT(pTiller);
		pTiller->Speak( "We've stopped Cap'n", 0, TALKMODE_SAY, FONT_NORMAL );
		return HRES_INVALID_HANDLE;
	}

	// left side
	CPointMap ptTmp;
	ptTmp = m_pRegion->GetRegionCorner(GetDirTurn(dir,-1));
	ptTmp.Move( dir );
	ptTmp.m_z = GetTopZ();
	if ( ! Ship_CanMoveTo( ptTmp ))
		goto cantmove;

	// right side.
	ptTmp = m_pRegion->GetRegionCorner(GetDirTurn(dir,+1));
	ptTmp.Move( dir );
	ptTmp.m_z = GetTopZ();
	if ( ! Ship_CanMoveTo( ptTmp ))
		goto cantmove;

	Ship_MoveDelta( ptDelta );

	// Move again
	GetTopSector()->SetSectorWakeStatus(GetTopMap());	// may get here b4 my client does.
	return NO_ERROR;
}

bool CItemMulti::Ship_OnMoveTick( void )
{
	// We just got a move tick.
	// RETURN: false = delete the boat.

	if ( ! m_itShip.m_fSail )	// decay the ship instead ???
		return( true );

	// Calculate the leading point.
	DIR_TYPE dir = (DIR_TYPE) m_itShip.m_DirMove;
	HRESULT hRes = Ship_Move( dir );
	if ( IS_ERROR(hRes))
	{
		Ship_Stop();
		return( true );
	}

	SetTimeout(( m_itShip.m_fSail == 1 ) ? 1*TICKS_PER_SEC : (TICKS_PER_SEC/2));
	return( true );
}

void CItemMulti::OnHearRegion( LPCTSTR pszCmd, CChar* pSrc )
{
	// IT_SHIP or IT_MULTI
	// Speaking in this ships region.
	// The ship or house hears a spoken command.
	// NOTE:
	//  Incomplete commands.
	//  "One (direction*)", " (Direction*), one" Moves ship one tile in desired direction and stops.
	//  "Slow (direction*)" Moves ship slowly in desired direction (see below for possible directions).
	// RETURN:
	//  true = command for the ship.

	CItemDefMultiPtr pMultiDef = Multi_GetDef();
	if ( pMultiDef == NULL )
		return;

	CSphereExpArgs exec( this, pSrc, pszCmd );
	for ( int i=0; i<pMultiDef->m_Speech.GetSize(); i++ )
	{
		TRIGRET_TYPE iRet = OnHearTrigger( pMultiDef->m_Speech[i], exec );
		if ( iRet != TRIGRET_ENDIF && iRet != TRIGRET_RET_FALSE )
			break;
	}
}

HRESULT CItemMulti::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CItem::s_PropGet(pszKey, vValRet, pSrc));
	}
	switch(iProp)
	{
	case P_Region:
		vValRet.SetRef(m_pRegion);
		break;
	case P_Ban:
		return HRES_INVALID_FUNCTION;
	case P_CoOwner:
		return HRES_INVALID_FUNCTION;
	case P_Friend:
		return HRES_INVALID_FUNCTION;
	case P_Tag:
		return HRES_INVALID_FUNCTION;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CItemMulti::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	s_FixExtendedProp( pszKey, "Region", vVal );

	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CItem::s_PropSet( pszKey, vVal ));
	}
	switch(iProp)
	{
	case P_Ban:
		m_Bans.AttachUID( vVal.GetUID());
		break;
	case P_CoOwner:
		m_CoOwners.AttachUID( vVal.GetUID());
		break;
	case P_Friend:
		m_Friends.AttachUID( vVal.GetUID());
		break;
	case P_Region:
		// PropSet will not handle extensions !!
		if ( ! IsTopLevel())
		{
			MoveTo( GetTopPoint()); // Put item on the ground here.
		}
		if ( ! m_pRegion )
			return HRES_INVALID_HANDLE;
		{
			int i = vVal.MakeArraySize();
			if ( i < 2 )
				return HRES_BAD_ARG_QTY;
			CString sKey = vVal.GetArrayStr(0);
			vVal.RemoveArrayElement(0);
			return( m_pRegion->s_PropSet( sKey, vVal ));
		}

	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}

HRESULT CItemMulti::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp < 0 )
	{
		return( CItem::s_Method( pszKey, vArgs, vValRet, pSrc ));
	}

	switch (iProp)
	{
	case M_Bans:
		return m_Bans.s_MethodObjs( vArgs, vValRet, pSrc );
	case M_CoOwners:
		return m_CoOwners.s_MethodObjs( vArgs, vValRet, pSrc );
	case M_Friends:
		return m_Friends.s_MethodObjs( vArgs, vValRet, pSrc );
	case M_IsBanned:
		vValRet.SetBool( m_Bans.IsUIDIn( vArgs.GetUID()));
		return 0;
	case M_IsCoowner:
		vValRet.SetBool( m_CoOwners.IsUIDIn( vArgs.GetUID()));
		return 0;
	case M_IsFriend:
		vValRet.SetBool( m_Friends.IsUIDIn( vArgs.GetUID()));
		return 0;
	}

	if ( ! pSrc )
		return( HRES_PRIVILEGE_NOT_HELD );
	if ( IsAttr(ATTR_MOVE_NEVER) || ! IsTopLevel())
		return HRES_INVALID_HANDLE;

	CCharPtr pCharSrc = GET_ATTACHED_CCHAR(pSrc);

	// Only key holders can command the ship ???
	// if ( pCharSrc && pCharSrc->ContentFindKeyFor( pItem ))

	// Find the tiller man object.
	CItemPtr pTiller = Multi_GetSign();
	ASSERT( pTiller );

	// Get current facing dir.
	DIR_TYPE DirFace = sm_Ship_FaceDir[ Ship_GetFaceOffset() ];
	int DirMoveChange;
	LPCTSTR pszSpeak = NULL;

	switch ( iProp )
	{
	case M_Comp:
		// itterate through the components.
		vValRet.SetRef( Multi_FindItemComponent( vArgs.GetInt()));
		break;
	case M_ShipStop:
		// "Furl sail"
		// "Stop" Stops current ship movement.
		if ( ! m_itShip.m_fSail )
			return( HRES_INVALID_HANDLE );
		Ship_Stop();
		break;
	case M_ShipFace:
		// Face this direction. do not change the direction of movement.
		if ( vArgs.IsEmpty())
			return( HRES_BAD_ARG_QTY );
		return Ship_Face( CGRect::GetDirStr( vArgs.GetPSTR()));

	case M_ShipMove:
		// Move one space in this direction.
		// Does NOT protect against exploits !
		if ( vArgs.IsEmpty())
			return( HRES_BAD_ARG_QTY );
		m_itShip.m_DirMove = CGRect::GetDirStr( vArgs.GetPSTR());
		return Ship_Move( (DIR_TYPE) m_itShip.m_DirMove );

	case M_ShipGate:
		// Move the whole ship and contents to another place.
		if ( vArgs.IsEmpty())
			return( HRES_BAD_ARG_QTY );
		{
			CPointMap ptdelta = g_Cfg.GetRegionPoint( vArgs.GetPSTR());
			if ( ! ptdelta.IsValidPoint())
				return( HRES_BAD_ARGUMENTS );
			ptdelta -= GetTopPoint();
			return Ship_MoveDelta( ptdelta );
		}
		break;
	case M_ShipTurnLeft:
		// "Port",
		// "Turn Left",
		DirMoveChange = -2;
	doturn:
		if ( m_itShip.m_fAnchored )
		{
		anchored:
			pszSpeak = "The anchor is down <SEX Sir/Mam>!";
			break;
		}
		m_itShip.m_DirMove = GetDirTurn( DirFace, DirMoveChange );
		Ship_Face( (DIR_TYPE) m_itShip.m_DirMove );
		break;
	case M_ShipTurnRight:
		// "Turn Right",
		// "Starboard",	// Turn Right
		DirMoveChange = 2;
		goto doturn;
	case M_ShipDriftLeft:
		// "Left",
		// "Drift Left",
		DirMoveChange = -2;
	dodirmovechange:
		if ( m_itShip.m_fAnchored )
			goto anchored;
		if ( ! Ship_SetMoveDir( GetDirTurn( DirFace, DirMoveChange )))
			return( HRES_BAD_ARGUMENTS );
		break;
	case M_ShipDriftRight:
		// "Right",
		// "Drift Right",
		DirMoveChange = 2;
		goto dodirmovechange;
	case M_ShipBack:
		// "Back",			// Move ship backwards
		// "Backward",		// Move ship backwards
		// "Backwards",	// Move ship backwards
		DirMoveChange = 4;
		goto dodirmovechange;
	case M_ShipFore:
		// "Forward"
		// "Foreward",		// Moves ship forward.
		// "Unfurl sail",	// Moves ship forward.
		DirMoveChange = 0;
		goto dodirmovechange;
	case M_ShipForeLeft: // "Forward left",
		DirMoveChange = -1;
		goto dodirmovechange;
	case M_ShipForeRight: // "forward right",
		DirMoveChange = 1;
		goto dodirmovechange;
	case M_ShipBackLeft:
		// "backward left",
		// "back left",
		DirMoveChange = -3;
		goto dodirmovechange;
	case M_ShipBackRight:
		// "backward right",
		// "back right",
		DirMoveChange = 3;
		goto dodirmovechange;
	case M_ShipAnchorRaise: // "Raise Anchor",
		if ( ! m_itShip.m_fAnchored )
		{
			pszSpeak = "The anchor is already up <SEX Sir/Mam>";
			break;
		}
		m_itShip.m_fAnchored = false;
		break;
	case M_ShipAnchorDrop: // "Drop Anchor",
		if ( m_itShip.m_fAnchored )
		{
			pszSpeak = "The anchor is already down <SEX Sir/Mam>";
			break;
		}
		m_itShip.m_fAnchored = true;
		Ship_Stop();
		break;
	case M_ShipTurn:
		//	"Turn around",	// Turns ship around and proceeds.
		// "Come about",	// Turns ship around and proceeds.
		DirMoveChange = 4;
		goto doturn;
	case M_ShipUp: // "Up"
		{
			if ( ! IsAttr(ATTR_MAGIC ))
				return( HRES_INVALID_HANDLE );

			CPointMap pt;
			pt.m_z = PLAYER_HEIGHT;
			HRESULT hRes = Ship_MoveDelta( pt );
			if ( IS_ERROR(hRes))
			{
				pszSpeak = "Can't do that <SEX Sir/Mam>";
			}
			else
			{
				pszSpeak = "As you command <SEX Sir/Mam>";
			}
		}
		break;
	case M_ShipDown: // "Down"
		{
			if ( ! IsAttr(ATTR_MAGIC ))
				return( HRES_INVALID_HANDLE );
			CPointMap pt;
			pt.m_z = -PLAYER_HEIGHT;
			HRESULT hRes = Ship_MoveDelta( pt );
			if ( IS_ERROR(hRes))
			{
				pszSpeak = "Can't do that <SEX Sir/Mam>";
			}
			else
			{
				pszSpeak = "As you command <SEX Sir/Mam>";
			}
		}
		break;
	case M_ShipLand: // "Land"
		{
			if ( ! IsAttr(ATTR_MAGIC ))
				return( HRES_INVALID_HANDLE );
			PNT_Z_TYPE zold = GetTopZ();
			CPointMap pt = GetTopPoint();
			pt.m_z = zold;
			SetTopZ( -SPHEREMAP_SIZE_Z );	// bottom of the world where i won't get in the way.

			CMulMapBlockState block( CAN_C_SWIM );
			g_World.GetHeightPoint( pt, block );

			PNT_Z_TYPE z = block.GetResultZ();
			SetTopZ( zold );	// restore z for now.
			pt.InitPoint();
			pt.m_z = z - zold;
			if ( pt.m_z )
			{
				Ship_MoveDelta( pt );
				pszSpeak = "As you command <SEX Sir/Mam>";
			}
			else
			{
				pszSpeak = "We have landed <SEX Sir/Mam>";
			}
		}
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	if ( pCharSrc )
	{
		if ( pszSpeak == NULL )
		{
			static LPCTSTR const sm_pszAye[] =
			{
				"Aye",
				"Aye Cap'n",
				"Aye <SEX Sir/Mam>",
			};
			pszSpeak = sm_pszAye[ Calc_GetRandVal( COUNTOF( sm_pszAye )) ];
		}

		TCHAR szText[ MAX_TALK_BUFFER ];
		strcpy( szText, pszSpeak );

		CSphereExpContext exec( pCharSrc, &g_Serv );
		exec.s_ParseEscapes( szText, 0 );

		pTiller->Speak( szText, 0, TALKMODE_SAY, FONT_NORMAL );
	}

	return( NO_ERROR );
}

void CItemMulti::s_Serialize( CGFile& a )
{
	CItem::s_Serialize(a);
}

void CItemMulti::s_WriteProps( CScript& s )
{
	CItem::s_WriteProps(s);

	m_Bans.s_WriteObjs( s, "BAN" );
	m_CoOwners.s_WriteObjs( s, "COOWNER" );
	m_Friends.s_WriteObjs( s, "FRIEND" );

	if (m_pRegion)
	{
		m_pRegion->s_WriteBody( s, "REGION." );
	}
}

void CItemMulti::DupeCopy( const CItem* pItem )
{
	CItem::DupeCopy(pItem);

	const CItemMulti* pMultiItem = PTR_CAST(const CItemMulti,pItem);
	DEBUG_CHECK(pMultiItem);
	if ( pMultiItem == NULL )
		return;

	m_Bans.CopyArray( pMultiItem->m_Bans );
	m_CoOwners.CopyArray( pMultiItem->m_CoOwners );
	m_Friends.CopyArray( pMultiItem->m_Friends );

	// Region attributes ?
}


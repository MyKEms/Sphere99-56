//
// CItemDef.CPP
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
// define the base types of an item (rather than the instance)
//
#include "stdafx.h"	// predef header.

/////////////////////////////////////////////////////////////////
// -CItemDef

const CScriptProp CItemDef::sm_Props[CItemDef::P_QTY+1] =
{
#define CITEMDEFPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "citemdefprops.tbl"
#undef CITEMDEFPROP
	NULL,
};

const CScriptProp CItemDef::sm_Triggers[CItemDef::T_QTY+1] =	// static
{
#define CITEMEVENT(a,b,c) {"@" #a,b,c},
	CITEMEVENT(AAAUNUSED,CSCRIPTPROP_UNUSED,NULL)	// reserved for XTRIG_UNKNOWN
#include "citemevents.tbl"
#undef CITEMEVENT
	NULL,
};

CSCRIPT_CLASS_IMP1(ItemDef,CItemDef::sm_Props,NULL,CItemDef::sm_Triggers,ObjBaseDef);

CItemDef::CItemDef( ITEMID_TYPE id ) :
	CObjBaseDef( CSphereUID( RES_ItemDef, id ))
{
	ASSERT( id>0 );

	m_weight = 0;
	m_type = IT_NORMAL;
	m_layer = LAYER_NONE;

	// Just applies to equippable weapons/armor.
	m_ttNormal.m_tData1 = 0;
	m_ttNormal.m_tData2 = 0;
	m_ttNormal.m_tData3 = 0;
	m_ttNormal.m_tData4 = 0;

	if ( ! IsValidDispID( id ))
	{
		// There should be an ID= in the scripts later.
		m_wDispIndex = ITEMID_GOLD_C1; // until i hear otherwise from the script file.
		return;
	}

	// Set the artwork/display id.
	m_wDispIndex = id;

	// I have it indexed but it needs to be loaded.
	// read it in from the script and *.mul files.

	CUOItemTypeRec tiledata;
	memset( &tiledata, 0, sizeof(tiledata));
	if ( id < ITEMID_MULTI )
	{
		if ( ! CItemDef::GetItemData( id, &tiledata ))	// some valid items don't show up here !
		{
			// return( NULL );
		}
	}
	else
	{
		tiledata.m_weight = 0xFF;
	}

	m_dwFlags = tiledata.m_flags;

	// ??? The script should be doing ALL this for us !
	if ( g_Serv.m_iModeCode != SERVMODE_Test5 )
	{
		m_type = GetTypeBase( id, tiledata );
	}

	// Stuff read from .mul file.
	// Some items (like hair) have no names !
	// Get rid of the strange leading spaces in some of the names.
	TCHAR szName[ sizeof(tiledata.m_name)+1 ];
	int j=0;
	for ( int i=0; i<sizeof(tiledata.m_name) && tiledata.m_name[i]; i++ )
	{
		if ( j==0 && ISWHITESPACE(tiledata.m_name[i]))
			continue;
		szName[j++] = tiledata.m_name[i];
	}

	szName[j] = '\0';
	m_sName = szName;	// default type name.

	// Do some special processing for certain items.

	if ( IsType(IT_CHAIR))
	{
		SetHeight( 0 ); // have no effective height if they don't block.
	}
	else
	{
		SetHeight( GetItemHeightFlags( tiledata, m_CanFlags ));
	}

	if ( tiledata.m_flags& UFLAG3_LIGHT )	// this may actually be a moon gate or fire ?
	{
		m_CanFlags |= CAN_I_LIGHT;	// normally of type IT_LIGHT_LIT;
	}
	if (( tiledata.m_flags & UFLAG2_STACKABLE ) || m_type == IT_REAGENT ||
		id == ITEMID_EMPTY_BOTTLE )
	{
		m_CanFlags |= CAN_I_PILE;
	}

	if ( tiledata.m_weight == 0xFF ||	// not movable.
		( tiledata.m_flags & UFLAG1_WATER ))
	{
		// water can't be picked up.
		m_weight = USHRT_MAX;
	}
	else
	{
		m_weight = tiledata.m_weight* WEIGHT_UNITS;
	}

	if ( tiledata.m_flags & ( UFLAG1_EQUIP | UFLAG3_EQUIP2 ))
	{
		m_layer = tiledata.m_layer;
		if ( m_layer && ! IsMovableType())
		{
			// How am i supposed to equip something i can't pick up ?
			m_weight = WEIGHT_UNITS;
		}
	}
}

CItemDef::~CItemDef()
{
	// These don't really get destroyed til the server is shut down but keep this around anyhow.
	DEBUG_CHECK( GetDispID());
}

void CItemDef::SetTypeName( LPCTSTR pszName )
{
	ASSERT(pszName);
	if ( ! strcmp( pszName, GetTypeName()))
		return;
	m_dwFlags |= UFLAG2_ZERO1;	// we override the name
	CObjBaseDef::SetTypeName( pszName );
}

LPCTSTR CItemDef::GetArticleAndSpace() const
{
	if ( m_dwFlags & UFLAG2_ZERO1 )	// Name has been changed from TILEDATA.MUL
	{
		return( Str_GetArticleAndSpace( GetTypeName()));
	}
	if ( m_dwFlags & UFLAG2_AN )
	{
		return( "an " );
	}
	if ( m_dwFlags & UFLAG2_A )
	{
		return( "a " );
	}
	return( "" );
}

void CItemDef::CopyBasic( const CItemDef* pBase )
{
	m_weight = pBase->m_weight;
	m_flip_id.CopyArray( pBase->m_flip_id );
	m_type = pBase->m_type;
	m_layer = pBase->m_layer;

	m_ttNormal.m_tData1 = pBase->m_ttNormal.m_tData1;
	m_ttNormal.m_tData2 = pBase->m_ttNormal.m_tData2;
	m_ttNormal.m_tData3 = pBase->m_ttNormal.m_tData3;
	m_ttNormal.m_tData4 = pBase->m_ttNormal.m_tData4;

	CObjBaseDef::CopyBasic( pBase );	// This will overwrite the CResourceLink!!
}

void CItemDef::CopyTransfer( CItemDef* pBase )
{
	CopyBasic( pBase );

	m_values = pBase->m_values;
	m_SkillMake.CopyArray( pBase->m_SkillMake );

	CObjBaseDef::CopyTransfer( pBase );	// This will overwrite the CResourceLink!!
}

CGString CItemDef::GetName() const
{
	// Get rid of the strange %s type stuff for pluralize rules of names.
	return( GetNamePluralize( GetTypeName(), false ));
}

TCHAR* CItemDef::GetNamePluralize( LPCTSTR pszNameBase, bool fPluralize )	// static
{
	TCHAR* pszName = Str_GetTemp();
	int j=0;
	bool fInside = false;
	bool fPlural;
	for ( int i=0; pszNameBase[i]; i++ )
	{
		if ( pszNameBase[i] == '%' )
		{
			fInside = ! fInside;
			fPlural = true;
			continue;
		}
		if ( fInside )
		{
			if ( pszNameBase[i] == '/' )
			{
				fPlural = false;
				continue;
			}
			if ( fPluralize )
			{
				if ( ! fPlural )
					continue;
			}
			else
			{
				if ( fPlural )
					continue;
			}
		}
		pszName[j++] = pszNameBase[i];
	}
	pszName[j] = '\0';
	return( pszName );
}

bool CItemDef::IsTypeMulti( IT_TYPE type )  // static
{
	switch( type )
	{
	case IT_SHIP:
	case IT_MULTI:
		return true;
	}
	return false;
}

bool CItemDef::IsTypeArmor( IT_TYPE type )  // static
{
	switch( type )
	{
	case IT_CLOTHING:
	case IT_ARMOR:
	case IT_ARMOR_LEATHER:
	case IT_SHIELD:
		return( true );
	}
	return( false );
}
bool CItemDef::IsTypeWeapon( IT_TYPE type )  // static
{
	// These must all be equippable!
	// NOTE: a wand can be a weapon.
	switch( type )
	{
	case IT_WEAPON_MACE_STAFF:
	case IT_WEAPON_MACE_CROOK:
	case IT_WEAPON_MACE_PICK:
	case IT_WEAPON_AXE:
	case IT_WEAPON_XBOW:
	// case IT_SHOVEL:		// not equippable unfortunately.
		return( true );
	}
	return( type >= IT_WEAPON_MACE_SMITH && type <= IT_WAND );
}

GUMP_TYPE CItemDef::IsTypeContainer() const
{
	// IT_CONTAINER
	// return the container gump id.

	switch ( m_type )
	{
	case IT_CONTAINER:
	case IT_SIGN_GUMP:
	case IT_SHIP_HOLD:
	case IT_BBOARD:
	case IT_CORPSE:
	case IT_TRASH_CAN:
	case IT_GAME_BOARD:
	case IT_EQ_BANK_BOX:
	case IT_EQ_VENDOR_BOX:
	case IT_KEYRING:
		return(	m_ttContainer.m_gumpid );
	default:
		return( GUMP_NONE );
	}
}

bool CItemDef::IsTypeEquippable() const
{
	// Equippable on (possibly) visible layers.

	switch ( m_type )
	{
	case IT_SPELLBOOK:
	case IT_LIGHT_LIT:
	case IT_LIGHT_OUT:	// Torches and lanterns.
	case IT_FISH_POLE:
	case IT_HAIR:
	case IT_BEARD:
	case IT_JEWELRY:
	case IT_EQ_HORSE:
		// ussually these are on a specific layer but don't worry about that here.
		return( true );

	case IT_EQ_BANK_BOX:
	case IT_EQ_VENDOR_BOX:
	case IT_EQ_CLIENT_LINGER:
	case IT_EQ_MURDER_COUNT:
	case IT_EQ_STUCK:
	case IT_EQ_TRADE_WINDOW:
	case IT_EQ_MEMORY_OBJ:
	case IT_EQ_SCRIPT_BOOK:
	case IT_EQ_SCRIPT:
	case IT_EQ_MESSAGE:
	case IT_EQ_DIALOG:
		// Even not normally visible things.
		if ( IsVisibleLayer( (LAYER_TYPE) m_layer ))
			return( false );
		return( true );
	}

	if ( IsTypeArmor( m_type ))
		return( true );
	if ( IsTypeWeapon( m_type ))
		return( true );

	return( false );
}

bool CItemDef::IsID_Multi( ITEMID_TYPE id ) // static
{
	// NOTE: Ships are also multi's
	return( id >= ITEMID_MULTI && id < ITEMID_MULTI_MAX );
}

static bool IsID_GamePiece( ITEMID_TYPE id ) // static
{
	return( id >= ITEMID_GAME1_CHECKER && id <= ITEMID_GAME_HI );
}

static bool IsID_Track( ITEMID_TYPE id ) // static
{
	return( id >= ITEMID_TRACK_BEGIN && id <= ITEMID_TRACK_END );
}

static bool IsID_WaterFish( ITEMID_TYPE id ) // static
{
	// IT_WATER
	// Assume this means water we can fish in.
	// Not water we can wash in.
	if ( id >= 0x1796 && id <= 0x17b2 )
		return( true );
	if ( id == 0x1559 )
		return( true );
	return( false );
}

static bool IsID_WaterWash( ITEMID_TYPE id ) // static
{
	// IT_WATER_WASH
	if ( id >= ITEMID_WATER_TROUGH_1 && id <= ITEMID_WATER_TROUGH_2	)
		return( true );
	return( IsID_WaterFish( id ));
}

bool CItemDef::GetItemData( ITEMID_TYPE id, CUOItemTypeRec* pData ) // static
{
	// Read from g_MulInstall.m_fTileData
	// Get an Item tiledata def data.
	// Invalid object id ?
	// NOTE: This data should already be read into the m_ItemBase table ???

	if ( ! IsValidDispID(id))
		return( false );

	try
	{
		CMulItemInfo info( id );
		*pData = *( STATIC_CAST(CUOItemTypeRec,&info ));
	}
	SPHERE_LOG_TRY_CATCH1( "GetItemData %d", id )

#if 1
	// Unused tiledata I guess. Don't create it.
	if ( ! pData->m_flags &&
		! pData->m_weight &&
		! pData->m_layer &&
		! pData->m_dwUnk6 &&
		! pData->m_dwAnim &&
		! pData->m_wUnk14 &&
		! pData->m_height &&
		! pData->m_name[0]
		)
	{
		// What are the exceptions to the rule ?
		if ( id == ITEMID_BBOARD_MSG ) // special
			return( true );
		if ( IsID_GamePiece( id ))
			return( true );
		if ( IsID_Track(id))	// preserve these
			return( true );
		return( false );
	}
#endif

	return( true );
}

inline PNT_Z_TYPE CItemDef::GetItemHeightFlags( const CUOItemTypeRec& tiledata, CAN_TYPE& wBlockThis ) // static
{
	// Chairs are marked as blocking for some reason ?
	// SImilar to GetBlockFromTileData();
	if ( tiledata.m_flags & UFLAG4_DOOR ) // door
	{
		wBlockThis = CAN_I_DOOR;
		return( tiledata.m_height );
	}
	if ( tiledata.m_flags & UFLAG1_BLOCK )
	{
		if ( tiledata.m_flags & UFLAG1_WATER )	// water	(IsID_WaterFish() ?
		{
			wBlockThis = CAN_I_WATER;
			return( tiledata.m_height );
		}
		wBlockThis = CAN_I_BLOCK;
	}
	else
	{
		wBlockThis = 0;
		if ( ! ( tiledata.m_flags & (UFLAG2_PLATFORM|UFLAG4_ROOF)))
			return 0;	// have no effective height if it doesn't block.
	}
	if ( tiledata.m_flags & (UFLAG2_PLATFORM|UFLAG4_ROOF))
	{
		wBlockThis |= CAN_I_PLATFORM;
	}
	if ( tiledata.m_flags & UFLAG2_CLIMBABLE )
	{
		// actual standing height is height/2
		wBlockThis |= CAN_I_CLIMB;
	}
	return( tiledata.m_height );
}

PNT_Z_TYPE CItemDef::GetItemHeight( ITEMID_TYPE id, CAN_TYPE& wBlockThis ) // static
{
	// Get just the height and the blocking flags for the item by id.
	// used for walk block checking.

	CSphereUID rid = CSphereUID( RES_ItemDef, id );
	int index = g_Cfg.m_ResHash.FindKey(rid);
	if ( index >= 0 )	// already loaded ?
	{
		CResourceDefPtr pResDef = g_Cfg.m_ResHash.GetAt( index );
		ASSERT(pResDef);
		CItemDefPtr pBase = REF_CAST(CItemDef,pResDef);
		if ( pBase )
		{
			wBlockThis = pBase->GetCanFlags();
			return( pBase->GetHeight());
		}

		// CItemDefDupe items are not always the same height !!!
	}

	// Not already loaded.

	CUOItemTypeRec tiledata;
	if ( ! GetItemData( id, &tiledata ))
	{
		wBlockThis = 0xFF;
		return( SPHEREMAP_SIZE_Z );
	}
	return( GetItemHeightFlags( tiledata, wBlockThis ));
}

IT_TYPE CItemDef::GetTypeBase( ITEMID_TYPE id, const CUOItemTypeRec &tiledata ) // static
{
	if ( id >= ITEMID_MULTI && id <= ITEMID_SHIP6_W )
		return IT_SHIP;
	if ( IsID_Multi( id ))
		return IT_MULTI;

	if (( tiledata.m_flags & UFLAG1_BLOCK ) && (tiledata.m_flags & UFLAG1_WATER))
		return IT_WATER;
	if ( IsID_WaterFish( id )) // UFLAG1_WATER
		return IT_WATER;

	if ( tiledata.m_flags & UFLAG3_CONTAINER )
		return IT_CONTAINER;

	if ( IsID_WaterWash( id ))
		return IT_WATER_WASH;
	else if ( IsID_Track( id ))
		return IT_FIGURINE;
	else if ( IsID_GamePiece( id ))
		return IT_GAME_PIECE;

	// Get rid of the stuff below here !

	if (( tiledata.m_flags & UFLAG1_DAMAGE ) && ! ( tiledata.m_flags & UFLAG1_BLOCK ))
		return IT_TRAP_ACTIVE;

	if ( tiledata.m_flags & UFLAG3_LIGHT )	// this may actually be a moon gate or fire ?
	{
		return IT_LIGHT_LIT;
	}

	return IT_NORMAL;	// Get from script i guess.
}

ITEMID_TYPE CItemDef::GetNextFlipID( ITEMID_TYPE id ) const
{
	if ( m_flip_id.GetSize())
	{
		ITEMID_TYPE idprev = GetDispID();
		for ( int i=0; true; i++ )
		{
			if ( i>=m_flip_id.GetSize())
			{
				break;
			}
			ITEMID_TYPE idnext = m_flip_id[i];
			if ( idprev == id )
				return( idnext );
			idprev = idnext;
		}
	}
	return( GetDispID());
}

bool CItemDef::IsSameDispID( ITEMID_TYPE id ) const
{
	// Does this item look like the item we want ?
	// Take into account flipped items.

	if ( ! IsValidDispID( id ))	// this should really not be here but handle it anyhow.
	{
		return( GetID() == id );
	}

	if ( id == GetDispID())
		return( true );

	for ( int i=0; i<m_flip_id.GetSize(); i ++ )
	{
		if ( m_flip_id[i] == id )
			return( true );
	}
	return( false );
}

void CItemDef::Restock()
{
	// Re-evaluate the base random value rate some time in the future.
	if ( m_values.m_iLo < 0 || m_values.m_iHi < 0 )
	{
		m_values.Init();
	}
}

int CItemDef::CalculateMakeValue( int iQualityPercent ) const
{
	// Calculate the value in gold for this item based on its components.
	// NOTE: Watch out for circular RESOURCES= list in the scripts.
	// ARGS:
	//   iQualityPercent = 0-100

	static int sm_iReentrantCount = 0;
	sm_iReentrantCount++;
	if ( sm_iReentrantCount > 32 )
	{
		DEBUG_ERR(( "CalculateMakeValue reentrant item=%s" LOG_CR, (LPCTSTR) GetResourceName()));
		return( 0 );
	}

	int lValue = 0;

	// add value based on the base resources making this up.
	int i;
	for ( i=0; i<m_BaseResources.GetSize(); i++ )
	{
		CSphereUID rid = m_BaseResources[i].GetResourceID();
		if ( rid.GetResType() == RES_TypeDef )
		{
			// defines that it is make of a "Type" of item. (ie. any sort of wood)
			// NOTE: How do we assign value here ???
			lValue += 1;
			continue;
		}
		if ( rid.GetResType() == RES_ItemDef )
		{
			CItemDefPtr pItemDef = g_Cfg.FindItemDef( (ITEMID_TYPE) rid.GetResIndex());
			if ( pItemDef == NULL ) // Err
			{
				DEBUG_CHECK(0);
				continue;
			}
			lValue += pItemDef->GetMakeValue( iQualityPercent )* m_BaseResources[i].GetResQty();
			continue;
		}
	}

	// add some value based on the skill required to create it.
	for ( i=0; i<m_SkillMake.GetSize(); i++ )
	{
		CSphereUID rid = m_SkillMake[i].GetResourceID();
		if ( rid.GetResType() != RES_Skill )
			continue;
		CSkillDefPtr pSkillDef = g_Cfg.GetSkillDef( (SKILL_TYPE) rid.GetResIndex());
		if ( pSkillDef == NULL )
		{
			DEBUG_CHECK(0);
			continue;
		}

		// this is the normal skill required.
		// if iQuality is much less than iSkillNeed then something is wrong.
		int iSkillNeed = m_SkillMake[i].GetResQty();
		if ( iQualityPercent < iSkillNeed )
			iQualityPercent = iSkillNeed;

		lValue += pSkillDef->m_Values.GetLinear( iQualityPercent );
	}

	sm_iReentrantCount--;
	return( lValue );
}

long CItemDef::GetMakeValue( int iQualityPercent )
{
	// Set the items value based on the resources and skill used to make it.
	// ARGS:
	// iQualityPercent = 0-100

	int iMin = m_values.GetMin();
	int iMax = m_values.GetMax();

	if ( m_values.IsInvalid())
	{
		iMin = CalculateMakeValue(0);		// low quality specimen
		iMax = CalculateMakeValue(100); 		// Top quality specimen

		// negative means they will float.
		m_values.SetRange( -iMin, -iMax );
	}
	else
	{
		// stored as negative to show that it is floating.
		if ( iMin < 0 )
			iMin = -iMin;
		if ( iMax < 0 )
			iMax = -iMax;
	}

	CValueRangeInt values(iMin,iMax);
	return( values.GetLinear( iQualityPercent*10 ));
}

HRESULT CItemDef::SetBaseType( IT_TYPE type, CItemDefPtr* ppItemDef )
{
	// Upgrade the CItemDef::pBase to the type specific class.

	if ( type == IT_CONTAINER_LOCKED )
	{
		// At this level it just means to add a key for it.
		m_type = IT_CONTAINER;
	}
	else
	{
		m_type = type;
	}

	CItemDefPtr pBase = this;
	if ( IsTypeMulti(type))
	{
		if ( REF_CAST(CItemDefMulti,pBase) == NULL )
		{
			pBase = new CItemDefMulti( pBase );
		}
	}
	else if ( IsTypeArmor(type) || IsTypeWeapon(type))
	{
		if ( REF_CAST(CItemDefWeapon,pBase) == NULL )
		{
			pBase = new CItemDefWeapon( pBase );
		}
	}

	if ( ppItemDef )
	{
		*ppItemDef = pBase;
	}
	return NO_ERROR;
}

HRESULT CItemDef::SetBaseID( const char* pszArg, CItemDefPtr* ppItemDef )
{
	// NOTE: This could cvhange the class we use !
	//  could change it to a weapon./ multi 
	//

	// DEBUG_CHECK( g_Serv.IsLoading()); // or resync ?
	if ( GetID() < ITEMID_MULTI )
	{
		DEBUG_ERR(( "Setting new id '%s' for base type %s not allowed" LOG_CR, pszArg, (LPCTSTR) GetResourceName()));
		return( HRES_BAD_ARGUMENTS );
	}

	ITEMID_TYPE id = (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, pszArg );
	if ( ! IsValidDispID(id))
	{
		DEBUG_ERR(( "Setting invalid id=%s for base type %s" LOG_CR, pszArg, (LPCTSTR) GetResourceName()));
		return( HRES_BAD_ARGUMENTS );
	}

	CItemDefPtr pItemDef = g_Cfg.FindItemDef( id );	// make sure the base is loaded.
	if ( ! pItemDef )
	{
		DEBUG_ERR(( "Setting unknown base id=0%x for %s" LOG_CR, id, (LPCTSTR) GetResourceName()));
		return( HRES_BAD_ARGUMENTS );
	}

	CopyBasic( pItemDef );
	m_wDispIndex = id;	// Might not be the default of a DUPEITEM

	return SetBaseType( pItemDef->m_type, ppItemDef );
}

HRESULT CItemDef::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CObjBaseDef::s_PropGet( pszKey, vValRet, pSrc ));
	}
	switch (iProp)
	{
	case P_Id:
		vValRet.SetDWORD( GetDispID());
		break;
	case P_DispId:
		vValRet = g_Cfg.ResourceGetName( CSphereUID( RES_ItemDef, GetDispID()));
		break;
	case P_DupeList:
		{
			TCHAR szTemp[ CSTRING_MAX_LEN ];
			int iLen = 0;
			*szTemp = '\0';
			for ( int i=0; i<m_flip_id.GetSize(); i++ )
			{
				if ( i ) iLen += strcpylen( szTemp+iLen, "," );
				iLen += sprintf( szTemp+iLen, "0%x", m_flip_id[i] );
				ASSERT(iLen<CSTRING_MAX_LEN);
			}
			vValRet = szTemp;
		}
		break;
	case P_Dye:
		vValRet.SetBool( Can(CAN_I_DYE));
		break;
	case P_Flip:
		vValRet.SetBool( Can(CAN_I_FLIP));
		break;
	case P_Layer:
		vValRet.SetInt( m_layer );
		break;
	case P_Replicate:
		vValRet.SetBool( Can(CAN_I_REPLICATE));
		break;
	case P_Repair:
		vValRet.SetBool( Can(CAN_I_REPAIR));
		break;
	case P_ReqStr:
		if ( ! IsTypeEquippable())
			return( HRES_INVALID_HANDLE );
		vValRet.SetInt( m_ttEquippable.m_StrReq );
		break;

	case P_SkillMake:
		m_SkillMake.v_GetKeys( vValRet );
		break;
	case P_ResMake:	// same as P_ResourceNames
		// Print the resources need to make in nice format.
		m_BaseResources.v_GetNames( vValRet );
		break;
	case P_TData1:
		vValRet.SetDWORD( m_ttNormal.m_tData1 );
		break;
	case P_TData2:
		vValRet.SetDWORD( m_ttNormal.m_tData2 );
		break;
	case P_TData3:
		vValRet.SetDWORD( m_ttNormal.m_tData3 );
		break;
	case P_TData4:
		vValRet.SetDWORD( m_ttNormal.m_tData4 );
		break;
	case P_TwoHands:
		// In some cases the layer is not set right.
		// override the layer here.
		if ( ! IsTypeEquippable())
			return( HRES_INVALID_HANDLE );
		vValRet.SetBool( m_layer == LAYER_HAND2 );
		break;
	case P_Type:
		vValRet.SetInt( m_type );
		break;
	case P_Value:
		if ( m_values.GetRange())
			vValRet.SetArrayFormat( "iLowVal,iHighVal", GetMakeValue(0), GetMakeValue(100));
		else
			vValRet.SetInt( GetMakeValue(0));
		break;
	case P_Weight:
		vValRet.SetInt( m_weight / WEIGHT_UNITS );
		break;
	case P_Resources2:
	case P_Resources3:
	case P_Skill:
	case P_DupeItem:
		return( HRES_INVALID_FUNCTION );
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CItemDef::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CObjBaseDef::s_PropSet( pszKey, vVal ));
	}
	switch (iProp)
	{
	case P_DispId:
		// Can't set this.
		return( HRES_INVALID_FUNCTION );
	case P_DupeItem:
		// Just ignore these. Cant use this way
		return( HRES_INVALID_FUNCTION );
	case P_Resources2:
	case P_Resources3:
		// Just ignore this stuff for now.
		break;
	case P_Skill:
		// NA Skill to use. (for old weapons defs)
#ifdef _DEBUG
		{
			int iSkillJunk = g_Cfg.FindSkillKey( vVal.GetPSTR(), true);
		}
#endif
		break;

	case P_DupeList:
		{
#ifdef _DEBUG
			if ( g_Serv.m_iModeCode == SERVMODE_Test5 )	// special mode. ignore this
				break;
#endif
			int iArgQty = vVal.MakeArraySize();
			if ( iArgQty <= 0 )
				return( HRES_BAD_ARG_QTY );
			m_flip_id.RemoveAll();
			for ( int i=0; i<iArgQty; i++ )
			{
				ITEMID_TYPE id = (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, vVal.GetArrayPSTR(i));
				if ( ! IsValidDispID( id ))
					continue;
				if ( IsSameDispID(id))
					continue;
				m_flip_id.Add(id);
			}
		}
		break;
	case P_Dye:
		if ( vVal.IsEmpty())
			m_CanFlags |= CAN_I_DYE;
		else
			m_CanFlags |= ( vVal.GetInt()) ? CAN_I_DYE : 0;
		break;
	case P_Flip:
		if ( vVal.IsEmpty())
			m_CanFlags |= CAN_I_FLIP;
		else
			m_CanFlags |= ( vVal.GetInt()) ? CAN_I_FLIP : 0;
		break;
	case P_Id:
		return SetBaseID( vVal, NULL );

	case P_Layer:
		// Is this equippable?
		m_layer = (LAYER_TYPE) vVal.GetInt();
		break;

	case P_Repair:
		m_CanFlags |= ( vVal.GetBool()) ? CAN_I_REPAIR : 0;
		break;
	case P_Replicate:
		m_CanFlags |= ( vVal.GetBool()) ? CAN_I_REPLICATE : 0;
		break;
	case P_ReqStr:
		if ( ! IsTypeEquippable())
			return( HRES_INVALID_HANDLE );
		m_ttEquippable.m_StrReq = vVal.GetInt();
		break;
	case P_SkillMake: // Skill required to make this.
		m_SkillMake.s_LoadKeys( vVal.GetPSTR());
		break;
	case P_TData1:
		m_ttNormal.m_tData1 = vVal.GetInt();
		break;
	case P_TData2:
		m_ttNormal.m_tData2 = vVal.GetInt();
		break;
	case P_TData3:
		m_ttNormal.m_tData3 = vVal.GetInt();
		break;
	case P_TData4:
		m_ttNormal.m_tData4 = vVal.GetInt();
		break;

	case P_TwoHands:
		// In some cases the layer is not set right.
		// override the layer here.
		if ( ! IsTypeEquippable())
			return( HRES_INVALID_HANDLE );
		if ( vVal.GetBool())
		{
			m_layer = LAYER_HAND2;
		}
		break;
	case P_Type:
		return SetBaseType( 
				(IT_TYPE) g_Cfg.ResourceGetIndexType( RES_TypeDef, vVal.GetPSTR()), 
				NULL );
	case P_Value:
		m_values.v_Set(vVal);
		break;
	case P_Weight:
		// Read in the weight but it may not be decimalized correctly
		{
			bool fDecimal = ( strchr( vVal.GetPSTR(), '.' ) != NULL );
			m_weight = vVal.GetInt();
			if ( ! fDecimal )
			{
				m_weight *= WEIGHT_UNITS;
			}
		}
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

CItemDefPtr CItemDef::MakeDupeReplacement( CItemDef* pBase, CGVariant& vValMaster ) // static
{
	// This item is just a dupe. So store it differently.

	ITEMID_TYPE id = pBase->GetID();
	ITEMID_TYPE idmaster = (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, vValMaster );

	if ( ! IsValidDispID( idmaster ))
	{
		DEBUG_ERR(( "CItemDef:DUPEITEM 0%x bad master '%s'" LOG_CR, id, (LPCSTR) vValMaster ));
		return( pBase );
	}
	if ( idmaster == id )
	{
		DEBUG_ERR(( "CItemDef:DUPEITEM 0%x==0%x weirdness" LOG_CR, id, idmaster ));
		return( pBase );
	}

	CItemDefPtr pBaseMaster = g_Cfg.FindItemDef( idmaster );
	if ( pBaseMaster == NULL )
	{
		DEBUG_ERR(( "CItemDef:DUPEITEM 0%x not exist 0%x" LOG_CR, id, idmaster ));
		return( pBase );
	}

	if ( pBaseMaster->GetID() != idmaster )
	{
		DEBUG_ERR(( "CItemDef:DUPEITEM 0%x circlular ref 0%x" LOG_CR, id, idmaster ));
		return( pBase );
	}

	if ( ! pBaseMaster->IsSameDispID(id))	// already here ?!
	{
		pBaseMaster->m_flip_id.Add(id);
	}

	// create the dupe stub.
	CItemDefDupe* pBaseDupe = new CItemDefDupe( id, pBaseMaster );
	ASSERT(pBaseDupe);
	g_Cfg.m_ResHash.AddSortKey( pBaseDupe->GetUIDIndex(), pBaseDupe );

	return( pBaseMaster );
}

//**************************************************
// -CItemDefWeapon

const CScriptProp CItemDefWeapon::sm_Props[CItemDefWeapon::P_QTY+1] =
{
#define CITEMDEFWEAPONPROP(a,b,c)	CSCRIPT_PROP_IMP(a,b,c)
#include "CItemDefWeaponProps.tbl"
#undef CITEMDEFWEAPONPROP
	NULL,	
};

CSCRIPT_CLASS_IMP1(ItemDefWeapon,CItemDefWeapon::sm_Props,NULL,CItemDef::sm_Triggers,ItemDef);

HRESULT CItemDefWeapon::s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp<0)
	{
		return( CItemDef::s_PropGet( pszKey, vVal, pSrc ));
	}
	switch (iProp)
	{
	case P_Armor:
	case P_Att:
	case P_Dam:
		// Full range?
		vVal.SetInt( m_damage.GetMin());
		break;
	case P_ArmorLo:
	case P_DamageLo:
		vVal.SetInt( m_damage.GetMin());
		break;
	case P_ArmorHi:
	case P_DamageHi:
		vVal.SetInt( m_damage.GetMax());
		break;
	case P_Speed:
		vVal.SetInt( GetSpeedAttack() );
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return NO_ERROR;
}

HRESULT CItemDefWeapon::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp<0)
	{
		return( CItemDef::s_PropSet(pszKey, vVal));
	}
	switch ( iProp )
	{
	case P_Armor:
	case P_Att:
	case P_Dam:
	case P_DamageLo:
		m_damage.v_Set(vVal);
		break;
	case P_ArmorHi:
	case P_DamageHi:
		m_damage.SetMax( vVal );
		break;
	case P_Speed:
		m_speedAttack = vVal.GetInt();
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return NO_ERROR;
}

CItemDefWeapon::CItemDefWeapon( CItemDef* pBase ) :
	CItemDef( pBase->GetID())
{
	// copy the stuff from the pBase
	CItemDefWeapon*	pBaseWeapon = PTR_CAST(CItemDefWeapon,pBase);
	if ( pBaseWeapon )
	{
		CopyBasic(pBaseWeapon);
	}
	else
	{
		m_speedAttack = 0;
	}
	CopyTransfer(pBase);
}

//**************************************************
// -CItemDefMulti

CItemDefMulti::CItemDefMulti( CItemDef* pBase ) :
	CItemDef( pBase->GetID())
{
	m_dwRegionFlags = REGION_FLAG_NODECAY | REGION_ANTIMAGIC_TELEPORT | REGION_ANTIMAGIC_RECALL_IN | REGION_FLAG_NOBUILDING;
	m_rect.SetRectEmpty();
	// copy the stuff from the pBase
	CopyTransfer(pBase);
}

HRESULT CItemDefMulti::AddComponent( ITEMID_TYPE id, PNT_X_TYPE dx, PNT_Y_TYPE dy, PNT_Z_TYPE dz )
{
	m_rect.UnionPoint( dx, dy );
	if ( id > 0 )	// we can add a phantom item just to increase the size.
	{
// WESTY MOD (WESTY BAN)
		if( ( id >= ITEMID_MULTI ) && ( id < ITEMID_MULTI_MAX ) )
		{
			DEBUG_ERR(( "Bad COMPONENT 0%x" LOG_CR, id ));
			return HRES_BAD_ARGUMENTS;
		}
// END WESTY MOD

		CMulMultiItemRec comp;
		comp.m_wTileID = id;
		comp.m_dx = dx;
		comp.m_dy = dy;
		comp.m_dz = dz;
		m_Components.Add( comp );
	}

	return( NO_ERROR );
}

HRESULT CItemDefMulti::SetMultiRegion(CGVariant& vVal)
{
	// inclusive region.
	int iArgQty = vVal.MakeArraySize();
	if ( iArgQty <= 1 )
		return HRES_BAD_ARG_QTY;
	m_Components.RemoveAll();	// might be after a resync
	m_rect.SetRect( vVal.GetArrayInt(0), vVal.GetArrayInt(1),
		vVal.GetArrayInt(2)+1, vVal.GetArrayInt(3)+1 );
	return NO_ERROR;
}

HRESULT CItemDefMulti::AddComponent(CGVariant& vVal)
{
	int iArgQty = vVal.MakeArraySize();
	if ( iArgQty <= 1 )
		return HRES_BAD_ARG_QTY;
	return AddComponent( (ITEMID_TYPE) RES_GET_INDEX( vVal.GetArrayInt(0)), 
		vVal.GetArrayInt(1),
		vVal.GetArrayInt(2),
		vVal.GetArrayInt(3));
}

int CItemDefMulti::GetMaxDist() const
{
	// Get Max radius.
	int iDist = ABS( m_rect.m_left );
	int iDistTmp = ABS( m_rect.m_top );
	if ( iDistTmp > iDist )
		iDist = iDistTmp;
	iDistTmp = ABS( m_rect.m_right + 1 );
	if ( iDistTmp > iDist )
		iDist = iDistTmp;
	iDistTmp = ABS( m_rect.m_bottom + 1 );
	if ( iDistTmp > iDist )
		iDist = iDistTmp;
	return( iDist+1 );
}

const CScriptProp CItemDefMulti::sm_Props[CItemDefMulti::P_QTY+1] =
{
#define CITEMDEFMULTIPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "citemdefmultiprops.tbl"
#undef CITEMDEFMULTIPROP
	NULL,
};

CSCRIPT_CLASS_IMP1(ItemDefMulti,CItemDefMulti::sm_Props,NULL,CItemDef::sm_Triggers,ItemDef);

HRESULT CItemDefMulti::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if (iProp<0)
	{
		return( CItemDef::s_PropSet(pszKey, vVal));
	}
	switch ( iProp)
	{
	case P_Component:
		return AddComponent(vVal);
	case P_MultiRegion:
		return SetMultiRegion(vVal);
	case P_RegionFlags:
		m_dwRegionFlags = vVal.GetInt();
		break;
	case P_TSpeech:
		if ( ! m_Speech.v_Set( vVal, RES_Speech ))
			return HRES_BAD_ARGUMENTS;
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CItemDefMulti::s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if (iProp<0)
	{
		return( CItemDef::s_PropGet(pszKey, vVal,pSrc));
	}
	switch ( iProp)
	{
	case P_Component:
	case P_MultiRegion:
	case P_RegionFlags:
	case P_TSpeech:
		return HRES_INVALID_FUNCTION;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

//**************************************************

CItemDefPtr CItemDef::TranslateBase( CResourceDef* pResDef ) // static
{
	// CItemDef
	// is a like item already loaded.

	if ( pResDef == NULL )
		return NULL;

	CItemDefPtr pBase = PTR_CAST(CItemDef,pResDef);
	if ( pBase )
	{
		return( pBase );	// already loaded all base info.
	}

	CItemDefDupe* pBaseDupe = PTR_CAST(CItemDefDupe,pResDef);
	if ( pBaseDupe )
	{
		return( pBaseDupe->GetItemDef());	// this is just a dupeitem
	}

	ITEMID_TYPE id = (ITEMID_TYPE) RES_GET_INDEX(pResDef->GetUIDIndex());
	if ( id == ITEMID_NOTHING )
		return NULL;

	// Just a CResourceLink was made when the scripts were scanned.
	CItemTypeDef* pBaseLink = PTR_CAST(CItemTypeDef,pResDef);
	ASSERT(pBaseLink);

	pBase = new CItemDef(id);
	ASSERT(pBase);
	pBase->CopyLink( pBaseLink );

	// Find the previous one in the series if any.
	// Find it's script section offset.

	CResourceLock s(pBase);
	if ( ! s.IsFileOpen())
	{
		// must be scripted. not in the artwork set.
		g_Log.Event( LOG_GROUP_DEBUG, LOGL_ERROR, "UN-scripted item 0%0x NOT allowed." LOG_CR, pBase->GetID());
		return( NULL );
	}

	// Read the Script file preliminary.
	while ( s.ReadKeyParse())
	{
		if ( s.IsLineTrigger())	// trigger scripting marks the end
			break;

		P_TYPE_ iProp = (P_TYPE_) s_FindKeyInTable( s.GetKey(), CItemDef::sm_Props );
		switch(iProp)
		{
		case P_DupeItem:
			return( MakeDupeReplacement( pBase, s.GetArgVar()));

		case P_Type:
			pBase->SetBaseType( 
				(IT_TYPE) g_Cfg.ResourceGetIndexType( RES_TypeDef, s.GetArgRaw()), 
				&pBase );
			continue;

		case P_Id:
			// This could set the type as well !
			pBase->SetBaseID( s.GetArgRaw(), &pBase );
			continue;
		}

		pBase->s_PropSet(s.GetKey(), s.GetArgVar());
	}

	// replace existing one
	g_Cfg.m_ResHash.AddSortKey( pResDef->GetHashCode(), pBase );	// Replace with new in sorted order.
	return( pBase );
}


//
// CItemSp.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.

//*********************************************************************
// CItemCorpse

bool CItemCorpse::IsPlayerDecayed() const
{
	// anyone can loot now?
	ASSERT( IsType(IT_CORPSE));
	return ! IsPlayerCorpse();
}

bool CItemCorpse::IsPlayerCorpse() const
{
	// Is this a players corpse?
	ASSERT( IsType(IT_CORPSE));
	CCharPtr pCharCorpse = g_World.CharFind(m_uidLink);
	if ( pCharCorpse == NULL )
		return false;
	if ( pCharCorpse->m_pPlayer )
		return true;
	return false;
}

CCharPtr CItemCorpse::IsCorpseSleeping() const
{
	// Is this corpse really a sleeping person ?
	// CItemCorpse
	ASSERT( IsType(IT_CORPSE));

	CCharPtr pCharCorpse = g_World.CharFind(m_uidLink);
	if ( pCharCorpse == NULL )
		return NULL;
	if ( pCharCorpse->IsStatFlag( STATF_Sleeping ) &&
		! m_itCorpse.m_timeDeath.IsTimeValid() )
	{
		return( pCharCorpse );
	}
	return( NULL );
}

//*********************************************************************
// -CItemMap

const CScriptProp CItemMap::sm_Props[2] = // static
{
	CSCRIPT_PROP_IMP(Pin,CSCRIPTPROP_WRITEO|CSCRIPTPROP_NARG2,"Add a pin")
	NULL,
};

#ifdef USE_JSCRIPT
#define CITEMMAPMETHOD(a,b,c) JSCRIPT_METHOD_IMP(CItemMap,a)
#include "citemmapmethods.tbl"
#undef CITEMMAPMETHOD
#endif

const CScriptMethod CItemMap::sm_Methods[CItemMap::M_QTY+1] = // static
{
#define CITEMMAPMETHOD(a,b,c) CSCRIPT_METHOD_IMP(a,b,c)
#include "citemmapmethods.tbl"
#undef CITEMMAPMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP1(ItemMap,CItemMap::sm_Props,CItemMap::sm_Methods,NULL,ItemVendable);

HRESULT CItemMap::s_PropSet( LPCTSTR pszKey, CGVariant& vVal ) // Load an item Script
{
	int iProp = s_FindMyPropKey(pszKey);
	if ( iProp == 0 )
	{
		int i = vVal.MakeArraySize();
		if ( i != 2 )
			return( HRES_BAD_ARG_QTY );
		CPointMap pntTemp( vVal.GetArrayInt(0), vVal.GetArrayInt(1));
		m_Pins.Add(pntTemp);
		return( NO_ERROR );
	}
	return( CItemVendable::s_PropSet( pszKey, vVal ));
}

HRESULT CItemMap::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	return( CItemVendable::s_PropGet( pszKey, vValRet, pSrc ));
}

HRESULT CItemMap::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp != M_Pin )
	{
		return( CItemVendable::s_Method( pszKey, vArgs, vValRet, pSrc ));
	}

	int i = vArgs.MakeArraySize();
	switch (i)
	{
	case 0:
		return( HRES_BAD_ARG_QTY );
	case 1:
		i = vArgs.GetArrayInt(0);
		if ( ! m_Pins.IsValidIndex(i)) 
			return( HRES_INVALID_INDEX );
		vValRet.SetArrayFormat( "iX,iY", m_Pins[i].m_x, m_Pins[i].m_y );
		break;
	case 2:
		{
		CPointMap pntTemp( vArgs.GetArrayInt(0), vArgs.GetArrayInt(1));
		m_Pins.Add(pntTemp);
		}
		break;
	case 3:
		i = vArgs.GetArrayInt(0);
		if ( ! m_Pins.IsValidIndex(i)) 
			return( HRES_INVALID_INDEX );
		{
		CPointMap pntTemp( vArgs.GetArrayInt(1), vArgs.GetArrayInt(2));
		m_Pins.SetAtGrow(i,pntTemp);
		}
		break;
	}
	return( NO_ERROR );
}

void CItemMap::s_Serialize( CGFile& a )
{
	// Read and write binary.
	CItemVendable::s_Serialize(a);

}

void CItemMap::s_WriteProps( CScript& s )
{
	CItemVendable::s_WriteProps( s );
	for ( int i=0; i<m_Pins.GetSize(); i++ )
	{
		s.WriteKey( "PIN", m_Pins[i].v_Get());
	}
}

void CItemMap::DupeCopy( const CItem* pItem )
{
	CItemVendable::DupeCopy(pItem);

	const CItemMap* pMapItem = PTR_CAST(const CItemMap,pItem);
	DEBUG_CHECK(pMapItem);
	if ( pMapItem == NULL )
		return;
	m_Pins.CopyArray( pMapItem->m_Pins );
}

//*********************************************************************
// -CItemMessage

const CScriptProp CItemMessage::sm_Props[CItemMessage::P_QTY+1] = // static
{
#define CITEMMESSAGEPROP(a,b,c)		CSCRIPT_PROP_IMP(a,b,c)
#include "citemmessageprops.tbl"
#undef CITEMMESSAGEPROP
	NULL,
};

#ifdef USE_JSCRIPT
#define CITEMMESSAGEMETHOD(a,b,c) JSCRIPT_METHOD_IMP(CItemMessage,a)
#include "citemmessagemethods.tbl"
#undef CITEMMESSAGEMETHOD
#endif

const CScriptMethod CItemMessage::sm_Methods[CItemMessage::M_QTY+1] =
{
#define CITEMMESSAGEMETHOD(a,b,c) CSCRIPT_METHOD_IMP(a,b,c)
#include "citemmessagemethods.tbl"
#undef CITEMMESSAGEMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP1(ItemMessage,CItemMessage::sm_Props,CItemMessage::sm_Methods,NULL,ItemVendable);

CItemMessage::CItemMessage( ITEMID_TYPE id, CItemDef* pItemDef ) : 
	CItemVendable( id, pItemDef )
{
}

CItemMessage::~CItemMessage()
{
	UnLoadSystemPages();
}

void CItemMessage::s_Serialize( CGFile& a )
{
	// Read and write binary.
	CItemVendable::s_Serialize(a);
}

void CItemMessage::s_WriteProps( CScript& s )
{
	CItemVendable::s_WriteProps( s );

	if ( ! m_sAuthor.IsEmpty())
	{
		s.WriteKey( "AUTHOR", m_sAuthor );
	}

	// Store the message body lines. MAX_BOOK_PAGES
	for ( int i=0; i<GetPageCount(); i++ )
	{
		LPCTSTR pszText = GetPageText(i);
		s.WriteKey( "BODY", ( pszText ) ?  pszText : "" );
	}
}

HRESULT CItemMessage::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	CGVariant vJunk;
	s_FixExtendedProp( pszKey, "Body", vJunk ); // just toss it.

	// Load the message body for a book or a bboard message.
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CItemVendable::s_PropSet( pszKey, vVal));
	}

	switch (iProp)
	{
	case P_Body:
		AddPageText( vVal );
		break;
	case P_Author:
		m_sAuthor = vVal.GetStr();
		break;
	case P_Pages:	// not settable. (used for resource stuff)
		return( HRES_INVALID_FUNCTION );
	case P_Title:
		SetName( vVal );
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CItemMessage::s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc )
{
	// Load the message body for a book or a bboard message.
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CItemVendable::s_PropGet( pszKey, vVal, pSrc ));
	}
	switch (iProp)
	{
	case P_Author:
		vVal = m_sAuthor;
		break;
	case P_Body:	// not avail.
		return( HRES_INVALID_FUNCTION );
	case P_Pages:	// not settable. (used for resource stuff)
		vVal = m_sBodyLines.GetSize();
		break;
	case P_Title:
		vVal = GetName();
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CItemMessage::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	// Execute command from script
	ASSERT(pSrc);
	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp < 0 )
	{
		return( CItemVendable::s_Method( pszKey, vArgs, vValRet, pSrc ));
	}
	switch(iProp)
	{
	case M_Erase:
		{
		// 1 based pages.
		int iPage = vArgs.GetInt();
		if ( ! iPage )
		{
			m_sBodyLines.RemoveAll();
			break;
		}
		iPage --;
		if ( ! m_sBodyLines.IsValidIndex(iPage))
			return( HRES_INVALID_INDEX );

		m_sBodyLines.RemoveAt( iPage );
		}
		break;
	case M_Body:
	case M_Page: // _Page: // 1 based
		{
		int iArgs = vArgs.MakeArraySize();
		if ( iArgs <= 0 )
			return HRES_BAD_ARG_QTY;

		int iPage = vArgs.GetInt()-1;
		if ( iArgs == 1 )
		{
			if ( ! m_sBodyLines.IsValidIndex(iPage))
				return( HRES_INVALID_INDEX );
			vValRet = m_sBodyLines.ElementAt(iPage);
		}
		else
		{
			vArgs.RemoveArrayElement(0);
			SetPageText( iPage, vArgs );
		}
		}
		break;
	default:
		DEBUG_CHECK(0);
		return HRES_INTERNAL_ERROR;
	}
	return( NO_ERROR );
}

void CItemMessage::DupeCopy( const CItem* pItem )
{
	CItemVendable::DupeCopy( pItem );

	const CItemMessage* pMsgItem = PTR_CAST(const CItemMessage,pItem);
	DEBUG_CHECK(pMsgItem);
	if ( pMsgItem == NULL )
		return;

	m_sAuthor = pMsgItem->m_sAuthor;
	for ( int i=0; i<pMsgItem->GetPageCount(); i++ )
	{
		SetPageText( i, pMsgItem->GetPageText(i));
	}
}

void CItemMessage::SetPageText( int iPage, LPCTSTR pszText )
{
	if ( pszText == NULL )
	{
		if ( iPage < m_sBodyLines.GetSize())
		{
			m_sBodyLines.RemoveAt( iPage );
		}
	}
	else
	{
		m_sBodyLines.SetAtGrow( iPage, CGString( pszText ));
	}
}

bool CItemMessage::LoadSystemPages()
{
	// Load the text of a book or message.

	DEBUG_CHECK( IsBookSystem());

	int iPages = -1;
	{
	CResourceLock s( g_Cfg.ResourceGetDef( m_itBook.m_ResID ));
	if ( ! s.IsFileOpen())
		return false;

	while ( s.ReadKeyParse())
	{
		switch ( s_FindMyPropKey( s.GetKey()))
		{
		case P_Pages:
			iPages = s.GetArgInt();
			break;
			// s_PropSet();
		case P_Author:
			m_sAuthor = s.GetArgStr();
			break;
		case P_Title:
			SetName( s.GetArgStr());	// Make sure the book is named.
			break;
		}
	}
	}

	if ( iPages > 2*MAX_BOOK_PAGES || iPages < 0 )
		return( false );

	TCHAR szTemp[ 16*1024 ];
	for ( int iPage=1; iPage<=iPages; iPage++ )
	{
		CResourceLock sPage( g_Cfg.ResourceGetDef( CSphereUID( RES_Book, m_itBook.m_ResID.GetResIndex(), iPage )));
		if ( ! sPage.IsFileOpen())
			break;

		// NOTE: Real blank lines are eaten here. (".line"?)

		int iLen = 0;
		while (sPage.ReadLine())
		{
			int iLenStr = strlen( sPage.GetKey());
			if ( iLen + iLenStr >= sizeof( szTemp ))
				break;
			iLen += strcpylen( szTemp+iLen, sPage.GetKey());
			szTemp[ iLen++ ] = '\t';
		}
		szTemp[ --iLen ] = '\0';

		AddPageText( szTemp );
	}

	return( true );
}

//*********************************************************************
// -CItemMemory

// CSCRIPT_CLASS_IMP0(ItemMemory,CItemMemory::sm_Props,NULL);

CItemMemory::CItemMemory( ITEMID_TYPE id, CItemDef* pItemDef ) : 
	CItemVendable( ITEMID_MEMORY, pItemDef )
{
}
CItemMemory::~CItemMemory()
{
}

CItemStonePtr CItemMemory::Guild_GetLink()
{
	if ( ! IsMemoryTypes(MEMORY_TOWN|MEMORY_GUILD))
		return NULL;
	return REF_CAST(CItemStone, g_World.ItemFind( m_uidLink ));
}

void CItemMemory::DeleteThis()	// delete myself from the list and system.
{
	CItemStonePtr pStone = Guild_GetLink();
	if ( pStone )
	{
		m_itEqMemory.m_Arg2 = STONESTATUS_REMOVE;	// i dont count anymore.
		pStone->Member_ElectMaster();	// May have changed the vote count.
	}

	CItemVendable::DeleteThis();	// delete myself from the system.
}

void CItemMemory::ChangeMotivation( MOTIVE_LEVEL iChange )
{
	// Change my MOTIVE_LEVEL about the linked object. 
	// ie. love level.

}

int CItemMemory::FixWeirdness()
{
	int iResultCode = CItem::FixWeirdness();
	if ( iResultCode )
	{
	bailout:
		return( iResultCode );
	}

	if ( ! IsItemEquipped() ||
		GetEquipLayer() != LAYER_SPECIAL ||
		! GetMemoryTypes())	// has to be a memory of some sort.
	{
		iResultCode = 0x4222;
		goto bailout;	// get rid of it.
	}

	CCharPtr pChar = PTR_CAST(CChar,GetParent());
	if ( pChar == NULL )
	{
		iResultCode = 0x4223;
		goto bailout;	// get rid of it.
	}

	// If it is my guild make sure i am linked to it correctly !
	if ( IsMemoryTypes(MEMORY_GUILD|MEMORY_TOWN))
	{
		CItemStonePtr pStone = Guild_GetLink();
		if ( pStone == NULL )
		{
			iResultCode = 0x4224;
			goto bailout;	// get rid of it.
		}
		pStone->m_Members.AttachObj( pChar );
	}

	if ( g_World.m_iLoadVersion < 49 )
	{
		if ( IsMemoryTypes( OLDMEMORY_MURDERDECAY ))
		{
			pChar->Noto_Murder();
			iResultCode = 0x4227;
			goto bailout;	// get rid of it.
		}
	}

	return( 0 );
}

//*********************************************************
// CItemCommCrystal

const CScriptProp CItemCommCrystal::sm_Props[2] =
{
	CSCRIPT_PROP_IMP(Speech,0,0)
	NULL,
};

CSCRIPT_CLASS_IMP1(ItemCommCrystal,CItemCommCrystal::sm_Props,NULL,NULL,ItemVendable);

void CItemCommCrystal::OnMoveFrom()
{
	// Being removed from the top level.
	CSectorPtr pSector = GetTopPoint().GetSector();
	ASSERT(pSector);
	pSector->RemoveListenItem();
}

bool CItemCommCrystal::MoveTo( CPointMap pt ) // Put item on the ground here.
{
	// Move this item to it's point in the world. (ground/top level)
	if ( ! CItem::MoveTo(pt))
		return false;
	CSectorPtr pSector = pt.GetSector();
	ASSERT(pSector);
	pSector->AddListenItem();
	return true;
}

void CItemCommCrystal::OnHear( LPCTSTR pszCmd, CChar* pSrc )
{
	// IT_COMM_CRYSTAL
	// STATF_COMM_CRYSTAL = if i am on a person.

	CSphereExpArgs exec( this, pSrc, pszCmd );

	for ( int i=0; i<m_Speech.GetSize(); i++ )
	{
		TRIGRET_TYPE iRet = OnHearTrigger( m_Speech[i], exec );
		if ( iRet != TRIGRET_ENDIF && iRet != TRIGRET_RET_FALSE )
			break;
	}
	if ( m_uidLink.IsValidObjUID())
	{
		// I am linked to something ?
		// Transfer the sound.
		CItemPtr pItem = g_World.ItemFind( m_uidLink );
		if ( pItem != NULL && pItem->IsType(IT_COMM_CRYSTAL))
		{
			pItem->Speak( pszCmd );
		}
	}
	else if ( ! m_Speech.GetSize())
	{
		Speak( pszCmd );
	}
}

void CItemCommCrystal::s_Serialize( CGFile& a )
{
	// Read and write binary.
	CItemVendable::s_Serialize(a);
}

void CItemCommCrystal::s_WriteProps( CScript& s )
{
	CItemVendable::s_WriteProps(s);
	m_Speech.s_WriteProps( s, "SPEECH" );
}
HRESULT CItemCommCrystal::s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc )
{
	int iProp = s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return CItemVendable::s_PropGet(pszKey,vVal,pSrc);
	}
	m_Speech.v_Get( vVal );
	return( NO_ERROR );
}
HRESULT CItemCommCrystal::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	int iProp = s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return CItemVendable::s_PropSet(pszKey, vVal);
	}
	if ( ! m_Speech.v_Set( vVal, RES_Speech ))
		return HRES_BAD_ARGUMENTS;
	return NO_ERROR;
}
void CItemCommCrystal::DupeCopy( const CItem* pItem )
{
	CItemVendable::DupeCopy(pItem);

	const CItemCommCrystal* pItemCrystal = PTR_CAST(const CItemCommCrystal,pItem);
	DEBUG_CHECK(pItemCrystal);
	if ( pItemCrystal == NULL )
		return;

	m_Speech.CopyArray( pItemCrystal->m_Speech );
}

//*********************************************************************
// -CItemServerGate

const CScriptProp CItemServerGate::sm_Props[3] =
{
	CSCRIPT_PROP_IMP(Server,	CSCRIPTPROP_RETVAL,	"Server name")
	CSCRIPT_PROP_IMP(Servp,		CSCRIPTPROP_RETREF, "ref to server")
	NULL,
};

CSCRIPT_CLASS_IMP1(ItemServerGate,CItemServerGate::sm_Props,NULL,NULL,Item);

HRESULT CItemServerGate::SetServerLink( LPCTSTR pszName )
{
	// IT_DREAM_GATE
	CThreadLockPtr lock( &g_Cfg.m_Servers );
	int index = g_Cfg.m_Servers.FindKey( m_sServerName );
	if ( index < 0 )
		return( HRES_INVALID_INDEX );
	m_itDreamGate.m_index = index;	// save a quicker index here. (tho it may change)
	m_sServerName = pszName;
	return( NO_ERROR );
}

CServerPtr CItemServerGate::GetServerRef() const
{
	CThreadLockPtr lock( &g_Cfg.m_Servers );
	int index = g_Cfg.m_Servers.FindKey( m_sServerName );
	if ( index < 0 )
		return( NULL );
	return g_Cfg.Server_GetDef( index );
}

int CItemServerGate::FixWeirdness()
{
	CServerLock pServ = GetServerRef();
	if ( pServ == NULL )
	{
		m_sServerName.Empty();
	}
	return CItem::FixWeirdness();
}
void CItemServerGate::s_Serialize( CGFile& a )
{
}
void CItemServerGate::s_WriteProps( CScript& s )
{
	CItem::s_WriteProps( s );
	s.WriteKey( sm_Props[1].m_pszName, GetServerLink());
}
HRESULT CItemServerGate::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	int iProp = s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return CItem::s_PropGet( pszKey, vValRet, pSrc );
	}
	switch(iProp)
	{
	case 0:
		vValRet.SetRef( GetServerRef());
		break;
	case 1:
		vValRet = GetServerLink(); // name
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}
HRESULT CItemServerGate::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	int iProp = s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CItem::s_PropSet(pszKey, vVal));
	}
	switch(iProp)
	{
	case 0:
	case 1:
		return SetServerLink( vVal );
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

//************************************************************************************

CResourceDefPtr CItem::Spawn_FixDef()
{
	// Get a proper CSphereUID from the id provided.
	// RETURN: true = ok.

	CSphereUIDBase rid;
	if ( IsType(IT_SPAWN_ITEM))
	{
		rid = m_itSpawnItem.m_ItemID;
	}
	else
	{
		ASSERT( IsType(IT_SPAWN_CHAR) );
		rid = m_itSpawnChar.m_CharID;
	}

	if ( rid.GetResType() != RES_UNKNOWN )
	{
		return g_Cfg.ResourceGetDef(rid);
	}

	// No type info here !?

	if ( IsType(IT_SPAWN_ITEM))
	{
		ITEMID_TYPE id = (ITEMID_TYPE) rid.GetResIndex();
		if ( id < ITEMID_TEMPLATE )
		{
		why_not_try_this_item:
			CItemDefPtr pItemDef = g_Cfg.FindItemDef( id );
			if ( pItemDef )
			{
				m_itSpawnItem.m_ItemID = CSphereUID( RES_ItemDef, id );
				return( (CResourceDef*)pItemDef );
			}
		}
		else
		{
			// try a template.
			rid = CSphereUID( RES_Template, id );
			CResourceDefPtr pDef = g_Cfg.ResourceGetDef(rid);
			if ( pDef )
			{
				m_itSpawnItem.m_ItemID = rid;
				return( pDef );
			}
			goto why_not_try_this_item;
		}
	}
	else
	{
		CREID_TYPE id = (CREID_TYPE) rid.GetResIndex();
		if ( id < SPAWNTYPE_START )
		{
why_not_try_this_char:
			CCharDefPtr pCharDef = g_Cfg.FindCharDef( id );
			if ( pCharDef )
			{
				m_itSpawnChar.m_CharID = CSphereUID( RES_CharDef, id );
				return( (CResourceDef*)pCharDef );
			}
		}
		else
		{
			// try a spawn group.
			rid = CSphereUID( RES_Spawn, id );
			CResourceDefPtr pDef = g_Cfg.ResourceGetDef(rid);
			if ( pDef )
			{
				m_itSpawnChar.m_CharID = rid;
				return( pDef );
			}
			goto why_not_try_this_char;
		}
	}

	return( NULL );
}

int CItem::Spawn_GetName( TCHAR* pszOut ) const
{
	CSphereUIDBase rid;
	if ( IsType(IT_SPAWN_ITEM))
	{
		rid = m_itSpawnItem.m_ItemID;
	}
	else
	{
		// Name the spawn type.
		rid = m_itSpawnChar.m_CharID;
	}

	CGString sName;
	CResourceDefPtr pDef = g_Cfg.ResourceGetDef( rid );
	if ( pDef != NULL )
	{
		sName = pDef->GetName();
	}
	if ( pDef == NULL || sName.IsEmpty())
	{
		sName = g_Cfg.ResourceGetName( rid );
	}
	return sprintf( pszOut, " (%s)", (LPCSTR) sName );
}

void CItem::Spawn_GenerateItem( CResourceDef* pDef )
{
	// Count how many items are here already.
	// This could be in a container.

	CSphereUID rid = pDef->GetUIDIndex();
	ITEMID_TYPE id = (ITEMID_TYPE) rid.GetResIndex();
	int iDistMax = m_itSpawnItem.m_DistMax;
	int iAmountPile = m_itSpawnItem.m_pile;

	int iCount = 0;
	CItemContainerPtr pCont = PTR_CAST(CItemContainer,GetParent());
	if ( pCont)
	{
		iCount = pCont->ContentCount( rid );
	}
	else
	{
		// If is equipped this will produce the item where you are standing.
		CPointMap pt = GetTopLevelObj()->GetTopPoint();
		CWorldSearch AreaItems( pt, iDistMax );
		for(;;)
		{
			CItemPtr pItem = AreaItems.GetNextItem();
			if ( pItem == NULL )
				break;
			if ( pItem->IsType(IT_SPAWN_ITEM))
				continue;
			if ( pItem->IsAttr( ATTR_INVIS ))
				continue;
			if ( pItem->GetID() != id )
				continue;
			// if ( pItem->m_uidLink != GetUID()) continue;
			iCount += pItem->GetAmount();
		}
	}
	if ( iCount >= GetAmount())
		return;

	CItemPtr pItem = CreateTemplate( id,NULL,NULL);
	if ( pItem == NULL )
		return;

	pItem->SetAttr( m_AttrMask & ( ATTR_OWNED | ATTR_MOVE_ALWAYS ));

	if ( iAmountPile > 1 )
	{
		CItemDefPtr pItemDef = pItem->Item_GetDef();
		ASSERT(pItemDef);
		if ( pItemDef->IsStackableType())
		{
			if ( iAmountPile == 0 || iAmountPile > GetAmount())
				iAmountPile = GetAmount();
			pItem->SetAmount( Calc_GetRandVal(iAmountPile) + 1 );
		}
	}

	// pItem->m_uidLink = GetUID();	// This might be dangerous ?
	pItem->SetDecayTime( g_Cfg.m_iDecay_Item );	// It will decay eventually to be replaced later.
	pItem->MoveNearObj( this, iDistMax );
}

void CItem::Spawn_GenerateChar( CResourceDef* pDef )
{
	if ( ! IsTopLevel())
		return;	// creatures can only be top level.
	if ( m_itSpawnChar.m_current >= GetAmount())
		return;
	int iComplexity = GetTopSector()->GetCharComplexity();
	if ( iComplexity > g_Cfg.m_iMaxCharComplexity )
	{
		DEBUG_TRACE(( "Spawn uid=0%lx too complex (%d>%d)" LOG_CR, GetUID(), iComplexity, g_Cfg.m_iMaxCharComplexity ));
		return;
	}

	int iDistMax = m_itSpawnChar.m_DistMax;
	CSphereUID rid = pDef->GetUIDIndex();
	if ( rid.GetResType() == RES_Spawn )
	{
		const CRegionType* pSpawnGroup = STATIC_CAST(const CRegionType,pDef);
		ASSERT(pSpawnGroup);
		int i = pSpawnGroup->GetRandMemberIndex();
		if ( i >= 0 )
		{
			rid = pSpawnGroup->GetMemberID(i);
		}
	}

	CREID_TYPE id;
	if ( rid.GetResType() == RES_CharDef ||
		rid.GetResType() == RES_UNKNOWN )
	{
		id = (CREID_TYPE) rid.GetResIndex();
	}
	else
	{
		return;
	}
	if ( id == CREID_INVALID )
		return;

	CCharPtr pChar = CChar::CreateNPC( id );
	ASSERT(pChar);
	ASSERT(pChar->m_pNPC);

	m_itSpawnChar.m_current ++;
	pChar->Memory_AddObjTypes( this, MEMORY_ISPAWNED );
	// Move to spot "near" the spawn item.
	pChar->MoveNearObj( this, iDistMax );
	if ( iDistMax )
	{
		pChar->m_ptHome = GetTopPoint();
		pChar->m_pNPC->m_Home_Dist_Wander = iDistMax;
	}
	pChar->Update();
}

void CItem::Spawn_OnTick( bool fExec )
{
	int iMinutes;
	if ( m_itSpawnChar.m_TimeHiMin <= 0 )
	{
		iMinutes = Calc_GetRandVal(30) + 1;
	}
	else
	{
		iMinutes = MIN( m_itSpawnChar.m_TimeHiMin, m_itSpawnChar.m_TimeLoMin ) + Calc_GetRandVal( ABS( m_itSpawnChar.m_TimeHiMin - m_itSpawnChar.m_TimeLoMin ));
	}

	if ( iMinutes <= 0 )
		iMinutes = 1;
	SetTimeout( iMinutes* 60* TICKS_PER_SEC );	// set time to check again.

	if ( ! fExec )
		return;

	CResourceDefPtr pDef = Spawn_FixDef();
	if ( pDef == NULL )
	{
		CSphereUIDBase rid;
		if ( IsType(IT_SPAWN_ITEM))
		{
			rid = m_itSpawnItem.m_ItemID;
		}
		else
		{
			rid = m_itSpawnChar.m_CharID;
		}
		DEBUG_ERR(( "Bad Spawn point uid=0%lx, id=%s" LOG_CR, GetUID(), (LPCTSTR) g_Cfg.ResourceGetName(rid) ));
		return;
	}

	if ( IsType(IT_SPAWN_ITEM))
	{
		Spawn_GenerateItem(pDef);
	}
	else
	{
		Spawn_GenerateChar(pDef);
	}
}

void CItem::Spawn_KillChildren()
{
	// kill all creatures spawned from this !
	DEBUG_CHECK( IsType(IT_SPAWN_CHAR));
	int iCurrent = m_itSpawnChar.m_current;
	for ( int i=0; i<SECTOR_QTY; i++ )
	{
		CSectorPtr pSector = g_World.GetSector(i);
		ASSERT(pSector);
		CCharPtr pCharNext;
		CCharPtr pChar = pSector->m_Chars_Active.GetHead();
		for ( ; pChar!=NULL; pChar = pCharNext )
		{
			pCharNext = pChar->GetNext();
			if ( pChar->NPC_IsSpawnedBy( this ))
			{
				pChar->DeleteThis();
				iCurrent --;
			}
		}
	}
	if (iCurrent && ! g_Serv.IsLoading())
	{
		DEBUG_CHECK(iCurrent==0);
	}
	m_itSpawnChar.m_current = 0;	// Should not be necessary
	Spawn_OnTick( false );
}

CCharDefPtr CItem::Spawn_SetTrackID()
{
	if ( ! IsType(IT_SPAWN_CHAR))
		return NULL;
	CCharDefPtr pCharDef;
	CSphereUIDBase rid = m_itSpawnChar.m_CharID;

	if ( rid.GetResType() == RES_CharDef )
	{
		CREID_TYPE id = (CREID_TYPE) rid.GetResIndex();
		pCharDef = g_Cfg.FindCharDef( id );
	}
	if ( IsAttr(ATTR_INVIS))	// They must want it to look like this.
	{
		SetDispID( ( pCharDef == NULL ) ? ITEMID_TRACK_WISP : pCharDef->m_trackID );
		SetHue( HUE_RED_DARK );	// Indicate to GM's that it is invis.
	}
	return( pCharDef );
}

//*********************************************************************

void CItem::Plant_SetTimer()
{
	SetTimeout( GetDecayTime() );
}

bool CItem::Plant_Use( CChar* pChar )
{
	// Pick cotton/hay/etc...
	// use the
	//  IT_CROPS = transforms into a "ripe" variety then is used up on reaping.
	//  IT_FOLIAGE = is not consumed on reap (unless eaten then will regrow invis)
	//

	DEBUG_CHECK( IsType(IT_CROPS) || IsType(IT_FOLIAGE));

	ASSERT(pChar);
	if ( ! pChar->CanSeeItem(this))	// might be invis underground.
		return( false );

	CItemDefPtr pItemDef = Item_GetDef();
	ITEMID_TYPE iFruitID = (ITEMID_TYPE) RES_GET_INDEX( pItemDef->m_ttCrops.m_idFruit );	// if it's reapable at this stage.
	if ( iFruitID <= 0 )
	{
		// not ripe. (but we could just eat it if we are herbivorous ?)
		pChar->WriteString( "None of the crops are ripe enough." );
		return( true );
	}
	if ( m_itCrop.m_ReapFruitID )
	{
		iFruitID = (ITEMID_TYPE) RES_GET_INDEX( m_itCrop.m_ReapFruitID );
	}

	if ( iFruitID > 0 )
	{
		CItemPtr pItemFruit = CItem::CreateScript( iFruitID, pChar );
		if ( pItemFruit)
		{
			pChar->ItemBounce( pItemFruit );
		}
	}
	else
	{
		pChar->WriteString( "The plant yields nothing useful" );
	}

	Plant_CropReset();

	pChar->UpdateAnimate( ANIM_BOW );
	pChar->Sound( 0x13e );
	return true;
}

bool CItem::Plant_OnTick()
{
	ASSERT( IsType(IT_CROPS) || IsType(IT_FOLIAGE));
	// If it is in a container, kill it.
	if ( !IsTopLevel())
	{
		return false;
	}

	// Make sure the darn thing isn't moveable
	SetAttr(ATTR_MOVE_NEVER);
	Plant_SetTimer();

	// No tree stuff below here
	if ( IsAttr(ATTR_INVIS)) // If it's invis, take care of it here and return
	{
		SetHue( HUE_DEFAULT );
		ClrAttr(ATTR_INVIS);
		Update();
		return true;
	}

	CItemDefPtr pItemDef = Item_GetDef();
	ITEMID_TYPE iGrowID = pItemDef->m_ttCrops.m_idGrow;

	if ( iGrowID == -1 )
	{
		// Some plants geenrate a fruit on the ground when ripe.
		ITEMID_TYPE iFruitID = (ITEMID_TYPE) RES_GET_INDEX( pItemDef->m_ttCrops.m_idGrow );
		if ( m_itCrop.m_ReapFruitID )
		{
			iFruitID = (ITEMID_TYPE) RES_GET_INDEX( m_itCrop.m_ReapFruitID );
		}
		if ( ! iFruitID )
		{
			return( true );
		}

		// put a fruit on the ground if not already here.
		CWorldSearch AreaItems( GetTopPoint() );
		for(;;)
		{
			CItemPtr pItem = AreaItems.GetNextItem();
			if ( pItem == NULL )
			{
				CItemPtr pItemFruit = CItem::CreateScript( iFruitID, NULL );
				ASSERT(pItemFruit);
				pItemFruit->MoveToDecay(GetTopPoint(),10*g_Cfg.m_iDecay_Item);
				break;
			}
			if ( pItem->IsType( IT_FRUIT ) || pItem->IsType( IT_REAGENT_RAW ))
				break;
		}

		// NOTE: ??? The plant should cycle here as well !
		iGrowID = pItemDef->m_ttCrops.m_idReset;
	}

	if ( iGrowID )
	{
		SetID( (ITEMID_TYPE) RES_GET_INDEX( iGrowID ));
		Update();
		return true;
	}

	// some plants go dormant again ?

	// m_itCrop.m_Fruit_ID = iTemp;
	return true;
}

void CItem::Plant_CropReset()
{
	// Animals will eat crops before they are ripe, so we need a way to reset them prematurely

	if ( ! IsType(IT_CROPS) && ! IsType(IT_FOLIAGE))
	{
		// This isn't a crop, and since it just got eaten, we should delete it
		DeleteThis();
		return;
	}

	CItemDefPtr pItemDef = Item_GetDef();
	ITEMID_TYPE iResetID = (ITEMID_TYPE) RES_GET_INDEX( pItemDef->m_ttCrops.m_idReset );
	if ( iResetID != ITEMID_NOTHING )
	{
		SetID(iResetID);
	}

	Plant_SetTimer();
	RemoveFromView();	// remove from most screens.
	SetHue( HUE_RED_DARK );	// Indicate to GM's that it is growing.
	SetAttr(ATTR_INVIS);	// regrown invis.
}


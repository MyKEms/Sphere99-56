//
// CObjBase.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
// base classes.
//
#include "stdafx.h"	// predef header.

static bool GetDeltaStr( CPointMap& pt, CGVariant& vArgs )
{
	int iArgQty = vArgs.MakeArraySize();

	if ( vArgs.GetArrayElement(0).IsNumeric())
	{
		pt.m_x += vArgs.GetArrayInt(0);
		pt.m_y += vArgs.GetArrayInt(1);
		pt.m_z += vArgs.GetArrayInt(2);
	}
	else	// a direction by name.
	{
		DIR_TYPE eDir = CGRect::GetDirStr( vArgs.GetArrayPSTR(0));
		if ( eDir >= DIR_QTY )
			return( false );
		int iDist = vArgs.GetArrayInt(1);
		if ( iDist == 0 )
			iDist = 1;
		pt.MoveN( eDir, iDist );
	}

	return( true );
}

/////////////////////////////////////////////////////////////////
// -CObjBase stuff
// Either a player, npc or item.

const CScriptProp CObjBase::sm_Props[CObjBase::P_QTY+1] =
{
#define COBJBASEPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "cobjbaseprops.tbl"
#undef COBJBASEPROP
	NULL,
};

#ifdef USE_JSCRIPT
#define COBJBASEMETHOD(a,b,c,d) JSCRIPT_METHOD_IMP(CObjBase,a)
#include "cobjbasemethods.tbl"
#undef COBJBASEMETHOD
#endif

const CScriptMethod CObjBase::sm_Methods[CObjBase::M_QTY+1] =
{
#define COBJBASEMETHOD(a,b,c,d) CSCRIPT_METHOD_IMP(a,b,c d)
#include "cobjbasemethods.tbl"
#undef COBJBASEMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP1(ObjBase,CObjBase::sm_Props,CObjBase::sm_Methods,NULL,ResourceObj);

int CObjBase::sm_iCount = 0;	// UID table.
bool CObjBase::sm_fDeleteReal = false;	// UID table.

CObjBase::CObjBase( UID_INDEX dwUIDMask )
{
	// based on CObjBaseTemplate
	// dwUIDMask = UID_F_ITEM;
	sm_iCount ++;
	m_wHue=HUE_DEFAULT;
	m_timeCreate.InitTimeCurrent();

	if ( ! g_Serv.IsLoading())
	{
		// Find a free UID slot for this.
		g_World.AllocUID( this, dwUIDMask );
	}

	// Put in the idle list by default. (til placed in the world)
	g_World.m_ObjNew.InsertHead( this );
}

CObjBase::~CObjBase()
{
	sm_iCount --;
	// ASSERT(sm_fDeleteReal);
	// ASSERT( GetParent() == NULL || GetParent() == &g_World.m_ObjDelete );

	// free up the UID slot.
	g_World.FreeUID(this);
}

bool CObjBase::IsContainer() const
{
	// Simple test if object IS a container. (may be parellel def)
	return( PTR_CAST(const CContainer,this) != NULL );
}

int CObjBase::IsWeird() const
{
	int iResultCode = CObjBaseTemplate::IsWeird();
	if ( iResultCode )
	{
		return( iResultCode );
	}
	if ( ! g_Serv.IsLoading())
	{
		if ( g_World.ObjFind(GetUID()) != this )	// make sure it's linked both ways correctly.
		{
			return( 0x3201 );
		}
	}
	return( 0 );
}

int CObjBase::NameStrip( TCHAR* pszNameOut, LPCTSTR pszNameInp, int iNameLen )
{
	// MAX_ITEM_NAME_SIZE
	//	allow just basic chars. 
	//	spaces and letters
	// RETURN: 
	//  new length of the name.

	bool fLastSpace = false;
	int iLen=0;
	for ( int i=0; pszNameInp[i] && i<iNameLen; i++ )
	{
		if ( isalpha(pszNameInp[i]))
		{
			pszNameOut[iLen++] = pszNameInp[i];
			continue;
		}
		if ( isspace(pszNameInp[i]) && ! fLastSpace )
		{
			fLastSpace = true;
			pszNameOut[iLen++] = ' ';
			continue;
		}
		continue;
	}
	pszNameOut[iLen] = '\0';

	if ( g_Cfg.IsObscene( pszNameOut ))
	{
		DEBUG_ERR(( "Obscene name '%s' ignored." LOG_CR, pszNameInp ));
		pszNameOut[0] = '\0';
		return( -1 );
	}

	return iLen;
}

bool CObjBase::SetNamePool( LPCTSTR pszName )
{
	ASSERT(pszName);

	// Parse out the name from the name pool ?
	if ( pszName[0] == '#' )
	{
		pszName ++;
		CResourceLock s( g_Cfg.ResourceGetDefByName( RES_Names, pszName ));
		if ( ! s.IsFileOpen())
		{
failout:
			DEBUG_ERR(( "Name pool '%s' could not be found" LOG_CR, pszName ));
			CObjBase::SetName( pszName );
			return false;
		}

		// Pick a random name.
		if ( ! s.ReadLine())
			goto failout;
		int iCount = Calc_GetRandVal( atoi( s.GetKey())) + 1;

		while ( iCount-- )
		{
			if ( ! s.ReadLine())
				goto failout;
		}

		return CObjBaseTemplate::SetName( s.GetKey());
	}

	// NOTE: Name must be <= MAX_NAME_SIZE
	TCHAR szTmp[ MAX_ITEM_NAME_SIZE + 1 ];
	int iLen = NameStrip( szTmp, pszName, MAX_ITEM_NAME_SIZE );

	// Can't be a dupe name with type ?
	LPCTSTR pszTypeName = Base_GetDef()->GetTypeName();
	if ( ! _stricmp( pszTypeName, szTmp ))
		szTmp[0] = '\0';

	return CObjBaseTemplate::SetName( szTmp );
}

bool CObjBase::IsResourceMatch( CSphereUID rid, DWORD dwAmount )
{
	return( m_Events.FindResourceID( rid ) >= 0 );
}

bool CObjBase::MoveNearObj( const CObjBaseTemplate* pObj, int iSteps, CAN_TYPE wCanFlags )
{
	ASSERT( pObj );
	if ( pObj->IsDisconnected())	// nothing is "near" a disconnected item.
	{
		// DEBUG_CHECK(! pObj->IsDisconnected() );
		return( false );
	}

	pObj = pObj->GetTopLevelObj();
	MoveNear( pObj->GetTopPoint(), iSteps, wCanFlags );
	return( true );
}

void CObjBase::s_WriteSafe( CScript& s )
{
	// Write an object with some fault protection.

	CSphereUID uid;
	try
	{
		uid = GetUID();
		if ( ! g_Cfg.m_fSaveGarbageCollect )
		{
			if ( g_World.FixObj( this ))
				return;
		}
		s_WriteProps(s);
	}
	SPHERE_LOG_TRY_CATCH1( "Write Object 0%x", (DWORD) uid )
}

void CObjBase::SetTimeout( int iDelayInTicks )
{
	// Set delay in TICKS_PER_SEC of a sec.
	// -1 = never.

#if 0 // def _DEBUG
	if ( g_Log.IsLogged( LOGL_TRACE ))
	{
		// DEBUG_MSG(( "SetTimeout(%d) for 0%lx" LOG_CR, iDelay, GetUID()));
		// if ( iDelay > 100*TICKS_PER_SEC )
		//{
		//	DEBUG_MSG(( "SetTimeout(%d) for 0%lx LARGE" LOG_CR, iDelay, GetUID()));
		//}
	}
#endif

	if ( iDelayInTicks < 0 )
	{
		m_timeout.InitTime();
	}
	else
	{
		m_timeout.InitTimeCurrent( iDelayInTicks );
	}
}

void CObjBase::DupeCopy( const CObjBase* pObj )
{
	CObjBaseTemplate::DupeCopy( pObj );
	m_wHue = pObj->GetHue();
	// m_timeout = pObj->m_timeout;
	m_TagDefs = pObj->m_TagDefs;
	m_Events.CopyArray( pObj->m_Events );
}

void CObjBase::Sound( SOUND_TYPE id, int iOnce ) const // Play sound effect for player
{
	// play for everyone near by.

	if ( id <= 0 )
		return;

	for ( CClientPtr pClient = g_Serv.GetClientHead(); pClient; pClient = pClient->GetNext())
	{
		if ( ! pClient->CanHear( this, TALKMODE_OBJ ))
			continue;
		pClient->addSound( id, this, iOnce );
	}
}

void CObjBase::Effect( EFFECT_TYPE motion, ITEMID_TYPE id, const CObjBase* pSource, BYTE bSpeedSeconds, BYTE bLoop, bool fExplode ) const
{
	// show for everyone near by.
	//
	// bSpeedSeconds
	// bLoop
	// fExplode

	for ( CClientPtr pClient = g_Serv.GetClientHead(); pClient; pClient = pClient->GetNext())
	{
		if ( ! pClient->CanSee( this ))
			continue;
		pClient->addEffect( motion, id, this, pSource, bSpeedSeconds, bLoop, fExplode );
	}
}

void CObjBase::Emote( LPCTSTR pText, CClient* pClientExclude, bool fForcePossessive )
{
	// IF this is not the top level object then it might be possessive ?

	// "*You see NAME blah*" or "*You blah*"
	// fPosessive = "*You see NAME's blah*" or "*Your blah*"

	CObjBasePtr pObjTop = GetTopLevelObj();
	ASSERT(pObjTop);

	CGString sMsgThem;
	CGString sMsgYou;

	if ( pObjTop->IsChar())
	{
		// Someone has this equipped.

		if ( pObjTop != this )
		{
			sMsgThem.Format( "*You see %ss %s %s*", (LPCTSTR) pObjTop->GetName(), (LPCTSTR) GetName(), (LPCTSTR) pText );
			sMsgYou.Format( "*Your %s %s*", (LPCTSTR) GetName(), (LPCTSTR) pText );
		}
		else if ( fForcePossessive )
		{
			// ex. "You see joes poor shot ruin an arrow"
			sMsgThem.Format( "*You see %ss %s*", (LPCTSTR) GetName(), (LPCTSTR) pText );
			sMsgYou.Format( "*Your %s*", pText );
		}
		else
		{
			sMsgThem.Format( "*You see %s %s*", (LPCTSTR) GetName(), (LPCTSTR) pText );
			sMsgYou.Format( "*You %s*", pText );
		}
	}
	else
	{
		// Top level is an item. Article ?
		sMsgThem.Format( "*You see %s %s*", (LPCTSTR) GetName(), (LPCTSTR) pText );
		sMsgYou = sMsgThem;
	}

	pObjTop->UpdateObjMessage( sMsgThem, sMsgYou, pClientExclude, HUE_RED, TALKMODE_EMOTE );
}

void CObjBase::Speak( LPCTSTR pText, HUE_TYPE wHue, TALKMODE_TYPE mode, FONT_TYPE font )
{
	g_World.Speak( this, pText, wHue, mode, font );
}

void CObjBase::SpeakUTF8( LPCTSTR pText, HUE_TYPE wHue, TALKMODE_TYPE mode, FONT_TYPE font, CLanguageID lang )
{
	// convert UTF8 to UNICODE.
	NCHAR szBuffer[ MAX_TALK_BUFFER ];
	int iLen = CvtSystemToNUNICODE( szBuffer, COUNTOF(szBuffer), pText );
	g_World.SpeakUNICODE( this, szBuffer, wHue, mode, font, lang );
}

void CObjBase::MoveNear( CPointMap pt, int iSteps, CAN_TYPE wCanFlags )
{
	// Move to nearby this other object.
	// Actually move it within +/- iSteps
	//  wCanFlags = what we can pass thru. doors, water, walls ?
	//		CAN_C_GHOST	= Moves thru doors etc.	- CAN_I_DOOR

	DIR_TYPE dir = (DIR_TYPE) Calc_GetRandVal( DIR_QTY );
	for (; iSteps > 0 ; iSteps-- )
	{
		// Move to the right or left?
		CPointMap ptTest = pt;	// Save this so we can go back to it if we hit a blocking object.
		pt.Move( dir );
		dir = GetDirTurn( dir, Calc_GetRandVal(3)-1 );	// stagger ?

		// Put the item at the correct Z point
		CMulMapBlockState block( wCanFlags );
		g_World.GetHeightPoint( pt, block, pt.GetRegion(REGION_TYPE_MULTI));
		if ( block.IsResultBlocked())
		{
			// Hit a block, so go back to the previous valid position
			pt = ptTest;
			break;	// stopped
		}
		pt.m_z = block.GetResultZ();
	}

	MoveTo( pt );
}

void CObjBase::UpdateObjMessage( LPCTSTR pTextThem, LPCTSTR pTextYou, CClient* pClientExclude, HUE_TYPE wHue, TALKMODE_TYPE mode ) const
{
	// Show everyone a msg coming form this object.

	for ( CClientPtr pClient = g_Serv.GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
	{
		if ( pClient == pClientExclude )
			continue;
		if ( ! pClient->CanSee( this ))
			continue;

		pClient->addBark(( pClient->GetChar() == this )? pTextYou : pTextThem, this, wHue, mode, FONT_NORMAL );
	}
}

void CObjBase::UpdateCanSee( const CUOCommand* pCmd, int iLen, CClient* pClientExclude ) const
{
	// Send this update message to everyone who can see this.
	// NOTE: Need not be a top level object. CanSee() will calc that.
	for ( CClientPtr pClient = g_Serv.GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
	{
		if ( pClient == pClientExclude )
			continue;
		if ( ! pClient->CanSee( this ))
			continue;
		pClient->xSendPkt( pCmd, iLen );
	}
}

TRIGRET_TYPE CObjBase::OnHearTrigger( CResourceLink* pLink, CSphereExpArgs& exec )
{
	// Check all the keys in this script section.
	// look for pattern or partial trigger matches.
	// RETURN:
	//  TRIGRET_ENDIF = no match.
	//  TRIGRET_DEFAULT = found match but it had no RETURN

	if ( CSphereUID( pLink->GetUIDIndex()).GetResType() != RES_Speech )
		return( TRIGRET_ENDIF );

	CResourceLock s(pLink);
	if ( ! s.IsFileOpen())
		return( TRIGRET_ENDIF );

	bool fMatch = false;

	while ( s.ReadKeyParse())
	{
		if ( s.IsLineTrigger())
		{
			// Look for some key word match.
			_strupr( s.GetArgMod());
			if ( Str_Match( s.GetArgRaw(), exec.m_s1 ) == MATCH_VALID )
				fMatch = true;
			continue;
		}

		if ( ! fMatch )
			continue;	// look for the next "ON" section.

		TRIGRET_TYPE iRet = exec.ExecuteScript( s, TRIGRUN_SECTION_EXEC );
		if ( iRet != TRIGRET_RET_FALSE )
			return( iRet );

		fMatch = false;
	}

	return( TRIGRET_ENDIF );	// continue looking.
}

HRESULT CObjBase::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	DEBUG_CHECK(pSrc);

	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return(	CObjBaseTemplate::s_PropGet( pszKey, vValRet, pSrc ));
	}

	switch (iProp)
	{
	case P_Tag:
		return(HRES_INVALID_FUNCTION);
	case P_Events:
		m_Events.v_Get( vValRet );
		break;
	case P_BaseID:
		vValRet = GetResourceName();
		break;
	case P_Create:
		vValRet.SetDWORD( m_timeCreate.GetTimeRaw());
		break;
	case P_Age:
		vValRet.SetInt( m_timeCreate.GetCacheAge() / TICKS_PER_SEC );
		break;
	case P_Changer:
		vValRet.SetUID( m_uidChanger );
		break;
	case P_Def:
	case P_TypeDef:
		vValRet.SetRef( Base_GetDef());
		break;
	case P_Sector:
		vValRet.SetRef( GetTopLevelObj()->GetTopSector());
		break;
	case P_TopObj:
		vValRet.SetRef( GetTopLevelObj());
		break;
	case P_Color:
		vValRet.SetDWORD( GetHue());
		break;
	case P_IsChar:
		vValRet.SetBool( IsChar());
		break;
	case P_IsItem:
		vValRet.SetBool( IsItem());
		break;
	case P_MapPlane:
		vValRet.SetInt( GetUnkPoint().m_mapplane );
		break;
	case P_Name:
		vValRet = GetName();
		break;
	case P_P:
		GetUnkPoint().v_Get(vValRet);
		break;
	case P_P_X:
		vValRet = GetUnkPoint().m_x;
		break;
	case P_P_Y:
		vValRet = GetUnkPoint().m_y;
		break;
	case P_Z:
	case P_P_Z:
		vValRet.SetInt( GetUnkZ());
		break;
	case P_Timer:
		vValRet.SetInt( GetTimerAdjusted());
		break;
	case P_TimerD:
		vValRet.SetInt( GetTimerAdjustedD());
		break;
	case P_Serial:
		vValRet.SetUID( GetUID());
		break;
	case P_Weight:
		vValRet.SetInt( GetWeight());
		break;
	case P_Value:
		vValRet.SetInt( GetMakeValue());
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CObjBase::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	// load the basic stuff.

	s_FixExtendedProp( pszKey, "Tag", vVal );

	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CObjBaseTemplate::s_PropSet(pszKey,vVal));
	}

	switch ( iProp )
	{
	case P_Events:
		// ??? Check that it is not already part of Char_GetDef()->m_Events
		if ( g_World.m_iLoadVersion < 53 )
		{
			break;
		}
		if ( ! m_Events.v_Set( vVal, RES_Events ))
			return(HRES_BAD_ARGUMENTS);
		break;
	case P_Tag:
		return( m_TagDefs.s_PropSetTags( vVal ));
	case P_BaseID:
		return( HRES_WRITE_FAULT );
	case P_Create:
		if ( ! g_Serv.IsLoading())	// only set on load.
			return( HRES_PRIVILEGE_NOT_HELD );
		m_timeCreate.InitTime( vVal.GetDWORD());
		break;
	case P_Age:
		if ( ! g_Serv.IsLoading())	// only set on load.
			return( HRES_PRIVILEGE_NOT_HELD );
		m_timeCreate.InitTimeCurrent( - (long)( vVal.GetDWORD() * TICKS_PER_SEC ));
		break;
	case P_Changer:
		if ( ! g_Serv.IsLoading())	// only set (this way) on load.
			return( HRES_PRIVILEGE_NOT_HELD );
		m_uidChanger = vVal.GetUID();
		break;
	case P_Color:
		if ( ! _stricmp( vVal.GetPSTR(), "match_shirt" ) ||
			! _stricmp( vVal.GetPSTR(), "match_hair" ))
		{
			CCharPtr pChar = REF_CAST(CChar,GetTopLevelObj());
			if ( pChar )
			{
				CItemPtr pItem = pChar->LayerFind( ! _stricmp( vVal.GetPSTR()+6, "shirt" ) ? LAYER_SHIRT : LAYER_HAIR );
				if ( pItem )
				{
					m_wHue = pItem->GetHue();
					break;
				}
			}
			m_wHue = HUE_GRAY;
			break;
		}
		RemoveFromView();
		m_wHue = (HUE_TYPE) vVal.GetInt();
		Update();
		break;
	case P_MapPlane:
		// Move to another map plane.
		if ( ! IsTopLevel())
			return( HRES_INVALID_HANDLE );
		{
			MAPPLANE_TYPE iNewMapPlane = vVal.GetInt();
			if ( iNewMapPlane == GetTopMap())
				break;
			RemoveFromView();
			CPointMap pt = GetTopPoint();
			pt.m_mapplane = iNewMapPlane;
			if ( ! MoveTo(pt))
				return(HRES_INVALID_HANDLE);
			Update();
		}
		break;
	case P_Name:
		SetName( vVal.GetPSTR());
		break;
	case P_P:	// Must set the point via the CItem or CChar methods.
		return( HRES_INVALID_FUNCTION );
	case P_Timer:
		if ( g_World.m_iLoadVersion < 34 )
		{
			m_timeout.InitTime( vVal.GetInt());
		}
		else
		{
			SetTimeout( vVal.GetInt() * TICKS_PER_SEC );
		}
		break;
	case P_TimerD:
		SetTimeout( vVal.GetInt());
		break;

	case P_Serial:
		// set the UID serial number, (cannot move uid already set)
		// Only allow this when world is loading !
		if ( ! g_Serv.IsLoading())
			return HRES_INVALID_HANDLE ;
		if ( IsValidUID())
			return HRES_INVALID_INDEX;
		{
			DWORD dwUID = vVal.GetInt();
			if ( IsItem())
			{
				dwUID |= UID_F_ITEM;
			}
			if ( g_World.LoadUID( dwUID, this ) == 0 )
				return HRES_INVALID_INDEX;
		}
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}

void CObjBase::s_Serialize( CGFile& a )
{
	// Read and write binary.
}

void CObjBase::s_WriteProps( CScript& s )
{
	s.WriteKeyDWORD( "SERIAL", GetUID());
	if ( IsIndividualName())
		s.WriteKey( "NAME", GetIndividualName());
	if ( m_wHue != HUE_DEFAULT )
		s.WriteKeyDWORD( "COLOR", GetHue());
	if ( m_timeout.IsTimeValid() )
		s.WriteKeyInt( "TIMER", GetTimerAdjusted());
	s.WriteKeyDWORD( "AGE", m_timeCreate.GetCacheAge() / TICKS_PER_SEC );
	m_TagDefs.s_WriteTags(s,NULL);
	m_Events.s_WriteProps( s, "EVENTS" );
}

HRESULT CObjBase::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp < 0 )
	{
		// Not found.
		return( CObjBaseTemplate::s_Method( pszKey, vArgs, vValRet, pSrc ));
	}

	ASSERT(pSrc);
	CCharPtr pCharSrc = GET_ATTACHED_CCHAR(pSrc);
	CClientPtr pClientSrc = (pCharSrc && pCharSrc->IsClient()) ? (CClient*)(pCharSrc->GetClient()) : (CClient*)NULL ;

	switch (iProp)
	{
	case M_IsEvent:
		vValRet.SetBool( m_Events.FindResourceID( vArgs.GetUID()) >= 0 );
		break;
	case M_Tag:
		return( m_TagDefs.s_MethodTags( vArgs, vValRet, pSrc ));

	case M_Distance: // Distance from source to this.
	case M_DistanceFrom: // DISTANCEFROM(uid)
		// Distance from source to this.
		{
			CObjBasePtr pObj;
			if (vArgs.IsEmpty())
				pObj = pCharSrc;
			else
				pObj = g_World.ObjFind(vArgs.GetUID()); 
			if ( pObj == NULL )
				return( HRES_INVALID_INDEX );
			vValRet.SetInt( GetDist( pObj ));
		}
		break;

	case M_IsNearType: // ISNEARTYPE(type,dist)
		{
			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty < 1 )
				return HRES_BAD_ARG_QTY;
			vValRet.SetBool( g_World.IsItemTypeNear( GetTopLevelObj()->GetTopPoint(), (IT_TYPE) vArgs.GetArrayInt(0), vArgs.GetArrayInt(1)));
		}
		break;
	case M_Hit:	//	"Amount,SourceFlags,SourceCharUid" = do me some damage.
		{
			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty < 1 )
				return( HRES_BAD_ARG_QTY );
			if ( iArgQty > 2 )	// Give it a new source char UID
			{
				CObjBasePtr pObj = g_World.ObjFind( vArgs.GetArrayElement(2).GetUID());
				if ( pObj )
				{
					pObj = pObj->GetTopLevelObj();
				}
				pCharSrc = REF_CAST(CChar,pObj);
			}
			OnGetHit( vArgs.GetArrayInt(0),
				pCharSrc,
				( iArgQty > 1 ) ? vArgs.GetArrayInt(1) : ( DAMAGE_HIT_BLUNT | DAMAGE_GENERAL ));
		}
		break;

	case M_Damage:	//	"Amount,SourceFlags,SourceCharUid" = do me some damage.
		{
			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty < 1 )
				return( HRES_BAD_ARG_QTY );
			if ( iArgQty > 2 )	// Give it a new source char UID
			{
				CSphereUID uid( vArgs.GetArrayInt(2) );
				CObjBasePtr pObj = g_World.ObjFind(uid);
				if ( pObj )
				{
					pObj = pObj->GetTopLevelObj();
				}
				pCharSrc = REF_CAST(CChar,pObj);
			}
			OnTakeDamage( vArgs.GetArrayInt(0),
				pCharSrc,
				( iArgQty > 1 ) ? vArgs.GetArrayInt(1) : ( DAMAGE_HIT_BLUNT | DAMAGE_GENERAL ));
		}
		break;
	case M_Effect: // some visual effect.
		{
			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty < 2 )
				return( HRES_BAD_ARG_QTY );

			Effect( (EFFECT_TYPE) vArgs.GetArrayInt(0), 
				(ITEMID_TYPE) RES_GET_INDEX(vArgs.GetArrayInt(1)),
				pCharSrc,
				(iArgQty>=3)? vArgs.GetArrayInt(2) : 5, // BYTE bSpeedSeconds = 5,
				(iArgQty>=4)? vArgs.GetArrayInt(3) : 1, // BYTE bLoop = 1,
				(iArgQty>=5)? vArgs.GetArrayInt(4) : false // bool fExplode = false
				);
		}
		break;
	case M_Emote:
		Emote( vArgs.GetPSTR());
		break;
	case M_Flip:
		Flip( vArgs.GetPSTR());
		break;

	case M_M:
	case M_Move:
		// move without restriction. east,west,etc. (?up,down,)
		if ( IsTopLevel())
		{
			CPointMap pt = GetTopPoint();
			if ( ! GetDeltaStr( pt, vArgs ))
				return( HRES_BAD_ARGUMENTS );
			MoveTo( pt );
			Update();
		}
		break;
	case M_NudgeDown:
		if ( IsTopLevel())
		{
			int zdiff = vArgs.GetInt();
			SetTopZ( GetTopZ() - ( zdiff ? zdiff : 1 ));
			Update();
		}
		break;
	case M_NudgeUp:
		if ( IsTopLevel())
		{
			int zdiff = vArgs.GetInt();
			SetTopZ( GetTopZ() + ( zdiff ? zdiff : 1 ));
			Update();
		}
		break;
	case M_MoveTo:
	case M_P:
		if ( vArgs.IsEmpty())
		{
			GetUnkPoint().v_Get(vValRet);
		}
		else
		{
			MoveTo( g_Cfg.GetRegionPoint( vArgs ));
		}
		break;

	case M_Remove:	// remove this object now.
		DeleteThis();
		break;
	case M_RemoveFromView:
		RemoveFromView();	// remove this item from all clients.
		break;
	case M_Say:
	case M_Speak:	// speak so everyone can here
		Speak( vArgs.GetPSTR());
		break;

	case M_SayU:
	case M_SpeakU:
		// Speak in unicode from the UTF8 system format.
		SpeakUTF8( vArgs.GetPSTR(), HUE_TEXT_DEF, TALKMODE_SAY, FONT_NORMAL );
		break;

	case M_SayUA:
	case M_SpeakUA:
		// This can have full args. SAYUA Color, Mode, Font, Lang, Text Text
		{
			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty < 1 )
				return HRES_BAD_ARG_QTY;
			SpeakUTF8( vArgs.GetArrayPSTR(4),
				(HUE_TYPE) ( (iArgQty>0) ? vArgs.GetArrayInt(0) : HUE_TEXT_DEF ),
				(TALKMODE_TYPE) ( (iArgQty>1) ? vArgs.GetArrayInt(1) : TALKMODE_SAY ),
				(FONT_TYPE) ( (iArgQty>2) ? vArgs.GetArrayInt(2) : FONT_NORMAL ),
				CLanguageID(vArgs.GetArrayPSTR(3)));
		}
		break;

	case M_Sfx:
	case M_Sound:
		{
			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty < 1 )
				return HRES_BAD_ARG_QTY;
			Sound( vArgs.GetArrayInt(0), ( iArgQty > 1 ) ? vArgs.GetArrayInt(1) : 1 );
		}
		break;
	case M_SpellEffect:	// spell, strength, noresist
		{
			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty < 1 )
				return HRES_BAD_ARG_QTY;
			if ( vArgs.GetArrayInt(2) == 1 )
			{
				pCharSrc = PTR_CAST(CChar,this);
			}
			OnSpellEffect( (SPELL_TYPE) RES_GET_INDEX( vArgs.GetArrayInt(0) ), pCharSrc, vArgs.GetArrayInt(1), NULL );
		}
		break;

	case M_Update:
		Update();
		break;
	case M_UpdateX:
		// Some things like equipped items need to be removed before they can be updated !
		UpdateX();
		break;

	case M_Fix:
		vArgs.SetVoid();

	case M_Z:	//	ussually in "SETZ" form
		if ( IsItemEquipped())
			return( HRES_INVALID_HANDLE );
		if ( !vArgs.IsEmpty())
		{
			SetUnkZ( vArgs.GetInt());
		}
		else if ( IsTopLevel())
		{
			CMulMapBlockState block( CAN_C_WALK | CAN_C_CLIMB );
			g_World.GetHeightPoint( GetTopPoint(), block, GetTopRegion(REGION_TYPE_MULTI));
			SetTopZ( block.GetResultZ());
		}
		Update();
		break;

		//*********************************************
		// SRC is effected.

	case M_SrcCanSee:		// Was "CanSee"
		{
			if ( pCharSrc == NULL )
				return( HRES_PRIVILEGE_NOT_HELD );
			vValRet.SetBool( pCharSrc->CanSee( this ));
		}
		break;
	case M_SrcCanSeeLOS:	// Was "CanSeeLOS"
		{
			if ( pCharSrc == NULL )
				return( HRES_PRIVILEGE_NOT_HELD );
			vValRet.SetBool( pCharSrc->CanSeeLOS( this ));
		}
		break;
	case M_SrcFollow:	// Was "Followed"
		// Follow this item.
		if ( pCharSrc )
		{
			// I want to follow this person about.
			pCharSrc->Memory_AddObjTypes( this, MEMORY_FOLLOW );
		}
		break;

		//*****************************
		// SRC dialogs.

	case M_ColorDlg:
		if ( pClientSrc == NULL )
			return HRES_PRIVILEGE_NOT_HELD;
		// Ask what color you want?
		pClientSrc->addDyeOption( this );
		break;
	case M_Edit:
		{
			// Put up a list of items in the container. (if it is a container)
			if ( pClientSrc == NULL )
				return( HRES_PRIVILEGE_NOT_HELD );
			pClientSrc->m_Targ.m_sText = vArgs.GetStr();
			pClientSrc->Cmd_EditItem( this, -1 );
		}
		break;
	case M_InpDlg:
		// "INPDLG" verb maxchars
		// else assume it was a property button.
		{
			if ( pClientSrc == NULL )
				return( HRES_PRIVILEGE_NOT_HELD );

			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty < 2 )
				return HRES_BAD_ARG_QTY;
			pClientSrc->m_Targ.m_sText = vArgs.GetArrayStr(0);	// The attribute we want to edit.

			if ( s_PropGet( pClientSrc->m_Targ.m_sText, vValRet, pSrc ) != NO_ERROR )
				vValRet = ".";

			int iMaxLength = vArgs.GetArrayInt(1);

			CGString sPrompt;
			sPrompt.Format( "%s (# = default)", (LPCTSTR) pClientSrc->m_Targ.m_sText );
			pClientSrc->addGumpInpVal( true, INPVAL_STYLE_TEXTEDIT,
				iMaxLength,	sPrompt, vValRet, this );
			// Should result in Event_GumpInpValRet
		}
		break;
	case M_Menu:
		{
			if ( pClientSrc == NULL )
				return( HRES_PRIVILEGE_NOT_HELD );
			pClientSrc->Menu_Setup( vArgs.GetUID(), this );
		}
		break;
	case M_Message:	// put info message (for pSrc client only) over item.
		{
			if ( pCharSrc == NULL )
			{
				UpdateObjMessage( vArgs, vArgs, NULL, HUE_TEXT_DEF, IsChar() ? TALKMODE_EMOTE : TALKMODE_ITEM );
			}
			else
			{
				pCharSrc->ObjMessage( vArgs, this );
			}
		}
		break;
	case M_Props:
	case M_Tweak:
	case M_Info:
		if ( ! pClientSrc )
			return(HRES_PRIVILEGE_NOT_HELD);
		if ( ! pClientSrc->addGumpDialogProps( GetUID()))
			return(HRES_BAD_ARGUMENTS);
		break;

	case M_Target:
	case M_TargetG:
		{
			if ( pClientSrc == NULL )
				return( HRES_PRIVILEGE_NOT_HELD);
			pClientSrc->m_Targ.m_UID = GetUID();
			pClientSrc->m_Targ.m_tmUseItem.m_pParent = GetParent();	// Cheat Verify
			pClientSrc->addTarget( CLIMODE_TARG_USE_ITEM, vArgs, iProp == M_TargetG, true );
		}
		break;

	case M_Dialog:
		{
			if ( pClientSrc == NULL )
				return( HRES_PRIVILEGE_NOT_HELD );
			if ( ! pClientSrc->Dialog_Setup( CLIMODE_DIALOG, vArgs.GetUID(), this ))
				return(HRES_BAD_ARGUMENTS);
		}
		break;

	case M_TryP:
		if ( pClientSrc == NULL )
			return( HRES_PRIVILEGE_NOT_HELD );
		{
			int iMinPriv = vArgs.GetArrayInt(0);

			if ( iMinPriv >= PLEVEL_QTY )
			{
				pSrc->Printf("The %s property can't be changed.", (LPCTSTR) vArgs.GetPSTR());
				return( HRES_INVALID_FUNCTION );
			}
			if ( pSrc->GetPrivLevel() < iMinPriv )
			{
				pSrc->Printf( "You lack the privilege to change the %s property.", (LPCTSTR) vArgs.GetPSTR());
				return( HRES_PRIVILEGE_NOT_HELD );
			}
			vArgs.RemoveArrayElement(0);
		}
		// fall thru...

	case M_Try:
		// do this verb only if we can touch it.
		if ( pClientSrc == NULL )
			return( HRES_PRIVILEGE_NOT_HELD );
		pClientSrc->OnTarg_Obj_Command( this, vArgs.GetPSTR());
		break;

	case M_UseItem:
	case M_DClick:
		if ( ! pCharSrc )
			return( HRES_PRIVILEGE_NOT_HELD );
		if ( ! pCharSrc->Use_Obj( this, ( iProp == M_DClick )))
			return( HRES_BAD_ARGUMENTS );
		break;

	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

void CObjBase::RemoveFromView( CClient* pClientExclude )
{
	// Remove this item from all clients.
	// In a destructor this can do funny things.

	if ( IsDisconnected())
		return;	// not in the world.

	CObjBasePtr pObjTop = GetTopLevelObj();

	for ( CClientPtr pClient = g_Serv.GetClientHead(); pClient; pClient = pClient->GetNext())
	{
		if ( pClientExclude == pClient )
			continue;
		CCharPtr pChar = pClient->GetChar();
		if ( pChar == NULL )
			continue;
		// Too far away to care ?
		if ( pChar->GetTopDist( pObjTop ) > SPHEREMAP_VIEW_RADAR )
			continue;
		pClient->addObjectRemove( this );
	}
}

void CObjBase::SetChangerSrc( CScriptConsole* pSrc )
{
	// Who changed the props on this last ? m_uidChanger
	if ( pSrc == NULL )
		return;
	if ( pSrc == &g_Serv )	// never set to this.
		return;
	CAccountConsole* pConsole = dynamic_cast <CAccountConsole*>(pSrc);
	if ( pConsole == NULL )
		return;
	CAccountPtr pAccount = pConsole->GetAccount();
	if ( pAccount == NULL )
		return;
	m_uidChanger = pAccount->GetUIDIndex();
}

void CObjBase::DeleteThis()
{
	// Do this before the actual destructor.
	// Because in the destruct the virtuals will most likely not work anyhow.

	if ( sm_fDeleteReal )
	{
		// ASSERT( GetParent() == NULL || GetParent() == &(g_World.m_ObjDelete) || Closing );
		RemoveSelf();	// Must remove early or else virtuals will fail.
	}
	else
	{
		if ( GetParent() == &(g_World.m_ObjDelete))	// already been deleted
			return;
		CObjBasePtr temp(this);	// make sure we do not destruct yet
		RemoveFromView();
		// free up the UID slot.
		g_World.FreeUID(this);
		g_World.m_ObjDelete.InsertHead(this); // just get added to the list of stuff to delete later.
	}
}


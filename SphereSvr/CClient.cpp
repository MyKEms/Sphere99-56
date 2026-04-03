//
// CClient.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
#include "stdafx.h"	// predef header.
#include "spherelog.h"
#ifndef _WIN32
#include <netinet/tcp.h>
#endif

static void s_CombineKeys(TCHAR* pszOut, LPCTSTR pszKey, LPCTSTR pszArg) {
	sprintf(pszOut, "%s.%s", pszKey, pszArg ? pszArg : "");
}
static void s_DumpHelp(CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc) {
	// STUB
}

/////////////////////////////////////////////////////////////////
// -CClient stuff.

const CScriptProp CClient::sm_Props[CClient::P_QTY+1] = // static
{
#define CCLIENTPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "cclientprops.tbl"
#undef CCLIENTPROP
	NULL,			
};

#ifdef USE_JSCRIPT
#define CCLIENTMETHOD(a,b,c) JSCRIPT_METHOD_IMP(CClient,a)
#include "cclientmethods.tbl"
#undef CCLIENTMETHOD
#endif

const CScriptMethod CClient::sm_Methods[CClient::M_QTY+1] =	// static
{
#define CCLIENTMETHOD(a,b,c) CSCRIPT_METHOD_IMP(a,b,c)
#include "cclientmethods.tbl"
#undef CCLIENTMETHOD
	NULL,
};

CSCRIPT_CLASS_IMP1(Client,CClient::sm_Props,CClient::sm_Methods,NULL,ResourceObj);

CClient::CClient( SOCKET client ) :
	CResourceObj( client )
{
	// This may be a web connection or Telnet ?

	m_Socket.Attach( client );
	m_ConnectType = CONNECT_UNK;	// don't know what sort of connect this is yet.
	m_Crypt.SetCryptVerEnum( g_Serv.m_ClientVersion.GetCryptVer() );
	m_ProtoVer = m_Crypt;

	m_timeLastSend.InitTimeCurrent();
	m_timeLastEvent.InitTimeCurrent();
	m_timeLastDispatch.InitTimeCurrent();

	m_bin_PrvMsg = XCMD_QTY;
	m_bin_msg_len = 0;
	m_fClientVer3d = false;
	m_iClientResourceLevel = 0;   // 0=base, 2=T2A, 3=3rd dawn, 4=Black

	m_wWalkCount = -1;
	m_iWalkStepCount = 0;
	m_fPaused = false;
	m_wHueText = HUE_TEXT_DEF;

	m_Targ.m_Mode = CLIMODE_SETUP_CONNECTING;
	m_Targ.m_tmSetup.m_dwCryptKey = 0;
	m_Targ.m_tmSetup.m_iConnect = 0;

	m_Env.SetInvalid();

	g_Serv.StatInc( SERV_STAT_CLIENTS );
	g_Serv.ClientsAvgCalc();
	g_Serv.m_Clients.InsertHead( this );

	CSocketAddress PeerName = m_Socket.GetPeerName();
	SPHERE_LOG_NET("CClient::CClient constructed sock=%d peer=%s total=%d", m_Socket.GetSocket(), (LPCTSTR) PeerName.GetAddrStr(), g_Serv.StatGet( SERV_STAT_CLIENTS ));

#ifdef _WIN32
	DWORD lVal = 1;	// 0 =  block
	bool fRet = m_Socket.IOCtl( FIONBIO, &lVal );
	DEBUG_CHECK( fRet );
#endif
	if ( g_Serv.m_uSizeMsgMax < UO_MAX_EVENT_BUFFER )
	{
		// UO_MAX_EVENT_BUFFER for outgoing buffers ?
	}

	// disable NAGLE algorythm for data compression/coalescing. Send as fast as we can. we handle packing ourselves.
	BOOL nbool=TRUE;
	m_Socket.SetSockOpt( TCP_NODELAY, &nbool, sizeof(BOOL), IPPROTO_TCP );

	DEBUG_CHECK( g_Serv.StatGet( SERV_STAT_CLIENTS ) == g_Serv.m_Clients.GetCount());
}

CClient::~CClient()
{
	m_Socket.Close();
	g_Serv.StatDec( SERV_STAT_CLIENTS );
}

void CClient::DeleteThis()	
{
	CSocketAddress PeerName = m_Socket.GetPeerName();
	if ( ! PeerName.IsSameIP( g_Cfg.m_RegisterServer ))
	{
		g_Log.Event( LOG_GROUP_CLIENTS, LOGL_TRACE, "%x:Client disconnected [Total:%i]" LOG_CR, m_Socket.GetSocket(), g_Serv.StatGet(SERV_STAT_CLIENTS)-1 );
	}

	CharDisconnect();	// am i a char in game ?
	Cmd_GM_PageClear();

	if ( m_pAccount )
	{
		m_pAccount->OnLogout( this );
		if ( m_pAccount->IsPrivFlag(PRIV_TEMPORARY) || m_pAccount->GetPrivLevel() >= PLEVEL_QTY ) // kill the temporary account.
		{
			m_pAccount->DeleteThis();
		}
	}

	xFlush();

	RemoveSelf();	// remove myself from my parent list.
}

bool CClient::CanInstantLogOut() const
{
	// Should I log out now or linger?
	if ( g_Serv.IsLoading())	// or exiting.
		return( true );
	if ( ! g_Cfg.m_iClientLingerTime )
		return true;
	if ( IsPrivFlag( PRIV_GM ))
		return true;
	if ( m_pChar == NULL )
		return( true );
	if ( m_pChar->IsStatFlag(STATF_DEAD))
		return( true );
	if ( m_pChar->IsStatFlag(STATF_Stone|STATF_Sleeping))
		return( false );

	const CRegionComplex* pArea = m_pChar->GetArea();
	if ( pArea == NULL )
		return( true );
	if ( pArea->IsFlag( REGION_FLAG_INSTA_LOGOUT ))
		return( true );

#if 0
	// Camp near by ? have we been standing near it long enough ?
	if ( pArea->IsFlagGuarded() && ! m_pChar->IsStatFlag( STATF_Criminal ))
		return( true );
#endif

	return( false );
}

void CClient::CharDisconnect()
{
	// Disconnect the CChar from the client.
	// Even tho the CClient might stay active.
	if ( ! m_pChar)
		return;

	Announce( false );
	bool fCanInstaLogOut = CanInstantLogOut();
	m_pChar->ClientDetach();	// we are not a client any more.

	CSphereExpContext se( m_pChar, m_pChar);
	TRIGRET_TYPE iRet = m_pChar->OnTrigger( CCharDef::T_LogOut, se);
	if ( iRet == TRIGRET_RET_VAL )
		fCanInstaLogOut	= false;
   	else if ( iRet == TRIGRET_RET_FALSE )
		fCanInstaLogOut	= true;

	// log out immediately ? (test before ClientDetach())
	if ( ! fCanInstaLogOut )
	{
		// become an NPC for a little while
		CItemPtr pItemChange = CItem::CreateBase( ITEMID_RHAND_POINT_W );
		ASSERT(pItemChange);
		pItemChange->SetType( IT_EQ_CLIENT_LINGER );
		pItemChange->SetTimeout( g_Cfg.m_iClientLingerTime );
		m_pChar->LayerAdd( pItemChange, LAYER_FLAG_ClientLinger );
	}
	else
	{
		// remove me from other clients screens now.
		m_pChar->SetDisconnected();
	}

	m_pChar = NULL;
}

void CClient::SetMessageColorType( int iMsgColorType )
{
	// Colorize the text depending on the message type.
	m_wHueText = iMsgColorType;	// ???
}

bool CClient::WriteString( LPCTSTR pszMsg ) // System message (In lower left corner)
{
	// Diff sorts of clients.

	switch (m_ConnectType )
	{
	case CONNECT_TELNET:
		{
			// just dump raw text out.
			if ( pszMsg == NULL || ISINTRESOURCE(pszMsg))
				return false;
			int iLen = 0;
			for ( ; *pszMsg != '\0'; pszMsg++ )
			{
				if ( *pszMsg == '\n' || ( pszMsg[0] == '\r' && pszMsg[1] != '\n' ))	// translate.
				{
					xSendReady( pszMsg-iLen, iLen );
					iLen = 0;
					xSendReady( "\r\n", 2 );
				}
				else
					iLen ++;
			}
			xSendReady( pszMsg-iLen, iLen );
		}
		return true;

	case CONNECT_HTTP:
		// Write out to temporary space since all HTTP pages must have "Content-Length"
		m_bin.AddNewData( (const BYTE*) pszMsg, strlen(pszMsg));
		return true;

	case CONNECT_CRYPT:
	case CONNECT_LOGIN:
	case CONNECT_GAME:
		addSysMessage( pszMsg, m_wHueText );
		return true;

		// else toss it.?
	}
	return false;
}

void CClient::Announce( bool fArrive ) const
{
	if ( GetAccount() == NULL )
		return;
	if ( GetChar() == NULL || 
		! GetChar()->m_pPlayer )
	{
		ASSERT( GetChar() != NULL );
		ASSERT( GetChar()->m_pPlayer );
		return;
	}

	// We have logged in or disconnected.
	// Annouce my arrival or departure.
	if ( g_Cfg.m_fArriveDepartMsg )
	{
		CGString sMsg;
		for ( CClientPtr pClient = g_Serv.GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
		{
			if ( pClient == this )
				continue;
			CCharPtr pChar = pClient->GetChar();
			if ( pChar == NULL )
				continue;
			if ( GetPrivLevel() > pClient->GetPrivLevel())
				continue;
			if ( ! pClient->IsPrivFlag( PRIV_DETAIL|PRIV_HEARALL ))
				continue;
			if ( sMsg.IsEmpty())
			{
				CRegionPtr pRegion = m_pChar->GetTopRegion( REGION_TYPE_AREA );
				sMsg.Format( _TEXT( "%s has %s %s." ),
					(LPCTSTR) m_pChar->GetName(),
					(fArrive)? _TEXT("arrived in") : _TEXT("departed from"),
					pRegion ? (LPCTSTR) pRegion->GetName() : (LPCTSTR) g_Serv.GetName());
			}
			pClient->WriteString( sMsg );
		}
	}

	// re-Start murder decay if arriving

	if ( m_pChar->m_pPlayer->m_wMurders )
	{
		CItemPtr pMurders = m_pChar->LayerFind(LAYER_FLAG_Murders);
		if ( pMurders )
		{
			if ( fArrive )
			{
				// If the Memory exists, put it in the loop
				pMurders->SetTimeout( pMurders->m_itEqMurderCount.m_Decay_Balance );
			}
			else
			{
				// Or save decay point if departing and remove from the loop
				pMurders->m_itEqMurderCount.m_Decay_Balance = pMurders->GetTimerAdjusted();
				pMurders->SetTimeout(-1); // turn off the timer til we log in again.
			}
		}
		else if ( fArrive )
		{
			// If not, see if we need one made
			m_pChar->Noto_Murder();
		}
	}

	if ( m_pChar != NULL )
	{
		m_pAccount->m_uidLastChar = m_pChar->GetUID();
	}
}

////////////////////////////////////////////////////

bool CClient::CanSee( const CObjBaseTemplate* pObj ) const
{
	// Can player see pObj
	if ( m_pChar == NULL )
		return( false );
	return( m_pChar->CanSee( pObj ));
}

bool CClient::CanHear( const CObjBaseTemplate* pSrc, TALKMODE_TYPE mode ) const
{
	// can we hear this text or sound.

	if ( ! IsConnectTypePacket())
	{
		if ( m_ConnectType != CONNECT_TELNET )
			return( false );
		if ( mode == TALKMODE_BROADCAST ) // && GetAccount()
			return( true );
		return( false );
	}

	if ( mode == TALKMODE_BROADCAST || pSrc == NULL )
		return( true );
	if ( m_pChar == NULL )
		return( false );

	if ( IsPrivFlag( PRIV_HEARALL ) &&
		pSrc->IsChar() &&
		( mode == TALKMODE_SYSTEM || mode == TALKMODE_SAY || mode == TALKMODE_WHISPER || mode == TALKMODE_YELL ))
	{
		CCharPtr pCharSrc = PTR_CAST(CChar,const_cast<CObjBaseTemplate*>(pSrc));
		ASSERT(pCharSrc);
		if ( pCharSrc && pCharSrc->IsClient())
		{
			if ( pCharSrc->GetPrivLevel() <= GetPrivLevel())
			{
				return( true );
			}
		}
		// Else it does not apply.
	}

	return( m_pChar->CanHear( pSrc, mode ));
}

////////////////////////////////////////////////////

bool CClient::addTargetVerb( LPCTSTR pszCmd, LPCTSTR pszArg )
{
	// Target a verb at some object .

	if ( m_pChar == NULL || pszCmd == NULL )
		return false;

	TCHAR szTmp[CSTRING_MAX_LEN];
	s_CombineKeys(szTmp,pszCmd,pszArg);
	m_Targ.m_sText = szTmp;

	if ( ! g_Cfg.CanUsePrivVerb( m_pChar, szTmp, this ))
	{
		WriteString( "You can't use this command." );
		return false;
	}

	CGString sPrompt;
	sPrompt.Format( "Select object to set/command '%s'", (LPCTSTR) m_Targ.m_sText );
	return addTarget( CLIMODE_TARG_OBJ_SET, sPrompt );
}

HRESULT CClient::s_PropGet( int iProp, CGVariant& vValRet, CScriptConsole* pSrc )
{
	switch (iProp)
	{
	case P_Account:
		vValRet.SetRef( GetAccount());
		break;
	case P_GMPageP:
		vValRet.SetRef( m_pGMPage );
		break;
	case P_TargUID:
		vValRet.SetUID( m_Targ.m_UID );
		break;
	case P_T:
	case P_Targ:
		vValRet.SetRef( g_World.ObjFind(m_Targ.m_UID));
		break;
	case P_TargPrv:
	case P_TPrv:
		vValRet.SetRef( g_World.ObjFind(m_Targ.m_PrvUID));
		break;
	case P_TargProp:
	case P_TProp:
		vValRet.SetRef( g_World.ObjFind(m_Prop_UID));
		break;
	case P_AllMove:
		vValRet.SetBool( IsPrivFlag( PRIV_ALLMOVE ));
		break;
	case P_AllShow:
		vValRet.SetBool( IsPrivFlag( PRIV_ALLSHOW ));
		break;
	case P_Client3D:
		vValRet.SetBool( m_fClientVer3d );
		break;
	case P_ClientRes:
		vValRet.SetInt( m_iClientResourceLevel );
		break;
	case P_ClientCrypt:
		{
			TCHAR szVersion[ 128 ];
			vValRet = m_Crypt.WriteCryptVer( szVersion );
		}
		break;
	case P_ClientVer:
		{
			TCHAR szVersion[ 128 ];
			vValRet = m_ProtoVer.WriteCryptVer( szVersion );
		}
		break;
	case P_Debug:
		vValRet.SetBool( IsPrivFlag( PRIV_DEBUG ));
		break;
	case P_Detail:
		vValRet.SetBool( IsPrivFlag( PRIV_DETAIL ));
		break;
	case P_GM:	// toggle your GM status on/off
		vValRet.SetBool( IsPrivFlag( PRIV_GM ));
		break;
	case P_HearAll:
	case P_Listen:
		vValRet.SetBool( IsPrivFlag( PRIV_HEARALL ));
		break;
	case P_PrivHide:
		// Hide my priv title.
		vValRet.SetBool( IsPrivFlag( PRIV_PRIV_HIDE ));
		break;
	case P_PrivShow:
		// Show my priv title.
		vValRet.SetBool( ! IsPrivFlag( PRIV_PRIV_HIDE ));
		break;
	case P_TargP:
		m_Targ.m_pt.v_Get(vValRet);
		break;
	case P_TargTxt:
		vValRet = m_Targ.m_sText;
		break;
	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CClient::s_PropSet( int iProp, CGVariant& vVal )
{
	CAccountPtr pAccount = GetAccount();
	if ( ! pAccount )
	{
		switch(iProp)
		{
		case P_TargTxt: // this is the only thing we can do.
			break;
		default:
			return( HRES_PRIVILEGE_NOT_HELD );
		}
	}

	switch (iProp)
	{
	case P_AllMove:
		pAccount->TogPrivFlags( PRIV_ALLMOVE, vVal );
		addReSync();
		break;
	case P_AllShow:
		addRemoveAll( true, true );
		pAccount->TogPrivFlags( PRIV_ALLSHOW, vVal );
		addReSync();
		break;
	case P_Debug:
		pAccount->TogPrivFlags( PRIV_DEBUG, vVal );
		addRemoveAll( true, false );
		addReSync();
		break;
	case P_Detail:
		pAccount->TogPrivFlags( PRIV_DETAIL, vVal );
		break;
	case P_GM: // toggle your GM status on/off
		if ( GetPrivLevel() >= PLEVEL_GM )	// last ditch check.
		{
			pAccount->TogPrivFlags( PRIV_GM, vVal );
		}
		break;
	case P_HearAll:
	case P_Listen:
		pAccount->TogPrivFlags( PRIV_HEARALL, vVal );
		break;
	case P_PrivHide:
		pAccount->TogPrivFlags( PRIV_PRIV_HIDE, vVal);
		break;
	case P_PrivShow:
		// Hide my priv title.
		if ( GetPrivLevel() >= PLEVEL_Counsel )
		{
			if ( vVal.IsEmpty())
			{
				pAccount->TogPrivFlags( PRIV_PRIV_HIDE, vVal );
			}
			else if ( vVal.GetBool())
			{
				pAccount->ClearPrivFlags( PRIV_PRIV_HIDE );
			}
			else
			{
				pAccount->SetPrivFlags( PRIV_PRIV_HIDE );
			}
		}
		break;

	case P_Targ:
	case P_T:
		m_Targ.m_UID = vVal.GetUID();
		break;
	case P_TargP:
		m_Targ.m_pt.v_Set( vVal );
		break;
	case P_TargProp:
		m_Prop_UID = vVal.GetUID();
		break;
	case P_TargPrv:
		m_Targ.m_PrvUID = vVal.GetUID();
		break;
	case P_TargTxt:
		m_Targ.m_sText = vVal.GetStr();
		break;
	case P_Client3D:
		m_fClientVer3d = vVal.GetBool();
		break;
	case P_ClientRes:
		m_iClientResourceLevel = vVal;
		break;
	case P_ClientVer:
	case P_ClientCrypt:
	case P_Account:
		return HRES_WRITE_FAULT;
	default:
		DEBUG_CHECK(false);
		return( HRES_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CClient::s_Method( int iProp, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	// NOTE: This can be called directly from a RES_WebPage script.
	//  So do not assume we are a game client !
	// NOTE: Mostly called from CChar::s_Method
	// NOTE: Little security here so watch out for dangerous scripts !

	ASSERT(pSrc);

#if 0
	if ( toupper( pszKey[0] ) == 'X' )	// All methods with the X prefix.
	{
		// Target this command verb on some other object.
		if ( ! addTargetVerb( pszKey+1, vArgs.GetPSTR()))
			return HRES_BAD_ARGUMENTS;
		return NO_ERROR;
	}
#endif

	switch (iProp)
	{
	case M_X:
	case M_Set:
		// Args are to be a command targetted.
		// Target this command verb on some other object.
		if ( ! addTargetVerb( vArgs.GetPSTR(), NULL ))
			return( HRES_BAD_ARGUMENTS );
		break;

	case M_Add:
	case M_AddItem:
		if ( vArgs.IsEmpty())
		{
			Menu_Setup( g_Cfg.ResourceGetIDType( RES_Menu, _TEXT("MENU_ADDITEM")), m_pChar );
		}
		else
		{
			CSphereUID rid = g_Cfg.ResourceGetIDByName( RES_ItemDef, vArgs );
			if ( rid.GetResType() == RES_CharDef )
			{
				m_Targ.m_PrvUID.InitUID();
				if ( ! Cmd_CreateChar( (CREID_TYPE) rid.GetResIndex(), SPELL_Summon, false ))
					return HRES_BAD_ARGUMENTS;
				break;
			}

			ITEMID_TYPE id = (ITEMID_TYPE) rid.GetResIndex();
			if ( ! Cmd_CreateItem( id ))
				return HRES_BAD_ARGUMENTS;
		}
		break;

	case M_AddNPC:
		// Add a script NPC
		m_Targ.m_PrvUID.InitUID();
		if ( ! Cmd_CreateChar( (CREID_TYPE) g_Cfg.ResourceGetIndexType( RES_CharDef, vArgs.GetStr()), SPELL_Summon, false ))
			return HRES_BAD_ARGUMENTS;
		break;

	case M_Clients:
		if ( g_Serv.m_Clients.GetCount() <= ADMIN_CLIENTS_PER_PAGE ) // || pSrc->m_ConnectType Not Client !
		{
			g_Serv.ListClients( this );
			break;
		}
		// fall thru...

	case M_Admin:
		vArgs.MakeArraySize();
		addGumpDialogAdmin( vArgs.GetArrayInt(0), vArgs.GetArrayInt(1)); // Open the Admin console
		break;
	case M_ArrowQuest:
		if ( vArgs.MakeArraySize() < 1 )
			return HRES_BAD_ARG_QTY;
		addArrowQuest( vArgs.GetArrayInt(0), vArgs.GetArrayInt(1));
		break;
	case M_BankSelf: // open my own bank
		addBankOpen( m_pChar, (LAYER_TYPE) vArgs.GetInt());
		break;
	case M_Cast:
		if ( pSrc == NULL )
			return(HRES_PRIVILEGE_NOT_HELD);
		return Cmd_Skill_Magery( 
			(SPELL_TYPE) g_Cfg.ResourceGetIndexType(RES_Spell,vArgs),
			PTR_CAST(CObjBase,pSrc->GetAttachedObj()));

	case M_CharList:
		// ussually just a gm command
		addCharList3();
		break;

	case M_EverBTarg:
		m_Targ.m_sText = vArgs.GetStr();
		addPromptConsole( CLIMODE_PROMPT_TARG_VERB, m_Targ.m_sText.IsEmpty() ? "Enter the verb" : "Enter the text" );
		break;

	case M_FeatureMask:
		addFeatureEnable( vArgs.GetInt());
		break;

	case M_Ghost:
		// Leave your current body behind as an NPC.
		{
			// Create the ghost. c_GHOST_WOMAN
			CCharPtr pChar = CChar::CreateBasic( CREID_EQUIP_GM_ROBE );
			ASSERT(pChar);
			pChar->StatFlag_Set( STATF_Insubstantial );
			pChar->MoveToChar( m_pChar->GetTopPoint());

			// Switch bodies to it.
			return Cmd_Control( pChar );
		}
		break;

	case M_GMPage:
		m_Targ.m_sText = vArgs.GetStr();
		addPromptConsole( CLIMODE_PROMPT_GM_PAGE_TEXT, "Describe your comment or problem" );
		break;
	case M_GoTarg: // go to my (preselected) target.
		if ( m_pChar == NULL )
			return(HRES_PRIVILEGE_NOT_HELD);
		{
			CObjBasePtr pObj = g_World.ObjFind(m_Targ.m_UID);
			if ( pObj == NULL )
				return( HRES_INVALID_HANDLE );
			{
				// 3 paces away
				CPointMap pto = pObj->GetTopLevelObj()->GetTopPoint();
				CPointMap pnt = pto;
				pnt.MoveN( DIR_W, 3 );

				CMulMapBlockState block( m_pChar->GetMoveCanFlags());
				g_World.GetHeightPoint( pnt, block, NULL );	// ??? Get Area
				pnt.m_z = block.GetResultZ();

				m_pChar->m_dirFace = pnt.GetDir( pto, m_pChar->m_dirFace ); // Face the player
				m_pChar->Spell_Effect_Teleport( pnt, true, false );
			}
		}
		break;
	case M_Help:
		if ( vArgs.IsEmpty()) // if no command, display the main help system dialog.
		{
			if ( Dialog_Setup( CLIMODE_DIALOG, g_Cfg.ResourceGetIDType( RES_Dialog, IsPrivFlag(PRIV_GM) ? "d_HELPGM" : "d_HELP" ), m_pChar ))
				break;
		}
		{
			// Below here, we're looking for a command to get help on
			int index = g_Cfg.m_HelpDefs.FindKey( vArgs.GetStr());
			if ( index >= 0 )
			{
				CResourceLock sLock( g_Cfg.m_HelpDefs[index] );
				if ( sLock.IsFileOpen())
				{
					addScrollScript( sLock, SCROLL_TYPE_TIPS, 0, vArgs );
				}
			}
			else
			{
				// just display help on a command
				s_DumpHelp( vArgs, vValRet, pSrc );
			}
		}
		break;
	case M_Info:
		// We could also get ground tile info.
		addTarget( CLIMODE_TARG_OBJ_INFO, "What would you like info on?", true, false );
		break;

	case M_Last:
		// Fake Previous target.
		if ( GetTargMode() >= CLIMODE_MOUSE_TYPE )
		{
			ASSERT(m_pChar);
			CObjBasePtr pObj = g_World.ObjFind(m_pChar->m_Act.m_Targ);
			if ( pObj != NULL )
			{
				Event_Target( GetTargMode(), pObj->GetUID(),
					pObj->GetUnkPoint(), ITEMID_NOTHING );
			}
			break;
		}
		return( false );
	case M_Link:	// link doors
		m_Targ.m_UID.InitUID();
		addTarget( CLIMODE_TARG_LINK, "Select the item to link." );
		break;

	case M_LogIn:
		{
			// Try to login with name and password given.
			CLogIPPtr pLogIP = g_Cfg.FindLogIP(m_Socket);
			if ( pLogIP == NULL )
				return( HRES_INVALID_HANDLE );

			m_pAccount = NULL;	// this is odd ???
			pLogIP->SetAccount( NULL );

			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty < 2 )
				return HRES_BAD_ARG_QTY;

			LOGIN_ERR_TYPE nCode = LogIn( vArgs.GetArrayPSTR(0), vArgs.GetArrayPSTR(1));
			if ( nCode == LOGIN_SUCCESS )
			{
				// Associate this with the CLogIP !
				pLogIP->SetAccount( GetAccount());
			}
		}
		break;

	case M_LogOut:
		{
			// Clear the account and the link to this CLogIP
			CLogIPPtr pLogIP = g_Cfg.FindLogIP(m_Socket);
			if ( pLogIP == NULL )
				return( HRES_INVALID_HANDLE );
			pLogIP->InitTimes();
		}
		break;

	case M_Quit:
		// just break the connection. only do this to yourself.
		if ( pSrc != this && GetAttachedObj() != pSrc->GetAttachedObj())
			return HRES_INVALID_HANDLE;
		m_Socket.Close();
		break;

	case M_ItemMenu:
		// cascade to another menu.
		Menu_Setup( vArgs.GetUID(), m_pChar );
		break;

	case M_MidiList:
	case M_Music:
		{
			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty <= 0 )
				return HRES_BAD_ARG_QTY;
			addMusic( vArgs.GetArrayInt( Calc_GetRandVal( iArgQty )));
		}
		break;

	case M_Nudge:
		if ( vArgs.IsEmpty())
			return HRES_BAD_ARG_QTY;
		{
			m_Targ.m_sText = vArgs.GetStr();
			m_Targ.m_tmTile.m_ptFirst.InitPoint(); // Clear this first
			m_Targ.m_tmTile.m_Code = CClient::M_Nudge;
			addTarget( CLIMODE_TARG_TILE, "Select area to Nudge", true );
		}
		break;

	case M_Nuke:
		m_Targ.m_sText = vArgs.GetStr();
		m_Targ.m_tmTile.m_ptFirst.InitPoint(); // Clear this first
		m_Targ.m_tmTile.m_Code = CClient::M_Nuke;	// set nuke code.
		addTarget( CLIMODE_TARG_TILE, "Select area to Nuke", true );
		break;
	case M_NukeChar:
		m_Targ.m_sText = vArgs.GetStr();
		m_Targ.m_tmTile.m_ptFirst.InitPoint(); // Clear this first
		m_Targ.m_tmTile.m_Code = CClient::M_NukeChar;	// set nuke code.
		addTarget( CLIMODE_TARG_TILE, "Select area to Nuke Chars", true );
		break;

	case M_OneClick:
		m_Targ.m_sText = vArgs.GetStr();
		m_pChar->Skill_Start( (m_Targ.m_sText.IsEmpty()) ? SKILL_NONE : NPCACT_OneClickCmd );
		break;

	case M_Page:
		Cmd_GM_PageCmd( vArgs.GetStr());
		break;
	case M_Repair:
		addTarget( CLIMODE_TARG_REPAIR, "What item do you want to repair?" );
		break;
	case M_Resend:
	case M_Resync:
		addReSync();
		break;
	case M_Scroll:
		// put a scroll gump up.
		{
			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty <= 0 )
				return HRES_BAD_ARG_QTY;
			addScrollResource( vArgs.GetArrayStr(0), (SCROLL_TYPE)((iArgQty>1) ? vArgs.GetArrayInt(1) : SCROLL_TYPE_UPDATES ));
		}
		break;
	case M_Self:
		// Fake self target.
		if ( GetTargMode() < CLIMODE_MOUSE_TYPE )
			return HRES_BAD_ARGUMENTS;
		ASSERT(m_pChar);
		Event_Target( GetTargMode(), m_pChar->GetUID(), 
			m_pChar->GetTopPoint(), ITEMID_NOTHING );
		break;

	case M_ShowSkills:
		if ( ! vArgs.IsVoid())
		{
			CCharPtr pChar = g_World.CharFind( vArgs.GetUID());
			if ( pChar == NULL )
				return(HRES_INVALID_HANDLE);
			addSkillWindow( pChar, SKILL_QTY, false ); // Reload the real skills
			break;
		}
		addSkillWindow( m_pChar, SKILL_QTY, true ); // Reload the real skills
		break;

	case M_SkillMenu:
		// Just put up another menu.
		if ( vArgs.IsEmpty())
			return HRES_BAD_ARG_QTY;
		return Cmd_Skill_Menu( vArgs.GetUID());

	case M_Static:
		if ( vArgs.IsEmpty())
			return HRES_BAD_ARG_QTY;
		Cmd_CreateItem( (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, vArgs ), true );
		break;
	case M_Summon:	// from the spell skill script.
		// m_Targ.m_PrvUID should already be set.
		if ( ! Cmd_CreateChar( (CREID_TYPE) g_Cfg.ResourceGetIndexType( RES_CharDef, vArgs ), SPELL_Summon, true ))
			return HRES_BAD_ARGUMENTS;
		break;

	case M_SysMessageU:
	case M_SMsgU:
		// If the client is a game client/login then do this special.
		if ( m_ConnectType == CONNECT_LOGIN || m_ConnectType == CONNECT_GAME )
		{
			addSysMessageU( vArgs );
			break;
		}
		// fall through...
	case M_SysMessage:
	case M_SMsg:
		WriteString( vArgs );
		break;

	case M_Tele:
		return Cmd_Skill_Magery( SPELL_Teleport, PTR_CAST(CObjBase,pSrc->GetAttachedObj()));
	case M_Tile:
		if ( vArgs.IsEmpty())
			return HRES_BAD_ARG_QTY;
		{
			m_Targ.m_sText = vArgs.GetStr(); // Point at the options
			m_Targ.m_tmTile.m_ptFirst.InitPoint(); // Clear this first
			m_Targ.m_tmTile.m_Code = CClient::M_Tile;
			addTarget( CLIMODE_TARG_TILE, "Pick 1st corner:", true );
		}
		break;
	case M_WebLink:
		addWebLaunch( vArgs.GetStr());
		break;

	case M_SendPacket:
		// DEBUG send client message.
		if ( pSrc->GetPrivLevel() >= GetPrivLevel()) 
		{
			int iArgQty = vArgs.MakeArraySize();
			if ( iArgQty < 1 )
				return HRES_BAD_ARG_QTY;
			BYTE bData[512];
			for ( int i=0; i<iArgQty && i<COUNTOF(bData); i++ )
			{
				bData[i] = vArgs.GetArrayElement(i).GetDWORD();
			}
			xSendReady( bData, iArgQty );
		}
		break;

	case M_Extract:
		// sort of like EXPORT but for statics.
		// Opposite of the "UNEXTRACT" command

		if ( vArgs.IsEmpty())
			return HRES_BAD_ARG_QTY;
		{
			int iArgQty = vArgs.MakeArraySize();
			m_Targ.m_sText = vArgs.GetArrayPSTR(0); // Point at the options, if any
			m_Targ.m_tmTile.m_ptFirst.InitPoint(); // Clear this first
			m_Targ.m_tmTile.m_Code = CClient::M_Extract;	// set extract code.
			m_Targ.m_tmTile.m_id = vArgs.GetArrayInt(1);	// extract id.
			addTarget( CLIMODE_TARG_TILE, "Select area to Extract", true );
		}
		break;
	case M_Unextract:
		// Create item from script.
		// Opposite of the "EXTRACT" command
		if ( vArgs.IsEmpty())
			return HRES_BAD_ARG_QTY;
		{
			int iArgQty = vArgs.MakeArraySize();

			m_Targ.m_sText = vArgs.GetArrayPSTR(0); // Point at the options, if any
			m_Targ.m_tmTile.m_ptFirst.InitPoint(); // Clear this first
			m_Targ.m_tmTile.m_Code = CClient::M_Unextract;	// set extract code.
			m_Targ.m_tmTile.m_id = vArgs.GetArrayInt(1);	// extract id.

			addTarget( CLIMODE_TARG_UNEXTRACT, "Where to place the extracted multi?", true );
		}
		break;

		// More global static type methods.

	case M_Information:
		WriteString( g_Serv.GetStatusString( 0x22 ));
		WriteString( g_Serv.GetStatusString( 0x24 ));
		break;
	case M_Version:	// "SHOW VERSION"
		WriteString( g_szServerDescription );
		break;

	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}

HRESULT CClient::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceObj::s_Method( pszKey, vArgs, vValRet, pSrc ));
	}
	return s_Method( iProp, vArgs, vValRet, pSrc );
}
HRESULT CClient::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceObj::s_PropGet( pszKey, vValRet, pSrc  ));
	}
	return s_PropGet( iProp, vValRet, pSrc );
}
HRESULT CClient::s_PropSet( const char* pszKey, CGVariant& vVal )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceObj::s_PropSet( pszKey, vVal ));
	}
	return s_PropSet( iProp, vVal );
}


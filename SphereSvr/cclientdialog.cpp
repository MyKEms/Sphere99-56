//
// CClientDialog.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
// Special gumps for the client.

#include "stdafx.h"	// predef header.

static LPCTSTR const sm_pszDialogTags[] =		// GUMPCTL_QTY is more !
{
	"resizepic",
	"gumppic",
	"tilepic",
	"text",
	"croppedtext",
	"htmlgump",
	"xmfhtmlgump",
	"button",
	"radio",
	"checkbox",
	"textentry",	// textentry	// 7 = x,y,widthpix,widthchars,wHue,gumpid,startstringindex
	"page",
	"group",
	"nomove",
	"noclose",
	"nodispose",
	"gumppictiled",
	"checkertrans",
	"xmfhtmlgumpcolor",
	"tilepichue",
	NULL,
};

TRIGRET_TYPE CClient::Dialog_OnButton( CSphereUID rid, DWORD dwButtonID, CSphereExpArgs& exec )
{
	// CLIMODE_DIALOG
	// one of the gump dialog buttons was pressed.
	// NOTE: Button 0 = cancel.

	CResourceLock s( g_Cfg.ResourceGetDef( CSphereUID( RES_Dialog, rid.GetResIndex(), RES_DIALOG_BUTTON )));
	if ( ! s.IsFileOpen())
	{
		DEBUG_WARN(("Dialog_OnButton '%s' No BUTTON handler" LOG_CR, (LPCTSTR) g_Cfg.ResourceGetName(rid) ));
		return TRIGRET_ENDIF;
	}

	if ( ! s.FindTriggerNumber( dwButtonID ))
	{
		if ( dwButtonID )
		{
			DEBUG_WARN(("Dialog_OnButton '%s' No such button %d" LOG_CR, (LPCTSTR) g_Cfg.ResourceGetName(rid), dwButtonID ));
		}
		return TRIGRET_ENDIF;
	}

	return exec.ExecuteScript( s, TRIGRUN_SECTION_TRUE );
}

bool CClient::Dialog_Setup( CLIMODE_TYPE mode, CSphereUID rid, CObjBase* pObj )
{
	if ( pObj == NULL )
		return( false );

	CResourceLock sDialog( g_Cfg.ResourceGetDef( rid ));
	if ( ! sDialog.IsFileOpen())
	{
		return false;
	}

	// read the size.
	if ( ! sDialog.ReadLine())
	{
		return( false );
	}

	// starting x,y location.
	int piArgs[2];
	int iArgQty = Exp_ParseCmds( sDialog.GetLineBuffer(), piArgs, COUNTOF(piArgs));
	int x = piArgs[0];
	int y = piArgs[1];

	CSphereExpContext exec( pObj, m_pChar );

	CGStringArray asControls;
	while ( sDialog.ReadLine())
	{
		// The first part of the key is GUMPCTL_TYPE
		TCHAR* pszCmd = sDialog.GetLineBuffer();
		GETNONWHITESPACE( pszCmd );
		int iCmd = FindTableHead( pszCmd, sm_pszDialogTags );
		if ( iCmd < 0 )
		{
			DEBUG_WARN(( "Unknown Gump Dialog Command '%s'" LOG_CR, pszCmd ));
			// continue;
		}

		// Beware that <HTML> tags may be in here as well!

		exec.s_ParseEscapes( pszCmd, 0 );
		asControls.Add( pszCmd );

		// ? count how much text we will need.
	}

	// Now get the RES_DIALOG_TEXT.
	CResourceLock sText( g_Cfg.ResourceGetDef( CSphereUID( RES_Dialog, rid.GetResIndex(), RES_DIALOG_TEXT )));
	if ( ! sText.IsFileOpen())
	{
		return false;
	}

	CGStringArray asText;
	while ( sText.ReadLine())
	{
		exec.s_ParseEscapes( sText.GetLineBuffer(), CSCRIPT_PARSE_HTML );
		asText.Add( sText.GetLineBuffer());
	}

	// Now pack it up to send,
	m_Targ.m_tmGumpDialog.m_ResourceID = rid;

	addGumpDialog( mode, asControls, asText, x, y, pObj );
	return( true );
}

void CClient::addGumpDialog( CLIMODE_TYPE mode, CGStringArray& asControls, CGStringArray& asText, int x, int y, CObjBase* pObj )
{
	// Add a generic GUMP menu.
	// Should return a Event_GumpDialogRet
	// NOTE: These packets can get rather LARGE.
	// x,y = where on the screen ?

	if ( pObj == NULL )
		pObj = m_pChar;

	int lengthControls=1;
	int i=0;
	for ( ; i < asControls.GetSize(); i++)
	{
		lengthControls += asControls[i].GetLength() + 2;
	}

	int lengthText = lengthControls + 20 + 3;
	for ( i=0; i < asText.GetSize(); i++)
	{
		int lentext2 = asText[i].GetLength();
		DEBUG_CHECK( lentext2 < MAX_TALK_BUFFER );
		lengthText += (lentext2*2)+2;
	}

	// Send the fixed length stuff
	CUOCommand cmd;
	cmd.GumpDialog.m_Cmd = XCMD_GumpDialog;
	cmd.GumpDialog.m_len = lengthText;
	cmd.GumpDialog.m_UID = pObj->GetUID();
	cmd.GumpDialog.m_context = mode;
	cmd.GumpDialog.m_x = x;
	cmd.GumpDialog.m_y = y;
	cmd.GumpDialog.m_lenCmds = lengthControls;
	xSend( &cmd, 21 );

	for ( i=0; i<asControls.GetSize(); i++)
	{
		CGString sTmp;
		sTmp.Format( "{%s}", (LPCTSTR) asControls[i] );
		xSend( sTmp, sTmp.GetLength() );
	}

	// Pack up the variable length stuff
	BYTE Pkt_gump2[3];
	Pkt_gump2[0] = '\0';
	PACKWORD( &Pkt_gump2[1], asText.GetSize() );
	xSend( Pkt_gump2, 3);

	// Pack text in UNICODE type format.
	for ( i=0; i < asText.GetSize(); i++)
	{
		int len1 = asText[i].GetLength();

		NWORD len2;
		len2 = len1;
		xSend( &len2, sizeof(NWORD));
		if ( len1 )
		{
			NCHAR szTmp[MAX_TALK_BUFFER];
			int len3 = CvtSystemToNUNICODE( szTmp, COUNTOF(szTmp), asText[i] );
			xSend( szTmp, len2*sizeof(NCHAR));
		}
	}

	m_Targ.m_tmGumpDialog.m_UID = pObj->GetUID();

	SetTargMode( mode );
}

bool CClient::addGumpDialogProps( CSphereUID uid )
{
	// put up a prop dialog for the object.
	// NOTE: Only GM's should ever really see this.
	CObjBasePtr pObj = g_World.ObjFind(uid);
	if ( pObj == NULL )
		return false;
	if ( m_pChar == NULL )
		return( false );
	if ( ! m_pChar->CanTouch( pObj ))	// probably a security issue.
		return( false );

	m_Prop_UID = m_Targ.m_UID = uid;
	if ( uid.IsChar())
	{
		addSkillWindow( REF_CAST(CChar,pObj), SKILL_QTY, false ); // load the targets skills
	}

	CGString sName;
	sName.Format( pObj->IsItem() ? "d_ITEMPROP1" : "d_CHARPROP1" );

	CSphereUID rid = g_Cfg.ResourceGetIDType( RES_Dialog, sName );
	if ( ! rid.IsValidRID())
		return false;

	Dialog_Setup( CLIMODE_DIALOG, rid, pObj );
	return( true );
}

struct CClientSortArray : public CGPtrSortArray<CClient*, CClient*>
{
public:
	int m_iSortType;
public:
	virtual int CompareData( CClient* pLeft, CClient* pServ ) const;
	void SortByType( int iType, CChar* pCharSrc );
};

int CClientSortArray::CompareData( CClient* pClientComp, CClient* pClient ) const
{
	ASSERT(pClientComp);
	switch(m_iSortType)
	{
	case 0:	// socket number
		return( pClientComp->m_Socket.GetSocket() - pClient->m_Socket.GetSocket() );
	case 1: // Account Name
	default:
		return( _stricmp( pClientComp->GetName(), pClient->GetName()));
	}
}

void CClientSortArray::SortByType( int iType, CChar* pCharSrc )
{
	// 1 = player name

	ASSERT(pCharSrc);
	int iSize = g_Serv.m_Clients.GetCount();
	this->SetCount( iSize );

	CClientPtr pClient = g_Serv.GetClientHead();
	int i;
	for ( i=0; pClient; pClient = pClient->GetNext())
	{
		if ( pClient->GetChar() == NULL )
			continue;
		if ( ! pCharSrc->CanDisturb( pClient->GetChar()))
			continue;
		SetAt( i, pClient );
		i++;
	}

	this->SetCount( i );
	if ( i <= 1 )
		return;

	m_iSortType = iType;
	if ( iType < 0 )
		return;

	// Now quicksort the list to the needed format.
	QSort();
}

void CClient::addGumpDialogAdmin( int iAdPage, int iSortType )
{
	// Alter this routine at your own risk....if anymore
	// bytes get sent to the client, you WILL crash them
	// Take away, but don't add (heh) (actually there's like
	// 100 bytes available, but they get eaten up fast with
	// this gump stuff)

	static LPCTSTR const sm_szGumps[] = // These are on every page and don't change
	{
		"page 0",
		"resizepic 0 0 5120 623 310",
		"resizepic 242 262 5120 170 35",
		"text 230 10 955 0",
	};

	CGStringArray asControls;
	int i;
	for ( i=0; i<COUNTOF(sm_szGumps); i++ )
	{
		asControls.Add( sm_szGumps[i] );
	}

	CGStringArray asText;
	asText.AddFormat( "Admin Control Panel (%d clients)", g_Serv.m_Clients.GetCount());

	static const int sm_iColSpaces[] = // These are on every page and don't change
	{
		8 + 19,		// Text (sock)
		8 + 73,		// Text (account name)
		8 + 188,	// Text (name)
		8 + 328,	// Text (ip)
		8 + 474		// Text (location)
	};

	static LPCTSTR const sm_szColText[] = // These are on every page and don't change
	{
		"Sock",
		"Account",
		"Name",
		"IP Address",
		"Location"
	};

	for ( i=0; i<COUNTOF(sm_szColText); i++ )
	{
		asControls.AddFormat( "text %d 30 955 %d", sm_iColSpaces[i]-2, i+1 );
		asText.Add( sm_szColText[i] );
	}

	// Count clients to show.
	// Create sorted list.
	CClientSortArray ClientList;
	ClientList.SortByType( 1, m_pChar );

	// none left to display for this page ?
	int iClient = iAdPage*ADMIN_CLIENTS_PER_PAGE;
	if ( iClient >= ClientList.GetSize())
	{
		iAdPage = 0;	// just go back to first page.
		iClient = 0;
	}

	m_Targ.m_tmGumpAdmin.m_iPageNum = iAdPage;
	m_Targ.m_tmGumpAdmin.m_iSortType = iSortType;

	int iY = 50;

	for ( i=0; iClient<ClientList.GetSize() && i<ADMIN_CLIENTS_PER_PAGE; iClient++, i++ )
	{
		CClientPtr pClient = ClientList.ElementAt(iClient);
		ASSERT(pClient);

		// Sock buttons
		//	X, Y, Down gump, Up gump, pressable, iPage, id
		asControls.AddFormat(
			"button 12 %i 2362 2361 1 1 %i",
			iY + 4, // Y
			901 + i );	// id

		for ( int j=0; j<COUNTOF(sm_iColSpaces); j++ )
		{
			asControls.AddFormat(
				"text %i %i 955 %i",
				sm_iColSpaces[j],
				iY,
				asText.GetSize()+j );
		}

		CSocketAddress PeerName = pClient->m_Socket.GetPeerName();

		TCHAR accountName[MAX_NAME_SIZE];
		strcpylen( accountName, pClient->GetName(), 12 );
		// max name size for this dialog....otherwise CRASH!!!, not a big enuf buffer :(

		CCharPtr pChar = pClient->GetChar();
		if ( pClient->IsPrivFlag(PRIV_GM) || pClient->GetPrivLevel() >= PLEVEL_Counsel )
		{
			memmove( accountName+1, accountName, 13 );
			accountName[0] = ( pChar && pChar->Player_IsDND()) ? '*' : '+';
		}

		ASSERT( i<COUNTOF(m_Targ.m_tmGumpAdmin.m_Item));
		if ( pChar != NULL )
		{
			m_Targ.m_tmGumpAdmin.m_Item[i] = pChar->GetUID();

			TCHAR characterName[MAX_NAME_SIZE];
			strcpy(characterName, pChar->GetName());
			characterName[12] = 0; // same comment as the accountName...careful with this stuff!
			asText.AddFormat( "%x", pClient->m_Socket.GetSocket());
			asText.Add( accountName );
			asText.Add( characterName );

			CPointMap pt = pChar->GetTopPoint();
			asText.AddFormat( "%s", PeerName.GetAddrStr()) ;
			asText.AddFormat( "%d,%d,%d [%d]",
				pt.m_x,
				pt.m_y,
				pt.m_z,
				pt.m_mapplane) ;
		}
		else
		{
			m_Targ.m_tmGumpAdmin.m_Item[i] = 0;

			asText.AddFormat( "%03x", pClient->m_Socket.GetSocket());
			asText.Add( accountName );
			asText.Add( "N/A" );
			asText.Add( PeerName.GetAddrStr());
			asText.Add( "N/A" );
		}

		iY += 20; // go down a 'line' on the dialog
	}

	if ( m_Targ.m_tmGumpAdmin.m_iPageNum )	// is there a previous ?
	{
		asControls.Add( "button 253 267 5537 5539 1 0 801" ); // p
	}
	if ( iClient<ClientList.GetSize())	// is there a next ?
	{
		asControls.Add( "button 385 267 5540 5542 1 0 802" ); // n
	}

	addGumpDialog( CLIMODE_DIALOG_ADMIN, asControls, asText, 0x05, 0x46, NULL );
}


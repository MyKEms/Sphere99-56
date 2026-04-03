//
// CClientGMPage.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
#include "stdafx.h"	// predef header.

/////////////////////////////////////////////

void CClient::Cmd_GM_Page( LPCTSTR pszReason ) // Help button (Calls GM Call Menus up)
{
	// Player pressed the help button.
	// m_Targ_Text = menu desc.
	// CLIMODE_PROMPT_GM_PAGE_TEXT

	if ( pszReason[0] == '\0' )
	{
		WriteString( "Game Master Page Cancelled.");
		return;
	}

	CGString sMsg;
	sMsg.Format( "GM Page from %s [0%lx]: %s" LOG_CR,
		(LPCTSTR) m_pChar->GetName(), m_pChar->GetUID(), (LPCTSTR) pszReason );
	g_Log.Event( LOG_GROUP_GM_PAGE, LOGL_TRACE, (LPCTSTR) sMsg );

	bool fFound=false;
	for ( CClientPtr pClient = g_Serv.GetClientHead(); pClient; pClient = pClient->GetNext())
	{
		if ( pClient->GetChar() && pClient->IsPrivFlag( PRIV_GM_PAGE )) // found GM
		{
			fFound=true;
			pClient->WriteString(sMsg);
		}
	}
	if ( ! fFound)
	{
		WriteString( "There is no Game Master available to take your call. Your message has been queued.");
	}
	else
	{
		WriteString( "Available Game Masters have been notified of your request.");
	}

	// Already have a message in the queue ?
	// Find an existing GM page for this account.
	CGMPagePtr pPage = g_World.m_GMPages.GetHead();
	int iCount = 0;
	for ( ; pPage!= NULL; pPage = pPage->GetNext())
	{
		if ( pPage->GetAccount() == GetAccount())
			break;
		iCount++;
	}
	if ( pPage != NULL )
	{
		WriteString( "You have an existing page. It has been updated." );
		pPage->SetReason( pszReason );
		pPage->m_timePage.InitTimeCurrent();
	}
	else
	{
		// Queue a GM page. (based on account)
		pPage = new CGMPage( GetAccount());
		pPage->SetReason( pszReason );	// Description of reason for call.
	}

	Printf( "There are %d messages queued ahead of you", iCount );
	pPage->m_ptOrigin = m_pChar->GetTopPoint();		// Origin Point of call.
}

void CClient::Cmd_GM_PageClear()
{
	if ( m_pGMPage )
	{
		m_pGMPage->ClearGMHandler();
		m_pGMPage = NULL;
	}
}

void CClient::Cmd_GM_PageMenu( int iEntryStart )
{
	// Just put up the GM page menu.
	SetPrivFlags( PRIV_GM_PAGE );
	Cmd_GM_PageClear();

	CMenuItem menuitem[10];	// only display x at a time.
	ASSERT( COUNTOF(menuitem)<=COUNTOF(m_Targ.m_tmMenu.m_Item));

	menuitem[0].m_sText = "GM Page Menu";

	int entry=0;
	int count=0;
	CGMPagePtr pPage = g_World.m_GMPages.GetHead();
	for ( ; pPage!= NULL; pPage = pPage->GetNext(), entry++ )
	{
		if ( entry < iEntryStart )
			continue;

		CClientPtr pGM = pPage->FindGMHandler();	// being handled ?
		if ( pGM != NULL )
			continue;

		if ( ++count >= COUNTOF( menuitem )-1 )
		{
			// Add the "MORE" option if there is more than 1 more.
			if ( pPage->GetNext() != NULL )
			{
				menuitem[count].m_id = count-1;
				menuitem[count].m_sText.Format( "MORE" );
				m_Targ.m_tmMenu.m_Item[count] = 0xFFFF0000 | entry;
				break;
			}
		}

		CClientPtr pClient = g_Serv.FindClientAccount( pPage->GetAccount());	// logged in ?

		menuitem[count].m_id = count-1;
		menuitem[count].m_sText.Format( "%s %s %s",
			(LPCTSTR) pPage->GetName(),
			(pClient==NULL) ? "OFF":"ON ",
			(LPCTSTR) pPage->GetReason());
		m_Targ.m_tmMenu.m_Item[count] = entry;
	}

	if ( ! count )
	{
		WriteString( "No GM pages queued. Use .PAGE ?" );
		return;
	}
	addItemMenu( CLIMODE_MENU_GM_PAGES, menuitem, count );
}

void CClient::Cmd_GM_PageInfo()
{
	// Show the current page.
	// This should be a dialog !!!??? book or scroll.
	ASSERT( m_pGMPage );

	Printf(
		"Current GM .PAGE Account=%s (%s) "
		"Reason='%s' "
		"Time=%ld",
		(LPCTSTR)m_pGMPage->GetName(),
		(LPCTSTR)m_pGMPage->GetAccountStatus(),
		(LPCTSTR)m_pGMPage->GetReason(),
		m_pGMPage->GetAge());
}

enum GPV_TYPE
{
	GPV_B,
	GPV_BAN,
	GPV_C,
	GPV_CURRENT,
	GPV_D,
	GPV_DELETE,
	GPV_G,	// .PAGE go
	GPV_GO,
	GPV_H,	// help ?
	GPV_HELP,
	GPV_J,	// .PAGE jail
	GPV_JAIL,
	GPV_K,	// .PAGE ban or kick = locks the account of this person and kicks
	GPV_KICK,	// .PAGE ban or kick = locks the account of this person and kicks
	GPV_L,	// List
	GPV_LIST,
	GPV_O,
	GPV_OFF,
	GPV_ON,
	GPV_ORIGIN,
	GPV_P,	// .PAGE player = go to the player that made the page. (if they are logged in)
	GPV_PLAYER,
	GPV_Q,
	GPV_QUEUE,
	GPV_U,
	GPV_UNDO,
	GPV_WIPE,
	GPV_QTY,
};

static LPCTSTR const sm_pszGMPageVerbs[GPV_QTY+1] =
{
	"B",
	"BAN",
	"C",
	"CURRENT",
	"D",
	"DELETE",
	"G",	// .PAGE go
	"GO",
	"H",	// help ?
	"HELP",
	"J",	// .PAGE jail
	"JAIL",
	"K",	// .PAGE ban or kick = locks the account of this person and kicks
	"KICK",	// .PAGE ban or kick = locks the account of this person and kicks
	"L",	// List
	"LIST",
	"O",
	"OFF",
	"ON",
	"ORIGIN",
	"P",	// .PAGE player = go to the player that made the page. (if they are logged in)
	"PLAYER",
	"Q",
	"QUEUE",
	"U",
	"UNDO",
	"WIPE",
	NULL,
};

static LPCTSTR const sm_pszGMPageVerbsHelp[] =
{
	".PAGE on/off" LOG_CR,
	".PAGE list = list of pages." LOG_CR,
	".PAGE delete = dispose of this page. Assume it has been handled." LOG_CR,
	".PAGE origin = go to the origin point of the page" LOG_CR,
	".PAGE undo/queue = put back in the queue" LOG_CR,
	".PAGE current = info on the current selected page." LOG_CR,
	".PAGE go/player = go to the player that made the page. (if they are logged in)" LOG_CR,
	".PAGE jail" LOG_CR,
	".PAGE ban/kick" LOG_CR,
	".PAGE wipe (gm only)",
	NULL,
};

void CClient::Cmd_GM_PageCmd( LPCTSTR pszCmd )
{
	// A GM page command.
	// Put up a list of GM pages pending.

	if ( pszCmd == NULL || pszCmd[0] == '?' )
	{
		// addScrollText( pCmds );
		for ( int i=0; i<COUNTOF(sm_pszGMPageVerbsHelp)-1; i++ )
		{
			WriteString( sm_pszGMPageVerbsHelp[i] );
		}
		return;
	}
	if ( pszCmd[0] == '\0' )
	{
		if ( m_pGMPage )
			Cmd_GM_PageInfo();
		else
			Cmd_GM_PageMenu();
		return;
	}

	int iProp = s_FindKeyInTable( pszCmd, sm_pszGMPageVerbs );
	if ( iProp < 0 )
	{
		// some other verb ?
#if 0
		if ( m_pGMPage )
		{
			r_Verb( sdfsdf );
		}
#endif
		Cmd_GM_PageCmd(NULL);
		return;
	}

	switch ( iProp )
	{
	case GPV_OFF:
		if ( GetPrivLevel() < PLEVEL_Counsel )
			return;	// cant turn off.
		ClearPrivFlags( PRIV_GM_PAGE );
		Cmd_GM_PageClear();
		WriteString( "GM pages off" );
		return;
	case GPV_ON:
		SetPrivFlags( PRIV_GM_PAGE );
		WriteString( "GM pages on" );
		return;
	case GPV_WIPE:
		if ( ! IsPrivFlag( PRIV_GM ))
			return;
		g_World.m_GMPages.DeleteAll();
		return;
	case GPV_H:	// help ?
	case GPV_HELP:
		Cmd_GM_PageCmd(NULL);
		return;
	}

	if ( ! m_pGMPage.IsValidRefObj())
	{
		// No gm page has been selected yet.
		Cmd_GM_PageMenu();
		return;
	}

	// We must have a select page for these commands.
	switch ( iProp )
	{
	case GPV_L:	// List
	case GPV_LIST:
		Cmd_GM_PageMenu();
		return;
	case GPV_D:
	case GPV_DELETE:
		// .PAGE delete = dispose of this page. We assume it has been handled.
		WriteString( "GM Page deleted" );
		m_pGMPage.ReleaseRefObj();
		return;
	case GPV_O:
	case GPV_ORIGIN:
		// .PAGE origin = go to the origin point of the page
		m_pChar->Spell_Effect_Teleport( m_pGMPage->m_ptOrigin, true, false );
		return;
	case GPV_U:
	case GPV_UNDO:
	case GPV_Q:
	case GPV_QUEUE:
		// .PAGE queue = put back in the queue
		WriteString( "GM Page re-queued." );
		Cmd_GM_PageClear();
		return;
	case GPV_B:
	case GPV_BAN:
	case GPV_K:	// .PAGE ban or kick = locks the account of this person and kicks
	case GPV_KICK:	// .PAGE ban or kick = locks the account of this person and kicks
		// This should work even if they are not logged in.
		{
			CAccountPtr pAccount = m_pGMPage->GetAccount();
			if ( pAccount )
			{
				if ( ! pAccount->Block( this, INT_MAX ))
					return;
			}
			else
			{
				WriteString( "Invalid account for page !?" );
			}
		}
		break;
	case GPV_C:
	case GPV_CURRENT:
		// What am i servicing ?
		Cmd_GM_PageInfo();
		return;
	}

	// Manipulate the character only if logged in.

	CClientPtr pClient = g_Serv.FindClientAccount( m_pGMPage->GetAccount());
	if ( pClient == NULL || pClient->GetChar() == NULL )
	{
		WriteString( "The account is not logged in." );
		if ( iProp == GPV_P || iProp == GPV_PLAYER )
		{
			m_pChar->Spell_Effect_Teleport( m_pGMPage->m_ptOrigin, true, false );
		}
		return;
	}

	switch ( iProp )
	{
	case GPV_G:
	case GPV_GO: // .PAGE player = go to the player that made the page. (if they are logged in)
	case GPV_P:	// .PAGE player = go to the player that made the page. (if they are logged in)
	case GPV_PLAYER:
		m_pChar->Spell_Effect_Teleport( pClient->GetChar()->GetTopPoint(), true, false );
		return;
	case GPV_B:
	case GPV_BAN:
	case GPV_K:	// .PAGE ban or kick = locks the account of this person and kicks
	case GPV_KICK:	// .PAGE ban or kick = locks the account of this person and kicks
		pClient->addKick( m_pChar, INT_MAX );
		return;
	case GPV_J:	// .PAGE jail
	case GPV_JAIL:
		pClient->GetChar()->Jail( this, 4*365*24*60 );
		return;
	default:
		DEBUG_CHECK(0);
		return;
	}
}

void CClient::Cmd_GM_PageSelect( int iSelect )
{
	// 0 = cancel.
	// 1 based.

	if ( m_pGMPage != NULL )
	{
		WriteString( "Current message sent back to the queue" );
		Cmd_GM_PageClear();
	}

	if ( iSelect <= 0 || iSelect >= COUNTOF(m_Targ.m_tmMenu.m_Item))
	{
		return;
	}

	if ( m_Targ.m_tmMenu.m_Item[iSelect] & 0xFFFF0000 )
	{
		// "MORE" selected
		Cmd_GM_PageMenu( m_Targ.m_tmMenu.m_Item[iSelect] & 0xFFFF );
		return;
	}

	CGMPagePtr pPage = static_cast<CGMPage*>(g_World.m_GMPages.GetAt( m_Targ.m_tmMenu.m_Item[iSelect] ));
	if ( pPage != NULL )
	{
		if ( pPage->FindGMHandler())
		{
			WriteString( "GM Page already being handled." );
			return;	// someone already has this.
		}

		m_pGMPage = pPage;
		m_pGMPage->SetGMHandler( this );
		Cmd_GM_PageInfo();
		Cmd_GM_PageCmd( "P" );	// go there.
	}
}


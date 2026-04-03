//
// CChat.CPP
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//
// Chat system
//

#include "stdafx.h"	// predef header.
#include "CChat.h"

void CClient::Event_ChatButton(const NCHAR* pszName) // Client's chat button was pressed
{
	// See if they've made a chatname yet
	// m_ChatPersona.SetClient(this);

	CAccountPtr pAccount = GetAccount();
	ASSERT(pAccount);

	if ( pAccount->m_sChatName.IsEmpty())
	{
		// No chatname yet, see if the client sent one
		if (pszName[0] == 0) // No name was sent, so ask for a permanent chat system nickname (account based)
		{
			addChatSystemMessage(CHATMSG_GetChatName);
			return;
		}
		// OK, we got a nick, now store it with the account stuff.

		// Make it non unicode
		TCHAR szChatName[ MAX_NAME_SIZE* 2 + 2 ];
		CvtNUNICODEToSystem( szChatName, sizeof(szChatName), pszName, 128 );

		if ( ! CChat::IsValidName(szChatName, true) ||
			g_Accounts.Account_FindChatName(szChatName)) // Check for legal name, duplicates, etc
		{
			// Is already used ?
			addChatSystemMessage(CHATMSG_Error);
			addChatSystemMessage(CHATMSG_GetChatName);
			return;
		}

		pAccount->m_sChatName = szChatName;
	}

	// Ok, below here we have a chat system nickname
	// Tell the chat system it has a new client using it
	if ( Chat_IsActive())
		return;

	m_fChatActive = true;

	// Tell the client to open the chat window dialog
	addChatSystemMessage( CHATMSG_OpenChatWindow, pAccount->m_sChatName );
	// Send all existing channel names to the client

	// Send all existing channel names to this client
	CChatChannelPtr pChannel = g_Serv.m_Chats.GetFirstChannel();
	for ( ; pChannel != NULL; pChannel = pChannel->GetNext())
	{
		addChatSystemMessage(CHATMSG_SendChannelName, pChannel->GetName(), pChannel->GetPassModeString());
	}
}

void CClient::Event_ChatText( const NCHAR* pszText, int len, CLanguageID lang ) // Text from a client
{
	// Just send it all to the chat system
	// ARGS:
	//  len = length of the pszText string in NCHAR's.
	//

	TCHAR szText[MAX_TALK_BUFFER* 2];
	CvtNUNICODEToSystem( szText, sizeof(szText), pszText, len );

	CChatChannelPtr pChannel = Channel_Get();

	// The 1st character is a command byte, join channel, private message someone, etc etc
	TCHAR* pszMsg = szText+1;
	switch ( szText[0] )
	{
	case 'a':	// a = client typed a plain message in the text entry area
		// Check for a chat command here
		if (pszMsg[0] == '/')
		{
			g_Serv.m_Chats.DoCommand( pszMsg + 1, this );
			break;
		}
		if (!pChannel)
		{
not_in_a_channel:
			addChatSystemMessage(CHATMSG_MustBeInAConference);
			return;
		}
		// Not a chat command, must be speech
		pChannel->Speak_Normal(this, pszMsg, lang);
		break;

	case 'A':	// A = change the channel password
		{
			if (!pChannel)
				goto not_in_a_channel;
			pChannel->ChangePassword(this, pszMsg);
			break;
		};
	case 'b':	// b = client joining an existing channel
		{
			// Look for second double quote to separate channel from password
			char* pszPassword = strchr( pszMsg+1, '"' ); 
			if (pszPassword==NULL)
			{
				pszPassword = "";
			}
			else
			{
				*pszPassword++ = '\0';
				GETNONWHITESPACE(pszPassword); // skip leading space if any
			}
			g_Serv.m_Chats.JoinChannel( this, pszMsg+1, pszPassword );
			break;
		};
	case 'c':	// c = client creating (and joining) new channel
		{
			char* pszPassword = NULL;
			for (int i = 0; pszMsg[i]; i++)
			{
				if (pszMsg[i] == '{') // OK, there's a password here
				{
					pszMsg[i] = 0;
					pszPassword = pszMsg + i + 1;
					for(i = 0; pszPassword[i]; i++)
					{
						if (pszPassword[i] == '}')
						{
							pszPassword[i] = 0;
							break;
						}
					}
					break;
				}
			}
			g_Serv.m_Chats.CreateJoinChannel(this, pszMsg, pszPassword);
			break;
		};
	case 'd':	// d = (/rename x) rename conference
		{
			if (!pChannel)
				goto not_in_a_channel;
			RenameChannel(pszMsg);
			break;
		};
	case 'e':	// e = Send a private message to ....
		{
			if (!pChannel)
				goto not_in_a_channel;
			char buffer[2048];
			strcpy(buffer, pszMsg);
			// Separate the recipient from the message (look for a space)
			int i=0;
			for (; buffer[i]; i++)
			{
				if (buffer[i] == ' ')
				{
					buffer[i] = 0;
					break;
				}
			}
			pChannel->Speak_Private( this, buffer, buffer+i+1, lang );
			break;
		};
	case 'f':	// f = (+ignore) ignore this person
		IgnoreList_Add( pszMsg );
		break;
	case 'g':	// g = (-ignore) don't ignore this person
		IgnoreList_Remove(pszMsg);
		break;
	case 'h':	// h = toggle ignoring this person
		IgnoreList_Toggle(pszMsg);
		break;
	case 'i':	// i = grant speaking privs to this person
		if (!pChannel)
			goto not_in_a_channel;
		pChannel->Voice_Grant(this, pszMsg);
		break;
	case 'j':	// j = remove speaking privs from this person
		if (!pChannel)
			goto not_in_a_channel;
		pChannel->Voice_Revoke(this, pszMsg);
		break;
	case 'k':	// k = (/voice) toggle voice status
		if (!pChannel)
			goto not_in_a_channel;
		pChannel->Voice_Toggle(this, pszMsg);
		break;
	case 'l':	// l = grant moderator status to this person
		{
			if (!pChannel)
				goto not_in_a_channel;
			pChannel->Moderator_Grant(this, pszMsg);
			break;
		};
	case 'm':	// m = remove moderator status from this person
		{
			if (!pChannel)
				goto not_in_a_channel;
			pChannel->Moderator_Revoke(this, pszMsg);
			break;
		};
	case 'n':	// m = toggle the moderator status for this person
		{
			if (!pChannel)
				goto not_in_a_channel;
			pChannel->Moderator_Toggle(this, pszMsg);
			break;
		}
	case 'o':	// o = turn on receiving private messages
		{
			Receiving_Set(true);
			break;
		}
	case 'p':	// p = turn off receiving private messages
		{
			Receiving_Set(false);
			break;
		}
	case 'q':	// q = toggle receiving messages
		{
			Receiving_Toggle();
			break;
		};
	case 'r':	// r = (+showname) turn on showing character name
		{
			WhoIs_Permit();
			break;
		};
	case 's':	// s = (-showname) turn off showing character name
		{
			WhoIs_Forbid();
			break;
		};
	case 't':	// t = toggle showing character name
		{
			WhoIs_Toggle();
			break;
		};
	case 'u':	// u = who is this player
		{
			if (!pChannel)
				goto not_in_a_channel;
			pChannel->Member_WhoIs( this, pszMsg);
			break;
		};
	case 'v':	// v = kick this person out of the conference
		{
			if (!pChannel)
				goto not_in_a_channel;

			CClientPtr pClient = pChannel->Member_Find(pszMsg);
			if (!pClient)
			{
				addChatSystemMessage(CHATMSG_NoPlayer, pszMsg);
				break;
			}

			pChannel->Member_Kick(this, pClient);
			break;
		};
	case 'X':	// X = client quit chat
		Chat_Quit();
		break;
	case 'w':	// w = (+defaultvoice) make moderators be the only ones with a voice by default
		if (!pChannel)
			goto not_in_a_channel;
		pChannel->VoiceDefault_Disable(this);
		break;
	case 'x':	// x = (-defaultvoice) give everyone a voice by default
		if (!pChannel)
			goto not_in_a_channel;
		pChannel->VoiceDefault_Enable(this);
		break;
	case 'y':	// y = (/defaultvoice) toggle
		if (!pChannel)
			goto not_in_a_channel;
		pChannel->VoiceDefault_Toggle(this);
		break;
	case 'z':	// z = emote
		if (!pChannel)
			goto not_in_a_channel;
		pChannel->Speak_Emote(this, pszMsg, lang );
		break;
	}
}

void CClient::addChatSystemMessage( CHATMSG_TYPE iType, LPCTSTR pszName1, LPCTSTR pszName2, CLanguageID lang )
{
	CUOCommand cmd;
	cmd.ChatReq.m_Cmd = XCMD_ChatReq;
	cmd.ChatReq.m_subcmd = iType;

	if ( iType >= CHATMSG_PlayerTalk && iType <= CHATMSG_PlayerPrivate) // These need the language stuff
		lang.GetStrDef( cmd.ChatReq.m_lang ); // unicode support: pszLang
	else
		memset( cmd.ChatReq.m_lang, 0, sizeof(cmd.ChatReq.m_lang));

	// Convert internal UTF8 to UNICODE for client.
	// ? If we're sending out player names, prefix name with moderator status

	if ( pszName1 == NULL )
		pszName1 = "";
	int len1 = CvtSystemToNUNICODE( cmd.ChatReq.m_uname, MAX_TALK_BUFFER, pszName1 );

	if ( pszName2 == NULL )
		pszName2 = "";
	int len2 = CvtSystemToNUNICODE( cmd.ChatReq.m_uname+len1+1, MAX_TALK_BUFFER, pszName2 );

	int len = sizeof(cmd.ChatReq) + (len1*2) + (len2*2);
	cmd.ChatReq.m_len = len;
	xSendPkt( &cmd, len );
}

//*************************************************************************
// -CChatClient

CChatClient::CChatClient()
{
	m_fChatActive = false;
	m_fReceiving = true;
	m_fAllowWhoIs = true;
}

CChatClient::~CChatClient()
{
	Chat_Quit();	// Are we chatting currently ?
}

CClientPtr CChatClient::GetClient() const
{
	return( STATIC_CAST(CClient,const_cast<CChatClient*>(this)));
}

void CChatClient::Chat_SendMsg(CHATMSG_TYPE iType, LPCTSTR pszName1, LPCTSTR pszName2, CLanguageID lang )
{
	ASSERT( Chat_IsActive());
	GetClient()->addChatSystemMessage(iType, pszName1, pszName2, lang );
}

CGString CChatClient::Chat_GetName() const
{
	CAccountPtr pAccount = GetClient()->GetAccount();
	ASSERT(pAccount);
	return( pAccount->m_sChatName );
}

int CChatClient::Chat_GetNameColor() const 
{
	// What color am i in the channel i'm connected to ?
	// RETURN:
	// 0 = yellow
	// 1 = purple =  moderator
	// 2 = blue = no voice
	// 3 = purple
	// 4 = white = not in channel?
	// 5 = green = GM

	if ( GetClient()->IsPrivFlag( PRIV_GM ))
		return 5;

	CChatChannelPtr pChannel = Channel_Get();
	if (!pChannel) // Must be a system command if these are invalid
	{
		return 4;
	}
	if ( pChannel->Moderator_Is(GetClient()))
		return 1;
	if ( !pChannel->Voice_Has(GetClient()))
		return 2;
	return 0;
}

CGString CChatClient::Chat_GetNameDecorated() const
{
	CGString sName;
	sName.Format( "%d%s", Chat_GetNameColor(), (LPCTSTR) Chat_GetName());
	return sName;
}

void CChatClient::Send_Member( CChatClient* pClientNew )
{
	// Update member list for 1 member change.
	if ( pClientNew == NULL )
		return;
	Chat_SendMsg( CHATMSG_SendPlayerName, pClientNew->Chat_GetNameDecorated());
}

void CChatClient::Chat_Quit()
{
	// Are we in a channel now?
	if ( ! Chat_IsActive())
		return;

	// Remove from old channel (if any)
	CChatChannelPtr pCurrentChannel = Channel_Get();
	if (pCurrentChannel)
	{
		// Remove myself from the channels list of members
		pCurrentChannel->Member_Remove( this );
	}

	// Now tell the chat system you left
	m_fChatActive = false;
}

bool CChatClient::IgnoreList_Is( CClient* pClient ) const
{
	if ( pClient == NULL )
		return false;
	CAccountPtr pAccount = pClient->GetAccount();
	ASSERT(pAccount);
	return( m_IgnoreList.FindObj( pAccount ) >= 0 );
}

int CChatClient::IgnoreList_FindIndex( LPCTSTR pszChatName ) const
{
	// pszChatName = chat name !
	ASSERT( Chat_IsActive());

	CAccountPtr pAccount = g_Accounts.Account_FindChatName( pszChatName );
	if ( pAccount == NULL )
		return -1;

	return m_IgnoreList.FindObj(pAccount);
}

void CChatClient::IgnoreList_Add( LPCTSTR pszChatName )
{
	// CHATMSG_AlreadyIgnoringMax,			// 1 - You are already ignoring the maximum amount of people

	CAccountPtr pAccount = g_Accounts.Account_FindChatName( pszChatName );
	if ( pAccount == NULL )
	{
		Chat_SendMsg( CHATMSG_NoPlayer, pszChatName );
		return;
	}
	int i = m_IgnoreList.AttachObj(pAccount);
	if ( i < 0 )
	{
		Chat_SendMsg( CHATMSG_AlreadyIgnoringPlayer, pszChatName );
	}
	else
	{
		Chat_SendMsg( CHATMSG_NowIgnoring, pszChatName ); // This message also takes the ignored person off the clients local list of channel members
	}
}

void CChatClient::IgnoreList_Remove( int i, const char* pszChatName )
{
	m_IgnoreList.RemoveAt(i);

	if ( pszChatName == NULL )
		return;

	Chat_SendMsg(CHATMSG_NoLongerIgnoring, pszChatName);

	// Resend the un ignored member to the client's local list of members (but only if they are currently in the same channel!)
	if (m_pChannel)
	{
		Send_Member( m_pChannel->Member_Find(pszChatName));
	}
}

void CChatClient::IgnoreList_Remove(const char* pszChatName)
{
	int i = IgnoreList_FindIndex(pszChatName);
	if ( i >= 0 )
		IgnoreList_Remove(i,pszChatName);
	else
		Chat_SendMsg(CHATMSG_NotIgnoring, pszChatName);
}

void CChatClient::IgnoreList_Toggle(const char* pszChatName)
{
	int i = IgnoreList_FindIndex( pszChatName );
	if ( i>=0 )
	{
		IgnoreList_Remove(i,pszChatName);
	}
	else
	{
		IgnoreList_Add( pszChatName );
	}
}

void CChatClient::IgnoreList_Clear()
{
	m_IgnoreList.RemoveAll();
	Chat_SendMsg(CHATMSG_NoLongerIgnoringAnyone);
}

void CChatClient::RenameChannel(const char* pszName)
{
	CChatChannelPtr pChannel = Channel_Get();
	if (!pChannel)
	{
		Chat_SendMsg(CHATMSG_MustBeInAConference);
		return;
	}

	if (!pChannel->Moderator_Is(GetClient()))
	{
		Chat_SendMsg(CHATMSG_MustHaveOps);
		return;
	}

	pChannel->RenameChannel(this, pszName);
}

void CChatClient::Receiving_Toggle()
{
	m_fReceiving = !m_fReceiving;
	Chat_SendMsg((m_fReceiving) ? CHATMSG_ReceivingPrivate : CHATMSG_NoLongerReceivingPrivate);
}

void CChatClient::WhoIs_Permit()
{
	if (m_fAllowWhoIs)
		return;
	m_fAllowWhoIs = true;
	Chat_SendMsg(CHATMSG_ShowingName);
}

void CChatClient::WhoIs_Forbid()
{
	if (!m_fAllowWhoIs)
		return;
	m_fAllowWhoIs = false;
	Chat_SendMsg(CHATMSG_NotShowingName);
}

void CChatClient::WhoIs_Toggle()
{
	m_fAllowWhoIs = !m_fAllowWhoIs;
	Chat_SendMsg((m_fAllowWhoIs) ? CHATMSG_ShowingName : CHATMSG_NotShowingName);
}

//****************************************************************************
// -CChatChannel

void CChatChannel::RenameChannel(CChatClient* pSrc, const char* pszName)
{
	// Ask the chat system if the new name is ok
	if ( ! CChat::IsValidName( pszName, false ))
	{
		pSrc->Chat_SendMsg(CHATMSG_InvalidConferenceName);
		return;
	}
	// Ask the chat system if the new name is already taken
	if ( g_Serv.m_Chats.IsDuplicateChannelName(pszName))
	{
		pSrc->Chat_SendMsg(CHATMSG_AlreadyAConference);
		return;
	}
	// Tell the channel members our name changed
	Send_Broadcast(CHATMSG_ConferenceRenamed, GetName(), pszName);
	// Delete the old name from all chat clients
	SendDeleteChannel();
	// Do the actual renaming
	SetChannelName(pszName);
	// Update all channel members' current channel bar
	Send_Broadcast(CHATMSG_UpdateChannelBar, pszName, "");
	// Send out the new name to all chat clients so they can join
	g_Serv.m_Chats.SendNewChannel(this);
}

void CChatChannel::ChangePassword(CClient* pSrc, const char* pszPassword)
{
	if (!Moderator_Is(pSrc))
	{
		pSrc->Chat_SendMsg(CHATMSG_MustHaveOps);
		return;
	}

	SetPassword(pszPassword);
	g_Serv.m_Chats.SendNewChannel(pSrc->Channel_Get());
	Send_Broadcast(CHATMSG_PasswordChanged, "","");
}

int CChatChannel::Member_FindIndex(const char* pszChatName) const
{
	// Find this ChatName in the members ?
	for(int i = 0; i < m_Members.GetSize(); i++)
	{
		if ( ! _stricmp( m_Members[i]->Chat_GetName(), pszChatName ))
			return i;
	}
	return -1;
}

CClientPtr CChatChannel::Member_Find(const char* pszChatName) const
{
	int i = Member_FindIndex( pszChatName );
	if ( i < 0 )
		return( NULL );
	return m_Members[i];
}

void CChatChannel::Member_WhoIs( CClient* pSrc, const char* pszChatName)
{
	CClientPtr pClient = Member_Find(pszChatName);
	CCharPtr pChar = pClient->GetClient()->GetChar();
	if (!pClient||!pChar)
	{
		pSrc->Chat_SendMsg(CHATMSG_NoPlayer, pszChatName);
	}
	else if (pClient->WhoIs_Get())
	{
		pSrc->Chat_SendMsg(CHATMSG_PlayerKnownAs, pszChatName, pChar->GetName());
	}
	else
	{
		pSrc->Chat_SendMsg(CHATMSG_PlayerIsAnonymous, pszChatName);
	}
}

bool CChatChannel::Member_Join( CClient* pClient, const char* pszPassword )
{
	CChatChannelPtr pCurrentChannel = pClient->Channel_Get();
	if ( pCurrentChannel == this )
	{
		// Is it the same channel as the one I'm already in?
		// Tell them and return
		pClient->Chat_SendMsg(CHATMSG_AlreadyInConference, GetName());
		return false;
	}

	// If there's a password, is it the correct one?
	if ( strcmp( GetPassword(), pszPassword))
	{
		pClient->addChatSystemMessage(CHATMSG_IncorrectPassword);
		return false;
	}

	// Leave the old channel 1st
	// Remove from old channel (if any)
	if (pCurrentChannel)
	{
		// Remove myself from the channels list of members
		pCurrentChannel->Member_Remove(pClient);

		// Since we left, clear all members from our client that might be in our list from the channel we just left
		pClient->addChatSystemMessage(CHATMSG_ClearMemberList);
	}

	// Now join a new channel
	// TODO: Ask the channel if it's ok to join, if not return here
	// Add all the members of the channel to the clients list of channel participants
	Send_Members(pClient);

	// Add ourself to the channels list of members
	pClient->Channel_Set(this);

	m_Members.Add( pClient );

	// See if only moderators have a voice by default
	if (!VoiceDefault_Get() && !Moderator_Is(pClient))
	{
		// If only moderators have a voice by default, then add this member to the list of no voices
		Voice_Set(pClient, false);
	}
	// Set voice status

	// Set the channel name title bar
	pClient->addChatSystemMessage(CHATMSG_UpdateChannelBar, GetName());

	// Now send out my name to all clients in this channel
	Send_MemberChange(pClient);
	return true;
}

void CChatChannel::Member_Remove( CChatClient* pClient )
{
	for ( int i = 0; i < m_Members.GetSize(); i++)
	{
		// Tell the other clients in this channel (if any) you are leaving (including yourself)
		CClientPtr pClient = m_Members[i];
		ASSERT(pClient);
		pClient->addChatSystemMessage(CHATMSG_RemoveMember, pClient->Chat_GetName());

		// Remove from channel's list of participants
		if (m_Members[i] == pClient)
			m_Members.RemoveAt(i);
	}

	// Update our persona
	pClient->Channel_Set(NULL);

	// Am I the last one here? Delete it from all other clients?
	if (m_Members.GetSize() <= 0)
	{
		// If noone is left, tell the chat system to delete it from memory
		DeleteThis();
	}
}

bool CChatChannel::Member_Remove(LPCTSTR pszChatName)
{
	int i = Member_FindIndex( pszChatName );
	if ( i >= 0 )
	{
		Member_Remove(m_Members[i]);
		return true;
	}
	return false;
}

void CChatChannel::Member_Kick( CClient* pSrc, CClient* pClient )
{
	ASSERT( pClient );

	LPCTSTR pszChatNameBy;
	if (pSrc) // If NULL, then an ADMIN or a GM did it
	{
		pszChatNameBy = pSrc->Chat_GetName();
		if (!Moderator_Is(pSrc))
		{
			pSrc->Chat_SendMsg(CHATMSG_MustHaveOps);
			return;
		}
	}
	else
	{
		pszChatNameBy = "SYSTEM";
	}

	LPCTSTR pszName = pClient->Chat_GetName();

	// Kicking this person...remove from list of moderators first
	if (Moderator_Is(pClient))
	{
		Moderator_Set(pClient, false);
		Send_MemberChange(pClient);
		Send_Broadcast(CHATMSG_PlayerNoLongerModerator, pszName, "");
		pClient->Chat_SendMsg(CHATMSG_RemovedListModerators, pszChatNameBy);
	}

	// Now kick them
	// Remove them from the channels list of members
	Member_Remove(pClient);

	if ( m_Members.GetSize())
	{
		// Tell the remain members about this
		Send_Broadcast(CHATMSG_PlayerIsKicked, pszName, "");

		// Now clear their channel member list
		pClient->Chat_SendMsg(CHATMSG_ClearMemberList);
		// And give them the bad news
		pClient->Chat_SendMsg(CHATMSG_ModeratorHasKicked, pszChatNameBy);
	}
}

void CChatChannel::Member_KickAll(CClient* pClientException)
{
	// We may delete ourself when done !
	for ( int i=0; i<m_Members.GetSize(); i++)
	{
		if ( m_Members[i] == pClientException) // If it's not me, then kick them
			continue;
		Member_Kick( pClientException, m_Members[i] );
	}
}

void CChatChannel::Send_Broadcast(CHATMSG_TYPE iType, const char* pszNameFrom, const char* pszText, CLanguageID lang, bool fOverride )
{
	CGString sName;
	CClientPtr pClientSender = Member_Find(pszNameFrom);

	if ( iType >= CHATMSG_PlayerTalk && iType <= CHATMSG_PlayerPrivate) // Only chat, emote, and privates get a color status number
	{
		sName = CChat::DecorateName( pClientSender, fOverride );
	}
	else
	{
		sName = pszNameFrom;
	}

	for (int i = 0; i < m_Members.GetSize(); i++)
	{
		// Check to see if the recipient is ignoring messages from the sender
		// Just pass over it if it's a regular talk message
		if ( m_Members[i]->IgnoreList_Is(pClientSender))
		{
			// If it's a private message, then tell the sender the recipient is ignoring them
			if ( iType == CHATMSG_PlayerPrivate && pClientSender )
			{
				pClientSender->Chat_SendMsg(CHATMSG_PlayerIsIgnoring, m_Members[i]->Chat_GetName());
			}
		}

		m_Members[i]->Chat_SendMsg(iType, sName, pszText, lang );
	}
}

void CChatChannel::Send_Members(CChatClient* pClient)
{
	// Send all the members to a single client.
	for (int i = 0; i < m_Members.GetSize(); i++)
	{
		pClient->Send_Member( m_Members[i] );
	}
}

void CChatChannel::Send_MemberChange(CChatClient* pClient)
{
	// Send a single change to all members.
	for (int i = 0; i < m_Members.GetSize(); i++)
	{
		m_Members[i]->Send_Member( pClient );
	}
}

void CChatChannel::Speak_Emote( CClient* pSrc, const char* pszMsg, CLanguageID lang )
{
	ASSERT(pSrc);
	ASSERT(pSrc->Channel_Get()==this);
	if (Voice_Has(pSrc))
		Send_Broadcast( CHATMSG_PlayerEmote, pSrc->Chat_GetName(), pszMsg, lang );
	else
		pSrc->Chat_SendMsg(CHATMSG_RevokedSpeaking);
}

void CChatChannel::Speak_Private( CClient* pClientFrom, const char* pszChatNameTo, const char* pszMsg, CLanguageID lang )
{
	CClientPtr pClientTo = Member_Find(pszChatNameTo);
	if (!pClientTo)
	{
		pClientFrom->Chat_SendMsg(CHATMSG_NoPlayer, pszChatNameTo);
		return;
	}
	if (!pClientTo->Receiving_IsAllowed())
	{
		pClientFrom->Chat_SendMsg(CHATMSG_PlayerNotReceivingPrivate, pszChatNameTo );
		return;
	}

	// Can always send private messages to moderators (but only if they are receiving)
	bool fHasVoice = Voice_Has(pClientFrom);
	if ( !fHasVoice && !Moderator_Is(pClientTo))
	{
		pClientFrom->Chat_SendMsg(CHATMSG_RevokedSpeaking);
		return;
	}

	if (pClientTo->IgnoreList_Is(pClientFrom)) // See if ignoring you
	{
		pClientFrom->Chat_SendMsg(CHATMSG_PlayerIsIgnoring, pszChatNameTo );
		return;
	}

	CGString sName = pClientFrom->Chat_GetNameDecorated();

	// Echo to the sending client so they know the message went out
	pClientFrom->Chat_SendMsg(CHATMSG_PlayerPrivate, sName, pszMsg);

	// If the sending and receiving are different send it out to the receiver
	if (pClientTo != pClientFrom)
	{
		pClientTo->Chat_SendMsg(CHATMSG_PlayerPrivate, sName, pszMsg);
	}
}

void CChatChannel::Speak_Normal( CClient* pSrc, const char* pszText, CLanguageID lang )
{
	// Do I have a voice?
	if (!Voice_Has(pSrc))
	{
		pSrc->Chat_SendMsg(CHATMSG_RevokedSpeaking);
		return;
	}
	Send_Broadcast(CHATMSG_PlayerTalk, pSrc->Chat_GetName(), pszText, lang );
}

void CChatChannel::VoiceDefault_Toggle( CClient* pSrc )
{
	if (!Moderator_Is(pSrc))
	{
		pSrc->Chat_SendMsg(CHATMSG_MustHaveOps);
		return;
	}
	if (VoiceDefault_Get())
		Send_Broadcast(CHATMSG_ModeratorsSpeakDefault, "", "");
	else
		Send_Broadcast(CHATMSG_SpeakingByDefault, "", "");

	VoiceDefault_Set(!VoiceDefault_Get());
}

void CChatChannel::VoiceDefault_Disable(CClient* pSrc)
{
	if (VoiceDefault_Get())
		VoiceDefault_Toggle(pSrc);
}

void CChatChannel::VoiceDefault_Enable(CClient* pSrc)
{
	if (!VoiceDefault_Get())
		VoiceDefault_Toggle(pSrc);
}

bool CChatChannel::Voice_Has( const CClient* pClient ) const
{
	if ( pClient == NULL )
		return false;
	CAccountPtr pAccount = pClient->GetAccount();
	ASSERT(pAccount);
	return( m_NoVoices.FindObj( pAccount ) >= 0 );
}

void CChatChannel::Voice_Set( CClient* pClient, bool fFlag )
{
	// See if they have no voice already
	if ( pClient == NULL )
		return;
	CAccountPtr pAccount = pClient->GetAccount();
	ASSERT(pAccount);
	if (fFlag)
	{
		m_NoVoices.DetachObj( (const CResourceObj*)(CAccount*)pAccount );
	}
	else 
	{
		m_NoVoices.AttachObj( pAccount );
	}
}

void CChatChannel::Voice_Grant( CClient* pSrc, const char* pszChatNameTo)
{
	if (!Moderator_Is(pSrc))
	{
		pSrc->Chat_SendMsg(CHATMSG_MustHaveOps);
		return;
	}
	CClientPtr pClientTo = Member_Find(pszChatNameTo);
	if (!pClientTo)
	{
		pSrc->Chat_SendMsg(CHATMSG_NoPlayer, pszChatNameTo);
		return;
	}
	if (Voice_Has(pClientTo))
		return;
	Voice_Set(pClientTo, true);
	Send_MemberChange(pClientTo); // Update the color
	pClientTo->Chat_SendMsg(CHATMSG_ModeratorGrantSpeaking, pSrc->Chat_GetName());
	Send_Broadcast(CHATMSG_PlayerNowSpeaking, pszChatNameTo, "", "");
}

void CChatChannel::Voice_Revoke(CClient* pSrc, const char* pszChatNameTo)
{
	if (!Moderator_Is(pSrc))
	{
		pSrc->Chat_SendMsg(CHATMSG_MustHaveOps);
		return;
	}
	CClientPtr pClientTo = Member_Find(pszChatNameTo);
	if (!pClientTo)
	{
		pSrc->Chat_SendMsg(CHATMSG_NoPlayer, pszChatNameTo);
		return;
	}
	if (!Voice_Has(pClientTo))
		return;
	Voice_Set(pClientTo, false);
	Send_MemberChange(pClientTo); // Update the color
	pClientTo->Chat_SendMsg(CHATMSG_ModeratorRemovedSpeaking, pSrc->Chat_GetName());
	Send_Broadcast(CHATMSG_PlayerNoSpeaking, pszChatNameTo, "", "");
}

void CChatChannel::Voice_Toggle(CClient* pSrc, const char* pszChatNameTo)
{
	CClientPtr pClientTo = Member_Find(pszChatNameTo);
	if (!pClientTo)
	{
		pSrc->Chat_SendMsg(CHATMSG_NoPlayer, pszChatNameTo);
		return;
	}
	if (!Voice_Has(pClientTo)) // (This also returns true if this person is not in the channel)
		Voice_Grant(pSrc, pszChatNameTo); // this checks and reports on membership
	else
		Voice_Revoke(pSrc, pszChatNameTo); // this checks and reports on membership
}

bool CChatChannel::Moderator_Is( const CClient* pClient) const
{
	if ( pClient == NULL )
		return false;
	CAccountPtr pAccount = pClient->GetAccount();
	ASSERT(pAccount);
	return( m_Moderators.FindObj( pAccount ) >= 0 );
}

void CChatChannel::Moderator_Set(CClient* pClient, bool fFlag)
{
	// See if they are already a moderator
	if ( pClient == NULL )
		return;
	CAccountPtr pAccount = pClient->GetAccount();
	ASSERT(pAccount);
	if (!fFlag)
	{
		m_Moderators.DetachObj( (const CResourceObj*)(CAccount*)pAccount );
	}
	else
	{
		m_Moderators.AttachObj( pAccount );
	}
}

void CChatChannel::Moderator_Grant(CClient* pSrc, const char* pszChatNameTo)
{
	if (!Moderator_Is(pSrc))
	{
		pSrc->Chat_SendMsg(CHATMSG_MustHaveOps);
		return;
	}
	CClientPtr pClientTo = Member_Find(pszChatNameTo);
	if (!pClientTo)
	{
		pSrc->Chat_SendMsg(CHATMSG_NoPlayer, pszChatNameTo);
		return;
	}
	if (Moderator_Is(pClientTo))
		return;
	Moderator_Set(pClientTo, true);
	Send_MemberChange(pClientTo); // Update the color
	Send_Broadcast(CHATMSG_PlayerIsAModerator, pClientTo->Chat_GetName(), "", "");
	pClientTo->Chat_SendMsg(CHATMSG_YouAreAModerator, pSrc->Chat_GetName());
}

void CChatChannel::Moderator_Revoke(CClient* pSrc, const char* pszChatNameTo)
{
	if (!Moderator_Is(pSrc ))
	{
		pSrc->Chat_SendMsg(CHATMSG_MustHaveOps);
		return;
	}
	CClientPtr pClientTo = Member_Find(pszChatNameTo);
	if (!pClientTo)
	{
		pSrc->Chat_SendMsg(CHATMSG_NoPlayer, pszChatNameTo);
		return;
	}
	if (!Moderator_Is(pClientTo))
		return;
	Moderator_Set(pClientTo, false);
	Send_MemberChange(pClientTo); // Update the color
	Send_Broadcast(CHATMSG_PlayerNoLongerModerator, pClientTo->Chat_GetName(), "", "");
	pClientTo->Chat_SendMsg(CHATMSG_RemovedListModerators, pSrc->Chat_GetName());
}

void CChatChannel::Moderator_Toggle(CClient* pSrc, const char* pszChatNameTo)
{
	CClientPtr pClientTo = Member_Find(pszChatNameTo);
	if (!pClientTo)
	{
		pSrc->Chat_SendMsg(CHATMSG_NoPlayer, pszChatNameTo);
		return;
	}
	if (!Moderator_Is(pClientTo))
		Moderator_Grant(pSrc, pszChatNameTo);
	else
		Moderator_Revoke(pSrc, pszChatNameTo);
}

void CChatChannel::SendDeleteChannel()	// tell everyone about it first.
{
	// Send a delete channel name message to all clients using the chat system
	CClientPtr pClient = g_Serv.GetClientHead();
	for ( ; pClient; pClient = pClient->GetNext())
	{
		if ( ! pClient->Chat_IsActive())
			continue;
		pClient->addChatSystemMessage( CHATMSG_RemoveChannelName, GetName());
	}
}

void CChatChannel::DeleteThis()
{
	DEBUG_CHECK(m_Members.GetSize()<=0);
	Member_KickAll(NULL);
	SendDeleteChannel();
	RemoveSelf();	// just dec the ref count. (should delete automatically)
}

//****************************************************************************
// -CChat

static LPCTSTR const sm_szMethods[] =
{
	"ALLKICK",
	"BC",
	"BCALL",
	"CHATSOK",
	"CLEARIGNORE",
	"KILLCHATS",
	"NOCHATS",
	"SYSMSG",
	"WHEREIS",
};

void CChat::DoCommand( LPCTSTR pszCommand, CClient* pSrc )
{
	ASSERT(pSrc);

	char* pszArgs = strchr( pszCommand, ' ');
	if ( pszArgs )
		pszArgs ++;
	else
		pszArgs = "";

	CGString sFrom;
	CChatChannelPtr pChannel = pSrc->Channel_Get();

	// static bool fFlipper = false;
	// static int iCounter = 0;

	int index = FindTableHeadSorted( pszCommand, sm_szMethods, COUNTOF(sm_szMethods));
	if ( index < 0 )
	{
		CGString sMsg;
		sMsg.Format( "Unknown command: '%s'", pszCommand );
		CGString sFrom;
		sFrom = DecorateName( NULL, true);
		pSrc->Chat_SendMsg(CHATMSG_PlayerTalk, sFrom, sMsg);
		return;
	}

	switch (index)
	{
	case 0: // "ALLKICK"
		if (!pChannel)
		{
			pSrc->Chat_SendMsg(CHATMSG_MustBeInAConference);
			break;
		}
		if (!pChannel->Moderator_Is(pSrc))
		{
			pSrc->Chat_SendMsg(CHATMSG_MustHaveOps);
			break;
		}
		pChannel->Member_KickAll(pSrc);
		sFrom = DecorateName( NULL, true);
		pSrc->Chat_SendMsg(CHATMSG_PlayerTalk, sFrom, "All members have been kicked!", "");
		break;

	case 1: // "BC"
		if ( ! pSrc->GetClient()->IsPrivFlag( PRIV_GM ))
		{
		need_gm_privs:
			sFrom = DecorateName( NULL, true);
			pSrc->Chat_SendMsg(CHATMSG_PlayerTalk, sFrom, "You need to have GM privs to use this command.");
			break;
		}
		Chat_Broadcast(pSrc, pszArgs);
		return;

	case 2: // "BCALL"
		if ( ! pSrc->GetClient()->IsPrivFlag( PRIV_GM ))
			goto need_gm_privs;
		Chat_Broadcast(pSrc, pszArgs, "", true);
		return;
	case 3: // "CHATSOK"
		if ( ! pSrc->GetClient()->IsPrivFlag( PRIV_GM ))
			goto need_gm_privs;
		if (!m_fChatsOK)
		{
			m_fChatsOK = true;
			Chat_Broadcast(pSrc, "Conference creation is enabled.");
		}
		break;
	case 4: // "CLEARIGNORE"
		pSrc->IgnoreList_Clear();
		break;
	case 5: // "KILLCHATS"
		if ( ! pSrc->GetClient()->IsPrivFlag( PRIV_GM ))
			goto need_gm_privs;
		KillChannels();
		break;
	case 6: // "NOCHATS"
		if ( ! pSrc->GetClient()->IsPrivFlag( PRIV_GM ))
			goto need_gm_privs;
		if (m_fChatsOK)
		{
			Chat_Broadcast(pSrc, "Conference creation is now disabled.");
			m_fChatsOK = false;
		}
		break;
	case 7: // "SYSMSG"
		if ( ! pSrc->GetClient()->IsPrivFlag( PRIV_GM ))
			goto need_gm_privs;
		Chat_Broadcast(pSrc, pszArgs, "", true);
		break;
	case 8:	// "WHEREIS"
		WhereIs(pSrc, pszArgs);
		break;

	default:
		DEBUG_CHECK(0);
		return;
	}
}

void CChat::KillChannels()
{
	CChatChannelPtr pChannel = GetFirstChannel();
	// First /kick everyone
	for ( ; pChannel != NULL; pChannel = pChannel->GetNext())
		pChannel->Member_KickAll();
	m_Channels.Empty();
};

void CChat::WhereIs(CChatClient* pSrc, LPCTSTR pszChatName ) const
{
	CClientPtr pClient = g_Serv.GetClientHead();
	for ( ; pClient; pClient = pClient->GetNext())
	{
		if ( ! _stricmp( pClient->Chat_GetName(), pszChatName))
			continue;

		CGString sMsg;
		if (! pClient->Chat_IsActive() || !pClient->Channel_Get())
			sMsg.Format( "%s is not currently in a conference.", pszChatName);
		else
			sMsg.Format( "%s is in conference '%s'.", (LPCTSTR) pszChatName, (LPCTSTR) pClient->Channel_Get()->GetName());
		CGString sFrom;
		sFrom = DecorateName( NULL, true);
		pSrc->Chat_SendMsg( CHATMSG_PlayerTalk, sFrom, sMsg );
		return;
	}

	pSrc->Chat_SendMsg(CHATMSG_NoPlayer, pszChatName);
}

bool CChat::IsValidName( const char* pszChatName, bool fPlayer ) // static
{
	// Channels can have spaces, but not player names
	if ( pszChatName[0] == '\0' )
		return false;
	if ( ! _stricmp(pszChatName, "SYSTEM"))
		return false;
	for (int i = 0; pszChatName[i]; i++)
	{
		if ( pszChatName[i] == ' ' )
		{
			if (fPlayer)
				return false;
			continue;
		}
		if ( ! isalnum(pszChatName[i]))
			return( false );
		if ( i > MAX_NAME_SIZE )
			return false;
	}
	if ( g_Cfg.IsObscene(pszChatName))
		return false;
	return true;
}

CGString CChat::DecorateName( const CChatClient* pClient, bool fSystem) // static
{
	// 0 = yellow
	// 1 = purple
	// 2 = blue
	// 3 = purple
	// 4 = white
	// 5 = green

	if (!pClient) // Must be a system command if these are invalid
	{
		CGString sName;
		sName.Format( "%i%s", fSystem?5:4, "SYSTEM" );
		return sName;
	}

	return( pClient->Chat_GetNameDecorated());
}

void CChat::SendNewChannel(CChatChannel* pNewChannel)
{
	// Send this new channel name to all clients using the chat system
	CClientPtr pClient = g_Serv.GetClientHead();
	for ( ; pClient; pClient = pClient->GetNext())
	{
		if ( ! pClient->Chat_IsActive())
			continue;
		pClient->addChatSystemMessage(CHATMSG_SendChannelName, pNewChannel->GetName(), pNewChannel->GetPassModeString());
	}
}

CChatChannelPtr CChat::FindChannel(LPCTSTR pszChannel) const
{
	CChatChannelPtr pChannel = GetFirstChannel();
	for ( ; pChannel != NULL; pChannel = pChannel->GetNext())
	{
		if (!_stricmp(pChannel->GetName(), pszChannel))
			break;
	}
	return pChannel;
};

void CChat::Chat_Broadcast( CClient* pClientFrom, LPCTSTR pszText, CLanguageID lang, bool fOverride )
{
	// NOTE: pClientFrom can be NULL
	CClientPtr pClient = g_Serv.GetClientHead();
	for ( ; pClient; pClient = pClient->GetNext())
	{
		if ( ! pClient->Chat_IsActive())
			continue;
		if ( fOverride || ( ! fOverride && pClient->Receiving_IsAllowed()))
		{
			CGString sName;
			sName = DecorateName( pClientFrom, fOverride);
			pClient->Chat_SendMsg(CHATMSG_PlayerTalk, sName, pszText, lang );
		}
	}
}

void CChat::CreateJoinChannel(CClient* pSrc, LPCTSTR pszName, LPCTSTR pszPassword)
{
	if ( ! IsValidName( pszName, false ))
	{
		pSrc->addChatSystemMessage( CHATMSG_InvalidConferenceName );
		return;
	}
	if (IsDuplicateChannelName(pszName))
	{
		pSrc->addChatSystemMessage( CHATMSG_AlreadyAConference );
		return;
	}

	if ( CreateChannel(pszName, ((pszPassword != NULL) ? pszPassword : ""), pSrc))
	{
		JoinChannel(pSrc, pszName, ((pszPassword != NULL) ? pszPassword : ""));
	}
}

bool CChat::CreateChannel(const char* pszName, const char* pszPassword, CClient* pClient)
{
	if (!m_fChatsOK)
	{
		CGString sName;
		sName = DecorateName( NULL, true);
		pClient->Chat_SendMsg(CHATMSG_PlayerTalk, sName, "Conference creation is disabled.");
		return false;
	}

	CChatChannelPtr pChannel = new CChatChannel( pszName, pszPassword );
	m_Channels.InsertTail( pChannel );
	pChannel->Moderator_Set(pClient);
	// Send all clients with an open chat window the new channel name
	SendNewChannel(pChannel);
	return true;
}

bool CChat::JoinChannel(CClient* pClient, const char* pszChannel, const char* pszPassword)
{
	// Are we in a channel now?
	CChatChannelPtr pNewChannel = FindChannel(pszChannel);
	if (!pNewChannel)
	{
		pClient->addChatSystemMessage(CHATMSG_NoConference, pszChannel );
		return false;
	}

	return pNewChannel->Member_Join( pClient, pszPassword );
}


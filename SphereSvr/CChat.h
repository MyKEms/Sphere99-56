//
// CChat.h
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#ifndef _INC_CCHAT_H
#define _INC_CCHAT_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class CChat;
class CChatChannel;
typedef CRefPtr<CChatChannel> CChatChannelPtr;

class CChatClient
{
	// This is owned by CClient.
	friend CChat;
	friend CChatChannel;
private:
	bool m_fReceiving;
	bool m_fAllowWhoIs;
	CChatChannelPtr m_pChannel;	// I can only be a member of one chan at a time.
	CUIDRefArray m_IgnoreList;			// CAccount UID list of ignored 
protected:
	bool m_fChatActive;		// only send chat updates to these.

public:
	CClientPtr GetClient() const;

	bool Chat_IsActive() const
	{
		return( m_fChatActive );
	}
	void Chat_SendMsg( CHATMSG_TYPE iType, LPCTSTR pszName1 = NULL, LPCTSTR pszName2 = NULL, CLanguageID lang = 0 );
	int Chat_GetNameColor() const;
	CGString Chat_GetName() const;
	CGString Chat_GetNameDecorated() const;
	void Chat_Quit();
	void Send_Member( CChatClient* pClientNew );

	CChatChannelPtr Channel_Get() const { return m_pChannel; }
	void Channel_Set(CChatChannel* pChannel) { m_pChannel = pChannel; }
	void RenameChannel(LPCTSTR pszName);

	bool Receiving_IsAllowed() const { return m_fReceiving; }
	void Receiving_Set(bool fOnOff)
	{
		if (m_fReceiving != fOnOff)
			Receiving_Toggle();
	}
	void Receiving_Toggle();

	bool WhoIs_Get() const { return m_fAllowWhoIs; }
	void WhoIs_Permit();
	void WhoIs_Forbid();
	void WhoIs_Toggle();

	bool IgnoreList_Is( CClient* pClient ) const;
	int  IgnoreList_FindIndex( LPCTSTR pszChatName) const;
	void IgnoreList_Add(LPCTSTR pszChatName);
	void IgnoreList_Remove(LPCTSTR pszChatName);
	void IgnoreList_Remove(int i, LPCTSTR pszChatName);
	void IgnoreList_Toggle(LPCTSTR pszChatName);
	void IgnoreList_Clear();

	CChatClient();
	virtual ~CChatClient();
};

class CChatChannel : public CGObListRec, public CRefObjDef
{
	// a number of clients can be attached to this chat channel.
public:
	DECLARE_LISTREC_REF(CChatChannel);
private:
	friend class CChatClient;
	friend class CChat;
	CGString m_sName;		// Name of the channel.
	CGString m_sPassword;
	bool m_fVoiceDefault;	// give others voice by default.
public:
	CGRefArray<CClient> m_Members;	// Current list of members in this channel

	CUIDRefArray m_NoVoices;	// CAccount list of channel members with no voice
	CUIDRefArray m_Moderators;	// CAccount list of channel's moderators (may or may not be currently in the channel)
	CUIDRefArray m_BannedAccounts; // CAccount list of banned accounts in this channel

public:
	virtual void DeleteThis();
	void SendDeleteChannel();	// tell everyone about it first.

	//***********
	// Channel Name
	virtual CGString GetName() const
	{
		return( m_sName );
	}
	void SetChannelName(LPCTSTR pszName)
	{
		m_sName = pszName;
	}
	void RenameChannel(CChatClient* pBy, LPCTSTR pszName);

	//***********
	// Password
	bool IsPassworded() const
	{
		return ( !m_sPassword.IsEmpty());
	}
	LPCTSTR GetPassModeString() const
	{
		// (client needs this) "0" = not passworded, "1" = passworded
		return(( IsPassworded()) ? "1" : "0" );
	}
	LPCTSTR GetPassword() const
	{
		return( m_sPassword );
	}
	void SetPassword( LPCTSTR pszPassword )
	{
		m_sPassword = pszPassword;
	}
	void ChangePassword(CClient* pClientBy, LPCTSTR pszPassword);

	//***********
	// Member List
	void Send_Members(CChatClient* pMember);
	void Send_MemberChange(CChatClient* pMemberChanged);
	void Send_Broadcast(CHATMSG_TYPE iType, LPCTSTR pszText1, LPCTSTR pszText2, CLanguageID lang = 0, bool fOverride = false);

	void Member_WhoIs( CClient* pBy, LPCTSTR pszMember );
	CClientPtr Member_Find(LPCTSTR pszName) const;
	int  Member_FindIndex( LPCTSTR pszName ) const;
	bool Member_Join( CClient* pMember, const char* pszPassword );
	void Member_Kick( CClient* pByMember, CClient* pMember );
	void Member_Remove(CChatClient* pMember);
	bool Member_Remove(LPCTSTR pszName);
	void Member_KickAll(CClient* pMember = NULL);

	//***********
	// Voice
	bool VoiceDefault_Get()  const { return m_fVoiceDefault; }
	void VoiceDefault_Set(bool fVoiceDefault) { m_fVoiceDefault = fVoiceDefault; }
	void VoiceDefault_Toggle(CClient* pBy);
	void VoiceDefault_Disable(CClient* pBy);
	void VoiceDefault_Enable(CClient* pBy);

	void Voice_Set(CClient* pClient, bool fFlag = true);
	bool Voice_Has(const CClient* pClient) const;
	void Voice_Grant(CClient* pByMember, LPCTSTR pszName);
	void Voice_Revoke(CClient* pByMember, LPCTSTR pszName);
	void Voice_Toggle(CClient* pByMember, LPCTSTR pszName);

	//***********
	// Moderator
	bool Moderator_Is(const CClient* pClient) const;
	void Moderator_Set(CClient* pClient, bool fFlag = true);
	void Moderator_Grant(CClient* pByMember, LPCTSTR pszName);
	void Moderator_Revoke(CClient* pByMember, LPCTSTR pszName);
	void Moderator_Toggle(CClient* pByMember, LPCTSTR pszName);

	//***********
	// Speaking
	void Speak_Emote(CClient* pFrom, LPCTSTR pszMsg, CLanguageID lang = 0 );
	void Speak_Normal(CClient* pFrom, LPCTSTR pszMsg, CLanguageID lang = 0 );
	void Speak_Private(CClient* pFrom, LPCTSTR pszTo, LPCTSTR  pszMsg, CLanguageID lang = 0);

	CChatChannel(LPCTSTR pszName, LPCTSTR pszPassword = NULL)
	{
		m_sName = pszName;
		m_sPassword = pszPassword;
		m_fVoiceDefault = true;
	};
};

class CChat
{
	// All the chat channels.
	friend class CClient;
	friend class CChatClient;
	friend class CChatChannel;
private:
	bool m_fChatsOK;	// allowed to create new chats ?
	CGObListType<CChatChannel> m_Channels;		// CChatChannel // List of chat channels.
private:
	void DoCommand( LPCTSTR pszMsg, CClient* pSrc);
	void DeleteChannel(CChatChannel* pChannel);
	void WhereIs(CChatClient* pBy, LPCTSTR pszName) const;
	void KillChannels();
	bool JoinChannel(CClient* pMember, LPCTSTR pszChannel, LPCTSTR pszPassword);
	bool CreateChannel(LPCTSTR pszName, LPCTSTR pszPassword, CClient* pMember);
	void CreateJoinChannel(CClient* pByMember, LPCTSTR pszName, LPCTSTR pszPassword);
	CChatChannelPtr FindChannel(LPCTSTR pszChannel) const;

public:
	CChatChannelPtr GetFirstChannel() const
	{
		return m_Channels.GetHead();
	}

	static bool IsValidName(LPCTSTR pszName, bool fPlayer);

	void SendDeleteChannel(CChatChannel* pChannel);
	void SendNewChannel(CChatChannel* pNewChannel);
	bool IsDuplicateChannelName(const char* pszName) const
	{
		return FindChannel(pszName) != NULL;
	}

	void Chat_Broadcast(CClient* pFrom, LPCTSTR pszText, CLanguageID lang = 0, bool fOverride = false);
	static CGString DecorateName( const CChatClient* pMember = NULL, bool fSystem = false );

	CChat()
	{
		m_fChatsOK = true;
	}
};

#endif // _INC_CCHAT_H

//
// cDSoundChat.cpp
// Copyright Menace Software (www.menasoft.com).
// Use the direct sound to load and transmit the sounds.
//

#include "stdafx.h"
#include "cdsound.h"
#include "common.h"

#if 0 // def SPHERE_CLIENT

class CSphereSoundPeer : public CGSocket
{
	// Send/Receive sound to this socket.
private:
	DWORD m_dwIP;	// CSocketAddressIP
	CSphereUID m_uid;	// the uid of the sound source.

public:
	void OnEvent( WORD wEvent, WORD wError );
};

class CSphereSoundCapture
{
	// IDirectSoundCapture
	// LPDIRECTSOUNDCAPTURE m_pDS;
	CGRefArray <CSphereSoundPeer> m_Peers;
	int m_iMaxPeerConnects;	// max number of connected audio systems.
	bool m_fMute;	// do not send out any audio.
	int m_iSquelchLevel;	// don't send stuff under this level. (for .10 secs)
public:
	CSphereSoundCapture()
	{
		// m_pDS = NULL;
		m_iMaxPeerConnects = 0;
	}
#if 0
	bool IsOpen() const
	{
		return( m_pDS != NULL );
	}
#endif
	void Init();
	void OnEvent( SOCKET socket, WORD wEvent, WORD wError );
};

void CSphereSoundCapture::Open()
{
	// Turn the local record option on. (if we want it at all)

	if ( ! m_iMaxPeerConnects )
	{
		// close down and return
		return;
	}

}

void CSphereSoundCapture::AddPeer( bool fSelectPriv, DWORD dwIP )
{
	// Add a peer to my group.
	// fSelectPriv = They are part of my selected high priv group.
	// Attempt to connect to it.

}

void CSphereSoundPeer::OnEvent( WORD wEvent, WORD wError )
{

}

#endif


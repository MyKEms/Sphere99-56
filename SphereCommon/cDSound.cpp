//
// cDSound.cpp
// Copyright Menace Software (www.menasoft.com).
// Use the direct sound to load and transmit the sounds.
//
#include "stdafx.h"
#include "cdsound.h"

#ifdef _WIN32

CDSoundMgr g_DSoundMgr;

/////////////////////////////////////////////////////////////////////////////
// -CDSoundPlayInst

CDSoundPlayInst::CDSoundPlayInst( const BYTE * pWaveData, DWORD dwDataSize, PCMWAVEFORMAT *pwf )
{
	// A single playing sound
	assert( g_DSoundMgr.IsOpen());
	m_iVol = 0;
	m_pDSBuffer = NULL;	// Reset the sound buffer
#ifdef USE_DDX_3D_SOUND
	m_pDS3DBuffer = NULL;	// Reset the 3D buffer
#endif

	// Set up DSBUFFERDESC structure.
	DSBUFFERDESC dsbdesc;
	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));  // Zero it out. 
	dsbdesc.dwSize        = sizeof(DSBUFFERDESC);

#ifdef USE_DDX_3D_SOUND
	dsbdesc.dwFlags       = DSBCAPS_CTRLFREQUENCY|DSBCAPS_CTRLPAN|DSBCAPS_CTRLVOLUME|DSBCAPS_CTRL3D; // m_pDS3DBuffer
#else
	dsbdesc.dwFlags       = DSBCAPS_CTRLFREQUENCY|DSBCAPS_CTRLPAN|DSBCAPS_CTRLVOLUME; // |DSBCAPS_CTRL3D
#endif
	dsbdesc.dwBufferBytes = dwDataSize;
	dsbdesc.lpwfxFormat   = (LPWAVEFORMATEX)pwf;

	HRESULT hRes = g_DSoundMgr.GetDS()->CreateSoundBuffer( &dsbdesc, &m_pDSBuffer, NULL);
	if ( hRes != DS_OK )
	{
		throw CGException( LOGL_ERROR, hRes, "CDSoundPlayInst.CreateSoundBuffer FAIL");
		return;
	}

	// Lock data in buffer for writing
	LPVOID pData1;
	DWORD  dwData1Size;
	LPVOID pData2;
	DWORD  dwData2Size;

	hRes = m_pDSBuffer->Lock( 0, dwDataSize, &pData1, &dwData1Size, &pData2, &dwData2Size, DSBLOCK_FROMWRITECURSOR );	// DSBLOCK_ENTIREBUFFER 
	if (hRes != DS_OK)
	{
		throw CGException( LOGL_ERROR, hRes, "CDSoundPlayInst.Lock FAIL");
		return;
	}

	// Read in first chunk of data
	memcpy( pData1, pWaveData, dwData1Size );
	// read in second chunk if necessary
	memcpy( pData2, pWaveData+dwData1Size, dwData2Size );

	// Unlock data in buffer
	hRes = m_pDSBuffer->Unlock(pData1, dwData1Size, pData2, dwData2Size);
	if ( hRes != DS_OK)
	{
		throw CGException( LOGL_ERROR, hRes, "CDSoundPlayInst.Unlock FAIL");
		return;
	}

#ifdef USE_DDX_3D_SOUND
	if (dsbdesc.dwFlags & DSBCAPS_CTRL3D)
	{
		// Get pointer to 3D buffer
		hRes = m_pDSBuffer->QueryInterface( IID_IDirectSound3DBuffer, (void **)&m_pDS3DBuffer );
		if ( hRes != S_OK )
		{
			throw CGException( LOGL_ERROR, hRes, "CDSoundPlayInst.IID_IDirectSound3DBuffer FAIL");
			return;
		}
	}
#endif
}

CDSoundPlayInst::~CDSoundPlayInst()
{
	Stop();
	if(m_pDSBuffer)
	{       
		m_pDSBuffer->Release();
	}
#ifdef USE_DDX_3D_SOUND
	if(m_pDS3DBuffer)
	{       
		m_pDS3DBuffer->Release();
	}
#endif
}

DWORD CDSoundPlayInst::GetStatus() const
{
	// RETURN: DSBSTATUS_PLAYING
	if ( m_pDSBuffer == NULL )
		return( 0 );

	DWORD dwStatus;
	HRESULT hRes = m_pDSBuffer->GetStatus(&dwStatus);
	if ( hRes != DS_OK )
	{
		throw CGException( LOGL_ERROR, hRes, "CDSoundPlayInst.GetStatus FAIL");
	}

	return( dwStatus );
}

bool CDSoundPlayInst::Set3D()
{
	// Set up the 3d aspect. Position Settings
	// RETURN: true = sound is ok.
	//  false = delete it.
	if ( ! m_pt.IsValidPoint()) 
		return true;
	int iDist = m_pt.GetDistBase( g_DSoundMgr.m_pntCenter );
	if ( iDist > SPHEREMAP_VIEW_SIZE )
		return( false );

#ifdef USE_DDX_3D_SOUND
	Get3DBuffer()->SetPosition( 
		( g_DSoundMgr.m_pntCenter.m_x - m_pt.m_x ) / 100.0f,
		( g_DSoundMgr.m_pntCenter.m_y - m_pt.m_y ) / 100.0f,
		( g_DSoundMgr.m_pntCenter.m_z - m_pt.m_z ) / 100.0f,
		DS3D_IMMEDIATE );
#else

#ifndef DSBVOLUME_MAX
#define DSBVOLUME_MAX 0
#define DSBVOLUME_MIN -10000
#endif

	long iVol = DSBVOLUME_MIN + IMULDIV( SPHEREMAP_VIEW_SIZE-(iDist/2), (255-m_iVol) * ( DSBVOLUME_MAX - DSBVOLUME_MIN ), SPHEREMAP_VIEW_SIZE * 255 );
	GetBuffer()->SetVolume( iVol );
#endif

	return( true );
}

bool CDSoundPlayInst::Play( CPointMap pt, int iVol, int iOnce )
{
	// dwFlags == DSBPLAY_LOOPING or 0
	// iVol = 255 percent vol.

	if ( ! IsValid())	// Make sure we have a valid sound buffer
		return false;

	m_iVol = iVol;
	m_pt = pt;
	if ( ! Set3D())
		return( false );

	//DWORD dwStatus = GetStatus();
	//if ( dwStatus & DSBSTATUS_PLAYING )
	//	return;

	HRESULT hRes = m_pDSBuffer->Play(0, 0, iOnce ? 0 : DSBPLAY_LOOPING );		// Play the sound
	if ( hRes != DS_OK )
	{
		// throw CGException( LOGL_ERROR, hRes, "CDSoundPlayInst.Play FAIL");
		return( false );
	}

	return( true );
}

void CDSoundPlayInst::Stop()
{
	if ( ! IsValid())	// Make sure we have a valid sound buffer
		return;

	DWORD dwStatus = GetStatus();
	if ( dwStatus & DSBSTATUS_PLAYING )
	{
		HRESULT hRes = m_pDSBuffer->Stop();		// Stop the sound
		if ( hRes != DS_OK )
		{
			throw CGException( LOGL_ERROR, hRes, "CDSoundPlayInst.Stop FAIL");
		}
	}
}

#if 0

GetParams
{
	// Volume Settings
	m_hVolume.SetRange( DSBVOLUME_MIN, DSBVOLUME_MAX, TRUE );
	GetDocument()->m_pDSBuffer->GetBuffer()->GetVolume((long *)&m_nVolume);

	// Pan Settings
	m_hPan.SetRange(-10000, 10000, TRUE);
	GetDocument()->m_pDSBuffer->GetBuffer()->GetPan((long *)&m_nPan);

	// Position Settings
	D3DVECTOR Position;
	m_hPosX.SetRange(-300, 300, TRUE);
	m_hPosY.SetRange(-300, 300, TRUE);
	m_hPosZ.SetRange(-300, 300, TRUE);
	GetDocument()->m_pDSBuffer->Get3DBuffer()->GetPosition(&Position);
	m_PosX = (int)(Position.x * 100.0f);
	m_PosY = (int)(Position.y * 100.0f);
	m_PosZ = (int)(Position.z * 100.0f);

	// Velocity Settings
	D3DVECTOR Velocity;
	m_hVelocityX.SetRange(-300, 300, TRUE);
	m_hVelocityY.SetRange(-300, 300, TRUE);
	m_hVelocityZ.SetRange(-300, 300, TRUE);
	GetDocument()->m_pDSBuffer->Get3DBuffer()->GetVelocity(&Velocity);
	m_VelocityX = (int)(Velocity.x * 100.0f);
	m_VelocityY = (int)(Velocity.y * 100.0f);
	m_VelocityZ = (int)(Velocity.z * 100.0f);
}

#endif

/////////////////////////////////////////////////////////////////////////////
// -CDSoundMgr

CDSoundMgr::~CDSoundMgr()
{
}

void CDSoundMgr::Init( HWND hWnd, const CPointMap & ptCenter )
{
	// Create DirectSound Object
	HRESULT hRes = DirectSoundCreate(NULL, &m_pDS, NULL);
	if ( hRes != DS_OK )
	{
		throw CGException( LOGL_ERROR, hRes, "CDSoundMgr.DirectSoundCreate FAIL");
	}

	// Set Cooperative Level
	hRes = m_pDS->SetCooperativeLevel(hWnd, DSSCL_NORMAL);
	if ( hRes != DS_OK )
	{
		m_pDS = NULL;
		throw CGException( LOGL_ERROR, hRes, "CDSoundMgr.SetCooperativeLevel FAIL");
	}

	m_pntCenter = ptCenter;
}

void CDSoundMgr::OnCheckStatus( const CPointMap & ptCenter )
{
	// Get rid of all the dead sounds.
	// recenter the 3d sound location.

	if ( ! IsOpen()) 
		return;
	bool fReCenter = ( m_pntCenter != ptCenter );

	// Make sure we are not playing too many sounds at one time.,
	for ( int i=0; i<m_Sounds.GetSize(); i++ )
	{
		if ( m_Sounds[i]->GetStatus() & DSBSTATUS_PLAYING ) 
		{
			// Make sure it is centered.
			if ( ! fReCenter )
				continue;
			if ( m_Sounds[i]->Set3D())
				continue;
			// Out of range. delete it.
		}
		m_Sounds.RemoveAt(i);
		i--;
	}

	m_pntCenter = ptCenter;
}

void CDSoundMgr::Play( SOUND_TYPE sound, CPointMap pt, int iVol, int iOnce )
{
	// iVol = 255 = no volume
	// iVol = 0 = full loud.

	if ( ! IsOpen()) 
		return;
	if ( ! sound || sound == SOUND_CLEAR ) 
	{
		// Kill all the active sounds.
		m_Sounds.RemoveAll();
		return;
	}

	// Clean up dead sounds.
	OnCheckStatus(m_pntCenter);
	if ( m_Sounds.GetSize() >= m_iMaxSounds )
	{
		// ??? we need to remove a current sound to play one more !
		// m_Sounds.RemoveAt( Calc_GetRandVal( m_Sounds.GetCount()));
	}

	CMulSound data( sound );
	CUOSoundFrame * pDataHdr = (CUOSoundFrame *) data.GetSoundData(); 
	if ( pDataHdr == NULL )
		return;

	DWORD dwDataLength = data.GetLength() - sizeof(CUOSoundFrame);

	// Create the sound buffer for the wave file
	// Set up wave format structure.
	PCMWAVEFORMAT wf;
	data.GetWaveFormat(&wf);

	// Create the new sound
	CDSoundPlayInst * pSound = new CDSoundPlayInst( (BYTE*)(pDataHdr+1), dwDataLength, &wf );
	assert( pSound );

	// Play it.
	if ( pSound->Play( pt, iVol, iOnce ))
	{
		m_Sounds.Add( pSound );
	}
	else
	{
		delete pSound;
	}
}

#endif


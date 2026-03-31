//
// CDSound.h
// Copyright Menace Software (www.menasoft.com).
//

#ifndef _INC_CDSOUND_H
#define _INC_CDSOUND_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifdef _WIN32

//#include <d3d8types.h>
#include "CMemBlock.h"
#include "cpointmap.h"

#ifdef D3DRGB
//#define USE_DDX_3D_SOUND
#endif

class CDSoundPlayInst : public CMemDynamic
{
	// A single sound playing.
protected:
	DECLARE_MEM_DYNAMIC;

protected:
	CPointMap m_pt;	// D3DVECTOR
	BYTE m_iVol;	// Volume attenuator. 0=full, 255 = none
	LPDIRECTSOUNDBUFFER		m_pDSBuffer;			// Sound buffer
#ifdef USE_DDX_3D_SOUND
	LPDIRECTSOUND3DBUFFER	m_pDS3DBuffer;			// 3D buffer
#endif

public:
	bool IsValid() const
	{
		return( (m_pDSBuffer) ? true : false );
	}

	bool Set3D();
	DWORD GetStatus() const;
	bool Play( CPointMap pt, int iVol = 0, int iLoop = 1 );
	void Stop();

	LPDIRECTSOUNDBUFFER		GetBuffer() const { return m_pDSBuffer;}
#ifdef USE_DDX_3D_SOUND
	LPDIRECTSOUND3DBUFFER	Get3DBuffer() const { return m_pDS3DBuffer;}
#endif

	CDSoundPlayInst( const BYTE * pWaveData, DWORD dwDataSize, PCMWAVEFORMAT *pwf );
	~CDSoundPlayInst();
};

extern class CDSoundMgr
{
	// Assume 4 bytes per sample = 16 bit stereo @ 11025 Hz
private:
	LPDIRECTSOUND m_pDS;
	CGNewArray <CDSoundPlayInst> m_Sounds;
	int m_iMaxSounds;		// max concurrent sounds.
public:
	CPointMap m_pntCenter;	// Last update for the view for sounds.
public:
	void Init( HWND hWnd, const CPointMap & ptCenter );
	void OnCheckStatus( const CPointMap & ptCenter );
	void Play( SOUND_TYPE sound, CPointMap pt = CPointMap(0,0,0), int iVol = 0, int iLoop = 1 );
	bool IsOpen() const
	{
		return( m_pDS != NULL );
	}
	LPDIRECTSOUND GetDS() const { return m_pDS; }

	CDSoundMgr()
	{
		m_pDS = NULL;
		m_iMaxSounds = 8;
	}
	~CDSoundMgr();

} g_DSoundMgr;

#endif	// _WIN32
#endif // _INC_CDSOUND_H

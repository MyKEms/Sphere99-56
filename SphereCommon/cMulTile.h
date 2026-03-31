//
// CMulTile.h
// Copyright Menace Software (www.menasoft.com).
//
// Basic rendering of all the MUL data. Client only type stuff.

#ifndef _INC_CTILEMUL_H
#define _INC_CTILEMUL_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifdef _WIN32

#ifdef SPHERE_CLIENT
#include "cdirectdraw.h"		// CDirectSurface
#endif

#include "cMulMap.h"
#include "cmulmulti.h"

inline COLORREF CvtColorToCOLORREF( COLOR_TYPE wColor )
{
	// convert WORD RGB555 colors to 24 bit RGB() format.
	COLORREF dwNewColor = RGB(
		(( wColor >> 10 ) & 0x01f) << 3,
		(( wColor >> 5	) & 0x01f) << 3,
		(( wColor		) & 0x01f) << 3 );
	return (dwNewColor);
}

inline COLOR_TYPE CvtCOLORREFtoColor( COLORREF dwColor )
{
	// Convert 24 bit RBG() format to WORD RGB555 color
	BYTE r, g, b;
	r = (BYTE) (dwColor & 0xFF);
	g = (BYTE) ((dwColor & 0xFF00) >> 8);
	b = (BYTE) ((dwColor & 0xFF0000) >> 16);
	COLOR_TYPE wNewColor = (COLOR_TYPE) ( (( r / 8 ) << 10 ) | (( g / 8 ) << 5) | ( b / 8) );
	return (wNewColor);
}

//**************************************************************************

class CMulSkillDef : public CResourceObj	
{
public:
	bool m_fButton;	// can it have a button ?
	CGString m_sName;
public:
	virtual CGString GetName() const
	{
		return( m_sName );
	}
	CMulSkillDef( SKILL_TYPE iSkillNum ) :
		CResourceObj( iSkillNum )
	{
		m_fButton = false;
	}
	CMulSkillDef( SKILL_TYPE iSkillNum, bool fButton, const TCHAR * pszName ) :
		CResourceObj( iSkillNum ),
		m_fButton( fButton ),
		m_sName( pszName )
	{
	}
};

//**************************************************************************

class CMulSound : public CMulBlockType
{
	// VERFILE_SOUND
	// Load a sound into memory.
	// 16 bit Stereo at 11025 Hz
protected:
	DECLARE_MEM_DYNAMIC;
private:
	DWORD m_dwLength;
private:
	bool Load();
public:
	SOUND_TYPE GetID() const
	{
		return((SOUND_TYPE) GetBaseIndex());
	}
	DWORD GetLength() const
	{
		ASSERT( IsLoaded());
		return m_dwLength;
	}
	WORD * GetSoundData()
	{
		// get 16 bit samples.
		return( GetLoadData());	
	}
	void GetWaveFormat( void * pPCMWaveFormat ); // 	PCMWAVEFORMAT pcmwf;

	CMulSound( HASH_INDEX dwHashIndex );
	CMulSound( SOUND_TYPE sound );
};

//**************************************************************************

class CMulImageType : public CMulBlockType
{
	// This MUL block is going to be an image.
public:
#ifdef SPHERE_CLIENT
	CDirectSurface m_SurfCache;	// make a cached surface of this. (if it is special for light levels etc.)
#endif
public:
	CMulImageType( HASH_INDEX dwHashIndex ) :
		CMulBlockType( dwHashIndex )
	{
	}
	virtual ~CMulImageType()
	{
	}
};

#define MAXTILESIZE 0x1FFFF	// max size of a single art tile

//**************************************************************************

struct CTileItemType : public CMulImageType
{
	// Only cache a single instance of this static item.
	// VERFILE_ART
protected:
	DECLARE_MEM_DYNAMIC;
private:
	CNewPtr<CMulItemInfo> m_pInfo;	// info stuff loaded on demand.
private:
	bool Load();
public:
	WORD GetHeight() 
	{
		WORD * pData = GetLoadedData();	
		assert( pData );
		return( pData[3] );
	}
	WORD GetWidth() 
	{
		WORD * pData = GetLoadedData();	
		assert( pData );
		return( pData[2] );
	}

	int GetAnimFrames();	// how many frames does this animate over ? (include self) (<=1 not animated.)

	ITEMID_TYPE GetID() const 
	{ 
		return( (ITEMID_TYPE) ( GetBaseIndex() - TERRAIN_QTY )); 
	}
	const CMulItemInfo * GetInfo();

	CGRect GetDrawRect( int x, int y );

	CTileItemType( HASH_INDEX dwHashIndex );
	CTileItemType( ITEMID_TYPE id );
	~CTileItemType()
	{
	}
};

//**************************************************************************

struct CTileTextureType : public CMulImageType
{
	// This is like terrain but just stored with more detail.
	// VERFILE_TEXTURE
protected:
	DECLARE_MEM_DYNAMIC;
private:
	int m_iWidth;	// 64 or 128
private:
	bool LoadFromTerrain();
	bool Load();
public:
	TERRAIN_TYPE GetID() const { return( (TERRAIN_TYPE) GetBaseIndex() ); }

	int GetSourceWidth() const 
	{
		ASSERT( IsLoaded());
		return( m_iWidth );
	}

	CTileTextureType( HASH_INDEX dwHashIndex );
	CTileTextureType( TERRAIN_TYPE id );
};

//**************************************************************************

struct CTileTerrainType : public CMulImageType
{
	// VERFILE_ART
#define TERRAIN_ART_WIDTH			44	// width of a landscape tile.
#define TERRAIN_ART_HALF			(TERRAIN_ART_WIDTH/2)
protected:
	DECLARE_MEM_DYNAMIC;
private:
	CNewPtr<CMulTerrainInfo> m_pInfo;
private:
	bool Load();
	// void LoadCache();	// not used yet.
public:
	const CMulTerrainInfo * GetInfo();

	TERRAIN_TYPE GetID() const { return( (TERRAIN_TYPE) GetBaseIndex() ); }

	void DrawToTexture( WORD * pData );

	CTileTerrainType( HASH_INDEX dwHashIndex );
	CTileTerrainType( TERRAIN_TYPE id );
	~CTileTerrainType()
	{
	}
};

//**************************************************************************

struct CTileGumpType : public CMulImageType
{
	// Gump art.
	// VERFILE_GUMP
protected:
	DECLARE_MEM_DYNAMIC;
private:
	SIZE m_size;	// size is in the index info.
public:
	HRGN m_hMaskRegion;
private:
	bool Load();
public:
	GUMP_TYPE GetID() const { return( (GUMP_TYPE) GetBaseIndex() ); }
	SIZE GetDrawSize()
	{
		LoadCheck();	// must be loaded to have size.
		return( m_size );
	}
	int GetWidth() const 
	{ 
		ASSERT( IsLoaded());
		return( m_size.cx );
	}
	int GetHeight() const 
	{ 
		ASSERT( IsLoaded());
		return( m_size.cy ); 
	}

	CTileGumpType( HASH_INDEX dwHashIndex );
	CTileGumpType( GUMP_TYPE gump );
	virtual ~CTileGumpType();
};

//**************************************************************************

struct CTileLightType : public CMulImageType
{
	// A light pattern bitmap.
	// VERFILE_LIGHT
protected:
	DECLARE_MEM_DYNAMIC;
private:
	SIZE m_size;	// size is in the index info.
private:
	bool Load();
public:
	int GetWidth() const
	{
		assert( m_size.cx );
		return( m_size.cx );
	}
	int GetHeight() const
	{
		assert( m_size.cy );
		return( m_size.cy );
	}

	LIGHT_PATTERN GetID() const { return( (LIGHT_PATTERN) GetBaseIndex() ); }

	CGRect GetDrawRect( int x, int y );

	CTileLightType( HASH_INDEX dwHashIndex );
	CTileLightType( LIGHT_PATTERN light );
};

//**************************************************************************

enum ANIMTYPE_TYPE	// Animation groups
{
	ANIMTYPE_MONSTER = 0,
	ANIMTYPE_ANIMAL = 1,
	ANIMTYPE_HUMAN = 2,
};

struct CTileAnimType : public CMulImageType
{
	// VERFILE_ANIM
	// Animation frames. should be 5 (DIR_ANIM_QTY) views per action. n actions per char.
	// CREID_TYPE id, ANIM_TYPE anim, view, int frame
protected:
	DECLARE_MEM_DYNAMIC;
private:
	DWORD GetFrameCount( WORD * pData ) const // static
	{
		return( *((DWORD*)&(pData[256])) );
	}
	bool Load();
public:

	CUOAnimFrame* GetFrameHead( int iFrame ) const
	{
		WORD * pData = GetLoadedData();
		ASSERT(pData);
		ASSERT( (DWORD) iFrame < GetFrameCount(pData));
		DWORD offset = (256*2) + *((DWORD*)&(pData[256+2+iFrame*2])); // byte offset
		return( (CUOAnimFrame*)(((BYTE*)pData) + offset ));
	}
	void GetDrawFrameOffset( CUOAnimFrame * pHead, bool fFlipEW, int &ex, int &ey ) const
	{
		if ( fFlipEW )	// iDir < DIR_SE
		{
			ex -= ( pHead->m_Width - pHead->m_ImageCenterX );
		}
		else
		{
			ex -= pHead->m_ImageCenterX;
		}
		ey -= pHead->m_Height + pHead->m_ImageCenterY;	// top to bottom.
		ey += TERRAIN_ART_HALF;
	}

	static DWORD CvtCharAnimDirToIndex( CREID_TYPE id, ANIM_TYPE anim, DIR_TYPE dirrelative );
	static void CvtIndexToCharAnim( DWORD index, CREID_TYPE & id, ANIM_TYPE & anim );
	static ANIMTYPE_TYPE GetTypeFromIndex( DWORD index );
	static ANIMTYPE_TYPE GetTypeFromChar( CREID_TYPE id );

	DWORD GetFrameCount() // const
	{
		WORD * pData = GetLoadData();	// load if needed.
		if ( pData == NULL ) 
			return( 0 );
		return( GetFrameCount(pData));
	}

	CGRect GetDrawRect( int x, int y, int iFrame, DIR_TYPE dirrelative );

	DWORD GetID() const
	{
		return GetBaseIndex();
	}

	CTileAnimType( DWORD id );
};

inline ANIMTYPE_TYPE CTileAnimType::GetTypeFromIndex( DWORD index ) // static
{
	if ( index < 22000 ) 
		return( ANIMTYPE_MONSTER );	// monster
	if ( index < 35000 )
		return( ANIMTYPE_ANIMAL );	// animal (low detail)
	return( ANIMTYPE_HUMAN );	// man
}

inline ANIMTYPE_TYPE CTileAnimType::GetTypeFromChar( CREID_TYPE id ) // static
{
	if ( id < CREID_HORSE1 ) 
		return( ANIMTYPE_MONSTER );	// monster
	if ( id < CREID_MAN ) 
		return( ANIMTYPE_ANIMAL );		// animal (low detail)
	return( ANIMTYPE_HUMAN );	// man
}

//**************************************************************************

class CTileFontChar : public CMulImageType
{
	// VERFILE_FONTS
	// A single char in the font.
	DECLARE_MEM_DYNAMIC
private:
	FILE_POS_TYPE m_lOffset;	// offset into the FONTS.MUL file for this char.
	CUOFontImageHeader m_Head;
protected:
	virtual bool Load();			// load later on demand.
public:
	BYTE GetHeight() const
	{
		return( m_Head.m_bRows );
	}
	BYTE GetWidth() const
	{
		return( m_Head.m_bCols );
	}
#define FONT_MAX_SIZE	64
#define FONT_MAX_CHARS	224

	void UnLoad()
	{
		Free();
	}
	void InitFontChar( FILE_POS_TYPE lOffset, CUOFontImageHeader Hdr )
	{
		assert( ( Hdr.m_bRows * Hdr.m_bCols * 2 ) < FONT_MAX_SIZE*FONT_MAX_SIZE*2 );
		m_lOffset = lOffset;
		m_Head = Hdr;
	}

	CTileFontChar() : CMulImageType( VERDATA_MAKE_HASH( VERFILE_FONTS, 0 ))	// id is not important
	{
		m_lOffset = 0;
	}
};

class CTileFont
{
	// VERFILE_FONTS
private:
	SIZE m_size;	// max char size for the font.
	CTileFontChar m_Char[FONT_MAX_CHARS];
private:
	int GetWidth( const TCHAR *pszText ) const;
public:
	void Init();
	int GetHeight() const
	{
		return( m_size.cy );
	}
	CGRect GetDrawRect( int x, int y, const TCHAR *pszText ) const
	{
		// x,y = bottom left of the text.
		CGRect rect;
		rect.m_left = x;
		rect.m_top = y - GetHeight();
		rect.m_right = x + GetWidth( pszText );
		rect.m_bottom = y;
		return( rect );
	}
	void UnLoad();
	CTileFont()
	{
		m_size.cx = 0;
		m_size.cy = 0;
	}
};

class CMulFonts
{
	// All fonts are read in sequentially. not indexed. (Stupid)
private:	
	CTileFont   m_Font[FONT_QTY];	// Fonts
public:
	CTileFont * GetFont( FONT_TYPE f )
	{
		assert( f < FONT_QTY );
		return( &m_Font[f] );
	}
	CGRect FontGetDrawRect( FONT_TYPE f, int x, int y, const TCHAR *pszText )
	{
		return GetFont(f)->GetDrawRect( x, y, pszText );
	}

	void Load();
	void UnLoad();
};

//**************************************************************************

class CMulHues
{
private:
	int m_iGammaCorrect;	// default gamma adjust.
	CGTypedArray<CUOHueEntryBare, CUOHueEntryBare&> m_Hues;
public:
	void Load( int iGamma );
	void UnLoad()
	{
		m_iGammaCorrect = -1;	
	}

	HUE_TYPE GetValidHue( HUE_TYPE iHue ) const
	{
		if ( iHue >= m_Hues.GetSize())
			return 0;
		return( iHue );
	}
	COLOR_TYPE * GetHueTablePtr()
	{
		return( &( m_Hues.ElementAt(0).m_GammaRange[0]) );
	}
	COLOR_TYPE CvtHueToColor( HUE_TYPE iHue, int iGamma ) const
	{
		// Args Should be pre-checked.
		ASSERT(iGamma<LIGHT_QTY);
		return( m_Hues[iHue].m_GammaRange[iGamma] );
	}
	void SetGammaCorrect( int iGamma )
	{
		ASSERT( iGamma < LIGHT_QTY );
		m_iGammaCorrect = iGamma;
	}
	int GetGammaCorrect() const
	{
		return m_iGammaCorrect;
	}
	CMulHues()
	{
		m_iGammaCorrect = -1;
	}
};

#endif 	// _WIN32

#endif // _INC_CTILEMUL_H


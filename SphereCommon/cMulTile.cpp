// CMulTile.cpp
// Copyright Menace Software (www.menasoft.com).
//
// Deal with the resources in the MUL files.
//

#include "stdafx.h"
#include "cMulTile.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
//#define new DEBUG_NEW
#endif

#ifdef _WIN32

#pragma optimize( "agt", on )

/////////////////////////////////////////////////////////////////////////////
// -CMulSound

CMulSound::CMulSound( HASH_INDEX dwHashIndex ) :
	CMulBlockType( dwHashIndex )
{
	m_dwLength = 0;
}
CMulSound::CMulSound( SOUND_TYPE sound ) :
	CMulBlockType( VERDATA_MAKE_HASH( VERFILE_SOUND, sound ))
{
	m_dwLength = 0;
}

bool CMulSound::Load()
{
	// A sound resource from the SOUNDS.MUL file.
	CMulIndexRec Index;
	if ( ! g_MulInstall.ReadMulIndex( VERFILE_SOUNDIDX, VERFILE_SOUND, GetID(), Index ))
		return( false );
	m_dwLength = Index.GetBlockLength();
	LoadMulData( VERFILE_SOUND, Index );
	return( true );
}

void CMulSound::GetWaveFormat( void * pWaveFormat )
{
	// Get the format structure for playing this data.

	PCMWAVEFORMAT * pcmwf = (PCMWAVEFORMAT *) pWaveFormat;
	memset( pcmwf, 0, sizeof(PCMWAVEFORMAT));
	pcmwf->wf.wFormatTag         = WAVE_FORMAT_PCM;      
	pcmwf->wf.nChannels          = 2;
	pcmwf->wf.nSamplesPerSec     = 11025;
	pcmwf->wf.nBlockAlign        = (2*sizeof(WORD));
	pcmwf->wf.nAvgBytesPerSec    = (2*sizeof(WORD)*11025);
	pcmwf->wBitsPerSample        = 16;
}

/////////////////////////////////////////////////////////////////////////
// -CTileItemType

CTileItemType::CTileItemType( HASH_INDEX dwHashIndex ) :
	CMulImageType( dwHashIndex )
{
	assert( GetID() < ITEMID_MULTI );
}
CTileItemType::CTileItemType( ITEMID_TYPE id ) :
	CMulImageType( VERDATA_MAKE_HASH( VERFILE_ART, id+TERRAIN_QTY ))
{
	assert( GetID() < ITEMID_MULTI );
}

bool CTileItemType::Load()
{
	DWORD id = TERRAIN_QTY + GetID();
	CMulIndexRec Index;
	if ( ! g_MulInstall.ReadMulIndex( VERFILE_ARTIDX, VERFILE_ART, id, Index ))
		return( false );
	assert( Index.GetBlockLength() <= MAXTILESIZE );
	LoadMulData( VERFILE_ART, Index );
	return( true );
}

const CMulItemInfo * CTileItemType::GetInfo()
{
	if ( ! m_pInfo.IsValidNewObj())
	{
		m_pInfo = new CMulItemInfo( GetID());
	}
	return( m_pInfo );
}

CGRect CTileItemType::GetDrawRect( int sx, int sy )
{
	int iWidth;
	int iHeight;

	WORD * pData = GetLoadData();
	if ( ! pData ) 
	{
		iWidth  = 0;
		iHeight = 0;
	}
	else
	{
		iWidth  = pData[2];
		iHeight = pData[3];
		sy -= ( iHeight - TERRAIN_ART_WIDTH );	
		sx -= ( iWidth / 2 );	// center it.
	}

	CGRect rect;
	rect.left = sx;
	rect.top = sy;
	rect.right = sx + iWidth;
	rect.bottom = sy + iHeight;
	return( rect );
}

#ifdef SPHERE_CLIENT

int CTileItemType::GetAnimFrames()	// how many frames does this animate over ? (include self) (<=1 not animated.)
{
	// If this is an animated tile.
	// How many tiles does this animate over ?
	// The layer value is wrong most of the time !

	ASSERT( GetInfo()->m_flags & UFLAG4_ANIM );

	if ( m_pInfo->m_flags & UFLAG2_ZERO1 )
		return( m_pInfo->m_wUnk14 );

	//if ( m_pInfo->m_layer )	// This is sometimes wrong ?
	//	return( m_pInfo->m_layer );

	// We need to verify this value !
	// The MUL files are really goofy when it comes to animated items.

	CItemInst ItemNext;
	int idnext = GetID();
	for(;;)
	{
		// attempt to load the next frame.
		idnext++;
		ItemNext.AttachMul( (ITEMID_TYPE) idnext );

		const CMulItemInfo * pInfoNext = ItemNext.GetInfo();
		// if ( m_pInfo->m_layer == 0 )
		{
			// Must stop if the flag is not set.
			if ( ! ( pInfoNext->m_flags & UFLAG4_ANIM ))
			{
				// idnext++;	// inclusive
				break;
			}
		}

		if ( // pInfoNext->m_name[0] &&
			strcmp( m_pInfo->m_name, pInfoNext->m_name ))
		{
			// It has a name and it's not the same as the one we started with.
			break;
		}
	}

	// We know the value as best we can.
	// Now assign it the "correct" vlaue.
	m_pInfo->m_wUnk14 = idnext - GetID();
	m_pInfo->m_flags |= UFLAG2_ZERO1;

	return( m_pInfo->m_wUnk14 );
}

#endif

//////////////////////////////////////////////////////////////////////
// -CTileTerrainType

CTileTerrainType::CTileTerrainType( HASH_INDEX dwHashIndex ) :
	CMulImageType( dwHashIndex )
{
	assert( GetBaseIndex() < TERRAIN_QTY );
}
CTileTerrainType::CTileTerrainType( TERRAIN_TYPE id ) :
	CMulImageType( VERDATA_MAKE_HASH( VERFILE_ART, id ))
{
	// Cache this in memory.
	assert( GetBaseIndex() < TERRAIN_QTY );
}

#define TERRAIN_ART_SIZE ( TERRAIN_ART_WIDTH*(TERRAIN_ART_HALF+1) )	// actual useful pixels

bool CTileTerrainType::Load()
{
	CMulIndexRec Index;
	if ( ! g_MulInstall.ReadMulIndex( VERFILE_ARTIDX, VERFILE_ART, GetID(), Index ))
		return( false );
	assert( Index.GetBlockLength() == 2048 );
	LoadMulData( VERFILE_ART, Index );
	return( true );
}

const CMulTerrainInfo * CTileTerrainType::GetInfo()
{
	if ( ! m_pInfo.IsValidNewObj())
	{
		m_pInfo = new CMulTerrainInfo( GetID());
	}
	return( m_pInfo);
}

void CTileTerrainType::DrawToTexture( WORD * pDst )
{
	// Convert the terrain diamond into a square texture.

	WORD * pData = GetLoadedData();
	ASSERT(pData);

	memset( pDst, 0, sizeof(WORD)*TERRAIN_ART_WIDTH );
	WORD * pDstCur = pDst + TERRAIN_ART_WIDTH;

	int iWidth = 2;
	bool rbDown = true;

	do
	{
		WORD * pDstRow = pDstCur;

		for ( int i=iWidth; --i; )
		{
			ASSERT( pDstCur >= pDst );
			ASSERT( pDstCur < pDst+TERRAIN_ART_SIZE );
			*pDstCur = *pData;
			pData++;	// WORD index.
			pDstCur -= TERRAIN_ART_WIDTH-1;
		}
		if ( rbDown && iWidth >= TERRAIN_ART_WIDTH )
		{
			pDstCur = pDstRow + 1;
			rbDown = false;
		}
		else if (rbDown)
		{
			pDstCur = pDstRow + TERRAIN_ART_WIDTH;
			iWidth += 2;
		}
		else
		{
			pDstCur = pDstRow + 1;
			iWidth -= 2;
		}
	} while (iWidth);
}

//////////////////////////////////////////////////////////////////////
// -CTileTextureType

CTileTextureType::CTileTextureType( HASH_INDEX dwHashIndex ) :
	CMulImageType( dwHashIndex )
{
	assert( GetBaseIndex() < TERRAIN_QTY );
	m_iWidth = 0;
}
CTileTextureType::CTileTextureType( TERRAIN_TYPE id ) :
	CMulImageType( VERDATA_MAKE_HASH( VERFILE_TEXMAPS, id ))
{
	// Cache this in video memory? 64x64 or 128x128
	assert( GetBaseIndex() < TERRAIN_QTY );
	m_iWidth = 0;
	// Load();	// Have to load them to know how big they are. or if they even exist.
}

bool CTileTextureType::LoadFromTerrain()
{
	// we can transform the terrain tile into a texture ?

	return false;

#ifdef SPHERE_CLIENT
	CTerrainInst terrain;
	terrain.AttachMul( GetID());
	ASSERT(terrain.IsValidRefObj());
	if ( ! terrain->LoadCheck())
		return( false );

	m_iWidth = TERRAIN_ART_WIDTH;
	Alloc( TERRAIN_ART_WIDTH * TERRAIN_ART_WIDTH * sizeof( COLOR_TYPE ));

	terrain->DrawToTexture( GetLoadData());
#endif
#ifdef SPHERE_MAKER
	CTileTerrainType terrain(GetID());
	if ( ! terrain.LoadCheck())
		return( false );

	m_iWidth = TERRAIN_ART_WIDTH;
	Alloc( TERRAIN_ART_WIDTH * TERRAIN_ART_WIDTH * sizeof( COLOR_TYPE ));

	terrain.DrawToTexture( GetLoadData());
#endif

	return( true );
}

bool CTileTextureType::Load()
{
	CMulIndexRec Index;
	if ( ! g_MulInstall.ReadMulIndex( VERFILE_TEXIDX, VERFILE_TEXMAPS, GetID(), Index ))
	{
		// we can transform the terrain tile into this ?
		return( LoadFromTerrain());
		//return( false );
	}

	assert( m_iWidth == 0 );
	if ( Index.GetBlockLength() == 64*64*2 )
		m_iWidth = 64;
	else if ( Index.GetBlockLength() == 128*128*2 )
		m_iWidth = 128;
	else
	{
		assert( 0 );
	}
	LoadMulData( VERFILE_TEXMAPS, Index );
	return( true );
}

////////////////////////////////////////
// Gumps 

CTileGumpType::CTileGumpType( HASH_INDEX dwHashIndex ) :
	CMulImageType( dwHashIndex )
{
	m_hMaskRegion = NULL;
	assert( GetBaseIndex() < GUMP_QTY );
}
CTileGumpType::CTileGumpType( GUMP_TYPE id ) : 
	CMulImageType( VERDATA_MAKE_HASH( VERFILE_GUMPART, id ))
{
	m_hMaskRegion = NULL;
	assert( GetBaseIndex() < GUMP_QTY );
	// Load();	// Have to load them to know how big they are..
}

CTileGumpType::~CTileGumpType()
{
	if ( m_hMaskRegion )
	{
		DeleteObject( m_hMaskRegion );
	}
}

bool CTileGumpType::Load()
{
	CMulIndexRec Index;
	if ( ! g_MulInstall.ReadMulIndex( VERFILE_GUMPIDX, VERFILE_GUMPART, GetID(), Index ))
		return( false );
	m_size.cy = Index.m_wVal3;
	m_size.cx = Index.m_wVal4;
	LoadMulData( VERFILE_GUMPART, Index );
	return( true );
}

//////////////////////////////////////////////////////////////////////
// -CTileAnimType

CTileAnimType::CTileAnimType( DWORD id ) : 
	CMulImageType( VERDATA_MAKE_HASH( VERFILE_ANIM, id ))
{
}

bool CTileAnimType::Load()
{
	CMulIndexRec Index;
	if ( ! g_MulInstall.ReadMulIndex( VERFILE_ANIMIDX, VERFILE_ANIM, GetID(), Index ))
		return( false );
	LoadMulData( VERFILE_ANIM, Index );
	return( true );
}

DWORD CTileAnimType::CvtCharAnimDirToIndex( CREID_TYPE id, ANIM_TYPE anim, DIR_TYPE dirRelative ) // static
{
	// For high detail, id = CREID_TYPE * 110 
	// 
	// always 5 anims per action. (DIR_ANIM_QTY)
	// 
	// 22 actions for high detail critters.
	//
	// 22000 - = Char C8 = Horse, 5 anims per action.
	// 13 actions for low detail
	// 
	// 35000 - = Char = Human
	// 35 actions for humans and equip
	//

	int index;
	int animqty;
	if ( id < CREID_HORSE1 )
	{
		// Monsters and high detail critters.
		// 22 actions per char.
		index = ( id * ANIM_QTY_MON * DIR_ANIM_QTY );
		animqty = ANIM_QTY_MON;
	}
	else if ( id < CREID_MAN )
	{
		// Animals and low detail critters.
		// 13 actions per char.
		index = 22000 + (( id - CREID_HORSE1 ) * ANIM_QTY_ANI * DIR_ANIM_QTY );
		animqty = ANIM_QTY_ANI;
	}
	else
	{
		// Human and equip
		// 35 actions per char.
		index = 35000 + (( id - CREID_MAN ) * ANIM_QTY_MAN * DIR_ANIM_QTY );
		animqty = ANIM_QTY_MAN;
	}

	// Factor in the animation. (assuming it exists, not all anims are supported)

	if ( anim < 0 || anim >= animqty )
	{
		anim = ANIM_WALK_UNARM;
	}

	index += ( anim * DIR_ANIM_QTY );

	// Convert the direction.
	assert( dirRelative >= 0 && dirRelative < DIR_QTY );

	static const int sm_DirOffsets[DIR_QTY] = 
	{
		3,	// DIR_N = flipped.
		2,	// flipped.
		1,	// flipped.
		0,	// DIR_SE
		1,
		2,
		3,
		4,
	};

	index += sm_DirOffsets[ dirRelative ];
	return( index );
}

void CTileAnimType::CvtIndexToCharAnim( DWORD index, CREID_TYPE & id, ANIM_TYPE & anim ) // static
{
	if ( index < 22000 )	// monster
	{
		id = (CREID_TYPE)( index / ( ANIM_QTY_MON * DIR_ANIM_QTY ));
		index -= id * ( ANIM_QTY_MON * DIR_ANIM_QTY );
	}
	else if ( index < 35000 )	// animal
	{
		index -= 22000;
		id = (CREID_TYPE)( index / ( ANIM_QTY_ANI * DIR_ANIM_QTY ));
		index -= id * ( ANIM_QTY_ANI * DIR_ANIM_QTY );
	}
	else	// human
	{
		index -= 35000;
		id = (CREID_TYPE)( index / ( ANIM_QTY_MAN * DIR_ANIM_QTY ));
		index -= id * ( ANIM_QTY_MAN * DIR_ANIM_QTY );
	}

	anim = (ANIM_TYPE) ( index / DIR_ANIM_QTY );
}

CGRect CTileAnimType::GetDrawRect( int sx, int sy, int iFrame, DIR_TYPE dirRelative )
{
	CGRect rect;
	if ( (DWORD) iFrame >= GetFrameCount())
	{
		rect.SetRectEmpty();
		return( rect );
	}
	CUOAnimFrame * pHead = GetFrameHead( iFrame );

	bool fFlipEW = ( dirRelative < DIR_SE );
	if ( fFlipEW )
	{
		rect.left = sx - ( pHead->m_Width - pHead->m_ImageCenterX );
	}
	else
	{
		rect.left = sx - pHead->m_ImageCenterX;
	}

	sy -= pHead->m_Height + pHead->m_ImageCenterY;	// top to bottom.
	sy += TERRAIN_ART_HALF;
	rect.top = sy;

	rect.right = rect.left + pHead->m_Width;
	rect.bottom = sy + pHead->m_Height;
	return( rect );
}

//////////////////////////////////////////////////////////////////////
// -CTileLightType

CTileLightType::CTileLightType( HASH_INDEX dwHashIndex ) :
	CMulImageType( dwHashIndex )
{
	assert( GetBaseIndex() < LIGHT_QTY );
}
CTileLightType::CTileLightType( LIGHT_PATTERN light ) :
	CMulImageType( VERDATA_MAKE_HASH( VERFILE_LIGHT, light ))
{
	assert( GetBaseIndex() < LIGHT_QTY );
	// Load();	// size is stored here.
}

bool CTileLightType::Load()
{
	CMulIndexRec Index;
	if ( ! g_MulInstall.ReadMulIndex( VERFILE_LIGHTIDX, VERFILE_LIGHT, GetID(), Index ))
		return( false );
	m_size.cy = Index.m_wVal3;
	m_size.cx = Index.m_wVal4;
	// NOTE: These are just uncompressed rectangles. but somtimes has extra crap ?
	assert( Index.GetBlockLength() >= (DWORD) (m_size.cx*m_size.cy));

	LoadMulData( VERFILE_LIGHT, Index );
	return( true );
}

CGRect CTileLightType::GetDrawRect( int sx, int sy )
{
	CGRect rect;
	rect.left = sx;
	rect.right = sx + m_size.cx;
	rect.top = sy;
	rect.bottom = sy + m_size.cy;
	return( rect );
}

////////////////////////////////////////
// -CTileFont

bool CTileFontChar::Load()
{
	if ( ! m_lOffset ||
		! GetHeight() || 
		! GetWidth())	// Then it has no graphics.. Use 'space' instead..
		return( false );

	// Make up a fake index record for this char.
	CMulIndexRec Index;
	Index.SetupIndex( m_lOffset, GetHeight() * GetWidth() * sizeof(COLOR_TYPE) );

	assert( Index.GetBlockLength() <= FONT_MAX_SIZE*FONT_MAX_SIZE*sizeof(COLOR_TYPE) );

	LoadMulData( VERFILE_FONTS, Index );
	return( true );
}

///////////////////////////////////

void CTileFont::Init()
{
	// Read in the whole font so we can use it.
	// Fonts are non-indexed but sequencial so just read from current pos.

	BYTE ucHeader;
	if ( g_MulInstall.m_File[VERFILE_FONTS].Read((void *)&ucHeader, 1 ) <= 0 )
		throw CGException(LOGL_CRIT, E_FAIL, "CTileFont.FontInit: Read (Header)");

	for ( int i=0; i<FONT_MAX_CHARS; i++ )
	{
		CUOFontImageHeader Hdr;
		if ( g_MulInstall.m_File[VERFILE_FONTS].Read( &Hdr, sizeof(Hdr)) <= 0 )
			throw CGException(LOGL_CRIT, E_FAIL, "CTileFont.FontInit: Read (FontHdr)");

		m_Char[i].InitFontChar( g_MulInstall.m_File[VERFILE_FONTS].GetPosition(), Hdr );

		// Skip data for now.
		if ( ! g_MulInstall.m_File[VERFILE_FONTS].Seek( Hdr.m_bRows * Hdr.m_bCols * sizeof(COLOR_TYPE), SEEK_CUR ))
		{
			throw CGException(LOGL_CRIT, E_FAIL, "CTileFont:Init: Seek");
		}

		if ( Hdr.m_bCols > m_size.cx )
			m_size.cx = Hdr.m_bCols;
		if ( Hdr.m_bRows > m_size.cy )
			m_size.cy = Hdr.m_bRows;
	}
}

int CTileFont::GetWidth( const TCHAR *pszText ) const
{
	ASSERT(pszText);
	int cx = 0;
	for ( int i = 0; pszText[i]; i++ )
	{
		TCHAR ch = pszText[i] - ' ';
		if ( ch < 0 ) 
			continue;
		assert( ch < FONT_MAX_CHARS );

		int iWidth = m_Char[ch].GetWidth();
		if ( ! iWidth )
		{
			iWidth = m_Char[0].GetWidth();	// Draw a space instead ?
		}

		cx += iWidth;
	}
	return( cx );
}

void CTileFont::UnLoad()
{
	for ( int i=0; i<COUNTOF(m_Char); i++ )
	{
		m_Char[i].UnLoad();
	}
}

//*********************************************
// -CMulFonts

void CMulFonts::Load()
{
	// Fonts must stupidly be read in all at once.
	g_MulInstall.m_File[VERFILE_FONTS].SeekToBegin();
	for ( int f=0; f<COUNTOF(m_Font); f++ )
	{
		m_Font[f].Init();
	}
}

void CMulFonts::UnLoad()
{
	for ( int i=0; i<FONT_QTY; i++ )
	{
		m_Font[i].UnLoad();	// Fonts
	}
}

//////////////////////////////////////////////////////////////////////
// - CMulHues

void CMulHues::Load( int iGamma )
{
	g_MulInstall.m_File[VERFILEX_HUES].SeekToBegin();

	DWORD dwHueGroupQty = g_MulInstall.m_File[VERFILEX_HUES].GetLength() / sizeof( CUOHueGroup );
	assert( iGamma < LIGHT_QTY );

	// Create temporary read buffer.
	CUOHueGroup * pHueGroups = new CUOHueGroup[ dwHueGroupQty ];
	ASSERT(pHueGroups);

	if ( g_MulInstall.m_File[VERFILEX_HUES].Read( (void *)pHueGroups, dwHueGroupQty * sizeof( CUOHueGroup )) <= 0 )
	{
		delete [] pHueGroups;
		throw CGException(LOGL_CRIT, E_FAIL, "CMulHues:Load Read" );
		return;
	}

	// Fix this silliness. get rid of all the extra crap stored with the hues.
	DWORD dwHueQty = UOHUE_BLOCK_QTY * dwHueGroupQty;
	m_Hues.SetSize(dwHueQty);

	int j = 0;
	for ( DWORD k=0; k<dwHueGroupQty; k++ )
	{
		for ( int n=0; n<UOHUE_BLOCK_QTY; n++ )
		{
			// Weird reverse order ?
#if 1
			memcpy( &(m_Hues.ElementAt(j)), pHueGroups[k].m_Entries[n].m_GammaRange, sizeof(CUOHueEntryBare));
#else
			for ( int i=0; i<LIGHT_QTY; i++ )
			{
				m_Hues[j].m_GammaRange[i] = pHueGroups[k].m_Entries[n].m_GammaRange[(LIGHT_QTY-1)-i];
			}
#endif
			j++;
		}
	}

	delete [] pHueGroups;

	// ??? Get any patched hues from the verdata.mul file.

#if 0
	CUOHueGroup hueGroup;
	CMulIndexRec dataIndex;
	if ( g_MulVerData.FindVerDataBlock( VERFILEX_HUES, group, dataIndex ) )
	{
		// Hues in the verdata file are seriously messed up
		// CUOHueGroup
		BYTE bOldData[sizeof(DWORD) + 8 * (sizeof(CUOHueEntry) + 64)];
		g_MulInstall.ReadMulData( VERFILE_VERDATA, dataIndex, &bOldData[0] );
		for ( int i = 0; i < 8; i++ )
		{
			memcpy(&(hueGroup.m_Entries[i]), &bOldData[sizeof(DWORD) + i * (sizeof(CUOHueEntry) + 64)], sizeof(CUOHueEntry));
		}
		memcpy(&hueGroup.m_Header, &bOldData[0], sizeof(DWORD));
	}

#endif
	// g_MulInstall.m_File[VERFILEX_HUES].Close();
}
#pragma optimize( "", on )
#endif


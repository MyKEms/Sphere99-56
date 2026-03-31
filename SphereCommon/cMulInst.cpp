//
// CMulINST.CPP
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"
#include "spherecommon.h"
#include "cMulInst.h"
#ifdef _WIN32
#include "cregistry.h"
#endif

#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif
#ifndef NO_ERROR
#define NO_ERROR 0
#endif

CMulInstall g_MulInstall;

void CMulInstall::SetExePath( LPCTSTR pszName )
{
	TCHAR szValue[ _MAX_PATH ];
	strcpy( szValue, pszName );
	CGFile::ExtractPath(szValue);	// get rid of the client.exe part of the name
	m_sExePath = szValue;		// Files that are installed. "ExePath"
}
void CMulInstall::SetInstCDPath( LPCTSTR pszName )
{
	m_sInstCDPath = pszName;	// For files that may still be on the CD. "InstCDPath"
}

bool CMulInstall::FindInstall()
{
#ifdef _WIN32
	// Get the install path from the registry.
	// ExePath=
	// InstCDPath=
	// PatchExePath=
	// StartExePath=
	// Upgraded=Yes
	//

	static LPCTSTR m_szKeys[] =
	{
		SPHERE_REGKEY,
		"Software\\Origin Worlds Online\\Ultima Online Third Dawn\\1.0",
		"Software\\Origin Worlds Online\\Ultima Online\\1.0",
	};

	CGRegKey reg(HKEY_LOCAL_MACHINE);
	LONG lRet;
	for ( int i=0; i<COUNTOF(m_szKeys); i++ )
	{
		lRet = reg.Open( m_szKeys[i], KEY_READ );
		if ( lRet == NO_ERROR )
			break;
		reg.Attach(HKEY_LOCAL_MACHINE);
	}
	if ( lRet != NO_ERROR )
	{
		return( false );
	}

	TCHAR szValue[ _MAX_PATH ];
	DWORD lSize = sizeof( szValue );
	DWORD dwType = REG_SZ;
	lRet = reg.QueryValue( _TEXT("ExePath"), dwType, szValue, lSize );

	if ( lRet == NO_ERROR && dwType == REG_SZ )
	{
		SetExePath(szValue);
	}

	// ??? Find CDROM install base as well, just in case.
	// uo.cfg CdRomDataPath=e:\uo

	lSize = sizeof( szValue );
	lRet = reg.QueryValue( _TEXT("InstCDPath"), dwType, szValue, lSize );

	if ( lRet == NO_ERROR && dwType == REG_SZ )
	{
		SetInstCDPath( szValue );
	}

#else
	// LINUX has no registry so we must have the INI file show us where it is installed.
#endif

	return( true );
}

HRESULT CMulInstall::SetMulFile( VERFILE_TYPE i, LPCTSTR pszName )
{
	// Set this file to have an individual path.
	// Close(); // then reopen later ?
	CGFile* pFile = GetMulFile(i);
	if ( pFile == NULL )
		return HRES_INVALID_INDEX;
	pFile->SetFilePath( pszName );
	return( NO_ERROR );
}

bool CMulInstall::OpenFile( CGFile& file, LPCTSTR pszName, WORD wFlags )
{
	ASSERT(pszName);
	fprintf(stderr, "DBG: OpenFile name='%s' preferPath='%s'\n", pszName, (LPCTSTR)m_sPreferPath);
	if ( ! m_sPreferPath.IsEmpty())
	{
		CGString sTry = GetPreferPath( pszName );
		fprintf(stderr, "DBG: Trying prefer path: '%s'\n", (LPCTSTR)sTry);
		if ( file.Open( sTry, wFlags ))
			return true;
		fprintf(stderr, "DBG: Prefer path failed\n");
	}

#ifdef _AFXDLL
	CFileException e;
	if ( file.Open( GetFullExePath( pszName ), wFlags, &e ))
		return true;
	if ( file.Open( GetFullCDPath( pszName ), wFlags, &e ))
		return true;
#else
	if ( file.Open( GetFullExePath( pszName ), wFlags ))
		return true;
	if ( file.Open( GetFullCDPath( pszName ), wFlags ))
		return true;
#endif

	return( false );
}

LPCTSTR const CMulInstall::sm_szFileNames[VERFILE_QTY+1] = //static
{
		"map0.mul",		// Terrain data
		"staidx0.mul",	// Index into STATICS0
		"statics0.mul", // Static objects on the map
		"artidx.mul",	// Index to ART
		"art.mul",		// Artwork such as ground, objects, etc.
		"anim.idx",
		"anim.mul",		// Animations such as monsters, people, and armor.
		"soundidx.mul", // Index into SOUND
		"sound.mul",	// Sampled sounds
		"texidx.mul",	// Index into TEXMAPS
		"texmaps.mul",	// Texture map data (the ground).
		"gumpidx.mul",	// Index to GUMPART
		"gumpart.mul",	// Gumps. Stationary controller bitmaps such as windows, buttons, paperdoll pieces, etc.
		"multi.idx",
		"multi.mul",	// Groups of art (houses, castles, etc)
		"skills.idx",
		"skills.mul",
		"radarcol.mul",	// ? color translation from terrain to radar map.
		"fonts.mul",	//  Fixed size bitmaps style fonts.
		"palette.mul",	// Contains an 8 bit palette (use unknown)
		"light.mul",	// light pattern bitmaps.
		"lightidx.mul", // light pattern bitmaps.
		"speech.mul",	// > 2.0.5 new versions only.
		"verdata.mul",
		"map2.mul",		// Terrain data
		"staidx2.mul",	// Index into STATICS0
		"statics2.mul", // Static objects on the map
		"",
		"",
		"",
		"tiledata.mul", // Data about tiles in ART. name and flags, etc
		"animdata.mul", //
		"hues.mul",		// the 16 bit color pallete we use for everything.

		"mapdifl0.mul",
		"mapdif0.mul",
		"stadifl0.mul",
		"stadifi0.mul",
		"stadif0.mul",

		"mapdifl1.mul",
		"mapdif1.mul",
		"stadifl1.mul",
		"stadifi1.mul",
		"stadif1.mul",

		"mapdifl2.mul",
		"mapdif2.mul",
		"stadifl2.mul",
		"stadifi2.mul",
		"stadif2.mul",

		NULL,
};

LPCTSTR CMulInstall::GetBaseFileName( VERFILE_TYPE i ) // static
{
	if ( i<0 || i>=VERFILE_QTY )
		return( NULL );
	return( sm_szFileNames[ i ] );
}

bool CMulInstall::OpenFile( VERFILE_TYPE i )
{
	ASSERT( i < VERFILE_QTY );
	CGFile* pFile = GetMulFile(i);
	if ( pFile == NULL )
		return false;
	if ( pFile->IsFileOpen())
		return( true );

	if ( ! pFile->GetFilePath().IsEmpty())
	{
		if ( pFile->Open( pFile->GetFilePath(), OF_READ|OF_SHARE_DENY_WRITE ))
			return true;
	}

	LPCTSTR pszTitle = GetBaseFileName( (VERFILE_TYPE) i );
	if ( pszTitle == NULL )
		return( false );

	return( OpenFile( m_File[i], pszTitle, OF_READ|OF_SHARE_DENY_WRITE ));
}

VERFILE_TYPE CMulInstall::OpenFiles( DWORD dwFilesMask, DWORD dwFilesMaskEx )
{
	// Now open all the required files.
	// REUTRN: 
	//  VERFILE_QTY = all open success.
	//  the failed file index.

	VERFILE_TYPE filefail = VERFILE_QTY;
	for ( int i=0; i<VERFILE_QTY; i++ )
	{
		if ( i>=0x20)
		{
			if ( ! _ISSET( dwFilesMaskEx, i ))
				continue;
		}
		else
		{
			if ( ! _ISSET( dwFilesMask, i ))
				continue;
		}

		if ( GetBaseFileName( (VERFILE_TYPE) i ) == NULL )
			continue;

		if ( ! OpenFile( (VERFILE_TYPE) i ))
		{
			// Problems ! Can't open the file anyplace !
			// some files are optional. VERFILE_VERDATA
			if ( filefail == VERFILE_QTY )
				filefail = (VERFILE_TYPE) i;
		}
	}

	return( filefail );
}

void CMulInstall::CloseFiles()
{
	for ( int i=0; i<VERFILE_QTY; i++ )
	{
		if ( ! m_File[i].IsFileOpen())
			continue;
		m_File[i].Close();
	}
}

#if 0

bool CMulInstall::IsValidMulIndex( VERFILE_TYPE fileindex, DWORD id )
{
	// Mul files may vary in size in the future.
	// Some are hard coded. or limited by scripts.

	switch ( fileindex )
	{
	case VERFILE_ANIMIDX:
	case VERFILE_ANIM:
		return( true );

	case VERFILE_SOUNDIDX:
	case VERFILE_SOUND:
		if ( id >= SOUND_QTY )
			return( false );
		return( true );

	case VERFILE_GUMPIDX:
	case VERFILE_GUMPART:
		return( true );

	case VERFILE_MULTIIDX:
	case VERFILE_MULTI:
		return( true );

	case VERFILE_SKILLSIDX:
	case VERFILE_SKILLS:
		return( true );
	}

	DEBUG_CHECK(0);
	return( false );
}

#endif

bool CMulInstall::ReadMulIndex( VERFILE_TYPE fileindex, VERFILE_TYPE fileverdata, DWORD id, CMulIndexRec& Index )
{
	// Read about this data type in one of the index files.
	// RETURN: true = we are ok.
	ASSERT(fileindex<VERFILE_QTY);

	// Is there an index for it in the VerData ?
	if ( g_MulVerData.FindVerDataBlock( fileverdata, id, Index ))
	{
		return( true );
	}

	// Just in the regular Index data.
	// NOTE: Seek to end of file = success ?!

	LONG lOffset = id * sizeof(CMulIndexRec);
	if ( m_File[fileindex].Seek( lOffset, SEEK_SET ) != lOffset )
	{
		//throw CGException(LOGL_CRIT, CGFile::GetLastError(), "CMulInstall:ReadMulIndex Seek");
		return( false );
	}
	if ( m_File[fileindex].Read( (void *) &Index, sizeof(CMulIndexRec)) != sizeof(CMulIndexRec))
	{
		//throw CGException(LOGL_CRIT, CGFile::GetLastError(), "CMulInstall:ReadMulIndex Read");
		return( false );
	}
	return( Index.HasData());
}

bool CMulInstall::ReadMulData( VERFILE_TYPE filedata, const CMulIndexRec& Index, void * pData )
{
	// Use CGFile::GetLastError() for error.
	if ( Index.IsVerData())
	{
		filedata = VERFILE_VERDATA;
	}
	if ( m_File[filedata].Seek( Index.GetFileOffset(), SEEK_SET ) != Index.GetFileOffset())
	{
		throw CGException(LOGL_CRIT, CGFile::GetLastError(), "CMulInstall:ReadMulData Seek");
		return( false );
	}
	DWORD dwLength = Index.GetBlockLength();
	if ( m_File[filedata].Read( pData, dwLength ) != dwLength )
	{
		throw CGException(LOGL_CRIT, CGFile::GetLastError(), "CMulInstall:ReadMulData Read");
		return( false );
	}
	return( true );
}


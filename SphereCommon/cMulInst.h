//
// CMulinst.h
// Copyright Menace Software (www.menasoft.com).
//

#ifndef _INC_CSPHEREINST_H
#define _INC_CSPHEREINST_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "cfile.h"
#include "CArray.h"

#include "spheremul.h"

////////////////////////////////////////////////////////

struct CMulInstall
{
	// "Software\\Menasoft\\Sphere"
	// "Software\\Origin Worlds Online\\Ultima Online\\1.0"
	// bool m_fFullInstall;	// Are all files avail ?
private:
	static LPCTSTR const sm_szFileNames[VERFILE_QTY+1];

	CGString m_sPreferPath;	// Prefer path in which to choose the files. (look here FIRST)
	CGString m_sExePath;		// Files that are installed. "ExePath"
	CGString m_sInstCDPath;		// For files that may still be on the CD. "InstCDPath"
public:
	CGFile m_File[ VERFILE_QTY ];	// Get our list of files we need to access.

public:
	CGString GetFullExePath( LPCTSTR pszName = NULL ) const
	{
		return( CGFile::GetMergedFileName( m_sExePath, pszName ));
	}
	CGString GetFullExePath( VERFILE_TYPE i ) const
	{
		return( GetFullExePath( GetBaseFileName( i )));
	}
	CGString GetFullCDPath( LPCTSTR pszName = NULL ) const
	{
		return( CGFile::GetMergedFileName( m_sInstCDPath, pszName ));
	}
	CGString GetFullCDPath( VERFILE_TYPE i ) const
	{
		return( GetFullCDPath( GetBaseFileName( i )));
	}

public:
	bool FindInstall();
	VERFILE_TYPE OpenFiles( DWORD dwMask, DWORD dwMaskEx );
	bool OpenFile( CGFile& file, LPCTSTR pszName, WORD wFlags );
	bool OpenFile( VERFILE_TYPE i );
	void CloseFiles();

	static LPCTSTR GetBaseFileName( VERFILE_TYPE i );
	CGFile* GetMulFile( VERFILE_TYPE i )
	{
		ASSERT( i<VERFILE_QTY );
		return( &(m_File[i]));
	}

	// set the attributes of a specific file.
	HRESULT SetMulFile( VERFILE_TYPE i, LPCTSTR pszName );

	void SetPreferPath( LPCTSTR pszName )
	{
		m_sPreferPath = pszName;
	}
	CGString GetPreferPath( LPCTSTR pszName = NULL ) const
	{
		return( CGFile::GetMergedFileName( m_sPreferPath.IsEmpty() ? m_sExePath : m_sPreferPath, pszName ));
	}
	CGString GetPreferPath( VERFILE_TYPE i ) const
	{
		return( GetPreferPath( GetBaseFileName( i )));
	}

	void SetExePath( LPCTSTR pszName );
	void SetInstCDPath( LPCTSTR pszName );

	bool IsValidMulIndex( VERFILE_TYPE fileindex, DWORD id );
	bool ReadMulIndex( VERFILE_TYPE fileindex, VERFILE_TYPE filedata, DWORD id, CMulIndexRec& Index );
	bool ReadMulData( VERFILE_TYPE filedata, const CMulIndexRec& Index, void * pData );
};

extern CMulInstall g_MulInstall;

///////////////////////////////////////////////////////////////////////////////

class CMulVerData : public CGSortedArray< CMulVersionBlock, const CMulVersionBlock&, HASH_INDEX>
{
	// Find version diffs to the files listed.
	// Assume this is a sorted array of some sort.
protected:
	int CompareKey( HASH_INDEX dwIndex, const CMulVersionBlock& verdata ) const
	{
		HASH_INDEX dwID2 = verdata.GetHashCode();
		return HASH_COMPARE( dwIndex, dwID2 );
	}
public:
	const CMulVersionBlock* GetEntry( int i ) const
	{
		return( &ConstElementAt(i));
	}
	bool Load( CGFile& file );
	bool FindVerDataBlock( VERFILE_TYPE type, DWORD id, CMulIndexRec& Index ) const;
};

extern CMulVerData g_MulVerData;

#endif	// _INC_CSPHEREINST_H_
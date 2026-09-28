//
// sphereproto.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"
#include "spherecommon.h"
#include "sphereproto.h"

int CvtSystemToNUNICODE( NCHAR* pOut, int iSizeOutChars, LPCTSTR pInp )
{
	// Convert a UTF8 string to network order UNICODE
	// Packed protocol fields can leave NCHAR at an odd address. Build each
	// network-order word in aligned storage and copy its bytes into the packet.
	int iOutMax = iSizeOutChars - 1;
	int iOut = 0;
	for ( ; iOut < iOutMax && pInp[iOut]; iOut++ )
	{
		NCHAR nChar;
		nChar = (WCHAR)(unsigned char)pInp[iOut];
		memcpy( reinterpret_cast<BYTE*>(pOut) + iOut * sizeof(NCHAR),
			&nChar, sizeof(nChar) );
	}

	NCHAR nTerminator;
	nTerminator = 0;
	memcpy( reinterpret_cast<BYTE*>(pOut) + iOut * sizeof(NCHAR),
		&nTerminator, sizeof(nTerminator) );
	return( iOut );
}

int CvtNUNICODEToSystem( TCHAR* pOut, int iSizeOutBytes, const NCHAR* pInp, int iInpMaxLen )
{
	// Convert a network order UNICODE string to UTF8 string.
	WCHAR szBuffer[ CSTRING_MAX_LEN+1 ];
	int iInp;
	// Check the length before reading: a packet string need not be terminated.
	for ( iInp=0; iInp < iInpMaxLen && iInp < COUNTOF(szBuffer)-1; iInp++ )
	{
		NCHAR nChar;
		memcpy( &nChar, reinterpret_cast<const BYTE*>(pInp) + iInp * sizeof(NCHAR), sizeof(nChar) );
		szBuffer[iInp] = nChar;
		if ( szBuffer[iInp] == 0 )
			break;
	}
	szBuffer[iInp] = '\0';
	return( CvtUNICODEToSystem( pOut,iSizeOutBytes,szBuffer,iInp));
}

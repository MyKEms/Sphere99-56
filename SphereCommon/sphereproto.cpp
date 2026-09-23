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
	// flip all the words to network order .
	int iOutTmp = CvtSystemToUNICODE( (WCHAR*) pOut, iSizeOutChars, pInp );
	int iOut;
	for ( iOut=0; iOut<iOutTmp; iOut++ )
	{
		pOut[iOut] = *((WCHAR*)&(pOut[iOut]));
	}
	return( iOut );
}

int CvtNUNICODEToSystem( TCHAR* pOut, int iSizeOutBytes, const NCHAR* pInp, int iInpMaxLen )
{
	// Convert a network order UNICODE string to UTF8 string.
	WCHAR szBuffer[ CSTRING_MAX_LEN+1 ];
	int iInp;
	// Check the length before reading: a packet string need not be terminated.
	for ( iInp=0; iInp < iInpMaxLen && iInp < COUNTOF(szBuffer)-1 && pInp[iInp]; iInp++ )
	{
		szBuffer[iInp] = pInp[iInp];
	}
	szBuffer[iInp] = '\0';
	return( CvtUNICODEToSystem( pOut,iSizeOutBytes,szBuffer,iInp));
}


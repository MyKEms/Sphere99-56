//
// CWebPage.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.
#include "cfiledir.h"
#define ERROR_INTERNAL_ERROR HRES_INTERNAL_ERROR
#define ERROR_INVALID_HANDLE HRES_INVALID_HANDLE
#define ERROR_BAD_ARGUMENTS HRES_BAD_ARGUMENTS
static int sm_iListColIndex = 0;

const CScriptProp CWebPageDef::sm_Props[CWebPageDef::P_QTY+1] =
{
#define CWEBPAGEPROP(a,b,c) CSCRIPT_PROP_IMP(a,b,c)
#include "cwebpageprops.tbl"
#undef CWEBPAGEPROP
	NULL,
};

#ifdef USE_JSCRIPT
#define CWEBPAGEMETHOD(a,b,c) JSCRIPT_METHOD_IMP(CWebPageDef,a)
#include "cwebpagemethods.tbl"
#undef CWEBPAGEMETHOD
#endif

const CScriptMethod CWebPageDef::sm_Methods[CWebPageDef::M_QTY+1] =
{
#define CWEBPAGEMETHOD(a,b,c) CSCRIPT_METHOD_IMP(a,b,c)
#include "cwebpagemethods.tbl"
#undef CWEBPAGEMETHOD
	NULL,
};

const CScriptProp CWebPageDef::sm_Triggers[CWebPageDef::T_QTY+1] =	// static
{
#define CWEBPAGEEVENT(a,b,c) {"@" #a,b,c},
	CWEBPAGEEVENT(AAAUNUSED,0,NULL)	// reserved for XTRIG_UNKNOWN
#include "cwebpageevents.tbl"
#undef CWEBPAGEEVENT
	NULL,
};

CSCRIPT_CLASS_IMP1(WebPageDef,CWebPageDef::sm_Props,CWebPageDef::sm_Methods,CWebPageDef::sm_Triggers,ResourceLink);

//////////////////////////////////////////////////////////
// -CServersSortArray

class CServersSortArray : public CGRefSortArray<CServerDef,CServerDef>
{
private:
	CServerDef::P_TYPE_ m_iType;	// ? SC_TYPE ? = type I am sorting by
public:
	CServersSortArray();
	int CompareData( CServerDef* pServLeft, CServerDef* pServRef ) const;
	virtual int CompareKey(CServerDef key, CServerDef* obj) const { return 0; } // stub
	void SortByType( CServerDef::P_TYPE_ iType );
};

int Str_CompareFair( const char* pszName1, const char* pszName2 )
{
	// special sort based on alnum. ignore non alnum chars
	// return( _stricmp( pszName1, pszName2 ));
	int i=0;
	for(;;)
	{
		TCHAR ch1;
		for(;;) // skip duplicates and non-alpha
		{
			ch1 = toupper( *pszName1++ );
			if ( ! ch1 )
				break;
			if ( isalpha(ch1))
			{
				if ( ch1 == toupper( *pszName1 ))
					continue;
				break;
			}
		}

		TCHAR ch2;
		for(;;)
		{
			ch2 = toupper( *pszName2++ );
			if ( ! ch2 )
				break;
			if ( isalpha(ch2))
			{
				if ( ch2 == toupper( *pszName2 ))
					continue;
				break;
			}
		}

		if ( ch1 == ch2 )
		{
			if ( ch1 == 0 )
				return( 0 );
			i++;
			continue;
		}

		if ( ch1 == 0 && i == 0 )
			return( -1 );
		if ( ch2 == 0 && i == 0 )
			return( 1 );

		return( (int) ch2 - (int) ch1 );
	}
}

int CServersSortArray::CompareData( CServerDef* pServLeft, CServerDef* pServRef ) const
{
	// 1 = left > right
	ASSERT(pServRef);
	switch ( m_iType )
	{
	default:
		// Just do a string compare?
		{
		CGVariant vValRetLeft;
		pServLeft->s_PropGet( m_iType, vValRetLeft, &g_Serv );
		CGVariant vValRetRight;
		pServRef->s_PropGet( m_iType, vValRetRight, &g_Serv );
		return vValRetRight.CompareData( vValRetLeft ); 
		}

	case CServerDef::P_Name:	// Name
		return Str_CompareFair( pServRef->GetName(), pServLeft->GetName()); 

	case CServerDef::P_ClientsAvg:
		return( pServRef->GetClientsAvg() - pServLeft->GetClientsAvg());

	case CServerDef::P_Age:
		return( pServRef->GetAgeHours() - pServLeft->GetAgeHours());

	case CServerDef::P_Lang:
		return( _stricmp( pServRef->m_sLang,pServLeft->m_sLang ));

	case CServerDef::P_Ver:
		return( _stricmp( pServRef->GetServerVersion(), pServLeft->GetServerVersion()));

	case CServerDef::P_TZ:
		return( pServRef->m_TimeZone - pServLeft->m_TimeZone );

	case CServerDef::P_LastValidDate:
	case CServerDef::P_LastValidTime:
		return( pServRef->GetTimeSinceLastValid() - pServLeft->GetTimeSinceLastValid());
	}
}

CServersSortArray::CServersSortArray()
{
	m_iType = CServerDef::P_Name;

	// Copy a bunch of pointers.
	CThreadLockPtr lock( &(g_Cfg.m_Servers));
	SetCount(g_Cfg.m_Servers.GetSize());
	for ( int i=0; i<GetSize(); i++ )
	{
		SetAt(i,g_Cfg.m_Servers.ElementAt(i));
	}
	if ( ! g_Cfg.m_sMainLogServerDir.IsEmpty())
	{
		// List myself first.
		InsertAt( 0, &g_Serv );
	}
}

void CServersSortArray::SortByType( CServerDef::P_TYPE_ iType )
{
	if ( iType < 0 )
		return;
	m_iType = iType;
	if ( GetSize() <= 1 )
		return;
	if ( iType == CServerDef::P_Name )		// just leave it the same.
		return;

	// Now quicksort the list to the needed format.
	QSort();
}

//********************************************************
// -CFileConsole

class CFileConsole : public CScriptConsole
{
	// output to a file.
public:
	CFileText m_FileOut;
public:
	virtual int GetPrivLevel() const
	{
		return PLEVEL_Admin;
	}
	virtual CGString GetName() const
	{
		return "WebFile";
	}
	virtual bool WriteString( LPCTSTR pszMessage )
	{
		if ( pszMessage == NULL || ISINTRESOURCE(pszMessage))
			return false;
		return(m_FileOut.WriteString(pszMessage));
	}
};

//********************************************************
// -CWebPageDef

CWebPageDef::CWebPageDef( CSphereUID rid ) :
	CResourceTriggered( rid )
{
	// Web page m_sWebPageFilePath
	m_type = WEBPAGE_TEMPLATE;
	m_privlevel=PLEVEL_Guest;
	m_iUpdatePeriod = 2*60*TICKS_PER_SEC;
}

HRESULT CWebPageDef::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceTriggered::s_PropGet( pszKey, vValRet, pSrc ));
	}
	switch (iProp)
	{
	case P_PLevel:
		vValRet.SetInt( m_privlevel );
		break;
	case P_WebPageDst:
	case P_WebPageFile:
		vValRet = m_sDstFilePath;
		break;
	case P_WebPageSrc:
		vValRet = m_sSrcFilePath;
		break;
	case P_WebPageUpdate:	// (seconds)
		vValRet.SetInt( m_iUpdatePeriod / TICKS_PER_SEC );
		break;
	default:
		DEBUG_CHECK(0);
		return( ERROR_INTERNAL_ERROR );
	}

	return NO_ERROR;
}

HRESULT CWebPageDef::s_PropSet( LPCTSTR pszKey, CGVariant& vVal ) // Load an item Script
{
	P_TYPE_ iProp = (P_TYPE_) s_FindMyPropKey(pszKey);
	if ( iProp < 0 )
	{
		return( CResourceTriggered::s_PropSet(pszKey, vVal));
	}
	switch ( iProp )
	{
	case P_PLevel:
		m_privlevel = (PLEVEL_TYPE) vVal.GetInt();
		break;
	case P_WebPageDst:
	case P_WebPageFile:
		m_sDstFilePath = vVal.GetPSTR();
		break;
	case P_WebPageSrc:
		return SetSourceFile( vVal.GetPSTR(), NULL );
	case P_WebPageUpdate:	// (seconds)
		m_iUpdatePeriod = vVal.GetInt()*TICKS_PER_SEC;
		if ( m_iUpdatePeriod && m_type == WEBPAGE_TEXT )
		{
			m_type = WEBPAGE_TEMPLATE;
		}
		break;
	default:
		DEBUG_CHECK(0);
		return( ERROR_INTERNAL_ERROR );
	}
	return( NO_ERROR );
}

HRESULT CWebPageDef::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	M_TYPE_ iProp = (M_TYPE_) s_FindMyMethodKey(pszKey);
	if ( iProp<0 )
	{
		return( CResourceTriggered::s_Method(pszKey, vArgs, vValRet, pSrc));
	}

	ASSERT(pSrc);
	sm_iListColIndex = 0;
	TCHAR szTmp2[ CSTRING_MAX_LEN ];

	switch ( iProp )
	{
	case M_WebPage:
		{
			// serv a web page to the pSrc
			CClientPtr pClient = PTR_CAST(CClient,pSrc);
			if ( pClient == NULL )
				return( ERROR_INVALID_HANDLE );
			return ServePage( pClient, vArgs.GetPSTR(), NULL );
		}
		break;

	case M_ClientList:
		{
			CSphereExpContext exec( NULL, pSrc );
			for ( CClientPtr pClient = g_Serv.GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
			{
				CCharPtr pChar = pClient->GetChar();
				if ( pChar == NULL )
					continue;
				if ( pChar->Player_IsDND())
					continue;
				DEBUG_CHECK( ! pChar->IsWeird());

				sm_iListColIndex++;

				LPCTSTR pszArgs;
				if ( vArgs.IsEmpty())
					pszArgs = _TEXT("<tr><td>%NAME%</td><td>%REGION.NAME%</td></tr>" LOG_CR);
				else
					pszArgs = vArgs.GetPSTR();

				exec.SetBaseObject( REF_CAST(CChar,pChar));
				strcpy( szTmp2, pszArgs );
				exec.s_ParseEscapes( szTmp2, CSCRIPT_PARSE_HTML|CSCRIPT_PARSE_NBSP );
				pSrc->WriteString( szTmp2 );
			}
		}
		break;

	case M_ServList:
		{
			// A list sorted in some special way
			CServersSortArray Array;

			int iArgs = vArgs.MakeArraySize();
			if ( iArgs >= 2 )
			{
				Array.SortByType( (CServerDef::P_TYPE_) s_FindKeyInTable( vArgs.GetArrayPSTR(0), CServerDef::sm_Props ));
				vArgs.RemoveArrayElement(0);
			}

			CSphereExpContext exec( NULL, pSrc );
			for ( int i=0; i<Array.GetSize(); i++ )
			{
				CServerLock pServ = Array[i];
				if ( pServ == NULL )
					continue;

				// NOTE: servers list onces per hour. if they have not registered in 8 hours then they are not valid.

				if ( pServ->GetConnectStatus() > 2 )
					continue;

				sm_iListColIndex++;

				LPCTSTR pszArgs;
				if ( vArgs.IsEmpty())
					pszArgs = _TEXT("<tr><td>%SERVNAME%</td><td>%STATUS%</td></tr>" LOG_CR);
				else
					pszArgs = vArgs.GetPSTR();

				exec.SetBaseObject( REF_CAST(CServerDef,pServ));
				strcpy( szTmp2, pszArgs );
				exec.s_ParseEscapes( szTmp2, CSCRIPT_PARSE_HTML|CSCRIPT_PARSE_NBSP );
				pSrc->WriteString( szTmp2 );
			}
		}
		break;

	case M_GuildList:
	case M_TownList:
		{
			if ( vArgs.IsEmpty())
				return( HRES_BAD_ARG_QTY );

			int iSize = (iProp == CWebPageDef::M_GuildList) ?
				g_World.m_GuildStones.GetSize() :
				g_World.m_TownStones.GetSize();

			CSphereExpContext exec( NULL, pSrc );
			for ( int i=0; i < iSize; i++ )
			{
				CItemStonePtr pStone = (iProp == CWebPageDef::M_GuildList) ?
					g_World.m_GuildStones[i] :
					g_World.m_TownStones[i];

				ASSERT( pStone != NULL );
				DEBUG_CHECK( ! pStone->IsWeird());

				if ( iProp == CWebPageDef::M_GuildList )
				{
					if ( ! pStone->IsType(IT_STONE_GUILD))
						continue;
				}
				else
				{
					if ( ! pStone->IsType(IT_STONE_TOWN))
						continue;
				}

				sm_iListColIndex++;
				strcpy( szTmp2, vArgs.GetPSTR() );
				exec.SetBaseObject( REF_CAST(CItemStone,pStone));
				exec.s_ParseEscapes( szTmp2, CSCRIPT_PARSE_HTML|CSCRIPT_PARSE_NBSP );
				pSrc->WriteString( szTmp2 );
			}
		}
		break;

	case M_GMPageList:
		{
			if ( vArgs.IsEmpty())
				return( HRES_BAD_ARG_QTY );
			CSphereExpContext exec( NULL, pSrc );
			CGMPagePtr pPage = g_World.m_GMPages.GetHead();
			for ( ; pPage!=NULL; pPage = pPage->GetNext())
			{
				sm_iListColIndex++;
				strcpy( szTmp2, vArgs.GetPSTR() );
				exec.SetBaseObject( REF_CAST(CGMPage,pPage));
				exec.s_ParseEscapes( szTmp2, CSCRIPT_PARSE_HTML|CSCRIPT_PARSE_NBSP );
				pSrc->WriteString( szTmp2 );
			}
		}
		break;

	default:
		DEBUG_CHECK(0);
		return( ERROR_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}

CGString CWebPageDef::GetErrorBasicText( int iErrorCode ) // static
{
	// HTTP error codes:
	//
	// 200 OK
	// 201 Created
	// 202 Accepted
	// 203 Non-Authorative Information
	// 204 No Content
	// 205 Reset Content
	// 206 Partial Content
	//
	// 300 Multiple Choices
	// 301 Moved Permanently
	// 302 Moved Temporarily
	// 303 See Other
	// 304 Not Modified
	// 305 Use Proxy
	//
	// 400 Bad Request
	// 401 Authorization Required
	// 402 Payment Required
	// 403 Forbidden
	// 404 Not Found
	// 405 Method Not Allowed
	// 406 Not Acceptable (encoding)
	// 407 Proxy Authentication Required
	// 408 Request Timed Out
	// 409 Conflicting Request
	// 410 Gone
	// 411 Content Length Required
	// 412 Precondition Failed
	// 413 Request Entity Too Long
	// 414 Request URI Too Long
	// 415 Unsupported Media Type
	//
	// 500 Internal Server Error
	// 501 Not Implemented
	// 502 Bad Gateway
	// 503 Service Unavailable
	// 504 Gateway Timeout
	// 505 HTTP Version Not Supported
	//

	switch (iErrorCode)
	{
	case 401: return "Authorization Required";
	case 403: return "Forbidden"; 
	case 404: return "Object Not Found"; 
	case 500: return "Internal Server Error";
	}
	return "Unknown Error"; 
}

LPCTSTR const CWebPageDef::sm_szPageExt[] =
{
	".bmp",
	".cfg",
	".gif",
	".htm",
	".html",
	".htt",
	".jpeg",
	".jpg",
	".js",	// client side script.
	".png",
	".txt",
	NULL,
};
static WEBPAGE_TYPE const sm_szPageExtType[] =
{
	WEBPAGE_BMP,
	WEBPAGE_TEXT,
	WEBPAGE_GIF,
	WEBPAGE_TEMPLATE,
	WEBPAGE_TEXT,
	WEBPAGE_TEMPLATE,
	WEBPAGE_JPG,
	WEBPAGE_JPG,
	WEBPAGE_TEXT,
	WEBPAGE_PNG,
	WEBPAGE_TEXT,
};

HRESULT CWebPageDef::SetSourceFile( LPCTSTR pszName, CClient* pClient )
{
	// attempt to set this to a source file.
	// test if it exists.
	// NOTE: Str_GetBare has already been called.

	int iLen = strlen( pszName );
	if ( iLen <= 3 )
		return( ERROR_BAD_ARGUMENTS );

	CSocketAddress PeerName;
	bool fMasterList = true;
	if ( pClient )
	{
		PeerName = pClient->m_Socket.GetPeerName();
		fMasterList = PeerName.IsSameIP( g_Cfg.m_RegisterServer );
	}

	LPCTSTR pszExt = CGFile::GetFileNameExt( pszName );
	if ( pszExt == NULL || pszExt[0] == '\0' )
		return( ERROR_BAD_ARGUMENTS );

	int iType = FindTableSorted( pszExt, sm_szPageExt, COUNTOF( sm_szPageExt )-1 );
	if ( iType < 0 )
	{
		if ( pClient )
		{
			g_Log.Event( LOG_GROUP_HTTP, LOGL_TRACE, "Bad HTTP file type request '%s' [%s]" LOG_CR, (LPCTSTR)pszName, PeerName.GetAddrStr() );
			if ( ! fMasterList )
				return( ERROR_BAD_ARGUMENTS );
		}
	}
	else
	{
		m_type = sm_szPageExtType[iType];
	}

	if ( pClient == NULL )
	{
		// this is being set via the Script files.
		// make sure the file is valid
		CScript FileRead;
		if ( ! g_Cfg.OpenScriptFind( FileRead, pszName ))
		{
			DEBUG_ERR(( "Can't open web page input '%s'" LOG_CR, (LPCTSTR) pszName ));
			return( ERROR_BAD_ARGUMENTS );
		}
		m_sSrcFilePath = FileRead.GetFilePath();
	}
	else
	{
		// Requested by a client
		// Don't allow path changes.

		if ( *pszName == '/' )	// allow this to start and thats all.
			pszName ++;
		if ( strstr( pszName, ".." ))	// this sort of access is not allowed.
			return( ERROR_BAD_ARGUMENTS );
		if ( strstr( pszName, "\\\\" ))	// this sort of access is not allowed.
			return( ERROR_BAD_ARGUMENTS );
		if ( strstr( pszName, "//" ))	// this sort of access is not allowed.
			return( ERROR_BAD_ARGUMENTS );
		m_sSrcFilePath = CGFile::GetMergedFileName( g_Cfg.m_sSCPBaseDir, pszName );
	}

	return( NO_ERROR );
}

bool CWebPageDef::IsMatch( LPCTSTR pszMatch ) const
{
	if ( pszMatch == NULL )	// match all.
		return( true );

	LPCTSTR pszDstName = GetDstName();

	// Page is generated periodically. match the output name
	// else match the source name
	LPCTSTR pszTry = CScript::GetFileNameTitle( pszDstName[0] ? pszDstName : (LPCTSTR)GetName() );

	return( ! _stricmp( pszTry, pszMatch ));
}

LPCTSTR const CWebPageDef::sm_szPageType[WEBPAGE_QTY+1] =
{
	"text/html",		// WEBPAGE_TEMPLATE
	"text/html",		// WEBPAGE_TEXT
	"image/x-xbitmap",	// WEBPAGE_BMP,
	"image/gif",		// WEBPAGE_GIF,
	"image/jpeg",		// WEBPAGE_JPG,
	"bin",				// WEBPAGE_BIN,
	NULL,				// WEBPAGE_QTY
};

bool CWebPageDef::GenerateTemplateFile( CSphereExpContext& exec )
{
	// Generate the status web pages.
	// Read in the tempalate page m_sSrcFilePath
	// Filter the XML type tags.
	// Output the status page "*.HTM"

	ASSERT(m_type == WEBPAGE_TEMPLATE);

	m_timeNextUpdate.InitTimeCurrent( m_iUpdatePeriod );

	if ( m_sSrcFilePath.IsEmpty())
		return false;
	if ( m_iUpdatePeriod < 0 )	// never change.
	{
		return true;
	}

	// g_Log.Event( LOG_GROUP_HTTP, LOGL_TRACE, "HTTP Page Generate '%s'" LOG_CR, (LPCTSTR) m_sSrcFilePath );
	CScript FileRead;
	if ( ! FileRead.Open( m_sSrcFilePath, OF_NONCRIT|OF_READ|OF_TEXT ))
	{
		return false;
	}

	CSphereScriptContext context( &FileRead );	// set this as the context.

	bool fScriptMode = false;

	while ( FileRead.ReadTextLine( false ))
	{
		TCHAR* pszLine = FileRead.GetLineBuffer();
		if ( ! fScriptMode )
		{
			TCHAR* pszHead = strstr( pszLine, "<script language=\"Sphere\">" );
			if ( pszHead != NULL )
			{
				// Deal with the stuff preceding the scripts.
				*pszHead = '\0';
				pszHead += 26;
				exec.s_ParseEscapes( pszLine, CSCRIPT_PARSE_HTML|CSCRIPT_PARSE_NBSP );
				exec.GetSrc()->WriteString( pszLine );
				fScriptMode = true;
				pszLine = pszHead;
			}
		}

		if ( fScriptMode )
		{
			// Look for the end of </script>
			GETNONWHITESPACE(pszLine);
			TCHAR* pszFormat = strstr( pszLine, "</script>" );
			if ( pszFormat != NULL )
			{
				*pszFormat = '\0';
				pszFormat += 9;
				fScriptMode = false;
			}

			if ( pszLine[0] != '\0' )
			{
				// Allow if/then logic ??? ExecuteScript( FileRead, TRIGRUN_SINGLE_EXEC )
				HRESULT hRes = exec.ExecuteCommand( pszLine );
				if ( IS_ERROR(hRes))
				{
					DEBUG_ERR(( "Web page source format error '%s'" LOG_CR, (LPCTSTR) pszLine ));
					continue;
				}
			}

			if ( pszFormat == NULL )	// still in script mode.
				continue;
			pszLine = pszFormat;	// process the rest of the line.
		}

		// Look for stuff we can displace here. <?STUFF?>
		exec.s_ParseEscapes( pszLine, CSCRIPT_PARSE_HTML|CSCRIPT_PARSE_NBSP );
		exec.GetSrc()->WriteString( pszLine );
	}

	return( true );
}

void CWebPageDef::OnTick( bool fNow )
{
	if ( m_type != WEBPAGE_TEMPLATE )
		return;
	if ( m_sDstFilePath.IsEmpty())
		return;
	if ( m_iUpdatePeriod <= 0 ) // These never change or ALWAYS generated on demand.
		return;
	if ( ! fNow )
	{
		// Time to generate periodical pages?
		if ( CServTime::GetCurrentTime() < m_timeNextUpdate )
			return;	// should still be valid
	}

	CFileConsole FileOut;
	if ( ! FileOut.m_FileOut.Open( m_sDstFilePath, OF_WRITE|OF_TEXT ))
	{
		DEBUG_ERR(( "Can't open web page output '%s'" LOG_CR, (LPCTSTR) m_sDstFilePath ));
		return;
	}

	CSphereExpContext exec( this, &FileOut );
	GenerateTemplateFile(exec);
}

int CWebPageDef::ServeTemplate( CClient* pClient, CSphereExpContext& exec )
{
	// WEBPAGE_TEMPLATE pages will always expire.
	// m_bin
	ASSERT( m_type == WEBPAGE_TEMPLATE );

	if ( ! GenerateTemplateFile( exec ))
		return 500;

	CGTime datetime;
	datetime.InitTimeCurrent();
	CGString sDate = datetime.FormatGmt(CTIME_FORMAT_DEFAULT);	// current date.

	char szTmp[ 2*1024 ];
	int iLen = sprintf( szTmp, 
		"HTTP/1.1 200 OK\r\n"	// 100 Continue
		"Server: " SPHERE_TITLE " V" SPHERE_VERSION "\r\n"	// Microsoft-IIS/4.0
		"Date: %s\r\n"
		"Content-Type: %s\r\n"
		"Accept-Ranges: bytes\r\n"
		"Last-Modified: %s\r\n"
		"Expires: 0\r\n"
		"ETag: \"%s\"\r\n"	// No idea. "094e54b9f48bf1:1f83"
		"Content-Length: %d\r\n"
		"\r\n"
		,
		(LPCTSTR) sDate,
		(LPCTSTR) sm_szPageType[ m_type ],	// type of the file. image/gif, image/x-xbitmap, image/jpeg
		(LPCTSTR) sDate,
		(LPCTSTR) GetName(),
		pClient->m_bin.GetDataQty()
		);
	pClient->xSendReady( szTmp, iLen );

	pClient->xSendReady( pClient->m_bin.RemoveDataLock(), pClient->m_bin.GetDataQty());

	pClient->m_bin.Empty();

	return 0;
}

int CWebPageDef::ServeFile( CClient* pClient, LPCTSTR pszSrcFile, CGTime* pdateIfModifiedSince )
{
	// serv the data from the file directly. (no template translation)
	// Get proper Last-Modified: time.
	// RETURN:
	//  Error Code.
	//
	// NOTE:
	//  We need to know "Content-Length:" in advance !?!?!

	CFileAttributes attr;
	if ( ! CFileDir::ReadFileInfo( pszSrcFile, attr ))
	{
		return 500;
	}

	CGTime datetime;
	datetime.InitTimeCurrent();
	CGString sDate = datetime.FormatGmt(CTIME_FORMAT_DEFAULT);	// current date.

	char szTmp[ 8*1024 ];
	int iLen;

	if ( pdateIfModifiedSince &&
		pdateIfModifiedSince->IsTimeValid() &&
		attr.m_timeChange <= pdateIfModifiedSince->GetTime() )
	{
		// return 304 (not modified)  if the "If-Modified-Since" is not true.
		// DEBUG_MSG(( "Web page '%s' not modified since '%s'" LOG_CR, (LPCTSTR) GetName(), ((CGTime)attr.m_timeChange).FormatGmt(NULL) ));
		// "\r\n" = 0d0a

		iLen = sprintf( szTmp, 
			"HTTP/1.1 304 Not Modified\r\n"
			"Server: " SPHERE_TITLE " V" SPHERE_VERSION "\r\n"
			"Date: %s\r\n"
			"ETag: \"%s\"\r\n"	// No idea. "094e54b9f48bf1:1f83"
			"Content-Length: 0\r\n"
			"\r\n",
			(LPCTSTR) sDate,
			(LPCTSTR) GetName());

		pClient->xSendReady( szTmp, iLen );
		return(0);
	}

	// Now serve up the page.
	CGFile FileRead;
	if ( ! FileRead.Open( pszSrcFile, OF_READ|OF_BINARY ))
		return 500;

	// Send the header first.
	iLen = sprintf( szTmp, 
		"HTTP/1.1 200 OK\r\n"	// 100 Continue
		"Server: " SPHERE_TITLE " V" SPHERE_VERSION "\r\n"	// Microsoft-IIS/4.0
		"Date: %s\r\n"
		"Content-Type: %s\r\n"
		"Accept-Ranges: bytes\r\n"
		"Last-Modified: %s\r\n"
		"ETag: \"%s\"\r\n"	// No idea. "094e54b9f48bf1:1f83"
		"Content-Length: %d\r\n"
		"\r\n"
		,
		(LPCTSTR) sDate,
		(LPCTSTR) sm_szPageType[ m_type ],	// type of the file. image/gif, image/x-xbitmap, image/jpeg
		(LPCTSTR) attr.m_timeChange.FormatGmt(CTIME_FORMAT_DEFAULT),
		(LPCTSTR) GetName(),
		attr.m_Size
		);
	pClient->xSendReady( szTmp, iLen );

	DWORD dwSize = attr.m_Size;
	for(;;)
	{
		iLen = FileRead.Read( szTmp, sizeof( szTmp ));
		if ( iLen <= 0 )
			break;
		pClient->xSendReady( szTmp, iLen );
		dwSize -= iLen;
		if ( iLen < sizeof( szTmp ))
		{
			// memset( szTmp+iLen, 0, sizeof(szTmp)-iLen );
			break;
		}
	}

	DEBUG_CHECK( dwSize == 0 );
	return(0);
}

int CWebPageDef::ServePageRequest( CClient* pClient, const char* pszURLArgs, CGTime* pdateIfModifiedSince )
{
	// Got a web page request from the client.
	// ARGS:
	//  pszURLArgs = args on the URL line ex. http://www.hostname.com/dir?args
	// RETURN:
	//  HTTP error code = 0=200 page was served.

	ASSERT(pClient);

	CSphereExpArgs exec(this,pClient);
	exec.m_ArgArray.AddHtmlArgs( pszURLArgs );

	TRIGRET_TYPE iRet = OnTriggerScript( exec, T_Load, sm_Triggers[T_Load].m_pszName );
	if ( iRet == TRIGRET_RET_VAL )
	{
		return( 0 );	// Block further action.
	}

	if ( m_privlevel )
	{
		if ( pClient->GetAccount() == NULL )
			return( 401 );	// Authorization required
		if ( pClient->GetPrivLevel() < m_privlevel )
			return( 403 );	// Forbidden
	}

	LPCTSTR pszDstName;

	if ( m_type == WEBPAGE_TEMPLATE ) // my version of cgi
	{
		pszDstName = GetDstName();

		// The page must be generated on demand.
		if ( pszDstName[0] == '\0' || ! m_iUpdatePeriod )
		{
			// Always generated.
			return ServeTemplate( pClient, exec );
		}
	}
	else
	{
		pszDstName = GetName();
	}

	return ServeFile( pClient, pszDstName, pdateIfModifiedSince );
}

bool CWebPageDef::ServePagePost( CClient* pClient, LPCTSTR pszURLArgs, TCHAR* pContentData, int iContentLength )
{
	// RETURN: true = this was the page of interest.

	ASSERT(pClient);

	if ( pContentData == NULL || iContentLength <= 0 )
		return( false );
	if ( ! HasTrigger(XTRIG_UNKNOWN))	// this form has no triggers.
		return( false );

	CSphereExpArgs exec(this,pClient);
	exec.m_ArgArray.AddHtmlArgs( pszURLArgs );

	// Parse the data.
	TCHAR* pszLast;
	pContentData[iContentLength] = '\0';
	if ( ! exec.m_ArgArray.AddHtmlArgs( pContentData, &pszLast ))
		return false;

	// Use the data in RES_WebPage block.
	CResourceLock s(this);
	if ( ! s.IsFileOpen())
		return( false );

	// Find the correct "submit" entry point.
	if ( ! s.FindTriggerName( pszLast ))
	{
		// Put up some sort of failure page ? No such "submit" button
		return false;
	}

	TRIGRET_TYPE iRet = exec.ExecuteScript( s, TRIGRUN_SECTION_TRUE );
	return( true );
}

HRESULT CWebPageDef::ServeGenericError( CClient* pClient, const char* pszPage, int iErrorCode ) // static
{
	CGTime datetime;
	datetime.InitTimeCurrent();
	CGString sDate = datetime.FormatGmt(CTIME_FORMAT_DEFAULT);
	CGString sMsgHead;
	CGString sText;

	sText.Format(
		"<html><head><title>Error %d</title>\r\n"
		"<meta name=\"robots\" content=\"noindex\">\r\n"
		"<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=iso-8859-1\"></head>\r\n"
		"<body>\r\n"
		"<h2>HTTP Error %d</h2>\r\n"
		"<p><strong>%d %s</strong></p>\r\n"
		"<p>%s</p>\r\n"
		"<p>The " SPHERE_TITLE " server cannot deliver the file or script you asked for.</p>\r\n"
		"<p>Please contact the server's administrator if this problem persists.</p>\r\n"
		"</body></html>\r\n",
		iErrorCode,
		iErrorCode,
		iErrorCode,
		(LPCTSTR) GetErrorBasicText(iErrorCode),
		pszPage );

	sMsgHead.Format(
		"HTTP/1.1 %d %s\r\n"
		"Server: " SPHERE_TITLE " V" SPHERE_VERSION "\r\n"
		"Date: %s\r\n"
		"Connection: close\r\n"
		"Content-Length: %d\r\n"
		"Content-Type: text/html\r\n"
		"\r\n%s",
		iErrorCode, 
		(LPCTSTR) GetErrorBasicText(iErrorCode),
		(LPCTSTR) sDate,
		sText.GetLength(),
		(LPCTSTR) sText );

	pClient->xSendReady( (LPCTSTR) sMsgHead, sMsgHead.GetLength() );
	return( ERROR_INVALID_HANDLE );
}

HRESULT CWebPageDef::ServePage( CClient* pClient, const char* pszPage, CGTime* pdateIfModifiedSince )	// static
{
	// make sure this is a valid format for the request.

	ASSERT(pClient);
	ASSERT(pszPage);

	TCHAR szPageName[ 260 ];
	int lenPageName = Str_GetBare( szPageName, pszPage, sizeof(szPageName), " !\"#$%&()*,:;<=>?[]^{|}-+'`" );

	int iErrorCode;
	CWebPagePtr pWebPage = g_Cfg.FindWebPage(szPageName);
	if ( pWebPage )
	{
		iErrorCode = pWebPage->ServePageRequest( pClient, pszPage, pdateIfModifiedSince );
		if ( ! iErrorCode )
			return NO_ERROR;
	}
	else
	{
		iErrorCode = 404;
	}

	// Is it a file in the Script directory ?
	if ( iErrorCode == 404 )
	{
		// temporarily create and render the page.
		CWebPagePtr tmppage = new CWebPageDef( CSphereUID( RES_WebPage, 0 ));
		HRESULT hRes = tmppage->SetSourceFile( szPageName, pClient );
		if ( hRes == NO_ERROR )
		{
			if ( ! tmppage->ServePageRequest( pClient, szPageName, pdateIfModifiedSince ))
				return NO_ERROR;
		}

		// Log an error of someone looking for bad page !
		g_Log.Event( LOG_GROUP_HTTP, LOGL_EVENT, "%x:HTTP Page Bad Request '%s'" LOG_CR, pClient->m_Socket.GetSocket(), (LPCTSTR) szPageName );
	}

	// Can't find it !?
	// just take the default page. or have a custom 404 page ?

	pClient->m_Targ.m_sText = pszPage;

	CGString sErrorPage;
	sErrorPage.Format( SPHERE_FILE "%d.htt", iErrorCode );

	pWebPage = g_Cfg.FindWebPage( sErrorPage );
	if ( pWebPage )
	{
		if ( ! pWebPage->ServePageRequest( pClient, pszPage, NULL ))
			return NO_ERROR;
	}

	// Hmm we should do something !!!?
	// Try to give a reasonable default error msg.

	return( ServeGenericError( pClient, pszPage, iErrorCode ));
}


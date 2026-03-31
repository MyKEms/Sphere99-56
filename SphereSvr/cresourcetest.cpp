//
// CResourceTest.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.
#include <signal.h>

bool CSphereResourceMgr::ResourceDump( const char* pArg )
{
	// Dump a particular type of resource out to a file.
	CFileText FileOut;
	int i = 0;

	switch ( toupper( pArg[0] ))
	{
	case '0':
	case 'A':
		// Dump all scripts to a single file.
		{
			bool fRemoveBlanks = true;

			if ( ! FileOut.Open( "dumpall.txt", OF_CREATE|OF_WRITE|OF_TEXT ))
				return( false );

			CResourceScriptPtr pResFile = &m_scpIni;
			for ( ;pResFile; pResFile = GetResourceFile(i++))
			{
				CResourceScriptPtr pScript = REF_CAST(CResourceScript,pResFile);
				if ( pScript == NULL )
					continue;
				if ( ! pScript->IsFileOpen() ) // OpenResourceCheck stub
					continue;
				while ( pScript->ReadTextLine(fRemoveBlanks))
				{
					if ( ! _strnicmp( pScript->GetLineBuffer(), "[EOF]", 5 ))
						continue;

					FileOut.WriteString( pScript->GetLineBuffer());
					if ( fRemoveBlanks )
					{
						FileOut.WriteString( "\n" );
					}
				}
				pScript->Close();
			}
		}
		return true;

	case '\0':
	case 'U':
	case 'D':
	case '1':
		{
			if ( ! FileOut.Open( "dumpdefs.txt", OF_WRITE|OF_TEXT ))
				return( false );

			int iQty = g_Cfg.m_Const.GetSize();
			for ( ; i < iQty; i++ )
			{
				if ( !( i%0x1ff ))
				{
					g_Serv.Event_PrintPercent( SERVTRIG_TestStatus, i, iQty );
				}
				FileOut.Printf( "%s=%s\n",
					(LPCTSTR) g_Cfg.m_Const[i]->GetKey(),
					(LPCTSTR) g_Cfg.m_Const[i]->GetPSTR());
			}
		}
		return true;

	case '2':
	case 'I':
		// dump the items database. as just a text doc.
		// Argument = hex bitmask to search thru.
		{
			if ( ! FileOut.Open( "dumpitems.txt", OF_WRITE|OF_TEXT ))
				return( false );

			DWORD dwFlagMask = 0xFFFFFFFF;	// UFLAG4_ANIM
			if ( pArg[1] == '0' )
			{
				dwFlagMask = Str_ahextou( pArg+1 );
			}

			for ( ; i < ITEMID_MULTI; i++ )
			{
				if ( !( i%0x1ff ))
				{
					g_Serv.Event_PrintPercent( SERVTRIG_TestStatus, i, ITEMID_MULTI );
				}

				CUOItemTypeRec item;
				if ( ! CItemDef::GetItemData((ITEMID_TYPE) i, &item ))
					continue;
				if ( ! ( item.m_flags & dwFlagMask ))
					continue;

				FileOut.Printf( "%04x: %08x,W%02x,L%02x,?%08x,A%08x,?%04x,H%02x,'%s'" LOG_CR,
					i,
					item.m_flags,
					item.m_weight,
					item.m_layer,
					item.m_dwUnk6,
					item.m_dwAnim,
					item.m_wUnk14,
					item.m_height,
					item.m_name );
			}
		}
		return true;

	case '3':
	case 'T':
		// dump the ground tiles database.
		{
			if ( ! FileOut.Open( "dumpterrain.txt", OF_WRITE|OF_TEXT ))
				return( false );

			for ( ; i<TERRAIN_QTY; i++ )
			{
				CMulTerrainInfo block( (TERRAIN_TYPE) i);

				if ( ! block.m_flags &&
					! block.m_index &&	// just counts up.  0 = unused.
					! block.m_name[0] )
					continue;

				FileOut.Printf( "%04x: %08x,%04x,'%s'" LOG_CR,
					i,
					block.m_flags,
					block.m_index,	// just counts up.  0 = unused.
					block.m_name );
			}
		}
		return true;

	case '4':
	case 'C':
	case 'N':
		// dump a list of npc's and chars from all RES_CharDef
		{
			if ( ! FileOut.Open( "dumpnpcs.txt", OF_WRITE|OF_TEXT ))
				return( false );

			FOR_HASH( g_Cfg.m_ResHash, i, j )
			{
				CResourceDefPtr pResDef = g_Cfg.m_ResHash.GetAtArray(i,j);
				ASSERT(pResDef);
				if ( RES_GET_TYPE(pResDef->GetUIDIndex()) != RES_CharDef )
					continue;
				CCharDefPtr pCharDef = CCharDef::TranslateBase(pResDef);
				if ( pCharDef == NULL )
					continue;
				FileOut.Printf( "[%04x] '%s'" LOG_CR, i, pCharDef->GetTypeName());
			}
		}
		return true;

	case '5':
	case 'P':
		// dump the packet length file to a file.
		// read directly from the client ?
		{
			CFileBin FileIn;
			if ( ! FileIn.Open( "dumppacketlen.dat", OF_READ ))
				return( false );
			if ( ! FileOut.Open( "dumppacketlen.txt", OF_WRITE|OF_TEXT ))
				return( false );

			for(;;)
			{
				DWORD bData[3];

				int iSize = FileIn.Read( (void*)bData, sizeof(bData));
				if ( iSize <= 0 )
					break;

				TCHAR szData[256];
				sprintf( szData, "0x%04x 0x%04x\n", bData[0], bData[1] );
				FileOut.WriteString( szData );
			}
		}
		return true;
	}

	return false;
}

static bool ResourceWriteLink( int id, CResourceLink* pLink, LPCTSTR pszKey, LPCTSTR pszVal )
{
	// Write to an alternate file that we are not reading from !
	ASSERT(pLink);
	CResourceScriptPtr pResFile = pLink->GetLinkFile();
	ASSERT(pResFile);

	TCHAR szSectionName[ SCRIPT_MAX_SECTION_LEN ];

	CGString sTmpName = CGFile::GetMergedFileName( "d:\\menace\\scripts\\tmp", pResFile->GetFileTitle() );
	CScript s;
	if ( ! s.Open( sTmpName, OF_NONCRIT ))
	{
		// Copy it to the Tmp dir if we can't find it !
		if ( true ) // CopyFileTo stub - always fail
		{
		bailout:
			DEBUG_ERR(( "Can't find item 0%x file '%s'" LOG_CR, id, (LPCTSTR) sTmpName ));
			return( false );
		}
		if ( ! s.Open( sTmpName ))
		{
			goto bailout;
		}
	}

	sprintf( szSectionName, "ITEMDEF 0%x", id );
	if ( ! s.FindSection( szSectionName, 0 ))
	{
		sprintf( szSectionName, "ITEMDEF %s", (LPCTSTR) pLink->GetResourceName() );
		if ( ! s.FindSection( szSectionName, 0 ))
		{
			DEBUG_ERR(( "Can't find item 0%x name '%s' in file '%s'" LOG_CR, id, (LPCTSTR) pLink->GetResourceName(), (LPCTSTR) sTmpName ));
			return( false );
		}
	}

	ASSERT(pszKey);
	// is it already set this way ?
	if ( s.FindKey( pszKey )) // Find a key in the current section
	{
		if ( pszVal && ! strcmp( pszVal, s.GetArgRaw()))
			return( true );
	}
	else
	{
		if ( pszVal == NULL )
			return( true );
	}

	s.WriteProfileStringSec( szSectionName, pszKey, pszVal );
	return( true );
}

static int sm_iResourceChanges;

bool CSphereResourceMgr::ResourceTestSkills()
{
	// '8'
	// Move the RESOURCE= and the TEST for skills in SPHERESKILL.SCP to proper ITEMDEF entries.
	// RES_SkillMenu
	// CARTOGRAPHY
	// BOWCRAFT
	// BLACKSMITHING
	// CARPENTRY
	// TAILORING
	// TINKERING

	sm_iResourceChanges = 0;

	FOR_HASH( m_ResHash, i, j )
	{
		CResourceDefPtr pResDef = m_ResHash.GetAtArray(i,j);
		if ( RES_GET_TYPE(pResDef->GetUIDIndex()) != RES_SkillMenu )
			continue;
		CResourceLinkPtr pResSkill = REF_CAST(CResourceLink,pResDef);
		if ( pResSkill == NULL )
		{
			ASSERT(0);
			continue;
		}
		CResourceLock s(pResSkill);
		if ( ! s.IsFileOpen())
		{
			return( false );
		}

		// any place "MAKEITEM" is used.

		CGVariant vResources;
		CGString sSkillMake;
		while ( s.ReadKeyParse())
		{
			if ( s.IsLineTrigger())
			{
				vResources.SetVoid();
				continue;
			}
			if ( s.IsKey( "TEST" ) || s.IsKey( "TESTIF" ))
			{
				sSkillMake = s.GetArgRaw();
				continue;
			}
			if ( s.IsKey( "RESOURCES" ))
			{
				vResources = s.GetArgRaw();
				continue;
			}
			if ( s.IsKey( "MAKEITEM" ) && ! vResources.IsEmpty())
			{
				// this should be last.
				// Write out what we learned here !
				ITEMID_TYPE id = (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, s.GetArgRaw() );
				CItemDefPtr pItemDef = g_Cfg.FindItemDef( (ITEMID_TYPE) id );

				if ( pItemDef == NULL || id <= 0)
				{
					DEBUG_ERR(( "Cant MAKEITEM '%s'" LOG_CR, (LPCTSTR) s.GetArgRaw()));
					continue;
				}

				if ( pItemDef->m_BaseResources.GetSize())
				{
					// we already have resources !
					// DEBUG_ERR(( "Resources conflict for '%s'" LOG_CR, (LPCTSTR) pItemDef->GetResourceName()));
					CResourceQtyArray SimpResources;
					SimpResources.s_LoadKeys( vResources );
					if ( ! ( SimpResources == pItemDef->m_BaseResources ))
					{
						ResourceWriteLink( id, pItemDef, "RESOURCES", vResources );
						pItemDef->s_PropGet( "RESOURCES", vResources, NULL );
						ResourceWriteLink( id, pItemDef, "RESOURCES2", vResources );
					}
				}
				else
				{
					ResourceWriteLink( id, pItemDef, "RESOURCES", vResources );
				}

				// Just overwrite the skill to make.
#if 0
				if ( pItemDef->m_SkillMake.GetCount())
				{
					CResourceQtyArray SkillMake;
					SkillMake.Load( sSkillMake );
					if ( ! ( SkillMake == pItemDef->m_SkillMake ))
					{
						DEBUG_ERR(( "SkillMake conflict for '%s'" LOG_CR, (LPCTSTR) pItemDef->GetResourceName()));
					}
				}
				else
#endif
				{
					for(;;)
					{
						TCHAR* pEquals = strchr( (TCHAR*)(LPCTSTR)sSkillMake, '=' );
						if ( pEquals == NULL )
							break;
						*pEquals = ' ';
					}
					ResourceWriteLink( id, pItemDef, "SKILLMAKE", sSkillMake );
				}

				continue;
			}
		}
	}

	return( true );
}

static void ResourceTestSortMove( CGString* sLines, int iTo, int iFrom )
{
	// NOTE : size we are just moving something we do not need to know the total size.
	if ( iFrom == iTo )
		return;

	sm_iResourceChanges++;

	ASSERT(iFrom>=0);
	ASSERT(iTo>=0);
	BYTE Tmp[ sizeof(CGString) ];
	memcpy( Tmp, &sLines[iFrom], sizeof(CGString));
	if ( iFrom < iTo )
	{
		memmove( &sLines[iFrom], &sLines[iFrom+1], sizeof(CGString)*((iTo-iFrom)));
		iTo--;
	}
	else
	{
		memmove( &sLines[iTo+1], &sLines[iTo], sizeof(CGString)*((iFrom-iTo)));
	}
	memcpy( &sLines[iTo], Tmp, sizeof(CGString));
}

static int ResourceTestSortSection( LPCTSTR psLastHead, CGString* sLines, int iLines )
{
	// THis is all outdated !!!
	static LPCTSTR const sm_szCharKeysDef[] =	// CCharDef
	{
		"DEFNAME",
		"DEFNAME2",
		"NAME",
		"ID",
		"CAN",
		"ICON",
		"ANIM",
		"SOUND",
		"BLOODCOLOR",
		"SHELTER",
		"AVERSIONS",
		"ATT",
		"ARMOR",
		"RESOURCES",
		"DESIRES",
		"FOODTYPE",
		"HIREDAYWAGE",
		"JOB",
		"MAXFOOD",
		//	"DEX",
		//	"STR",
		"TEVENTS",
		"TSPEECH",
		"DAM",
		"ARMOR",

		"CATEGORY",
		"DESCRIPTION",
		"SUBSECTION",
		NULL,
	};

	static LPCTSTR const sm_szCharKeysCreate[] =
	{
		"STR",
		"DEX",
		"INT",
		"COLOR",
		"BODY",
		"FLAGS",

		"ALCHEMY",
		"ANATOMY",
		"ANIMALLORE",
		"ITEMID",
		"ARMSLORE",
		"PARRYING",
		"BEGGING",
		"BLACKSMITHING",
		"BOWCRAFT",
		"PEACEMAKING",
		"CAMPING",
		"CARPENTRY",
		"CARTOGRAPHY",
		"COOKING",
		"DETECTINGHIDDEN",
		"ENTICEMENT",
		"EVALUATINGINTEL",
		"HEALING",
		"FISHING",
		"FORENSICS",
		"HERDING",
		"HIDING",
		"PROVOCATION",
		"INSCRIPTION",
		"LOCKPICKING",
		"MAGERY",
		"MAGICRESISTANCE",
		"TACTICS",
		"SNOOPING",
		"MUSICIANSHIP",
		"POISONING",
		"ARCHERY",
		"SPIRITSPEAK",
		"STEALING",
		"TAILORING",
		"TAMING",
		"TASTEID",
		"TINKERING",
		"TRACKING",
		"VETERINARY",
		"SWORDSMANSHIP",
		"MACEFIGHTING",
		"FENCING",
		"WRESTLING",
		"LUMBERJACKING",
		"MINING",
		"MEDITATION",
		"STEALTH",
		"REMOVETRAP",
		"NECROMANCY",

		"NPC",
		"FAME",
		"KARMA",
		"NEED",

		"SPEECH",
		"EVENTS",
		"TIMER",
		"TIMED",
		NULL,
	};

	static LPCTSTR const sm_szCharKeysEnd[] =
	{
		"CONTAINER",
		"ITEM",
		"ITEMNEWBIE",
		"ON",
		"ONTRIGGER",
		NULL,
	};

	static LPCTSTR const sm_szItemKeysDef[] =
	{
		"DEFNAME",
		"DEFNAME2",
		"NAME",
		"ID",
		"TYPE",
		"VALUE",

		"TDATA1",
		"TDATA2",
		"TDATA3",
		"TDATA4",

		"FLIP",
		"DYE",
		"LAYER",
		"PILE",
		"REPAIR",
		"REPLICATE",
		"REQSTR",
		"SKILL",
		"SKILLMAKE",
		//	"SPEED",
		"TWOHANDS",
		"WEIGHT",
		"DUPELIST",
		"DUPEITEM",
		"RESOURCES",
		"RESOURCES2",
		"DAM",
		"ARMOR",

		"MULTIREGION",
		"COMPONENT",

		"CATEGORY",
		"DESCRIPTION",
		"SUBSECTION",
		NULL,
	};

	static LPCTSTR const sm_szItemKeysCreate[] =
	{
		"COLOR",
		"AMOUNT",
		"ATTR",
		//	"CONT",
		//	"DISPID",
		//	"DISPIDDEC",
		"FRUIT",
		"HITPOINTS",	// for weapons
		//	"ID",
		"LAYER",
		//	"LINK",
		"MORE",
		"MORE1",
		"MORE2",
		"MOREP",
		"MOREX",
		"MOREY",
		"MOREZ",
		"TIMER",
		"TIMED",
		//	"P",
		NULL,
	};

	static LPCTSTR const sm_szItemKeysEnd[] =
	{
		"ON",
		"ONTRIGGER",
		NULL,
	};

	static LPCTSTR const sm_szItemKeysDupe[] =
	{
		"DUPEITEM",
		"CATEGORY",
		"DESCRIPTION",
		"SUBSECTION",
		"DEFNAME",
		"DEFNAME2",
		NULL,
	};

	static LPCTSTR const sm_szKeysDelete[] =
	{
		"SELLVALUE",
		"MATERIAL",
		"NOINDEX",
		NULL,
		// "SIMPRESOURCES",
	};

	if ( iLines == 0 )
		return 0;

	// what type of resource is this ?
	LPCTSTR const* pszKeysDef;
	int iKeysDef;
	LPCTSTR const* pszKeysCreate;
	int iKeysCreate;
	LPCTSTR const* pszKeysEnd;
	int iKeysEnd;
	RES_TYPE restype;

	if ( ! _strnicmp( psLastHead, "[ITEMDEF", 8 ))
	{
		restype = RES_ItemDef;
		pszKeysDef = sm_szItemKeysDef;
		iKeysDef = COUNTOF(sm_szItemKeysDef);
		pszKeysCreate = sm_szItemKeysCreate;
		iKeysCreate = COUNTOF(sm_szItemKeysCreate);
		pszKeysEnd = sm_szItemKeysEnd;
		iKeysEnd = COUNTOF(sm_szItemKeysEnd);
	}
	else if ( ! _strnicmp( psLastHead, "[CHARDEF", 8 ))
	{
		restype = RES_CharDef;
		pszKeysDef = sm_szCharKeysDef;
		iKeysDef = COUNTOF(sm_szCharKeysDef);
		pszKeysCreate = sm_szCharKeysCreate;
		iKeysCreate = COUNTOF(sm_szCharKeysCreate);
		pszKeysEnd = sm_szCharKeysEnd;
		iKeysEnd = COUNTOF(sm_szCharKeysEnd);
	}
	else
	{
		return iLines;
	}

	// leave header comments alone.
	int iLineStart = 0;
	for (;true;iLineStart++)
	{
		if ( iLineStart >= iLines )
			return iLines;
		if ( sLines[iLineStart][0] != '/' )
			break;
	}

	int iLineCreate = -1;
	int iLineEnd = iLineStart;

	bool fItemDupe = false;

	static LPCTSTR const sm_szMultiValidKey[] =
	{
		"COMPONENT",
		"DEFNAME2",
		"TSPEECH",
		"TEVENT",
		"SPEECH",
		"EVENT",
		"ON",
		NULL,
	};

	// Find the end and ON=@Create
	for ( ; iLineEnd < iLines; iLineEnd++ )
	{
		LPCTSTR pszTag = sLines[iLineEnd];
		GETNONWHITESPACE(pszTag);

		// First look for multiple instances of this .
		if ( pszTag[0] && pszTag[0] != '/' )
			for ( int j=iLineStart; j<iLineEnd; j++ )
		{
			LPCTSTR pszTest = sLines[j];
			GETNONWHITESPACE(pszTest);

			if ( ! _stricmp( pszTest, pszTag ))
			{
				DEBUG_ERR(( "Multi Instance of line %s" LOG_CR, (LPCTSTR) pszTag ));
				break;
			}

			for ( int k=0; true; k++ )
			{
				bool fEnd1 = ! _ISCSYM( pszTag[k] );
				bool fEnd2 = ! _ISCSYM( pszTest[k] );
				if ( fEnd1 && fEnd2 )
				{
					// NOTE: It's ok to duplicate some keys.
					if ( FindTableHead( pszTag, sm_szMultiValidKey ) >= 0 )
						break;

					DEBUG_ERR(( "Multi Instance of key %s" LOG_CR, (LPCTSTR) pszTag ));
					break;
				}
				if ( toupper( pszTag[k] ) != toupper( pszTest[k] ))
					break;	// no match
			}
		}

		if ( ! _strnicmp( pszTag, "ON=@Create", 10 ))
		{
			iLineCreate = iLineEnd;
			continue;
		}

		if ( ! _strnicmp( pszTag, "DEFNAME=", 8 ))
		{
			if ( _strnicmp( pszTag+8, ( restype == RES_ItemDef ) ? "i_" : "c_", 2 ))
			{
				DEBUG_WARN(( "DEFNAME '%s' does not match convention" LOG_CR, (LPCTSTR) (pszTag+8) ));
			}
			_strlwr( (TCHAR*)( pszTag+8 ));

			if ( iLineStart == iLineEnd )	// fine where it is.
				continue;

			if ( iLineEnd && ! _strnicmp( sLines[iLineEnd-1], "DEFNAME=", 8 ))
			{
				// make this DEFNAME2 !
				DEBUG_WARN(( "Multi DEFNAMEs" LOG_CR ));
				continue;
			}

			// put this at the top
			ResourceTestSortMove( sLines, iLineStart, iLineEnd );
			continue;
		}

		if ( ! _strnicmp( pszTag, "DUPEITEM=", 9 ))
		{
			fItemDupe = true;
			continue;
		}

		int j = FindTableHead( pszTag, pszKeysEnd );
		if ( j >= 0 )
			break;

		j = FindTableHead( pszTag, sm_szKeysDelete );
		if ( j >= 0 )
		{
			ResourceTestSortMove( sLines, iLines, iLineEnd );
			iLineEnd--;
			iLines--;
			continue;
		}
	}

	// Now sort the main defs section

	int i;
	for ( i=iLineStart; i<iLineEnd; i++ )
	{
		// where does this line go ?
		if ( iLineCreate >= 0 && i >= iLineCreate )	// done.
		{
			break;
		}

		LPCTSTR pszTag = sLines[i];
		if ( pszTag[0] == '\n' )
			continue;
		if ( pszTag[0] == '/' )
			continue;

		GETNONWHITESPACE(pszTag);

		if ( fItemDupe )	// this is a DUPEITEM
		{
			int j = FindTableHead( pszTag, sm_szItemKeysDupe );
			if ( j < 0 )
			{
				DEBUG_ERR(( "Needless duplication key '%s'" LOG_CR, (LPCTSTR) pszTag ));
#if 0
				ResourceTestSortMove( sLines, iLines, i );
				i--;
				iLineEnd--;
				iLines--;
#endif
				continue;
			}
		}

		int j = FindTableHead( pszTag, pszKeysDef );
		if ( j >= 0 )
		{
			continue;
		}

		j = FindTableHead( pszTag, pszKeysCreate );
		if ( j >= 0 )
		{
			// must move this down to the create section.

			if ( iLineCreate < 0 )
			{
				// there is none. so we must create it.
				sLines[iLines] = "ON=@Create\n";
				ResourceTestSortMove( sLines, iLineEnd, iLines );
				iLineCreate = iLineEnd;
				iLines++;
			}

			ResourceTestSortMove( sLines, iLineCreate+1, i );
			iLineCreate--;
			i--;
			continue;
		}

		DEBUG_ERR(( "Unknown key '%s'" LOG_CR, (LPCTSTR) pszTag ));

#if 0
		ResourceTestSortMove( sLines, iLines, i );
		i--;
		iLineEnd--;
		iLines--;
#endif

		continue;
	}

	// Now look at stuff after the ON=@Create

	for ( ++i; i<iLineEnd; i++ )
	{
		ASSERT( iLineCreate >= 0 );

		LPCTSTR pszTag = sLines[i];

		if ( pszTag[0] == '\n' )
			continue;
		if ( pszTag[0] == '/' )
			continue;
		GETNONWHITESPACE(pszTag);

		int j = FindTableHead( pszTag, pszKeysCreate);
		if ( j >= 0 )
		{
			continue;
		}

		j = FindTableHead( pszTag, pszKeysDef );
		if ( j >= 0 )
		{
			ResourceTestSortMove( sLines, iLineCreate-1, i );
			continue;
		}

		DEBUG_ERR(( "Unknown key '%s'" LOG_CR, (LPCTSTR) pszTag ));
#if 0
		ResourceTestSortMove( sLines, iLines, i );
		i--;
		iLineEnd--;
		iLines--;
#endif
		continue;
	}

	// now look for strange blanks.

	return( iLines );
}

bool CSphereResourceMgr::ResourceTestSort( LPCTSTR pszFilename )
{
	// '7' = test sort
	// This only applies to files with ITEMDEF and CHARDEF sections.

	ASSERT(pszFilename);
	if ( pszFilename[0] == '*' )
	{
		// do all the scripts.
		int i=0;
		for (;; i++ )
		{
			CResourceFilePtr pResFile = GetResourceFile( i );
			if ( pResFile == NULL )
				break;
			ResourceTestSort( pResFile->GetFilePath());
		}

		return( true );
	}

	CScript sFileInp;
	CScript* pFileInp;

	CResourceFilePtr pResFile = FindResourceFile( pszFilename );
	if ( pResFile == NULL )
	{
		sFileInp.SetFilePath( pszFilename );
		pFileInp = &sFileInp;
	}
	else
	{
		pFileInp = REF_CAST(CScript,pResFile);
		if ( pFileInp == NULL )
			return false;
		pFileInp->Close();
	}

	if ( ! pFileInp->Open( NULL, OF_READ ))
	{
		DEBUG_ERR(( "Can't load resource script '%s'" LOG_CR, (LPCTSTR) pFileInp->GetFilePath() ));
		return false;
	}

	CGString sOutName;
	sOutName = "tmpsort.scp";
	CScript sFileOut;
	if ( ! sFileOut.Open( sOutName, OF_WRITE ))
	{
		DEBUG_ERR(( "Can't open resource output script '%s'" LOG_CR, (LPCTSTR) sOutName ));
		return( false );
	}

	CSphereScriptContext context( pFileInp );	// set this as the error context.
	FILE_POS_TYPE lSizeInp = pFileInp->GetLength();

	CGString sLastHead;
	bool fInSection = false;
	int iTotalLines = 0;
	int iLines=0;
	CGString sLines[4*1024];

	sm_iResourceChanges = 0;
	while ( true )
	{
		if ( ! pFileInp->ReadTextLine( false ))
		{
			break;
		}

		LPCTSTR pszKey = pFileInp->GetKey();

		iTotalLines++;
		if ( ! ( iTotalLines & 0x3f ))
		{
			g_Serv.Event_PrintPercent( SERVTRIG_TestStatus, pFileInp->GetPosition(), lSizeInp );
		}

		if ( ! fInSection )
		{
			sFileOut.WriteString(pszKey);

			if ( pszKey[0] == '[' )	// we hit a new section !
			{
				sLastHead = pszKey;
				fInSection = true;
			}
			continue;
		}

		if ( pszKey[0] == '[' )	// we hit a new section !
		{
			// Process the old section. sort the tags.

			if ( iLines )
			{
				iLines = ResourceTestSortSection( sLastHead, sLines, iLines );

				// Clip off extra blank lines.
				while ( iLines > 0 && ( sLines[iLines-1][0] == '\0' || sLines[iLines-1][0] == '\n' ))
				{
					iLines--;
				}
				// Now write out all the lines in the section.
				for ( int i=0; i<iLines; i++ )
				{
					sFileOut.WriteString(sLines[i]);
				}
				sFileOut.WriteString("\n");	// spacer to the next section.
				iLines=0;
			}

			sLastHead = pszKey;
			sFileOut.WriteString(pszKey);
		}
		else
		{
			if ( iLines >= COUNTOF(sLines)-1 )
			{
				DEBUG_ERR(( "Section '%s' too large, %d changes" LOG_CR, (LPCTSTR) sLastHead, sm_iResourceChanges ));
				return( false );
			}
			sLines[iLines] = pszKey;
			iLines++;
		}
	}

	DEBUG_MSG(( "Sort resource '%s', %d changes" LOG_CR, pFileInp->GetFileTitle(), sm_iResourceChanges ));

	// delete the old and rename to the new !
	sFileOut.Close();
	pFileInp->Close();

	if ( sm_iResourceChanges )
	{
		remove( pFileInp->GetFilePath());

		int iRet = rename( sFileOut.GetFilePath(), pFileInp->GetFilePath() );
		if ( iRet )
		{
			DEBUG_WARN(( "rename of %s to %s failed = 0%x!" LOG_CR, (LPCTSTR) sFileOut.GetFilePath(), (LPCTSTR) pFileInp->GetFilePath(), iRet ));
		}
	}
	else
	{
		// remove( sFileOut.GetFilePath());
	}

	return( true );
}

bool CSphereResourceMgr::ResourceTestItemDupes()
{
	// '5'
	// xref the DUPEITEM= stuff to match DUPELIST=aa,bb,cc,etc stuff

	// read them all in first.
	int i=1;
	for ( ; i<ITEMID_MULTI; i++ )	// ITEMID_MULTI
	{
		if ( !( i%0x1ff ))
		{
			g_Serv.Event_PrintPercent( SERVTRIG_TestStatus, i, 2*ITEMID_MULTI );
		}

		CItemDefPtr pItemDef = g_Cfg.FindItemDef((ITEMID_TYPE) i );
		if ( pItemDef == NULL )
		{
			// Is it a defined item ?
			CUOItemTypeRec item;
			if ( ! CItemDef::GetItemData((ITEMID_TYPE) i, &item ))
				continue;

			// Seems we have a new item on our hands.
			g_Log.Event( LOG_GROUP_DEBUG, LOGL_EVENT, "[ITEMDEF 0%x] // %s //New" LOG_CR, i, (LPCTSTR) item.m_name );
			continue;
		}
	}

	// Now do XREF stuff. write out the differences.
	for ( i=1; i<ITEMID_MULTI; i++ )
	{
		if ( !( i%0x1ff ))
		{
			g_Serv.Event_PrintPercent( SERVTRIG_TestStatus, i+ITEMID_MULTI, 2*ITEMID_MULTI );
		}

		CItemDefPtr pItemDef = g_Cfg.FindItemDef((ITEMID_TYPE) i );
		if ( pItemDef == NULL )
			continue;
		if ( pItemDef->GetResourceID().GetResIndex() != i )	// this is just a DUPEITEM
			continue;

		CGVariant vVal;
		HRESULT hRes = pItemDef->s_PropGet( "DUPELIST", vVal, &g_Serv );
		if ( IS_ERROR(hRes))
		{
			ASSERT(0);
			continue;
		}

		ResourceWriteLink( i, pItemDef, "DUPELIST", ( vVal.IsEmpty() ) ? NULL : ((LPCTSTR) vVal ));

		CUOItemTypeRec item;
		if ( ! CItemDef::GetItemData((ITEMID_TYPE) i, &item ))
		{
			g_Log.Event( LOG_GROUP_DEBUG, LOGL_EVENT, "[ITEMDEF 0%x] // Undefined in tiledata.mul" LOG_CR, i );
		}

		IT_TYPE type = CItemDef::GetTypeBase( (ITEMID_TYPE) i, item );
		if ( type != IT_NORMAL && ! pItemDef->IsType(type) )
		{
			if ( pItemDef->IsType(IT_NORMAL) )
			{
				ResourceWriteLink( i, pItemDef, "TYPE", ResourceGetName( CSphereUID( RES_TypeDef, type )));
			}
			else
			{
				g_Log.Event( LOG_GROUP_DEBUG, LOGL_EVENT, "[ITEMDEF 0%x] // Type mismatch %s != %s ?" LOG_CR, i,
					(LPCTSTR) ResourceGetName( CSphereUID( RES_TypeDef, pItemDef->GetType() )),
					(LPCTSTR) ResourceGetName( CSphereUID( RES_TypeDef, type )));
			}
		}
	}

	return( true );
}

bool CSphereResourceMgr::ResourceTestItemMuls()
{
	// Xref the RES_ItemDef blocks with the tiledata.mul
	//  items database to make sure we have defined all.
	int i = 0;
	for ( ; i < ITEMID_MULTI; i++ )
	{
		if ( !( i%0x1ff ))
		{
			g_Serv.Event_PrintPercent( SERVTRIG_TestStatus, i, ITEMID_MULTI );
		}

		CUOItemTypeRec item;
		if ( ! CItemDef::GetItemData((ITEMID_TYPE) i, &item ))
			continue;

		CItemDefPtr pItemDef = g_Cfg.FindItemDef((ITEMID_TYPE) i );
		if ( pItemDef == NULL )
		{
			// Seems we have a new item on our hands.
			// Write it to the file !
			g_Log.Event( LOG_GROUP_DEBUG, LOGL_EVENT, "[%04x] // %s //New" LOG_CR, i, (LPCTSTR) item.m_name );
			continue;
		}

		// Make sure all the tiledata.mul info matches !?

	}
	return( true );
}

bool CSphereResourceMgr::ResourceTestCharAnims()
{
	// xref the ANIM= lines in RES_CharDef with ANIM.IDX

	if ( ! g_MulInstall.OpenFile( VERFILE_ANIMIDX ))
	{
		return( false );
	}

	for ( int id = 0; id < CREID_QTY; id ++ )
	{
		if ( id >= CREID_MAN )
		{
			// must already have an entry in the RES_CharDef. (don't get the clothing etc)

			continue;
		}

		// Does it have an entry in "ANIM.IDX" ?

		int index;
		int animqty;
		if ( id < CREID_HORSE1 )
		{
			// Monsters and high detail critters.
			// 22 actions per char.
			index = ( id* ANIM_QTY_MON* 5 );
			animqty = ANIM_QTY_MON;
		}
		else if ( id < CREID_MAN )
		{
			// Animals and low detail critters.
			// 13 actions per char.
			index = 22000 + (( id - CREID_HORSE1 )* ANIM_QTY_ANI* 5 );
			animqty = ANIM_QTY_ANI;
		}
		else
		{
			// Human and equip
			// 35 actions per char.
			index = 35000 + (( id - CREID_MAN )* ANIM_QTY_MAN* 5 );
			animqty = ANIM_QTY_MAN;
		}

		// It MUST have a walk entry !

		DWORD dwAnim = 0;	// mask of valid anims.
		for ( int anim = 0; anim < animqty; anim ++, index += 5 )
		{
			CMulIndexRec Index;
			if ( ! g_MulInstall.ReadMulIndex( VERFILE_ANIMIDX, VERFILE_ANIM, index, Index ))
			{
				if ( anim == 0 ) break;	// skip this.
				continue;
			}

			dwAnim |= ( 1L<<anim );
		}

		if ( dwAnim )
		{
			// report the valid animations.
			CGString vVal;
			vVal.FormatHex( dwAnim );
			CGString sSec;
			sSec.Format( "%04X", id );

			CCharDefPtr pCharDef = g_Cfg.FindCharDef((CREID_TYPE) id );
			if ( pCharDef == NULL )
			{
				continue;
			}

			CResourceScriptPtr pResFile = pCharDef->GetLinkFile();
			ASSERT(pResFile);

			pResFile->Close();
			pResFile->WriteProfileStringOffset( pCharDef->GetLinkContext().m_lOffset, "ANIM", vVal );
			pResFile->Close();
		}
	}
	return( true );
}

#if 0
void CSphereResourceMgr::ResourceTestMatchItems()
{
	// resolve all the defs for the item names.
	// Match up DEFS.SCP with the ITEM.SCP;

	CScript s;
	s.SetFilePath( SPHERE_FILE "ITEM" SCRIPT_EXT );

	int i=1;
	for ( ; i<ITEMID_MULTI; i++ )	// ITEMID_MULTI
	{
		if ( !( i%0x1ff ))
		{
			Event_PrintPercent( SERVTRIG_TestStatus, i, ITEMID_MULTI );
		}
		CItemDefPtr pItemDef = g_Cfg.FindItemDef((ITEMID_TYPE) i );
		if ( pItemDef == NULL )
			continue;

		if ( pItemDef->GetResourceID() != i )
			continue;

		int j = g_Cfg.m_Const.FindValNum( i );
		if ( j<0 )
		{
			for ( int k=0; pItemDef->GetFlippedID(k); k++ )
			{
				j = g_Cfg.m_Const.FindValNum( pItemDef->GetFlippedID(k));
				if ( j>=0) break;
			}
		}
		if ( j>= 0 )
		{
			s.WriteProfileStringOffset( sdfsdf, "DEFNAME", g_Cfg.m_Const[j]->GetName());
			g_Cfg.m_Const.RemoveAt(j);
		}

		j = g_Cfg.m_Const.FindValNum( i );
		if ( j<0 )
		{
			for ( int k=0; pItemDef->GetFlippedID(k); k++ )
			{
				j = g_Cfg.m_Const.FindValNum( pItemDef->GetFlippedID(k));
				if ( j>=0) break;
			}
		}
		if ( j>= 0 )
		{
			s.WriteProfileStringOffset( dfsdf, "DEFNAME2", g_Cfg.m_Const[j]->GetName());
			g_Cfg.m_Const.RemoveAt(j);
		}

	}

	// What DEFS are left ?
	for ( i=0; i< g_Cfg.m_Const.GetCount(); i++ )
	{
		g_Log.Event( LOG_GROUP_DEBUG, LOGL_EVENT, "Unused DEF = %s" LOG_CR, (LPCTSTR) g_Cfg.m_Const[i]->GetName());
	}
}

void CSphereResourceMgr::ResourceTestMatchChars()
{
	// resolve all the defs as char names.
	// Match up DEFS.SCP with the CHAR.SCP;

	CScript s;
	s.SetFilePath( SPHERE_FILE "CHAR" SCRIPT_EXT );

	int i=1;
	for ( ; i<CREID_QTY; i++ )	// ITEMID_MULTI
	{
		if ( !( i%0x07 ))
		{
			Event_PrintPercent( SERVTRIG_TestStatus, i, CREID_QTY );
		}

		CCharDefPtr pCharDef = g_Cfg.FindCharDef((CREID_TYPE) i );
		if ( pCharDef == NULL )
			continue;

		ASSERT( pCharDef->GetResourceID() == i );

		int j = g_Cfg.m_Const.FindValNum( i );
		if ( j>= 0 )
		{
			s.WriteProfileStringOffset( sSsdfsdfec, "DEFNAME", g_Cfg.m_Const[j]->GetName());
			g_Cfg.m_Const.RemoveAt(j);
		}

		j = g_Cfg.m_Const.FindValNum( i );
		if ( j>= 0 )
		{
			s.WriteProfileStringOffset( sSsfsdfec, "DEFNAME2", g_Cfg.m_Const[j]->GetName());
			g_Cfg.m_Const.RemoveAt(j);
		}
	}

	// What DEFS are left ?
	for ( i=0; i< g_Cfg.m_Const.GetCount(); i++ )
	{
		g_Log.Event( LOG_GROUP_DEBUG, LOGL_EVENT, "Unused DEF = %s" LOG_CR, (LPCTSTR) g_Cfg.m_Const[i]->GetName());
	}
}

#endif	// 0

static LPCTSTR const sm_szSkillMenuKeys[] =
{
	"TEST",
	"TESTIF",
	"RESOURCES",
	"RESTEST",
	"POLY",
	"SUMMON",
	"DRAWMAP",
	"REPLICATE",
	"MAKEITEM",
	"SKILLMENU",
	"REPAIR",
	NULL,
};

bool CSphereResourceMgr::ResourceTest( LPCTSTR pszFilename )
{
	// 'T' = Just run a test on this resource file.
	ASSERT(pszFilename);
	if ( pszFilename[0] == '*' )
	{
		// do all the scripts.
		int i=0;
		for (;; i++ )
		{
			CResourceFilePtr pResFile = GetResourceFile( i );
			if ( pResFile == NULL )
				break;
			ResourceTest( pResFile->GetFilePath());
		}

		return( true );
	}

	CResourceFilePtr pResFile = FindResourceFile( pszFilename );
	if ( pResFile == NULL )
	{
		pResFile = LoadResourcesAdd( pszFilename );
		if ( pResFile == NULL )
		{
			DEBUG_ERR(( "Can't load resource script '%s'" LOG_CR, (LPCTSTR) pszFilename ));
			return false;
		}
	}

	// NOW test all the stuff that is CResourceLink based !
	// CSphereScriptContext context( pResFile );	// set this for error reporting.

	CREID_TYPE idchar = (CREID_TYPE) ResourceGetIndexType( RES_CharDef, "c_h_provis" );
	CCharPtr pCharProvis = CChar::CreateNPC( idchar );
	if ( pCharProvis == NULL )
	{
		DEBUG_ERR(( "c_h_provis or DEFAULTCHAR is not valid %d!" LOG_CR, idchar ));
	}

	pCharProvis->MoveToChar( CPointMap( 1, 1 ));

	CItemContainerPtr pItemCont = REF_CAST(CItemContainer,CItem::CreateTemplate( ITEMID_BACKPACK, pCharProvis, pCharProvis ));
	if ( pItemCont == NULL )
	{
		DEBUG_ERR(( "Backpack item 0%x is not a container ?" LOG_CR, ITEMID_BACKPACK ));
	}

	CItemPtr pItemGold = CItem::CreateTemplate( ITEMID_GOLD_C1, NULL, NULL );
	ASSERT(pItemGold);
	pItemGold->MoveTo( CPointMap( 1, 1 ));

	// Now look for all the elements in it.

	FOR_HASH( m_ResHash, i, j )
	{
		CResourceDefPtr pResDef = m_ResHash.GetAtArray(i,j);
		ASSERT(pResDef);
		CResourceLinkPtr pLink = REF_CAST(CResourceLink,pResDef);
		if ( pLink == NULL )
			continue;
		if ( pLink->GetLinkFile() != pResFile )
			continue;

		CResourceLock s(pLink);
		if ( ! s.IsFileOpen())
		{
			continue;
		}

		CSphereUID uid( pLink->GetUIDIndex());
		RES_TYPE restype = uid.GetResType();
		switch ( restype )
		{
		case RES_CharDef:
			{
				CCharPtr pCharNew = CChar::CreateNPC( (CREID_TYPE) uid.GetResIndex() );
				ASSERT(pCharNew);
				pCharNew->DeleteThis();
			}
			break;

		case RES_ItemDef:
			{
				CItemPtr pItemNew = CItem::CreateScript( (ITEMID_TYPE) uid.GetResIndex(), NULL );
				pItemNew->MoveTo( CPointMap( 1, 1 ));

#if 0
				CItemDefPtr pItemNewDef = pItemNew->Item_GetDef();
				ASSERT(pItemNewDef);
				if ( pItemNewDef->m_BaseResources.GetCount() && pItemNewDef->GetBaseValueMin() > 0 )
				{
					DEBUG_ERR(( "%s: Odd to have both VALUE and RESOURCES" LOG_CR, (LPCTSTR) pItemNewDef->GetResourceName() ));
				}
#endif

				// Excercise all the possible triggers.
				CSphereExpContext se(pItemNew, pCharProvis);
				for ( int j=0; j<CItemDef::T_QTY; j++ )
				{
					// pLink
					pItemNew->OnTrigger( (CItemDef::T_TYPE_) j, se);
				}

				pItemNew->DeleteThis();
			}
			break;

		case RES_TypeDef:
			{
				pItemGold->SetType( (IT_TYPE) uid.GetResIndex());

				// Excercise all the possible triggers.
				CSphereExpContext se(pItemGold, pCharProvis);
				for ( int j=0; j<CItemDef::T_QTY; j++ )
				{
					pItemGold->OnTrigger( (CItemDef::T_TYPE_) j, se );
				}
			}
			break;
		case RES_Menu:
			// read in all the options to make sure they are valid.
			{
				int iOnHeaders = 0;
				CSphereExpContext exec(pCharProvis, pCharProvis);
				for ( int k=0; s.ReadKeyParse(); k++ )
				{
					if ( ! k )
						continue;

					if ( s.IsLineTrigger())
					{
						iOnHeaders ++;
						CMenuItem menuitem;
						if ( ! menuitem.ParseLine( s.GetArgMod(), exec ))
						{
						}
						continue;
					}

					if ( ! iOnHeaders )
						continue;

					if ( s.IsKey( "ADDITEM" ) || s.IsKey( "ADD" ))
					{
						ITEMID_TYPE id = (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, s.GetArgRaw());
						if ( id <= 1)	// pCharDef == NULL ||
						{
							DEBUG_ERR(( "Cant resolve %s=%s" LOG_CR, (LPCTSTR) s.GetKey(), (LPCTSTR) s.GetArgRaw()));
							continue;
						}
					}

					if ( s.IsKey( "ADDNPC" ))
					{
						CREID_TYPE id = (CREID_TYPE) g_Cfg.ResourceGetIndexType( RES_CharDef, s.GetArgRaw());
						if ( id < 1)	// pCharDef == NULL ||
						{
							DEBUG_ERR(( "Cant resolve %s=%s" LOG_CR, (LPCTSTR) s.GetKey(), (LPCTSTR) s.GetArgRaw()));
							continue;
						}
					}

					if ( s.IsKey( "GO" ))
					{
						CPointMap pt = GetRegionPoint( s.GetArgRaw());
						if ( ! pt.IsValidPoint())
						{
							DEBUG_ERR(( "Cant resolve %s=%s" LOG_CR, (LPCTSTR) s.GetKey(), (LPCTSTR) s.GetArgRaw()));
							continue;
						}
					}

					// we should try to check other stuff as well.
				}
			}
			break;
		case RES_EMailMsg:
			break;
		case RES_Speech:
			break;
		case RES_Names:
			break;
		case RES_Events:
			// trigger all the events on the cchar ?
			{
				// Excercise all the possible triggers.
				ASSERT(CCharDef::T_QTY<=64);
				CSphereExpContext exec(pCharProvis,&g_Serv);
				CResourceTrigPtr pTrig = REF_CAST(CResourceTriggered,pLink);
				for ( int j=0; j<CCharDef::T_QTY; j++ )
				{
					TRIGRET_TYPE iRet = pTrig->OnTriggerScript( exec, j, CCharDef::sm_Triggers[j].m_pszName );
				}
			}
			break;
		case RES_RegionType:
			{
				// Excercise all the possible triggers.

				CPointMap pt( 1,1,1 );
				CRegionComplexPtr pRegion = REF_CAST(CRegionComplex,pt.GetRegion(REGION_TYPE_AREA));
				ASSERT(pRegion);
				CSphereExpContext exec(pRegion,pCharProvis);
				CResourceTrigPtr pTrig = REF_CAST(CResourceTriggered,pLink);
				for ( int j=0; j<CRegionType::T_QTY; j++ )
				{
					TRIGRET_TYPE iRet = pTrig->OnTriggerScript( exec, j, CRegionType::sm_Triggers[j].m_pszName );
				}
			}
			break;
		case RES_Scroll:
			break;
		case RES_BlockEMail:
			break;
		case RES_Book:
			break;
		case RES_Newbie:
			pCharProvis->ReadScript( s );
			pItemCont->DeleteAll();
			break;
		case RES_Dialog:
			// Excercise all the triggers.
			break;
		case RES_Template:
			{
				CItemPtr pItemNew = CItem::CreateTemplate( (ITEMID_TYPE) uid.GetResIndex(), pItemCont, NULL );
				pItemCont->DeleteAll();
			}
			break;
		case RES_SkillMenu:	// - try making all the items.
			{
				int iOnHeaders = 0;
				int iTestCount = 0;
				CSphereExpContext exec(pCharProvis, pCharProvis);
				for ( int k=0; s.ReadKeyParse(); k++ )
				{
					if ( ! k )
						continue;
					if ( s.IsLineTrigger())
					{
						iOnHeaders++;
						CMenuItem menuitem;
						if ( ! menuitem.ParseLine( s.GetArgMod(), exec ))
						{
						}
						continue;
					}

					int j = FindTableHead( s.GetKey(), sm_szSkillMenuKeys );
					if ( j < 0 )
					{
						DEBUG_ERR(( "Odd key '%s'" LOG_CR, (LPCTSTR) s.GetKey() ));
						continue;
					}

					if ( j <= 1 )
						iTestCount = 0;

					if ( s.IsKey( "TEST" ) || s.IsKey( "TESTIF" ))
					{
						if ( iTestCount )
						{
							DEBUG_WARN(( "Multiple TEST=, combine these on a line!" LOG_CR ));
						}
						iTestCount++;
						continue;
					}
					if ( s.IsKey( "MAKEITEM" ))
					{
						ITEMID_TYPE id = (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, s.GetArgRaw() );
						if ( id <= 1)	// pCharDef == NULL ||
						{
							DEBUG_ERR(( "Cant resolve MAKEITEM '%s'" LOG_CR, (LPCTSTR) s.GetArgRaw()));
							continue;
						}

						CItemDefPtr pItemDef = g_Cfg.FindItemDef( (ITEMID_TYPE) id );
						if ( pItemDef == NULL )
						{
							DEBUG_ERR(( "Cant MAKEITEM '%s'" LOG_CR, (LPCTSTR) s.GetArgRaw()));
							continue;
						}

						if ( pItemDef->GetID() != id )
						{
							DEBUG_ERR(( "MAKEITEM '%s' is a DUPEITEM!" LOG_CR, (LPCTSTR) s.GetArgRaw()));
							continue;
						}

						continue;
					}
				}
			}
			break;
		default:
			ASSERT(0);
			break;
		}
	}

	// RES_Function:
	// RES_Help:

	return( true );
}


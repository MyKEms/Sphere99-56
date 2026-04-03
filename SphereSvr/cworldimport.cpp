//
// CWorldImport.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"	// predef header.

struct CImportSer : public CGObListRec
{
	// Temporary holding structure for new objects being impoted.
public:
	DECLARE_LISTREC_TYPE(CImportSer);
public:
	// Translate the import UID's into my UID's
	const DWORD m_dwSer;		// My Imported serial number
	CObjBasePtr m_pObj;	// new world object corresponding.
	DWORD m_dwContSer;	// My containers' serial number
	LAYER_TYPE m_layer;	// UOX does this diff than us. so store this here.
public:
	bool IsTopLevel() const
	{
		return( m_dwContSer == UID_INDEX_MASK );
	}

	CImportSer( DWORD dwSer ) :
	m_dwSer( dwSer )
	{
		m_pObj = NULL;
		m_dwContSer = UID_INDEX_MASK;
		m_layer = LAYER_NONE;
	}
	~CImportSer()
	{
		// if ( m_pObj != NULL ) delete m_pObj;
	}
};

struct CImportFile
{
	CObjBasePtr m_pCurObj;		// current entry.
	CImportSer* m_pCurSer;	// current entry.

	CGObListType<CImportSer> m_ListSer;	// list of imported objects i'm working on.

	const WORD m_wModeFlags;	// IMPFLAGS_TYPE
	const CPointMap m_ptCenter;
	const int m_iDist;	// distance fom center.

	LPCSTR m_pszArg1;	// account
	LPCSTR m_pszArg2;	// name

public:
	CImportFile( WORD wModeFlags, CPointMap ptCenter, int iDist ) :
	m_wModeFlags(wModeFlags),
	m_ptCenter(ptCenter),
	m_iDist(iDist)
	{
		m_pCurSer = NULL;
		m_pCurObj = NULL;
	}
	void CheckLast();
	void ImportFix();
	bool ImportSCP( CScript & s, WORD wModeFlags );
	bool ImportWSC( CScript & s, WORD wModeFlags );
};

void CImportFile::CheckLast()
{
	// Make sure the object we where last working on is linked to the world.

	if ( m_pCurSer != NULL )
	{
		if ( m_pCurObj != NULL && m_pCurSer->m_pObj == m_pCurObj )
		{
			// Do we even want it ?
			if ( m_iDist &&
				m_ptCenter.IsValidPoint() &&
				m_pCurSer->IsTopLevel() &&
				m_ptCenter.GetDist( m_pCurObj->GetTopPoint()) > m_iDist )
			{
				delete m_pCurSer;
			}
			else
			{
				m_pCurObj = NULL;	// accept it.
			}
		}
		else
		{
			delete m_pCurSer;
		}
		m_pCurSer = NULL;
	}
	if ( m_pCurObj != NULL )
	{
		delete m_pCurObj;
		m_pCurObj = NULL;
	}
}

void CImportFile::ImportFix()
{
	// adjust all the containered items and eliminate duplicates.

	CheckLast();

	int iRemoved = 0;

	CImportSer* pSerNext;
	m_pCurSer = m_ListSer.GetHead();
	for ( ; m_pCurSer != NULL; m_pCurSer = pSerNext )
	{
		pSerNext = m_pCurSer->GetNext();
		if ( m_pCurSer->m_pObj == NULL )		// NEver created correctly
		{
			delete m_pCurSer;
			continue;
		}

		// Make sure this item is not a dupe ?

		CItemPtr pItemTest;
		if ( m_pCurSer->IsTopLevel())	// top level only
		{
			if ( m_pCurSer->m_pObj->IsItem())
			{
				CItemPtr pItemCheck = REF_CAST(CItem,m_pCurSer->m_pObj );
				ASSERT(pItemCheck);
#if 1
				pItemCheck->SetAttr(ATTR_MOVE_NEVER);
#endif
				CWorldSearch AreaItems( m_pCurSer->m_pObj->GetTopPoint());
				for(;;)
				{
					CItemPtr pItem = AreaItems.GetNextItem();
					if ( pItem == NULL )
						break;
					if ( ! pItem->IsSameType( m_pCurSer->m_pObj ))
						continue;
					pItem->SetName( m_pCurSer->m_pObj->GetName());
					if ( ! ( m_pCurSer->m_pObj->GetTopZ() == pItem->GetTopZ()))
						continue;

					if ( g_Log.IsLogged( LOGL_TRACE ))
					{
						DEBUG_ERR(( "Import: delete dupe item" LOG_CR ));
					}
					goto item_delete;
				}
			}
			else
			{
				// dupe char ?
			}

			// Make sure the top level object is placed correctly.
			m_pCurSer->m_pObj->MoveTo( m_pCurSer->m_pObj->GetTopPoint());
			if ( ! m_pCurSer->m_pObj->IsContainer())
				delete m_pCurSer;
			continue;
		}

		pItemTest = REF_CAST(CItem,m_pCurSer->m_pObj);
		if ( pItemTest == NULL )
		{
			if ( g_Log.IsLogged( LOGL_TRACE ))
			{
				DEBUG_ERR(( "Import:Invalid item in container" LOG_CR ));
			}
		item_delete:
			delete m_pCurSer->m_pObj;
			delete m_pCurSer;
			iRemoved ++;
			continue;
		}

		// Find it's container.
		CImportSer* pSerCont = m_ListSer.GetHead();
		CObjBasePtr pObjCont;
		for ( ; pSerCont != NULL; pSerCont = pSerCont->GetNext())
		{
			if ( pSerCont->m_pObj == NULL )
				continue;
			if ( pSerCont->m_dwSer == m_pCurSer->m_dwContSer )
			{
				pObjCont = pSerCont->m_pObj;
				HRESULT hRes = pItemTest->LoadSetContainer( pObjCont->GetUID(), m_pCurSer->m_layer );
				if ( IS_ERROR(hRes))
				{
					goto item_delete;	// not in a cont ?
				}
				m_pCurSer->m_dwContSer = UID_INDEX_MASK;	// found it.
				break;
			}
		}
		if ( ! m_pCurSer->IsTopLevel())
		{
			if ( g_Log.IsLogged( LOGL_TRACE ))
			{
				DEBUG_ERR(( "Import:Invalid item container" LOG_CR ));
			}
			goto item_delete;
		}

		// Is it a dupe in the container or equipped ?
		CContainer* pCont = REF_CAST(CContainer,pObjCont);
		ASSERT(pCont);
		CItemPtr pItem = pCont->GetHead();
		for ( ; pItem != NULL; pItem = pItem->GetNext())
		{
			if ( pItemTest == pItem )
				continue;
			if ( pItemTest->IsItemEquipped())
			{
				if ( pItemTest->GetEquipLayer() != pItem->GetEquipLayer())
					continue;
			}
			else
			{
				if ( ! pItemTest->GetUnkPoint().IsSame2D( pItem->GetUnkPoint()))
					continue;
			}
			if ( ! pItemTest->IsSameType( pItem ))
				continue;
			if ( g_Log.IsLogged( LOGL_TRACE ))
			{
				DEBUG_ERR(( "Import: delete dupe item" LOG_CR ));
			}
			goto item_delete;
		}

		// done with it if not a container.
		if ( ! pItemTest->IsContainer())
		{
			delete m_pCurSer;
		}
	}

	if ( iRemoved )
	{
		DEBUG_ERR(( "Import: removed %d bad items" LOG_CR, iRemoved ));
	}
	m_ListSer.DeleteAll();	// done with the list now.
}

bool CImportFile::ImportSCP( CScript& s, WORD wModeFlags )
{
	// Import a SPHERE format SCP file.

	while ( s.FindNextSection())
	{
		CheckLast();
		if ( s.IsSectionType( "ACCOUNT" ))
		{
			g_Cfg.LoadScriptSection( s );
			continue;
		}
		else if ( s.IsSectionType( "WORLDCHAR" ))
		{
			ImportFix();
			if ( wModeFlags & IMPFLAGS_CHARS )
			{
				m_pCurObj = CChar::CreateBasic( (CREID_TYPE) g_Cfg.ResourceGetIndexType( RES_CharDef, s.GetArgRaw()));
			}
		}
		else if ( s.IsSectionType( "WORLDITEM" ))
		{
			if ( wModeFlags & IMPFLAGS_ITEMS )
			{
				m_pCurObj = CItem::CreateTemplate( (ITEMID_TYPE) g_Cfg.ResourceGetIndexType( RES_ItemDef, s.GetArgRaw()), NULL, NULL );
			}
		}
		else
		{
			continue;
		}

		if ( m_pCurObj == NULL )
			continue;

		while ( s.ReadKeyParse())
		{
			if ( s.IsKey( "SERIAL"))
			{
				if ( m_pCurSer != NULL )
					return( false );
				m_pCurSer = new CImportSer( s.GetArgInt());
				m_pCurSer->m_pObj = m_pCurObj;
				m_ListSer.InsertHead( m_pCurSer );
				continue;
			}

			if ( m_pCurSer == NULL )
				continue;

			if ( s.IsKey( "CONT" ))
			{
				m_pCurSer->m_dwContSer = s.GetArgInt();
			}
			else if ( s.IsKey( "LAYER" ))
			{
				m_pCurSer->m_layer = (LAYER_TYPE) s.GetArgInt();
			}
			else
			{
				m_pCurObj->s_PropSet(s.GetKey(), s.GetArgVar());
			}
		}
	}

	return( true );
}

bool CImportFile::ImportWSC( CScript& s, WORD wModeFlags )
{
	// This file is a WSC or UOX world script file.

	int mode = IMPFLAGS_NOTHING;
	CGString sName;
	CItemPtr pItem;
	CCharPtr pChar;

	while ( s.ReadTextLine(true))
	{
		if ( s.IsKeyHead( "SECTION WORLDITEM", 17 ))
		{
			CheckLast();
			mode = IMPFLAGS_ITEMS;
			continue;
		}
		else if ( s.IsKeyHead( "SECTION CHARACTER", 17 ))
		{
			CheckLast();
			mode = ( wModeFlags & IMPFLAGS_CHARS ) ? IMPFLAGS_CHARS : IMPFLAGS_NOTHING;
			continue;
		}
		else if ( s.GetKey()[0] == '{' )
		{
			CheckLast();
			continue;
		}
		else if ( s.GetKey()[0] == '}' )
		{
			CheckLast();
			mode = IMPFLAGS_NOTHING;
			continue;
		}
		else if ( mode == IMPFLAGS_NOTHING )
			continue;
		else if ( s.GetKey()[0] == '\\' )
			continue;

		// Parse the line.
		TCHAR* pArg = strchr( s.GetKey(), ' ' );
		if ( pArg != NULL )
		{
			*pArg++ = '\0';
			pArg = Str_GetNonWhitespace( pArg );
		}
		else
		{
			pArg = "";
		}
		if ( s.IsKey("SERIAL" ))
		{
			if ( m_pCurSer != NULL )
				return( false );

			DWORD dwSerial = atoi( pArg );
			if ( dwSerial == UID_INDEX_MASK )
			{
				DEBUG_ERR(( "Import:Bad serial number" LOG_CR ));
				break;
			}
			m_pCurSer = new CImportSer( dwSerial );
			m_ListSer.InsertHead( m_pCurSer );
			continue;
		}
		if ( s.IsKey("NAME" ))
		{
			sName = ( pArg[0] == '#' ) ? "" : pArg;
			if ( mode == IMPFLAGS_ITEMS )
				continue;
		}
		if ( m_pCurSer == NULL )
		{
			DEBUG_ERR(( "Import:No serial number" LOG_CR ));
			break;
		}
		if ( mode == IMPFLAGS_ITEMS )	// CItem.
		{
			if ( s.IsKey("ID" ))
			{
				if ( m_pCurObj != NULL )
					return( false );
				pItem = CItem::CreateTemplate( (ITEMID_TYPE) atoi( pArg ), NULL, NULL);
				pItem->SetName( sName );
				m_pCurObj = pItem;
				m_pCurSer->m_pObj = pItem;
				continue;
			}

			if ( m_pCurObj == NULL )
			{
				DEBUG_ERR(( "Import:Bad Item Key '%s'" LOG_CR, s.GetKey()));
				break;
			}
			if ( s.IsKey("CONT" ))
			{
				m_pCurSer->m_dwContSer = atoi(pArg);
			}
			else if ( s.IsKey("X" ))
			{
				CPointMap pt = pItem->GetUnkPoint();
				pt.m_x = atoi(pArg);
				pItem->SetUnkPoint(pt);
				continue;
			}
			else if ( s.IsKey("Y" ))
			{
				CPointMap pt = pItem->GetUnkPoint();
				pt.m_y = atoi(pArg);
				pItem->SetUnkPoint(pt);
				continue;
			}
			else if ( s.IsKey("Z" ))
			{
				CPointMap pt = pItem->GetUnkPoint();
				pt.m_z = atoi(pArg);
				pItem->SetUnkPoint(pt);
				continue;
			}
			else if ( s.IsKey("COLOR" ))
			{
				pItem->SetHue( atoi(pArg));
				continue;
			}
			else if ( s.IsKey("LAYER" ))
			{
				m_pCurSer->m_layer = (LAYER_TYPE) atoi(pArg);
				continue;
			}
			else if ( s.IsKey("AMOUNT" ))
			{
				pItem->SetAmount( atoi(pArg));
				continue;
			}
			else if ( s.IsKey("MOREX" ))
			{
				pItem->m_itNormal.m_morep.m_x = atoi(pArg);
				continue;
			}
			else if ( s.IsKey("MOREY" ))
			{
				pItem->m_itNormal.m_morep.m_y = atoi(pArg);
				continue;
			}
			else if ( s.IsKey("MOREZ" ))
			{
				pItem->m_itNormal.m_morep.m_z = atoi(pArg);
				continue;
			}
			else if ( s.IsKey("MORE" ))
			{
				pItem->m_itNormal.m_more1 = atoi(pArg);
				continue;
			}
			else if ( s.IsKey("MORE2" ))
			{
				pItem->m_itNormal.m_more2 = atoi(pArg);
				continue;
			}
			else if ( s.IsKey("DYEABLE" ))
			{
				// if ( atoi(pArg))
				//	pItem->m_pDef->m_Can |= CAN_I_DYE;
				continue;
			}
			else if ( s.IsKey("ATT" ))
			{
				// pItem->m_pDef->m_attackBase = atoi(pArg);
			}
			else if ( s.IsKey("TYPE" ))
			{
				// ??? translate the type field.
				int i = atoi(pArg);

			}
#ifdef COMMENT
			fprintf(wscfile, "DEF %i\n", items[i].def);
			fprintf(wscfile, "VISIBLE %i\n", items[i].visible);
			fprintf(wscfile, "SPAWN %i\n", (items[i].spawn1*16777216)+(items[i].spawn2*65536)+(items[i].spawn3*256)+items[i].spawn4);
			fprintf(wscfile, "CORPSE %i\n", items[i].corpse);
			fprintf(wscfile, "OWNER %i\n", (items[i].owner1*16777216)+(items[i].owner2*65536)+(items[i].owner3*256)+items[i].owner4);
#endif
		}
		if ( mode == IMPFLAGS_CHARS )
		{
			if ( s.IsKey("NAME" ))
			{
				if ( m_pCurObj != NULL )
					return( false );
				pChar = CChar::CreateBasic( CREID_MAN );
				ASSERT(pChar);
				pChar->SetName( sName );
				m_pCurObj = pChar;
				m_pCurSer->m_pObj = pChar;
				continue;
			}
			if ( m_pCurObj == NULL )
			{
				DEBUG_ERR(( "Import:Bad Item Key '%s'" LOG_CR, s.GetKey()));
				break;
			}
			if ( s.IsKey("X" ))
			{
				CPointMap pt = pChar->GetUnkPoint();
				pt.m_x = atoi(pArg);
				pChar->SetUnkPoint(pt);
				continue;
			}
			else if ( s.IsKey("Y" ))
			{
				CPointMap pt = pChar->GetUnkPoint();
				pt.m_y = atoi(pArg);
				pChar->SetUnkPoint(pt);
				continue;
			}
			else if ( s.IsKey("Z" ))
			{
				CPointMap pt = pChar->GetUnkPoint();
				pt.m_z = atoi(pArg);
				pChar->SetUnkPoint(pt);
				continue;
			}
			else if ( s.IsKey("BODY" ))
			{
				pChar->SetID( (CREID_TYPE) atoi(pArg));
				continue;
			}
			else if ( s.IsKey("SKIN" ))
			{
				pChar->SetHue( atoi(pArg));
				continue;
			}
			else if ( s.IsKey("DIR" ))
			{
				pChar->m_dirFace = (DIR_TYPE) atoi(pArg);
				continue;
			}
			else if ( s.IsKey("XBODY" ))
			{
				pChar->m_prev_id = (CREID_TYPE) atoi(pArg);
				continue;
			}
			else if ( s.IsKey("XSKIN" ))
			{
				pChar->m_prev_Hue = atoi(pArg);
				continue;
			}
			else if ( s.IsKey("FONT" ))
			{
				pChar->m_fonttype = (FONT_TYPE) atoi(pArg);
				continue;
			}
			else if ( s.IsKey("TITLE" ))
			{
				pChar->m_TagDefs.SetKeyStr( "TITLE", pArg );
				continue;
			}
			else if ( s.IsKey("KARMA" ))
			{
				pChar->Stat_Set(STAT_Karma,atoi(pArg));
				continue;
			}
			else if ( s.IsKey("FAME" ))
			{
				pChar->Stat_Set(STAT_Fame,atoi(pArg));
				continue;
			}
			else if ( s.IsKey("STRENGTH" ))
			{
				pChar->Stat_Set(STAT_Str,atoi(pArg));
			}
			else if ( s.IsKey("DEXTERITY" ))
			{
				pChar->Stat_Set(STAT_Dex,atoi(pArg));
			}
			else if ( s.IsKey("INTELLIGENCE" ))
			{
				pChar->Stat_Set(STAT_Int,atoi(pArg));
			}
			else if ( s.IsKey("HITPOINTS" ))
			{
				pChar->Stat_Set(STAT_Health, atoi(pArg));
			}
			else if ( s.IsKey("STAMINA" ))
			{
				pChar->Stat_Set(STAT_Stam, atoi(pArg));
			}
			else if ( s.IsKey( "MANA" ))
			{
				pChar->Stat_Set(STAT_Mana, atoi(pArg));
			}
			else if ( s.IsKeyHead( "SKILL", 5 ))
			{
				SKILL_TYPE skill = (SKILL_TYPE) atoi( &(s.GetKey()[5]));
				if ( pChar->IsSkillBase(skill))
				{
					pChar->Skill_SetBase( skill, atoi(pArg));
				}
			}
			else if ( s.IsKey("ACCOUNT" ))
			{
				// What if the account does not exist ?
				pChar->Player_SetAccount( pArg );
			}
			else if ( s.IsKey("KILLS" ) && pChar->m_pPlayer.IsValidNewObj() )
			{
				pChar->m_pPlayer->m_wMurders = atoi(pArg);
			}
			else if ( s.IsKey("NPCAITYPE" ))
			{
				// Convert to proper NPC type.
				int i = atoi( pArg );
				switch ( i )
				{
				case 0x01:	pChar->NPC_SetBrain( NPCBRAIN_HEALER ); break;
				case 0x02:	pChar->NPC_SetBrain( NPCBRAIN_MONSTER ); break;
				case 0x04:
				case 0x40:	pChar->NPC_SetBrain( NPCBRAIN_GUARD ); break;
				case 0x08:	pChar->NPC_SetBrain( NPCBRAIN_BANKER ); break;
				default:	pChar->NPC_SetBrain( pChar->GetCreatureType()); break;
				}
			}
#ifdef COMMENT
			"NPC"
			fprintf(wscfile, "SPEECH %i\n", chars[i].speech);

			fprintf(wscfile, "DEAD %i\n", chars[i].dead);
			fprintf(wscfile, "SPAWN %i\n", (chars[i].spawn1*16777216)+(chars[i].spawn2*65536)+(chars[i].spawn3*256)+chars[i].spawn4);
			fprintf(wscfile, "WAR %i\n", chars[i].war);
			fprintf(wscfile, "OWN %i\n", (chars[i].own1*16777216)+(chars[i].own2*65536)+(chars[i].own3*256)+chars[i].own4);

			fprintf(wscfile, "ALLMOVE %i\n", chars[i].priv2);
			fprintf(wscfile, "PACKITEM %i\n", chars[i].packitem);
			fprintf(wscfile, "RESERVED1 %i\n", chars[i].cell);
			fprintf(wscfile, "NPCWANDER %i\n", chars[i].npcWander);
			fprintf(wscfile, "FOLLOWTARGET %i\n", (chars[i].ftarg1*16777216)+(chars[i].ftarg2*65536)+(chars[i].ftarg3*256)+chars[i].ftarg4);
			fprintf(wscfile, "FX1 %i\n", chars[i].fx1);
			fprintf(wscfile, "FY1 %i\n", chars[i].fy1);
			fprintf(wscfile, "FZ1 %i\n", chars[i].fz1);
			fprintf(wscfile, "FX2 %i\n", chars[i].fx2);
			fprintf(wscfile, "FY2 %i\n", chars[i].fy2);
			fprintf(wscfile, "STRENGTH2 %i\n", chars[i].st2);
			fprintf(wscfile, "DEXTERITY2 %i\n", chars[i].dx2);
			fprintf(wscfile, "INTELLIGENCE2 %i\n", chars[i].in2);
			fprintf(wscfile, "ATT %i\n", chars[i].att);
			fprintf(wscfile, "DEF %i\n", chars[i].def);
			fprintf(wscfile, "DEATHS %i\n", chars[i].deaths);
			fprintf(wscfile, "ROBE %i\n", (chars[i].robe1*16777216)+(chars[i].robe2*65536)+(chars[i].robe3*256)+chars[i].robe4);
#endif
			continue;
		}
	}
	return( true );
}

HRESULT CWorld::Import( LPCTSTR pszFilename, const CChar* pSrc, WORD wModeFlags, int iDist, LPCSTR pszArg1, LPCSTR pszArg2 )
{
	// M_Import
	// wModeFlags = IMPFLAGS_TYPE
	//
	// iDistance = distance from the current point.

	// dx = change in x from file to world.
	// dy = change in y

	// NOTE: We need to set the IsLoading() for this ???

	// ??? What if i want to just import into the local area ?
	int iLen = strlen( pszFilename );
	if ( iLen <= 4 )
		return( HRES_BAD_ARGUMENTS );
	CScript s;
	if ( ! s.Open( pszFilename ))
		return( HRES_INVALID_HANDLE );

	CPointMap ptCenter;
	if ( pSrc )
	{
		ptCenter = pSrc->GetTopPoint();

		if ( wModeFlags & IMPFLAGS_RELATIVE )
		{
			// dx += ptCenter.m_x;
			// dy += ptCenter.m_y;
		}
	}

	CImportFile fImport( wModeFlags, ptCenter, iDist );

	fImport.m_pszArg1 = pszArg1;
	fImport.m_pszArg2 = pszArg2;

	if ( ! _stricmp( pszFilename + iLen - 4, ".WSC" ))
	{
		if ( ! fImport.ImportWSC(s, wModeFlags ))
			return( HRES_INVALID_HANDLE );
	}
	else
	{
		// This is one of our files. ".SCP"
		if ( ! fImport.ImportSCP(s, wModeFlags ))
			return( HRES_INVALID_HANDLE );
	}

	// now fix the contained items.
	fImport.ImportFix();

	GarbageCollection();
	return( 0 );
}

HRESULT CWorld::Export( LPCTSTR pszFilename, const CChar* pSrc, WORD wModeFlags, int iDist, int dx, int dy )
{
	// M_Export
	// wModeFlags = IMPFLAGS_TYPE
	// Just get the items in the local area to export.
	// dx = change in x from world to file.
	// dy = change in y

	if ( pSrc == NULL )
		return( HRES_PRIVILEGE_NOT_HELD );

	int iLen = strlen( pszFilename );
	if ( iLen <= 4 )
		return( HRES_BAD_ARGUMENTS );

	CScript s;
	if ( ! s.Open( pszFilename, OF_WRITE|OF_TEXT ))
		return( HRES_INVALID_HANDLE );

	if ( wModeFlags & IMPFLAGS_RELATIVE )
	{
		dx -= pSrc->GetTopPoint().m_x;
		dy -= pSrc->GetTopPoint().m_y;
	}

	int index = 0;
	if ( ! _stricmp( pszFilename + iLen - 4, ".WSC" ))
	{
		// Export as UOX format. for world forge stuff.
		CWorldSearch AreaItems( pSrc->GetTopPoint(), iDist );
		for(;;)
		{
			CItemPtr pItem = AreaItems.GetNextItem();
			if ( pItem == NULL )
				break;
			pItem->WriteUOX( s, index++ );
		}
		return( 0 );
	}

	// (???NPC) Chars and the stuff they are carrying.
	if ( wModeFlags & IMPFLAGS_CHARS )
	{
		CWorldSearch AreaChars( pSrc->GetTopPoint(), iDist );
		AreaChars.SetFilterAllShow( pSrc->IsPrivFlag( PRIV_ALLSHOW ));	// show logged out chars?
		for(;;)
		{
			CCharPtr pChar = AreaChars.GetNextChar();
			if ( pChar == NULL )
				break;
			pChar->s_WriteSafe( s );
		}
	}

	if ( wModeFlags & IMPFLAGS_ITEMS )
	{
		// Items on the ground.
		CWorldSearch AreaItems( pSrc->GetTopPoint(), iDist );
		AreaItems.SetFilterAllShow( pSrc->IsPrivFlag( PRIV_ALLSHOW ));	// show logged out chars?
		for(;;)
		{
			CItemPtr pItem = AreaItems.GetNextItem();
			if ( pItem == NULL )
				break;
			pItem->s_WriteSafe( s );
		}
	}

	return( 0 );
}


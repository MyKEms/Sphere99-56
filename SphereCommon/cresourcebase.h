//
// cresourcebase.h
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)

#ifndef _INC_CRESOURCEBASE_H
#define _INC_CRESOURCEBASE_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "cSphereExp.h"

// Desguise an id as a pointer.
#ifndef MAKEINTRESOURCE
#define MAKEINTRESOURCE(id) ((LPCTSTR)((DWORD)((WORD)(id))))
#endif
#define ISINTRESOURCE(p)	(!(((DWORD)p)&0xFFFFF000))
#define GETINTRESOURCE(p)	(((DWORD)p)&0x0FFF)

struct CResourceQty
{
	// This can be used to "weight" any resource.
public:
	void SetResourceID( UID_INDEX rid, int iQty )
	{
		m_rid = rid;
		m_iQty = iQty;
	}
	CSphereUID GetResourceID() const
	{
		return( m_rid );
	}
	RES_TYPE GetResType() const
	{
		return( m_rid.GetResType());
	}
	int GetResIndex() const
	{
		return( m_rid.GetResIndex());
	}
	int GetResQty() const
	{
		return( m_iQty );
	}
	void SetResQty( int wQty )
	{
		m_iQty = wQty;
	}

	bool LoadResQty( LPCTSTR& pszCmds );
	int WriteKey( TCHAR* pszArgs ) const;
	int WriteNameSingle( TCHAR* pszArgs ) const;

private:
	CSphereUID m_rid;	// A RES_Skill, RES_ItemDef, or RES_TypeDef
	int m_iQty;		// How much of this ?
};

class CResourceQtyArray : public CGTypedArray<CResourceQty, CResourceQty&>
{
	// Similar to CUIDRefArray except it has m_iQty attached.
	// Define a list of index id's (not references) to resource objects.
	// (Not owned by the list)
public:
	CResourceQtyArray()
	{
	}
	CResourceQtyArray( LPCTSTR pszCmds )
	{
		s_LoadKeys( pszCmds );
	}

	bool operator == ( const CResourceQtyArray& array ) const;

	int  s_LoadKeys( LPCTSTR pszCmds );
	// HRESULT s_MethodObjs( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc );

	void v_GetKeys( CGVariant& vVal ) const;
	void v_GetNames( CGVariant& vVal ) const;

	int FindResourceID( CSphereUID rid ) const;
	int FindResourceType( RES_TYPE type ) const;

	int FindResourceMatch( CObjBase* pObj ) const;
	bool IsResourceMatchAll( CObjBase* pObj ) const;
};

class CResourceDef : public CResourceObj
{
private:
	CSphereUID m_rid;		// the true resource id. (must be unique for the RES_TYPE)
	CGString m_sName;		// the DEFNAME name

public:
	CResourceDef(CSphereUID rid);
	CResourceDef(CSphereUID rid, LPCTSTR pszName) : CResourceObj(rid.GetHashCode()), m_rid(rid) { SetResourceName(pszName); }
	LPCTSTR GetResourceName() const { return m_sName; }
	void SetResourceName(LPCTSTR pszName) { if (pszName) m_sName = pszName; }
	void SetResourceVar(const void* pVarNum) { /* STUB */ }

	virtual CGString GetName() const { return m_sName; } // default to same as the DEFNAME name.
	virtual bool s_LoadProps(CScript& s)
	{
		while (s.ReadKeyParse())
		{
			s_PropSet(s.GetKey(), s.GetArgVar());
		}
		return true;
	}
	virtual HRESULT s_PropSet(LPCTSTR pszKey, const CGVariant& vVal) { return HRES_UNKNOWN_PROPERTY; }
	virtual HRESULT s_PropGet(LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc) { return HRES_UNKNOWN_PROPERTY; }
	virtual HRESULT s_Method(LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc) { return HRES_UNKNOWN_PROPERTY; }

	CSphereUID GetUIDIndex() const
	{
		return(m_rid);
	}
	CSphereUID GetHashCode() const
	{
		return m_rid;
	}

	CSphereUID GetResourceID() const
	{
		return(m_rid);
	}

	// unlink all this data. (tho don't delete the def as the pointer might still be used !)
	virtual void UnLink()
	{
		// This does nothing in the CResourceDef case, Only in the CResourceLink case.
	}

	bool IsValidHeap() const;
};
typedef CRefPtr<CResourceDef> CResourceDefPtr;

#define XTRIG_UNKNOWN 0	// bit 0 is reserved to say there are triggers here that do not conform.

class CResourceScript;
class CResourceLink : public CResourceDef
{
private:
	CResourceScript* m_pScript;
	CScriptLineContext m_LineContext;

public:
	CResourceLink(CSphereUID rid);

	CResourceScript* GetLinkFile() const { return m_pScript; }
	CScriptLineContext GetLinkContext() const { return m_LineContext; }
	void CopyLink( const CResourceLink* pLink )
	{
		if (pLink)
		{
			m_pScript = pLink->m_pScript;
			m_LineContext = pLink->m_LineContext;
		}
	}
	void SetLinkSection(CResourceScript* pScript)
	{
		m_pScript = pScript;
	}

	virtual HRESULT s_PropSet(LPCTSTR pszKey, const CGVariant& vVal) { return HRES_UNKNOWN_PROPERTY; }
	virtual HRESULT s_PropGet(LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc) { return HRES_UNKNOWN_PROPERTY; }
	virtual HRESULT s_Method(LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc) { return HRES_UNKNOWN_PROPERTY; }
};
typedef CRefPtr<CResourceLink> CResourceLinkPtr;

class CResourceTriggered : public CResourceLink
{
public:
	CResourceTriggered(CSphereUID rid);

	// Execute a trigger script from this resource.
	// Declaration only -- implementation after CResourceLock is defined.
	TRIGRET_TYPE OnTriggerScript(CScriptExecContext& context, int iNum, LPCTSTR pszName);

	bool HasTrigger(int trig) const { return true; } // STUB - assume all triggers exist
};
typedef CRefPtr<CResourceTriggered> CResourceTrigPtr;

class CResourceNamed : public CResourceLink
{
public:
	CResourceNamed(CSphereUID rid, LPCTSTR pszName) : CResourceLink(rid) { SetResourceName(pszName); }
	CGString GetName() const { return GetResourceName(); }
};

class CUIDRefArray
{
	// List of Players and NPC's involved in the quest/party/account etc..

public:
	static const char* m_sClassName;

	CUIDRefArray() { };

private:
	CGTypedArray<CSphereUID, CSphereUID> m_uidCharArray;

public:
	size_t FindObj(const CObjBase* pChar) const;
	size_t FindObj(const CResourceObj* pObj) const
	{
		if ( pObj == NULL ) return BadIndex();
		CSphereUID uid = pObj->GetUIDIndex();
		for ( size_t i = 0; i < GetCharCount(); i++ )
		{
			if ( GetChar(i) == uid ) return i;
		}
		return BadIndex();
	}
	size_t AttachObj(const CObjBase* pChar);
	size_t AttachObj(const CResourceObj* pObj)
	{
		if ( pObj == NULL ) return BadIndex();
		return AttachUID( pObj->GetUIDIndex() );
	}
	size_t InsertObj(const CObjBase* pChar, size_t i);
	void DetachObj(size_t i);
	size_t DetachObj(const CObjBase* pChar);
	size_t DetachObj(const CResourceObj* pObj)
	{
		size_t i = FindObj(pObj);
		if ( i != BadIndex() ) DetachObj(i);
		return i;
	}
	size_t GetSize() const { return m_uidCharArray.GetSize(); }

	CSphereUID GetAt(size_t i) const
	{
		return m_uidCharArray[i];
	}

	int AttachUID(const CSphereUID uid)
	{
		// Don't add duplicates.
		for ( size_t i = 0; i < GetCharCount(); i++ )
		{
			if ( GetChar(i) == uid ) return (int)i;
		}
		return (int) m_uidCharArray.Add( uid );
	}
	void RemoveAll()
	{
		m_uidCharArray.RemoveAll();
	}
	void RemoveAt(size_t i)
	{
		m_uidCharArray.RemoveAt(i);
	}
	void CopyArray(const CUIDRefArray& arr)
	{
		RemoveAll();
		for ( size_t i = 0; i < arr.GetCharCount(); i++ )
		{
			m_uidCharArray.Add( arr.GetChar(i) );
		}
	}

	bool IsUIDIn(const CSphereUID uid)
	{
		for ( size_t i = 0; i < GetCharCount(); i++ )
		{
			if ( GetChar(i) == uid ) return true;
		}
		return false;
	}

	void s_WriteObjs(CScript& s, LPCTSTR pszKey)
	{
		for ( size_t i = 0; i < GetCharCount(); i++ )
		{
			s.WriteKeyInt( pszKey, (int)(DWORD)GetChar(i) );
		}
	}
	HRESULT s_MethodObjs(CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc)
	{
		// Stub for now - not critical for loading.
		return HRES_INVALID_FUNCTION;
	}

	CSphereUID GetChar(size_t i) const
	{
		return m_uidCharArray[i];
	}
	size_t GetCharCount() const
	{
		return m_uidCharArray.GetCount();
	}

	bool IsObjIn(const CObjBase* pChar) const
	{
		return (FindObj(pChar) != m_uidCharArray.BadIndex());
	}

	bool IsValidIndex(size_t i) const
	{
		return m_uidCharArray.IsValidIndex(i);
	}
	inline size_t BadIndex() const
	{
		return m_uidCharArray.BadIndex();
	}

private:
	CUIDRefArray(const CUIDRefArray& copy);
	CUIDRefArray& operator=(const CUIDRefArray& other);
};


//***********************************************************

class CResourceRefArray : public CGRefArray<CResourceLink>
{
	// Define a list of pointer references to CResourceLink. (Not owned by the list)
	// An indexed list of CResourceLink s.
public:
	~CResourceRefArray()
	{
		RemoveAll();
	}

	int FindResourceType( RES_TYPE type ) const;
	int FindResourceID( CSphereUID rid ) const;
	int FindResourceName( RES_TYPE restype, LPCTSTR pszKey ) const;

	void v_Get( CGVariant& vVal ) const;
	bool v_Set( CGVariant& vVal, RES_TYPE restype );

	// HRESULT s_MethodObjs( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc );
	void s_WriteProps( CScript& s, LPCTSTR pszKey ) const;

private:
	CString GetResourceName( int iIndex ) const
	{
		// look up the name of the fragment given it's index.
		CResourceLink* pResourceLink = this->GetAt( iIndex );
		if (!pResourceLink) return CString();
		return( pResourceLink->GetResourceName());
	}

public:
	CSCRIPT_CLASS_DEF1();
	enum M_TYPE_
	{
#define CUIDREFARRAYMETHOD(a,b,c,d) M_##a,
#include "cuidrefarraymethods.tbl"
#undef CUIDREFARRAYMETHOD
		M_QTY,
	};
	static const CScriptMethod sm_Methods[M_QTY+1];
#ifdef USE_JSCRIPT
#define CUIDREFARRAYMETHOD(a,b,c,d) JSCRIPT_METHOD_DEF(a)
#include "cuidrefarraymethods.tbl"
#undef CUIDREFARRAYMETHOD
#endif
};

class CResourceScript : public CScript, public CMemDynamic
{
public:
	virtual void OnTick(bool fNow) { /* no-op for now */ }

	// A script file containing resource, speech, motives or events handlers.
	// NOTE: we should check periodically if this file has been altered externally ?
protected:
	DECLARE_MEM_DYNAMIC;
};
typedef CRefPtr<CResourceScript> CResourceScriptPtr;

class CResourceFile : public CResourceScript
{
public:
	void OnTick(bool fNow) { /* no-op for now */ }
};
typedef CRefPtr<CResourceFile> CResourceFilePtr;

class CResourceLock : public CScript
{
private:
	CResourceLink* m_pLink;	// The resource link we opened from.

public:
	// Construct from a CResourceLink (or CResourceDef -- dynamic_cast down).
	// Opens the underlying script file and seeks to the saved section offset.
	CResourceLock(CScriptObj* pObj)
		: m_pLink(NULL)
	{
		// Try to get a CResourceLink from the object.
		CResourceLink* pLink = dynamic_cast<CResourceLink*>(pObj);
		if ( !pLink )
			return;
		m_pLink = pLink;
		CResourceScript* pScript = pLink->GetLinkFile();
		if ( !pScript )
			return;
		// Open the same file that the resource link points to.
		if ( !pScript->IsFileOpen() )
			return;
		// Copy the file path and open it.
		SetFilePath(pScript->GetFilePath());
		if ( !Open(pScript->GetFilePath(), OF_READ) )
			return;
		// Seek to the section start.
		CScriptLineContext ctx = pLink->GetLinkContext();
		SeekContext(ctx);
	}

	// Check if the current line is a trigger header (starts with "ON").
	bool IsLineTrigger()
	{
		LPCTSTR pszKey = GetKey();
		if ( !pszKey )
			return false;
		if ( _strnicmp(pszKey, "ON", 2) != 0 )
			return false;
		// Make sure it is "ON" and not a property like "ONCOUNT"
		char ch = pszKey[2];
		return (ch == '\0' || ch == '=' || ISWHITESPACE(ch));
	}

	// Re-parse the current line to separate key and args.
	void ParseKeyLate()
	{
		// Just re-trigger the key parse on the current line buffer.
		// The ReadKeyParse already splits key/arg, but some callers
		// want to re-parse after modifying the line.
		ReadKeyParse();
	}

	// Scan forward looking for a trigger by name.
	// ON=<trigName>
	bool FindTriggerName(LPCTSTR pszName)
	{
		if ( !pszName || !*pszName )
			return false;
		while ( ReadKeyParse() )
		{
			if ( !IsLineTrigger() )
				continue;
			// The arg should be the trigger name.
			LPCTSTR pszArg = GetArgRaw();
			if ( pszArg && !_stricmp(pszArg, pszName) )
				return true;
		}
		return false;
	}

	// Scan forward looking for a trigger by number.
	// Trigger numbers map to sm_Triggers table indices.
	bool FindTriggerNumber(DWORD dwNumber)
	{
		// We iterate and check the arg as a number.
		while ( ReadKeyParse() )
		{
			if ( !IsLineTrigger() )
				continue;
			LPCTSTR pszArg = GetArgRaw();
			if ( pszArg )
			{
				// The arg might be "@TriggerName" or a number.
				// For now just try number comparison.
				int iNum = atoi(pszArg);
				if ( (DWORD) iNum == dwNumber )
					return true;
			}
		}
		return false;
	}
};

// Implementation of CResourceTriggered::OnTriggerScript.
// Placed after CResourceLock is fully defined.
inline TRIGRET_TYPE CResourceTriggered::OnTriggerScript(CScriptExecContext& context, int iNum, LPCTSTR pszName)
{
	// Open the resource section for reading.
	CResourceLock s(this);
	if ( !s.IsFileOpen() )
		return TRIGRET_RET_DEFAULT;

	// Try to find the trigger in this section.
	// First try by name if we have one.
	bool fFound = false;
	if ( pszName && *pszName )
	{
		fFound = s.FindTriggerName(pszName);
	}

	if ( !fFound )
	{
		// Rewind and try with "@" prefix stripped
		// (triggers stored as "Create" not "@Create" in scp files).
		CScriptLineContext ctx = GetLinkContext();
		s.SeekContext(ctx);
		if ( pszName && *pszName == '@' )
		{
			fFound = s.FindTriggerName(pszName + 1);
		}
	}

	if ( !fFound )
		return TRIGRET_RET_DEFAULT;

	// Execute the trigger's script block.
	return context.ExecuteScript(s, TRIGRUN_SECTION_TRUE);
}

#endif // _INC_CRESOURCEBASE_H
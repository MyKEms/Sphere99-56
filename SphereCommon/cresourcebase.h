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
	virtual HRESULT s_Method(LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc) { throw "not implemented"; } // Execute command from script

	CSphereUID GetUIDIndex() const
	{
		return(m_rid);
	}
	CSphereUID GetHashCode() const
	{
		return m_rid;
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
	virtual HRESULT s_Method(LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc) { throw "not implemented"; } // Execute command from script
};
typedef CRefPtr<CResourceLink> CResourceLinkPtr;

class CResourceTriggered : public CResourceLink
{
public:
	CResourceTriggered(CSphereUID rid);

	TRIGRET_TYPE OnTriggerScript(CScriptExecContext& context, int iNum, LPCTSTR pszName) { throw "not implemented"; }
	bool HasTrigger(int trig) const { return true; } // STUB
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
	size_t FindObj(const CResourceObj* pObj) const { throw "not implemented"; }
	size_t AttachObj(const CObjBase* pChar);
	size_t AttachObj(const CResourceObj* pObj) { throw "not implemented"; }
	size_t InsertObj(const CObjBase* pChar, size_t i);
	void DetachObj(size_t i);
	size_t DetachObj(const CObjBase* pChar);
	size_t DetachObj(const CResourceObj* pObj) { throw "not implemented"; }
	size_t GetSize() const { return m_uidCharArray.GetSize(); }

	CSphereUID GetAt(size_t i) const { throw "not implemented"; }

	int AttachUID(const CSphereUID uid) { throw "not implemented"; }
	void RemoveAll() { throw "not implemented"; }
	void RemoveAt(size_t i) { throw "not implemented"; }
	void CopyArray(const CUIDRefArray& arr) { throw "not implemented"; }

	bool IsUIDIn(const CSphereUID uid) { throw "not implemented"; }

	void s_WriteObjs(CScript& s, LPCTSTR pszKey) { throw "not implemented"; }
	HRESULT s_MethodObjs(CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc) { throw "not implemented"; }

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
public:
	CResourceLock(CScriptObj* pObj) { throw "not implemented"; }

	bool IsLineTrigger() { throw "not implemented"; }
	void ParseKeyLate() { throw "not implemented"; }
	bool FindTriggerName(LPCTSTR pszName) { throw "not implemented"; }
	bool FindTriggerNumber(DWORD dwNumber) { throw "not implemented"; }
};

#endif // _INC_CRESOURCEBASE_H
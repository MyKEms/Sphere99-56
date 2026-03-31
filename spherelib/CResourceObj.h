#ifndef _INC_CRESOURCEOBJ_H
#define _INC_CRESOURCEOBJ_H
#include "CExpression.h"
#include "CScriptConsole.h"

class CScript;
class CResourceObj : public CScriptObj
{
private:
	HASH_INDEX m_dwHashIndex;

public:
	CResourceObj(HASH_INDEX dwHashIndex)
	{
		m_dwHashIndex = dwHashIndex;
	}

	virtual HRESULT s_Method(LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc) { throw "not implemented"; }
	virtual bool s_LoadProps(CScript& s) { throw "not implemented"; } // Load an item from script
	virtual HRESULT s_PropGet(LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc) { throw "not implemented"; }
	virtual HRESULT s_PropSet(const char* pszKey, CGVariant& vVal) { throw "not implemented"; }

	int GetRefCount() { throw "not implemented"; }
	void IncRefCount() { /* stub */ }
	void StaticDestruct() { /* stub */ }
	HASH_INDEX GetUIDIndex() const { return m_dwHashIndex; }
	HASH_INDEX GetHashCode() const { return m_dwHashIndex; }
	bool IsValidUID() const { throw "not implemented"; }
};
typedef CRefPtr<CResourceObj> CResourceObjPtr;

struct CUIDArray
{
	CGRefArray<CResourceObj> m_UIDs;	// all the UID's in the World. CChar and CItem.

	CUIDArray() {}
	CUIDArray(DWORD dwMaxSize) { /* stub - set initial capacity */ }

	DWORD GetUIDCount() const
	{
		return(m_UIDs.GetCount());
	}
#define UID_PLACE_HOLDER (CResourceObj*)0xFFFFFFFF
	CResourceObj* FindUIDObj(DWORD dwIndex) const
	{
		if (!dwIndex || dwIndex >= GetUIDCount())
			return(NULL);
		if (m_UIDs[dwIndex] == UID_PLACE_HOLDER)	// unusable for now. (background save is going on)
			return(NULL);
		return(m_UIDs[dwIndex]);
	}
	void FreeUID(CResourceObj* pObj)
	{
		// Can't free up the UID til after the save !
		m_UIDs.SetAt(pObj->GetUIDIndex(), UID_PLACE_HOLDER);
	}
	DWORD AllocUID(CResourceObj* pObj, DWORD dwIndex) { throw "not implemented"; }
	void DeleteAllUIDs() { m_UIDs.RemoveAll(); }
};
#endif // _INC_CRESOURCEOBJ_H

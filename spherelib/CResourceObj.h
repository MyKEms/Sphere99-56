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

	virtual HRESULT s_Method(LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc) { return HRES_UNKNOWN_PROPERTY; }
	virtual bool s_LoadProps(CScript& s) { return false; } // Load an item from script
	virtual HRESULT s_PropGet(LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc) { return HRES_UNKNOWN_PROPERTY; }
	virtual HRESULT s_PropSet(const char* pszKey, CGVariant& vVal) { return HRES_UNKNOWN_PROPERTY; }

	int GetRefCount() { return 1; /* stub - always at least 1 */ }
	void IncRefCount() { /* stub */ }
	void StaticDestruct() { /* stub */ }
	HASH_INDEX GetUIDIndex() const { return m_dwHashIndex; }
	HASH_INDEX GetHashCode() const { return m_dwHashIndex; }
	bool IsValidUID() const { return m_dwHashIndex != 0; }
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
	DWORD AllocUID(CResourceObj* pObj, DWORD dwIndex)
	{
		// Allocate a UID slot for a game object.
		// dwIndex = desired UID index (0 = allocate new)
		// RETURN: the UID index actually assigned.
		ASSERT(pObj);
		if ( dwIndex > 0 )
		{
			// Requested a specific UID slot.
			if ( dwIndex >= GetUIDCount())
			{
				// Grow the array to accommodate.
				m_UIDs.SetAtGrow( dwIndex, pObj );
			}
			else
			{
				CResourceObj* pObjPrv = FindUIDObj(dwIndex);
				if ( pObjPrv && pObjPrv != UID_PLACE_HOLDER && pObjPrv != pObj )
				{
					// UID collision - assign a new one instead.
					dwIndex = 0;
				}
				else
				{
					m_UIDs.SetAt( dwIndex, pObj );
				}
			}
		}
		if ( dwIndex <= 0 )
		{
			// Find a free slot.
			DWORD dwCount = GetUIDCount();
			for ( dwIndex = 1; dwIndex < dwCount; dwIndex++ )
			{
				if ( m_UIDs[dwIndex] == NULL || m_UIDs[dwIndex] == UID_PLACE_HOLDER )
				{
					m_UIDs.SetAt( dwIndex, pObj );
					return dwIndex;
				}
			}
			// No free slot found - grow the array.
			dwIndex = dwCount;
			m_UIDs.SetAtGrow( dwIndex, pObj );
		}
		return dwIndex;
	}
	void DeleteAllUIDs() { m_UIDs.RemoveAll(); }
};
#endif // _INC_CRESOURCEOBJ_H

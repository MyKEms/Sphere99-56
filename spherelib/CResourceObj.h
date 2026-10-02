#ifndef _INC_CRESOURCEOBJ_H
#define _INC_CRESOURCEOBJ_H
#include "CExpression.h"
#include "CScriptConsole.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <vector>
#pragma pop_macro("max")
#pragma pop_macro("min")

class CScript;
class CResourceObj : public CScriptObj
{
private:
	HASH_INDEX m_dwHashIndex;
	DWORD m_dwUIDGeneration;

public:
	CResourceObj(HASH_INDEX dwHashIndex)
	{
		m_dwHashIndex = dwHashIndex;
		m_dwUIDGeneration = 0;
	}

	virtual HRESULT s_Method(LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc) { return HRES_UNKNOWN_PROPERTY; }
	virtual bool s_LoadProps(CScript& s); // Implemented in spherelib/stubs.cpp
	virtual HRESULT s_PropGet(LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc) { return HRES_UNKNOWN_PROPERTY; }
	virtual HRESULT s_PropSet(const char* pszKey, CGVariant& vVal) { return HRES_UNKNOWN_PROPERTY; }

	int GetRefCount() const { return 1; /* stub - always at least 1 */ }
	void IncRefCount() { /* stub */ }
	void StaticDestruct() { /* stub */ }
	HASH_INDEX GetUIDIndex() const { return m_dwHashIndex; }
	HASH_INDEX GetHashCode() const { return m_dwHashIndex; }
	void SetUIDIndex(HASH_INDEX uid) { m_dwHashIndex = uid; }
	DWORD GetUIDGeneration() const override { return m_dwUIDGeneration; }
	LPCTSTR GetUIDTypeName() const override { return "resource"; }
	void SetUIDGeneration(DWORD generation) { m_dwUIDGeneration = generation; }
	bool IsValidUID() const { return m_dwHashIndex != 0; }
};
typedef CRefPtr<CResourceObj> CResourceObjPtr;

struct CUIDArray
{
	struct UID_QUARANTINE_ENTRY
	{
		DWORD m_dwIndex;
		std::chrono::steady_clock::time_point m_timeFreed;
		unsigned long long m_iReleaseSaveEpoch;
	};

	CGRefArray<CResourceObj> m_UIDs;	// all the UID's in the World. CChar and CItem.
	std::vector<DWORD> m_UIDGenerations;
	std::unordered_map<DWORD, UID_QUARANTINE_ENTRY> m_UIDQuarantine;
	std::chrono::milliseconds m_timeUIDReuseDelay;
	unsigned long long m_iUIDSaveEpoch;
	bool m_fPreventUIDReuse;

	void EnsureGenerationCount()
	{
		if ( m_UIDGenerations.size() < static_cast<size_t>(GetUIDCount()) )
			m_UIDGenerations.resize( GetUIDCount(), 0 );
	}

	void PruneUIDQuarantine()
	{
		const std::chrono::steady_clock::time_point timeNow = std::chrono::steady_clock::now();
		for ( std::unordered_map<DWORD, UID_QUARANTINE_ENTRY>::iterator it = m_UIDQuarantine.begin();
			it != m_UIDQuarantine.end(); )
		{
			const UID_QUARANTINE_ENTRY& entry = it->second;
			const bool fAgeElapsed = timeNow - entry.m_timeFreed >= m_timeUIDReuseDelay;
			const bool fSaveCompleted = m_iUIDSaveEpoch >= entry.m_iReleaseSaveEpoch;
			if ( fAgeElapsed && fSaveCompleted )
			{
				it = m_UIDQuarantine.erase( it );
			}
			else
				++it;
		}
	}

	bool IsUIDQuarantined( DWORD dwIndex ) const
	{
		const std::chrono::steady_clock::time_point timeNow = std::chrono::steady_clock::now();
		std::unordered_map<DWORD, UID_QUARANTINE_ENTRY>::const_iterator it =
			m_UIDQuarantine.find( dwIndex );
		if ( it == m_UIDQuarantine.end() || m_fPreventUIDReuse )
			return it != m_UIDQuarantine.end();
		const UID_QUARANTINE_ENTRY& entry = it->second;
		const bool fAgeElapsed = timeNow - entry.m_timeFreed >= m_timeUIDReuseDelay;
		const bool fSaveCompleted = m_iUIDSaveEpoch >= entry.m_iReleaseSaveEpoch;
		return !fAgeElapsed || !fSaveCompleted;
	}

	DWORD AssignUID( CResourceObj* pObj, DWORD dwIndex )
	{
		EnsureGenerationCount();
		DWORD& dwGeneration = m_UIDGenerations[dwIndex];
		if ( dwGeneration == 0 )
			dwGeneration = 1;
		else
			dwGeneration++;
		m_UIDs.SetAt( dwIndex, pObj );
		m_UIDQuarantine.erase( dwIndex );
		pObj->SetUIDGeneration( dwGeneration );
		return dwIndex;
	}

	CUIDArray()
		: m_timeUIDReuseDelay( std::chrono::seconds( 60 )),
		  m_iUIDSaveEpoch( 0 ), m_fPreventUIDReuse( false )
	{
		// UID 0 is reserved as the invalid/clear value.  Keep a real slot at
		// index 0 so the first runtime object is allocated UID 1, not UID 0.
		m_UIDs.SetAtGrow(0, NULL);
		m_UIDGenerations.resize( 1, 0 );
	}
	CUIDArray(DWORD dwMaxSize)
		: m_timeUIDReuseDelay( std::chrono::seconds( 60 )),
		  m_iUIDSaveEpoch( 0 ), m_fPreventUIDReuse( false )
	{
		// Preserve the same invariant for callers that provide an initial size.
		m_UIDs.SetCount(dwMaxSize > 0 ? dwMaxSize : 1);
		m_UIDGenerations.resize( GetUIDCount(), 0 );
	}

	DWORD GetUIDCount() const
	{
		return(m_UIDs.GetCount());
	}
#define UID_PLACE_HOLDER (CResourceObj*)0xFFFFFFFF
	CResourceObj* FindUIDObj(DWORD dwIndex) const
	{
		dwIndex &= 0x3FFFFFFF; // strip type flags
		if (!dwIndex || dwIndex >= GetUIDCount())
			return(NULL);
		if (m_UIDs[dwIndex] == UID_PLACE_HOLDER)	// unusable for now. (background save is going on)
			return(NULL);
		return(m_UIDs[dwIndex]);
	}
	void FreeUID(CResourceObj* pObj)
	{
		// Can't free up the UID til after the save !  A deferred object can
		// share a slot with its replacement before its destructor runs; only
		// release the slot if it still belongs to this object.
		DWORD dwIndex = pObj->GetUIDIndex() & 0x3FFFFFFF; // strip type flags
		if (dwIndex > 0 && dwIndex < GetUIDCount() && m_UIDs[dwIndex] == pObj)
		{
			m_UIDs.SetAt(dwIndex, UID_PLACE_HOLDER);
			m_UIDQuarantine[dwIndex] = { dwIndex, std::chrono::steady_clock::now(), m_iUIDSaveEpoch + 1 };
		}
	}
	void SetPreventUIDReuse() { m_fPreventUIDReuse = true; }
	void SetAllowUIDReuse()
	{
		m_fPreventUIDReuse = false;
		++m_iUIDSaveEpoch;
		PruneUIDQuarantine();
	}
	void SetUIDReuseDelaySeconds( DWORD dwSeconds )
	{
		m_timeUIDReuseDelay = std::chrono::seconds( dwSeconds );
	}
	DWORD GetUIDGeneration( DWORD dwIndex ) const
	{
		dwIndex &= 0x3FFFFFFF;
		return dwIndex < m_UIDGenerations.size() ? m_UIDGenerations[dwIndex] : 0;
	}
	DWORD AllocUID(CResourceObj* pObj, DWORD dwIndex)
	{
		// Allocate a UID slot for a game object.
		// dwIndex = desired UID index (0 = allocate new), may include UID_F_ITEM flag
		// RETURN: the UID index actually assigned.
		ASSERT(pObj);
		EnsureGenerationCount();
		// Strip type flags — only use the index portion for array storage.
		// UID_INDEX_MASK = 0x3FFFFFFF (lose upper 2 bits: UID_F_ITEM and RID_F_RESOURCE)
		dwIndex &= 0x3FFFFFFF;
		if ( dwIndex > 0 )
		{
			// Requested a specific UID slot.
			if ( dwIndex >= GetUIDCount())
			{
				// Grow the array to accommodate.
				m_UIDs.SetAtGrow( dwIndex, NULL );
				EnsureGenerationCount();
				AssignUID( pObj, dwIndex );
			}
			else
			{
				CResourceObj* pObjPrv = FindUIDObj(dwIndex);
				if ( pObjPrv && pObjPrv != UID_PLACE_HOLDER && pObjPrv != pObj )
				{
					// UID collision - assign a new one instead.
					dwIndex = 0;
				}
				else if ( m_UIDs[dwIndex] == UID_PLACE_HOLDER && IsUIDQuarantined( dwIndex ))
				{
					// An explicit request cannot bypass the same quarantine as an
					// automatically allocated slot.
					dwIndex = 0;
				}
				else
				{
					AssignUID( pObj, dwIndex );
				}
			}
		}
		if ( dwIndex <= 0 )
		{
			// Find a free slot.
			DWORD dwCount = GetUIDCount();
			for ( dwIndex = 1; dwIndex < dwCount; dwIndex++ )
			{
				if ( (m_UIDs[dwIndex] == NULL || m_UIDs[dwIndex] == UID_PLACE_HOLDER) &&
					(m_UIDs[dwIndex] == NULL || !IsUIDQuarantined( dwIndex )) )
				{
					return AssignUID( pObj, dwIndex );
				}
			}
			// No free slot found - grow the array.
			dwIndex = dwCount;
			while ( IsUIDQuarantined( dwIndex ))
				++dwIndex;
			m_UIDs.SetAtGrow( dwIndex, NULL );
			EnsureGenerationCount();
			AssignUID( pObj, dwIndex );
		}
		return dwIndex;
	}
	void DeleteAllUIDs()
	{
		m_UIDs.RemoveAll();
		// Keep UID 0 reserved after a reset just as the constructors do.
		m_UIDs.SetAtGrow(0, NULL);
		m_UIDGenerations.clear();
		m_UIDGenerations.resize( 1, 0 );
		m_UIDQuarantine.clear();
		m_iUIDSaveEpoch = 0;
		m_fPreventUIDReuse = false;
	}
};
#endif // _INC_CRESOURCEOBJ_H

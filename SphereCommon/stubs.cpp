// SphereCommon/stubs.cpp - Stub implementations for SphereCommon layer
// Provides minimal implementations to satisfy linker

#include "stdafx.h"
#include "spherecommon.h"
#include "cresourcebase.h"

// CResourceDef
CResourceDef::CResourceDef(CSphereUID rid)
	: CResourceObj(rid.GetHashCode()), m_rid(rid)
{
}

bool CResourceDef::IsValidHeap() const
{
	return true;
}

// CResourceLink
CResourceLink::CResourceLink(CSphereUID rid)
	: CResourceDef(rid), m_pScript(NULL)
{
}

// CResourceTriggered
CResourceTriggered::CResourceTriggered(CSphereUID rid)
	: CResourceLink(rid)
{
}

// CUIDRefArray
size_t CUIDRefArray::FindObj(const CObjBase* pChar) const
{
	if (!pChar)
		return m_uidCharArray.BadIndex();

	CSphereUID uid = ((const CResourceObj*)pChar)->GetUIDIndex();
	for (size_t i = 0; i < m_uidCharArray.GetCount(); i++)
	{
		if (m_uidCharArray[i] == uid)
			return i;
	}
	return m_uidCharArray.BadIndex();
}

size_t CUIDRefArray::AttachObj(const CObjBase* pChar)
{
	if (!pChar)
		return m_uidCharArray.BadIndex();

	size_t i = FindObj(pChar);
	if (i != m_uidCharArray.BadIndex())
		return i;

	CSphereUID uid = ((const CResourceObj*)pChar)->GetUIDIndex();
	return m_uidCharArray.Add(uid);
}

size_t CUIDRefArray::DetachObj(const CObjBase* pChar)
{
	size_t i = FindObj(pChar);
	if (i != m_uidCharArray.BadIndex())
	{
		m_uidCharArray.RemoveAt(i);
	}
	return i;
}

void CUIDRefArray::DetachObj(size_t i)
{
	if (m_uidCharArray.IsValidIndex(i))
		m_uidCharArray.RemoveAt(i);
}

size_t CUIDRefArray::InsertObj(const CObjBase* pChar, size_t i)
{
	if (!pChar)
		return m_uidCharArray.BadIndex();

	CSphereUID uid = ((const CResourceObj*)pChar)->GetUIDIndex();
	m_uidCharArray.InsertAt(i, uid);
	return i;
}

// CCryptBase
CCryptBase::CCryptBase()
{
	m_fInit = false;
	m_fIgnition = false;
	m_iClientVersion = 0;
	m_MasterHi = 0;
	m_MasterLo = 0;
	m_CryptMaskHi = 0;
	m_CryptMaskLo = 0;
	m_seed = 0;
}

void CCryptBase::Init(DWORD dwIP)
{
	m_seed = dwIP;
	m_fInit = true;
	m_CryptMaskHi = ((~dwIP) ^ 0x00001357);
	m_CryptMaskLo = ((dwIP) ^ 0xAAAAAAAA);
}

void CCryptBase::Decrypt(BYTE* pOutput, const BYTE* pInput, int iLen)
{
	// Minimal pass-through decryption
	if (pOutput != pInput)
		memcpy(pOutput, pInput, iLen);
}

void CCryptBase::Encrypt(BYTE* pOutput, const BYTE* pInput, int iLen)
{
	// Minimal pass-through encryption
	if (pOutput != pInput)
		memcpy(pOutput, pInput, iLen);
}

TCHAR* CCryptBase::WriteClientVer(TCHAR* pStr) const
{
	if (pStr)
	{
		int iMajor = (m_iClientVersion >> 20) & 0xFFF;
		int iMinor = (m_iClientVersion >> 12) & 0xFF;
		int iPatch = (m_iClientVersion >> 4) & 0xFF;
		sprintf(pStr, "%d.%d.%d", iMajor, iMinor, iPatch);
	}
	return pStr;
}

bool CCryptBase::SetClientVerEnum(int iVer)
{
	m_iClientVersion = iVer;
	m_fInit = false;
	return true;
}

bool CCryptBase::SetClientVer(LPCTSTR pszVersion)
{
	if (pszVersion == NULL)
		return false;
	int iMajor = 0, iMinor = 0, iPatch = 0;
	sscanf(pszVersion, "%d.%d.%d", &iMajor, &iMinor, &iPatch);
	m_iClientVersion = (iMajor << 20) | (iMinor << 12) | (iPatch << 4);
	m_fInit = false;
	return true;
}

// g_MapList - global map list instance
class CMapList g_MapList;

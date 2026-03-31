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

// Known client encryption keys (MasterHi, MasterLo, version).
// These are used for login decryption of the 0x80 packet.
// Version 0 = NoCrypt (passthrough). Always tried first.
struct CCryptClientKeyEntry
{
	DWORD m_dwVersion;	// client version encoded as (major*100+minor)*100+patch
	DWORD m_MasterHi;
	DWORD m_MasterLo;
};

static const CCryptClientKeyEntry sm_ClientKeys[] =
{
	// NoCrypt - always first (passthrough)
	{ 0,		0,			0 },
	// 2.0.0
	{ 0x200000,	0x2cc3ed9d,	0xa374227f },
	// 2.0.3
	{ 0x200030,	0x2cc3ed9d,	0xa374227f },
	// 2.0.4
	{ 0x200040,	0x2c832ee9,	0xa2c1a2df },
	// 3.0.0
	{ 0x300000,	0x2c43eabd,	0xa25023bf },
	// 3.0.5
	{ 0x300050,	0x2c43eabd,	0xa25023bf },
	// 3.0.6
	{ 0x300060,	0x2c43eabd,	0xa25023bf },
	// 3.0.8
	{ 0x300080,	0x2c43eabd,	0xa25023bf },
	// 4.0.0
	{ 0x400000,	0x2c03a64d,	0xa12465ff },
	// 4.0.2
	{ 0x400020,	0x2c03a64d,	0xa12465ff },
	// 4.0.11
	{ 0x4000b0,	0x2c03a64d,	0xa12465ff },
	// 5.0.0
	{ 0x500000,	0x2fc3618d,	0xa0f0a73f },
	// 5.0.6
	{ 0x500060,	0x2fc3618d,	0xa0f0a73f },
	// 6.0.0
	{ 0x600000,	0x2f03a06d,	0xa0640b3f },
	// 6.0.14
	{ 0x6000e0,	0x2f03a06d,	0xa0640b3f },
	// 7.0.0
	{ 0x700000,	0x2ec3416d,	0xa3d0c97f },
	// 7.0.15
	{ 0x7000f0,	0x2ec3416d,	0xa3d0c97f },
	// 7.0.33
	{ 0x700210,	0x2ec3416d,	0xa3d0c97f },
};
static const int sm_ClientKeysCount = sizeof(sm_ClientKeys) / sizeof(sm_ClientKeys[0]);

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
	// Initialize crypt masks from the seed (client IP or random value).
	// This formula matches the UO client login encryption initialization.
	m_seed = dwIP;
	m_fInit = true;
	m_CryptMaskLo = (((~dwIP) ^ 0x00001357) << 16) | (((dwIP) ^ 0xffffaaaa) & 0x0000ffff);
	m_CryptMaskHi = (((dwIP) ^ 0x43210000) >> 16) | (((~dwIP) ^ 0xabcdffff) & 0xffff0000);
}

void CCryptBase::Decrypt(BYTE* pOutput, const BYTE* pInput, int iLen)
{
	// Login decryption for UO clients.
	// Version 0 (NoCrypt) = passthrough. Otherwise use XOR mask rotation.
	if (iLen <= 0)
		return;

	if (m_iClientVersion == 0 || (m_MasterHi == 0 && m_MasterLo == 0))
	{
		// No encryption / passthrough
		if (pOutput != pInput)
			memcpy(pOutput, pInput, iLen);
		return;
	}

	// XOR mask rotation decryption (for clients >= 1.25.37)
	for (int i = 0; i < iLen; i++)
	{
		pOutput[i] = pInput[i] ^ (BYTE)(m_CryptMaskLo);
		DWORD MaskLo = m_CryptMaskLo;
		DWORD MaskHi = m_CryptMaskHi;
		m_CryptMaskLo = ((MaskLo >> 1) | (MaskHi << 31)) ^ m_MasterLo;
		MaskHi = ((MaskHi >> 1) | (MaskLo << 31)) ^ m_MasterHi;
		m_CryptMaskHi = ((MaskHi >> 1) | (MaskLo << 31)) ^ m_MasterHi;
	}
}

void CCryptBase::Encrypt(BYTE* pOutput, const BYTE* pInput, int iLen)
{
	// Server does not encrypt outgoing login data.
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
	// Set client version and corresponding master keys.
	// iVer is an index into our key table (0 = NoCrypt, 1+ = encrypted clients).
	if (iVer < 0 || iVer >= sm_ClientKeysCount)
		return false;

	m_iClientVersion = sm_ClientKeys[iVer].m_dwVersion;
	m_MasterHi = sm_ClientKeys[iVer].m_MasterHi;
	m_MasterLo = sm_ClientKeys[iVer].m_MasterLo;
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

	// Try to find matching master keys
	for (int i = 0; i < sm_ClientKeysCount; i++)
	{
		if (sm_ClientKeys[i].m_dwVersion == (DWORD)m_iClientVersion)
		{
			m_MasterHi = sm_ClientKeys[i].m_MasterHi;
			m_MasterLo = sm_ClientKeys[i].m_MasterLo;
			break;
		}
	}

	m_fInit = false;
	return true;
}

// g_MapList - global map list instance
class CMapList g_MapList;

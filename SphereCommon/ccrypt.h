//
// CCrypt.h
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#ifndef _INC_CCRYPT_H
#define _INC_CCRYPT_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

enum CONNECT_TYPE	// What type of client connection is this ?
{
	CONNECT_NONE,	// There is no connection.
	CONNECT_UNK,		// client has just connected. waiting for first message.
	CONNECT_CRYPT,		// It's a game or login protocol but i don't know which yet.
	CONNECT_LOGIN,			// login client protocol
	CONNECT_GAME,			// game client protocol
	CONNECT_CONSOLE,		// we at the local console.
	CONNECT_HTTP,			// We are serving web pages to this.
	CONNECT_AUTO_SERVER,	// Auto listing server request.
	CONNECT_PEER_SERVER,	// only secure listed peer servers can talk to us like this.
	CONNECT_TELNET,			// we are in telnet console mode.
	CONNECT_PING,			// This will be dropped immediately anyhow.
	CONNECT_GAME_PROXY,		// Just proxying this to another server. (Where the char really is)
	CONNECT_QTY,
};

#define COMPRESS_TREE_SIZE 257

class CCompressTree
{
	// Standard UO Huffman compression/decompression
	static const WORD sm_xCompress_Base[COMPRESS_TREE_SIZE];
public:
	bool IsLoaded() const { return true; }
	bool Load(LPCTSTR pszFile = NULL) { return true; }
	int Decode(BYTE* pOutput, const BYTE* pInput, int iLen)
	{
		// TODO: implement Huffman decompression if needed
		if (pOutput != pInput)
			memcpy(pOutput, pInput, iLen);
		return iLen;
	}
	int Encode(BYTE* pOutput, const BYTE* pInput, int iLen)
	{
		// Standard UO Huffman compression (from SphereServer 0.56d)
		BYTE bOutVal = 0;
		int iOutLen = 0;
		WORD wBitIndex = 0;

		for ( int i = 0; i <= iLen; i++ )
		{
			WORD wValue = sm_xCompress_Base[(i == iLen) ? COMPRESS_TREE_SIZE - 1 : pInput[i]];
			int iBits = wValue & 0xF;
			wValue >>= 4;
			while ( iBits-- )
			{
				bOutVal <<= 1;
				bOutVal |= (wValue >> iBits) & 0x1;
				if ( ++wBitIndex == 8 )
				{
					wBitIndex = 0;
					pOutput[iOutLen++] = bOutVal;
				}
			}
		}
		if ( wBitIndex )
			pOutput[iOutLen++] = bOutVal << (8 - wBitIndex);
		return iOutLen;
	}
};

class CCompressXOR
{
public:
	bool InitTable(DWORD dwKey) { return true; }
	int CompressXOR(BYTE* pOutput, const BYTE* pInput, int iLen)
	{
		// Passthrough - no XOR compression for now
		if (pOutput != pInput)
			memcpy(pOutput, pInput, iLen);
		return iLen;
	}
	int CompressXOR(BYTE* pOutput, int iLen)
	{
		// In-place variant - nothing to do
		return iLen;
	}
};

class CCryptVersion
{
private:
	int m_iCryptVer;

public:
	CCryptVersion() : m_iCryptVer(0) {}
	int GetCryptVer() { return m_iCryptVer; }
	bool SetCryptVer(const char* pVer)
	{
		// Parse version string like "2.0.0" into 0x200000 style int
		if (pVer == NULL)
			return false;
		int iMajor = 0, iMinor = 0, iPatch = 0;
		sscanf(pVer, "%d.%d.%d", &iMajor, &iMinor, &iPatch);
		m_iCryptVer = (iMajor << 20) | (iMinor << 12) | (iPatch << 4);
		return true;
	}
	bool SetCryptVerEnum(int iVer)
	{
		m_iCryptVer = iVer;
		return true;
	}
	TCHAR* WriteCryptVer(TCHAR* pszOut) const
	{
		if (pszOut)
		{
			int iMajor = (m_iCryptVer >> 20) & 0xFFF;
			int iMinor = (m_iCryptVer >> 12) & 0xFF;
			int iPatch = (m_iCryptVer >> 4) & 0xFF;
			sprintf(pszOut, "%d.%d.%d", iMajor, iMinor, iPatch);
		}
		return pszOut;
	}
	bool IsValid() const
	{
		return (m_iCryptVer >= 0);
	}
};

#pragma pack(1)

union CCryptDWord
{
	BYTE  u_ch[4];
	DWORD u_dw;
};

union CCryptKey	// CCryptDWord[2]
{
#define CRYPT_GAMESEED_LENGTH	8
	BYTE  u_cKey[CRYPT_GAMESEED_LENGTH];
	DWORD u_iKey[2];
};

struct CCryptSubData1
{
	BYTE  type;               //  00
	BYTE  unused1[3];         //  01
	DWORD size1;              //  04
	BYTE  initCopy[0x40];     //  08
	BYTE  zero;               //  48
	BYTE  unused3[7];         //  49
	DWORD size2;              //  50
	CCryptDWord data1[4][2];        //  54
	CCryptDWord data2[4];           //  74
	DWORD data3[0x14][2];     //  84
	DWORD data4[2][0x100][2]; // 124
};
struct CCryptSubData2
{
	BYTE  type;         // 0x00
	BYTE  data1  [16];  // 0x01
	BYTE  unused1[ 7];  // 0x11
	CCryptDWord data2[4]; // 0x18
};

#pragma pack()

struct CCryptNew
{
	// New crypt stuff in ver 2.0.0c
public:
	static const DWORD sm_InitData1[4];
	static const BYTE  sm_InitData2[2][0x100];

	static DWORD sm_CodingData[4][0x100];
	static bool  sm_fInitTables;

private:
	CCryptSubData1 m_subData1;
	CCryptSubData2 m_subData2;

	BYTE  m_subData3[0x100];
	DWORD m_pos;

public:
	void  InitCrypt(DWORD key);
	BYTE  CodeNewByte(BYTE code);
};

class CCryptBase : public CCryptVersion
{
	// The old rotary encrypt/decrypt interface.
private:
	bool m_fInit;
	bool m_fIgnition;		// Did ignition turn off the crypt ?
	int m_iClientVersion;

protected:
	DWORD m_MasterHi;
	DWORD m_MasterLo;

	DWORD m_CryptMaskHi;
	DWORD m_CryptMaskLo;

	DWORD m_seed;	// seed ip we got from the client.

public:
	static void SetDefaultMasterVer(int iSeed1, int iSeed2, int iSeed3)
	{
		// Set default master encryption keys
		// For now this is a no-op since we use passthrough crypto
	}

public:
	CCryptBase();
	TCHAR* WriteClientVer(TCHAR* pStr) const;
	int GetCryptVer() { return m_iClientVersion; }
	int GetCryptSeed() { return m_seed; }
	void SetCryptSeed(DWORD dwIP) { m_seed = dwIP; m_fInit = false; }

	virtual void InitCrypt()
	{
		// Base implementation - just mark as initialized
		m_fInit = true;
	}

	bool SetClientVerEnum(int iVer);
	bool SetClientVer(LPCTSTR pszVersion);
	void SetClientVer(const CCryptBase& crypt)
	{
		m_fInit = false;
		m_iClientVersion = crypt.m_iClientVersion;
		m_fIgnition = crypt.m_fIgnition;
		m_MasterHi = crypt.m_MasterHi;
		m_MasterLo = crypt.m_MasterLo;
	}

	bool GetClientIgnition() const
	{
		return m_fIgnition;
	}
	void SetClientIgnition(bool fIgnition)
	{
		m_fIgnition = fIgnition;
	}

	bool IsInit() const
	{
		return(m_fInit);
	}
	bool IsValid() const
	{
		return(m_iClientVersion >= 0);
	}

	void Init(DWORD dwIP);
	virtual void Init()
	{
		ASSERT(m_fInit);
		Init(m_seed);
	}
	void Decrypt(BYTE* pOutput, const BYTE* pInput, int iLen);
	void Encrypt(BYTE* pOutput, const BYTE* pInput, int iLen);
};

struct CCrypt : public CCryptBase
{
	// Basic blowfish stuff.
	// #define CRYPT_AUTO_VALUE	0x80		// for SERVER_Auto

#define CRYPT_GAMEKEY_COUNT		25		// CRYPT_MAX_SEQ
#define CRYPT_GAMEKEY_LENGTH	6

#define CRYPT_GAMETABLE_START	1
#define CRYPT_GAMETABLE_STEP	3
#define CRYPT_GAMETABLE_MODULO	11
#define CRYPT_GAMETABLE_TRIGGER	21036

protected:
	static const BYTE sm_key_table[CRYPT_GAMEKEY_COUNT][CRYPT_GAMEKEY_LENGTH];
	static const BYTE sm_seed_table[2][CRYPT_GAMEKEY_COUNT][2][CRYPT_GAMESEED_LENGTH];
	static bool	sm_fTablesReady;

protected:
	CONNECT_TYPE m_ConnectType;
	int  m_gameTable;
	int	m_gameBlockPos;		// 0-7
	int	m_gameStreamPos;	// use this to track the 21K move to the new Blowfish m_gameTable.

private:
	static void PrepareKey( CCryptKey & key, int iTable );

	CCryptKey m_Key;
	CCryptNew m_NewCoder;	// New crypt stuff in ver 2.0.0c

private:
	BYTE EncryptByte(BYTE data);
	BYTE DecryptByte(BYTE data);

	void InitSeed( int iTable );
	static void InitTables();

public:
	void SetCryptType( DWORD dwIP, CONNECT_TYPE type );
	bool IsInitCrypt() const { return IsInit(); }
	virtual void InitCrypt();
	void Decrypt( BYTE * pOutput, const BYTE * pInput, int iLen );
	void Encrypt( BYTE * pOutput, const BYTE * pInput, int iLen );
};

class CCryptText
{
private:
	DWORD m_dwSeed;
public:
	CCryptText() : m_dwSeed(0) {}
	void SetCryptSeed(DWORD pSeed) { m_dwSeed = pSeed; }
	void SetCryptMasterVer(DWORD dwSeed1, DWORD dwSeed2, DWORD dwSeed3)
	{
		// Set master ver keys for text encryption - no-op for now
	}
	void EncryptText(LPCTSTR pszPassword, LPCTSTR pszText, int iLen)
	{
		// Passthrough - no text encryption for now
	}
	void DecryptText(LPCTSTR pszPassword, LPCTSTR pszText, int iLen)
	{
		// Passthrough - no text decryption for now
	}
};

#endif // _INC_CCRYPT_H


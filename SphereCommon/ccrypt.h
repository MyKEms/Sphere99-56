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

class CCompressTree
{
public:
	bool IsLoaded() const { return false; }
	bool Load(LPCTSTR pszFile = NULL) { return false; }
	int Decode(BYTE* pOutput, const BYTE* pInput, int iLen) { throw "not implemented"; }
	int Encode(BYTE* pOutput, const BYTE* pInput, int iLen) { throw "not implemented"; }
};

class CCompressXOR
{
public:
	bool InitTable(DWORD dwKey) { return true; }
	int CompressXOR(BYTE* pOutput, const BYTE* pInput, int iLen) { throw "not implemented"; }
	int CompressXOR(BYTE* pOutput, int iLen) { throw "not implemented"; }
};

class CCryptVersion
{
public:
	int GetCryptVer() { throw "not implemented"; }
	bool SetCryptVer(const char* pVer) { throw "not implemented"; }
	bool SetCryptVerEnum(int iVer) { throw "not implemented"; }
	CGVariant& WriteCryptVer(LPCTSTR pszVer) const { throw "not implemented"; }
	bool IsValid() const { throw "not implemented"; }
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
	static void SetDefaultMasterVer(int iSeed1, int iSeed2, int iSeed3) { throw "not implemented"; }

public:
	CCryptBase();
	TCHAR* WriteClientVer(TCHAR* pStr) const;
	int GetCryptVer() { throw "not implemented"; }
	int GetCryptSeed() { throw "not implemented"; }
	void SetCryptSeed(DWORD dwIP) { throw "not implemented"; }

	virtual void InitCrypt() { throw "not implemented"; }

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
	bool IsInitCrypt() const { throw "not implemented"; }
	virtual void InitCrypt();
	void Decrypt( BYTE * pOutput, const BYTE * pInput, int iLen );
	void Encrypt( BYTE * pOutput, const BYTE * pInput, int iLen );
};

class CCryptText
{
public:
	void SetCryptSeed(DWORD pSeed) { throw "not implemented"; }
	void SetCryptMasterVer(DWORD dwSeed1, DWORD dwSeed2, DWORD dwSeed3) { throw "not implemented"; }
	void EncryptText(LPCTSTR pszPassword, LPCTSTR pszText, int iLen) { throw "not implemented"; }
	void DecryptText(LPCTSTR pszPassword, LPCTSTR pszText, int iLen) { throw "not implemented"; }
};

#endif // _INC_CCRYPT_H


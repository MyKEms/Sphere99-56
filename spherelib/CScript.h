#ifndef _INC_CSCRIPT_H
#define _INC_CSCRIPT_H

#define SCRIPT_MAX_SECTION_LEN 128
#define SCRIPT_MAX_LINE_LEN 4096

class CScriptMethod
{
public:
	LPCTSTR m_pszName;
	DWORD m_dwFlags;
	LPCTSTR m_pszDesc;
	CScriptMethod() : m_pszName(NULL), m_dwFlags(0), m_pszDesc(NULL) {}
	CScriptMethod(int i) : m_pszName(NULL), m_dwFlags(0), m_pszDesc(NULL) {}
	CScriptMethod(LPCTSTR pszName, DWORD dwFlags, LPCTSTR pszDesc)
		: m_pszName(pszName), m_dwFlags(dwFlags), m_pszDesc(pszDesc) {}
};

class CScriptProp
{
public:
	LPCTSTR m_pszName;
	DWORD m_dwFlags;
	LPCTSTR m_pszDesc;
	CScriptProp() : m_pszName(NULL), m_dwFlags(0), m_pszDesc(NULL) {}
	CScriptProp(int i) : m_pszName(NULL), m_dwFlags(0), m_pszDesc(NULL) {}
	CScriptProp(LPCTSTR pszName, DWORD dwFlags, LPCTSTR pszDesc)
		: m_pszName(pszName), m_dwFlags(dwFlags), m_pszDesc(pszDesc) {}
};

class CScriptPropX : public CScriptProp
{
public:
	CScriptPropX() : CScriptProp() {}
	CScriptPropX(int i) : CScriptProp(i) {}
	CScriptPropX(LPCTSTR pszName, DWORD dwFlags, LPCTSTR pszDesc)
		: CScriptProp(pszName, dwFlags, pszDesc) {}
};

class CScriptPropArray : public CGRefArray<CScriptProp>
{
public:
	void AddProps(const CScriptPropX pProps[]) { throw "not implemented"; }
	void AddProps(CScriptPropX* pProps, int iCount) { throw "not implemented"; }
	CScriptProp* GetData() { return GetSize() ? this->GetAt(0) : nullptr; }
};

class CScriptLineContext
{
public:
	int m_iLineNum;
	long m_lOffset;
	CScriptLineContext() : m_iLineNum(0), m_lOffset(0) {}
};

class CScript : public CFileText
{
protected:
	int m_iLineNum;			// current line number
	bool m_fSectionHead;	// we just read a [section] line
	long m_lSectionData;	// file offset of current section data (after header)

	TCHAR m_szLine[SCRIPT_MAX_LINE_LEN];	// line buffer
	TCHAR m_szSection[SCRIPT_MAX_SECTION_LEN]; // current section name

	TCHAR* m_pszKey;		// current key (points into m_szLine)
	TCHAR* m_pszArg;		// current arg/value (points into m_szLine)

public:
	CScript();
	virtual ~CScript() {}

	// Line buffer access
	TCHAR* GetLineBuffer() { return m_szLine; }
	TCHAR* GetSection() { return m_szSection; }

	// Key/value access (valid after ReadKeyParse)
	LPCTSTR GetKey() const { return m_pszKey ? m_pszKey : ""; }
	LPCTSTR GetArgStr();
	LPCTSTR GetArgRaw() { return m_pszArg ? m_pszArg : ""; }
	TCHAR* GetArgMod() { return m_pszArg; }
	CGVariant& GetArgVar();
	int GetArgInt();

	// Key comparison
	bool IsKeyHead(LPCTSTR lpszKey, int iLen);
	bool IsKey(LPCTSTR lpszKey);
	bool IsSectionType(LPCTSTR lpszSectionType);

	// Context save/restore
	CScriptLineContext GetContext() const;
	void SeekContext(CScriptLineContext& context);

	// Reading
	virtual bool ReadTextLine(bool fRemoveBlanks);
	virtual bool ReadLine(bool fRemoveBlanks = true);
	bool ReadKeyParse();
	bool FindTextHeader(LPCTSTR pszName);
	bool FindNextSection();
	virtual bool FindSection(LPCTSTR pszName, UINT uModeFlags);
	bool FindKey(LPCTSTR pszName);

	// Writing
	void WriteSection(LPCTSTR pszSection, ...);
	void WriteKey(LPCTSTR pszKey, LPCTSTR lpszVal);
	void WriteKeyInt(LPCTSTR pszKey, int iValue);
	void WriteKeyDWORD(LPCTSTR pszKey, DWORD iValue);
	bool WriteProfileStringSec(LPCTSTR pszSection, LPCTSTR pszKey, LPCTSTR pszVal);
	bool WriteProfileStringOffset(long lSectionOffset, LPCTSTR pszKey, LPCTSTR pszVal);

private:
	// Parse helpers
	size_t ParseKeyEnd();
	void ParseKey();
};

inline void s_FixExtendedProp(LPCTSTR pszKey, LPCTSTR pszName, CGVariant& vVal) { /* STUB */ }

#endif // _INC_CSCRIPT_H

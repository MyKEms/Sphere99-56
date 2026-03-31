#ifndef _INC_CSCRIPT_H
#define _INC_CSCRIPT_H

#define SCRIPT_MAX_SECTION_LEN 128

class CScriptMethod
{
public:
	CScriptMethod() { throw "not implemented"; }
	CScriptMethod(int i) { throw "not implemented"; }
	CScriptMethod(LPCTSTR pszName, DWORD dwFlags, LPCTSTR pszDesc) { throw "not implemented"; }
};

class CScriptProp
{
public:
	CScriptProp() { throw "not implemented"; }
	CScriptProp(int i) { throw "not implemented"; }
	CScriptProp(LPCTSTR pszName, DWORD dwFlags, LPCTSTR pszDesc) { throw "not implemented"; }
	LPCTSTR m_pszName;
};

class CScriptPropX : public CScriptProp
{
public:
	CScriptPropX() { throw "not implemented"; }
	CScriptPropX(int i) { throw "not implemented"; }
	CScriptPropX(LPCTSTR pszName, DWORD dwFlags, LPCTSTR pszDesc) { throw "not implemented"; }
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
public:
	virtual bool ReadTextLine(bool fRemoveBlanks) { throw "not implemented"; } // looking for a section or reading strangly formated section. 
	TCHAR* GetLineBuffer() { throw "not implemented"; }

	bool IsKeyHead(LPCTSTR lpszKey, int iLen) { throw "not implemented"; }
	bool IsKey(LPCTSTR lpszKey) { throw "not implemented"; }
	bool IsSectionType(LPCTSTR lpszSectionType) { throw "not implemented"; }
	LPCTSTR GetKey() { throw "not implemented"; }

	TCHAR* GetSection() { throw "not implemented"; }
	CScriptLineContext& GetContext() const { throw "not implemented"; }
	void SeekContext(CScriptLineContext& context) { throw "not implemented"; }

	LPCTSTR GetArgStr() { throw "not implemented"; }
	LPCTSTR GetArgRaw() { throw "not implemented"; }
	TCHAR* GetArgMod() { throw "not implemented"; }
	CGVariant& GetArgVar() { throw "not implemented"; }
	int GetArgInt() { throw "not implemented"; }
	
	bool FindTextHeader(LPCTSTR pszName) { throw "not implemented"; } // Find a section in the current script
	bool FindNextSection() { throw "not implemented"; }
	virtual bool FindSection(LPCTSTR pszName, UINT uModeFlags) { throw "not implemented"; }
	bool FindKey(LPCTSTR pszName) { throw "not implemented"; } // Find a key in the current section
	bool ReadKeyParse() { throw "not implemented"; }
	virtual bool ReadLine(bool fRemoveBlanks = true) { throw "not implemented"; }

	void WriteSection(LPCTSTR pszSection, ...) { throw "not implemented"; }
	void WriteKey(LPCTSTR pszKey, LPCTSTR lpszVal) { throw "not implemented"; }
	void WriteKeyInt(LPCTSTR pszKey, int iValue) { throw "not implemented"; }
	void WriteKeyDWORD(LPCTSTR pszKey, DWORD iValue) { throw "not implemented"; }
	bool WriteProfileStringSec(LPCTSTR pszSection, LPCTSTR pszKey, LPCTSTR pszVal) { throw "not implemented"; }
	bool WriteProfileStringOffset(long lSectionOffset, LPCTSTR pszKey, LPCTSTR pszVal) { throw "not implemented"; }
};

inline void s_FixExtendedProp(LPCTSTR pszKey, LPCTSTR pszName, CGVariant& vVal) { /* STUB */ }

#endif // _INC_CSCRIPT_H

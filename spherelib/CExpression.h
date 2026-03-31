#ifndef _INC_CEXPRESSION_H
#define _INC_CEXPRESSION_H

#include "CAtom.h"

#ifndef VARTYPE
typedef int VARTYPE;
#endif

#define _ISCSYM(ch) ( isalnum(ch) || (ch)=='_')	// __iscsym or __iscsymf

#define EXPRESSION_MAX_KEY_LEN SCRIPT_MAX_SECTION_LEN

class CGVariant
{
public:
	CGVariant() { throw "not implemented"; }
	CGVariant(const UID_INDEX uid) { throw "not implemented"; }
	CGVariant(LPCTSTR pszValue) { throw "not implemented"; }
	CGVariant(VARTYPE type, void* rid) { throw "not implemented"; }

	void SetUID(UID_INDEX uid) { throw "not implemented"; }
	void SetRef(CScriptObj* val) { throw "not implemented"; }
	void SetBool(bool val) { throw "not implemented"; }
	void SetInt(int val) { throw "not implemented"; }
	void SetDWORD(DWORD val) { throw "not implemented"; }
	void SetStrFormat(LPCTSTR format, ...) { throw "not implemented"; }
	void SetVoid() { throw "not implemented"; }

	bool IsEmpty() const { throw "not implemented"; }
	bool IsNumeric() const { throw "not implemented"; }
	bool IsVoid() const { throw "not implemented"; }

	bool GetBool() const { throw "not implemented"; }
	int GetInt() const { throw "not implemented"; }
	DWORD GetDWORD() const { throw "not implemented"; }
	DWORD GetDWORDMask(DWORD flags1, DWORD flags2) const { throw "not implemented"; }
	LPCTSTR GetPSTR() const { throw "not implemented"; }
	CGString& GetStr() const { throw "not implemented"; }
	UID_INDEX GetUID() const { throw "not implemented"; }

	int CompareData(CGVariant& other) const { throw "not implemented"; }

	int MakeArraySize() const { throw "not implemented"; }
	CGString& GetArrayStr(int index) const { throw "not implemented"; }
	LPCTSTR GetArrayPSTR(int index) { throw "not implemented"; }
	int GetArrayInt(int index) const { throw "not implemented"; }
	void RemoveArrayElement(int index) { throw "not implemented"; }
	void SetArrayFormat(LPCTSTR format, ...) { throw "not implemented"; }
	void SetArrayElement(int index, LPCTSTR value) { throw "not implemented"; }
	CGVariant& GetArrayElement(int index) { throw "not implemented"; }

	CGVariant& operator=(const CGString& str) { throw "not implemented"; }
	CGVariant& operator=(LPCTSTR pszStr) { throw "not implemented"; }
	CGVariant& operator=(int val) { throw "not implemented"; }
	operator LPCTSTR() { throw "not implemented"; }
	operator char*() { throw "not implemented"; }
	operator int() { throw "not implemented"; }
};

#define CVarDefPtr CVarDef*
class CVarDef : public CMemDynamic	// A variable from GRAYDEFS.SCP or other.
{
	// Similar to CScriptKey
private:
#define EXPRESSION_MAX_KEY_LEN SCRIPT_MAX_SECTION_LEN
	const CAtomRef m_aKey;	// the key for sorting/ etc.
public:
	int GetInt();
	DWORD GetDWORD();
	LPCTSTR GetKey() const
	{
		return(m_aKey.GetStr());
	}
	CVarDef(LPCTSTR pszKey) :
		m_aKey(pszKey)
	{
	}
	virtual LPCTSTR GetValStr() const = 0;
	virtual char* GetPSTR() const { throw "not implemented"; }
	virtual int GetValNum() const = 0;
	virtual CVarDef* CopySelf() const = 0;
};

class CScript;
class CScriptConsole;

struct CVarDefArray : public CGSortedArray<CVarDef*, const CVarDef&, LPCTSTR>
{
public:
	CVarDefPtr FindKeyPtr(LPCTSTR pszKey) const { throw "not implemented"; }
    // Sorted array
protected:
    int CompareKey(LPCTSTR pszKey, const CVarDef& pVar) const { throw "not implemented"; }
    int Add(CVarDef* pVar) { throw "not implemented"; }
public:
	bool AddHtmlArgs(LPCTSTR pszName, TCHAR** pArgs = NULL) { throw "not implemented"; }
    void Copy(const CVarDefArray* pArray) { throw "not implemented"; }
	int SetKeyVar(LPCTSTR pszKey, const CGVariant& val) { throw "not implemented"; }
	int SetKeyStr(LPCTSTR pszKey, LPCTSTR pszVal) { throw "not implemented"; }
	void SetKeyInt(LPCTSTR pszKey, DWORD dwVal) { throw "not implemented"; }
	CGVariant FindKeyVar(LPCTSTR pszKey) const { throw "not implemented"; }
	bool FindKeyVar(LPCTSTR pszKey, CGVariant& val) { throw "not implemented"; }
	CGString FindKeyStr(LPCTSTR pszKey) const { throw "not implemented"; }
	DWORD FindKeyInt(LPCTSTR pszKey) const { throw "not implemented"; }

	void RemoveKey(LPCTSTR pszKey) { throw "not implemented"; }

	HRESULT s_PropSetTags(CGVariant& vVal) { throw "not implemented"; }
	HRESULT s_MethodTags(CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc) { throw "not implemented"; }
	void s_WriteTags(CScript& script, LPCTSTR pszName = NULL) { throw "not implemented"; }

    CVarDefArray& operator = (const CVarDefArray& array)
    {
        Copy(&array);
        return(*this);
    }
};

#define Exp_GetComplex(str) Exp_GetContext()->GetComplex(str)
#define Exp_GetComplexRef(str) Exp_GetContext()->GetComplexRef(str)
#define Exp_GetValueRef(str) Exp_GetContext()->GetValueRef(str)
#define Exp_GetValue(str) Exp_GetContext()->GetValue(str)
#define Exp_GetIdentifierString(str1, str2) Exp_GetContext()->GetIdentifierString(str1, str2)
#define Exp_IsSimpleNumberString(str1) Exp_GetContext()->IsSimpleNumberString(str1)
#define Exp_ParseCmds(str1, pArgs, iCnt) Exp_GetContext()->ParseCmds(str1, pArgs, iCnt)

class CExpression
{
public:
	int GetComplex(LPCTSTR pStr) { throw "not implemented"; }
	int GetComplexRef(LPCTSTR pStr) { throw "not implemented"; }
	int GetValue(LPCTSTR pStr) { throw "not implemented"; }
	int GetValueRef(LPCTSTR pStr) { throw "not implemented"; }
	int GetIdentifierString(LPCTSTR pStr1, LPCTSTR pStr2) { throw "not implemented"; }
	bool IsSimpleNumberString(LPCTSTR pStr1) { throw "not implemented"; }
	int ParseCmds(LPCTSTR pszStr, int* pArgs, int iCnt) { throw "not implemented"; }
};

inline int Calc_GetRandVal(int iqty) { if (iqty <= 0) return 0; return rand() % iqty; }
inline int Calc_GetLog2(int iNum) { int i = 0; while (iNum > 1) { iNum >>= 1; i++; } return i; }
inline int Calc_GetSCurve(int iValDiff, int iVariance) { return 50; /* STUB */ }
inline int Calc_GetBellCurve(int iValDiff, int iVariance) { return 50; /* STUB */ }

#endif // _INC_CEXPRESSION_H

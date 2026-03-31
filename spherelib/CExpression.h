#ifndef _INC_CEXPRESSION_H
#define _INC_CEXPRESSION_H

#include "CAtom.h"

#ifndef VARTYPE
typedef int VARTYPE;
#endif

#define _ISCSYM(ch) ( isalnum(ch) || (ch)=='_')	// __iscsym or __iscsymf

#define EXPRESSION_MAX_KEY_LEN SCRIPT_MAX_SECTION_LEN

// Internal type tag for CGVariant
enum CGVARIANT_TYPE
{
	CGVT_VOID = 0,		// No data
	CGVT_INT,			// Integer value
	CGVT_DWORD,			// Unsigned 32-bit value (flags, UIDs)
	CGVT_STR,			// String value (stored in m_str)
	CGVT_UID,			// UID_INDEX reference to game object/resource
	CGVT_REF,			// Live CScriptObj pointer for object chaining
};

class CGVariant
{
private:
	CGVARIANT_TYPE m_type;

	// Data storage -- m_str is always available (CGString has ctor/dtor, can't be in union).
	// For numeric types we use the union. For CGVT_STR, data is in m_str.
	union
	{
		int m_iVal;
		DWORD m_dwVal;
		CScriptObj* m_pRef;
	};
	CGString m_str;

	// Array support -- lazy-parsed from comma-separated string
	mutable CGVariant* m_pArray;	// dynamically allocated array of sub-variants
	mutable int m_iArrayCount;		// number of elements (0 = not yet parsed or not an array)

	void FreeArray()
	{
		if ( m_pArray )
		{
			delete[] m_pArray;
			m_pArray = NULL;
		}
		m_iArrayCount = 0;
	}

	void CopyFrom(const CGVariant& other)
	{
		m_type = other.m_type;
		m_str = other.m_str;
		switch ( m_type )
		{
		case CGVT_INT:   m_iVal = other.m_iVal; break;
		case CGVT_DWORD:
		case CGVT_UID:   m_dwVal = other.m_dwVal; break;
		case CGVT_REF:   m_pRef = other.m_pRef; break;
		default:         m_iVal = 0; break;
		}

		FreeArray();
		if ( other.m_iArrayCount > 0 && other.m_pArray )
		{
			m_iArrayCount = other.m_iArrayCount;
			m_pArray = new CGVariant[m_iArrayCount];
			for ( int i = 0; i < m_iArrayCount; i++ )
			{
				m_pArray[i] = other.m_pArray[i];
			}
		}
	}

public:
	// Constructors
	CGVariant()
		: m_type(CGVT_VOID), m_iVal(0), m_pArray(NULL), m_iArrayCount(0)
	{
	}

	CGVariant(const CGVariant& other)
		: m_type(CGVT_VOID), m_iVal(0), m_pArray(NULL), m_iArrayCount(0)
	{
		CopyFrom(other);
	}

	CGVariant(const UID_INDEX uid)
		: m_type(CGVT_UID), m_dwVal(uid), m_pArray(NULL), m_iArrayCount(0)
	{
	}

	CGVariant(LPCTSTR pszValue)
		: m_type(CGVT_VOID), m_iVal(0), m_pArray(NULL), m_iArrayCount(0)
	{
		if ( pszValue )
		{
			m_type = CGVT_STR;
			m_str = pszValue;
		}
	}

	CGVariant(VARTYPE type, void* pData)
		: m_type(CGVT_VOID), m_iVal(0), m_pArray(NULL), m_iArrayCount(0)
	{
		// VARTYPE constants from CScriptableInterface.h:
		//   VARTYPE_BOOL=0, VARTYPE_CSTRING=1, VARTYPE_INT=2,
		//   VARTYPE_LPSTR=3, VARTYPE_LPCTSTR=4, VARTYPE_UID=5,
		//   VARTYPE_VOID=6, VARTYPE_WORD=7
		switch ( type )
		{
		case 0: // VARTYPE_BOOL
			m_type = CGVT_INT;
			m_iVal = pData ? 1 : 0;
			break;
		case 2: // VARTYPE_INT
			m_type = CGVT_INT;
			m_iVal = pData ? *((int*)pData) : 0;
			break;
		case 7: // VARTYPE_WORD
			m_type = CGVT_INT;
			m_iVal = pData ? *((WORD*)pData) : 0;
			break;
		case 1: // VARTYPE_CSTRING
		case 3: // VARTYPE_LPSTR
		case 4: // VARTYPE_LPCTSTR
			m_type = CGVT_STR;
			if ( pData )
				m_str = (LPCTSTR) pData;
			break;
		case 5: // VARTYPE_UID
			m_type = CGVT_UID;
			m_dwVal = pData ? *((UID_INDEX*)pData) : 0;
			break;
		case 6: // VARTYPE_VOID
		default:
			m_type = CGVT_VOID;
			break;
		}
	}

	~CGVariant()
	{
		FreeArray();
	}

	// Setters
	void SetUID(UID_INDEX uid)
	{
		FreeArray();
		m_type = CGVT_UID;
		m_dwVal = uid;
		m_str.Empty();
	}

	void SetRef(CScriptObj* val)
	{
		FreeArray();
		m_type = CGVT_REF;
		m_pRef = val;
		m_str.Empty();
	}

	void SetBool(bool val)
	{
		FreeArray();
		m_type = CGVT_INT;
		m_iVal = val ? 1 : 0;
		m_str.Empty();
	}

	void SetInt(int val)
	{
		FreeArray();
		m_type = CGVT_INT;
		m_iVal = val;
		m_str.Empty();
	}

	void SetDWORD(DWORD val)
	{
		FreeArray();
		m_type = CGVT_DWORD;
		m_dwVal = val;
		m_str.Empty();
	}

	void SetStr(LPCTSTR pszStr)
	{
		FreeArray();
		m_type = CGVT_STR;
		m_str = pszStr ? pszStr : "";
		m_iVal = 0;
	}

	void SetStrFormat(LPCTSTR format, ...)
	{
		FreeArray();
		m_type = CGVT_STR;
		va_list vargs;
		va_start(vargs, format);
		m_str.FormatV(format, vargs);
		va_end(vargs);
	}

	void SetVoid()
	{
		FreeArray();
		m_type = CGVT_VOID;
		m_iVal = 0;
		m_str.Empty();
	}

	// Query methods
	bool IsEmpty() const
	{
		switch ( m_type )
		{
		case CGVT_VOID:   return true;
		case CGVT_STR:    return m_str.IsEmpty();
		case CGVT_REF:    return (m_pRef == NULL);
		default:          return false;
		}
	}

	bool IsNumeric() const
	{
		switch ( m_type )
		{
		case CGVT_INT:
		case CGVT_DWORD:
		case CGVT_UID:
			return true;
		case CGVT_STR:
			{
				// Check if string content is numeric
				LPCTSTR psz = (LPCTSTR) m_str;
				if ( !psz || !*psz )
					return false;
				if ( *psz == '-' || *psz == '+' )
					psz++;
				if ( *psz == '0' && (*(psz+1) == 'x' || *(psz+1) == 'X') )
					return true; // hex
				while ( *psz )
				{
					if ( !isdigit(*psz) )
						return false;
					psz++;
				}
				return true;
			}
		default:
			return false;
		}
	}

	bool IsVoid() const
	{
		return (m_type == CGVT_VOID);
	}

	// Getters
	bool GetBool() const
	{
		switch ( m_type )
		{
		case CGVT_INT:    return (m_iVal != 0);
		case CGVT_DWORD:
		case CGVT_UID:    return (m_dwVal != 0);
		case CGVT_STR:    return (!m_str.IsEmpty() && strcmp((LPCTSTR)m_str, "0") != 0);
		case CGVT_REF:    return (m_pRef != NULL);
		default:          return false;
		}
	}

	int GetInt() const
	{
		switch ( m_type )
		{
		case CGVT_INT:    return m_iVal;
		case CGVT_DWORD:
		case CGVT_UID:    return (int) m_dwVal;
		case CGVT_STR:
			{
				LPCTSTR psz = (LPCTSTR)m_str;
				if ( !psz || !*psz )
					return 0;
				// Support hex (0x...) and decimal
				if ( psz[0] == '0' && (psz[1] == 'x' || psz[1] == 'X') )
					return (int) strtoul(psz, NULL, 16);
				return atoi(psz);
			}
		default:          return 0;
		}
	}

	DWORD GetDWORD() const
	{
		switch ( m_type )
		{
		case CGVT_INT:    return (DWORD) m_iVal;
		case CGVT_DWORD:
		case CGVT_UID:    return m_dwVal;
		case CGVT_STR:
			{
				LPCTSTR psz = (LPCTSTR)m_str;
				if ( !psz || !*psz )
					return 0;
				return (DWORD) strtoul(psz, NULL, 0);
			}
		default:          return 0;
		}
	}

	DWORD GetDWORDMask(DWORD flags1, DWORD flags2) const
	{
		DWORD dw = GetDWORD();
		return (dw & flags1) | (dw & flags2);
	}

	LPCTSTR GetPSTR() const
	{
		switch ( m_type )
		{
		case CGVT_STR:
			return (LPCTSTR) m_str;
		case CGVT_INT:
			// Lazy format into string buffer (cast away const -- the original API is const-incorrect)
			const_cast<CGVariant*>(this)->m_str.Format("%d", m_iVal);
			return (LPCTSTR) m_str;
		case CGVT_DWORD:
		case CGVT_UID:
			const_cast<CGVariant*>(this)->m_str.Format("0%x", m_dwVal);
			return (LPCTSTR) m_str;
		default:
			return "";
		}
	}

	CGString& GetStr() const
	{
		// Ensure string is populated
		if ( m_type != CGVT_STR )
		{
			GetPSTR();  // force conversion to string
		}
		return const_cast<CGString&>(m_str);
	}

	UID_INDEX GetUID() const
	{
		switch ( m_type )
		{
		case CGVT_UID:    return m_dwVal;
		case CGVT_DWORD:  return m_dwVal;
		case CGVT_INT:    return (UID_INDEX) m_iVal;
		case CGVT_STR:    return (UID_INDEX) strtoul((LPCTSTR)m_str, NULL, 0);
		default:          return 0;
		}
	}

	CScriptObj* GetRef() const
	{
		return (m_type == CGVT_REF) ? m_pRef : NULL;
	}

	// Comparison
	int CompareData(CGVariant& other) const
	{
		// If both are numeric, compare numerically
		if ( IsNumeric() && other.IsNumeric() )
		{
			int a = GetInt();
			int b = other.GetInt();
			return (a > b) ? 1 : (a < b) ? -1 : 0;
		}
		// Otherwise compare as strings
		return strcmp(GetPSTR(), other.GetPSTR());
	}

	// Array support -- comma-separated value parsing
	// MakeArraySize() parses a comma-separated string into sub-variants.
	// Returns the number of elements. Idempotent.
	int MakeArraySize() const
	{
		if ( m_iArrayCount > 0 )
			return m_iArrayCount;  // already parsed

		if ( m_type == CGVT_VOID || (m_type == CGVT_STR && m_str.IsEmpty()) )
			return 0;

		// Get source string -- the entire value as a string
		LPCTSTR pszSrc = GetPSTR();
		if ( !pszSrc || !*pszSrc )
			return 0;

		// Count commas to determine array size
		int iCount = 1;
		for ( LPCTSTR p = pszSrc; *p; p++ )
		{
			if ( *p == ',' )
				iCount++;
		}

		// If only one element, just set count=1 and array[0]=self
		CGVariant* pNewArray = new CGVariant[iCount];

		// Parse comma-separated values
		// We need a mutable copy of the string
		CGString sBuf(pszSrc);
		TCHAR* pBuf = const_cast<TCHAR*>((LPCTSTR) sBuf);

		int idx = 0;
		TCHAR* pStart = pBuf;
		for ( TCHAR* p = pBuf; ; p++ )
		{
			if ( *p == ',' || *p == '\0' )
			{
				bool bEnd = (*p == '\0');
				*p = '\0';

				// Trim leading whitespace
				while ( *pStart == ' ' || *pStart == '\t' )
					pStart++;

				pNewArray[idx] = CGVariant(pStart);
				idx++;

				if ( bEnd || idx >= iCount )
					break;
				pStart = p + 1;
			}
		}

		m_iArrayCount = idx;
		m_pArray = pNewArray;
		return m_iArrayCount;
	}

	CGString& GetArrayStr(int index) const
	{
		if ( index >= 0 && index < m_iArrayCount && m_pArray )
			return m_pArray[index].GetStr();
		static CGString sEmpty;
		sEmpty.Empty();
		return sEmpty;
	}

	LPCTSTR GetArrayPSTR(int index)
	{
		if ( index >= 0 && index < m_iArrayCount && m_pArray )
			return m_pArray[index].GetPSTR();
		return "";
	}

	int GetArrayInt(int index) const
	{
		if ( index >= 0 && index < m_iArrayCount && m_pArray )
			return m_pArray[index].GetInt();
		return 0;
	}

	void RemoveArrayElement(int index)
	{
		if ( !m_pArray || index < 0 || index >= m_iArrayCount )
			return;

		int iNewCount = m_iArrayCount - 1;
		if ( iNewCount <= 0 )
		{
			FreeArray();
			SetVoid();
			return;
		}

		CGVariant* pNewArray = new CGVariant[iNewCount];
		int j = 0;
		for ( int i = 0; i < m_iArrayCount; i++ )
		{
			if ( i != index )
			{
				pNewArray[j] = m_pArray[i];
				j++;
			}
		}

		delete[] m_pArray;
		m_pArray = pNewArray;
		m_iArrayCount = iNewCount;

		// Rebuild the string representation
		RebuildStrFromArray();
	}

	void SetArrayFormat(LPCTSTR format, ...)
	{
		FreeArray();
		m_type = CGVT_STR;
		va_list vargs;
		va_start(vargs, format);
		m_str.FormatV(format, vargs);
		va_end(vargs);
		// Array will be lazily parsed on next MakeArraySize() call
	}

	void SetArrayElement(int index, LPCTSTR value)
	{
		if ( index < 0 )
			return;

		// Ensure array exists
		if ( m_iArrayCount == 0 )
			MakeArraySize();

		if ( index < m_iArrayCount && m_pArray )
		{
			m_pArray[index] = CGVariant(value);
			RebuildStrFromArray();
		}
		else if ( index >= m_iArrayCount )
		{
			// Grow the array
			int iNewCount = index + 1;
			CGVariant* pNewArray = new CGVariant[iNewCount];
			for ( int i = 0; i < m_iArrayCount; i++ )
				pNewArray[i] = m_pArray[i];
			pNewArray[index] = CGVariant(value);

			FreeArray();
			m_pArray = pNewArray;
			m_iArrayCount = iNewCount;
			RebuildStrFromArray();
		}
	}

	CGVariant& GetArrayElement(int index)
	{
		if ( m_iArrayCount == 0 )
			MakeArraySize();

		if ( index >= 0 && index < m_iArrayCount && m_pArray )
			return m_pArray[index];

		// Return self for out-of-bounds (safe fallback)
		static CGVariant s_empty;
		s_empty.SetVoid();
		return s_empty;
	}

	// Assignment operators
	CGVariant& operator=(const CGVariant& other)
	{
		if ( this != &other )
			CopyFrom(other);
		return *this;
	}

	CGVariant& operator=(const CGString& str)
	{
		FreeArray();
		m_type = CGVT_STR;
		m_str = str;
		return *this;
	}

	CGVariant& operator=(LPCTSTR pszStr)
	{
		FreeArray();
		if ( pszStr )
		{
			m_type = CGVT_STR;
			m_str = pszStr;
		}
		else
		{
			m_type = CGVT_VOID;
			m_str.Empty();
		}
		return *this;
	}

	CGVariant& operator=(int val)
	{
		FreeArray();
		m_type = CGVT_INT;
		m_iVal = val;
		m_str.Empty();
		return *this;
	}

	// Conversion operators
	operator LPCTSTR()
	{
		return GetPSTR();
	}

	operator char*()
	{
		return const_cast<char*>(GetPSTR());
	}

	operator int()
	{
		return GetInt();
	}

private:
	void RebuildStrFromArray()
	{
		// Rebuild m_str from array elements as comma-separated
		if ( !m_pArray || m_iArrayCount <= 0 )
			return;
		m_str.Empty();
		for ( int i = 0; i < m_iArrayCount; i++ )
		{
			if ( i > 0 )
				m_str += ",";
			m_str += m_pArray[i].GetPSTR();
		}
		m_type = CGVT_STR;
	}
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

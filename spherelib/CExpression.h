#ifndef _INC_CEXPRESSION_H
#define _INC_CEXPRESSION_H

#include "CAtom.h"

// The historical min/max macros collide with the standard headers.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <string>
#include <unordered_map>
#pragma pop_macro("max")
#pragma pop_macro("min")

#ifndef SCRIPT_MAX_LINE_LEN
#define SCRIPT_MAX_LINE_LEN 4096
#endif

#ifndef VARTYPE
typedef int VARTYPE;
#endif

#define _ISCSYM(ch) ( isalnum(ch) || (ch)=='_')	// __iscsym or __iscsymf

#ifdef SCRIPT_MAX_SECTION_LEN
#define EXPRESSION_MAX_KEY_LEN SCRIPT_MAX_SECTION_LEN
#else
#define EXPRESSION_MAX_KEY_LEN 128
#endif

// Longest reference operand (SRC.TAG.NAME, FINDUID(uid).CONT.NAME, ...) that
// a numeric expression resolves.
#define EXPRESSION_MAX_OPERAND_LEN 512

// Read a hex literal and keep its low 32 bits, as 0.99 does: 0FFFFFFFF and
// full resource IDs such as 0A2000E76 are 32-bit patterns. strtol() would
// saturate them at 0x7FFFFFFF wherever long is 32 bits (i386).
inline int Exp_GetHexValue( const char* psz, char** ppszEnd = NULL )
{
	return (int)(unsigned int) strtoull( psz, ppszEnd, 16 );
}

// Internal type tag for CGVariant
enum CGVARIANT_TYPE
{
	CGVT_VOID = 0,		// No data
	CGVT_INT,			// Integer value
	CGVT_FIXED,		// Fixed-point value stored in tenths (script text keeps one decimal)
	CGVT_DWORD,			// Unsigned 32-bit value (flags, UIDs)
	CGVT_STR,			// String value (stored in m_str)
	CGVT_UID,			// UID_INDEX reference to game object/resource
	CGVT_REF,			// Live CScriptObj pointer for object chaining
};

class CGVariant
{
private:
	CGVARIANT_TYPE m_type;
	// When a live object reference is copied through script arguments, retain
	// the UID-slot generation that was current when the reference was taken.
	// Non-reference values remain generation-neutral.
	DWORD m_dwUIDGeneration;
	LPCTSTR m_pszUIDType;

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
		m_dwUIDGeneration = other.m_dwUIDGeneration;
		m_pszUIDType = other.m_pszUIDType;
		m_str = other.m_str;
		switch ( m_type )
		{
		case CGVT_INT:
		case CGVT_FIXED: m_iVal = other.m_iVal; break;
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
		: m_type(CGVT_VOID), m_dwUIDGeneration(0), m_pszUIDType(NULL), m_iVal(0), m_pArray(NULL), m_iArrayCount(0)
	{
	}

	CGVariant(const CGVariant& other)
		: m_type(CGVT_VOID), m_dwUIDGeneration(0), m_pszUIDType(NULL), m_iVal(0), m_pArray(NULL), m_iArrayCount(0)
	{
		CopyFrom(other);
	}

	CGVariant(const UID_INDEX uid)
		: m_type(CGVT_UID), m_dwUIDGeneration(0), m_pszUIDType(NULL), m_dwVal(uid), m_pArray(NULL), m_iArrayCount(0)
	{
	}

	CGVariant(LPCTSTR pszValue)
		: m_type(CGVT_VOID), m_dwUIDGeneration(0), m_pszUIDType(NULL), m_iVal(0), m_pArray(NULL), m_iArrayCount(0)
	{
		if ( pszValue )
		{
			m_type = CGVT_STR;
			m_str = pszValue;
		}
	}

	CGVariant(VARTYPE type, void* pData)
		: m_type(CGVT_VOID), m_dwUIDGeneration(0), m_pszUIDType(NULL), m_iVal(0), m_pArray(NULL), m_iArrayCount(0)
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
		m_dwUIDGeneration = 0;
		m_pszUIDType = NULL;
		m_dwVal = uid;
		m_str.Empty();
	}

	void SetRef(CScriptObj* val)
	{
		FreeArray();
		m_type = CGVT_REF;
		m_dwUIDGeneration = val ? val->GetUIDGeneration() : 0;
		m_pszUIDType = val ? val->GetUIDTypeName() : NULL;
		m_pRef = val;
		m_str.Empty();
	}

	void SetBool(bool val)
	{
		FreeArray();
		m_type = CGVT_INT;
		m_dwUIDGeneration = 0;
		m_pszUIDType = NULL;
		m_iVal = val ? 1 : 0;
		m_str.Empty();
	}

	void SetInt(int val)
	{
		FreeArray();
		m_type = CGVT_INT;
		m_dwUIDGeneration = 0;
		m_pszUIDType = NULL;
		m_iVal = val;
		m_str.Empty();
	}

	void SetFixed(int val)
	{
		FreeArray();
		m_type = CGVT_FIXED;
		m_dwUIDGeneration = 0;
		m_pszUIDType = NULL;
		m_iVal = val;
		m_str.Empty();
	}

	void SetDWORD(DWORD val)
	{
		FreeArray();
		m_type = CGVT_DWORD;
		m_dwUIDGeneration = 0;
		m_pszUIDType = NULL;
		m_dwVal = val;
		m_str.Empty();
	}

	void SetStr(LPCTSTR pszStr)
	{
		FreeArray();
		m_type = CGVT_STR;
		m_dwUIDGeneration = 0;
		m_pszUIDType = NULL;
		m_str = pszStr ? pszStr : "";
		m_iVal = 0;
	}

	void SetStrFormat(LPCTSTR format, ...) __printfargs(2, 3)
	{
		FreeArray();
		m_type = CGVT_STR;
		m_dwUIDGeneration = 0;
		m_pszUIDType = NULL;
		va_list vargs;
		va_start(vargs, format);
		m_str.FormatV(format, vargs);
		va_end(vargs);
	}

	void SetVoid()
	{
		FreeArray();
		m_type = CGVT_VOID;
		m_dwUIDGeneration = 0;
		m_pszUIDType = NULL;
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
		case CGVT_FIXED:
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
				if ( *psz == '0' && isxdigit((unsigned char)psz[1]) )
				{
					psz += 2;
					while ( isxdigit((unsigned char)*psz) )
						psz++;
					return *psz == '\0';
				}
				while ( *psz )
				{
					if ( !isdigit((unsigned char)*psz) )
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
		case CGVT_FIXED:  return (m_iVal != 0);
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
		case CGVT_FIXED:  return m_iVal;
		case CGVT_DWORD:
		case CGVT_UID:    return (int) m_dwVal;
		case CGVT_STR:
			{
				LPCTSTR psz = (LPCTSTR)m_str;
				if ( !psz || !*psz )
					return 0;
				// Support hex (0x...) and Sphere convention (leading 0 + hex digit = hex)
				if ( psz[0] == '0' && (psz[1] == 'x' || psz[1] == 'X') )
					return Exp_GetHexValue(psz);
				if ( psz[0] == '0' && isxdigit(psz[1]) )
					return Exp_GetHexValue(psz);
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
		case CGVT_FIXED:  return (DWORD) m_iVal;
		case CGVT_DWORD:
		case CGVT_UID:    return m_dwVal;
		case CGVT_STR:
			{
				LPCTSTR psz = (LPCTSTR)m_str;
				if ( !psz || !*psz )
					return 0;
				// Sphere treats a leading 0 followed by a hex digit as
				// hexadecimal, including values whose high bit is set.  The
				// C library's base-0 parser treats that form as octal instead.
				if ( psz[0] == '0' && (psz[1] == 'x' || psz[1] == 'X') )
					return (DWORD) Exp_GetHexValue(psz);
				if ( psz[0] == '0' && isxdigit((unsigned char)psz[1]) )
					return (DWORD) Exp_GetHexValue(psz);
				// Once the Sphere hex forms above are handled, the remaining
				// script numbers are decimal.  Do not let libc reinterpret a
				// leading zero as an octal prefix.
				return (DWORD) strtoul(psz, NULL, 10);
			}
		default:          return 0;
		}
	}

	DWORD GetDWORDMask(DWORD dwStart, DWORD dwMask) const
	{
		// Script flag argument: no value toggles the mask bits, a nonzero
		// value sets them and zero clears them. Bits outside the mask keep
		// their current state.
		if ( IsEmpty())
			return dwStart ^ dwMask;
		if ( GetDWORD() != 0 )
			return dwStart | dwMask;
		return dwStart & ~dwMask;
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
		case CGVT_FIXED:
			{
				const int iAbs = (m_iVal < 0) ? -m_iVal : m_iVal;
				const int iWhole = iAbs / 10;
				const int iFraction = iAbs % 10;
				const_cast<CGVariant*>(this)->m_str.Format(
					"%s%d.%d", m_iVal < 0 ? "-" : "", iWhole, iFraction);
				return (LPCTSTR) m_str;
			}
		case CGVT_DWORD:
		case CGVT_UID:
			const_cast<CGVariant*>(this)->m_str.Format("0%x", m_dwVal);
			return (LPCTSTR) m_str;
		case CGVT_REF:
			// Reference-valued properties keep their live object for dotted
			// chaining, but selected reference types may provide a scalar spelling.
			// CProfessionDef uses DEFNAME here while .NAME remains the display
			// property; ordinary object references keep their historical empty value.
			if ( m_pRef == NULL )
				return "";
			const_cast<CGVariant*>(this)->m_str = m_pRef->GetScriptRefName();
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
		case CGVT_FIXED:  return (UID_INDEX) m_iVal;
		case CGVT_STR:
			{
				LPCTSTR psz = (LPCTSTR)m_str;
				if ( !psz || !*psz )
					return 0;
				int iBase = 10;
				if ( psz[0] == '0' && (psz[1] == 'x' || psz[1] == 'X') )
					iBase = 16;
				else if ( psz[0] == '0' && isxdigit((unsigned char)psz[1]) )
					iBase = 16;
				return (UID_INDEX) strtoul(psz, NULL, iBase);
			}
		default:          return 0;
		}
	}

	CScriptObj* GetRef() const
	{
		return (m_type == CGVT_REF) ? m_pRef : NULL;
	}

	DWORD GetUIDGeneration() const { return m_dwUIDGeneration; }
	LPCTSTR GetUIDTypeName() const { return m_pszUIDType ? m_pszUIDType : "<none>"; }

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

		// Count argument separators outside quoted strings.  Legacy script
		// helpers pass speech text through ARGV, and commas in that text are
		// data rather than argument boundaries.
		int iCount = 1;
		bool fQuoted = false;
		for ( LPCTSTR p = pszSrc; *p; p++ )
		{
			if ( *p == '"' && (p == pszSrc || p[-1] != '\\') )
				fQuoted = !fQuoted;
			else if ( *p == ',' && !fQuoted )
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
		fQuoted = false;
		for ( TCHAR* p = pBuf; ; p++ )
		{
			if ( *p == '"' && (p == pBuf || p[-1] != '\\') )
				fQuoted = !fQuoted;
			if ( (*p == ',' && !fQuoted) || *p == '\0' )
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

	void SetArrayFormat(LPCTSTR format, ...) __printfargs(2, 3)
	{
		FreeArray();
		m_type = CGVT_STR;
		m_dwUIDGeneration = 0;
		m_pszUIDType = NULL;
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
		m_dwUIDGeneration = 0;
		m_pszUIDType = NULL;
		m_str = str;
		return *this;
	}

	CGVariant& operator=(LPCTSTR pszStr)
	{
		FreeArray();
		m_dwUIDGeneration = 0;
		m_pszUIDType = NULL;
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
		m_dwUIDGeneration = 0;
		m_pszUIDType = NULL;
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
		m_dwUIDGeneration = 0;
		m_pszUIDType = NULL;
	}
};

#define CVarDefPtr CVarDef*
class CVarDef : public CMemDynamic	// A variable from GRAYDEFS.SCP or other.
{
	// Similar to CScriptKey
private:
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
	virtual char* GetPSTR() const { return const_cast<char*>(GetValStr()); }
	virtual int GetValNum() const = 0;
	virtual CVarDef* CopySelf() const = 0;
};

// String variable
class CVarDefStr : public CVarDef
{
	CGString m_sVal;
	bool m_fQuoted;
public:
	CVarDefStr(LPCTSTR pszKey, LPCTSTR pszVal, bool fQuoted = false) : CVarDef(pszKey), m_sVal(pszVal), m_fQuoted(fQuoted) {}
	LPCTSTR GetValStr() const { return m_sVal; }
	int GetValNum() const {
		LPCTSTR psz = (LPCTSTR)m_sVal;
		if (!psz || !*psz) return 0;
		// Sphere 0.99 convention: values starting with 0 followed by hex digit are hex
		if (psz[0] == '0' && (psz[1] == 'x' || psz[1] == 'X'))
			return Exp_GetHexValue(psz);
		if (psz[0] == '0' && isxdigit(psz[1]))
			return Exp_GetHexValue(psz);
		return atoi(psz);
	}
	void SetValStr(LPCTSTR pszVal) { m_sVal = pszVal; }
	bool IsQuoted() const { return m_fQuoted; }
	void SetQuoted(bool fQuoted) { m_fQuoted = fQuoted; }
	CVarDef* CopySelf() const { return new CVarDefStr(GetKey(), m_sVal, m_fQuoted); }
};

// Numeric variable
class CVarDefNum : public CVarDef
{
	int m_iVal;
	mutable TCHAR m_szTemp[32];
public:
	CVarDefNum(LPCTSTR pszKey, int iVal) : CVarDef(pszKey), m_iVal(iVal) { m_szTemp[0] = '\0'; }
	LPCTSTR GetValStr() const { snprintf(const_cast<char*>(m_szTemp), sizeof(m_szTemp), "%d", m_iVal); return m_szTemp; }
	int GetValNum() const { return m_iVal; }
	void SetValNum(int iVal) { m_iVal = iVal; }
	CVarDef* CopySelf() const { return new CVarDefNum(GetKey(), m_iVal); }
};

class CScript;
class CScriptConsole;
class CExpression;
CExpression* Exp_GetContext();
int CVarDefEvaluateExpression(LPCTSTR pszExpression);

struct CVarDefArray : public CGSortedArray<CVarDef*, CVarDef*, LPCTSTR>
{
protected:
	int CompareKey(LPCTSTR pszKey, CVarDef* pVar) const
	{
		return _stricmp(pszKey, pVar->GetKey());
	}
	int Add(CVarDef* pVar)
	{
		int i = this->GetSize();
		this->SetAtGrow(i, pVar);
		OnKeyAdded(pVar);
		return i;
	}
	// A derived array can keep an index of its keys.  These hooks report
	// every change of the key set that is made through this class.
	virtual void OnKeyAdded(CVarDef* pVar) { (void)pVar; }
	virtual void OnKeyRemoved(CVarDef* pVar) { (void)pVar; }
	virtual void OnKeysRemoved() {}
	CVarDefPtr FindKeyPtrLinear(LPCTSTR pszKey) const
	{
		for (int i = 0; i < (int)this->GetSize(); i++)
		{
			if (_stricmp(this->GetAt(i)->GetKey(), pszKey) == 0)
				return this->GetAt(i);
		}
		return NULL;
	}
public:
	virtual CVarDefPtr FindKeyPtr(LPCTSTR pszKey) const
	{
		return FindKeyPtrLinear(pszKey);
	}
	int SetKeyStr(LPCTSTR pszKey, LPCTSTR pszVal)
	{
		return SetKeyStr(pszKey, pszVal, false);
	}
	int SetKeyStr(LPCTSTR pszKey, LPCTSTR pszVal, bool fQuoted)
	{
		CVarDef* pVar = FindKeyPtr(pszKey);
		if (pVar)
		{
			CVarDefStr* pStr = dynamic_cast<CVarDefStr*>(pVar);
			if (pStr) { pStr->SetValStr(pszVal); pStr->SetQuoted(fQuoted); return 0; }
			// Type mismatch — remove old, add new
			RemoveKey(pszKey);
		}
		return Add(new CVarDefStr(pszKey, pszVal, fQuoted));
	}
	void SetKeyInt(LPCTSTR pszKey, DWORD dwVal)
	{
		CVarDef* pVar = FindKeyPtr(pszKey);
		if (pVar)
		{
			CVarDefNum* pNum = dynamic_cast<CVarDefNum*>(pVar);
			if (pNum) { pNum->SetValNum((int)dwVal); return; }
			RemoveKey(pszKey);
		}
		Add(new CVarDefNum(pszKey, (int)dwVal));
	}
	int SetKeyVar(LPCTSTR pszKey, const CGVariant& val)
	{
		if (val.IsNumeric())
		{
			SetKeyInt(pszKey, (DWORD)const_cast<CGVariant&>(val).GetInt());
			return 0;
		}
		return SetKeyStr(pszKey, (LPCTSTR)const_cast<CGVariant&>(val));
	}
	CGVariant FindKeyVar(LPCTSTR pszKey) const
	{
		CVarDef* pVar = FindKeyPtr(pszKey);
		if (!pVar)
			return CGVariant();
		CGVariant v;
		v = pVar->GetValStr();
		return v;
	}
	bool FindKeyVar(LPCTSTR pszKey, CGVariant& val)
	{
		CVarDef* pVar = FindKeyPtr(pszKey);
		if (!pVar)
			return false;
		val = pVar->GetValStr();
		return true;
	}
	CGString FindKeyStr(LPCTSTR pszKey) const
	{
		CVarDef* pVar = FindKeyPtr(pszKey);
		return pVar ? CGString(pVar->GetValStr()) : CGString();
	}
	DWORD FindKeyInt(LPCTSTR pszKey) const
	{
		CVarDef* pVar = FindKeyPtr(pszKey);
		return pVar ? (DWORD)pVar->GetValNum() : 0;
	}
	bool SetKeyCurrentValue(LPCTSTR pszKey, LPCTSTR pszVal)
	{
		// Sphere 0.99 uses a leading '#' for current-value arithmetic only
		// when it is immediately followed by an operator.  Other forms such
		// as #0DE97 and #<expression> are literal string values.
		if ( pszVal == NULL || pszVal[0] != '#' ||
			strchr("+-*/%|&^", pszVal[1]) == NULL )
			return false;

		CGVariant vCurrent;
		FindKeyVar( pszKey, vCurrent );
		TCHAR szExpression[SCRIPT_MAX_LINE_LEN];
		snprintf( szExpression, sizeof(szExpression), "%d%s",
			vCurrent.GetInt(), pszVal + 1 );
		SetKeyInt( pszKey, (DWORD)CVarDefEvaluateExpression( szExpression ));
		return true;
	}
	void RemoveKey(LPCTSTR pszKey)
	{
		for (int i = 0; i < (int)this->GetSize(); i++)
		{
			if (_stricmp(this->GetAt(i)->GetKey(), pszKey) == 0)
			{
				CVarDef* pVar = this->GetAt(i);
				this->RemoveAt(i);
				delete pVar;
				return;
			}
		}
	}
	// Removals by position and of the whole content also change the key set.
	void RemoveAt(size_t nIndex)
	{
		if (!this->IsValidIndex(nIndex))
			return;
		CVarDef* pVar = this->GetAt(nIndex);
		CGSortedArray<CVarDef*, CVarDef*, LPCTSTR>::RemoveAt(nIndex);
		if (pVar)
			OnKeyRemoved(pVar);
	}
	void RemoveAll()
	{
		CGSortedArray<CVarDef*, CVarDef*, LPCTSTR>::RemoveAll();
		OnKeysRemoved();
	}
	void Empty()
	{
		RemoveAll();
	}
	void Copy(const CVarDefArray* pArray)
	{
		this->RemoveAll();
		if (!pArray) return;
		for (int i = 0; i < (int)pArray->GetSize(); i++)
			Add(pArray->GetAt(i)->CopySelf());
	}
	bool AddHtmlArgs(LPCTSTR pszName, TCHAR** pArgs = NULL) { return false; /* STUB */ }
	HRESULT s_PropSetTags(CGVariant& vVal)
	{
		// Set a tag value. vVal format: "tagname value" or "tagname=value"
		// The value was transformed by s_FixExtendedProp to be "tagname value"
		LPCTSTR pszStr = vVal.GetPSTR();
		if ( pszStr == NULL || *pszStr == '\0' )
			return HRES_BAD_ARGUMENTS;
		// Extract the tag key name
		// The value comes from the complete script line.  A 128-byte key-sized
		// buffer silently truncated long unquoted legacy TAG values on load.
		TCHAR szTemp[SCRIPT_MAX_LINE_LEN];
		strncpy(szTemp, pszStr, sizeof(szTemp)-1);
		szTemp[sizeof(szTemp)-1] = '\0';
		// Split at first space or '='
		TCHAR* pszVal = szTemp;
		while ( *pszVal && *pszVal != ' ' && *pszVal != '\t' && *pszVal != '=' )
			pszVal++;
		if ( *pszVal )
		{
			*pszVal++ = '\0';
			// skip whitespace and '='
			while ( *pszVal && ( *pszVal == ' ' || *pszVal == '\t' || *pszVal == '=' ) )
				pszVal++;
		}
		if ( szTemp[0] == '\0' )
			return HRES_BAD_ARGUMENTS;
		// Set the var
		if ( pszVal[0] == '\0' )
		{
			// Delete the tag
			RemoveKey(szTemp);
		}
		else if ( ! SetKeyCurrentValue( szTemp, pszVal ) )
		{
			if ( pszVal[0] == '"' )
			{
				bool fQuoted = true;
				// String value - strip quotes
				pszVal++;
				int iLen = strlen(pszVal);
				if ( iLen > 0 && pszVal[iLen-1] == '"' )
					pszVal[iLen-1] = '\0';
				SetKeyStr(szTemp, pszVal, fQuoted);
			}
			else
				SetKeyStr(szTemp, pszVal);
		}
		return NO_ERROR;
	}
	HRESULT s_MethodTags(CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc)
	{
		// Handle TAG method calls - get/set/delete tags.
		// vArgs = "tagname" for get, "tagname value" for set
		LPCTSTR pszStr = vArgs.GetPSTR();
		if ( pszStr == NULL || *pszStr == '\0' )
			return HRES_BAD_ARGUMENTS;
		TCHAR szTemp[SCRIPT_MAX_LINE_LEN];
		strncpy(szTemp, pszStr, sizeof(szTemp)-1);
		szTemp[sizeof(szTemp)-1] = '\0';
		// Split at first separator
		TCHAR* pszVal = szTemp;
		while ( *pszVal && *pszVal != ' ' && *pszVal != '\t' && *pszVal != ',' && *pszVal != '=' )
			pszVal++;
		if ( *pszVal )
		{
			*pszVal++ = '\0';
			while ( *pszVal && ( *pszVal == ' ' || *pszVal == '\t' || *pszVal == ',' || *pszVal == '=' ) )
				pszVal++;
		}
		if ( *pszVal )
		{
			if ( ! SetKeyCurrentValue( szTemp, pszVal ))
			{
				// Call-form VAR(name,"text") uses quotes as delimiters.  Keep
				// the stored value text-only so a later <safe VAR(name)> read
				// cannot feed the delimiters back into a byte-token stream.
				if ( pszVal[0] == '"' )
				{
					pszVal++;
					int iLen = strlen(pszVal);
					if ( iLen > 0 && pszVal[iLen - 1] == '"' )
						pszVal[iLen - 1] = '\0';
				}
				SetKeyStr(szTemp, pszVal);
			}
		}
		else
		{
			// Get mode
			CVarDefPtr pVar = FindKeyPtr(szTemp);
			if ( pVar )
			{
				vValRet.SetStr(pVar->GetValStr());
			}
			else
			{
				vValRet.SetStr("");
			}
		}
		return NO_ERROR;
	}
	void s_WriteTags(CScript& script, LPCTSTR pszName = NULL); // implemented in stubs.cpp

	CVarDefArray& operator = (const CVarDefArray& array)
	{
		Copy(&array);
		return(*this);
	}
};

// A CVarDefArray with a hash index over its keys.  The element order is
// unchanged (it is the order the table is written in); only the lookup by
// key stops being a scan of the whole table.  Used for the tables that are
// read for every identifier a script evaluates.
struct CVarDefIndexedArray : public CVarDefArray
{
private:
	std::unordered_map<std::string, CVarDef*> m_Index;

	// Keys compare without case, as in the linear search.
	static std::string IndexKey(LPCTSTR pszKey)
	{
		std::string sKey(pszKey ? pszKey : "");
		for (size_t i = 0; i < sKey.size(); i++)
			sKey[i] = (char)tolower((unsigned char)sKey[i]);
		return sKey;
	}
protected:
	virtual void OnKeyAdded(CVarDef* pVar)
	{
		// The first entry of a key stays the one that is found.
		m_Index.insert(std::make_pair(IndexKey(pVar->GetKey()), pVar));
	}
	virtual void OnKeyRemoved(CVarDef* pVar)
	{
		std::unordered_map<std::string, CVarDef*>::iterator it = m_Index.find(IndexKey(pVar->GetKey()));
		if (it == m_Index.end() || it->second != pVar)
			return;
		m_Index.erase(it);
		// A later entry with the same key becomes the one that is found.
		CVarDef* pNext = FindKeyPtrLinear(pVar->GetKey());
		if (pNext)
			m_Index.insert(std::make_pair(IndexKey(pNext->GetKey()), pNext));
	}
	virtual void OnKeysRemoved()
	{
		m_Index.clear();
	}
public:
	virtual CVarDefPtr FindKeyPtr(LPCTSTR pszKey) const
	{
		std::unordered_map<std::string, CVarDef*>::const_iterator it = m_Index.find(IndexKey(pszKey));
		return it == m_Index.end() ? NULL : it->second;
	}
	CVarDefIndexedArray& operator = (const CVarDefArray& array)
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
	// Function pointer for resolving DEFNAME identifiers (set by server at startup)
	typedef int (*DEFNAME_RESOLVER)(LPCTSTR pszName);
	static DEFNAME_RESOLVER sm_fnResolveDefName;

	// Basic number parsing - handles decimal, hex (0x), octal (0), and DEFNAME identifiers
	static int GetSingle(LPCTSTR& pStr)
	{
		if (!pStr || !*pStr) return 0;
		// Skip whitespace
		while (ISWHITESPACE(*pStr)) pStr++;
		if (!*pStr) return 0;

		// Handle hex
		if (pStr[0] == '0' && (pStr[1] == 'x' || pStr[1] == 'X'))
			return Exp_GetHexValue(pStr, (char**)&pStr);
		// Handle negative
		bool fNeg = false;
		if (*pStr == '-') { fNeg = true; pStr++; }
		else if (*pStr == '+') { pStr++; }
		// Handle hex without 0x prefix — Sphere 0.99 convention: 0xxx values are hex
		if (*pStr == '0' && isxdigit(pStr[1]))
			return Exp_GetHexValue(pStr, (char**)&pStr);
		// Handle DEFNAME identifiers (starts with letter or underscore)
		if (isalpha(*pStr) || *pStr == '_')
		{
			// Extract the identifier
			LPCTSTR pStart = pStr;
			while (isalnum(*pStr) || *pStr == '_') pStr++;
			if (sm_fnResolveDefName && pStr > pStart)
			{
				char szName[256];
				int iLen = pStr - pStart;
				if (iLen >= (int)sizeof(szName)) iLen = sizeof(szName)-1;
				memcpy(szName, pStart, iLen);
				szName[iLen] = '\0';
				int val = sm_fnResolveDefName(szName);
				return fNeg ? -val : val;
			}
			return 0;
		}
		// Decimal number. A '.' is skipped, not a fraction: skill values are
		// tenths, so "30.0" is 300 and "30.5" is 305 (same as stock Sphere).
		int val = 0;
		for (;; pStr++)
		{
			if (*pStr == '.')
				continue;
			if (!isdigit(*pStr))
				break;
			val = val * 10 + (*pStr - '0');
		}
		return fNeg ? -val : val;
	}

	// Resolve a reference operand such as SRC.STR or FINDUID(uid).NAME.
	// The plain reader knows no objects; script execution contexts override
	// this.  Return false to read the operand as a number or DEFNAME.
	virtual bool ResolveReferenceOperand(LPCTSTR pszOperand, int& iValue)
	{
		(void)pszOperand;
		iValue = 0;
		return false;
	}

	// Read one operand.  An identifier followed by '.' or '(' is a reference
	// operand: it extends over names, dots and balanced parentheses up to the
	// first operator, whitespace or unmatched ')' and is resolved through
	// ResolveReferenceOperand().  Everything else is read by GetSingle().
	int GetOperand(LPCTSTR& pStr)
	{
		if (!pStr) return 0;
		LPCTSTR p = pStr;
		while (ISWHITESPACE(*p)) p++;
		bool fNeg = false;
		if (*p == '-' || *p == '+')
		{
			fNeg = (*p == '-');
			p++;
		}
		if (isalpha((unsigned char)*p) || *p == '_')
		{
			LPCTSTR pName = p;
			while (isalnum((unsigned char)*pName) || *pName == '_') pName++;
			if (*pName == '.' || *pName == '(')
			{
				int iDepth = 0;
				LPCTSTR pEnd = pName;
				for (; *pEnd; pEnd++)
				{
					if (*pEnd == '(')
						iDepth++;
					else if (*pEnd == ')')
					{
						if (iDepth == 0)
							break;
						iDepth--;
					}
					else if (iDepth == 0 && !isalnum((unsigned char)*pEnd) && *pEnd != '_' && *pEnd != '.')
						break;
				}
				char szOperand[EXPRESSION_MAX_OPERAND_LEN];
				size_t iLen = pEnd - p;
				if (iDepth == 0 && iLen < sizeof(szOperand))
				{
					memcpy(szOperand, p, iLen);
					szOperand[iLen] = '\0';
					int iValue = 0;
					if (ResolveReferenceOperand(szOperand, iValue))
					{
						pStr = pEnd;
						return fNeg ? -iValue : iValue;
					}
				}
			}
			else
			{
				// A named ARG local is a valid numeric operand even without a
				// dotted reference suffix. Let the execution context resolve it;
				// unresolved names still fall through to DEFNAME parsing below.
				char szOperand[EXPRESSION_MAX_OPERAND_LEN];
				size_t iLen = pName - p;
				if ( iLen < sizeof(szOperand) )
				{
					memcpy(szOperand, p, iLen);
					szOperand[iLen] = '\0';
					int iValue = 0;
					if ( ResolveReferenceOperand(szOperand, iValue) )
					{
						pStr = pName;
						return fNeg ? -iValue : iValue;
					}
				}
			}
		}
		return GetSingle(pStr);
	}

	// Read one primary: a parenthesized sub-expression, a unary ! applied to
	// one primary, or an operand (GetOperand).
	int GetPrimary(LPCTSTR& pStr)
	{
		if (!pStr) return 0;
		LPCTSTR p = pStr;
		while (ISWHITESPACE(*p)) p++;
		if (*p == '(')
		{
			p++;
			int val = GetComplexAdvance(p);
			while (ISWHITESPACE(*p)) p++;
			if (*p == ')')
				p++;
			pStr = p;
			return val;
		}
		if (*p == '!' && p[1] != '=')
		{
			p++;
			int val = GetPrimary(p);
			pStr = p;
			return !val;
		}
		return GetOperand(pStr);
	}

	struct OperatorToken
	{
		char op;
		int length;
		bool fCompareEqual;
		bool fShift;
		bool fLogical;
	};

	// Read one binary operator.  Sphere's expression grammar has no operator
	// precedence, so the distinction between a bitwise and logical pair is
	// needed only when applying the token, not when building the chain.
	static bool GetOperatorToken(LPCTSTR pStr, OperatorToken& token)
	{
		if (!pStr || !*pStr || !strchr("+-*/%|&^<>!=", *pStr))
			return false;

		token.op = *pStr;
		token.length = 1;
		token.fCompareEqual = false;
		token.fShift = false;
		token.fLogical = false;
		if (token.op == '!' && pStr[1] != '=')
			return false;
		if ((token.op == '&' || token.op == '|') && pStr[1] == token.op)
		{
			token.length = 2;
			token.fLogical = true;
		}
		else if ((token.op == '>' || token.op == '<') && pStr[1] == token.op)
		{
			token.length = 2;
			token.fShift = true;
		}
		else if ((token.op == '>' || token.op == '<') && pStr[1] == '=')
		{
			token.length = 2;
			token.fCompareEqual = true;
		}
		else if ((token.op == '!' || token.op == '=') && pStr[1] == '=')
		{
			token.length = 2;
		}
		return true;
	}

	static int ApplyOperator(const OperatorToken& token, int left, int right)
	{
		// Sphere stores expression results in a 32-bit slot.  Convert through
		// unsigned bits so the legacy wrap is explicit instead of relying on
		// signed-overflow behavior (which UBSan correctly rejects).
		auto WrapInt32 = [](int64_t value) -> int
		{
			const uint32_t bits = static_cast<uint32_t>(value);
			if (bits & 0x80000000U)
				return static_cast<int>(static_cast<int64_t>(bits) - 0x100000000LL);
			return static_cast<int>(bits);
		};
		if (token.fLogical)
			return token.op == '&' ? ((left && right) ? 1 : 0) : ((left || right) ? 1 : 0);
		if (token.fShift)
			return token.op == '<' ? (left << right) : (left >> right);
		switch (token.op)
		{
		case '+': return WrapInt32(static_cast<int64_t>(left) + right);
		case '-': return WrapInt32(static_cast<int64_t>(left) - right);
		case '*': return WrapInt32(static_cast<int64_t>(left) * right);
		case '/': return right ? WrapInt32(static_cast<int64_t>(left) / right) : 0;
		case '%': return right ? WrapInt32(static_cast<int64_t>(left) % right) : 0;
		case '|': return left | right;
		case '&': return left & right;
		case '^': return left ^ right;
		case '>': return token.fCompareEqual ? (left >= right) : (left > right);
		case '<': return token.fCompareEqual ? (left <= right) : (left < right);
		case '!': return left != right;
		case '=': return left == right;
		default: return left;
		}
	}

	// Evaluate one unparenthesized chain from right to left.  This is the
	// 0.99 grammar: every binary operator has the same precedence and the
	// right operand is itself the remainder of the chain.  Parentheses recurse
	// through GetPrimary/GetComplexAdvance and therefore remain explicit groups.
	int GetOperatorChain(LPCTSTR& pStr)
	{
		if (!pStr || !*pStr) return 0;
		int left = GetPrimary(pStr);
		LPCTSTR pOp = pStr;
		while (ISWHITESPACE(*pOp)) pOp++;

		OperatorToken token;
		if (!GetOperatorToken(pOp, token))
		{
			pStr = pOp;
			return left;
		}

		LPCTSTR pRight = pOp + token.length;
		int right = GetOperatorChain(pRight);
		pStr = pRight;
		return ApplyOperator(token, left, right);
	}

	// Kept as named entry points for callers and older subclasses.  Logical
	// operators are part of the same chain, so they no longer introduce a
	// separate precedence level.
	int GetLogicalAnd(LPCTSTR& pStr) { return GetOperatorChain(pStr); }

	int GetComplexAdvance(LPCTSTR& pStr)
	{
		return GetOperatorChain(pStr);
	}

	int GetComplex(LPCTSTR pStr) { return GetComplexAdvance(pStr); }

	// The *Ref variants advance the caller's pointer past the parsed
	// expression, so "7 i_gold" yields 7 and leaves "i_gold" to parse next.
	int GetComplexRef(LPCTSTR& pStr) { return GetComplexAdvance(pStr); }
	int GetValue(LPCTSTR pStr) { return GetComplex(pStr); }
	int GetValueRef(LPCTSTR& pStr) { return GetComplexAdvance(pStr); }

	// Copy the identifier at the start of pszArgs into szTag; return its length.
	int GetIdentifierString(TCHAR* szTag, LPCTSTR pszArgs)
	{
		int i = 0;
		if (pszArgs)
		{
			for (; pszArgs[i] && (isalnum((unsigned char)pszArgs[i]) || pszArgs[i] == '_'); i++)
			{
				if (i >= EXPRESSION_MAX_KEY_LEN - 1)
					break;
				szTag[i] = pszArgs[i];
			}
		}
		szTag[i] = '\0';
		return i;
	}

	bool IsSimpleNumberString(LPCTSTR pStr)
	{
		if (!pStr || !*pStr) return false;
		if (*pStr == '-' || *pStr == '+') pStr++;
		if (*pStr == '0' && (pStr[1] == 'x' || pStr[1] == 'X')) return true;
		return isdigit(*pStr) != 0;
	}

	int ParseCmds(LPCTSTR pszStr, int* pArgs, int iCnt)
	{
		// Parse comma-separated list of integers
		int i = 0;
		while (i < iCnt && pszStr && *pszStr)
		{
			while (ISWHITESPACE(*pszStr)) pszStr++;
			pArgs[i++] = GetSingle(pszStr);
			while (*pszStr && *pszStr != ',') pszStr++;
			if (*pszStr == ',') pszStr++;
		}
		return i;
	}
};

inline int CVarDefEvaluateExpression(LPCTSTR pszExpression)
{
	return Exp_GetContext()->GetComplex(pszExpression);
}

inline int Calc_GetRandVal(int iqty) { if (iqty <= 0) return 0; return rand() % iqty; }
inline int Calc_GetLog2(int iNum) { int i = 0; while (iNum > 1) { iNum >>= 1; i++; } return i; }
inline int Calc_GetSCurve(int iValDiff, int iVariance)
{
	// An S-curve for probability. iValDiff = how far IsFrom target. iVariance = total range.
	// Return: 0 = very unlikely, 50 = 50/50, 100 = very likely
	if ( iVariance <= 0 )
		return ( iValDiff >= 0 ) ? 100 : 0;
	int iVal = 50 + IMULDIV(iValDiff, 50, iVariance);
	if ( iVal < 0 ) iVal = 0;
	if ( iVal > 100 ) iVal = 100;
	return iVal;
}
inline int Calc_GetBellCurve(int iValDiff, int iVariance)
{
	// A bell-curve for probability. iValDiff = how far from center. iVariance = std deviation.
	// Return: 0 = very unlikely, 1000 = very likely (at center)
	if ( iVariance <= 0 )
		return ( iValDiff == 0 ) ? 1000 : 0;
	if ( iValDiff < 0 ) iValDiff = -iValDiff;
	if ( iValDiff > iVariance * 4 )
		return 0;
	// Simple approximation: linear falloff
	int iVal = 1000 - IMULDIV(iValDiff, 1000, iVariance);
	if ( iVal < 0 ) iVal = 0;
	return iVal;
}

#endif // _INC_CEXPRESSION_H

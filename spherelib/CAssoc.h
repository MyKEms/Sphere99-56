#ifndef _INC_CASSOC_H
#define _INC_CASSOC_H

struct CVarDefRegElem
{
	int m_type;
	size_t m_offset;

	bool SetVal(void* pBase, CGVariant& vVal) const { return true; } // STUB
	bool GetVal(const void* pBase, CGVariant& vValRet) const { return true; } // STUB
	bool CompareVal(const void* pBase, const void* pDefaults) const { return true; } // STUB
};

struct CAssocReg     // associate members of some class/structure with entries in the registry.
{
	LPCTSTR m_pszName;
	CVarDefRegElem m_elem;
};

#endif // _INC_CASSOC_H

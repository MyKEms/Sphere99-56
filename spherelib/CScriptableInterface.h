#ifndef _INC_CSCRIPTABLEINTERFACE_H
#define _INC_CSCRIPTABLEINTERFACE_H

#define CSCRIPTPROP_RETNUL	0x0001
#define CSCRIPTPROP_RETREF	0x0002
#define CSCRIPTPROP_RETVAL	0x0004
#define CSCRIPTPROP_ARG1	0x0008
#define CSCRIPTPROP_ARG1S	0x0010
#define CSCRIPTPROP_ARG2	0x0020
#define CSCRIPTPROP_ARG3    0x0040
#define CSCRIPTPROP_NARG1	0x0080
#define CSCRIPTPROP_NARG2	0x0100
#define CSCRIPTPROP_ARG5    0x0200
#define CSCRIPTPROP_NARGS	0x0400
#define CSCRIPTPROP_UNUSED	0x0800
#define CSCRIPTPROP_DUPE	0x1000
#define CSCRIPTPROP_READO	0x2000
#define CSCRIPTPROP_WRITEO	0x4000

#define VARTYPE_BOOL		0
#define VARTYPE_CSTRING		1
#define VARTYPE_INT			2
#define VARTYPE_LPSTR		3
#define VARTYPE_LPCTSTR		4
#define VARTYPE_UID			5
#define VARTYPE_VOID		6
#define VARTYPE_WORD		7

#define DECLARE_LISTREC_REF(a)
#define DECLARE_LISTREC_REF2(a)
#define CSCRIPT_EXEC_DEF void Exec
#define CSCRIPT_CLASS_IMP0(a,b,c)
#define DECLARE_LISTREC_TYPE(a)
#define CSCRIPT_PROP_IMP(a,b,c) { #a, b, c },
#define CSCRIPT_PROPX_IMP(a,b,c) CScriptPropX(#a, b, c),
#define CSCRIPT_METHOD_IMP(a,b,c) { #a, b, c },

#define CSCRIPT_CLASS_DEF1(...) \
	int s_FindMyPropKey(LPCTSTR pszKey); \
	int s_FindMyMethodKey(LPCTSTR pszKey); \
	static CScriptClass sm_ScriptClass;

#define CSCRIPT_CLASS_IMP1(CLASS_NAME,b,c,d,e) \
	int C##CLASS_NAME::s_FindMyPropKey(LPCTSTR pszKey) { \
		const CScriptProp* _tbl = (b); \
		return _tbl ? s_FindKeyInTable(pszKey, _tbl) : -1; \
	} \
	int C##CLASS_NAME::s_FindMyMethodKey(LPCTSTR pszKey) { \
		const CScriptMethod* _tbl = (c); \
		return _tbl ? s_FindKeyInTable(pszKey, _tbl) : -1; \
	}

#define CSCRIPT_CLASS_DEF2(...) \
	int s_FindMyPropKey(LPCTSTR pszKey); \
	int s_FindMyMethodKey(LPCTSTR pszKey);

#define CSCRIPT_CLASS_IMP2(CLASS_NAME,b,c,d,e) \
	int C##CLASS_NAME::s_FindMyPropKey(LPCTSTR pszKey) { \
		const CScriptProp* _tbl = (b); \
		return _tbl ? s_FindKeyInTable(pszKey, _tbl) : -1; \
	} \
	int C##CLASS_NAME::s_FindMyMethodKey(LPCTSTR pszKey) { \
		const CScriptMethod* _tbl = (c); \
		return _tbl ? s_FindKeyInTable(pszKey, _tbl) : -1; \
	}


class CScriptClass
{
public:
	void InitScriptClass() { /* no-op for now -- script class registration deferred */ }
	void AddSubClass(CScriptClass* pSubClass) { /* no-op for now */ }
};

template <class TYPE>
class CScriptClassTemplate : public CScriptClass
{
private:
	bool m_fInit;
public:
	CScriptClassTemplate() : m_fInit(false) {}
	virtual void InitScriptClass() { m_fInit = true; }
	bool IsInit() { return m_fInit; }
};

#endif // _INC_CSCRIPTABLEINTERFACE_H

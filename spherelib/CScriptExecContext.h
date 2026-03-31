#ifndef _INC_CSCRIPTEXECCONTEXT_H
#define _INC_CSCRIPTEXECCONTEXT_H

class CScriptExecContext : public CExpression
{
public:
	static CScriptPropArray sm_FunctionsAll;

	static void InitFunctions() { throw "not implemented"; }

public:
	CGVariant m_vValRet;
	CVarDefArray m_ArgArray;

	CScriptExecContext(CScriptObj *pObj, CScriptConsole* pConsole) { throw "not implemented"; }

public:
	virtual HRESULT Function_Dispatch(LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet) { throw "not implemented"; }
	void SetBaseObject(CScriptObj* pObj) { throw "not implemented"; }
	CScriptObj* GetBaseObject() const { throw "not implemented"; }
	CScriptConsole* GetSrc() const { throw "not implemented"; }
	void s_ParseEscapes(LPCTSTR pszBuf, DWORD dwFlags) { throw "not implemented"; }
	HRESULT ExecuteCommand(LPCTSTR pszCmd) { throw "not implemented"; }
	TRIGRET_TYPE ExecuteScript(CScript& script, TRIGRUN_TYPE type) { throw "not implemented"; }
};

#endif // _INC_CSCRIPTEXECCONTEXT_H

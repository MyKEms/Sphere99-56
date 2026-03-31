#ifndef _INC_CSCRIPTCONSOLE_H
#define _INC_CSCRIPTCONSOLE_H

class CStreamText
{
public:
	void Printf(LPCTSTR lpszFormat, ...) { throw "not implemented"; }
};

// aka CTextConsole
class CScriptConsole : public CStreamText
{
public:
	virtual int GetPrivLevel() const { throw "not implemented"; };
	virtual CGString GetName() const { throw "not implemented"; }	// ( every object must have at least a type name )

	void WriteString(LPCTSTR pszStr) { throw "not implemented"; }
	CScriptObj* GetAttachedObj() const { throw "not implemented"; }
	int AddConsoleKey(LPCTSTR pszKey, BYTE bVal, bool bEcho) { throw "not implemented"; }
};

#endif // _INC_CSCRIPTCONSOLE_H

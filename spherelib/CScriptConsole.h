#ifndef _INC_CSCRIPTCONSOLE_H
#define _INC_CSCRIPTCONSOLE_H

class CStreamText
{
public:
	void Printf(LPCTSTR lpszFormat, ...) { /* no-op for now */ }
};

// aka CTextConsole
class CScriptConsole : public CStreamText
{
public:
	virtual int GetPrivLevel() const { return 0; }
	virtual CGString GetName() const { return CGString("console"); }

	void WriteString(LPCTSTR pszStr) { /* no-op for now */ }
	CScriptObj* GetAttachedObj() const { return NULL; }
	int AddConsoleKey(LPCTSTR pszKey, BYTE bVal, bool bEcho) { return 0; }
};

#endif // _INC_CSCRIPTCONSOLE_H

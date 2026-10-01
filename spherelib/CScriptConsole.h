#ifndef _INC_CSCRIPTCONSOLE_H
#define _INC_CSCRIPTCONSOLE_H

class CScriptObj;

class CStreamText
{
public:
	virtual ~CStreamText() = default;

	virtual bool WriteString(LPCTSTR pszStr)
	{
		(void)pszStr;
		return false;
	}

	void Printf(LPCTSTR lpszFormat, ...) __printfargs(2, 3)
	{
		va_list args;
		va_start(args, lpszFormat);
		CGString sText;
		sText.FormatV(lpszFormat, args);
		va_end(args);
		WriteString(sText);
	}
};

// aka CTextConsole
class CScriptConsole : public CStreamText
{
public:
	virtual int GetPrivLevel() const { return 0; }
	virtual CGString GetName() const { return CGString("console"); }

	virtual bool WriteString(LPCTSTR pszStr) override
	{
		return CStreamText::WriteString(pszStr);
	}
	virtual CScriptObj* GetAttachedObj() { return NULL; }
	int AddConsoleKey(CGString& sText, BYTE bVal, bool bEcho)
	{
		if ( bVal == '\r' || bVal == '\n' )
			return 2;

		if ( bVal == '\b' || bVal == 0x7f )
		{
			if ( !sText.IsEmpty())
				sText.SetLength(sText.GetLength() - 1);
			if ( bEcho )
				WriteString("\b \b");
			return 1;
		}

		if ( bVal < 0x20 )
			return 1;

		if ( sText.GetLength() >= CSTRING_MAX_LEN - 1 )
			return 1;
		sText += static_cast<TCHAR>(bVal);
		if ( bEcho )
		{
			TCHAR szEcho[2] = { static_cast<TCHAR>(bVal), '\0' };
			WriteString(szEcho);
		}
		return 1;
	}
};

#endif // _INC_CSCRIPTCONSOLE_H

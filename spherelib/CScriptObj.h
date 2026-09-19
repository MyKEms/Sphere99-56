#ifndef _INC_CSCRIPTOBJ_H
#define _INC_CSCRIPTOBJ_H

#define SCRIPT_EXT ".scp"

#define CSCRIPT_ARGCHK_VAL 1000
#define CSCRIPT_PARSE_HTML 1
#define CSCRIPT_PARSE_NBSP 2

class CScript;

enum TRIGRUN_TYPE
{
	TRIGRUN_SECTION_EXEC,	// Execute this section (first line already read)
	TRIGRUN_SECTION_TRUE,	// Execute this section
	TRIGRUN_SECTION_FALSE,	// Ignore this section
	TRIGRUN_SINGLE_EXEC,	// Execute just this line or blocked segment (first line already read)
	TRIGRUN_SINGLE_TRUE,	// Execute just this line or blocked segment
	TRIGRUN_SINGLE_FALSE	// Ignore just this line or blocked segment
};

enum TRIGRET_TYPE	// trigger script returns.
{
	TRIGRET_RET_FALSE = 0,	// default return. (script might not have been handled)
	TRIGRET_RET_TRUE = 1,
	TRIGRET_RET_DEFAULT,	// we just came to the end of the script.
	TRIGRET_RET_VAL,
	TRIGRET_ENDIF,
	TRIGRET_ELSE,
	TRIGRET_ELSEIF,
	TRIGRET_RET_HALFBAKED,
	TRIGRET_BREAK,
	TRIGRET_CONTINUE,
	TRIGRET_QTY
};

class CScriptExecContext;

class CScriptObj
{
public:
	virtual TRIGRET_TYPE OnTrigger(LPCTSTR pszTrigName, CScriptExecContext& exec)
	{
		// Default: no triggers handled. Subclasses (CChar, CItem) override this.
		return TRIGRET_RET_DEFAULT;
	}
	virtual CGString GetName() const { return CGString(); }
};

enum SCRIPT_UNKNOWN_KIND
{
	SCRIPT_UNKNOWN_GET,
	SCRIPT_UNKNOWN_SET,
	SCRIPT_UNKNOWN_METHOD,
	SCRIPT_UNKNOWN_FUNCTION,
	SCRIPT_UNKNOWN_TRIGGER,
	SCRIPT_UNKNOWN_REJECTED
};

void ScriptUnknownRecord(SCRIPT_UNKNOWN_KIND kind, LPCTSTR pszKeyword, const CScriptObj* pObj);
const CScript* ScriptUnknownSetContext(const CScript* pScript);
void ScriptUnknownReportSetPath(LPCTSTR pszPath);
bool ScriptUnknownReportIsEnabled();
bool ScriptUnknownResultIsRejected(HRESULT hRes);
bool ScriptUnknownReportWrite();

class CScriptUnknownRejectTracker
{
public:
	CScriptUnknownRejectTracker()
		: m_pObj(NULL), m_fEnabled(ScriptUnknownReportIsEnabled()), m_fRejected(false)
	{
		m_szKeyword[0] = '\0';
	}

	void Observe(HRESULT hRes, LPCTSTR pszKeyword, const CScriptObj* pObj)
	{
		if ( !m_fEnabled || m_fRejected || !ScriptUnknownResultIsRejected(hRes) )
			return;
		strncpy(m_szKeyword, pszKeyword ? pszKeyword : "", sizeof(m_szKeyword) - 1);
		m_szKeyword[sizeof(m_szKeyword) - 1] = '\0';
		m_pObj = pObj;
		m_fRejected = true;
	}

	bool RecordIfPresent()
	{
		if ( !m_fRejected )
			return false;
		ScriptUnknownRecord(SCRIPT_UNKNOWN_REJECTED, m_szKeyword, m_pObj);
		m_fRejected = false;
		return true;
	}

private:
	const CScriptObj* m_pObj;
	TCHAR m_szKeyword[256];
	bool m_fEnabled;
	bool m_fRejected;
};

class CScriptUnknownContextScope
{
public:
	explicit CScriptUnknownContextScope(const CScript* pScript)
		: m_pPrevious(NULL), m_fActive(ScriptUnknownReportIsEnabled())
	{
		if ( m_fActive )
			m_pPrevious = ScriptUnknownSetContext(pScript);
	}

	~CScriptUnknownContextScope()
	{
		if ( m_fActive )
			ScriptUnknownSetContext(m_pPrevious);
	}

	CScriptUnknownContextScope(const CScriptUnknownContextScope&) = delete;
	CScriptUnknownContextScope& operator=(const CScriptUnknownContextScope&) = delete;

private:
	const CScript* m_pPrevious;
	bool m_fActive;
};

class CRefObjDef
{
};

#endif // _INC_CSCRIPTOBJ_H

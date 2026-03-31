#ifndef _INC_CSCRIPTOBJ_H
#define _INC_CSCRIPTOBJ_H

#define SCRIPT_EXT ".scp"

#define CSCRIPT_ARGCHK_VAL 1000
#define CSCRIPT_PARSE_HTML 1
#define CSCRIPT_PARSE_NBSP 2

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

class CRefObjDef
{
};

#endif // _INC_CSCRIPTOBJ_H

#ifndef _INC_CSCRIPTEXECCONTEXT_H
#define _INC_CSCRIPTEXECCONTEXT_H

// Script control-flow keywords recognized by ExecuteScript.
enum SK_TYPE
{
	SK_BEGIN,
	SK_BREAK,
	SK_CONTINUE,
	SK_DORAND,
	SK_DOSWITCH,
	SK_ELIF,
	SK_ELSE,
	SK_ELSEIF,
	SK_END,
	SK_ENDDO,
	SK_ENDFOR,
	SK_ENDIF,
	SK_ENDRAND,
	SK_ENDSWITCH,
	SK_ENDWHILE,
	SK_FOR,
	SK_FORCHAR,
	SK_FORCLIENTS,
	SK_FORITEM,
	SK_FOROBJ,
	SK_FORPLAYERS,
	SK_IF,
	SK_RETURN,
	SK_WHILE,
	SK_QTY
};

class CScriptExecContext : public CExpression
{
private:
	CScriptObj* m_pBaseObj;		// the "this" object we are scripting
	CScriptConsole* m_pSrc;	// who triggered this (the console/player source)

	static LPCTSTR const sm_szScriptKeys[];

public:
	static CScriptPropArray sm_FunctionsAll;

	static void InitFunctions()
	{
		// Base exec context has no special functions of its own.
		// Subclasses (CSphereExpContext) add theirs via AddProps.
	}

public:
	CGVariant m_vValRet;
	CVarDefArray m_ArgArray;

	CScriptExecContext(CScriptObj* pObj, CScriptConsole* pConsole)
		: m_pBaseObj(pObj), m_pSrc(pConsole)
	{
	}

public:
	virtual HRESULT Function_Dispatch(LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet)
	{
		// Base exec context has no special function table.
		// Subclasses override this for global methods (CSphereExpContext).
		return HRES_UNKNOWN_PROPERTY;
	}

	void SetBaseObject(CScriptObj* pObj)
	{
		m_pBaseObj = pObj;
	}

	CScriptObj* GetBaseObject() const
	{
		return m_pBaseObj;
	}

	CScriptConsole* GetSrc() const
	{
		return m_pSrc;
	}

	void s_ParseEscapes(TCHAR* pszBuf, DWORD dwFlags)
	{
		// Resolve <...> expression tags in a text buffer, in-place.
		// <eval 1+2>  → "3"
		// <SRC.NAME>  → character name
		// <STRLEN(x)> → string length
		// <safe ...>  → returns "" on error
		// <?...?>     → deferred macro, pass through for now
		//
		// dwFlags: CSCRIPT_PARSE_HTML = use %% delimiters instead of <>

		if ( pszBuf == NULL || *pszBuf == '\0' )
			return;

		TCHAR chBegin = '<';
		TCHAR chEnd = '>';
		if ( dwFlags & 0x01 ) // CSCRIPT_PARSE_HTML
		{
			chBegin = '%';
			chEnd = '%';
		}

		for ( int i = 0; pszBuf[i]; i++ )
		{
			if ( pszBuf[i] != chBegin )
				continue;

			// Check for <?...?> deferred macros — pass through for now.
			if ( pszBuf[i+1] == '?' )
				continue;

			// Must start with an alphanumeric or '<' (nested).
			if ( !isalnum(pszBuf[i+1]) && pszBuf[i+1] != '<' )
				continue;

			int iBegin = i;
			int iDepth = 1;
			int iEnd = -1;

			// Find the matching '>' respecting nesting.
			for ( int j = i + 1; pszBuf[j]; j++ )
			{
				if ( pszBuf[j] == chBegin && (isalnum(pszBuf[j+1]) || pszBuf[j+1] == '<') )
					iDepth++;
				else if ( pszBuf[j] == chEnd )
				{
					iDepth--;
					if ( iDepth <= 0 )
					{
						iEnd = j;
						break;
					}
				}
			}
			if ( iEnd < 0 )
				continue; // unmatched bracket

			// Extract the expression between < and >.
			pszBuf[iEnd] = '\0';
			TCHAR* pszExpr = pszBuf + iBegin + 1;

			// Recursively resolve any nested <...> first.
			s_ParseEscapes(pszExpr, dwFlags);

			// Check for "safe" prefix.
			bool fSafe = false;
			if ( !_strnicmp(pszExpr, "safe", 4) )
			{
				TCHAR ch5 = pszExpr[4];
				if ( ch5 == ' ' || ch5 == '.' || ch5 == '(' || ch5 == '\0' )
				{
					fSafe = true;
					pszExpr += 4;
					if ( *pszExpr == ' ' || *pszExpr == '.' )
						pszExpr++;
				}
			}

			// Try to resolve the expression.
			CGString sResult;
			bool fResolved = false;

			try
			{
				// Split function name from arguments: "FUNC(args)" or "FUNC args" or "OBJ.PROP"
				TCHAR szKey[SCRIPT_MAX_LINE_LEN];
				strncpy(szKey, pszExpr, sizeof(szKey)-1);
				szKey[sizeof(szKey)-1] = '\0';

				// Find argument separator: space, '(', or '.'
				TCHAR* pszArgs = szKey;
				while ( *pszArgs && *pszArgs != ' ' && *pszArgs != '(' )
					pszArgs++;

				CGVariant vArgs;
				CGVariant vValRet;

				if ( *pszArgs == '(' )
				{
					// Function call: FUNC(args)
					*pszArgs++ = '\0';
					// Strip trailing ')'
					int len = strlen(pszArgs);
					if ( len > 0 && pszArgs[len-1] == ')' )
						pszArgs[len-1] = '\0';
					vArgs = pszArgs;
				}
				else if ( *pszArgs == ' ' )
				{
					// Function or eval: "eval 1+2" or "FUNC args"
					*pszArgs++ = '\0';
					while ( ISWHITESPACE(*pszArgs) ) pszArgs++;
					vArgs = pszArgs;
				}

				// Try global function dispatch.
				HRESULT hRes = Function_Dispatch(szKey, vArgs, vValRet);
				if ( hRes == NO_ERROR )
				{
					sResult = vValRet.IsEmpty() ? "" : vValRet.GetPSTR();
					fResolved = true;
				}

				// Try object property access (SRC.NAME, OBJ.PROP, etc.)
				if ( !fResolved )
				{
					CResourceObj* pObj = dynamic_cast<CResourceObj*>(m_pBaseObj);
					if ( pObj )
					{
						hRes = pObj->s_PropGet(pszExpr, vValRet, m_pSrc);
						if ( hRes == NO_ERROR )
						{
							sResult = vValRet.IsEmpty() ? "" : vValRet.GetPSTR();
							fResolved = true;
						}
					}
				}

				// Try method call on object.
				if ( !fResolved && szKey[0] )
				{
					CResourceObj* pObj = dynamic_cast<CResourceObj*>(m_pBaseObj);
					if ( pObj )
					{
						hRes = pObj->s_Method(szKey, vArgs, vValRet, m_pSrc);
						if ( hRes == NO_ERROR )
						{
							sResult = vValRet.IsEmpty() ? "" : vValRet.GetPSTR();
							fResolved = true;
						}
					}
				}
			}
			catch (...)
			{
				fResolved = fSafe;
			}

			if ( !fResolved )
			{
				if ( fSafe )
				{
					sResult = "";
					fResolved = true;
				}
				else
				{
					// Restore the '>' and skip — don't modify unresolvable tags.
					pszBuf[iEnd] = chEnd;
					continue;
				}
			}

			// Replace <expr> with the resolved value, shifting the buffer.
			int iExprLen = iEnd - iBegin + 1; // includes < and >
			int iResultLen = sResult.GetLength();
			int iTrailLen = strlen(pszBuf + iEnd + 1); // chars after '>'

			// Restore the null we placed at iEnd for the trailing copy.
			// pszBuf[iEnd] is already '\0', the trailing starts at iEnd+1.
			memmove(pszBuf + iBegin + iResultLen, pszBuf + iEnd + 1, iTrailLen + 1);
			memcpy(pszBuf + iBegin, (LPCTSTR)sResult, iResultLen);

			// Re-scan from the end of the replacement (don't re-resolve our own output).
			i = iBegin + iResultLen - 1; // -1 because the for loop increments.
		}
	}

	//
	// ExecuteCommand -- execute a single script command line.
	// The line is "KEY VALUE" or "KEY=VALUE" or just "METHOD args".
	// Returns NO_ERROR on success, or an HRESULT error code.
	//
	HRESULT ExecuteCommand(LPCTSTR pszCmd)
	{
		if ( !pszCmd || !*pszCmd )
			return NO_ERROR;

		// Skip leading whitespace
		while ( ISWHITESPACE(*pszCmd) )
			pszCmd++;
		if ( !*pszCmd || *pszCmd == '/' )
			return NO_ERROR; // blank or comment

		// Split into key and arg at first space or '='
		TCHAR szLine[SCRIPT_MAX_LINE_LEN];
		strncpy(szLine, pszCmd, sizeof(szLine) - 1);
		szLine[sizeof(szLine) - 1] = '\0';

		TCHAR* pszKey = szLine;
		TCHAR* pszArg = NULL;

		// Find the split point
		TCHAR* p = pszKey;
		while ( *p && !ISWHITESPACE(*p) && *p != '=' )
			p++;
		if ( *p )
		{
			bool fHasEquals = (*p == '=');
			*p = '\0';
			p++;
			if ( fHasEquals )
			{
				// Skip whitespace after '='
				while ( ISWHITESPACE(*p) ) p++;
			}
			else
			{
				// Skip whitespace after key
				while ( ISWHITESPACE(*p) ) p++;
				// Check for '=' after whitespace
				if ( *p == '=' )
				{
					p++;
					while ( ISWHITESPACE(*p) ) p++;
				}
			}
			pszArg = p;
		}

		if ( !pszArg )
			pszArg = const_cast<TCHAR*>("");

		// Try dispatching to the base object.
		CResourceObj* pObj = dynamic_cast<CResourceObj*>(m_pBaseObj);
		if ( pObj )
		{
			// Try as a property set (KEY=VALUE).
			CGVariant vVal(pszArg);
			HRESULT hRes = pObj->s_PropSet(pszKey, vVal);
			if ( hRes == NO_ERROR )
				return NO_ERROR;

			// Try as a method call (KEY args).
			CGVariant vArgs(pszArg);
			CGVariant vValRet;
			hRes = pObj->s_Method(pszKey, vArgs, vValRet, m_pSrc);
			if ( hRes == NO_ERROR )
				return NO_ERROR;
		}

		// Unknown command -- not an error for now, just ignore.
		return HRES_UNKNOWN_PROPERTY;
	}

	//
	// ExecuteScript -- execute a block of script lines from a CScript.
	// This is the main script execution loop with control flow.
	//
	// type:
	//   TRIGRUN_SECTION_EXEC  - execute section, first line already read
	//   TRIGRUN_SECTION_TRUE  - execute section
	//   TRIGRUN_SECTION_FALSE - skip section (but track nesting)
	//   TRIGRUN_SINGLE_EXEC   - execute one line/block, first line read
	//   TRIGRUN_SINGLE_TRUE   - execute one line/block
	//   TRIGRUN_SINGLE_FALSE  - skip one line/block
	//
	TRIGRET_TYPE ExecuteScript(CScript& script, TRIGRUN_TYPE type)
	{
		bool fSectionFalse = (type == TRIGRUN_SECTION_FALSE || type == TRIGRUN_SINGLE_FALSE);

		LPCTSTR pszKey;

		if ( type == TRIGRUN_SECTION_EXEC || type == TRIGRUN_SINGLE_EXEC )
		{
			// First line already read -- jump straight to dispatch.
			pszKey = script.GetKey();
			goto jump_in;
		}

		while ( script.ReadKeyParse() )
		{
			pszKey = script.GetKey();

			// If we hit the start of the next ON trigger, stop.
			if ( !_strnicmp(pszKey, "ON", 2) )
			{
				// Check it's actually "ON" followed by non-alpha (like "ON=..." or "ON ")
				// or it could be a property starting with ON like ONCOUNT
				char ch = pszKey[2];
				if ( ch == '\0' || ch == '=' || ISWHITESPACE(ch) )
					break;
			}

		jump_in:

			// Identify control-flow keywords.
			SK_TYPE index = (SK_TYPE) FindTableHeadSorted(pszKey, sm_szScriptKeys, SK_QTY);

			// Handle block terminators first (always, regardless of fSectionFalse).
			switch ( index )
			{
			case SK_ENDIF:
			case SK_END:
			case SK_ENDDO:
			case SK_ENDFOR:
			case SK_ENDRAND:
			case SK_ENDSWITCH:
			case SK_ENDWHILE:
				return TRIGRET_ENDIF;
			case SK_ELIF:
			case SK_ELSEIF:
				return TRIGRET_ELSEIF;
			case SK_ELSE:
				return TRIGRET_ELSE;
			default:
				break;
			}

			if ( fSectionFalse )
			{
				// Skipping this section -- but we still need to track nested blocks.
				switch ( index )
				{
				case SK_IF:
					{
						// Skip nested IF/ELSEIF/ELSE/ENDIF blocks.
						TRIGRET_TYPE iRet;
						do {
							iRet = ExecuteScript(script, TRIGRUN_SECTION_FALSE);
						} while ( iRet == TRIGRET_ELSEIF || iRet == TRIGRET_ELSE );
					}
					break;
				case SK_WHILE:
				case SK_FOR:
				case SK_FORCHAR:
				case SK_FORCLIENTS:
				case SK_FORITEM:
				case SK_FOROBJ:
				case SK_FORPLAYERS:
				case SK_DORAND:
				case SK_DOSWITCH:
				case SK_BEGIN:
					// Skip the nested block.
					ExecuteScript(script, TRIGRUN_SECTION_FALSE);
					break;
				default:
					break;
				}
				if ( type >= TRIGRUN_SINGLE_EXEC )
					return TRIGRET_RET_DEFAULT;
				continue; // keep skipping
			}

			// Executing for real.
			TRIGRET_TYPE iRet = TRIGRET_RET_DEFAULT;

			switch ( index )
			{
			case SK_BREAK:
				return TRIGRET_BREAK;
			case SK_CONTINUE:
				return TRIGRET_CONTINUE;

			case SK_RETURN:
				{
					// RETURN [value]
					LPCTSTR pszArg = script.GetArgRaw();
					if ( pszArg && *pszArg )
					{
						int iVal = GetComplex(pszArg);
						m_vValRet.SetInt(iVal);
						return (TRIGRET_TYPE) iVal;
					}
					return TRIGRET_RET_DEFAULT;
				}

			case SK_IF:
				{
					// IF <condition>
					LPCTSTR pszArg = script.GetArgRaw();
					int fCondition = 0;
					if ( pszArg && *pszArg )
						fCondition = GetComplex(pszArg);
					bool fBeenTrue = false;

					for (;;)
					{
						iRet = ExecuteScript(script, fCondition ? TRIGRUN_SECTION_TRUE : TRIGRUN_SECTION_FALSE);
						if ( iRet < TRIGRET_ENDIF || iRet >= TRIGRET_RET_HALFBAKED )
							return iRet;
						if ( iRet == TRIGRET_ENDIF )
							break;
						fBeenTrue |= (fCondition != 0);
						if ( fBeenTrue )
							fCondition = 0;
						else if ( iRet == TRIGRET_ELSE )
							fCondition = 1;
						else if ( iRet == TRIGRET_ELSEIF )
						{
							LPCTSTR pszElseArg = script.GetArgRaw();
							fCondition = (pszElseArg && *pszElseArg) ? GetComplex(pszElseArg) : 0;
						}
					}
				}
				break;

			case SK_WHILE:
				{
					// WHILE <condition>
					CScriptLineContext ctxStart = script.GetContext();
					int iLoops = 0;
					for (;;)
					{
						if ( ++iLoops > 10000 )
							break; // safety limit

						// Re-evaluate condition each iteration.
						// The condition is in the arg of the WHILE line.
						// We need to seek back to re-read it each time.
						// For now, use a simplified approach: first iteration uses current arg,
						// subsequent iterations re-seek and re-read.
						LPCTSTR pszArg = script.GetArgRaw();
						int fCond = (pszArg && *pszArg) ? GetComplex(pszArg) : 0;
						if ( !fCond )
							break;

						iRet = ExecuteScript(script, TRIGRUN_SECTION_TRUE);
						if ( iRet == TRIGRET_BREAK )
							break;
						if ( iRet != TRIGRET_ENDIF && iRet != TRIGRET_CONTINUE )
							return iRet;
						script.SeekContext(ctxStart);
					}
					// Skip past the ENDWHILE if we didn't enter / broke out.
					if ( iLoops <= 1 || iRet == TRIGRET_BREAK )
					{
						// Need to skip the block.
						CScriptLineContext ctxNow = script.GetContext();
						if ( ctxNow.m_lOffset <= ctxStart.m_lOffset )
							ExecuteScript(script, TRIGRUN_SECTION_FALSE);
					}
				}
				break;

			case SK_FOR:
				{
					// FOR <max> or FOR <min> <max>
					CScriptLineContext ctxStart = script.GetContext();
					CScriptLineContext ctxEnd = ctxStart;
					LPCTSTR pszArg = script.GetArgRaw();
					int iMin = 1, iMax = 0;
					if ( pszArg && *pszArg )
					{
						iMax = GetComplex(pszArg);
					}
					int iLoops = 0;
					for ( int i = iMin; i <= iMax; i++ )
					{
						if ( ++iLoops > 10000 )
							break;
						iRet = ExecuteScript(script, TRIGRUN_SECTION_TRUE);
						if ( iRet == TRIGRET_BREAK )
						{
							ctxEnd = ctxStart;
							break;
						}
						if ( iRet != TRIGRET_ENDIF && iRet != TRIGRET_CONTINUE )
							return iRet;
						ctxEnd = script.GetContext();
						script.SeekContext(ctxStart);
					}
					if ( ctxEnd.m_lOffset <= ctxStart.m_lOffset )
						ExecuteScript(script, TRIGRUN_SECTION_FALSE);
					else
						script.SeekContext(ctxEnd);
				}
				break;

			case SK_DORAND:
			case SK_DOSWITCH:
				{
					// DORAND <count> / DOSWITCH <index>
					LPCTSTR pszArg = script.GetArgRaw();
					int iVal = (pszArg && *pszArg) ? GetComplex(pszArg) : 0;
					if ( index == SK_DORAND && iVal > 0 )
						iVal = Calc_GetRandVal(iVal);
					for (;;)
					{
						iRet = ExecuteScript(script, (iVal == 0) ? TRIGRUN_SINGLE_TRUE : TRIGRUN_SINGLE_FALSE);
						iVal--;
						if ( iRet == TRIGRET_RET_DEFAULT )
							continue;
						if ( iRet == TRIGRET_ENDIF )
							break;
						return iRet;
					}
				}
				break;

			case SK_BEGIN:
				// BEGIN...END block -- just execute the contents.
				iRet = ExecuteScript(script, TRIGRUN_SECTION_TRUE);
				if ( iRet != TRIGRET_ENDIF )
					return iRet;
				break;

			case SK_FORITEM:
			case SK_FORCHAR:
			case SK_FORCLIENTS:
			case SK_FOROBJ:
			case SK_FORPLAYERS:
				// World-iteration loops require game world objects.
				// Skip them for now (infrastructure only).
				iRet = ExecuteScript(script, TRIGRUN_SECTION_FALSE);
				break;

			default:
				// Regular command line -- dispatch it.
				{
					// Build the full command: "KEY VALUE"
					TCHAR szCmd[SCRIPT_MAX_LINE_LEN];
					LPCTSTR pszArgStr = script.GetArgRaw();
					if ( pszArgStr && *pszArgStr )
						snprintf(szCmd, sizeof(szCmd), "%s %s", pszKey, pszArgStr);
					else
						strncpy(szCmd, pszKey, sizeof(szCmd) - 1);
					szCmd[sizeof(szCmd) - 1] = '\0';
					ExecuteCommand(szCmd);
				}
				break;
			}

			// Handle loop-type returns.
			switch ( index )
			{
			case SK_FORITEM:
			case SK_FORCHAR:
			case SK_FORCLIENTS:
			case SK_FOROBJ:
			case SK_FORPLAYERS:
			case SK_FOR:
			case SK_WHILE:
				if ( iRet != TRIGRET_ENDIF )
					return iRet;
				break;
			default:
				break;
			}

			if ( type >= TRIGRUN_SINGLE_EXEC )
				return TRIGRET_RET_DEFAULT;
		}

		return TRIGRET_RET_DEFAULT;
	}
};

// Static keyword table for script control flow.
// Must match SK_TYPE enum order and be sorted alphabetically for FindTableHeadSorted.
inline LPCTSTR const CScriptExecContext::sm_szScriptKeys[] =
{
	"BEGIN",
	"BREAK",
	"CONTINUE",
	"DORAND",
	"DOSWITCH",
	"ELIF",
	"ELSE",
	"ELSEIF",
	"END",
	"ENDDO",
	"ENDFOR",
	"ENDIF",
	"ENDRAND",
	"ENDSWITCH",
	"ENDWHILE",
	"FOR",
	"FORCHAR",
	"FORCLIENTS",
	"FORITEM",
	"FOROBJ",
	"FORPLAYERS",
	"IF",
	"RETURN",
	"WHILE",
	NULL
};

#endif // _INC_CSCRIPTEXECCONTEXT_H

#ifndef _INC_CSCRIPTEXECCONTEXT_H
#define _INC_CSCRIPTEXECCONTEXT_H

// A WHILE or FOR loop stops after this many iterations.
#define SCRIPT_MAX_LOOP_ITERATIONS 10000

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

// CVarDefArray stores allocated CVarDef values but does not own them. ARG
// locals are context-owned, so release them with the execution context.
class CScriptLocalArgs : public CVarDefArray
{
public:
	~CScriptLocalArgs()
	{
		for ( int i = 0; i < GetSize(); ++i )
			delete GetAt(i);
	}
};

class CScriptExecContext : public CExpression
{
private:
	CScriptObj* m_pBaseObj;		// the "this" object we are scripting
	CScriptConsole* m_pSrc;	// who triggered this (the console/player source)

	static LPCTSTR const sm_szScriptKeys[];

	static SCRIPT_UNKNOWN_KIND UnknownExpressionKind(LPCTSTR pszExpr)
	{
		bool fHasDot = false;
		bool fHasCall = false;
		bool fHasArgs = false;
		for ( LPCTSTR p = pszExpr; p && *p; ++p )
		{
			if ( *p == '.' )
				fHasDot = true;
			else if ( *p == '(' )
			{
				fHasCall = true;
				break;
			}
			else if ( ISWHITESPACE(*p) )
			{
				fHasArgs = true;
				break;
			}
		}
		if ( fHasDot && (fHasCall || fHasArgs) )
			return SCRIPT_UNKNOWN_METHOD;
		if ( fHasCall || fHasArgs || (pszExpr && !_strnicmp(pszExpr, "f_", 2)) )
			return SCRIPT_UNKNOWN_FUNCTION;
		return SCRIPT_UNKNOWN_GET;
	}

protected:
	// Numeric values returned by script functions may be UIDs. Concrete engine
	// contexts can resolve them to world objects without coupling this shared
	// execution context to the server's world implementation.
	virtual CResourceObj* ResolveUIDObject(UID_INDEX uid)
	{
		(void)uid;
		return NULL;
	}

	virtual bool IsScriptFunction(LPCTSTR pszKey)
	{
		(void)pszKey;
		return false;
	}

	CResourceObj* ResolveObjectResult(const CGVariant& value, LPCTSTR pszFunctionRoot)
	{
		CResourceObj* pObj = dynamic_cast<CResourceObj*>(value.GetRef());
		if ( pObj == NULL && pszFunctionRoot && value.IsNumeric() )
		{
			// Script functions and the reference-valued argument helpers return
			// object UIDs as strings. Named ARG locals can hold the same UID after
			// ARGV() copies a function argument, so all of these roots use the
			// engine's UID resolver before property chaining continues.
			bool fUIDRoot = IsScriptFunction(pszFunctionRoot) ||
				!_stricmp(pszFunctionRoot, "ARG") ||
				!_stricmp(pszFunctionRoot, "ARGV") ||
				!_stricmp(pszFunctionRoot, "LASTNEW") ||
				!_stricmp(pszFunctionRoot, "LASTNEWITEM") ||
				!_stricmp(pszFunctionRoot, "LASTNEWCHAR") ||
				m_LocalArgs.FindKeyPtr(pszFunctionRoot) != NULL;
			if ( fUIDRoot )
				pObj = ResolveUIDObject(value.GetUID());
		}
		return pObj;
	}

	bool ResolveDottedFunctionResult(LPCTSTR pszKey, CGVariant& vValRet, CScriptUnknownRejectTracker& rejected)
	{
		LPCTSTR pszDot = strchr(pszKey, '.');
		if ( pszDot == NULL || pszDot == pszKey || pszDot[1] == '\0' )
			return false;

		TCHAR szRoot[SCRIPT_MAX_LINE_LEN];
		size_t iRootLen = pszDot - pszKey;
		if ( iRootLen >= sizeof(szRoot) )
			return false;

		memcpy(szRoot, pszKey, iRootLen);
		szRoot[iRootLen] = '\0';
		CGVariant vRootArgs;
		CGVariant vRoot;
		HRESULT hRoot = Function_Dispatch(szRoot, vRootArgs, vRoot);
		rejected.Observe(hRoot, szRoot, m_pBaseObj);
		if ( hRoot != NO_ERROR )
			return false;

		CResourceObj* pRootObj = ResolveObjectResult(vRoot, szRoot);
		if ( pRootObj == NULL )
			return false;

		vValRet.SetRef(pRootObj);
		return true;
	}

	// Split one reference-chain segment, "NAME" or "NAME(args)", into its
	// name and its own argument list.
	static bool SplitDottedSegment(LPCTSTR pszSegment, size_t iLen, TCHAR* pszName, size_t iNameSize, CGVariant& vArgs)
	{
		vArgs.SetVoid();
		if ( iLen == 0 || iLen >= iNameSize )
			return false;
		memcpy(pszName, pszSegment, iLen);
		pszName[iLen] = '\0';

		TCHAR* pParen = strchr(pszName, '(');
		if ( pParen == NULL )
			return true;
		if ( pParen == pszName || pszName[iLen - 1] != ')' )
			return false;
		pszName[iLen - 1] = '\0';
		*pParen = '\0';
		vArgs = pParen + 1;
		return true;
	}

	// Resolve a reference chain such as SRC.FINDLAYER(30).NAME,
	// FINDUID(uid).TAG(name), F_FUNC(arg).CONT.UID or SECTOR.LIGHT.  The root
	// is a context function or, failing that, a reference property or method
	// of the default object (CONT, TOPOBJ, SECTOR, REGION, ACT, LINK, ...).
	// Each segment keeps its own arguments.  A segment is looked up on the
	// current object as a property, then as a method, then as a function with
	// the object as its base -- the order the single-level reference path has
	// always used.  When that fails, the rest of the chain is offered to the
	// object as one property name, for properties that are dotted themselves
	// (TAG.name, TAG0.name).
	//
	// Returns true when the whole chain resolved.  fEffect reports whether a
	// script function or an object method already ran; the caller must then
	// not evaluate the same expression a second time through another path.
	// Expressions with top-level whitespace ("EVAL 1.5", "STRLEN a.b") are
	// function calls with arguments, not chains, and are left to the caller.
	bool ResolveDottedChain(LPCTSTR pszExpr, CGVariant& vValRet, CScriptUnknownRejectTracker& rejected, bool& fEffect)
	{
		fEffect = false;

		enum { MAX_CHAIN_SEGMENTS = 32 };
		size_t aStart[MAX_CHAIN_SEGMENTS];
		size_t aLen[MAX_CHAIN_SEGMENTS];
		int iSegments = 0;
		int iDepth = 0;
		size_t iSegmentStart = 0;
		size_t i = 0;
		for ( ; pszExpr[i]; i++ )
		{
			TCHAR ch = pszExpr[i];
			if ( ch == '(' )
				iDepth++;
			else if ( ch == ')' )
			{
				if ( --iDepth < 0 )
					return false;
			}
			else if ( iDepth == 0 )
			{
				if ( ISWHITESPACE(ch) )
					return false;
				if ( ch == '.' )
				{
					if ( iSegments >= MAX_CHAIN_SEGMENTS - 1 )
						return false;
					aStart[iSegments] = iSegmentStart;
					aLen[iSegments] = i - iSegmentStart;
					iSegments++;
					iSegmentStart = i + 1;
				}
			}
		}
		if ( iDepth != 0 || iSegments == 0 )
			return false;
		aStart[iSegments] = iSegmentStart;
		aLen[iSegments] = i - iSegmentStart;
		iSegments++;

		TCHAR szRoot[SCRIPT_MAX_LINE_LEN];
		CGVariant vArgs;
		if ( !SplitDottedSegment(pszExpr + aStart[0], aLen[0], szRoot, sizeof(szRoot), vArgs) )
			return false;

		CGVariant vCurrent;
		HRESULT hRes = Function_Dispatch(szRoot, vArgs, vCurrent);
		rejected.Observe(hRes, szRoot, m_pBaseObj);
		bool fRootFromFunction = (hRes == NO_ERROR);
		if ( fRootFromFunction )
		{
			if ( IsScriptFunction(szRoot) )
				fEffect = true;
		}
		else
		{
			CResourceObj* pBase = dynamic_cast<CResourceObj*>(m_pBaseObj);
			if ( pBase == NULL )
				return false;
			hRes = pBase->s_PropGet(szRoot, vCurrent, m_pSrc);
			rejected.Observe(hRes, szRoot, m_pBaseObj);
			if ( hRes != NO_ERROR )
			{
				hRes = pBase->s_Method(szRoot, vArgs, vCurrent, m_pSrc);
				rejected.Observe(hRes, szRoot, m_pBaseObj);
				if ( hRes == NO_ERROR )
					fEffect = true;
			}
			if ( hRes != NO_ERROR )
				return false;
		}

		CResourceObj* pCurrent = ResolveObjectResult(vCurrent, fRootFromFunction ? szRoot : NULL);
		if ( pCurrent == NULL )
		{
			// A script function that returned no object (empty or a UID that
			// does not resolve) reads as an empty value.  Other roots that
			// yield no object are left to the caller, which keeps the
			// historical result (for example FINDUID of a missing UID).
			if ( fEffect && (vCurrent.IsEmpty() || vCurrent.IsNumeric()) )
			{
				vValRet.SetStr("");
				return true;
			}
			return false;
		}

		for ( int iSegment = 1; iSegment < iSegments; iSegment++ )
		{
			TCHAR szName[SCRIPT_MAX_LINE_LEN];
			if ( !SplitDottedSegment(pszExpr + aStart[iSegment], aLen[iSegment], szName, sizeof(szName), vArgs) )
				return false;

			CGVariant vNext;
			bool fFromFunction = false;
			hRes = pCurrent->s_PropGet(szName, vNext, m_pSrc);
			rejected.Observe(hRes, szName, pCurrent);
			if ( hRes != NO_ERROR )
			{
				hRes = pCurrent->s_Method(szName, vArgs, vNext, m_pSrc);
				rejected.Observe(hRes, szName, pCurrent);
				if ( hRes == NO_ERROR )
					fEffect = true;
			}
			if ( hRes != NO_ERROR )
			{
				CScriptObj* pOldBase = GetBaseObject();
				SetBaseObject(pCurrent);
				hRes = Function_Dispatch(szName, vArgs, vNext);
				SetBaseObject(pOldBase);
				rejected.Observe(hRes, szName, pCurrent);
				if ( hRes == NO_ERROR )
				{
					fEffect = true;
					fFromFunction = true;
				}
			}
			if ( hRes != NO_ERROR )
			{
				// Offer the rest of the chain as one dotted property name.
				LPCTSTR pszRest = pszExpr + aStart[iSegment];
				if ( iSegment == iSegments - 1 || strchr(pszRest, '(') != NULL )
					return false;
				hRes = pCurrent->s_PropGet(pszRest, vNext, m_pSrc);
				rejected.Observe(hRes, pszRest, pCurrent);
				if ( hRes != NO_ERROR )
					return false;
				vValRet = vNext;
				return true;
			}

			if ( iSegment == iSegments - 1 )
			{
				vValRet = vNext;
				return true;
			}

			pCurrent = ResolveObjectResult(vNext, fFromFunction ? szName : NULL);
			if ( pCurrent == NULL )
			{
				// An intermediate lookup that found nothing (for example
				// FINDLAYER of an empty layer) reads as an empty value.
				if ( vNext.IsEmpty() )
				{
					vValRet.SetStr("");
					return true;
				}
				return false;
			}
		}
		return false;
	}

	// Evaluate the text of one <...> or <?...?> escape (without delimiters
	// and without a SAFE prefix).  Returns true and sets vResult when the
	// expression resolved; pfChainResolved reports whether the reference
	// chain walker produced the value.
	bool EvaluateEscapeValue(LPCTSTR pszExpr, CGVariant& vResult, CScriptUnknownRejectTracker& rejected, bool* pfChainResolved = NULL)
	{
		if ( pfChainResolved )
			*pfChainResolved = false;

		// Split function name from arguments: "FUNC(args)" or "FUNC args" or "OBJ.PROP"
		TCHAR szKey[SCRIPT_MAX_LINE_LEN];
		strncpy(szKey, pszExpr, sizeof(szKey)-1);
		szKey[sizeof(szKey)-1] = '\0';

		// Find argument separator: space or '('
		TCHAR* pszArgs = szKey;
		while ( *pszArgs && *pszArgs != ' ' && *pszArgs != '(' )
			pszArgs++;

		CGVariant vArgs;
		CGVariant vValRet;
		HRESULT hRes;

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

		// Reference chains with function roots and per-segment arguments.
		// Their diagnostics are kept apart so that a chain which is not
		// resolvable here does not change what the legacy path reports.
		bool fChainEffect = false;
		CScriptUnknownRejectTracker chainRejected;
		if ( ResolveDottedChain(pszExpr, vValRet, chainRejected, fChainEffect) )
		{
			if ( pfChainResolved )
				*pfChainResolved = true;
			vResult = vValRet;
			return true;
		}

		if ( fChainEffect )
		{
			// A script function or method already ran for this expression.
			// Every other path below can dispatch the same root or method
			// again, so the chain's result is final.
			rejected = chainRejected;
			return false;
		}

		// Try global function dispatch.
		hRes = Function_Dispatch(szKey, vArgs, vValRet);
		rejected.Observe(hRes, szKey, m_pBaseObj);
		if ( hRes != NO_ERROR && ResolveDottedFunctionResult(szKey, vValRet, rejected) )
			hRes = NO_ERROR;
		if ( hRes == NO_ERROR )
		{
			// Object reference chaining: <argo.tag(name)>, <argo.uid>, etc.
			CScriptObj* pRef = vValRet.GetRef();
			TCHAR* pDot = strchr(szKey, '.');
			if ( pRef == NULL || pDot == NULL )
			{
				vResult = vValRet;
				return true;
			}

			// Dispatch sub-key on the referenced object.
			LPCTSTR pszSubKey = pDot + 1;
			CResourceObj* pRefObj = dynamic_cast<CResourceObj*>(pRef);
			if ( pRefObj )
			{
				CGVariant vSubRet;
				hRes = pRefObj->s_PropGet(pszSubKey, vSubRet, m_pSrc);
				rejected.Observe(hRes, pszSubKey, pRefObj);
				if ( hRes != NO_ERROR )
				{
					hRes = pRefObj->s_Method(pszSubKey, vArgs, vSubRet, m_pSrc);
					rejected.Observe(hRes, pszSubKey, pRefObj);
				}
				if ( hRes != NO_ERROR )
				{
					// Try as function call with ref as base object.
					CScriptObj* pOldBase = GetBaseObject();
					SetBaseObject(pRefObj);
					hRes = Function_Dispatch(pszSubKey, vArgs, vSubRet);
					SetBaseObject(pOldBase);
					rejected.Observe(hRes, pszSubKey, pRefObj);
				}
				if ( hRes == NO_ERROR )
				{
					vResult = vSubRet;
					return true;
				}
			}
		}

		CResourceObj* pObj = dynamic_cast<CResourceObj*>(m_pBaseObj);
		if ( pObj == NULL )
			return false;

		// Try object property access (SRC.NAME, OBJ.PROP, etc.)
		hRes = pObj->s_PropGet(pszExpr, vValRet, m_pSrc);
		rejected.Observe(hRes, pszExpr, m_pBaseObj);
		if ( hRes == NO_ERROR )
		{
			vResult = vValRet;
			return true;
		}

		// Try method call on object.
		if ( szKey[0] )
		{
			hRes = pObj->s_Method(szKey, vArgs, vValRet, m_pSrc);
			rejected.Observe(hRes, szKey, m_pBaseObj);
			if ( hRes == NO_ERROR )
			{
				vResult = vValRet;
				return true;
			}
		}
		return false;
	}

	bool EvaluateEscapeExpression(LPCTSTR pszExpr, CGString& sResult, CScriptUnknownRejectTracker& rejected)
	{
		CGVariant vResult;
		if ( !EvaluateEscapeValue(pszExpr, vResult, rejected) )
			return false;
		sResult = vResult.IsEmpty() ? "" : vResult.GetPSTR();
		return true;
	}

public:
	// Numeric expressions (IF, ELIF, WHILE, RETURN, EVAL) read a bare
	// reference operand such as SRC.STR, SECTOR.LIGHT or FINDUID(uid).NAME
	// through the same evaluator as <...>.  A string result is read the way
	// a substituted <...> value would be; an object reference reads as its
	// UID.  Operands that do not resolve are left to the plain number and
	// DEFNAME reader, as before.
	virtual bool ResolveReferenceOperand(LPCTSTR pszOperand, int& iValue)
	{
		iValue = 0;
		if ( m_pBaseObj == NULL )
			return false;
		// Bare names are handed here only for ARG locals. Keep the legacy
		// DEFNAME/global lookup order for every other bare identifier, so a
		// condition cannot execute a zero-argument script function as a side
		// effect merely because it has no dotted suffix.
		if ( strchr(pszOperand, '.') == NULL && strchr(pszOperand, '(') == NULL &&
			m_LocalArgs.FindKeyPtr(pszOperand) == NULL )
			return false;

		CGVariant vValue;
		CScriptUnknownRejectTracker rejected;
		bool fChainResolved = false;
		if ( !EvaluateEscapeValue(pszOperand, vValue, rejected, &fChainResolved) )
			return false;

		CScriptObj* pRef = vValue.GetRef();
		if ( pRef != NULL )
		{
			// Without the chain walker, a dotted operand can only have been
			// matched by its first name (the rest ignored), so its object is
			// not the one asked for.
			CResourceObj* pObj = dynamic_cast<CResourceObj*>(pRef);
			if ( pObj && (fChainResolved || strchr(pszOperand, '.') == NULL) )
				iValue = (int) pObj->GetUIDIndex();
			return true;
		}
		if ( vValue.IsEmpty() )
			return true;
		LPCTSTR pszValue = vValue.GetPSTR();
		iValue = GetSingle(pszValue);
		return true;
	}

protected:

public:
	// Gump command table for dialog construction.
	static LPCTSTR const sm_szGumpCmds[];

	static bool IsGumpCommand(LPCTSTR pszKey)
	{
		for ( int i = 0; sm_szGumpCmds[i]; i++ )
		{
			size_t len = strlen(sm_szGumpCmds[i]);
			if ( !_strnicmp(pszKey, sm_szGumpCmds[i], len) )
			{
				char ch = pszKey[len];
				if ( ch == '\0' || ch == ' ' || ch == '(' || ch == '\t' )
					return true;
			}
		}
		return false;
	}

public:
	static CScriptPropArray sm_FunctionsAll;

	// Gump accumulator — set during dialog construction (single-threaded safe).
	static CGStringArray* sm_pGumpControls;
	static CGStringArray* sm_pGumpTexts;

	static void InitFunctions()
	{
		// Base exec context has no special functions of its own.
		// Subclasses (CSphereExpContext) add theirs via AddProps.
	}

public:
	CGVariant m_vValRet;
	CVarDefArray m_ArgArray;
	// Named ARG variables belong to this function/trigger execution context.
	// Nested contexts get fresh storage and discard it on return.
	CScriptLocalArgs m_LocalArgs;

	CScriptExecContext(CScriptObj* pObj, CScriptConsole* pConsole)
		: m_pBaseObj(pObj), m_pSrc(pConsole)
	{
	}

public:
	virtual HRESULT Function_Dispatch(LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet)
	{
		if ( !_stricmp(pszKey, "ARG") )
		{
			LPCTSTR pszArgs = vArgs.GetPSTR();
			if ( !pszArgs || !*pszArgs )
				return HRES_BAD_ARG_QTY;

			// ARG(name,value) treats everything after the first comma as one value.
			LPCTSTR pszComma = strchr(pszArgs, ',');
			size_t iNameLen = pszComma ? static_cast<size_t>(pszComma - pszArgs) : strlen(pszArgs);
			if ( iNameLen >= SCRIPT_MAX_LINE_LEN )
				return HRES_BAD_ARGUMENTS;

			TCHAR szName[SCRIPT_MAX_LINE_LEN];
			memcpy(szName, pszArgs, iNameLen);
			szName[iNameLen] = '\0';
			TCHAR* pszName = szName;
			while ( *pszName == ' ' || *pszName == '\t' )
				pszName++;
			if ( !*pszName )
				return HRES_BAD_ARGUMENTS;

			if ( pszComma )
			{
				LPCTSTR pszValue = pszComma + 1;
				while ( *pszValue == ' ' || *pszValue == '\t' )
					pszValue++;
				if ( *pszValue == '#' )
				{
					// Sphere's # prefix means "the current value". Evaluate the
					// suffix as an expression so #+1, #-1 and #*2 retain their
					// normal arithmetic meaning.
					CGVariant vCurrent;
					m_LocalArgs.FindKeyVar(pszName, vCurrent);
					TCHAR szExpression[SCRIPT_MAX_LINE_LEN];
					snprintf(szExpression, sizeof(szExpression), "%d%s", vCurrent.GetInt(), pszValue + 1);
					m_LocalArgs.SetKeyInt(pszName, (DWORD)GetComplex(szExpression));
				}
				else
					m_LocalArgs.SetKeyStr(pszName, pszValue);
			}
			if ( !m_LocalArgs.FindKeyVar(pszName, vValRet) )
				vValRet.SetStr("");
			return NO_ERROR;
		}
		if ( !_strnicmp(pszKey, "ARG.", 4) )
		{
			LPCTSTR pszName = pszKey + 4;
			if ( !*pszName )
				return HRES_BAD_ARGUMENTS;
			if ( !m_LocalArgs.FindKeyVar(pszName, vValRet) )
				vValRet.SetStr("");
			return NO_ERROR;
		}
		// Bare ARG locals are valid in both escape tags (<i>) and numeric
		// expressions (WHILE (i < 5)). Keep legacy DEFNAME/object lookup as
		// the fallback for names that are not local to this context.
		if ( strchr(pszKey, '.') == NULL && strchr(pszKey, '(') == NULL &&
			m_LocalArgs.FindKeyVar(pszKey, vValRet) )
			return NO_ERROR;

		// Subclasses override this for additional functions.
		return HRES_UNKNOWN_PROPERTY;
	}

	int GetScriptExpression(TCHAR* pszArg, size_t iBufCapacity = SCRIPT_MAX_LINE_LEN)
	{
		if ( !pszArg || !*pszArg )
			return 0;

		// Control-flow expressions and RETURN values need the same macro
		// expansion as ordinary command arguments.
		s_ParseEscapes(pszArg, 0, iBufCapacity);

		TCHAR* pszExpr = pszArg;
		while ( ISWHITESPACE(*pszExpr) ) pszExpr++;
		TCHAR* pszEnd = pszExpr + strlen(pszExpr);
		while ( pszEnd > pszExpr && ISWHITESPACE(pszEnd[-1]) )
			*--pszEnd = '\0';

		// Sphere conditions are commonly wrapped in one pair of parentheses.
		// The numeric expression reader accepts the expression inside them.
		if ( pszExpr < pszEnd && *pszExpr == '(' && pszEnd[-1] == ')' )
		{
			int iDepth = 0;
			bool fOuterParens = true;
			for ( TCHAR* p = pszExpr; p < pszEnd; ++p )
			{
				if ( *p == '(' )
					++iDepth;
				else if ( *p == ')' )
				{
					if ( --iDepth == 0 && p != pszEnd - 1 )
					{
						fOuterParens = false;
						break;
					}
					if ( iDepth < 0 )
					{
						fOuterParens = false;
						break;
					}
				}
			}
			if ( fOuterParens && iDepth == 0 )
			{
				pszEnd[-1] = '\0';
				++pszExpr;
			}
		}
		return GetComplex(pszExpr);
	}

	// GetKeywordArg normally returns a pointer into CScript::m_szLine.  Its
	// remaining capacity is smaller when the argument starts after the key;
	// preserve that bound while expanding control-flow expressions.
	int GetScriptExpression(CScript& script, TCHAR* pszArg)
	{
		size_t iBufCapacity = SCRIPT_MAX_LINE_LEN;
		TCHAR* pszLineArg = script.GetArgMod();
		if ( pszArg && pszArg == pszLineArg )
		{
			TCHAR* pszLine = script.GetLineBuffer();
			ptrdiff_t iOffset = pszLineArg - pszLine;
			if ( iOffset >= 0 && iOffset < SCRIPT_MAX_LINE_LEN )
				iBufCapacity = SCRIPT_MAX_LINE_LEN - static_cast<size_t>(iOffset);
		}
		return GetScriptExpression(pszArg, iBufCapacity);
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

	// Check the complete replacement size before shifting the suffix.  The
	// expression itself is temporarily NUL-terminated while it is evaluated,
	// so strlen(pszBuf) cannot be used for this check; iBegin and iTrailLen
	// describe the prefix and suffix explicitly.
	static bool IsEscapeExpansionWithinCapacity(size_t iBegin, size_t iResultLen,
		size_t iTrailLen, size_t iBufCapacity)
	{
		size_t iNewLen = iBegin + iResultLen + iTrailLen;
		if ( iNewLen < iBufCapacity )
			return true;

		DEBUG_ERR(( "Script escape expansion exceeds line buffer (required=%lu capacity=%lu)" LOG_CR,
			(unsigned long) (iNewLen + 1), (unsigned long) iBufCapacity ));
		return false;
	}

	void s_ParseEscapes(TCHAR* pszBuf, DWORD dwFlags,
		size_t iBufCapacity = SCRIPT_MAX_LINE_LEN)
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

			// Handle <?...?> expression macros — alternative delimiters for nesting.
			if ( pszBuf[i+1] == '?' )
			{
				int iBegin = i;
				int iDepth = 1;
				int iEnd = -1;
				for ( int j = i + 2; pszBuf[j]; j++ )
				{
					if ( pszBuf[j] == '<' && pszBuf[j+1] == '?' )
					{
						iDepth++;
						j++;
					}
					else if ( pszBuf[j] == '?' && pszBuf[j+1] == '>' )
					{
						iDepth--;
						if ( iDepth <= 0 )
						{
							iEnd = j + 1; // position of '>'
							break;
						}
						j++;
					}
				}
				if ( iEnd < 0 )
				{
					i++; // skip past '?' to avoid infinite loop
					continue;
				}

				// Extract content between <? and ?>
				pszBuf[iEnd - 1] = '\0'; // null-terminate at '?' of '?>'
				// Resolve nested expressions in a separate buffer.  A nested result can
				// be longer than its source text; parsing it in pszBuf would shift the
				// outer suffix before this replacement gets a chance to copy it.
				TCHAR szNestedExpr[SCRIPT_MAX_LINE_LEN];
				size_t iNestedLen = strlen(pszBuf + iBegin + 2);
				if ( iNestedLen >= SCRIPT_MAX_LINE_LEN )
					iNestedLen = SCRIPT_MAX_LINE_LEN - 1;
				memcpy(szNestedExpr, pszBuf + iBegin + 2, iNestedLen);
				szNestedExpr[iNestedLen] = '\0';
				TCHAR* pszExpr = szNestedExpr; // skip '<?'

				// Recursively resolve inner <...> and <?...?> tags.
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

				// Evaluate the expression (same as <...> evaluation).
				CGString sResult;
				bool fResolved = false;
				CScriptUnknownRejectTracker rejected;
				try
				{
					fResolved = EvaluateEscapeExpression(pszExpr, sResult, rejected);
				}
				catch (...)
				{
					fResolved = fSafe;
				}

				if ( !fResolved )
				{
					if ( !rejected.RecordIfPresent() )
						ScriptUnknownRecord(UnknownExpressionKind(pszExpr), pszExpr, m_pBaseObj);
					if ( fSafe )
					{
						sResult = "";
						fResolved = true;
					}
					else
					{
						pszBuf[iEnd - 1] = '?'; // restore
						i++; // skip past '?'
						continue;
					}
				}

				// Replace <?...?> with result, shifting buffer.
				int iExprLen = iEnd - iBegin + 1;
				int iResultLen = sResult.GetLength();
				int iTrailLen = strlen(pszBuf + iEnd + 1);
				if ( !IsEscapeExpansionWithinCapacity(
					(size_t) iBegin, (size_t) iResultLen, (size_t) iTrailLen, iBufCapacity) )
				{
					pszBuf[iEnd - 1] = '?';
					i = iEnd;
					continue;
				}
				// pszBuf[iEnd-1] is '\0', pszBuf[iEnd] is '>', trailing starts at iEnd+1
				memmove(pszBuf + iBegin + iResultLen, pszBuf + iEnd + 1, iTrailLen + 1);
				memcpy(pszBuf + iBegin, (LPCTSTR)sResult, iResultLen);
				i = iBegin + iResultLen - 1;
				continue;
			}

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
			// Resolve nested expressions in a separate buffer.  A nested result can
			// be longer than its source text; parsing it in pszBuf would shift the
			// outer suffix before this replacement gets a chance to copy it.
			TCHAR szNestedExpr[SCRIPT_MAX_LINE_LEN];
			size_t iNestedLen = strlen(pszBuf + iBegin + 1);
			if ( iNestedLen >= SCRIPT_MAX_LINE_LEN )
				iNestedLen = SCRIPT_MAX_LINE_LEN - 1;
			memcpy(szNestedExpr, pszBuf + iBegin + 1, iNestedLen);
			szNestedExpr[iNestedLen] = '\0';
			TCHAR* pszExpr = szNestedExpr;

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
			CScriptUnknownRejectTracker rejected;

			try
			{
				fResolved = EvaluateEscapeExpression(pszExpr, sResult, rejected);
			}
			catch (...)
			{
				fResolved = fSafe;
			}

			if ( !fResolved )
			{
				if ( !rejected.RecordIfPresent() )
					ScriptUnknownRecord(UnknownExpressionKind(pszExpr), pszExpr, m_pBaseObj);
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
			if ( !IsEscapeExpansionWithinCapacity(
				(size_t) iBegin, (size_t) iResultLen, (size_t) iTrailLen, iBufCapacity) )
			{
				// Restore the '>' and leave the original escape visible to the
				// command handler.  It is safer than silently truncating a valid
				// script line after an expansion does not fit.
				pszBuf[iEnd] = chEnd;
				i = iEnd;
				continue;
			}

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
	// A WHILE or FOR loop that reaches SCRIPT_MAX_LOOP_ITERATIONS stops there.
	// Report each such loop once (file and line), and at most
	// MAX_REPORTED_LOOPS different loops, so a loop that keeps reaching the
	// limit cannot flood the log.
	static void ReportLoopLimit(CScript& script, const CScriptLineContext& ctx, LPCTSTR pszLoop)
	{
		enum { MAX_REPORTED_LOOPS = 64, MAX_SITE_FILE = 64 };
		struct LoopSite
		{
			TCHAR m_szFile[MAX_SITE_FILE];
			int m_iLine;
		};
		static LoopSite s_Reported[MAX_REPORTED_LOOPS];
		static int s_iReported = 0;
		static bool s_fMoreNotReported = false;

		LPCTSTR pszFile = script.GetFileTitle();
		if ( pszFile == NULL )
			pszFile = "";
		for ( int i = 0; i < s_iReported; i++ )
		{
			if ( s_Reported[i].m_iLine == ctx.m_iLineNum &&
				!strncmp(s_Reported[i].m_szFile, pszFile, MAX_SITE_FILE - 1) )
				return;
		}
		if ( s_iReported >= MAX_REPORTED_LOOPS )
		{
			if ( !s_fMoreNotReported )
			{
				s_fMoreNotReported = true;
				DEBUG_ERR(( "More script loops stopped after %d iterations; they are not reported" LOG_CR,
					SCRIPT_MAX_LOOP_ITERATIONS ));
			}
			return;
		}
		strncpy(s_Reported[s_iReported].m_szFile, pszFile, MAX_SITE_FILE - 1);
		s_Reported[s_iReported].m_szFile[MAX_SITE_FILE - 1] = '\0';
		s_Reported[s_iReported].m_iLine = ctx.m_iLineNum;
		s_iReported++;
		DEBUG_ERR(( "%s(%d): %s loop stopped after %d iterations" LOG_CR,
			pszFile, ctx.m_iLineNum, pszLoop, SCRIPT_MAX_LOOP_ITERATIONS ));
	}

	// Split a statement key of the form NAME(args) or REF.NAME(args) (the
	// parentheses closing at the end of the key) into NAME or REF.NAME and
	// the argument text.  Returns false for any other key.
	static bool SplitCallStatement(TCHAR* pszKey, TCHAR*& pszArg)
	{
		size_t iLen = strlen(pszKey);
		if ( iLen < 3 || pszKey[iLen - 1] != ')' )
			return false;

		// Find the '(' that matches the final ')'.
		int iDepth = 0;
		TCHAR* pOpen = NULL;
		for ( TCHAR* q = pszKey + iLen - 1; q >= pszKey; q-- )
		{
			if ( *q == ')' )
				iDepth++;
			else if ( *q == '(' && --iDepth == 0 )
			{
				pOpen = q;
				break;
			}
		}
		if ( pOpen == NULL || pOpen == pszKey )
			return false;

		// The name before it must be a plain identifier segment.
		TCHAR* pName = pOpen;
		while ( pName > pszKey && (isalnum((unsigned char)pName[-1]) || pName[-1] == '_') )
			pName--;
		if ( pName == pOpen || !(isalpha((unsigned char)*pName) || *pName == '_') )
			return false;
		if ( pName != pszKey && pName[-1] != '.' )
			return false;

		pszKey[iLen - 1] = '\0';
		*pOpen = '\0';
		pszArg = pOpen + 1;
		while ( ISWHITESPACE(*pszArg) )
			pszArg++;
		return true;
	}

	HRESULT ExecuteCommand(LPCTSTR pszCmd, bool fScriptKeyEquals = false)
	{
		if ( !pszCmd || !*pszCmd )
			return NO_ERROR;

		// Skip leading whitespace
		while ( ISWHITESPACE(*pszCmd) )
			pszCmd++;
		if ( !*pszCmd || *pszCmd == '/' )
			return NO_ERROR; // blank or comment

		CScriptUnknownRejectTracker rejected;

		// ARG(name,value) is a statement as well as an expression.  The usual
		// command splitter treats its whole parenthesized form as the key, so
		// route this one function-style statement through the active context.
		if ( !_strnicmp(pszCmd, "ARG(", 4) )
		{
			LPCTSTR pszClose = strrchr(pszCmd, ')');
			if ( pszClose )
			{
				LPCTSTR pszTail = pszClose + 1;
				while ( ISWHITESPACE(*pszTail) ) pszTail++;
				if ( !*pszTail )
				{
					size_t iArgsLen = pszClose - (pszCmd + 4);
					TCHAR szArgs[SCRIPT_MAX_LINE_LEN];
					if ( iArgsLen >= sizeof(szArgs) )
					{
						rejected.Observe(HRES_BAD_ARGUMENTS, "ARG", m_pBaseObj);
						rejected.RecordIfPresent();
						return HRES_BAD_ARGUMENTS;
					}
					memcpy(szArgs, pszCmd + 4, iArgsLen);
					szArgs[iArgsLen] = '\0';
					s_ParseEscapes(szArgs, 0);
					CGVariant vArgs(szArgs);
					CGVariant vValRet;
					HRESULT hRes = Function_Dispatch("ARG", vArgs, vValRet);
					rejected.Observe(hRes, "ARG", m_pBaseObj);
					if ( hRes != HRES_UNKNOWN_PROPERTY )
					{
						rejected.RecordIfPresent();
						return hRes;
					}
				}
			}
		}

		// Split into key and arg at the first space or '=' outside
		// parentheses, so KEY(a, b) and ROOT(a, b).KEY stay one key.
		TCHAR szLine[SCRIPT_MAX_LINE_LEN];
		strncpy(szLine, pszCmd, sizeof(szLine) - 1);
		szLine[sizeof(szLine) - 1] = '\0';

		TCHAR* pszKey = szLine;
		TCHAR* pszArg = NULL;
		bool fPropertySet = fScriptKeyEquals;

		// Find the split point
		TCHAR* p = pszKey;
		int iKeyDepth = 0;
		for ( ; *p; p++ )
		{
			if ( *p == '(' )
				iKeyDepth++;
			else if ( *p == ')' )
			{
				if ( iKeyDepth > 0 )
					iKeyDepth--;
			}
			else if ( iKeyDepth == 0 && (ISWHITESPACE(*p) || *p == '=') )
				break;
		}
		if ( *p )
		{
			bool fHasEquals = (*p == '=');
			fPropertySet = fPropertySet || fHasEquals;
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
					fPropertySet = true;
					p++;
					while ( ISWHITESPACE(*p) ) p++;
				}
			}
			pszArg = p;
		}

		if ( !pszArg )
			pszArg = const_cast<TCHAR*>("");

		// A statement written as a call, NAME(args) or REF.NAME(args), runs
		// NAME with those arguments exactly as "NAME args" does.  Gump
		// commands keep their own parenthesized form.
		bool fCallForm = false;
		TCHAR szCallArgs[SCRIPT_MAX_LINE_LEN];
		if ( !fPropertySet && *pszArg == '\0' && !(sm_pGumpControls != NULL &&
			(IsGumpCommand(pszKey) || !_strnicmp(pszKey, "settext", 7))) )
		{
			fCallForm = SplitCallStatement(pszKey, pszArg);
			if ( fCallForm && strchr(pszArg, '<') )
			{
				// An expression can start in the part of the line that was
				// read as the key, as in F_FUNC(<STRMATCH a,b>).
				strncpy(szCallArgs, pszArg, sizeof(szCallArgs) - 1);
				szCallArgs[sizeof(szCallArgs) - 1] = '\0';
				s_ParseEscapes(szCallArgs, 0);
				pszArg = szCallArgs;
			}
		}

		// Try dispatching to the base object.
		CResourceObj* pObj = dynamic_cast<CResourceObj*>(m_pBaseObj);
		if ( pObj && ( !strchr(pszKey, '.') || fPropertySet ))
		{
			HRESULT hRes;
			if ( !fCallForm )
			{
				// Try as a property set (KEY=VALUE).
				CGVariant vVal(pszArg);
				hRes = pObj->s_PropSet(pszKey, vVal);
				rejected.Observe(hRes, pszKey, m_pBaseObj);
				if ( hRes == NO_ERROR )
					return NO_ERROR;
			}

			// Try as a method call (KEY args).
			CGVariant vArgs(pszArg);
			CGVariant vValRet;
			hRes = pObj->s_Method(pszKey, vArgs, vValRet, m_pSrc);
			rejected.Observe(hRes, pszKey, m_pBaseObj);
			if ( hRes == NO_ERROR )
				return NO_ERROR;
		}

		// Allow commands to invoke methods on referenced objects, for example
		// SRC.SYSMESSAGE inside an item trigger. Resolve the left side through
		// the context's function table first, then through the base object's
		// properties (ACT.TRIGGER, CONT.MESSAGE, etc.).
		LPCTSTR pszDot = strchr(pszKey, '.');
		if ( pszDot && pszDot != pszKey && pszDot[1] )
		{
			TCHAR szRoot[SCRIPT_MAX_LINE_LEN];
			size_t iRootLen = pszDot - pszKey;
			if ( iRootLen < sizeof(szRoot) )
			{
				memcpy(szRoot, pszKey, iRootLen);
				szRoot[iRootLen] = '\0';

				// Function-style roots such as FINDUID(uid) carry their own
				// arguments.
				TCHAR szRootName[SCRIPT_MAX_LINE_LEN];
				CGVariant vRootArgs;
				if ( !SplitDottedSegment(szRoot, iRootLen, szRootName, sizeof(szRootName), vRootArgs) )
				{
					strcpy(szRootName, szRoot);
					vRootArgs.SetVoid();
				}
				CGVariant vRoot;
				HRESULT hRoot = Function_Dispatch(szRootName, vRootArgs, vRoot);
				rejected.Observe(hRoot, szRootName, m_pBaseObj);
				bool fRootFromFunction = (hRoot == NO_ERROR);
				if ( hRoot != NO_ERROR && pObj )
				{
					hRoot = pObj->s_PropGet(szRoot, vRoot, m_pSrc);
					rejected.Observe(hRoot, szRoot, m_pBaseObj);
				}

				CResourceObj* pRootObj = ResolveObjectResult(vRoot, fRootFromFunction ? szRootName : NULL);
				if ( hRoot == NO_ERROR && pRootObj )
				{
					HRESULT hRes;
					if ( fPropertySet )
					{
						// ROOT.KEY=value is a property write on the referenced
						// object (SRC.NAME=x, FINDUID(uid).TAG.KEY=x).
						CGVariant vVal(pszArg);
						hRes = pRootObj->s_PropSet(pszDot + 1, vVal);
						rejected.Observe(hRes, pszDot + 1, pRootObj);
						if ( hRes == NO_ERROR )
							return NO_ERROR;
					}
					CGVariant vArgs(pszArg);
					CGVariant vValRet;
					hRes = pRootObj->s_Method(pszDot + 1, vArgs, vValRet, m_pSrc);
					rejected.Observe(hRes, pszDot + 1, pRootObj);
					if ( hRes == NO_ERROR )
						return NO_ERROR;
				}
			}
		}

		// Try as a global function/method dispatch (script [FUNCTION] calls).
		{
			CGVariant vArgs(pszArg);
			CGVariant vValRet;
			HRESULT hRes = Function_Dispatch(pszKey, vArgs, vValRet);
			rejected.Observe(hRes, pszKey, m_pBaseObj);
			if ( hRes == NO_ERROR )
				return NO_ERROR;
		}

		// During dialog construction, intercept gump commands and settext.
		if ( sm_pGumpControls != NULL )
		{
			// Handle settext(id, text) / setText(id, text)
			if ( !_strnicmp(pszKey, "settext", 7) )
			{
				LPCTSTR pArgs = pszKey + 7;
				if ( *pArgs == '(' ) pArgs++;
				else if ( *pArgs == ' ' ) pArgs++;
				else pArgs = pszArg;
				// Parse: id,text
				int iTextID = atoi(pArgs);
				while ( *pArgs && *pArgs != ',' ) pArgs++;
				if ( *pArgs == ',' ) pArgs++;
				// Strip trailing ')'
				TCHAR szText[SCRIPT_MAX_LINE_LEN];
				strncpy(szText, pArgs, sizeof(szText)-1);
				szText[sizeof(szText)-1] = '\0';
				int len = strlen(szText);
				if ( len > 0 && szText[len-1] == ')' )
					szText[len-1] = '\0';
				// Ensure text array is large enough
				if ( sm_pGumpTexts )
				{
					while ( sm_pGumpTexts->GetSize() <= iTextID )
						sm_pGumpTexts->Add( "" );
					sm_pGumpTexts->SetAt( iTextID, szText );
				}
				return NO_ERROR;
			}

			// Handle gump commands: resizepic, text, button, etc.
			// Check both with and without parentheses: "text(x,y,c,id)" or "text x y c id"
			TCHAR szGumpKey[128];
			strncpy(szGumpKey, pszKey, sizeof(szGumpKey)-1);
			szGumpKey[sizeof(szGumpKey)-1] = '\0';
			// Strip trailing '(' from key if present
			TCHAR* pParen = strchr(szGumpKey, '(');
			LPCTSTR pGumpArgs = pszArg;
			if ( pParen )
			{
				*pParen = '\0';
				pGumpArgs = pParen + 1;
				// Strip trailing ')' from args
				int len2 = strlen(pGumpArgs);
				// pGumpArgs might point into pszKey, make a copy
				static TCHAR s_szGA[SCRIPT_MAX_LINE_LEN];
				strncpy(s_szGA, pGumpArgs, sizeof(s_szGA)-1);
				s_szGA[sizeof(s_szGA)-1] = '\0';
				len2 = strlen(s_szGA);
				if ( len2 > 0 && s_szGA[len2-1] == ')' )
					s_szGA[len2-1] = '\0';
				pGumpArgs = s_szGA;
			}

			if ( IsGumpCommand(szGumpKey) )
			{
				// Convert comma-separated args to space-separated for gump protocol.
				TCHAR szGump[SCRIPT_MAX_LINE_LEN];
				if ( pGumpArgs && *pGumpArgs )
				{
					snprintf(szGump, sizeof(szGump), "%s %s", szGumpKey, pGumpArgs);
					// Replace commas with spaces in the args portion
					TCHAR* pComma = szGump + strlen(szGumpKey) + 1;
					for ( ; *pComma; pComma++ )
					{
						if ( *pComma == ',' )
							*pComma = ' ';
					}
				}
				else
				{
					strncpy(szGump, szGumpKey, sizeof(szGump)-1);
				}
				sm_pGumpControls->Add(szGump);
				return NO_ERROR;
			}
		}

		// Unknown command -- not an error for now, just ignore.
		if ( !rejected.RecordIfPresent() )
		{
			ScriptUnknownRecord(
				fPropertySet ? SCRIPT_UNKNOWN_SET :
					((fCallForm || strchr(pszKey, '(')) ? SCRIPT_UNKNOWN_FUNCTION : SCRIPT_UNKNOWN_METHOD),
				pszKey,
				m_pBaseObj);
		}
		return HRES_UNKNOWN_PROPERTY;
	}

	// The control-flow keyword a statement key starts with, as a whole word:
	// ELSEIF is not ELSE, and a name such as FOR_X or FORMAT is not FOR.  The
	// keyword may be followed directly by its argument, as in IF(<x>).
	// Returns SK_QTY for any other key; iLen is the keyword length.
	static SK_TYPE FindScriptKeyword(LPCTSTR pszKey, size_t& iLen)
	{
		iLen = 0;
		for ( int i = 0; sm_szScriptKeys[i]; i++ )
		{
			size_t iKeyLen = strlen(sm_szScriptKeys[i]);
			if ( _strnicmp(pszKey, sm_szScriptKeys[i], iKeyLen) )
				continue;
			TCHAR ch = pszKey[iKeyLen];
			if ( isalnum((unsigned char) ch) || ch == '_' || ch == '.' )
				continue;
			iLen = iKeyLen;
			return (SK_TYPE) i;
		}
		return SK_QTY;
	}

	// The argument of the control-flow keyword that starts the current line.
	// The line reader splits a line at its first space or '=', so in
	// IF(<a>==<b>) the argument starts inside the key: rebuild it there.
	static TCHAR* GetKeywordArg(CScript& script, size_t iKeywordLen, TCHAR* pszBuf, size_t iBufSize)
	{
		LPCTSTR pszKey = script.GetKey();
		TCHAR* pszArg = script.GetArgMod();
		if ( pszKey[iKeywordLen] == '\0' )
			return pszArg ? pszArg : const_cast<TCHAR*>("");
		LPCTSTR pszRest = pszKey + iKeywordLen;
		if ( pszArg && *pszArg )
			snprintf(pszBuf, iBufSize, "%s%c%s", pszRest,
				script.WasKeyValueAssignment() ? '=' : ' ', pszArg);
		else
			snprintf(pszBuf, iBufSize, "%s", pszRest);
		return pszBuf;
	}

	// A context that builds something from a section (a dialog layout) can
	// take a statement line before the generic dispatch.  Control-flow lines
	// never come here.  Returns true when the line was handled.
	virtual bool OnScriptStatement(CScript& script)
	{
		(void) script;
		return false;
	}

	// Called when a RETURN statement ends the section, with or without a value.
	virtual void OnScriptReturn()
	{
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
		CScriptUnknownContextScope scriptContextScope(&script);
		bool fSectionFalse = (type == TRIGRUN_SECTION_FALSE || type == TRIGRUN_SINGLE_FALSE);
		TCHAR szArg[SCRIPT_MAX_LINE_LEN];	// a keyword argument rebuilt by GetKeywordArg

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
			size_t iKeywordLen;
			SK_TYPE index = FindScriptKeyword(pszKey, iKeywordLen);

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
					OnScriptReturn();
					TCHAR* pszArg = GetKeywordArg(script, iKeywordLen, szArg, sizeof(szArg));
					if ( *pszArg )
					{
						int iVal = GetScriptExpression(script, pszArg);
						m_vValRet.SetInt(iVal);
						return (TRIGRET_TYPE) iVal;
					}
					return TRIGRET_RET_DEFAULT;
				}

			case SK_IF:
				{
					// IF <condition>
					TCHAR* pszArg = GetKeywordArg(script, iKeywordLen, szArg, sizeof(szArg));
					int fCondition = 0;
					if ( *pszArg )
						fCondition = GetScriptExpression(script, pszArg);
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
							size_t iElseLen;
							FindScriptKeyword(script.GetKey(), iElseLen);
							pszArg = GetKeywordArg(script, iElseLen, szArg, sizeof(szArg));
							fCondition = *pszArg ? GetScriptExpression(script, pszArg) : 0;
						}
					}
				}
				break;

			case SK_WHILE:
				{
					// WHILE <condition>
					CScriptLineContext ctxStart = script.GetContext();
					TCHAR szCondition[SCRIPT_MAX_LINE_LEN];
					LPCTSTR pszCondition = GetKeywordArg(script, iKeywordLen, szCondition, sizeof(szCondition));
					if ( pszCondition != szCondition )
					{
						strncpy(szCondition, pszCondition, sizeof(szCondition)-1);
						szCondition[sizeof(szCondition)-1] = '\0';
					}
					int iLoops = 0;
					for (;;)
					{
						if ( ++iLoops > SCRIPT_MAX_LOOP_ITERATIONS )
						{
							ReportLoopLimit(script, ctxStart, "WHILE");
							break; // safety limit
						}

						// Re-expand the original condition so changed local values are seen.
						TCHAR szConditionEval[SCRIPT_MAX_LINE_LEN];
						strncpy(szConditionEval, szCondition, sizeof(szConditionEval)-1);
						szConditionEval[sizeof(szConditionEval)-1] = '\0';
						int fCond = GetScriptExpression(szConditionEval);
						if ( !fCond )
						{
							iRet = TRIGRET_ENDIF;
							break;
						}

						iRet = ExecuteScript(script, TRIGRUN_SECTION_TRUE);
						if ( iRet == TRIGRET_BREAK )
							break;
						if ( iRet != TRIGRET_ENDIF && iRet != TRIGRET_CONTINUE )
							return iRet;
						script.SeekContext(ctxStart);
					}
					// Skip past the ENDWHILE if we didn't enter / broke out.
					if ( iRet == TRIGRET_BREAK || script.GetContext().m_lOffset <= ctxStart.m_lOffset )
					{
						// Need to skip the block.
						ExecuteScript(script, TRIGRUN_SECTION_FALSE);
					}
				}
				break;

			case SK_FOR:
				{
					// FOR <max> or FOR <min> <max>
					CScriptLineContext ctxStart = script.GetContext();
					CScriptLineContext ctxEnd = ctxStart;
					TCHAR* pszArg = GetKeywordArg(script, iKeywordLen, szArg, sizeof(szArg));
					int iMin = 1, iMax = 0;
					if ( *pszArg )
					{
						iMax = GetScriptExpression(script, pszArg);
					}
					int iLoops = 0;
					for ( int i = iMin; i <= iMax; i++ )
					{
						if ( ++iLoops > SCRIPT_MAX_LOOP_ITERATIONS )
						{
							ReportLoopLimit(script, ctxStart, "FOR");
							break;
						}
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
					TCHAR* pszArg = GetKeywordArg(script, iKeywordLen, szArg, sizeof(szArg));
					int iVal = *pszArg ? GetScriptExpression(script, pszArg) : 0;
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
				if ( OnScriptStatement(script) )
					break;
				{
					// Expand script expressions before dispatching the command: in
					// the key first (FINDUID(<VAR.x>).REMOVE, F_FUNC(<ARGS>)), then
					// in the arguments.  This is the path used by server-side
					// triggers such as SYSMESSAGE, and it is also what makes
					// table-backed expressions (EVAL, STRLEN, etc.) observable from
					// a fixture without a graphical client.
					TCHAR szKey[SCRIPT_MAX_LINE_LEN];
					strncpy(szKey, pszKey, sizeof(szKey) - 1);
					szKey[sizeof(szKey) - 1] = '\0';
					if ( strchr(szKey, '<') )
						s_ParseEscapes( szKey, 0 );
					if ( script.GetArgMod() && *script.GetArgMod() )
						s_ParseEscapes( script.GetArgMod(), 0,
							SCRIPT_MAX_LINE_LEN - (script.GetArgMod() - script.GetLineBuffer()) );

					// Rebuild the statement: "KEY VALUE", or "KEY=VALUE" for an
					// assignment.  The line reader splits at the first space or '=',
					// which can fall inside the parentheses of KEY(a, b); then
					// ExecuteCommand() splits the rebuilt text again outside the
					// parentheses.
					bool fKeyEquals = script.WasKeyValueAssignment();
					int iKeyDepth = 0;
					for ( LPCTSTR q = szKey; *q; q++ )
					{
						if ( *q == '(' )
							iKeyDepth++;
						else if ( *q == ')' && iKeyDepth > 0 )
							iKeyDepth--;
					}
					TCHAR szCmd[SCRIPT_MAX_LINE_LEN];
					LPCTSTR pszArgStr = script.GetArgRaw();
					LPCTSTR pszSeparator = " ";
					if ( iKeyDepth != 0 && fKeyEquals )
						pszSeparator = "=";
					if ( pszArgStr && *pszArgStr )
						snprintf(szCmd, sizeof(szCmd), "%s%s%s", szKey, pszSeparator, pszArgStr);
					else
						strncpy(szCmd, szKey, sizeof(szCmd) - 1);
					szCmd[sizeof(szCmd) - 1] = '\0';
					ExecuteCommand(szCmd, fKeyEquals && iKeyDepth == 0);
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

// Gump command names recognized during dialog construction.
inline LPCTSTR const CScriptExecContext::sm_szGumpCmds[] =
{
	"resizepic", "gumppic", "tilepic", "text", "texta", "croppedtext",
	"htmlgump", "htmlgumpa", "xmfhtmlgump", "button", "radio", "checkbox",
	"textentry", "textentrya", "page", "group", "nomove", "noclose",
	"nodispose", "gumppictiled", "checkertrans", "xmfhtmlgumpcolor",
	"tilepichue",
	NULL
};

// Static gump accumulator pointers (single-threaded safe).
inline CGStringArray* CScriptExecContext::sm_pGumpControls = NULL;
inline CGStringArray* CScriptExecContext::sm_pGumpTexts = NULL;

#endif // _INC_CSCRIPTEXECCONTEXT_H

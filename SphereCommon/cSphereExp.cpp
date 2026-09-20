//
// CSphereExp.cpp
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#include "stdafx.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#ifdef __GNUG__
#include <cxxabi.h>
#endif

#pragma pop_macro("max")
#pragma pop_macro("min")

static CSphereExpContext g_Exp( NULL, &g_Serv );	// default expression context.

// DEFNAME resolver for CExpression — resolves identifiers like MT_WALK to their numeric values
static int ResolveDefName(LPCTSTR pszName)
{
	CVarDef* pVar = g_Cfg.m_Const.FindKeyPtr(pszName);
	if (pVar)
		return pVar->GetValNum();
	return 0;
}

// Initialize the static member
CExpression::DEFNAME_RESOLVER CExpression::sm_fnResolveDefName = &ResolveDefName;

CExpression* Exp_GetContext()
{
	// Accesses the globals,locals and the current context current 'this' object.
	// Diff context depending on current CSphereThread?
	CSphereThread* pTask = CSphereThread::GetCurrentThread();
	if ( pTask && pTask->m_pExecContext )
	{
		return pTask->m_pExecContext;
	}
	return &g_Exp;
}

namespace
{
	const size_t UNKNOWN_KEYWORD_REPORT_LIMIT = 1024;
	const size_t UNKNOWN_KEYWORD_MAX_NAME = 128;
	const size_t UNKNOWN_KEYWORD_MAX_SOURCE = 256;
	const size_t UNKNOWN_KEYWORD_MAX_OBJECT_TYPE = 96;

	struct UnknownKeywordEntry
	{
		std::string kind;
		std::string keyword;
		uint64_t count;
		std::string firstFile;
		int firstLine;
		std::string firstObjectType;

		UnknownKeywordEntry()
			: count(0), firstLine(0)
		{
		}
	};

	struct UnknownKeywordReporter
	{
		std::mutex mutex;
		std::string path;
		std::unordered_map<std::string, UnknownKeywordEntry> entries;
		uint64_t total;
		uint64_t overflow;

		UnknownKeywordReporter()
			: total(0), overflow(0)
		{
		}
	};

	UnknownKeywordReporter g_UnknownKeywordReporter;
	std::atomic<bool> g_UnknownKeywordReportEnabled(false);

	LPCTSTR UnknownKeywordKindName(SCRIPT_UNKNOWN_KIND kind)
	{
		switch ( kind )
		{
		case SCRIPT_UNKNOWN_GET: return "get";
		case SCRIPT_UNKNOWN_SET: return "set";
		case SCRIPT_UNKNOWN_METHOD: return "method";
		case SCRIPT_UNKNOWN_FUNCTION: return "function";
		case SCRIPT_UNKNOWN_TRIGGER: return "trigger";
		case SCRIPT_UNKNOWN_REJECTED: return "rejected";
		default: return "unknown";
		}
	}

	std::string NormalizeUnknownKeyword(LPCTSTR pszKeyword)
	{
		std::string result;
		if ( !pszKeyword )
			return "<EMPTY>";

		for ( LPCTSTR p = pszKeyword; *p && result.size() < UNKNOWN_KEYWORD_MAX_NAME; )
		{
			if ( ISWHITESPACE(*p) || *p == '(' || *p == '=' )
				break;

			if ( *p == '.' )
			{
				result += ".*";
				break;
			}

			if ( *p == '[' )
			{
				LPCTSTR pEnd = strchr(p + 1, ']');
				if ( pEnd )
				{
					bool fNumericIndex = pEnd > p + 1;
					for ( LPCTSTR pIndex = p + 1; pIndex < pEnd; ++pIndex )
					{
						if ( !isdigit(static_cast<unsigned char>(*pIndex)) )
							fNumericIndex = false;
					}
					if ( fNumericIndex )
					{
						result += "[]";
						p = pEnd + 1;
						continue;
					}
				}
			}

			result += static_cast<char>(toupper(static_cast<unsigned char>(*p)));
			++p;
		}

		if ( result.empty() )
			return "<EMPTY>";
		if ( result.size() > UNKNOWN_KEYWORD_MAX_NAME )
			result.resize(UNKNOWN_KEYWORD_MAX_NAME);
		return result;
	}

	std::string UnknownObjectType(const CScriptObj* pObj)
	{
		if ( !pObj )
			return "none";

		const char* pszName = typeid(*pObj).name();
#ifdef __GNUG__
		int iStatus = 0;
		char* pszDemangled = abi::__cxa_demangle(pszName, NULL, NULL, &iStatus);
		if ( iStatus == 0 && pszDemangled )
		{
			std::string result(pszDemangled);
			free(pszDemangled);
			return result.substr(0, UNKNOWN_KEYWORD_MAX_OBJECT_TYPE);
		}
		free(pszDemangled);
#endif
		return std::string(pszName ? pszName : "CScriptObj").substr(0, UNKNOWN_KEYWORD_MAX_OBJECT_TYPE);
	}

	std::string JsonEscape(const std::string& value)
	{
		std::string result;
		for ( size_t i = 0; i < value.size(); ++i )
		{
			unsigned char ch = static_cast<unsigned char>(value[i]);
			switch ( ch )
			{
			case '"': result += "\\\""; break;
			case '\\': result += "\\\\"; break;
			case '\b': result += "\\b"; break;
			case '\f': result += "\\f"; break;
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			default:
				if ( ch < 0x20 )
				{
					static const char hex[] = "0123456789abcdef";
					result += "\\u00";
					result += hex[(ch >> 4) & 0x0f];
					result += hex[ch & 0x0f];
				}
				else
				{
					result += static_cast<char>(ch);
				}
				break;
			}
		}
		return result;
	}

	std::string CsvEscape(const std::string& value)
	{
		if ( value.find_first_of(",\"\r\n") == std::string::npos )
			return value;
		std::string result = "\"";
		for ( size_t i = 0; i < value.size(); ++i )
		{
			if ( value[i] == '"' )
				result += '"';
			result += value[i];
		}
		result += '"';
		return result;
	}

	void WriteUnknownKeywordJson(
		std::ofstream& output,
		const std::vector<UnknownKeywordEntry>& entries,
		uint64_t total,
		uint64_t overflow)
	{
		output << "{\n  \"distinct\": " << entries.size()
			<< ",\n  \"total\": " << total
			<< ",\n  \"overflow\": " << overflow
			<< ",\n  \"entries\": [";
		for ( size_t i = 0; i < entries.size(); ++i )
		{
			const UnknownKeywordEntry& entry = entries[i];
			output << (i ? ",\n" : "\n")
				<< "    {\"kind\": \"" << JsonEscape(entry.kind)
				<< "\", \"keyword\": \"" << JsonEscape(entry.keyword)
				<< "\", \"count\": " << entry.count
				<< ", \"first_file\": \"" << JsonEscape(entry.firstFile)
				<< "\", \"first_line\": " << entry.firstLine
				<< ", \"first_object_type\": \"" << JsonEscape(entry.firstObjectType)
				<< "\"}";
		}
		if ( !entries.empty() )
			output << '\n';
		output << "  ]\n}\n";
	}

	void WriteUnknownKeywordCsv(
		std::ofstream& output,
		const std::vector<UnknownKeywordEntry>& entries,
		uint64_t total,
		uint64_t overflow)
	{
		output << "kind,keyword,count,first_file,first_line,first_object_type\n";
		for ( size_t i = 0; i < entries.size(); ++i )
		{
			const UnknownKeywordEntry& entry = entries[i];
			output << CsvEscape(entry.kind) << ','
				<< CsvEscape(entry.keyword) << ',' << entry.count << ','
				<< CsvEscape(entry.firstFile) << ',' << entry.firstLine << ','
				<< CsvEscape(entry.firstObjectType) << '\n';
		}
		output << "overflow,*," << overflow << ",,,\n";
		output << "total,*," << total << ",,,\n";
	}
}

const CScript* ScriptUnknownSetContext(const CScript* pScript)
{
	CSphereThread* pThread = CSphereThread::GetCurrentThread();
	return pThread ? pThread->SetScriptContext(pScript) : NULL;
}

void ScriptUnknownReportSetPath(LPCTSTR pszPath)
{
	std::lock_guard<std::mutex> lock(g_UnknownKeywordReporter.mutex);
	g_UnknownKeywordReporter.path = (pszPath && *pszPath) ? pszPath : "";
	g_UnknownKeywordReportEnabled.store(!g_UnknownKeywordReporter.path.empty(), std::memory_order_release);
}

bool ScriptUnknownReportIsEnabled()
{
	return g_UnknownKeywordReportEnabled.load(std::memory_order_acquire);
}

bool ScriptUnknownResultIsRejected(HRESULT hRes)
{
	return hRes == HRES_BAD_ARG_QTY ||
		hRes == HRES_BAD_ARGUMENTS ||
		hRes == HRES_INVALID_HANDLE ||
		hRes == HRES_INVALID_INDEX ||
		hRes == HRES_INVALID_FUNCTION;
}

void ScriptUnknownRecord(SCRIPT_UNKNOWN_KIND kind, LPCTSTR pszKeyword, const CScriptObj* pObj)
{
	if ( !ScriptUnknownReportIsEnabled() )
		return;

	UnknownKeywordReporter& reporter = g_UnknownKeywordReporter;
	std::lock_guard<std::mutex> lock(reporter.mutex);
	if ( reporter.path.empty() )
		return;

	++reporter.total;
	std::string sKind = UnknownKeywordKindName(kind);
	std::string sKeyword = NormalizeUnknownKeyword(pszKeyword);
	std::string sKey = sKind + '\x1f' + sKeyword;
	std::unordered_map<std::string, UnknownKeywordEntry>::iterator it = reporter.entries.find(sKey);
	if ( it != reporter.entries.end() )
	{
		++it->second.count;
		return;
	}
	if ( reporter.entries.size() >= UNKNOWN_KEYWORD_REPORT_LIMIT )
	{
		++reporter.overflow;
		return;
	}

	UnknownKeywordEntry entry;
	entry.kind = sKind;
	entry.keyword = sKeyword;
	entry.count = 1;
	entry.firstObjectType = UnknownObjectType(pObj);
	entry.firstFile = "<native>";
	if ( entry.firstObjectType.size() > UNKNOWN_KEYWORD_MAX_OBJECT_TYPE )
		entry.firstObjectType.resize(UNKNOWN_KEYWORD_MAX_OBJECT_TYPE);

	CSphereThread* pThread = CSphereThread::GetCurrentThread();
	if ( pThread && pThread->m_pScriptContext )
	{
		entry.firstFile = pThread->m_pScriptContext->GetFileTitle();
		if ( entry.firstFile.size() > UNKNOWN_KEYWORD_MAX_SOURCE )
			entry.firstFile.resize(UNKNOWN_KEYWORD_MAX_SOURCE);
		entry.firstLine = pThread->m_pScriptContext->GetContext().m_iLineNum;
	}
	reporter.entries.insert(std::make_pair(sKey, entry));
}

bool ScriptUnknownReportWrite()
{
	std::string path;
	std::vector<UnknownKeywordEntry> entries;
	uint64_t total = 0;
	uint64_t overflow = 0;
	{
		std::lock_guard<std::mutex> lock(g_UnknownKeywordReporter.mutex);
		path = g_UnknownKeywordReporter.path;
		if ( path.empty() )
			return false;
		total = g_UnknownKeywordReporter.total;
		overflow = g_UnknownKeywordReporter.overflow;
		entries.reserve(g_UnknownKeywordReporter.entries.size());
		for ( std::unordered_map<std::string, UnknownKeywordEntry>::const_iterator it =
			g_UnknownKeywordReporter.entries.begin(); it != g_UnknownKeywordReporter.entries.end(); ++it )
		{
			entries.push_back(it->second);
		}
	}

	std::sort(entries.begin(), entries.end(), [](const UnknownKeywordEntry& left, const UnknownKeywordEntry& right)
	{
		if ( left.kind != right.kind )
			return left.kind < right.kind;
		return left.keyword < right.keyword;
	});

	std::ofstream output(path.c_str(), std::ios::out | std::ios::trunc);
	if ( !output.is_open() )
		return false;

	std::string extension;
	size_t dot = path.find_last_of('.');
	if ( dot != std::string::npos )
	{
		extension = path.substr(dot);
		for ( size_t i = 0; i < extension.size(); ++i )
			extension[i] = static_cast<char>(tolower(static_cast<unsigned char>(extension[i])));
	}
	if ( extension == ".csv" )
		WriteUnknownKeywordCsv(output, entries, total, overflow);
	else
		WriteUnknownKeywordJson(output, entries, total, overflow);
	output.close();
	return !output.fail();
}

//***************************************************************************
//	CSphereScriptContext

void CSphereScriptContext::OpenScript( const CScript* pScriptContext )
{
	// NOTE: These should be called stack based and therefore on the same thread.
	CloseScript();
	m_pScriptContext = pScriptContext;
	m_pPrvScriptContext = g_Cfg.SetScriptContext( pScriptContext );
}

void CSphereScriptContext::CloseScript()
{
	if ( m_pScriptContext )
	{
		m_pScriptContext = NULL;
		g_Cfg.SetScriptContext( m_pPrvScriptContext );
	}
}

//***************************************************************************

const CScriptPropX CSphereExpContext::sm_Functions[CSphereExpContext::F_QTY+1] =	// static
{
#define GLOBALMETHOD(a,b,c) CSCRIPT_PROPX_IMP(a,b,c)
#include "globalmethods.tbl"
#undef GLOBALMETHOD
	NULL,
};

CScriptPropArray CSphereExpContext::sm_FunctionsAll;	// static

CSphereExpContext::CSphereExpContext( CResourceObj* pBaseObj, CScriptConsole* pSrc )
	: CScriptExecContext(pBaseObj,pSrc)
{
	// NOTE: These should be called stack based and therefore on the same thread.
	CSphereThread* pThread = CSphereThread::GetCurrentThread();
	if ( pThread )
	{
		m_pPrvExecContext = pThread->SetExecContext( this );
	}
	else
	{
		m_pPrvExecContext = NULL;
	}
	m_iPrvTask = g_Serv.m_Profile.GetTaskCurrent();
	g_Serv.m_Profile.SwitchTask(PROFILE_Scripts);
}

CSphereExpContext::~CSphereExpContext()
{
	if ( m_iPrvTask >= 0 )
	{
		g_Serv.m_Profile.SwitchTask(m_iPrvTask);
	}
	CSphereThread* pThread = CSphereThread::GetCurrentThread();
	if ( pThread )
	{
		pThread->SetExecContext( m_pPrvExecContext );
	}
}

CResourceObj* CSphereExpContext::ResolveUIDObject(UID_INDEX uid)
{
	CObjBasePtr pObj = g_World.ObjFind(uid);
	return dynamic_cast<CResourceObj*>((CObjBase*)pObj);
}

bool CSphereExpContext::IsScriptFunction(LPCTSTR pszKey)
{
	CSphereUID ridFunc = g_Cfg.ResourceCheckIDType(RES_Function, pszKey);
	return ridFunc.IsValidRID();
}

void CSphereExpContext::InitFunctions()	// static
{
	if ( sm_FunctionsAll.GetSize())
		return;

	CScriptExecContext::InitFunctions();
	sm_FunctionsAll.AddProps( CScriptExecContext::sm_FunctionsAll.GetData(), CScriptExecContext::sm_FunctionsAll.GetSize() );
	sm_FunctionsAll.AddProps( sm_Functions );
}

HRESULT CSphereExpContext::Function_Dispatch( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet )
{
	// Evaluate an identifier.
	// Find the key in the defs collection
	// Skip to the end of the identifier name. ( + any args? )
	// The name can only be valid.

	// Access globals methods
	// Get a global var ref - A key name that just links to another object.
	// SRC is the source console's attached script object (normally its character).
	// Returning the ref lets s_ParseEscapes resolve SRC.NAME, SRC.SERIAL, etc.
	if ( !_stricmp(pszKey, "SRC") ||
		(!_strnicmp(pszKey, "SRC.", 4) && pszKey[4]) )
	{
		CScriptConsole* pSrc = GetSrc();
		CScriptObj* pSrcObj = pSrc ? pSrc->GetAttachedObj() : NULL;
		if ( pSrcObj )
		{
			vValRet.SetRef(pSrcObj);
			return NO_ERROR;
		}
	}

	int iProp = s_FindKeyInTable( pszKey, sm_Functions );
	if ( iProp < 0 )
	{
		HRESULT hRes = CScriptExecContext::Function_Dispatch(pszKey,vArgs,vValRet);
		if ( hRes != HRES_UNKNOWN_PROPERTY )
			return hRes;

		// Is it a global function/method? RES_Function
		CSphereUID ridFunc = g_Cfg.ResourceCheckIDType( RES_Function, pszKey );
		if ( ridFunc.IsValidRID())
		{
			CResourceLock sFunction( g_Cfg.ResourceGetDef(ridFunc));
			if ( ! sFunction.IsFileOpen())
				return( HRES_INVALID_HANDLE );
			// create a new sub-context with new args.
			CSphereExpArgs exec( STATIC_CAST(CResourceObj, GetBaseObject()), GetSrc(), vArgs );
			TRIGRET_TYPE iRet = exec.ExecuteScript( sFunction, TRIGRUN_SECTION_TRUE );
			vValRet = exec.m_vValRet;
			return( NO_ERROR );
		}

		// Check global vars and constants.
		if ( g_Cfg.m_Var.FindKeyVar(pszKey,vValRet))
			return NO_ERROR;
		if ( g_Cfg.m_Const.FindKeyVar(pszKey,vValRet))
			return NO_ERROR;

		return HRES_UNKNOWN_PROPERTY;
	}

	switch(iProp)
	{
	case F_AccountMgr:
		// Ref to the account manager. (for creating new accounts)
		vValRet.SetRef(&g_Accounts);
		break;
	case F_Account:
	case F_FindAccount:
		// lookup a specific account by name.
		vValRet.SetRef( g_Accounts.Account_FindNameCheck(vArgs));
		break;
	case F_Srv:
	case F_Serv:
		// "LASTNEWITEM" etc
		vValRet.SetRef(&g_Serv);
		break;
	case F_FindUID:
	// case F_UID:
		if ( vArgs.IsEmpty())
			return( HRES_BAD_ARG_QTY );
		vValRet.SetRef( g_Cfg.FindUID( vArgs.GetUID()));
		break;
	case F_Var:
		return g_Cfg.m_Var.s_MethodTags( vArgs, vValRet, GetSrc() );

	case F_Eval:
		{
			// Evaluate a numeric expression and return the result.
			LPCTSTR pszStr = vArgs.GetPSTR();
			if ( pszStr == NULL ) { vValRet.SetInt(0); break; }
			long lVal = GetValue(pszStr);
			vValRet.SetInt(lVal);
		}
		break;

	case F_Safe:
		{
			// Error-safe wrapper: try to resolve the expression, return 0 on failure.
			// <safe EXPR> or <safe(EXPR)>
			LPCTSTR pszStr = vArgs.GetPSTR();
			if ( pszStr == NULL ) { vValRet.SetInt(0); break; }
			try
			{
				// Try as a function dispatch first.
				TCHAR szKey[SCRIPT_MAX_LINE_LEN];
				strncpy(szKey, pszStr, sizeof(szKey)-1);
				szKey[sizeof(szKey)-1] = '\0';
				// Split at first space or '('
				TCHAR* pArg = szKey;
				while ( *pArg && *pArg != ' ' && *pArg != '(' ) pArg++;
				CGVariant vInnerArgs;
				if ( *pArg )
				{
					*pArg++ = '\0';
					if ( *pArg == '(' ) pArg++; // skip open paren
					vInnerArgs = pArg;
				}
				CGVariant vInnerRet;
				HRESULT hRes = Function_Dispatch(szKey, vInnerArgs, vInnerRet);
				if ( ScriptUnknownResultIsRejected(hRes) )
					ScriptUnknownRecord(SCRIPT_UNKNOWN_REJECTED, szKey, GetBaseObject());
				if ( hRes == NO_ERROR )
				{
					vValRet = vInnerRet;
					break;
				}
				// Try as a numeric expression.
				long lVal = GetValue(pszStr);
				vValRet.SetInt(lVal);
			}
			catch (...)
			{
				vValRet.SetInt(0);
			}
		}
		break;

	case F_StrLen:
		{
			LPCTSTR pszStr = vArgs.GetPSTR();
			if ( pszStr == NULL ) { vValRet.SetInt(0); break; }
			// Strip surrounding quotes if present.
			if ( *pszStr == '"' )
			{
				pszStr++;
				int len = strlen(pszStr);
				if ( len > 0 && pszStr[len-1] == '"' )
					len--;
				vValRet.SetInt(len);
			}
			else
			{
				vValRet.SetInt(strlen(pszStr));
			}
		}
		break;

	case F_StrCmp:
		{
			// strcmp(str1,str2) — case-sensitive compare.
			TCHAR szTmp[SCRIPT_MAX_LINE_LEN];
			strncpy(szTmp, vArgs.GetPSTR() ? vArgs.GetPSTR() : "", sizeof(szTmp)-1);
			szTmp[sizeof(szTmp)-1] = '\0';
			TCHAR* ppArgs[2] = { NULL, NULL };
			Str_ParseCmds(szTmp, ppArgs, 2, ",");
			if ( ppArgs[0] == NULL || ppArgs[1] == NULL )
			{
				vValRet.SetInt(1);
				break;
			}
			vValRet.SetInt(strcmp(ppArgs[0], ppArgs[1]));
		}
		break;

	case F_StrCmpI:
		{
			// strcmpi(str1,str2) — case-insensitive compare.
			TCHAR szTmp[SCRIPT_MAX_LINE_LEN];
			strncpy(szTmp, vArgs.GetPSTR() ? vArgs.GetPSTR() : "", sizeof(szTmp)-1);
			szTmp[sizeof(szTmp)-1] = '\0';
			TCHAR* ppArgs[2] = { NULL, NULL };
			Str_ParseCmds(szTmp, ppArgs, 2, ",");
			if ( ppArgs[0] == NULL || ppArgs[1] == NULL )
			{
				vValRet.SetInt(1);
				break;
			}
			vValRet.SetInt(_stricmp(ppArgs[0], ppArgs[1]));
		}
		break;

	case F_StrIndexOf:
		{
			// strindexof(haystack,needle[,start]) — find substring, returns -1 if not found.
			TCHAR szTmp[SCRIPT_MAX_LINE_LEN];
			strncpy(szTmp, vArgs.GetPSTR() ? vArgs.GetPSTR() : "", sizeof(szTmp)-1);
			szTmp[sizeof(szTmp)-1] = '\0';
			TCHAR* ppArgs[3] = { NULL, NULL, NULL };
			Str_ParseCmds(szTmp, ppArgs, 3, ",");
			if ( ppArgs[0] == NULL || ppArgs[1] == NULL )
			{
				vValRet.SetInt(-1);
				break;
			}
			int iStart = ppArgs[2] ? atoi(ppArgs[2]) : 0;
			LPCTSTR pHay = ppArgs[0] + iStart;
			LPCTSTR pFound = strstr(pHay, ppArgs[1]);
			vValRet.SetInt( pFound ? (int)(pFound - ppArgs[0]) : -1 );
		}
		break;

	case F_StrMatch:
		{
			// strmatch(str,pattern) — wildcard match.
			TCHAR szTmp[SCRIPT_MAX_LINE_LEN];
			strncpy(szTmp, vArgs.GetPSTR() ? vArgs.GetPSTR() : "", sizeof(szTmp)-1);
			szTmp[sizeof(szTmp)-1] = '\0';
			TCHAR* ppArgs[2] = { NULL, NULL };
			Str_ParseCmds(szTmp, ppArgs, 2, ",");
			if ( ppArgs[0] == NULL || ppArgs[1] == NULL )
			{
				vValRet.SetInt(0);
				break;
			}
			vValRet.SetInt( Str_Match(ppArgs[1], ppArgs[0]) == MATCH_VALID ? 1 : 0 );
		}
		break;

	case F_Rand:
		{
			// rand(n) — random 0..n-1.
			int iMax = vArgs.GetInt();
			vValRet.SetInt( iMax > 0 ? Calc_GetRandVal(iMax) : 0 );
		}
		break;

	case F_QVal:
		{
			// qval(test,ret_neg,ret_zero,ret_pos) — conditional.
			TCHAR szTmp[SCRIPT_MAX_LINE_LEN];
			strncpy(szTmp, vArgs.GetPSTR() ? vArgs.GetPSTR() : "", sizeof(szTmp)-1);
			szTmp[sizeof(szTmp)-1] = '\0';
			TCHAR* ppArgs[4] = { NULL, NULL, NULL, NULL };
			Str_ParseCmds(szTmp, ppArgs, 4, ",");
			int iTest = ppArgs[0] ? atoi(ppArgs[0]) : 0;
			if ( iTest < 0 )
				vValRet.SetInt( ppArgs[1] ? atoi(ppArgs[1]) : 0 );
			else if ( iTest == 0 )
				vValRet.SetInt( ppArgs[2] ? atoi(ppArgs[2]) : 0 );
			else
				vValRet.SetInt( ppArgs[3] ? atoi(ppArgs[3]) : 0 );
		}
		break;

	case F_IsNum:
		{
			LPCTSTR pszStr = vArgs.GetPSTR();
			if ( pszStr == NULL || *pszStr == '\0' ) { vValRet.SetInt(0); break; }
			if ( *pszStr == '-' || *pszStr == '+' ) pszStr++;
			if ( *pszStr == '0' && (pszStr[1] == 'x' || pszStr[1] == 'X') )
			{
				pszStr += 2;
				while ( isxdigit(*pszStr) ) pszStr++;
			}
			else
			{
				while ( isdigit(*pszStr) ) pszStr++;
			}
			vValRet.SetInt( *pszStr == '\0' ? 1 : 0 );
		}
		break;

	default:
		DEBUG_CHECK(0);
		return( HRES_INTERNAL_ERROR );
	}

	return( NO_ERROR );
}

//****************************************************************************
// -CSphereExpArgs

const CScriptPropX CSphereExpArgs::sm_Functions[CSphereExpArgs::F_QTY+1] =	// static
{
#define CSPHEREEXPFUNC(a,b,c) CSCRIPT_PROPX_IMP(a,b,c)
#include "csphereexpfunc.tbl"
#undef CSPHEREEXPFUNC
	NULL,
};

CScriptPropArray CSphereExpArgs::sm_FunctionsAll;	// static

CSphereExpArgs::CSphereExpArgs( CResourceObj* pBase, CScriptConsole* pSrc, LPCTSTR pszStr ) :
	CSphereExpContext(pBase,pSrc),
	m_s1(pszStr)
{
	// attempt to parse this.
	if ( Exp_IsSimpleNumberString(pszStr))
	{
		m_iN1 = Exp_GetComplex(pszStr);
	}
}

CSphereExpArgs::~CSphereExpArgs()
{
}

void CSphereExpArgs::InitFunctions()	// static
{
	if ( sm_FunctionsAll.GetSize())
		return;

	CSphereExpContext::InitFunctions();
	sm_FunctionsAll.AddProps( CSphereExpContext::sm_FunctionsAll.GetData(), CSphereExpContext::sm_FunctionsAll.GetSize() );
	sm_FunctionsAll.AddProps( sm_Functions );
}

HRESULT CSphereExpArgs::Function_Dispatch( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet ) // virtual
{
	s_FixExtendedProp( pszKey, "ArgV", vArgs );
	s_FixExtendedProp( pszKey, "ArgChk", vArgs );
	s_FixExtendedProp( pszKey, "ArgTxt", vArgs );

	// Arg* functions are generated from csphereexpfunc.tbl below.  Keep one
	// dispatch path so aliases such as ARGS1/ARGN2 cannot silently diverge from
	// the table or mask a broken table initializer.
	F_TYPE_ iProp = (F_TYPE_) s_FindKeyInTable( pszKey, sm_Functions );
	if ( iProp < 0 )
	{
		return( CSphereExpContext::Function_Dispatch( pszKey, vArgs, vValRet ));
	}

	switch (iProp)
	{
	case F_ArgChk:
	case F_ArgTxt:
		switch ( vArgs.MakeArraySize())
		{
		case 0:
			return( HRES_BAD_ARG_QTY );
		case 1:
			{
				CGString sName;
				sName.Format( (iProp==F_ArgChk)?"ARGCHK_%d":"ARGTXT_%d", vArgs.GetInt());
				m_ArgArray.FindKeyVar( sName, vValRet );
			}
			break;
		default:
			return HRES_WRITE_FAULT;
		}
		break;
	case F_ArgO:
	case F_ArgO1:
		if ( m_pO1 == NULL )
			return HRES_INVALID_HANDLE;
		vValRet.SetRef( m_pO1 );
		break;
	case F_ArgN:
	case F_ArgN1:
		vValRet.SetInt( m_iN1 );
		break;
	case F_ArgN2:
		vValRet.SetInt( m_iN2 );
		break;
	case F_ArgN3:
		vValRet.SetInt( m_iN3 );
		break;
	case F_ArgPt: // "PT",
		vValRet.SetArrayFormat( "ix,iy,iz", m_iN1, m_iN2, m_iN3 );
		break;
	case F_ArgS:
	case F_ArgS1:
		if ( m_s1.IsEmpty())
		{
			vValRet = m_vVal;
		}
		else
		{
			vValRet = m_s1;
		}
		break;
	case F_ArgV:
		if ( vArgs.IsEmpty())
		{
			vValRet = m_vVal;
		}
		else
		{
			vValRet = m_vVal.GetArrayElement( vArgs.GetInt());
		}
		break;
	case F_ArgVCount:
		vValRet.SetInt( m_vVal.MakeArraySize());
		break;
	default:
		DEBUG_CHECK(0);
		return HRES_INTERNAL_ERROR;
	}

	return( NO_ERROR );
}

void CSphereExpArgs::AddCheck( int n, DWORD dwCheckVal )
{
	// Add local variables to this context.
	// Add an id tagged value ARGCHK#=val
	DEBUG_CHECK(dwCheckVal>100);
	CGString sName;
	sName.Format( "ARGCHK_%d", n );
	m_ArgArray.SetKeyInt( (const char*) sName, dwCheckVal );
	sName.Format( "ARGCHK_%d", dwCheckVal );
	m_ArgArray.SetKeyInt( (const char*) sName, n+1 );
}

void CSphereExpArgs::AddText( int id, const char* pszText )
{
	// Add local variables to this context.
	// Add an id tagged string ARGTXT_#=string
	CString sName;
	sName.Format( "ARGTXT_%d", id );
	m_ArgArray.SetKeyStr( (const char*) sName, pszText );
}

#if 0

//**********************************************

CSphereDefVars::CSphereDefVars() : CResourceObj(UID_INDEX_CLEAR)
{
	IncRefCount();	// static singleton
}
CSphereDefVars::~CSphereDefVars()
{
	StaticDestruct();	// static singleton
}

HRESULT CSphereDefVars::s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc )
{
	// All objects have GetName() but it might mess up scripting here ?
	HRESULT hRes = s_PropGetTags( pszKey, vValRet);
	if ( hRes == NO_ERROR )
		return hRes;
	return( CResourceObj::s_PropGet( pszKey, vValRet, pSrc ));
}
HRESULT CSphereDefVars::s_PropSet( LPCTSTR pszKey, CGVariant& vVal )
{
	HRESULT hRes = s_PropSetTags( pszKey, vVal );
	if ( hRes == NO_ERROR )
		return NO_ERROR;
	return( CResourceObj::s_PropSet( pszKey, vVal ));
}
HRESULT CSphereDefVars::s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc )
{
	// Execute command from script
	// "Set/Get global (saved) variables",
	HRESULT hRes = s_MethodTags(pszKey,vArgs,vValRet,pSrc);
	if ( hRes == NO_ERROR )
		return NO_ERROR;
	return( CResourceObj::s_Method( pszKey, vArgs, vValRet, pSrc ));
}

#ifdef USE_JSCRIPT
#define GLOBALMETHOD(a,b,c)  JSCRIPT_METHOD_IMP(CSphereGlobalObject,a)
#include "globalmethods.tbl"
#undef GLOBALMETHOD
#endif

const CScriptMethod CSphereGlobalObject::sm_Methods[GLF_QTY+1] = 
{
#define GLOBALMETHOD(a,b,c)  CSCRIPT_METHOD_IMP(a,b,c)
#include "globalmethods.tbl"
#undef GLOBALMETHOD
		NULL,
};

CSCRIPT_CLASS_IMP1(SphereGlobalObject,NULL,sm_Methods,NULL,ScriptObj);

#endif

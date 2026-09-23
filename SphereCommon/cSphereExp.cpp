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
#include <memory>
#include <mutex>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <utility>
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

	const size_t SCRIPT_EXECUTION_COVERAGE_LIMIT = 65536;
	const size_t SCRIPT_EXECUTION_COVERAGE_MAX_RESOURCE_NAME = 128;
	const size_t SCRIPT_EXECUTION_COVERAGE_MAX_SECTION_NAME = 128;
	const size_t SCRIPT_EXECUTION_COVERAGE_MAX_SOURCE_FILE = 512;

	struct ScriptExecutionCoverageKey
	{
		int resourceType;
		int resourceIndex;
		int resourcePage;
		DWORD ordinal;
		std::string sectionKind;
		std::string sectionName;

		bool operator==(const ScriptExecutionCoverageKey& other) const
		{
			return resourceType == other.resourceType &&
				resourceIndex == other.resourceIndex &&
				resourcePage == other.resourcePage &&
				ordinal == other.ordinal &&
				sectionKind == other.sectionKind &&
				sectionName == other.sectionName;
		}
	};

	struct ScriptExecutionCoverageKeyHash
	{
		size_t operator()(const ScriptExecutionCoverageKey& key) const
		{
			size_t hash = std::hash<int>()(key.resourceType);
			auto combine = [&hash](size_t value)
			{
				hash ^= value + static_cast<size_t>(0x9e3779b9) + (hash << 6) + (hash >> 2);
			};
			combine(std::hash<int>()(key.resourceIndex));
			combine(std::hash<int>()(key.resourcePage));
			combine(std::hash<DWORD>()(key.ordinal));
			combine(std::hash<std::string>()(key.sectionKind));
			combine(std::hash<std::string>()(key.sectionName));
			return hash;
		}
	};

	struct ScriptExecutionCoverageEntry
	{
		int resourceType;
		int resourceIndex;
		int resourcePage;
		DWORD ordinal;
		std::string resourceTypeName;
		std::string resourceName;
		std::string sectionKind;
		std::string sectionName;
		std::string sourceFile;
		std::atomic<uint64_t> hits;

		ScriptExecutionCoverageEntry()
			: resourceType(0), resourceIndex(0), resourcePage(0), ordinal(0), hits(0)
		{
		}
	};

	struct ScriptExecutionCoverageReporter
	{
		std::mutex mutex;
		std::string path;
		std::unique_ptr<ScriptExecutionCoverageEntry> ownedEntries[SCRIPT_EXECUTION_COVERAGE_LIMIT];
		std::atomic<ScriptExecutionCoverageEntry*> hitEntries[SCRIPT_EXECUTION_COVERAGE_LIMIT];
		std::unordered_map<ScriptExecutionCoverageKey, DWORD, ScriptExecutionCoverageKeyHash> indices;
		size_t entryCount;
		std::atomic<uint64_t> overflowSections;
		std::atomic<uint64_t> overflowHits;

		ScriptExecutionCoverageReporter()
			: entryCount(0), overflowSections(0), overflowHits(0)
		{
			for (size_t i = 0; i < SCRIPT_EXECUTION_COVERAGE_LIMIT; ++i)
				hitEntries[i].store(NULL, std::memory_order_relaxed);
		}
	};

	UnknownKeywordReporter g_UnknownKeywordReporter;
	std::atomic<bool> g_UnknownKeywordReportEnabled(false);
	ScriptExecutionCoverageReporter g_ScriptExecutionCoverageReporter;
	std::atomic<bool> g_ScriptExecutionCoverageEnabled(false);

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

	LPCTSTR ScriptExecutionCoverageResourceTypeName(int iResourceType)
	{
		switch (static_cast<RES_TYPE>(iResourceType))
		{
		case RES_CharDef: return "CHARDEF";
		case RES_Dialog: return "DIALOG";
		case RES_Events: return "EVENTS";
		case RES_Function: return "FUNCTION";
		case RES_ItemDef: return "ITEMDEF";
		case RES_Menu: return "MENU";
		case RES_SkillMenu: return "SKILLMENU";
		case RES_TypeDef: return "TYPEDEF";
		default: return "UNKNOWN";
		}
	}

	std::string BoundedCoverageString(LPCTSTR pszValue, size_t maxLength)
	{
		if (!pszValue)
			return std::string();
		size_t length = 0;
		while (length < maxLength && pszValue[length])
			++length;
		return std::string(pszValue, length);
	}

	bool SaturatingIncrement(std::atomic<uint64_t>& counter)
	{
		uint64_t current = counter.load(std::memory_order_relaxed);
		while (current != UINT64_MAX)
		{
			if (counter.compare_exchange_weak(
				current, current + 1, std::memory_order_relaxed, std::memory_order_relaxed))
				return true;
		}
		return false;
	}

	struct ScriptExecutionCoverageSnapshot
	{
		int resourceType;
		int resourceIndex;
		int resourcePage;
		DWORD ordinal;
		std::string resourceTypeName;
		std::string resourceName;
		std::string sectionKind;
		std::string sectionName;
		std::string sourceFile;
		uint64_t hits;
	};

	void WriteScriptExecutionCoverageJson(
		std::ofstream& output,
		const std::vector<ScriptExecutionCoverageSnapshot>& entries,
		uint64_t total,
		uint64_t overflowSections,
		uint64_t overflowHits)
	{
		size_t executed = 0;
		for (size_t i = 0; i < entries.size(); ++i)
		{
			if (entries[i].hits > 0)
				++executed;
		}
		uint64_t loaded = static_cast<uint64_t>(entries.size());
		if (overflowSections > UINT64_MAX - loaded)
			loaded = UINT64_MAX;
		else
			loaded += overflowSections;

		output << "{\n  \"version\": 1"
			<< ",\n  \"loaded\": " << loaded
			<< ",\n  \"distinct\": " << entries.size()
			<< ",\n  \"executed\": " << executed
			<< ",\n  \"total_hits\": " << total
			<< ",\n  \"overflow_sections\": " << overflowSections
			<< ",\n  \"overflow_hits\": " << overflowHits
			<< ",\n  \"entries\": [";
		for (size_t i = 0; i < entries.size(); ++i)
		{
			const ScriptExecutionCoverageSnapshot& entry = entries[i];
			output << (i ? ",\n" : "\n")
				<< "    {\"resource_type\": \"" << JsonEscape(entry.resourceTypeName)
				<< "\", \"resource_index\": " << entry.resourceIndex
				<< ", \"resource_page\": " << entry.resourcePage
				<< ", \"resource_name\": \"" << JsonEscape(entry.resourceName)
				<< "\", \"section_kind\": \"" << JsonEscape(entry.sectionKind)
				<< "\", \"section_name\": \"" << JsonEscape(entry.sectionName)
				<< "\", \"ordinal\": " << entry.ordinal
				<< ", \"count\": " << entry.hits
				<< ", \"source_file\": \"" << JsonEscape(entry.sourceFile)
				<< "\"}";
		}
		if (!entries.empty())
			output << '\n';
		output << "  ]\n}\n";
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

void ScriptExecutionCoverageSetPath(LPCTSTR pszPath)
{
	ScriptExecutionCoverageReporter& reporter = g_ScriptExecutionCoverageReporter;
	std::lock_guard<std::mutex> lock(reporter.mutex);
	reporter.path = (pszPath && *pszPath) ? pszPath : "";
	g_ScriptExecutionCoverageEnabled.store(!reporter.path.empty(), std::memory_order_release);
}

bool ScriptExecutionCoverageIsEnabled()
{
	return g_ScriptExecutionCoverageEnabled.load(std::memory_order_acquire);
}

SCRIPT_EXECUTION_COVERAGE_TOKEN ScriptExecutionCoverageRegister(
	int iResourceType,
	int iResourceIndex,
	int iResourcePage,
	LPCTSTR pszResourceName,
	LPCTSTR pszSectionKind,
	LPCTSTR pszSectionName,
	DWORD dwOrdinal,
	LPCTSTR pszSourceFile)
{
	if (!ScriptExecutionCoverageIsEnabled())
		return SCRIPT_EXECUTION_COVERAGE_INVALID_TOKEN;

	ScriptExecutionCoverageKey key;
	key.resourceType = iResourceType;
	key.resourceIndex = iResourceIndex;
	key.resourcePage = iResourcePage;
	key.ordinal = dwOrdinal;
	key.sectionKind = BoundedCoverageString(pszSectionKind, SCRIPT_EXECUTION_COVERAGE_MAX_SECTION_NAME);
	key.sectionName = BoundedCoverageString(pszSectionName, SCRIPT_EXECUTION_COVERAGE_MAX_SECTION_NAME);
	std::string resourceName = BoundedCoverageString(pszResourceName, SCRIPT_EXECUTION_COVERAGE_MAX_RESOURCE_NAME);
	std::string sourceFile = BoundedCoverageString(pszSourceFile, SCRIPT_EXECUTION_COVERAGE_MAX_SOURCE_FILE);

	ScriptExecutionCoverageReporter& reporter = g_ScriptExecutionCoverageReporter;
	std::lock_guard<std::mutex> lock(reporter.mutex);
	if (reporter.path.empty())
		return SCRIPT_EXECUTION_COVERAGE_INVALID_TOKEN;

	std::unordered_map<ScriptExecutionCoverageKey, DWORD, ScriptExecutionCoverageKeyHash>::iterator existing =
		reporter.indices.find(key);
	if (existing != reporter.indices.end())
	{
		ScriptExecutionCoverageEntry* entry = reporter.ownedEntries[existing->second].get();
		if (entry)
		{
			entry->resourceName = resourceName;
			entry->resourceTypeName = ScriptExecutionCoverageResourceTypeName(iResourceType);
			entry->sourceFile = sourceFile;
		}
		return existing->second;
	}

	if (reporter.entryCount >= SCRIPT_EXECUTION_COVERAGE_LIMIT)
	{
		SaturatingIncrement(reporter.overflowSections);
		return SCRIPT_EXECUTION_COVERAGE_OVERFLOW_TOKEN;
	}

	DWORD token = static_cast<DWORD>(reporter.entryCount);
	std::unique_ptr<ScriptExecutionCoverageEntry> entry(new ScriptExecutionCoverageEntry);
	entry->resourceType = iResourceType;
	entry->resourceIndex = iResourceIndex;
	entry->resourcePage = iResourcePage;
	entry->ordinal = dwOrdinal;
	entry->resourceTypeName = ScriptExecutionCoverageResourceTypeName(iResourceType);
	entry->resourceName = resourceName;
	entry->sectionKind = key.sectionKind;
	entry->sectionName = key.sectionName;
	entry->sourceFile = sourceFile;
	reporter.indices.insert(std::make_pair(key, token));
	ScriptExecutionCoverageEntry* entryPointer = entry.get();
	reporter.ownedEntries[reporter.entryCount] = std::move(entry);
	reporter.hitEntries[reporter.entryCount].store(entryPointer, std::memory_order_release);
	++reporter.entryCount;
	return token;
}

void ScriptExecutionCoverageHit(SCRIPT_EXECUTION_COVERAGE_TOKEN token)
{
	if (token == SCRIPT_EXECUTION_COVERAGE_INVALID_TOKEN || !ScriptExecutionCoverageIsEnabled())
		return;

	ScriptExecutionCoverageReporter& reporter = g_ScriptExecutionCoverageReporter;
	if (token == SCRIPT_EXECUTION_COVERAGE_OVERFLOW_TOKEN)
	{
		SaturatingIncrement(reporter.overflowHits);
		return;
	}
	if (token >= SCRIPT_EXECUTION_COVERAGE_LIMIT)
		return;

	ScriptExecutionCoverageEntry* entry = reporter.hitEntries[token].load(std::memory_order_acquire);
	if (!entry)
		return;
	bool entryIncremented = SaturatingIncrement(entry->hits);
	if (!entryIncremented)
		SaturatingIncrement(reporter.overflowHits);
}

bool ScriptExecutionCoverageWrite()
{
	ScriptExecutionCoverageReporter& reporter = g_ScriptExecutionCoverageReporter;
	std::string path;
	std::vector<ScriptExecutionCoverageSnapshot> entries;
	uint64_t total = 0;
	uint64_t overflowSections = 0;
	uint64_t overflowHits = 0;
	{
		std::lock_guard<std::mutex> lock(reporter.mutex);
		path = reporter.path;
		if (path.empty())
			return false;
		entries.reserve(reporter.entryCount);
		for (size_t i = 0; i < reporter.entryCount; ++i)
		{
			const ScriptExecutionCoverageEntry* entry = reporter.ownedEntries[i].get();
			if (!entry)
				continue;
			ScriptExecutionCoverageSnapshot snapshot;
			snapshot.resourceType = entry->resourceType;
			snapshot.resourceIndex = entry->resourceIndex;
			snapshot.resourcePage = entry->resourcePage;
			snapshot.ordinal = entry->ordinal;
			snapshot.resourceTypeName = entry->resourceTypeName;
			snapshot.resourceName = entry->resourceName;
			snapshot.sectionKind = entry->sectionKind;
			snapshot.sectionName = entry->sectionName;
			snapshot.sourceFile = entry->sourceFile;
			snapshot.hits = entry->hits.load(std::memory_order_relaxed);
			entries.push_back(snapshot);
		}
		overflowSections = reporter.overflowSections.load(std::memory_order_relaxed);
		overflowHits = reporter.overflowHits.load(std::memory_order_relaxed);
	}
	for (size_t i = 0; i < entries.size(); ++i)
	{
		if (entries[i].hits > UINT64_MAX - total)
			total = UINT64_MAX;
		else
			total += entries[i].hits;
	}
	if (overflowHits > UINT64_MAX - total)
		total = UINT64_MAX;
	else
		total += overflowHits;

	std::sort(entries.begin(), entries.end(), [](
		const ScriptExecutionCoverageSnapshot& left,
		const ScriptExecutionCoverageSnapshot& right)
	{
		if (left.sourceFile != right.sourceFile)
			return left.sourceFile < right.sourceFile;
		if (left.resourceTypeName != right.resourceTypeName)
			return left.resourceTypeName < right.resourceTypeName;
		if (left.resourceIndex != right.resourceIndex)
			return left.resourceIndex < right.resourceIndex;
		if (left.resourcePage != right.resourcePage)
			return left.resourcePage < right.resourcePage;
		if (left.sectionKind != right.sectionKind)
			return left.sectionKind < right.sectionKind;
		if (left.sectionName != right.sectionName)
			return left.sectionName < right.sectionName;
		return left.ordinal < right.ordinal;
	});

	std::ofstream output(path.c_str(), std::ios::out | std::ios::trunc);
	if (!output.is_open())
		return false;
	WriteScriptExecutionCoverageJson(output, entries, total, overflowSections, overflowHits);
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
	// LASTNEW is exposed by CWorld rather than the generated function table.
	// Route all three aliases through the world property getter so a freshly
	// created object can be used as a reference-chain root.
	if ( !_stricmp(pszKey, "LASTNEW") || !_stricmp(pszKey, "LASTNEWITEM") ||
		!_stricmp(pszKey, "LASTNEWCHAR") )
	{
		HRESULT hRes = g_World.s_PropGet(pszKey, vValRet, GetSrc());
		if ( hRes == NO_ERROR )
			return hRes;
	}

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
			CResourceLink* pFunctionLink = sFunction.GetLinkResource();
			if (pFunctionLink)
				ScriptExecutionCoverageHit(pFunctionLink->GetScriptCoverageToken());
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
	case F_IsUIDValid:
		if ( vArgs.IsEmpty())
			return( HRES_BAD_ARG_QTY );
		vValRet.SetBool( g_World.ObjFind( vArgs.GetUID()) != NULL );
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

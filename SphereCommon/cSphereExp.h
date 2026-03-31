//
// CSphereExp.h
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)

#ifndef _INC_CSPHEREEXP_H
#define _INC_CSPHEREEXP_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

class CObjBase;

enum RES_TYPE	// all the script resource blocks we know how to deal with !
{
	RES_UNKNOWN = 0,		// Not to be used.
#define CRESOURCETAG(a,b,c) RES_##a,
#include "cresourcetag.tbl"
#undef CRESOURCETAG
	RES_QTY,			// Don't care (must be < RID_TYPE_MASK)
};

#define RES_DIALOG_TEXT				1	// sub page for the section.
#define RES_DIALOG_BUTTON			2
#define RES_NEWBIE_MALE_DEFAULT		(10000+1)	// just an unused number for the range.
#define RES_NEWBIE_FEMALE_DEFAULT	(10000+2)

struct CSphereUIDBase
{
	// UN-Initialized Resource ID. type of UID_INDEX
	// It may or may not be valid.
	// This may be an object in the world or a resource.
	// If this is a ref to a game object the top 2 bits are just flags.
#define UID_INDEX_CLEAR	0x00000000
#define RID_F_RESOURCE	0x80000000	// ALSO: pileable or special macro flag passed to client.
#define RID_F_MAP		0xC0000000	// ??? Both bits on means that it's map coords.
#define UID_F_ITEM		0x40000000	// CItem as apposed to CChar based

#define UID_INDEX_MASK	0x3FFFFFFF	// lose the upper 2 bits.
#define UID_INDEX_FREE	0x01000000	// Spellbook needs unused UID's ?

#define RID_TYPE_SHIFT	25	// 6 bits = 64 for RES_TYPE
#define RID_TYPE_MASK	63
#define RID_PAGE_SHIFT	18	// 7 bits = 128 pages of space. (if needed by the type)
#define RID_PAGE_MASK	127
#define RID_INDEX_SHIFT	0	// 18 bits = 262144 entries.
#define RID_INDEX_MASK	0x3FFFF		// max number of any given type. (not valid value)

public:

	// Generic uid.

	void InitUID( void )
	{
		m_dwUIDVal = UID_INDEX_CLEAR;
	}
	bool IsValidUID() const
	{
		return( m_dwUIDVal && ( m_dwUIDVal & UID_INDEX_MASK ) != UID_INDEX_MASK );
	}

	// World object

	bool IsValidObjUID() const
	{
		return( ! ( m_dwUIDVal & RID_F_RESOURCE ) && IsValidUID());
	}

	bool IsItem( void ) const	// Item vs. Char
	{
		if (( m_dwUIDVal & (UID_F_ITEM|RID_F_RESOURCE)) != 0 )
			return( IsValidObjUID());	// might be static in client ?
		return( false );
	}
	bool IsChar( void ) const	// Item vs. Char
	{
		if (( m_dwUIDVal & (UID_F_ITEM|RID_F_RESOURCE)) == 0 )
			return( IsValidObjUID());
		return( false );
	}

	void SetObjUID( UID_INDEX dwVal )
	{
		// can be set to -1 by the client.
		m_dwUIDVal = ( dwVal & (UID_F_ITEM|UID_INDEX_MASK));
	}

	// Resource

	bool IsValidRID( void ) const
	{
		// Is this a valid resource UID ?
		return(( m_dwUIDVal & RID_F_RESOURCE ) && 
			( m_dwUIDVal & RID_INDEX_MASK ) != RID_INDEX_MASK );
	}

#define RES_GET_TYPE(dw)	(((dw)>>RID_TYPE_SHIFT)&RID_TYPE_MASK)
	RES_TYPE GetResType() const
	{
		// ASSERT(IsValidRID());
		return( (RES_TYPE) RES_GET_TYPE(m_dwUIDVal));
	}
#define RES_GET_INDEX(dw)	((dw)&RID_INDEX_MASK)
	int GetResIndex() const
	{
		//ASSERT(IsValidRID());
		return( RES_GET_INDEX(m_dwUIDVal));
	}
#define RES_GET_PAGE(dw)	(((dw)>>RID_PAGE_SHIFT)&RID_PAGE_MASK)
	int GetResPage() const
	{
		//ASSERT(IsValidRID());
		return( RES_GET_PAGE(m_dwUIDVal));
	}

	HASH_INDEX GetHashCode() const
	{
		return m_dwUIDVal;
	}

	// operator overloads

	bool operator == ( UID_INDEX index ) const
	{
		return( m_dwUIDVal == index );
	}
	bool operator != ( UID_INDEX index ) const
	{
		return( m_dwUIDVal != index );
	}
	operator UID_INDEX () const
	{
		return( m_dwUIDVal );
	}
	CSphereUIDBase& operator = ( UID_INDEX uid )
	{
		m_dwUIDVal = uid;
		return( *this );
	}

protected:
	UID_INDEX m_dwUIDVal;
};

struct CSphereUID : public CSphereUIDBase
{
	// A resource or UID for an object.
	// Initialized Resource ID

	CSphereUID( UID_INDEX uidIndex )
	{
		m_dwUIDVal = uidIndex;
	}
	CSphereUID( CSphereUIDBase rid )
	{
		//ASSERT( rid.IsValidRID());
		//ASSERT( rid.IsResource());
		m_dwUIDVal = rid;
	}
	CSphereUID( RES_TYPE restype )
	{
		// single instance type.
		m_dwUIDVal = RID_F_RESOURCE|((restype)<<RID_TYPE_SHIFT);
	}
	CSphereUID( RES_TYPE restype, int index )
	{
		ASSERT( index < RID_INDEX_MASK );
		m_dwUIDVal = RID_F_RESOURCE|((restype)<<RID_TYPE_SHIFT)|(index);
	}
	CSphereUID( RES_TYPE restype, int index, int iPage )
	{
		ASSERT( index < RID_INDEX_MASK );
		ASSERT( iPage < RID_PAGE_MASK );
		m_dwUIDVal = RID_F_RESOURCE|((restype)<<RID_TYPE_SHIFT)|((iPage)<<RID_PAGE_SHIFT)|(index);
	}
	CSphereUID()
	{
		InitUID();
	}
};

//*************************************************************************

class CSphereScriptContext
{
	// Track a temporary context into a script for this CSphereThread.
	// NOTE: This should ONLY be stack based !
public:
	CSphereScriptContext()
	{
		Init();
	}
	CSphereScriptContext( const CScript* pScriptContext )
	{
		Init();
		OpenScript( pScriptContext );
	}
	~CSphereScriptContext()
	{
		CloseScript();
	}

	void OpenScript( const CScript* pScriptContext );
	void CloseScript();
	static const CScript* SetScriptContext( const CScript* pScriptContext );

private:
	void Init()
	{
		m_pScriptContext = NULL;
	}

private:
	const CScript* m_pScriptContext;	// the current script or resource we are using.
	const CScript* m_pPrvScriptContext;	// previous general context before this was opened.
	// m_pPrvScriptContext NULL context may be legit.
};

class CSphereExpContext : public CScriptExecContext
{
	// The base and default context. has no local arguments.
public:
	CSphereExpContext( CResourceObj* pBaseObj, CScriptConsole* pSrc );
	~CSphereExpContext();

	CSCRIPT_EXEC_DEF();
	enum F_TYPE_
	{
#define GLOBALMETHOD(a,b,c) F_##a,
#include "globalmethods.tbl"
#undef GLOBALMETHOD
		F_QTY,
	};

	virtual HRESULT Function_Dispatch(LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet);

public:
	static void InitFunctions();
	static CScriptPropArray sm_FunctionsAll;
	static const CScriptPropX sm_Functions[CSphereExpContext::F_QTY + 1];

private:
	CScriptExecContext* m_pPrvExecContext;	// previous general context before this was opened. (may be NULL)
	int m_iPrvTask;	// task profiling.
};

class CSphereExpArgs : public CSphereExpContext
{
	// Execution context for a script
	// a temporary context of an object for this CSphereThread.
	// With all the args an event will ever need.
	// NOTE: This is meant to be fast and simple.
	//   Most triggers are never executed. So make setup/destroy FAST!
	// NOTE: This should ONLY be stack based !

public:
	static void InitFunctions();

public:
	CSphereExpArgs( CResourceObj* pBaseObj, CScriptConsole* pSrc ) :
		CSphereExpContext(pBaseObj,pSrc)
	{
	}
	CSphereExpArgs( CResourceObj* pBaseObj, CScriptConsole* pSrc, CResourceObj* pObj ) :
		CSphereExpContext(pBaseObj,pSrc),
		m_pO1(pObj)
	{
	}
	CSphereExpArgs( CResourceObj* pBaseObj, CScriptConsole* pSrc, int iVal1 ) :
		CSphereExpContext(pBaseObj,pSrc),
		m_iN1(iVal1)
	{
	}
	CSphereExpArgs( CResourceObj* pBaseObj, CScriptConsole* pSrc, int iVal1, int iVal2, int iVal3 = 0 ) :
		CSphereExpContext(pBaseObj,pSrc),
		m_iN1(iVal1), m_iN2(iVal2), m_iN3(iVal3)
	{
	}
	CSphereExpArgs( CResourceObj* pBaseObj, CScriptConsole* pSrc, int iVal1, int iVal2, CResourceObj* pObj ) :
		CSphereExpContext(pBaseObj,pSrc),
		m_iN1(iVal1), m_iN2(iVal2), m_pO1(pObj)
	{
	}

	// argv=1
	CSphereExpArgs( CResourceObj* pBaseObj, CScriptConsole* pSrc, CGVariant& vVal ) :
		CSphereExpContext(pBaseObj,pSrc), m_vVal(vVal)
	{
	}

	// args=1
	CSphereExpArgs( CResourceObj* pBaseObj, CScriptConsole* pSrc, LPCTSTR pszStr );
	~CSphereExpArgs();

	void AddText( int id, const char* pszText );
	void AddCheck( int id, DWORD dwCheckVal );

	virtual HRESULT Function_Dispatch(LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet);

	CSCRIPT_EXEC_DEF();
	enum F_TYPE_
	{
#define CSPHEREEXPFUNC(a,b,c) F_##a,
#include "csphereexpfunc.tbl"
#undef CSPHEREEXPFUNC
		F_QTY,
	};

	static CScriptPropArray sm_FunctionsAll;
	static const CScriptPropX sm_Functions[F_QTY + 1];

public:
	// "ARGN" or "ARGN1" = a modifying numeric arg to the current trigger.
	int m_iN1;
	// "ARGN2" = a modifying numeric arg to the current trigger.
	int m_iN2;
	// "ARGN3" = a modifying numeric arg to the current trigger.
	int m_iN3;

	// "ARGO" or "ARGO1" = object 1
	CResourceObjPtr m_pO1;

	// "ARGS" or "ARGS1" = string 1
	CGString m_s1;

	// "ARGV" or ARG#
	CGVariant m_vVal;
};

//*****************************************************************

class CSphereThread 
#ifdef _MT
	: public CThread
#endif
{
	// Context stuff for each thread we run.
public:
	CSphereThread()
	{
		m_pScriptContext = NULL;
	}
	const CScript* SetScriptContext( const CScript* pScriptContext )
	{
		// CSphereScriptContext
		const CScript* pPrvScriptContext = m_pScriptContext;
		m_pScriptContext = pScriptContext;
		return( pPrvScriptContext );
	}
	CScriptExecContext* SetExecContext( CScriptExecContext* pExecContext )
	{
		// CSphereExpContext
		// This is now the threads current context.
		CScriptExecContext* pPrvExecContext = m_pExecContext;
		m_pExecContext = pExecContext;
		return( pPrvExecContext );
	}

	static CSphereThread* GetCurrentThread();

public:
	const CScript* m_pScriptContext;	// The current script open for use.
	CScriptExecContext* m_pExecContext;	// general context we are scripting in.

	CSphereUID m_uidLastNewItem;	// for script access. (put in context not thread?!)
	CSphereUID m_uidLastNewChar;	// for script access.
};

#if 0

struct CSphereDefVars : public CResourceObj, public CVarDefArray
{
	// Globally saved and set variables.
public:
	// Basic object Loading/Query services.
	CSphereDefVars();
	~CSphereDefVars();
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script

public:

#ifdef USE_JSCRIPT
#define GLOBALMETHOD(a,b,c)  JSCRIPT_METHOD_DEF(a)
#include "globalmethods.tbl"
#undef GLOBALMETHOD
#endif

};

#endif

CExpression* Exp_GetContext();

#endif // _INC_CSPHEREEXP_H


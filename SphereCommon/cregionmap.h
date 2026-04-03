//
// CRegionBasic.H
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#ifndef _INC_CREGIONMAP_H
#define _INC_CREGIONMAP_H

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "cpointmap.h"
#include "cobjbasetemplate.h"

enum RegionClass_Type
{
	// General classification for searching purposes.
	// Ask NPC for type of area ?
	RegionClass_Unspecified = 0,	// Unspecified.
	RegionClass_Jail,
	// ...
	RegionClass_Qty,
};

class CRegionBasic : public CResourceDef, public CGRegion
{
	// region of the world of arbitrary size and location.
	// made of (possibly multiple) rectangles.
	// RES_Room or base for RES_Area
	// This has a m_mapplane and m_z range element as well.

#define REGION_ANTIMAGIC_ALL		0x000001	// All magic banned here.
#define REGION_ANTIMAGIC_RECALL_IN	0x000002	// Teleport,recall in to this, and mark
#define REGION_ANTIMAGIC_RECALL_OUT	0x000004	// can't recall out of here.
#define REGION_ANTIMAGIC_GATE		0x000008
#define REGION_ANTIMAGIC_TELEPORT	0x000010	// Can't teleport into here.
#define REGION_ANTIMAGIC_DAMAGE		0x000020	// just no bad magic here

#define REGION_FLAG_SHIP			0x000040	// This is a ship region. ship commands
#define REGION_FLAG_NOBUILDING		0x000080	// No building in this area

#define REGION_FLAG_ANNOUNCE		0x000200	// Announce to all who enter.
#define REGION_FLAG_INSTA_LOGOUT	0x000400	// Instant Log out is allowed here. (hotel)
#define REGION_FLAG_UNDERGROUND		0x000800	// dungeon type area. (no weather)
#define REGION_FLAG_NODECAY			0x001000	// Things on the ground don't decay here.

#define REGION_FLAG_SAFE			0x002000	// This region is safe from all harm.
#define REGION_FLAG_GUARDED			0x004000	// try TAG.GUARDOWNER
#define REGION_FLAG_NO_PVP			0x008000	// Players cannot directly harm each other here.
#define REGION_FLAG_ARENA			0x010000	// Anything goes. no murder counts or crimes.

public:
	CRegionBasic( CSphereUID rid, LPCTSTR pszName = NULL );
	virtual ~CRegionBasic();

	virtual bool RealizeRegion();
	void UnRealizeRegion( bool fRetestChars );
	virtual void UnLink()
	{
		UnRealizeRegion(true);
	}

	void SetName( LPCTSTR pszName );
	virtual CGString GetName() const
	{
		return( m_sName );
	}

	bool IsMultiRegion() const
	{
		return(( GetUIDIndex() & (RID_F_RESOURCE|UID_F_ITEM)) == UID_F_ITEM );
	}
	bool IsMatchType( DWORD dwTypeMask ) const; // REGION_TYPE_AREA

	void s_WriteBase( CScript& s );

	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script
	virtual void s_WriteBody( CScript& s, LPCTSTR pszPrefix );
	virtual void s_WriteProps( CScript& s );

	CSectorPtr GetSector( int& i ) const;	// get all the sectors that make up this rect.

	bool AddRegionRect( const CRectMap& rect );
	bool SetRegionRect( const CRectMap& rect )
	{
		EmptyRegion();
		return AddRegionRect( rect );
	}

	bool IsPointInside( const CPointMap& pt ) const;	// Check m_mapplane and m_z also.

	DWORD GetRegionFlags() const
	{
		// REGION_ANTIMAGIC_ALL
		return( m_dwFlags );
	}
	bool IsFlag( DWORD dwFlags ) const
	{
		// REGION_FLAG_GUARDED
		return(( m_dwFlags & dwFlags ) ? true : false );
	}
	bool IsFlagGuarded() const
	{
		// Safe areas do not need guards.
		return( IsFlag( REGION_FLAG_GUARDED ) && ! IsFlag( REGION_FLAG_SAFE ));
	}
	void SetRegionFlags( DWORD dwFlags )
	{
		m_dwFlags |= dwFlags;
	}
	void TogRegionFlags( DWORD dwFlags, bool fSet )
	{
		if ( fSet )
			m_dwFlags |= dwFlags;
		else
			m_dwFlags &= ~dwFlags;
	}

	bool CheckAntiMagic( SPELL_TYPE spell ) const;
	virtual bool IsValid() const
	{
		ASSERT( IsValidHeap());
		return( m_sName.IsValid());
	}

private:
	void SendSectorsCommand( LPCTSTR pszCommand, CScriptConsole* pSrc ); // distribute to the CSectors

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CREGIONPROP(a,b,c) P_##a,
#include "cregionprops.tbl"
#undef CREGIONPROP
		P_QTY,
	};
	enum M_TYPE_
	{
#define CREGIONMETHOD(a,b,c) M_##a,
#include "cregionmethods.tbl"
#undef CREGIONMETHOD
		M_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];
	static const CScriptMethod sm_Methods[M_QTY+1];
#ifdef USE_JSCRIPT
#define CREGIONMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "cregionmethods.tbl"
#undef CREGIONMETHOD
#endif

	// NOTE: This also specifies the m_mapplane
	CPointMap m_pt;			// safe point in the region. (for teleporting to)
	RegionClass_Type m_RegionClass;	// general class of the region.
	// int m_iLinkedSectors; // just for statistics tracking. How many sectors are linked ?

protected:
	DECLARE_MEM_DYNAMIC;

private:
	CGString m_sName;	// Name of the region.
	DWORD m_dwFlags;
};

// See CRegionPtr;

class CRegionResourceDef : public CResourceDef
{
	// RES_RegionResource
	// When mining/lumberjacking etc. What can we get?
protected:
	DECLARE_MEM_DYNAMIC;
public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CREGIONRESOURCEPROP(a,b,c) P_##a,
#include "cregionresourceprops.tbl"
#undef CREGIONRESOURCEPROP
		P_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];

	// What item do we get when we try to mine this.
	ITEMID_TYPE m_ReapItem;	// ITEMID_ORE_1 most likely
	CValueRangeInt m_ReapAmount;	// How much can we reap at one time (based on skill)

	CValueRangeInt m_Amount;		// How much is here total
	CValueRangeInt m_Skill;			// Skill levels required to mine this.
	int m_iRegenerateTime;			// TICKS_PER_SEC once found how long to regen this type.

public:
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );

	CRegionResourceDef( CSphereUID rid );
	virtual ~CRegionResourceDef();
};

class CRegionType : public CResourceTriggered
{
	// RES_Spawn or RES_RegionType
	// Attributes attached to a region.
	// spawn groups, minable resources and event handlers.
protected:
	DECLARE_MEM_DYNAMIC;
public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CREGIONTYPEPROP(a,b,c) P_##a,
#include "cregiontypeprops.tbl"
#undef CREGIONTYPEPROP
		P_QTY,
	};
	enum T_TYPE_
	{
		// XTRIG_UNKNOWN	= some named trigger not on this list.
#define CREGIONEVENT(a,b,c) T_##a,
		CREGIONEVENT(AAAUNUSED,CSCRIPTPROP_NOARGS|CSCRIPTPROP_RETNULL,NULL)
#include "cregionevents.tbl"
#undef CREGIONEVENT
		T_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];
	static const CScriptProp sm_Triggers[T_QTY+1];

private:
	CResourceQtyArray m_Members;	// often refs to CRegionResourceDef
	int m_iTotalWeight;	// add up all the qty's in m_Members
private:
	int CalcTotalWeight();
public:

	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );

	int GetTotalWeight() const
	{
		return m_iTotalWeight;
	}
	int GetRandMemberIndex() const;
	CResourceQty GetMember( int i ) const
	{
		return( m_Members[i] );
	}
	CSphereUID GetMemberID( int i ) const
	{
		return( m_Members[i].GetResourceID() );
	}
	int GetMemberWeight( int i ) const
	{
		return( m_Members[i].GetResQty() );
	}

	CRegionType( CSphereUID rid ) : CResourceTriggered( rid )
	{
		m_iTotalWeight = 0;
	}
	virtual ~CRegionType()
	{
	}
};

class CRegionComplex : public CRegionBasic
{
	// A region with extra tags and properties.
	// [AREA] = RES_Area
public:
	CRegionComplex( CSphereUID rid, LPCTSTR pszName = NULL );
	virtual ~CRegionComplex();

	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script
	virtual void s_WriteBody( CScript& s, LPCTSTR pszPrefix );
	virtual void s_WriteProps( CScript& s );

#ifdef SPHERE_SVR
	TRIGRET_TYPE OnRegionTrigger( CScriptConsole* pChar, CRegionType::T_TYPE_ trig );
	const CRegionType* FindNaturalResource( int /* IT_TYPE */ type ) const;
	virtual void UnLink()
	{
		CRegionBasic::UnLink();
		m_Events.RemoveAll();
	}
#endif	// SPHERE_SVR

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CREGIONCOMPLEXPROP(a,b,c) P_##a,
#include "cregioncomplexprops.tbl"
#undef CREGIONCOMPLEXPROP
		P_QTY,
	};
	enum M_TYPE_
	{
#define CREGIONCOMPLEXMETHOD(a,b,c) M_##a,
#include "cregioncomplexmethods.tbl"
#undef CREGIONCOMPLEXMETHOD
		M_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];
	static const CScriptMethod sm_Methods[M_QTY+1];

#ifdef USE_JSCRIPT
#define CREGIONCOMPLEXMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "cregioncomplexmethods.tbl"
#undef CREGIONCOMPLEXMETHOD
#endif

#ifdef SPHERE_SVR
	// Define both events and resource handlers here.
	CResourceRefArray m_Events;	// trigger [REGION x] when entered or exited RES_RegionType
	CVarDefArray m_TagDefs;		// attach extra tags here.
#endif

	// Standard extra tags:
	// "TAG.ANNOUNCEMENT" = auto announce this when entering the area.
	// "TAG.GUARDID"	= body id for the guards. (if guarded)
	// "TAG.GUARDOWNER" = should have the word "the" in it if needs it.

protected:
	DECLARE_MEM_DYNAMIC;
};

typedef CRefPtr<CRegionComplex> CRegionComplexPtr;

enum TELDIR_TYPE
{
	TELDIR_1 = 1,		// just 1 directional.
	TELDIR_2_PRIMARY,
	TELDIR_2_SECONDARY,	// don't bother saving this 1.
};

class CTeleport;
typedef CRefPtr<CTeleport> CTeleportPtr;

class CTeleport : public CResourceDef	// The static world teleporters. SPHEREMAP.SCP
{
	// Put a built in trigger here ? can be CHashArray sorted by CPointMap.
	// RES_TELEPORT

public:
	CTeleport( const CPointMap& ptSrc );
	CTeleport( const TCHAR* pszArgs );
	virtual ~CTeleport();

	void SetSrc( const CPointMap& ptSrc );
	void SetDst( const CPointMap& ptDst );

	CTeleportPtr Get2Dir() const;	// get the other side of a link.
	bool RealizeTeleport();
	void UnRealizeTeleport();

	void s_WriteProps( CScript& s );

	HASH_INDEX GetHashCode() const // for quick searching for this 2d point
	{
		return( m_ptSrc.GetHashCode());
	}

public:
	CPointMap		m_ptSrc;
	CPointMap		m_ptDst;
	TELDIR_TYPE		m_DirType;		// Single dir, Primary 2 Dir, Secondary 2 Dir,

	CSCRIPT_CLASS_DEF1();

protected:
	DECLARE_MEM_DYNAMIC;
};

class CStartLoc : public CResourceDef	// The start locations for creating a new char. SPHERE.INI
{
	// RES_Starts
	// Just label a location on the map.
public:
	CStartLoc( LPCTSTR pszArea );
	virtual CGString GetName() const
	{
		return m_sName;
	}

public:
	CSCRIPT_CLASS_DEF1();

	CGString m_sArea;	// Displayable Area/City Name = Britain or Occlo
	CGString m_sName;	// Displayable Place name = Castle Britannia or Docks
	CPointMap m_pt;
protected:
	DECLARE_MEM_DYNAMIC;
};

#endif // _INC_CREGIONMAP_H

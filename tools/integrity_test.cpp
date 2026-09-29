// Disposable regression test for the bounded world-integrity watchdog.

#include "SphereSvr/stdafx.h"

#include <cstdio>

class CIntegrityItem : public CItem
{
public:
	CIntegrityItem( ITEMID_TYPE id, CItemDef* pItemDef ) : CItem( id, pItemDef ) {}
};

class CIntegrityContainer : public CItemContainer
{
public:
	CIntegrityContainer( ITEMID_TYPE id, CItemDef* pItemDef ) : CItemContainer( id, pItemDef ) {}
	bool AttachRaw( CItemPtr pItem )
	{
		return CContainer::InsertHead( pItem );
	}
};

static bool TestMissingContainerIsReported()
{
	g_Serv.SetServerMode( SERVMODE_Run );
	CItemDef containerDef( ITEMID_MULTI_MAX );
	CItemDef childDef( ITEMID_MULTI_MAX );
	CIntegrityContainer* pContainerRaw = new CIntegrityContainer( ITEMID_MULTI_MAX, &containerDef );
	CItemContainerPtr pContainer = pContainerRaw;
	CItemPtr pChild = new CIntegrityItem( ITEMID_MULTI_MAX, &childDef );
	pContainerRaw->AttachRaw( pChild );
	if ( pChild->GetParent() != pContainer )
	{
		std::fprintf( stderr, "fixture could not establish the contained item\n" );
		return false;
	}

	if ( g_World.CheckIntegrity( 0 ) != 0 )
	{
		std::fprintf( stderr, "a healthy container failed the integrity check\n" );
		return false;
	}
	if ( g_World.CheckIntegrity( 1 ) != 0 || g_World.CheckIntegrity( 1 ) != 0 )
	{
		std::fprintf( stderr, "bounded integrity passes did not cover the healthy fixture\n" );
		return false;
	}

	// Inject the same class of corruption the watchdog must catch: the object
	// remains addressable in the UID table, but its container backlink is gone.
	pChild->Detach();
	if ( pChild->GetParent() != NULL )
	{
		std::fprintf( stderr, "fixture corruption did not detach the child\n" );
		return false;
	}
	const bool fReported = g_World.CheckIntegrity( 0 ) > 0;
	pChild->DeleteThis();
	pContainer->DeleteThis();
	g_World.GarbageCollection_New();
	return fReported;
}

static bool TestContainerCycleIsReported()
{
	CItemDef outerDef( ITEMID_MULTI_MAX );
	CItemDef innerDef( ITEMID_MULTI_MAX );
	CIntegrityContainer* pOuter = new CIntegrityContainer( ITEMID_MULTI_MAX, &outerDef );
	CIntegrityContainer* pInner = new CIntegrityContainer( ITEMID_MULTI_MAX, &innerDef );
	pOuter->AttachRaw( pInner );
	pInner->AttachRaw( pOuter );
	const bool fReported = g_World.CheckIntegrity( 0 ) > 0;
	pOuter->Detach();
	pInner->Detach();
	pOuter->DeleteThis();
	pInner->DeleteThis();
	g_World.GarbageCollection_New();
	return fReported;
}

static bool TestCountDriftSkipsLiveMutation()
{
	CItemDef itemDef( ITEMID_MULTI_MAX );
	CItemPtr pAnchor = new CIntegrityItem( ITEMID_MULTI_MAX, &itemDef );
	CItemPtr pSecond = new CIntegrityItem( ITEMID_MULTI_MAX, &itemDef );
	if ( g_World.CheckIntegrity( 1 ) != 0 )
		return false;

	// This object is created after the cycle snapshot.  Its changed counter
	// must suppress count_drift for the in-flight scan.
	CItemPtr pMutation = new CIntegrityItem( ITEMID_MULTI_MAX, &itemDef );
	bool fReported = false;
	const DWORD dwPasses = g_World.GetUIDCount() - 1;
	for ( DWORD i = 0; i < dwPasses; ++i )
		fReported = ( g_World.CheckIntegrity( 1 ) != 0 ) || fReported;
	pMutation->DeleteThis();
	pAnchor->DeleteThis();
	pSecond->DeleteThis();
	g_World.GarbageCollection_New();
	return !fReported;
}

static bool TestCountDriftIsReported()
{
	CItemDef itemDef( ITEMID_MULTI_MAX );
	CItemPtr pDrift = new CIntegrityItem( ITEMID_MULTI_MAX, &itemDef );
	const DWORD dwIndex = pDrift->GetUIDIndex() & UID_INDEX_MASK;
	CResourceObj* pSaved = g_World.m_UIDs[dwIndex];
	g_World.m_UIDs.SetAt( dwIndex, NULL );
	const bool fReported = g_World.CheckIntegrity( 0 ) > 0;
	g_World.m_UIDs.SetAt( dwIndex, pSaved );
	pDrift->DeleteThis();
	g_World.GarbageCollection_New();
	return fReported;
}

static bool TestLogBudgetIsPerCycle()
{
	CItemDef itemDef( ITEMID_MULTI_MAX );
	CItemPtr pItems[24];
	for ( CItemPtr& pItem : pItems )
	{
		pItem = new CIntegrityItem( ITEMID_MULTI_MAX, &itemDef );
		pItem->Detach();
	}
	bool fReported = false;
	const DWORD dwPasses = g_World.GetUIDCount() - 1;
	for ( DWORD i = 0; i < dwPasses; ++i )
		fReported = ( g_World.CheckIntegrity( 1 ) != 0 ) || fReported;
	const bool fBounded = g_World.GetIntegrityLastLogCount() <= 16;
	for ( CItemPtr& pItem : pItems )
		pItem->DeleteThis();
	g_World.GarbageCollection_New();
	return fReported && fBounded;
}

int main()
{
	if ( !TestMissingContainerIsReported())
		return 1;
	if ( !TestContainerCycleIsReported())
		return 1;
	if ( !TestCountDriftSkipsLiveMutation())
	{
		std::fprintf( stderr, "live mutation incorrectly reported count_drift\n" );
		return 1;
	}
	if ( !TestCountDriftIsReported())
	{
		std::fprintf( stderr, "injected count drift was not reported\n" );
		return 1;
	}
	if ( !TestLogBudgetIsPerCycle())
	{
		std::fprintf( stderr, "integrity log budget was not bounded per cycle\n" );
		return 1;
	}
	std::printf( "integrity watchdog: injected missing container reported\n" );
	std::printf( "integrity watchdog: injected container cycle reported\n" );
	std::printf( "integrity watchdog: count drift and per-cycle log budget verified\n" );
	return 0;
}

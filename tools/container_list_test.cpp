// Offline regression tests for re-entrant object lists and pointer-array growth.

#include "common.h"
#include "crefobj.h"
#include "cstring.h"
#include "CArray.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

class CProbeRecord : public CGObListRec
{
public:
	static int sm_iDestructed;
	int m_iMarker;

	explicit CProbeRecord( int iMarker = 0 ) : m_iMarker( iMarker ) {}
	~CProbeRecord() override { ++sm_iDestructed; }
};

int CProbeRecord::sm_iDestructed = 0;

class CReinsertList : public CGObList
{
public:
	bool m_fReinsert = false;

protected:
	void OnRemoveOb( CGObListRec* pRec ) override
	{
		CGObList::OnRemoveOb( pRec );
		if ( m_fReinsert && pRec != NULL )
			InsertHead( pRec );
	}
};

static void DeleteAllTimeout( int )
{
	// The pre-fix DeleteAll implementation spins forever when this hook keeps
	// putting the same record back at the head.  Keep the failing-first test
	// bounded so that it is safe to run against an old binary.
	std::_Exit( 124 );
}

static bool Expect( bool fCondition, const char* pszDescription )
{
	if ( fCondition )
		return true;
	std::fprintf( stderr, "FAIL: %s\n", pszDescription );
	return false;
}

static bool TestInsertAfterFailure()
{
	CReinsertList source;
	CReinsertList destination;
	CProbeRecord* pRecord = new CProbeRecord();
	source.m_fReinsert = true;
	if ( !Expect( source.InsertHead( pRecord ), "source setup insertion" ))
		return false;

	const bool fInserted = destination.InsertHead( pRecord );
	const bool fRetained = pRecord->GetParent() == &source &&
		source.GetCount() == 1 && destination.GetCount() == 0;
	if ( !Expect( !fInserted && fRetained,
		"InsertAfter reports a retained record without corrupting either list" ))
		return false;

	source.m_fReinsert = false;
	source.DeleteAll();
	return Expect( CProbeRecord::sm_iDestructed == 1,
		"retained record is still reclaimable after a failed insertion" );
}

static bool TestDeleteAllBounded()
{
	CReinsertList list;
	list.m_fReinsert = true;
	CProbeRecord* pRecord = new CProbeRecord();
	if ( !Expect( list.InsertHead( pRecord ), "DeleteAll setup insertion" ))
		return false;

	std::signal( SIGALRM, DeleteAllTimeout );
	alarm( 2 );
	list.DeleteAll();
	alarm( 0 );

	return Expect( list.GetCount() == 0 && list.GetHead() == NULL &&
		CProbeRecord::sm_iDestructed == 2,
		"DeleteAll detaches and destroys a record reinserted by its hook" );
}

static bool TestReservedPointerArray()
{
	CGRefArray<CProbeRecord> records;
	CProbeRecord probes[66];
	records.Reserve( 66 );
	const size_t iReserved = records.GetRealCount();
	for ( size_t i = 0; i < 66; ++i )
		records.Add( &probes[i] );
	return Expect( records.GetCount() == 66 && records.GetRealCount() == iReserved,
		"reserved pointer array does not grow while appending" );
}

static bool TestReserveReinitializesSlots()
{
	CProbeRecord first( 1 );
	CProbeRecord second( 2 );
	CProbeRecord replacement( 3 );
	CGRefArray<CProbeRecord> records;
	records.Reserve( 4 );
	records.Add( &first );
	records.Add( &second );
	records.SetCount( 1 );
	records.SetCount( 2 );
	if ( !Expect( records.GetAt( 1 ) == NULL,
		"growing a reserved array clears a previously removed slot" ))
		return false;
	records.SetAt( 1, &replacement );
	records.SetCount( 1 );
	records.SetCount( 2 );
	return Expect( records.GetAt( 1 ) == NULL,
		"reusing a reserved slot after another shrink clears it" );
}

static bool TestOwnedArrayRemoveAt()
{
	const int iDestructedBefore = CProbeRecord::sm_iDestructed;
	bool fPassed = false;
	CProbeRecord* pThird = NULL;
	{
		CGObArray<CProbeRecord> records;
		records.Add( new CProbeRecord( 1 ));
		records.Add( new CProbeRecord( 2 ));
		pThird = new CProbeRecord( 3 );
		records.Add( pThird );

		records.DeleteAt( 0 );
		const bool fRemovedOnlyFirst =
			CProbeRecord::sm_iDestructed == iDestructedBefore + 1;
		const bool fContentsMoved = records.GetCount() == 2 &&
			records.GetAt( 0 ) != NULL && records.GetAt( 0 )->m_iMarker == 2 &&
			records.GetAt( 1 ) == pThird;
		// This member read is intentional: the old tail-destructor path leaves a
		// dangling third pointer in the live range, and ASan must report it.
		const bool fTailIsLive = fContentsMoved && pThird->m_iMarker == 3;
		fPassed = Expect( fRemovedOnlyFirst && fContentsMoved && fTailIsLive,
			"DeleteAt removes only the selected owned element" );
		if ( !fPassed && records.GetCount() > 1 && records.GetAt( 1 ) == pThird )
			// Keep the failing-first run from attempting to delete the known stale
			// pointer a second time when ASan is not enabled.
			records.ElementAt( 1 ) = NULL;
	}
	if ( fPassed )
		fPassed = Expect( CProbeRecord::sm_iDestructed == iDestructedBefore + 3,
			"DeleteAt and array teardown destroy each owned element once" );
	return fPassed;
}

int main()
{
	if ( !TestInsertAfterFailure() || !TestDeleteAllBounded() ||
		!TestReservedPointerArray() || !TestReserveReinitializesSlots() ||
		!TestOwnedArrayRemoveAt() )
		return 1;
	std::printf( "container lists: reentrant ownership and array lifetime checks passed\n" );
	return 0;
}

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

int main()
{
	if ( !TestInsertAfterFailure() || !TestDeleteAllBounded() ||
		!TestReservedPointerArray() )
		return 1;
	std::printf( "container lists: retained insertion, bounded DeleteAll, and reserved array checks passed\n" );
	return 0;
}

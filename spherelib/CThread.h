#ifndef _INC_CTHREAD_H
#define _INC_CTHREAD_H

#ifndef _WIN32
#include <pthread.h>
#endif

#ifdef _WIN32
#define THREAD_ENTRY_RET void
#else	// else LINUX
#define THREAD_ENTRY_RET void *
#endif

typedef THREAD_ENTRY_RET(_cdecl* PTHREAD_ENTRY_PROC)(void*);

class CThreadLockableObj
{
};

class CThreadLockPtr
{
public:
	CThreadLockPtr(CThreadLockableObj* pLockThis) { /* no-op in single-threaded mode */ }
	CThreadLockPtr() { /* no-op in single-threaded mode */ }
};

class CThread	// basic multi tasking functionality.
{
public:
	static DWORD GetCurrentThreadId()
	{
#ifdef _WIN32
		return ::GetCurrentThreadId();
#else
		return (DWORD)pthread_self();
#endif
	}

	void CreateThread(PTHREAD_ENTRY_PROC pEntryProc) { /* stub - single threaded */ }
	void CreateThread(PTHREAD_ENTRY_PROC pEntryProc, void* pArgs) { /* stub - single threaded */ }

	void InitInstance() { /* stub */ }
	void ExitInstance() { /* stub */ }

	bool IsActive() const { return false; }
	DWORD GetThreadID() const { return 0; }

	void WaitForClose(int iSec) { /* stub */ }
};

#endif // _INC_CTHREAD_H

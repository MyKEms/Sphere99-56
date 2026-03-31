#ifndef _INC_CTHREAD_H
#define _INC_CTHREAD_H

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
	CThreadLockPtr(CThreadLockableObj* pLockThis) { throw "not implemented"; }
	CThreadLockPtr() { throw "not implemented"; }
};

class CThread	// basic multi tasking functionality.
{
public:
	static DWORD GetCurrentThreadId() { throw "not implemented"; }

	void CreateThread(PTHREAD_ENTRY_PROC pEntryProc) { throw "not implemented"; }
	void CreateThread(PTHREAD_ENTRY_PROC pEntryProc, void* pArgs) { throw "not implemented"; }

	void InitInstance() { throw "not implemented"; }
	void ExitInstance() { throw "not implemented"; }

	bool IsActive() const { throw "not implemented"; }
	DWORD GetThreadID() const { throw "not implemented"; }

	void WaitForClose(int iSec) { throw "not implemented"; }
};

#endif // _INC_CTHREAD_H

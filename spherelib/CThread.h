#ifndef _INC_CTHREAD_H
#define _INC_CTHREAD_H

#ifndef _WIN32
#include <pthread.h>
#include <type_traits>
#endif

#ifdef _WIN32
#define THREAD_ENTRY_RET void
#define THREAD_ENTRY_RETURN return
#else	// else LINUX
#define THREAD_ENTRY_RET void *
#define THREAD_ENTRY_RETURN return NULL	// falling off the end of a void* function is UB
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
private:
#ifndef _WIN32
	template <typename T>
	static uintptr_t PthreadToUint(T thread, std::true_type)
	{
		return reinterpret_cast<uintptr_t>(thread);
	}

	template <typename T>
	static uintptr_t PthreadToUint(T thread, std::false_type)
	{
		return static_cast<uintptr_t>(thread);
	}

	static uintptr_t PthreadToUint(pthread_t thread)
	{
		return PthreadToUint(thread, typename std::is_pointer<pthread_t>::type());
	}

	pthread_t m_thread;
	bool m_fActive;
	uintptr_t m_dwThreadID;
#endif

public:
	CThread()
	{
#ifndef _WIN32
		m_thread = 0;
		m_fActive = false;
		m_dwThreadID = 0;
#endif
	}

	static uintptr_t GetCurrentThreadId()
	{
#ifdef _WIN32
		return static_cast<uintptr_t>(::GetCurrentThreadId());
#else
		return PthreadToUint(pthread_self());
#endif
	}

	void CreateThread(PTHREAD_ENTRY_PROC pEntryProc)
	{
		CreateThread(pEntryProc, NULL);
	}

	void CreateThread(PTHREAD_ENTRY_PROC pEntryProc, void* pArgs)
	{
#ifndef _WIN32
		if (m_fActive)
			return;
		int ret = pthread_create(&m_thread, NULL, (void*(*)(void*))pEntryProc, pArgs);
		if (ret == 0)
		{
			m_fActive = true;
			m_dwThreadID = PthreadToUint(m_thread);
		}
#endif
	}

	void InitInstance()
	{
#ifndef _WIN32
		m_dwThreadID = GetCurrentThreadId();
		m_fActive = true;
#endif
	}

	void ExitInstance()
	{
#ifndef _WIN32
		m_fActive = false;
#endif
	}

	bool IsActive() const
	{
#ifndef _WIN32
		return m_fActive;
#else
		return false;
#endif
	}

	uintptr_t GetThreadID() const
	{
#ifndef _WIN32
		return m_dwThreadID;
#else
		return 0;
#endif
	}

	void TerminateThread(DWORD dwExitCode)
	{
#ifndef _WIN32
		if (m_fActive && m_thread)
		{
			pthread_cancel(m_thread);
			m_fActive = false;
		}
#endif
	}

	void WaitForClose(int iSec)
	{
#ifndef _WIN32
		if (m_fActive && m_thread)
		{
			// Wait for the thread to finish
			struct timespec ts;
			ts.tv_sec = iSec;
			ts.tv_nsec = 0;
			nanosleep(&ts, NULL);
			m_fActive = false;
		}
#endif
	}
};

#endif // _INC_CTHREAD_H

// CSocket.h

#ifndef _INC_CSOCKET_H
#define _INC_CSOCKET_H
#if _MSC_VER >= 1000
#endif // _MSC_VER >= 1000

#include "common.h"

#ifdef _WIN32
#include <winsock.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define WSAEWOULDBLOCK EWOULDBLOCK
#define closesocket close
#define ioctlsocket ioctl
#endif

struct CSocketAddressIP : public in_addr
{
public:
	// Just the ip address. Not the port.
#define SOCKET_LOCAL_ADDRESS 0x0100007f
	// INADDR_ANY              (u_long)0x00000000
	// INADDR_LOOPBACK         0x7f000001
	// INADDR_BROADCAST        (u_long)0xffffffff
	// INADDR_NONE             0xffffffff

	DWORD GetAddrIP() const
	{
		return(s_addr);
	}
	void SetAddrIP(DWORD dwIP)
	{
		s_addr = dwIP;
	}
	LPCTSTR GetAddrStr() const
	{
		return inet_ntoa(*this);
	}
	void SetAddrStr(LPCTSTR pszIP)
	{
		// NOTE: This must be in 1.2.3.4 format.
		s_addr = inet_addr(pszIP);
	}
	bool IsValidAddr() const
	{
		// 0 and 0xffffffff=INADDR_NONE
		return(s_addr != INADDR_ANY && s_addr != INADDR_BROADCAST);
	}
	bool IsLocalAddr() const
	{
		return(s_addr == 0 || s_addr == SOCKET_LOCAL_ADDRESS);
	}

	bool IsSameIP(const CSocketAddressIP& ip) const;
	bool IsMatchIP(const CSocketAddressIP& ip) const;

	struct hostent* GetHostStruct() const
	{
		// try to reverse lookup a name for this IP address.
		// NOTE: This is a blocking call !!!!
		return gethostbyaddr((char*)&s_addr, sizeof(s_addr), AF_INET);
	}
	bool SetHostStruct(const struct hostent* pHost)
	{
		// Set the ip from the address name we looked up.
		if (pHost == NULL ||
			pHost->h_addr_list == NULL ||
			pHost->h_addr == NULL)	// can't resolve the address.
		{
			return(false);
		}
		SetAddrIP(*((DWORD*)(pHost->h_addr))); // 0.1.2.3
		return true;
	}

	bool SetHostStr(LPCTSTR pszHostName)
	{
		// try to resolve the host name with DNS for the true ip address.
		if (pszHostName[0] == '\0')
			return(false);
		if (isdigit(pszHostName[0]))
		{
			SetAddrStr(pszHostName); // 0.1.2.3
			return(true);
		}
		// NOTE: This is a blocking call !!!!
		return SetHostStruct(gethostbyname(pszHostName));
	}
	bool operator==(CSocketAddressIP ip) const
	{
		return(IsSameIP(ip));
	}
	CSocketAddressIP()
	{
		s_addr = INADDR_BROADCAST;
	}
	CSocketAddressIP(DWORD dwIP)
	{
		s_addr = dwIP;
	}
};

struct CSocketAddress : public CSocketAddressIP
{
	// IP plus port.
	// similar to sockaddr_in but without the waste.
	// use this instead.
private:
	WORD m_port;

public:
	WORD GetPort() const
	{
		return(m_port);
	}
	void SetPort(WORD port) { m_port = port; }
	void SetPortA(WORD port) { throw "not implemented"; }

	CSocketAddress() { throw "not implemented"; }
	CSocketAddress(in_addr dwIP, WORD uPort) { throw "not implemented"; }
	CSocketAddress(DWORD dwIP, WORD uPort) { throw "not implemented"; }
};

struct CSocketNamedAddr : public CSocketAddress
{
	bool IsEmptyHost() const { throw "not implemented"; }
	void EmptyHost() { throw "not implemented"; }
	LPCTSTR GetHostName() { throw "not implemented"; }
	void EmptyAddr() { throw "not implemented"; }
	void SetHostPortStr(LPCTSTR pszHost) { throw "not implemented"; }
	bool UpdateFromHostName() { throw "not implemented"; }

	CSocketNamedAddr() { throw "not implemented"; }
	CSocketNamedAddr(CSocketAddressIP ip, WORD uPort) { throw "not implemented"; }
};

class CGSocket
{
public:
	CSocketAddress GetPeerName() const { throw "not implemented"; }
	void Attach(SOCKET client) { throw "not implemented"; }
	SOCKET Detach() { throw "not implemented"; }
	SOCKET GetSocket() const { throw "not implemented"; }
	CSocketAddress& GetSockName() const { throw "not implemented"; }
	bool IsOpen() const { throw "not implemented"; }
	bool Socket() const { throw "not implemented"; }
	bool ConnectAddr(CSocketNamedAddr& addr) const { throw "not implemented"; }
	int Send(const void* pData, int len) const { throw "not implemented"; }
	int Receive(void* pData, int len, int flags = 0) const { throw "not implemented"; }
	void Close() { throw "not implemented"; }
	int Bind(CSocketAddress& pSockAddrIn) { throw "not implemented"; }
	int Listen() { throw "not implemented"; }
	int IOCtl(long icmd, DWORD* pdwArgs) { throw "not implemented"; }
	int SetSockOpt(int nOptionName, const void* optval, int optlen, int nLevel = SOL_SOCKET) const { throw "not implemented"; }
	int GetSockOpt(int nOptionName, void* optval, int* poptlen, int nLevel = SOL_SOCKET) const { throw "not implemented"; }
	int Accept(CGSocket& socket, CSocketAddress& addr) { throw "not implemented"; }

	static inline int GetLastError() { throw "not implemented"; }

	operator CSocketAddress() const { throw "not implemented"; }
};

class CGSocketSet
{
public:
	CGSocketSet(SOCKET socket) { throw "not implemented"; }
	int GetNFDS() { throw "not implemented"; }
	void Set(SOCKET socket) { throw "not implemented"; }
	bool IsSet(const SOCKET socket) const { throw "not implemented"; }
	operator fd_set*() { throw "not implemented"; }
};

class CLogIP
{
	// Keep a log of recent ip's we have talked to.
	// Prevent ping floods etc.
public:
	void SetAccount(CScriptObj* pAccount) { throw "not implemented"; }
	CScriptObj* GetAccount() const { throw "not implemented"; }
	bool IncPingBlock(bool bVal) { throw "not implemented"; }
	void InitTimes() { throw "not implemented"; }
	void IncBadPassword(LPCTSTR pszAccount) { throw "not implemented"; }
};
typedef CRefPtr<CLogIP> CLogIPPtr;

class CLogIPArray
{
public:
	CLogIPPtr FindLogIP(const CSocketAddress& socket, bool fCreate = false) { throw "not implemented"; }
	bool SetLogIPBlock(LPCTSTR pszIP, LPCTSTR pszReason) { throw "not implemented"; }
	void OnTick() { throw "not implemented"; }
};

#endif
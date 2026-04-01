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
	void SetPortA(WORD port) { m_port = ntohs(port); }

	// Get as sockaddr_in for POSIX calls
	struct sockaddr_in GetAddrPort() const
	{
		struct sockaddr_in SockAddrIn;
		SockAddrIn.sin_family = AF_INET;
		SockAddrIn.sin_port = htons(m_port);
		SockAddrIn.sin_addr.s_addr = s_addr;
		memset(&SockAddrIn.sin_zero, 0, sizeof(SockAddrIn.sin_zero));
		return SockAddrIn;
	}
	void SetAddrPort(const struct sockaddr_in& SockAddrIn)
	{
		s_addr = SockAddrIn.sin_addr.s_addr;
		m_port = ntohs(SockAddrIn.sin_port);
	}

	CSocketAddress()
	{
		m_port = 0;
	}
	CSocketAddress(in_addr dwIP, WORD uPort)
	{
		s_addr = dwIP.s_addr;
		m_port = uPort;
	}
	CSocketAddress(DWORD dwIP, WORD uPort)
	{
		s_addr = dwIP;
		m_port = uPort;
	}
	CSocketAddress(const struct sockaddr_in& SockAddrIn)
	{
		SetAddrPort(SockAddrIn);
	}
};

struct CSocketNamedAddr : public CSocketAddress
{
private:
	CGString m_sHostName;

public:
	bool IsEmptyHost() const { return m_sHostName.IsEmpty(); }
	void EmptyHost() { m_sHostName.Empty(); }
	LPCTSTR GetHostName() { return m_sHostName; }
	void EmptyAddr()
	{
		SetAddrIP(INADDR_BROADCAST);
		SetPort(0);
		m_sHostName.Empty();
	}
	void SetHostPortStr(LPCTSTR pszHost)
	{
		// Parse "host:port" or "host,port" string
		if (pszHost == NULL)
			return;
		TCHAR szHost[256];
		strncpy(szHost, pszHost, sizeof(szHost) - 1);
		szHost[sizeof(szHost) - 1] = '\0';
		TCHAR* pszPort = strchr(szHost, ',');
		if (pszPort == NULL)
			pszPort = strchr(szHost, ':');
		if (pszPort)
		{
			*pszPort = '\0';
			SetPort((WORD)atoi(pszPort + 1));
		}
		m_sHostName = szHost;
		SetHostStr(szHost);
	}
	bool UpdateFromHostName()
	{
		if (m_sHostName.IsEmpty())
			return false;
		return SetHostStr(m_sHostName);
	}

	CSocketNamedAddr() {}
	CSocketNamedAddr(CSocketAddressIP ip, WORD uPort) : CSocketAddress(ip.GetAddrIP(), uPort) {}
};

class CGSocket
{
private:
	SOCKET m_hSocket;

public:
	CGSocket() : m_hSocket(INVALID_SOCKET) {}
	~CGSocket() { Close(); }

	CSocketAddress GetPeerName() const
	{
		struct sockaddr_in SockAddrIn;
		socklen_t len = sizeof(SockAddrIn);
		if (getpeername(m_hSocket, (struct sockaddr*)&SockAddrIn, &len) != 0)
			return CSocketAddress(INADDR_BROADCAST, 0);
		return CSocketAddress(SockAddrIn);
	}
	void Attach(SOCKET client)
	{
		Close();
		m_hSocket = client;
	}
	SOCKET Detach()
	{
		SOCKET s = m_hSocket;
		m_hSocket = INVALID_SOCKET;
		return s;
	}
	SOCKET GetSocket() const
	{
		return m_hSocket;
	}
	CSocketAddress GetSockName() const
	{
		struct sockaddr_in SockAddrIn;
		socklen_t len = sizeof(SockAddrIn);
		if (getsockname(m_hSocket, (struct sockaddr*)&SockAddrIn, &len) != 0)
			return CSocketAddress(INADDR_BROADCAST, 0);
		return CSocketAddress(SockAddrIn);
	}
	bool IsOpen() const
	{
		return (m_hSocket != INVALID_SOCKET);
	}
	bool Socket() const
	{
		// Create a TCP socket. Cast away const since original API declares it const.
		CGSocket* pThis = const_cast<CGSocket*>(this);
		pThis->m_hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		return IsOpen();
	}
	bool ConnectAddr(CSocketNamedAddr& addr) const
	{
		struct sockaddr_in SockAddrIn = addr.GetAddrPort();
		return (connect(m_hSocket, (struct sockaddr*)&SockAddrIn, sizeof(SockAddrIn)) == 0);
	}
	int Send(const void* pData, int len) const
	{
		return send(m_hSocket, (const char*)pData, len, 0);
	}
	int Receive(void* pData, int len, int flags = 0) const
	{
		return recv(m_hSocket, (char*)pData, len, flags);
	}
	void Close()
	{
		if (m_hSocket != INVALID_SOCKET)
		{
			shutdown(m_hSocket, 2);
			closesocket(m_hSocket);
			m_hSocket = INVALID_SOCKET;
		}
	}
	int Bind(CSocketAddress& SockAddr)
	{
		struct sockaddr_in SockAddrIn = SockAddr.GetAddrPort();
		if (SockAddr.IsLocalAddr())
		{
			SockAddrIn.sin_addr.s_addr = INADDR_ANY;
		}
		return (bind(m_hSocket, (struct sockaddr*)&SockAddrIn, sizeof(SockAddrIn)) == 0);
	}
	int Listen()
	{
		return (listen(m_hSocket, SOMAXCONN) == 0);
	}
	int IOCtl(long icmd, DWORD* pdwArgs)
	{
		return ioctlsocket(m_hSocket, icmd, (unsigned long*)pdwArgs);
	}
	int SetSockOpt(int nOptionName, const void* optval, int optlen, int nLevel = SOL_SOCKET) const
	{
		return setsockopt(m_hSocket, nLevel, nOptionName, (const char*)optval, optlen);
	}
	int GetSockOpt(int nOptionName, void* optval, int* poptlen, int nLevel = SOL_SOCKET) const
	{
		return getsockopt(m_hSocket, nLevel, nOptionName, (char*)optval, (socklen_t*)poptlen);
	}
	int Accept(CGSocket& socknew, CSocketAddress& addr)
	{
		struct sockaddr_in SockAddrIn;
		socklen_t len = sizeof(SockAddrIn);
		SOCKET hSocket = accept(m_hSocket, (struct sockaddr*)&SockAddrIn, &len);
		if (hSocket == INVALID_SOCKET)
			return 0;
		socknew.Attach(hSocket);
		addr.SetAddrPort(SockAddrIn);
		return 1;
	}

	static inline int GetLastError()
	{
#ifdef _WIN32
		return WSAGetLastError();
#else
		return errno;
#endif
	}

	operator CSocketAddress() const
	{
		return GetSockName();
	}
};

class CGSocketSet
{
private:
	fd_set m_fds;
	int m_nfds;

public:
	CGSocketSet(SOCKET socket)
	{
		FD_ZERO(&m_fds);
		m_nfds = 0;
		if (socket != INVALID_SOCKET)
			Set(socket);
	}
	int GetNFDS()
	{
		return m_nfds + 1;
	}
	void Set(SOCKET socket)
	{
		if (socket == INVALID_SOCKET)
			return;
		FD_SET(socket, &m_fds);
		if ((int)socket >= m_nfds)
			m_nfds = (int)socket;
	}
	bool IsSet(const SOCKET socket) const
	{
		if (socket == INVALID_SOCKET)
			return false;
		return FD_ISSET(socket, &m_fds) != 0;
	}
	operator fd_set*()
	{
		return &m_fds;
	}
};

class CLogIP : public CRefObjDef
{
	// Keep a log of recent ip's we have talked to.
	// Prevent ping floods etc.
private:
	CSocketAddressIP m_ip;
	CScriptObj* m_pAccount;
	int m_iPingBlocks;
	int m_iBadPasswords;

public:
	CLogIP() : m_pAccount(NULL), m_iPingBlocks(0), m_iBadPasswords(0) {}

	CSocketAddressIP GetIP() const { return m_ip; }
	void SetIP(const CSocketAddressIP& ip) { m_ip = ip; }

	void SetAccount(CScriptObj* pAccount) { m_pAccount = pAccount; }
	CScriptObj* GetAccount() const { return m_pAccount; }
	bool IncPingBlock(bool bVal)
	{
		if (bVal)
			m_iPingBlocks++;
		// Block if too many pings in a short time
		return (m_iPingBlocks > 100);
	}
	void InitTimes()
	{
		m_iPingBlocks = 0;
		m_iBadPasswords = 0;
	}
	void IncBadPassword(LPCTSTR pszAccount)
	{
		m_iBadPasswords++;
	}
};
typedef CRefPtr<CLogIP> CLogIPPtr;

class CLogIPArray
{
private:
	// Simple fixed-size array of recent IPs
	enum { MAX_LOG_IPS = 256 };
	CLogIPPtr m_ips[MAX_LOG_IPS];
	int m_nCount;

public:
	CLogIPArray() : m_nCount(0) {}

	CLogIPPtr FindLogIP(const CSocketAddress& socket, bool fCreate = false)
	{
		// Search existing entries
		for (int i = 0; i < m_nCount; i++)
		{
			if (m_ips[i] && m_ips[i]->GetIP().IsSameIP(socket))
				return m_ips[i];
		}
		if (!fCreate)
			return NULL;
		// Create new entry if room
		if (m_nCount >= MAX_LOG_IPS)
		{
			// Recycle oldest entry
			m_ips[0] = new CLogIP();
			m_ips[0]->SetIP(socket);
			m_ips[0]->InitTimes();
			return m_ips[0];
		}
		CLogIPPtr pNew = new CLogIP();
		pNew->SetIP(socket);
		pNew->InitTimes();
		m_ips[m_nCount++] = pNew;
		return pNew;
	}
	bool SetLogIPBlock(LPCTSTR pszIP, LPCTSTR pszReason) { return false; }
	void OnTick()
	{
		// Periodically decay ping block counts
		for (int i = 0; i < m_nCount; i++)
		{
			if (m_ips[i])
				m_ips[i]->InitTimes();
		}
	}
};

#endif
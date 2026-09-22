// CFile.cpp - File I/O implementations for Linux
// Adapted from SphereServer99 reference implementation

#include "stdafx.h"
#include "cfile.h"
#include <cstring>
#include <cstdarg>
#include <cerrno>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef SPHERE_SAVE_IO_TEST
static CFileText::TEST_FAULT g_eTestFault = CFileText::TEST_FAULT_NONE;
static bool g_fTestFaultTriggered = false;
#endif

///////////////////////////////////////////////////////////
// CFile

void CFile::SetFilePath(LPCTSTR pszName)
{
	if (!pszName)
		return;

	bool fIsOpen = (m_hFile != NOFILE_HANDLE);
	if (fIsOpen)
		Close();

	m_strFileName = pszName;
	if (fIsOpen)
		Open(NULL, OF_READ | OF_BINARY);
}

LPCTSTR CFile::GetFileTitle() const
{
	return CGFile::GetFilesTitle(GetFilePath());
}

bool CFile::Open(LPCTSTR pszName, UINT uMode, CGrayError* e)
{
	if (m_hFile != NOFILE_HANDLE)
		Close();

	if (pszName)
		m_strFileName = pszName;

#ifdef _WIN32
	// Windows implementation not needed for Linux build
	return false;
#else
	int flags = uMode & 0x0F;  // OF_READ, OF_WRITE, OF_READWRITE
	if (uMode & OF_CREATE)
		flags |= O_CREAT | O_TRUNC;
	m_hFile = open(GetFilePath(), flags, 0666);
#endif
	return (m_hFile != NOFILE_HANDLE);
}

void CFile::Close()
{
	if (m_hFile != NOFILE_HANDLE)
	{
#ifdef _WIN32
		CloseHandle(m_hFile);
#else
		close(m_hFile);
#endif
		m_hFile = NOFILE_HANDLE;
	}
}

DWORD CFile::GetLength()
{
	DWORD dwPos = GetPosition();
	DWORD dwSize = SeekToEnd();
	Seek(dwPos, SEEK_SET);
	return dwSize;
}

DWORD CFile::GetPosition() const
{
#ifdef _WIN32
	return 0;
#else
	return lseek(m_hFile, 0, SEEK_CUR);
#endif
}

DWORD CFile::Seek(LONG lOffset, UINT iOrigin)
{
#ifdef _WIN32
	return 0;
#else
	if (m_hFile <= 0)
		return (DWORD)-1;
	return lseek(m_hFile, (off_t)lOffset, iOrigin);
#endif
}

DWORD CFile::Read(void* pData, DWORD dwLength) const
{
#ifdef _WIN32
	return 0;
#else
	ssize_t iRead = read(m_hFile, pData, dwLength);
	if (iRead == -1)
		return 0;
	return (DWORD)iRead;
#endif
}

bool CFile::Write(const void* pData, DWORD dwLength) const
{
#ifdef _WIN32
	return false;
#else
	ssize_t iWritten = write(m_hFile, pData, dwLength);
	if (iWritten == -1)
		return false;
	return (iWritten == (ssize_t)dwLength);
#endif
}

///////////////////////////////////////////////////////////
// CGFile

int CGFile::GetLastError()
{
#ifdef _WIN32
	return ::GetLastError();
#else
	return errno;
#endif
}

CGString CGFile::GetMergedFileName(LPCTSTR pszBase, LPCTSTR pszName)
{
	TCHAR szFullPath[1024];
	if (pszBase && pszBase[0])
	{
		strncpy(szFullPath, pszBase, sizeof(szFullPath) - 1);
		szFullPath[sizeof(szFullPath) - 1] = '\0';

		size_t iLen = strlen(szFullPath);
		if (iLen && szFullPath[iLen - 1] != '/' && szFullPath[iLen - 1] != '\\')
		{
			strcat(szFullPath, "/");
		}
	}
	else
		szFullPath[0] = '\0';

	if (pszName)
		strncat(szFullPath, pszName, sizeof(szFullPath) - strlen(szFullPath) - 1);

	return CGString(szFullPath);
}

LPCTSTR CGFile::GetFilesTitle(LPCTSTR pszPath)
{
	if (!pszPath) return "";
	size_t iLen = strlen(pszPath);
	while (iLen > 0)
	{
		iLen--;
		if (pszPath[iLen] == '\\' || pszPath[iLen] == '/')
		{
			iLen++;
			break;
		}
	}
	return pszPath + iLen;
}

LPCTSTR CGFile::GetFilesExt(LPCTSTR pszPath)
{
	if (!pszPath) return NULL;
	size_t iLen = strlen(pszPath);
	while (iLen > 0)
	{
		iLen--;
		if (pszPath[iLen] == '\\' || pszPath[iLen] == '/')
			break;
		if (pszPath[iLen] == '.')
			return pszPath + iLen;
	}
	return NULL;
}

bool CGFile::OpenBase(void* pExtra)
{
	(void)pExtra;
	return CFile::Open(GetFilePath(), GetMode());
}

void CGFile::CloseBase()
{
	CFile::Close();
}

bool CGFile::Open(LPCTSTR pszName, UINT uMode, void* pExtra)
{
	if (!pszName)
	{
		if (IsFileOpen())
			return true;
	}
	else
		Close();

	if (pszName)
		m_strFileName = pszName;

	if (m_strFileName.IsEmpty())
		return false;

	m_uMode = uMode;
	if (OpenBase(pExtra))
		return true;

	// If open failed and the name has no extension, try appending .scp
	LPCTSTR pszExt = GetFileNameExt(m_strFileName);
	if (!pszExt || !pszExt[0])
	{
		CGString sWithExt;
		sWithExt.Format("%s.scp", (LPCTSTR)m_strFileName);
		m_strFileName = sWithExt;
		return OpenBase(pExtra);
	}

	return false;
}

void CGFile::Close()
{
	if (!IsFileOpen())
		return;

	CloseBase();
	m_hFile = NOFILE_HANDLE;
}

///////////////////////////////////////////////////////////
// CFileText

LPCTSTR CFileText::GetModeStr() const
{
	if (IsBinaryMode())
		return IsModeWrite() ? "wb" : "rb";
	if (GetMode() & OF_READWRITE)
		return "a+b";
	if (GetMode() & OF_CREATE)
		return "w";
	if (IsModeWrite())
		return "w";
	return "rb";
}

void CFileText::CloseBase()
{
	CloseChecked();
}

bool CFileText::OpenBase(void* pExtra)
{
	(void)pExtra;
	m_pStream = fopen(GetFilePath(), GetModeStr());
	if (!m_pStream)
		return false;

	m_fIOError = false;
	m_hFile = (OSFILE_TYPE)(intptr_t)fileno(m_pStream);
	return true;
}

DWORD CFileText::Seek(LONG lOffset, UINT iOrigin)
{
	if (!IsFileOpen())
		return 0;
	if (lOffset < 0)
		return 0;
	if (fseek(m_pStream, lOffset, iOrigin) != 0)
		return 0;

	long lPos = ftell(m_pStream);
	if (lPos < 0)
		return 0;
	return (DWORD)lPos;
}

bool CFileText::Flush() const
{
	if (!IsFileOpen())
		return !m_fIOError;
#ifdef SPHERE_SAVE_IO_TEST
	if ( g_eTestFault == TEST_FAULT_FLUSH && !g_fTestFaultTriggered )
	{
		g_fTestFaultTriggered = true;
		m_fIOError = true;
		errno = EIO;
		return false;
	}
#endif
	if ( fflush(m_pStream) != 0 )
	{
		m_fIOError = true;
		return false;
	}
	return true;
}

bool CFileText::CloseChecked()
{
	if ( !m_pStream )
		return !m_fIOError;

	bool fOK = true;
	if ( IsModeWrite() && !Flush())
		fOK = false;
#ifdef SPHERE_SAVE_IO_TEST
	if ( g_eTestFault == TEST_FAULT_CLOSE && !g_fTestFaultTriggered )
	{
		g_fTestFaultTriggered = true;
		m_fIOError = true;
		fOK = false;
	}
#endif
	if ( fclose(m_pStream) != 0 )
	{
		m_fIOError = true;
		fOK = false;
	}
	m_pStream = NULL;
	m_hFile = NOFILE_HANDLE;
	return fOK && !m_fIOError;
}

DWORD CFileText::GetPosition() const
{
	if (!IsFileOpen())
		return (DWORD)-1;
	return ftell(m_pStream);
}

DWORD CFileText::Read(void* pBuffer, size_t sizemax) const
{
	if (!pBuffer || IsEOF())
		return 0;
	return fread(pBuffer, 1, sizemax, m_pStream);
}

#ifndef _WIN32
bool CFileText::Write(const void* pData, DWORD iLen) const
#else
bool CFileText::Write(const void* pData, DWORD iLen)
#endif
{
	if (!pData || !IsFileOpen())
	{
		m_fIOError = true;
		return false;
	}
#ifdef SPHERE_SAVE_IO_TEST
	if ( g_eTestFault == TEST_FAULT_SHORT_WRITE && !g_fTestFaultTriggered )
	{
		g_fTestFaultTriggered = true;
		m_fIOError = true;
		return false;
	}
#endif
	size_t iStatus = fwrite(pData, iLen, 1, m_pStream);
#ifndef _WIN32
	if ( iStatus == 1 && !Flush())
		return false;
#endif
	if ( iStatus != 1 )
		m_fIOError = true;
	return (iStatus == 1) && !m_fIOError;
}

bool CFileText::WriteString(LPCTSTR pStr)
{
	if (!pStr)
		return false;
	return Write(pStr, strlen(pStr));
}

size_t CFileText::VPrintf(LPCTSTR pFormat, va_list args)
{
	if (!pFormat || !IsFileOpen())
	{
		m_fIOError = true;
		return 0;
	}
	int iStatus = vfprintf(m_pStream, pFormat, args);
	if ( iStatus < 0 )
	{
		m_fIOError = true;
		return 0;
	}
#ifdef SPHERE_SAVE_IO_TEST
	if ( g_eTestFault == TEST_FAULT_SHORT_WRITE && !g_fTestFaultTriggered )
	{
		g_fTestFaultTriggered = true;
		m_fIOError = true;
		return iStatus > 0 ? (size_t)(iStatus - 1) : 0;
	}
#endif
	return (size_t)iStatus;
}

size_t _cdecl CFileText::Printf(LPCTSTR pFormat, ...)
{
	if (!pFormat)
		return 0;
	va_list vargs;
	va_start(vargs, pFormat);
	size_t ret = VPrintf(pFormat, vargs);
	va_end(vargs);
	return ret;
}

#ifdef SPHERE_SAVE_IO_TEST
void CFileText::SetTestFault( TEST_FAULT fault )
{
	g_eTestFault = fault;
	g_fTestFaultTriggered = false;
}

void CFileText::ClearTestFault()
{
	g_eTestFault = TEST_FAULT_NONE;
	g_fTestFaultTriggered = false;
}

bool CFileText::WasTestFaultTriggered()
{
	return g_fTestFaultTriggered;
}
#endif

bool CFileText::IsEOF() const
{
	if (!IsFileOpen())
		return true;
	return (bool)feof(m_pStream);
}

TCHAR* CFileText::ReadString(TCHAR* pBuffer, size_t sizemax) const
{
	if (!pBuffer || IsEOF())
		return NULL;
	return fgets(pBuffer, (int)sizemax, m_pStream);
}

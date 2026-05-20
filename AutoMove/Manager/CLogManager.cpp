#include "pch.h"
#include "CLogManager.h"
#include <afxmt.h>

namespace
{
	CMutex g_logMutex(FALSE, nullptr);
}

void CLogManager::Write(LOG_TYPE eLogType, const CString& strMessage)
{
	if (!EnsureLogDirectory())
	{
		return;
	}

	WriteUtf8Line(GetLogPath(eLogType), BuildLogLine(eLogType, strMessage));
}

void CLogManager::Clear(LOG_TYPE eLogType)
{
	DeleteFile(GetLogPath(eLogType));
}

CString CLogManager::GetLogPath(LOG_TYPE eLogType)
{
	return GetLogDirectory() + _T("\\") + GetLogTypeName(eLogType) + _T(".log");
}

CString CLogManager::GetLogDirectory()
{
	return _T(".\\Logs");
}

CString CLogManager::GetLogTypeName(LOG_TYPE eLogType)
{
	switch (eLogType)
	{
	case LOG_SYSTEM:
		return _T("System");
	case LOG_OPERATION:
		return _T("Operation");
	default:
		return _T("Unknown");
	}
}

CString CLogManager::GetTimestamp()
{
	SYSTEMTIME systemTime = {};
	GetLocalTime(&systemTime);

	CString strTime;
	strTime.Format(_T("%04d-%02d-%02d %02d:%02d:%02d.%03d"),
		systemTime.wYear,
		systemTime.wMonth,
		systemTime.wDay,
		systemTime.wHour,
		systemTime.wMinute,
		systemTime.wSecond,
		systemTime.wMilliseconds);
	return strTime;
}

CString CLogManager::BuildLogLine(LOG_TYPE eLogType, const CString& strMessage)
{
	CString strLogType = GetLogTypeName(eLogType);
	strLogType.MakeUpper();

	CString strLine;
	strLine.Format(_T("[%s][%s] %s"),
		static_cast<LPCTSTR>(GetTimestamp()),
		static_cast<LPCTSTR>(strLogType),
		static_cast<LPCTSTR>(strMessage));
	return strLine;
}

BOOL CLogManager::EnsureLogDirectory()
{
	const CString strDirectory = GetLogDirectory();
	const DWORD dwAttributes = GetFileAttributes(strDirectory);
	if (dwAttributes != INVALID_FILE_ATTRIBUTES)
	{
		return (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	return CreateDirectory(strDirectory, nullptr);
}

BOOL CLogManager::WriteUtf8Line(const CString& strPath, const CString& strLine)
{
	CSingleLock lock(&g_logMutex, TRUE);

	CFile file;
	if (!file.Open(strPath,
		CFile::modeCreate | CFile::modeNoTruncate | CFile::modeWrite | CFile::typeBinary))
	{
		return FALSE;
	}

	if (file.GetLength() == 0)
	{
		const BYTE utf8Bom[] = { 0xEF, 0xBB, 0xBF };
		file.Write(utf8Bom, static_cast<UINT>(sizeof(utf8Bom)));
	}

	CString strOutput = strLine;
	strOutput += _T("\r\n");

#ifdef UNICODE
	const int nUtf8Length = WideCharToMultiByte(CP_UTF8, 0, strOutput, -1, nullptr, 0, nullptr, nullptr);
	if (nUtf8Length <= 1)
	{
		file.Close();
		return FALSE;
	}

	CStringA strUtf8;
	LPSTR lpszUtf8 = strUtf8.GetBuffer(nUtf8Length - 1);
	WideCharToMultiByte(CP_UTF8, 0, strOutput, -1, lpszUtf8, nUtf8Length, nullptr, nullptr);
	strUtf8.ReleaseBuffer(nUtf8Length - 1);
#else
	CStringA strUtf8(strOutput);
#endif

	file.SeekToEnd();
	file.Write(static_cast<LPCSTR>(strUtf8), static_cast<UINT>(strUtf8.GetLength()));
	file.Close();

	return TRUE;
}

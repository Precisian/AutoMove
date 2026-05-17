#include "pch.h"
#include "CDriveManager.h"

CDriveManager::CDriveManager(const std::vector<CString>& vecDriveNames)
{
	for (int i = 0; i < static_cast<int>(vecDriveNames.size()); ++i)
	{
		DRIVE_INFO driveInfo;
		driveInfo.strDriveName = vecDriveNames[i];
		m_vecDriveInfos.push_back(driveInfo);
	}
}

void CDriveManager::LoadAvailableDrives()
{
	m_vecDriveInfos.clear();

	const DWORD dwLogicalDrives = GetLogicalDrives();
	for (TCHAR chDrive = _T('D'); chDrive <= _T('Z'); ++chDrive)
	{
		const DWORD dwMask = 1 << (chDrive - _T('A'));
		if ((dwLogicalDrives & dwMask) == 0)
		{
			continue;
		}

		CString strRoot;
		strRoot.Format(_T("%c:\\"), chDrive);
		if (GetDriveType(strRoot) == DRIVE_NO_ROOT_DIR)
		{
			continue;
		}

		DRIVE_INFO driveInfo;
		driveInfo.strDriveName.Format(_T("%c"), chDrive);
		m_vecDriveInfos.push_back(driveInfo);
	}
}

void CDriveManager::CheckDriveUsage()
{
	for (int i = 0; i < static_cast<int>(m_vecDriveInfos.size()); ++i)
	{
		ULARGE_INTEGER freeBytesAvailable = {};
		ULARGE_INTEGER totalBytes = {};
		ULARGE_INTEGER totalFreeBytes = {};

		if (!GetDiskFreeSpaceEx(BuildDriveRootPath(m_vecDriveInfos[i].strDriveName),
			&freeBytesAvailable, &totalBytes, &totalFreeBytes))
		{
			continue;
		}

		m_vecDriveInfos[i].nUsagePercent = CalculateUsagePercent(totalBytes, totalFreeBytes);
	}
}

std::vector<CString> CDriveManager::GetDriveNames() const
{
	std::vector<CString> vecDriveNames;

	for (int i = 0; i < static_cast<int>(m_vecDriveInfos.size()); ++i)
	{
		vecDriveNames.push_back(m_vecDriveInfos[i].strDriveName);
	}

	return vecDriveNames;
}

const std::vector<DRIVE_INFO>& CDriveManager::GetDriveInfos() const
{
	return m_vecDriveInfos;
}

CString CDriveManager::BuildDriveRootPath(const CString& strDriveName) const
{
	CString strRoot = strDriveName;
	strRoot.Trim();

	if (strRoot.GetLength() == 1)
	{
		strRoot.Format(_T("%c:\\"), strRoot[0]);
	}
	else if (strRoot.GetLength() == 2 && strRoot[1] == _T(':'))
	{
		strRoot += _T("\\");
	}

	return strRoot;
}

int CDriveManager::CalculateUsagePercent(const ULARGE_INTEGER& totalBytes, const ULARGE_INTEGER& totalFreeBytes) const
{
	if (totalBytes.QuadPart == 0)
	{
		return 0;
	}

	const ULONGLONG ullUsedBytes = totalBytes.QuadPart - totalFreeBytes.QuadPart;
	const int nPercent = static_cast<int>((static_cast<double>(ullUsedBytes) * 100.0 / static_cast<double>(totalBytes.QuadPart)) + 0.5);

	if (nPercent < 0)
	{
		return 0;
	}
	if (nPercent > 100)
	{
		return 100;
	}

	return nPercent;
}

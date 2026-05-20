#include "pch.h"
#include "CDriveManager.h"
#include <algorithm>

namespace
{
	struct FIND_ITEM
	{
		CString strPath;
		FILETIME ftLastWriteTime = {};
		int nDepth = 0;
		BOOL bDirectory = FALSE;
	};

	CString NormalizeDirectoryPath(CString strPath)
	{
		strPath.Trim();

		while (strPath.GetLength() > 3
			&& (strPath.Right(1) == _T("\\") || strPath.Right(1) == _T("/")))
		{
			strPath = strPath.Left(strPath.GetLength() - 1);
		}

		return strPath;
	}

	CString CombinePath(const CString& strParent, const CString& strName)
	{
		CString strPath = strParent;
		if (!strPath.IsEmpty()
			&& strPath.Right(1) != _T("\\")
			&& strPath.Right(1) != _T("/"))
		{
			strPath += _T("\\");
		}

		strPath += strName;
		return strPath;
	}

	CString BuildSearchPath(const CString& strDirectory)
	{
		return CombinePath(strDirectory, _T("*"));
	}

	BOOL IsDots(const CString& strName)
	{
		return strName == _T(".") || strName == _T("..");
	}

	BOOL IsReparsePoint(const WIN32_FIND_DATA& findData)
	{
		return (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
	}

	void SortByOldestFirst(std::vector<FIND_ITEM>& vecItems)
	{
		std::sort(vecItems.begin(), vecItems.end(),
			[](const FIND_ITEM& lhs, const FIND_ITEM& rhs)
			{
				const LONG nCompare = CompareFileTime(&lhs.ftLastWriteTime, &rhs.ftLastWriteTime);
				if (nCompare != 0)
				{
					return nCompare < 0;
				}

				return lhs.strPath.CompareNoCase(rhs.strPath) < 0;
			});
	}

	void SortFoldersForCleanup(std::vector<FIND_ITEM>& vecItems)
	{
		std::sort(vecItems.begin(), vecItems.end(),
			[](const FIND_ITEM& lhs, const FIND_ITEM& rhs)
			{
				if (lhs.nDepth != rhs.nDepth)
				{
					return lhs.nDepth > rhs.nDepth;
				}

				const LONG nCompare = CompareFileTime(&lhs.ftLastWriteTime, &rhs.ftLastWriteTime);
				if (nCompare != 0)
				{
					return nCompare < 0;
				}

				return lhs.strPath.CompareNoCase(rhs.strPath) < 0;
			});
	}

	void ScanFolderWorkItemsBottomUp(const FIND_ITEM& rootItem, std::vector<CString>& vecResult)
	{
		std::vector<FIND_ITEM> vecStack;
		std::vector<FIND_ITEM> vecFolders;
		vecStack.push_back(rootItem);

		while (!vecStack.empty())
		{
			const FIND_ITEM currentItem = vecStack.back();
			vecStack.pop_back();
			vecFolders.push_back(currentItem);

			WIN32_FIND_DATA findData = {};
			HANDLE hFind = FindFirstFile(BuildSearchPath(currentItem.strPath), &findData);
			if (hFind == INVALID_HANDLE_VALUE)
			{
				continue;
			}

			do
			{
				const CString strName(findData.cFileName);
				if (IsDots(strName) || IsReparsePoint(findData))
				{
					continue;
				}

				if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				{
					continue;
				}

				FIND_ITEM childItem;
				childItem.strPath = CombinePath(currentItem.strPath, strName);
				childItem.ftLastWriteTime = findData.ftLastWriteTime;
				childItem.nDepth = currentItem.nDepth + 1;
				childItem.bDirectory = TRUE;
				vecStack.push_back(childItem);
			} while (FindNextFile(hFind, &findData));

			FindClose(hFind);
		}

		SortFoldersForCleanup(vecFolders);

		for (int i = 0; i < static_cast<int>(vecFolders.size()); ++i)
		{
			vecResult.push_back(vecFolders[i].strPath);
		}
	}
}

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

// Build cleanup work items without keeping every child file path in memory.
// Folder consumers should process direct child files only, then delete the folder if it is empty.
std::vector<CString> CDriveManager::FindFiles(CString strPath)
{
	std::vector<CString> vecResult;
	strPath = NormalizeDirectoryPath(strPath);

	if (strPath.IsEmpty())
	{
		return vecResult;
	}

	const DWORD dwAttributes = GetFileAttributes(strPath);
	if (dwAttributes == INVALID_FILE_ATTRIBUTES
		|| (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0
		|| (dwAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
	{
		return vecResult;
	}

	std::vector<FIND_ITEM> vecTopItems;

	WIN32_FIND_DATA findData = {};
	HANDLE hFind = FindFirstFile(BuildSearchPath(strPath), &findData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return vecResult;
	}

	do
	{
		const CString strName(findData.cFileName);
		if (IsDots(strName) || IsReparsePoint(findData))
		{
			continue;
		}

		FIND_ITEM item;
		item.strPath = CombinePath(strPath, strName);
		item.ftLastWriteTime = findData.ftLastWriteTime;
		item.nDepth = 0;
		item.bDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		vecTopItems.push_back(item);
	} while (FindNextFile(hFind, &findData));

	FindClose(hFind);

	SortByOldestFirst(vecTopItems);

	for (int i = 0; i < static_cast<int>(vecTopItems.size()); ++i)
	{
		if (vecTopItems[i].bDirectory)
		{
			ScanFolderWorkItemsBottomUp(vecTopItems[i], vecResult);
		}
		else
		{
			vecResult.push_back(vecTopItems[i].strPath);
		}
	}

	return vecResult;
}

void CDriveManager::RemoveFiles(const std::vector<CString>& vecFilePaths)
{
}

void CDriveManager::MoveFiles(const std::vector<CString>& vecFilePaths, const CString& strDestPath)
{
}

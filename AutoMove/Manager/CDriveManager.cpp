#include "pch.h"
#include "CDriveManager.h"
#include "FileSystemUtil.h"
#include <algorithm>

namespace
{
	using namespace AutoMoveFileSystem;
	struct FILE_SYSTEM_ITEM
	{
		CString strPath;
		FILETIME ftCreationTime = {};
	};

	class CFindHandle
	{
	public:
		explicit CFindHandle(const CString& strSearchPath)
			: m_hFind(FindFirstFile(strSearchPath, &m_findData))
		{
		}

		CFindHandle(const CFindHandle&) = delete;
		CFindHandle& operator=(const CFindHandle&) = delete;

		~CFindHandle()
		{
			if (IsValid())
			{
				FindClose(m_hFind);
			}
		}

		BOOL IsValid() const
		{
			return m_hFind != INVALID_HANDLE_VALUE;
		}

		const WIN32_FIND_DATA& GetData() const
		{
			return m_findData;
		}

		BOOL MoveNext()
		{
			return FindNextFile(m_hFind, &m_findData);
		}

	private:
		HANDLE m_hFind;
		WIN32_FIND_DATA m_findData = {};
	};

	BOOL IsValidCleanupRoot(const CString& strPath)
	{
		return IsValidDirectoryPath(strPath);
	}
	FILE_SYSTEM_ITEM MakeFileSystemItem(const CString& strParent,
		const WIN32_FIND_DATA& findData)
	{
		FILE_SYSTEM_ITEM item;
		item.strPath = CombinePath(strParent, findData.cFileName);
		item.ftCreationTime = findData.ftCreationTime;
		return item;
	}

	BOOL ShouldSkipFindData(const WIN32_FIND_DATA& findData)
	{
		const CString strName(findData.cFileName);
		return strName.IsEmpty() || IsDots(strName) || IsReparsePoint(findData);
	}

	void SortByOldestFirst(std::vector<FILE_SYSTEM_ITEM>& vecItems)
	{
		std::sort(vecItems.begin(), vecItems.end(),
			[](const FILE_SYSTEM_ITEM& lhs, const FILE_SYSTEM_ITEM& rhs)
			{
				const LONG nCompare = CompareFileTime(&lhs.ftCreationTime, &rhs.ftCreationTime);
				if (nCompare != 0)
				{
					return nCompare < 0;
				}

				return lhs.strPath.CompareNoCase(rhs.strPath) < 0;
			});
	}

	std::vector<FILE_SYSTEM_ITEM> LoadTopLevelItems(const CString& strDirectory)
	{
		std::vector<FILE_SYSTEM_ITEM> vecItems;

		CFindHandle find(BuildSearchPath(strDirectory));
		if (!find.IsValid())
		{
			return vecItems;
		}

		do
		{
			const WIN32_FIND_DATA& findData = find.GetData();
			if (ShouldSkipFindData(findData))
			{
				continue;
			}

			vecItems.push_back(MakeFileSystemItem(strDirectory, findData));
		} while (find.MoveNext());

		SortByOldestFirst(vecItems);
		return vecItems;
	}

	std::vector<CString> LoadTopLevelPaths(CString strPath)
	{
		std::vector<CString> vecResult;
		strPath = NormalizeDirectoryPath(strPath);

		if (strPath.IsEmpty() || !IsValidCleanupRoot(strPath))
		{
			return vecResult;
		}

		const std::vector<FILE_SYSTEM_ITEM> vecTopItems = LoadTopLevelItems(strPath);
		for (int i = 0; i < static_cast<int>(vecTopItems.size()); ++i)
		{
			vecResult.push_back(vecTopItems[i].strPath);
		}

		return vecResult;
	}

	class CScopedBackgroundThreadPriority
	{
	public:
		CScopedBackgroundThreadPriority()
			: m_hThread(GetCurrentThread())
			, m_nOriginalPriority(GetThreadPriority(m_hThread))
			, m_bBackgroundMode(FALSE)
			, m_bPriorityChanged(FALSE)
		{
			m_bBackgroundMode = SetThreadPriority(m_hThread, THREAD_MODE_BACKGROUND_BEGIN);
			if (!m_bBackgroundMode && m_nOriginalPriority != THREAD_PRIORITY_ERROR_RETURN)
			{
				m_bPriorityChanged = SetThreadPriority(m_hThread, THREAD_PRIORITY_BELOW_NORMAL);
			}
		}

		~CScopedBackgroundThreadPriority()
		{
			if (m_bBackgroundMode)
			{
				SetThreadPriority(m_hThread, THREAD_MODE_BACKGROUND_END);
			}
			else if (m_bPriorityChanged && m_nOriginalPriority != THREAD_PRIORITY_ERROR_RETURN)
			{
				SetThreadPriority(m_hThread, m_nOriginalPriority);
			}
		}

	private:
		HANDLE m_hThread;
		int m_nOriginalPriority;
		BOOL m_bBackgroundMode;
		BOOL m_bPriorityChanged;
	};

	class CLowImpactDeleteContext
	{
	public:
		BOOL DeletePath(const CString& strPath)
		{
			const DWORD dwAttributes = GetFileAttributes(strPath);
			if (dwAttributes == INVALID_FILE_ATTRIBUTES || IsReparsePoint(dwAttributes))
			{
				return FALSE;
			}

			if (IsDirectory(dwAttributes))
			{
				return DeleteDirectoryTree(strPath);
			}

			return DeleteFilePath(strPath);
		}

	private:
		void PauseAfterWork()
		{
			++m_nWorkCount;
			Sleep((m_nWorkCount % 8) == 0 ? 10 : 1);
		}

		BOOL ClearReadOnlyAttribute(const CString& strPath, DWORD dwAttributes)
		{
			if ((dwAttributes & FILE_ATTRIBUTE_READONLY) == 0)
			{
				return TRUE;
			}

			return SetFileAttributes(strPath, dwAttributes & ~FILE_ATTRIBUTE_READONLY);
		}

		BOOL DeleteFilePath(const CString& strPath)
		{
			const DWORD dwAttributes = GetFileAttributes(strPath);
			if (dwAttributes == INVALID_FILE_ATTRIBUTES
				|| IsDirectory(dwAttributes)
				|| IsReparsePoint(dwAttributes))
			{
				return FALSE;
			}

			ClearReadOnlyAttribute(strPath, dwAttributes);

			const BOOL bDeleted = DeleteFile(strPath);
			PauseAfterWork();
			return bDeleted;
		}

		BOOL DeleteDirectoryTree(const CString& strRootPath)
		{
			std::vector<CString> vecDirectories;
			std::vector<CString> vecStack;
			vecStack.push_back(strRootPath);

			while (!vecStack.empty())
			{
				const CString strDirectory = vecStack.back();
				vecStack.pop_back();
				vecDirectories.push_back(strDirectory);

				CFindHandle find(BuildSearchPath(strDirectory));
				if (!find.IsValid())
				{
					continue;
				}

				do
				{
					const WIN32_FIND_DATA& findData = find.GetData();
					if (ShouldSkipFindData(findData))
					{
						continue;
					}

					const CString strChildPath = CombinePath(strDirectory, findData.cFileName);
					if (IsDirectory(findData.dwFileAttributes))
					{
						vecStack.push_back(strChildPath);
					}
					else
					{
						DeleteFilePath(strChildPath);
					}
				} while (find.MoveNext());
			}

			std::sort(vecDirectories.begin(), vecDirectories.end(),
				[](const CString& lhs, const CString& rhs)
				{
					return lhs.GetLength() > rhs.GetLength();
				});

			BOOL bResult = TRUE;
			for (int i = 0; i < static_cast<int>(vecDirectories.size()); ++i)
			{
				bResult &= RemoveDirectoryIfEmpty(vecDirectories[i]);
			}

			return bResult;
		}

		BOOL RemoveDirectoryIfEmpty(const CString& strPath)
		{
			const DWORD dwAttributes = GetFileAttributes(strPath);
			if (dwAttributes == INVALID_FILE_ATTRIBUTES
				|| !IsDirectory(dwAttributes)
				|| IsReparsePoint(dwAttributes))
			{
				return FALSE;
			}

			ClearReadOnlyAttribute(strPath, dwAttributes);

			const BOOL bRemoved = RemoveDirectory(strPath);
			PauseAfterWork();
			return bRemoved;
		}

		CScopedBackgroundThreadPriority m_backgroundPriority;
		int m_nWorkCount = 0;
	};

	BOOL MovePathToDirectory(CString strPath, CString strDestPath)
	{
		strPath.Trim();
		strDestPath = NormalizeDirectoryPath(strDestPath);
		if (strPath.IsEmpty() || strDestPath.IsEmpty())
		{
			return FALSE;
		}

		const DWORD dwAttributes = GetFileAttributes(strPath);
		if (dwAttributes == INVALID_FILE_ATTRIBUTES || IsReparsePoint(dwAttributes))
		{
			return FALSE;
		}

		if (!EnsureDirectoryExists(strDestPath))
		{
			return FALSE;
		}

		const CString strFileName = GetFileNameFromPath(strPath);
		if (strFileName.IsEmpty())
		{
			return FALSE;
		}

		const CString strTargetPath = CombinePath(strDestPath, strFileName);
		CString strFromList = strPath;
		CString strToList = strTargetPath;
		strFromList += _T('\0');
		strToList += _T('\0');

		SHFILEOPSTRUCT fileOp = {};
		fileOp.wFunc = FO_MOVE;
		fileOp.pFrom = strFromList;
		fileOp.pTo = strToList;
		fileOp.fFlags = FOF_NOCONFIRMATION
			| FOF_NOCONFIRMMKDIR
			| FOF_NOERRORUI
			| FOF_RENAMEONCOLLISION
			| FOF_SILENT;

		return SHFileOperation(&fileOp) == 0 && !fileOp.fAnyOperationsAborted;
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
		if (GetDriveType(strRoot) != DRIVE_FIXED)
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

int CDriveManager::FindDriveUsagePercent(const std::vector<DRIVE_INFO>& vecDriveInfos, LPCTSTR lpszDriveName)
{
	CString strDriveName;
	if (lpszDriveName != nullptr)
	{
		strDriveName = lpszDriveName;
	}
	strDriveName.Trim();
	if (strDriveName.IsEmpty())
	{
		return -1;
	}

	for (int i = 0; i < static_cast<int>(vecDriveInfos.size()); ++i)
	{
		if (vecDriveInfos[i].strDriveName.CompareNoCase(strDriveName) == 0)
		{
			return vecDriveInfos[i].nUsagePercent;
		}
	}

	return -1;
}

int CDriveManager::GetDriveUsagePercent(LPCTSTR lpszDriveName)
{
	CString strDriveName;
	if (lpszDriveName != nullptr)
	{
		strDriveName = lpszDriveName;
	}
	strDriveName.Trim();
	if (strDriveName.IsEmpty())
	{
		return -1;
	}

	std::vector<CString> vecDriveNames;
	vecDriveNames.push_back(strDriveName);

	CDriveManager driveManager(vecDriveNames);
	driveManager.CheckDriveUsage();

	return FindDriveUsagePercent(driveManager.GetDriveInfos(), strDriveName);
}

CString CDriveManager::BuildDriveRootPath(const CString& strDriveName) const
{
	return AutoMoveFileSystem::BuildDriveRootPath(strDriveName);
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

std::vector<CString> CDriveFileManager::FindFiles(CString strPath)
{
	return LoadTopLevelPaths(strPath);
}

std::vector<CString> CDriveFileManager::FindMoveItems(CString strPath)
{
	return LoadTopLevelPaths(strPath);
}

void CDriveFileManager::RemovePath(const CString& strPath)
{
	CLowImpactDeleteContext deleteContext;
	CString strTargetPath = strPath;
	strTargetPath.Trim();
	if (!strTargetPath.IsEmpty())
	{
		deleteContext.DeletePath(strTargetPath);
	}
}

void CDriveFileManager::MovePath(const CString& strPath, const CString& strDestPath)
{
	MovePathToDirectory(strPath, strDestPath);
}

void CDriveFileManager::RemoveFiles(const std::vector<CString>& vecFilePaths)
{
	CLowImpactDeleteContext deleteContext;
	for (int i = 0; i < static_cast<int>(vecFilePaths.size()); ++i)
	{
		CString strPath = vecFilePaths[i];
		strPath.Trim();
		if (strPath.IsEmpty())
		{
			continue;
		}

		deleteContext.DeletePath(strPath);
	}
}

void CDriveFileManager::MoveFiles(const std::vector<CString>& vecFilePaths, const CString& strDestPath)
{
	for (int i = 0; i < static_cast<int>(vecFilePaths.size()); ++i)
	{
		MovePath(vecFilePaths[i], strDestPath);
	}
}

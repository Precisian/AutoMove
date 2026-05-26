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
		int nDepth = 0;
		BOOL bDirectory = FALSE;
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
		const WIN32_FIND_DATA& findData, int nDepth)
	{
		FILE_SYSTEM_ITEM item;
		item.strPath = CombinePath(strParent, findData.cFileName);
		item.ftCreationTime = findData.ftCreationTime;
		item.nDepth = nDepth;
		item.bDirectory = IsDirectory(findData.dwFileAttributes);
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

	void SortFoldersForCleanup(std::vector<FILE_SYSTEM_ITEM>& vecItems)
	{
		std::sort(vecItems.begin(), vecItems.end(),
			[](const FILE_SYSTEM_ITEM& lhs, const FILE_SYSTEM_ITEM& rhs)
			{
				if (lhs.nDepth != rhs.nDepth)
				{
					return lhs.nDepth > rhs.nDepth;
				}

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

			vecItems.push_back(MakeFileSystemItem(strDirectory, findData, 0));
		} while (find.MoveNext());

		SortByOldestFirst(vecItems);
		return vecItems;
	}

	void AppendFolderCleanupItemsBottomUp(const FILE_SYSTEM_ITEM& rootItem, std::vector<CString>& vecResult)
	{
		std::vector<FILE_SYSTEM_ITEM> vecStack;
		std::vector<FILE_SYSTEM_ITEM> vecFolders;
		vecStack.push_back(rootItem);

		while (!vecStack.empty())
		{
			const FILE_SYSTEM_ITEM currentItem = vecStack.back();
			vecStack.pop_back();
			vecFolders.push_back(currentItem);

			CFindHandle find(BuildSearchPath(currentItem.strPath));
			if (!find.IsValid())
			{
				continue;
			}

			do
			{
				const WIN32_FIND_DATA& findData = find.GetData();
				if (ShouldSkipFindData(findData) || !IsDirectory(findData.dwFileAttributes))
				{
					continue;
				}

				vecStack.push_back(MakeFileSystemItem(currentItem.strPath,
					findData, currentItem.nDepth + 1));
			} while (find.MoveNext());
		}

		SortFoldersForCleanup(vecFolders);

		for (int i = 0; i < static_cast<int>(vecFolders.size()); ++i)
		{
			vecResult.push_back(vecFolders[i].strPath);
		}
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
				DeleteDirectChildFiles(strPath);
				return RemoveDirectoryIfEmpty(strPath);
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

		void DeleteDirectChildFiles(const CString& strDirectory)
		{
			CFindHandle find(BuildSearchPath(strDirectory));
			if (!find.IsValid())
			{
				return;
			}

			do
			{
				const WIN32_FIND_DATA& findData = find.GetData();
				if (ShouldSkipFindData(findData) || IsDirectory(findData.dwFileAttributes))
				{
					continue;
				}

				DeleteFilePath(CombinePath(strDirectory, findData.cFileName));
			} while (find.MoveNext());
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

// Build cleanup work items without keeping every child file path in memory.
// Folder consumers should process direct child files only, then delete the folder if it is empty.
std::vector<CString> CDriveFileManager::FindFiles(CString strPath)
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
		if (vecTopItems[i].bDirectory)
		{
			AppendFolderCleanupItemsBottomUp(vecTopItems[i], vecResult);
		}
		else
		{
			vecResult.push_back(vecTopItems[i].strPath);
		}
	}

	return vecResult;
}

std::vector<CString> CDriveFileManager::FindMoveItems(CString strPath)
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

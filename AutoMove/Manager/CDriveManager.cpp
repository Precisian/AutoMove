#include "pch.h"
#include "CDriveManager.h"
#include "CLogManager.h"
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

	void LogOperationFailure(LPCTSTR lpszOperation, const CString& strPath, DWORD dwError)
	{
		CString strMessage;
		strMessage.Format(_T("%s failed. path=\"%s\", error=%lu"),
			lpszOperation,
			static_cast<LPCTSTR>(strPath),
			dwError);
		CLogManager::Write(CLogManager::LOG_OPERATION, strMessage);
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
		explicit CLowImpactDeleteContext(const CDriveFileManager::WORK_CONTINUE_CALLBACK& continueCallback = {})
			: m_continueCallback(continueCallback)
		{
		}

		BOOL DeletePath(const CString& strPath)
		{
			const DWORD dwAttributes = GetFileAttributes(strPath);
			if (dwAttributes == INVALID_FILE_ATTRIBUTES || IsReparsePoint(dwAttributes))
			{
				const DWORD dwError = dwAttributes == INVALID_FILE_ATTRIBUTES
					? GetLastError()
					: ERROR_CANT_ACCESS_FILE;
				LogOperationFailure(_T("Delete path"), strPath, dwError);
				return FALSE;
			}

			if (IsDirectory(dwAttributes))
			{
				return DeleteDirectoryTree(strPath);
			}

			return DeleteFilePath(strPath);
		}

	private:
		BOOL CheckContinueAfterItem()
		{
			++m_nContinueCheckCount;
			if ((m_nContinueCheckCount % 8) == 0 && m_continueCallback && !m_continueCallback())
			{
				m_bContinue = FALSE;
			}

			return m_bContinue;
		}

		BOOL PauseAfterWork()
		{
			++m_nWorkCount;
			Sleep((m_nWorkCount % 8) == 0 ? 10 : 1);
			return CheckContinueAfterItem();
		}

		BOOL ClearReadOnlyAttribute(const CString& strPath, DWORD dwAttributes)
		{
			if ((dwAttributes & FILE_ATTRIBUTE_READONLY) == 0)
			{
				return TRUE;
			}

			if (!SetFileAttributes(strPath, dwAttributes & ~FILE_ATTRIBUTE_READONLY))
			{
				LogOperationFailure(_T("Clear read-only attribute"), strPath, GetLastError());
				return FALSE;
			}

			return TRUE;
		}

		BOOL DeleteFilePath(const CString& strPath)
		{
			const DWORD dwAttributes = GetFileAttributes(strPath);
			if (dwAttributes == INVALID_FILE_ATTRIBUTES
				|| IsDirectory(dwAttributes)
				|| IsReparsePoint(dwAttributes))
			{
				LogOperationFailure(_T("Delete file"), strPath, GetLastError());
				return FALSE;
			}

			if (!ClearReadOnlyAttribute(strPath, dwAttributes))
			{
				return FALSE;
			}

			const BOOL bDeleted = DeleteFile(strPath);
			if (!bDeleted)
			{
				LogOperationFailure(_T("Delete file"), strPath, GetLastError());
			}
			return PauseAfterWork() && bDeleted;
		}

		BOOL DeleteDirectoryTree(const CString& strRootPath)
		{
			std::vector<CString> vecDirectories;
			std::vector<CString> vecStack;
			vecStack.push_back(strRootPath);
			BOOL bResult = TRUE;

			while (!vecStack.empty() && m_bContinue)
			{
				const CString strDirectory = vecStack.back();
				vecStack.pop_back();
				vecDirectories.push_back(strDirectory);

				CFindHandle find(BuildSearchPath(strDirectory));
				if (!find.IsValid())
				{
					LogOperationFailure(_T("Enumerate delete directory"), strDirectory, GetLastError());
					bResult = FALSE;
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
						CheckContinueAfterItem();
					}
					else
					{
						bResult &= DeleteFilePath(strChildPath);
					}
				} while (find.MoveNext() && m_bContinue);
			}

			std::sort(vecDirectories.begin(), vecDirectories.end(),
				[](const CString& lhs, const CString& rhs)
				{
					return lhs.GetLength() > rhs.GetLength();
				});

			for (int i = 0; i < static_cast<int>(vecDirectories.size()) && m_bContinue; ++i)
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
				LogOperationFailure(_T("Remove directory"), strPath, GetLastError());
				return FALSE;
			}

			if (!ClearReadOnlyAttribute(strPath, dwAttributes))
			{
				return FALSE;
			}

			const BOOL bRemoved = RemoveDirectory(strPath);
			if (!bRemoved)
			{
				LogOperationFailure(_T("Remove directory"), strPath, GetLastError());
			}
			return PauseAfterWork() && bRemoved;
		}

		CScopedBackgroundThreadPriority m_backgroundPriority;
		CDriveFileManager::WORK_CONTINUE_CALLBACK m_continueCallback;
		int m_nWorkCount = 0;
		int m_nContinueCheckCount = 0;
		BOOL m_bContinue = TRUE;
	};

	BOOL MoveSinglePathToDirectory(CString strPath, CString strDestPath)
	{
		strPath.Trim();
		strDestPath = NormalizeDirectoryPath(strDestPath);
		if (strPath.IsEmpty() || strDestPath.IsEmpty())
		{
			LogOperationFailure(_T("Prepare move path"), strPath, ERROR_INVALID_PARAMETER);
			return FALSE;
		}

		const DWORD dwAttributes = GetFileAttributes(strPath);
		if (dwAttributes == INVALID_FILE_ATTRIBUTES || IsReparsePoint(dwAttributes))
		{
			const DWORD dwError = dwAttributes == INVALID_FILE_ATTRIBUTES
				? GetLastError()
				: ERROR_CANT_ACCESS_FILE;
			LogOperationFailure(_T("Move path"), strPath, dwError);
			return FALSE;
		}

		if (!EnsureDirectoryExists(strDestPath))
		{
			LogOperationFailure(_T("Create move destination"), strDestPath, GetLastError());
			return FALSE;
		}

		const CString strFileName = GetFileNameFromPath(strPath);
		if (strFileName.IsEmpty())
		{
			LogOperationFailure(_T("Prepare move path"), strPath, ERROR_INVALID_NAME);
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

		const int nResult = SHFileOperation(&fileOp);
		if (nResult != 0 || fileOp.fAnyOperationsAborted)
		{
			const DWORD dwError = nResult != 0 ? static_cast<DWORD>(nResult) : ERROR_CANCELLED;
			LogOperationFailure(_T("Move path"), strPath, dwError);
			return FALSE;
		}

		return TRUE;
	}

	class CLowImpactMoveContext
	{
	public:
		explicit CLowImpactMoveContext(const CDriveFileManager::WORK_CONTINUE_CALLBACK& continueCallback = {})
			: m_continueCallback(continueCallback)
		{
		}

		BOOL MovePath(const CString& strPath, const CString& strDestPath)
		{
			const DWORD dwAttributes = GetFileAttributes(strPath);
			if (dwAttributes == INVALID_FILE_ATTRIBUTES || IsReparsePoint(dwAttributes))
			{
				const DWORD dwError = dwAttributes == INVALID_FILE_ATTRIBUTES
					? GetLastError()
					: ERROR_CANT_ACCESS_FILE;
				LogOperationFailure(_T("Move path"), strPath, dwError);
				return FALSE;
			}

			if (!IsDirectory(dwAttributes))
			{
				return MoveFilePath(strPath, strDestPath);
			}

			const CString strDirectoryName = GetFileNameFromPath(strPath);
			const CString strTargetRoot = CombinePath(strDestPath, strDirectoryName);
			if (!EnsureDirectoryExists(strTargetRoot))
			{
				LogOperationFailure(_T("Create move destination"), strTargetRoot, GetLastError());
				return FALSE;
			}

			return MoveDirectoryTree(strPath, strTargetRoot);
		}

	private:
		BOOL CheckContinueAfterItem()
		{
			++m_nContinueCheckCount;
			if ((m_nContinueCheckCount % 8) == 0 && m_continueCallback && !m_continueCallback())
			{
				m_bContinue = FALSE;
			}

			return m_bContinue;
		}

		BOOL PauseAfterWork()
		{
			++m_nWorkCount;
			Sleep((m_nWorkCount % 8) == 0 ? 10 : 1);
			return CheckContinueAfterItem();
		}

		BOOL MoveFilePath(const CString& strPath, const CString& strDestPath)
		{
			const BOOL bMoved = MoveSinglePathToDirectory(strPath, strDestPath);
			return PauseAfterWork() && bMoved;
		}

		BOOL MoveDirectoryTree(const CString& strSourceRoot, const CString& strTargetRoot)
		{
			std::vector<CString> vecSourceDirectories;
			std::vector<CString> vecTargetDirectories;
			vecSourceDirectories.push_back(strSourceRoot);
			vecTargetDirectories.push_back(strTargetRoot);
			BOOL bResult = TRUE;

			for (int i = 0; i < static_cast<int>(vecSourceDirectories.size()) && m_bContinue; ++i)
			{
				const CString strSourceDirectory = vecSourceDirectories[i];
				const CString strTargetDirectory = vecTargetDirectories[i];

				CFindHandle find(BuildSearchPath(strSourceDirectory));
				if (!find.IsValid())
				{
					LogOperationFailure(_T("Enumerate move directory"), strSourceDirectory, GetLastError());
					bResult = FALSE;
					continue;
				}

				do
				{
					const WIN32_FIND_DATA& findData = find.GetData();
					if (ShouldSkipFindData(findData))
					{
						continue;
					}

					const CString strSourceChild = CombinePath(strSourceDirectory, findData.cFileName);
					if (IsDirectory(findData.dwFileAttributes))
					{
						const CString strTargetChild = CombinePath(strTargetDirectory, findData.cFileName);
						if (!EnsureDirectoryExists(strTargetChild))
						{
							LogOperationFailure(_T("Create move destination"), strTargetChild, GetLastError());
							bResult = FALSE;
							continue;
						}

						vecSourceDirectories.push_back(strSourceChild);
						vecTargetDirectories.push_back(strTargetChild);
						CheckContinueAfterItem();
					}
					else
					{
						bResult &= MoveFilePath(strSourceChild, strTargetDirectory);
					}
				} while (find.MoveNext() && m_bContinue);
			}

			for (int i = static_cast<int>(vecSourceDirectories.size()) - 1; i >= 0 && m_bContinue; --i)
			{
				const BOOL bRemoved = RemoveDirectory(vecSourceDirectories[i]);
				if (!bRemoved)
				{
					LogOperationFailure(_T("Remove moved source directory"), vecSourceDirectories[i], GetLastError());
				}
				bResult &= PauseAfterWork() && bRemoved;
			}

			return bResult;
		}

		CScopedBackgroundThreadPriority m_backgroundPriority;
		CDriveFileManager::WORK_CONTINUE_CALLBACK m_continueCallback;
		int m_nWorkCount = 0;
		int m_nContinueCheckCount = 0;
		BOOL m_bContinue = TRUE;
	};

	BOOL MovePathToDirectory(CString strPath, CString strDestPath,
		const CDriveFileManager::WORK_CONTINUE_CALLBACK& continueCallback = {})
	{
		strPath.Trim();
		strDestPath = NormalizeDirectoryPath(strDestPath);
		if (strPath.IsEmpty() || strDestPath.IsEmpty() || !EnsureDirectoryExists(strDestPath))
		{
			const DWORD dwError = strPath.IsEmpty() || strDestPath.IsEmpty()
				? ERROR_INVALID_PARAMETER
				: GetLastError();
			LogOperationFailure(_T("Prepare move path"), strPath, dwError);
			return FALSE;
		}

		CLowImpactMoveContext moveContext(continueCallback);
		return moveContext.MovePath(strPath, strDestPath);
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

BOOL CDriveFileManager::RemovePath(const CString& strPath)
{
	return RemovePath(strPath, {});
}

BOOL CDriveFileManager::RemovePath(const CString& strPath, const WORK_CONTINUE_CALLBACK& continueCallback)
{
	CLowImpactDeleteContext deleteContext(continueCallback);
	CString strTargetPath = strPath;
	strTargetPath.Trim();
	return !strTargetPath.IsEmpty() && deleteContext.DeletePath(strTargetPath);
}

BOOL CDriveFileManager::MovePath(const CString& strPath, const CString& strDestPath)
{
	return MovePath(strPath, strDestPath, {});
}

BOOL CDriveFileManager::MovePath(const CString& strPath, const CString& strDestPath,
	const WORK_CONTINUE_CALLBACK& continueCallback)
{
	return MovePathToDirectory(strPath, strDestPath, continueCallback);
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

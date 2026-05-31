#pragma once
#include "../DriveInfo.h"
#include <functional>
#include <vector>

class CDriveManager
{
public:
	CDriveManager() = default;
	explicit CDriveManager(const std::vector<CString>& vecDriveNames);

	void LoadAvailableDrives();
	void CheckDriveUsage();
	std::vector<CString> GetDriveNames() const;
	const std::vector<DRIVE_INFO>& GetDriveInfos() const;
	static int FindDriveUsagePercent(const std::vector<DRIVE_INFO>& vecDriveInfos, LPCTSTR lpszDriveName);
	static int GetDriveUsagePercent(LPCTSTR lpszDriveName);

private:
	CString BuildDriveRootPath(const CString& strDriveName) const;
	int CalculateUsagePercent(const ULARGE_INTEGER& totalBytes, const ULARGE_INTEGER& totalFreeBytes) const;

	std::vector<DRIVE_INFO> m_vecDriveInfos;
};

class CDriveFileManager
{
public:
	using WORK_CONTINUE_CALLBACK = std::function<BOOL()>;

	// Returns cleanup work items ordered oldest-first. Folder items are bottom-up.
	std::vector<CString> FindFiles(CString strPath);
	std::vector<CString> FindMoveItems(CString strPath);
	BOOL RemovePath(const CString& strPath);
	BOOL RemovePath(const CString& strPath, const WORK_CONTINUE_CALLBACK& continueCallback);
	BOOL MovePath(const CString& strPath, const CString& strDestPath);
	BOOL MovePath(const CString& strPath, const CString& strDestPath, const WORK_CONTINUE_CALLBACK& continueCallback);
	void RemoveFiles(const std::vector<CString>& vecFilePaths);
	void MoveFiles(const std::vector<CString>& vecFilePaths, const CString& strDestPath);
};

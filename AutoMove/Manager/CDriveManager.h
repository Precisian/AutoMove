#pragma once
#include "../DriveInfo.h"
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

	// Returns cleanup work items ordered oldest-first. Folder items are bottom-up.
	std::vector<CString> FindFiles(CString strPath);
	std::vector<CString> FindMoveItems(CString strPath);
	void RemoveFiles(const std::vector<CString>& vecFilePaths);
	void MoveFiles(const std::vector<CString>& vecFilePaths, const CString& strDestPath);

private:
	CString BuildDriveRootPath(const CString& strDriveName) const;
	int CalculateUsagePercent(const ULARGE_INTEGER& totalBytes, const ULARGE_INTEGER& totalFreeBytes) const;

	std::vector<DRIVE_INFO> m_vecDriveInfos;
};

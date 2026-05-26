#pragma once

namespace AutoMoveFileSystem
{
	CString NormalizeDirectoryPath(CString strPath);
	CString CombinePath(const CString& strParent, const CString& strName);
	CString BuildSearchPath(const CString& strDirectory);
	CString GetFileNameFromPath(CString strPath);
	CString BuildDriveRootPath(const CString& strDriveName);
	BOOL IsDots(const CString& strName);
	BOOL IsDirectory(DWORD dwAttributes);
	BOOL IsReparsePoint(DWORD dwAttributes);
	BOOL IsReparsePoint(const WIN32_FIND_DATA& findData);
	BOOL IsValidDirectoryPath(const CString& strPath);
	BOOL EnsureDirectoryExists(const CString& strDirectory);
}

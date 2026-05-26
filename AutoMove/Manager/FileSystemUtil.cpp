#include "pch.h"
#include "FileSystemUtil.h"

namespace AutoMoveFileSystem
{
	namespace
	{
		CString GetFullDirectoryPath(CString strPath)
		{
			strPath = NormalizeDirectoryPath(strPath);
			if (strPath.IsEmpty())
			{
				return strPath;
			}

			TCHAR szFullPath[MAX_PATH] = {};
			if (GetFullPathName(strPath, MAX_PATH, szFullPath, nullptr) == 0)
			{
				return strPath;
			}

			return NormalizeDirectoryPath(szFullPath);
		}

		CString GetDriveRootFromPath(const CString& strPath)
		{
			CString strFullPath = GetFullDirectoryPath(strPath);
			if (strFullPath.GetLength() < 2 || strFullPath[1] != _T(':'))
			{
				return _T("");
			}

			CString strRoot;
			strRoot.Format(_T("%c:\\"), static_cast<TCHAR>(_totupper(strFullPath[0])));
			return strRoot;
		}

		BOOL IsDriveRootPath(const CString& strPath)
		{
			const CString strFullPath = GetFullDirectoryPath(strPath);
			return strFullPath.GetLength() == 3
				&& strFullPath[1] == _T(':')
				&& (strFullPath[2] == _T('\\') || strFullPath[2] == _T('/'));
		}

		BOOL IsSamePath(const CString& strLeftPath, const CString& strRightPath)
		{
			return GetFullDirectoryPath(strLeftPath).CompareNoCase(GetFullDirectoryPath(strRightPath)) == 0;
		}

		BOOL IsKnownProtectedRoot(const CString& strPath)
		{
			TCHAR szPath[MAX_PATH] = {};
			if (GetWindowsDirectory(szPath, MAX_PATH) > 0 && IsSamePath(strPath, szPath))
			{
				return TRUE;
			}

			if (GetEnvironmentVariable(_T("ProgramFiles"), szPath, MAX_PATH) > 0 && IsSamePath(strPath, szPath))
			{
				return TRUE;
			}

			if (GetEnvironmentVariable(_T("ProgramFiles(x86)"), szPath, MAX_PATH) > 0 && IsSamePath(strPath, szPath))
			{
				return TRUE;
			}

			if (GetEnvironmentVariable(_T("ProgramData"), szPath, MAX_PATH) > 0 && IsSamePath(strPath, szPath))
			{
				return TRUE;
			}

			if (GetEnvironmentVariable(_T("USERPROFILE"), szPath, MAX_PATH) > 0 && IsSamePath(strPath, szPath))
			{
				return TRUE;
			}

			return FALSE;
		}
	}

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

	CString GetFileNameFromPath(CString strPath)
	{
		strPath = NormalizeDirectoryPath(strPath);
		const int nBackslash = strPath.ReverseFind(_T('\\'));
		const int nSlash = strPath.ReverseFind(_T('/'));
		const int nSeparator = max(nBackslash, nSlash);
		if (nSeparator < 0)
		{
			return strPath;
		}

		return strPath.Mid(nSeparator + 1);
	}

	CString BuildDriveRootPath(const CString& strDriveName)
	{
		CString strRoot = strDriveName;
		strRoot.Trim();
		strRoot.TrimRight(_T(":\\"));

		if (strRoot.GetLength() == 1)
		{
			strRoot.Format(_T("%c:\\"), strRoot[0]);
		}

		return strRoot;
	}

	BOOL IsDots(const CString& strName)
	{
		return strName == _T(".") || strName == _T("..");
	}

	BOOL IsDirectory(DWORD dwAttributes)
	{
		return (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	BOOL IsReparsePoint(DWORD dwAttributes)
	{
		return (dwAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
	}

	BOOL IsReparsePoint(const WIN32_FIND_DATA& findData)
	{
		return IsReparsePoint(findData.dwFileAttributes);
	}

	BOOL IsValidDirectoryPath(const CString& strPath)
	{
		const DWORD dwAttributes = GetFileAttributes(strPath);
		return dwAttributes != INVALID_FILE_ATTRIBUTES
			&& IsDirectory(dwAttributes)
			&& !IsReparsePoint(dwAttributes);
	}

	BOOL IsFixedDrivePath(const CString& strPath)
	{
		const CString strRoot = GetDriveRootFromPath(strPath);
		return !strRoot.IsEmpty() && GetDriveType(strRoot) == DRIVE_FIXED;
	}

	BOOL IsSafeWorkRoot(const CString& strPath)
	{
		CString strFullPath = GetFullDirectoryPath(strPath);
		if (strFullPath.IsEmpty()
			|| !IsValidDirectoryPath(strFullPath)
			|| !IsFixedDrivePath(strFullPath)
			|| IsDriveRootPath(strFullPath)
			|| IsKnownProtectedRoot(strFullPath))
		{
			return FALSE;
		}

		return TRUE;
	}

	BOOL IsSameOrChildPath(const CString& strParentPath, const CString& strChildPath)
	{
		CString strParent = GetFullDirectoryPath(strParentPath);
		CString strChild = GetFullDirectoryPath(strChildPath);
		if (strParent.IsEmpty() || strChild.IsEmpty())
		{
			return FALSE;
		}

		if (strParent.CompareNoCase(strChild) == 0)
		{
			return TRUE;
		}

		if (strParent.Right(1) != _T("\\") && strParent.Right(1) != _T("/"))
		{
			strParent += _T("\\");
		}

		return strChild.GetLength() > strParent.GetLength()
			&& strChild.Left(strParent.GetLength()).CompareNoCase(strParent) == 0;
	}

	BOOL EnsureDirectoryExists(const CString& strDirectory)
	{
		CString strPath = NormalizeDirectoryPath(strDirectory);
		if (strPath.IsEmpty())
		{
			return FALSE;
		}

		if (IsValidDirectoryPath(strPath))
		{
			return TRUE;
		}

		const int nBackslash = strPath.ReverseFind(_T('\\'));
		const int nSlash = strPath.ReverseFind(_T('/'));
		const int nSeparator = max(nBackslash, nSlash);
		if (nSeparator > 2)
		{
			const CString strParent = strPath.Left(nSeparator);
			if (!EnsureDirectoryExists(strParent))
			{
				return FALSE;
			}
		}

		if (CreateDirectory(strPath, nullptr))
		{
			return TRUE;
		}

		return GetLastError() == ERROR_ALREADY_EXISTS && IsValidDirectoryPath(strPath);
	}
}

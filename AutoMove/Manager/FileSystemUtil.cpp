#include "pch.h"
#include "FileSystemUtil.h"

namespace AutoMoveFileSystem
{
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

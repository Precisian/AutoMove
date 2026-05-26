#include "pch.h"
#include "CParameter.h"

namespace
{
	constexpr DWORD PROFILE_BUFFER_SIZE = 32767;

	struct PROFILE_ENTRY
	{
		CString strKey;
		CString strValue;
	};

	bool SplitProfileEntry(const CString& strLine, PROFILE_ENTRY& entry)
	{
		const int nEqual = strLine.Find(_T('='));
		if (nEqual <= 0)
		{
			return false;
		}

		entry.strKey = strLine.Left(nEqual);
		entry.strValue = strLine.Mid(nEqual + 1);
		return true;
	}

	std::vector<PROFILE_ENTRY> ReadProfileSection(LPCTSTR lpszSection, LPCTSTR lpszIniPath)
	{
		CString strSectionData;
		LPTSTR lpszBuffer = strSectionData.GetBuffer(PROFILE_BUFFER_SIZE);

		GetPrivateProfileSection(lpszSection, lpszBuffer, PROFILE_BUFFER_SIZE, lpszIniPath);
		strSectionData.ReleaseBuffer();

		std::vector<PROFILE_ENTRY> vecEntry;
		LPCTSTR lpszItem = strSectionData;
		while (*lpszItem != _T('\0'))
		{
			PROFILE_ENTRY entry;
			if (SplitProfileEntry(CString(lpszItem), entry))
			{
				vecEntry.push_back(entry);
			}

			lpszItem += _tcslen(lpszItem) + 1;
		}

		return vecEntry;
	}

	CString Trimmed(CString strValue)
	{
		strValue.Trim();
		return strValue;
	}

	BOOL IsTrueValue(const CString& strValue)
	{
		return Trimmed(strValue) == _T("1");
	}
}

CParameter::CParameter()
	: m_strIniPath(DEFAULT_INI_PATH)
{
	InitDefault();
}

void CParameter::InitDefault()
{
	m_vecTemplate.clear();
}

BOOL CParameter::Load()
{
	const BOOL bIniExists = PathFileExists(m_strIniPath);

	InitDefault();
	if (!bIniExists)
	{
		return Save();
	}

	BOOL bResult = TRUE;
	bResult &= LoadTemplate();

	return bResult;
}

BOOL CParameter::Save()
{
	BOOL bResult = TRUE;
	WritePrivateProfileString(_T("System"), nullptr, nullptr, m_strIniPath);
	bResult &= SaveTemplate();
	if (bResult)
	{
		bResult &= FormatIniFile();
	}

	return bResult;
}

void CParameter::AddTemplate(LPCTSTR lpszName)
{
	if (FindTemplate(lpszName) != nullptr)
	{
		return;
	}

	PARAM_TEMPLATE paramTemplate;
	paramTemplate.strName = lpszName;
	m_vecTemplate.push_back(paramTemplate);
}

void CParameter::AddTemplateParam(LPCTSTR lpszName, LPCTSTR lpszKey, LPCTSTR lpszValue)
{
	PARAM_TEMPLATE* pTemplate = FindTemplate(lpszName);
	if (pTemplate == nullptr)
	{
		AddTemplate(lpszName);
		pTemplate = FindTemplate(lpszName);
	}

	if (pTemplate == nullptr)
	{
		return;
	}

	SetTemplateValue(*pTemplate, lpszKey, lpszValue);
}

void CParameter::ClearTemplate()
{
	m_vecTemplate.clear();
}

void CParameter::SetIniPath(LPCTSTR lpszIniPath)
{
	m_strIniPath = lpszIniPath;
}

CString CParameter::GetIniPath() const
{
	return m_strIniPath;
}

CString CParameter::GetTemplateValue(const PARAM_TEMPLATE& paramTemplate,
	LPCTSTR lpszKey, LPCTSTR lpszDefault)
{
	if (lpszKey == nullptr)
	{
		return lpszDefault;
	}

	if (_tcscmp(lpszKey, TemplateKey::NAME) == 0)
	{
		return Trimmed(paramTemplate.strName);
	}
	if (_tcscmp(lpszKey, TemplateKey::ORIGIN_PATH) == 0)
	{
		return Trimmed(paramTemplate.strOriginPath);
	}
	if (_tcscmp(lpszKey, TemplateKey::DEST_PATH) == 0)
	{
		return Trimmed(paramTemplate.strDestPath);
	}
	if (_tcscmp(lpszKey, TemplateKey::ENABLE_MOVE) == 0)
	{
		return paramTemplate.bEnableMove ? _T("1") : _T("0");
	}
	if (_tcscmp(lpszKey, TemplateKey::BOOT_START) == 0)
	{
		return paramTemplate.bBootStart ? _T("1") : _T("0");
	}
	if (_tcscmp(lpszKey, TemplateKey::DRIVE_NAME) == 0)
	{
		return Trimmed(paramTemplate.strDriveName);
	}
	if (_tcscmp(lpszKey, TemplateKey::LIMIT_MODE) == 0)
	{
		return paramTemplate.strLimitMode.IsEmpty() ? lpszDefault : Trimmed(paramTemplate.strLimitMode);
	}
	if (_tcscmp(lpszKey, TemplateKey::LIMIT_VALUE) == 0)
	{
		return Trimmed(paramTemplate.strLimitValue);
	}
	if (_tcscmp(lpszKey, TemplateKey::END_VALUE) == 0)
	{
		return Trimmed(paramTemplate.strEndValue);
	}
	if (_tcscmp(lpszKey, TemplateKey::SCHEDULE_DAYS) == 0)
	{
		return Trimmed(paramTemplate.strScheduleDays);
	}
	if (_tcscmp(lpszKey, TemplateKey::SCHEDULE_TIME) == 0)
	{
		return Trimmed(paramTemplate.strScheduleTime);
	}

	return lpszDefault;
}

void CParameter::SetTemplateValue(PARAM_TEMPLATE& paramTemplate,
	LPCTSTR lpszKey, LPCTSTR lpszValue)
{
	CString strValue;
	if (lpszValue != nullptr)
	{
		strValue = lpszValue;
	}
	strValue.Trim();

	if (lpszKey == nullptr)
	{
		return;
	}

	if (_tcscmp(lpszKey, TemplateKey::NAME) == 0)
	{
		paramTemplate.strName = strValue;
	}
	else if (_tcscmp(lpszKey, TemplateKey::ORIGIN_PATH) == 0)
	{
		paramTemplate.strOriginPath = strValue;
	}
	else if (_tcscmp(lpszKey, TemplateKey::DEST_PATH) == 0)
	{
		paramTemplate.strDestPath = strValue;
	}
	else if (_tcscmp(lpszKey, TemplateKey::ENABLE_MOVE) == 0)
	{
		paramTemplate.bEnableMove = IsTrueValue(strValue);
	}
	else if (_tcscmp(lpszKey, TemplateKey::BOOT_START) == 0)
	{
		paramTemplate.bBootStart = IsTrueValue(strValue);
	}
	else if (_tcscmp(lpszKey, TemplateKey::DRIVE_NAME) == 0)
	{
		paramTemplate.strDriveName = strValue;
	}
	else if (_tcscmp(lpszKey, TemplateKey::LIMIT_MODE) == 0)
	{
		paramTemplate.strLimitMode = strValue.IsEmpty() ? TemplateKey::LIMIT_MODE_STORAGE : strValue;
	}
	else if (_tcscmp(lpszKey, TemplateKey::LIMIT_VALUE) == 0)
	{
		paramTemplate.strLimitValue = strValue;
	}
	else if (_tcscmp(lpszKey, TemplateKey::END_VALUE) == 0)
	{
		paramTemplate.strEndValue = strValue;
	}
	else if (_tcscmp(lpszKey, TemplateKey::SCHEDULE_DAYS) == 0)
	{
		paramTemplate.strScheduleDays = strValue;
	}
	else if (_tcscmp(lpszKey, TemplateKey::SCHEDULE_TIME) == 0)
	{
		paramTemplate.strScheduleTime = strValue;
	}
}

void CParameter::AddTemplateValue(PARAM_TEMPLATE& paramTemplate,
	LPCTSTR lpszKey, const CString& strValue)
{
	SetTemplateValue(paramTemplate, lpszKey, strValue);
}

BOOL CParameter::IsScheduleLimitMode(const PARAM_TEMPLATE& paramTemplate)
{
	return GetTemplateValue(paramTemplate, TemplateKey::LIMIT_MODE) == TemplateKey::LIMIT_MODE_SCHEDULE;
}

BOOL CParameter::LoadTemplate()
{
	m_vecTemplate.clear();

	std::vector<CString> vecTemplateNames;
	LoadTemplateNames(vecTemplateNames);

	for (int i = 0; i < static_cast<int>(vecTemplateNames.size()); ++i)
	{
		PARAM_TEMPLATE paramTemplate;
		paramTemplate.strName = vecTemplateNames[i];

		const std::vector<PROFILE_ENTRY> vecEntry = ReadProfileSection(GetTemplateSection(paramTemplate.strName), m_strIniPath);
		for (int j = 0; j < static_cast<int>(vecEntry.size()); ++j)
		{
			SetTemplateValue(paramTemplate, vecEntry[j].strKey, vecEntry[j].strValue);
		}

		m_vecTemplate.push_back(paramTemplate);
	}

	return TRUE;
}

void CParameter::LoadTemplateNames(std::vector<CString>& vecTemplateNames) const
{
	vecTemplateNames.clear();

	const std::vector<PROFILE_ENTRY> vecEntry = ReadProfileSection(TEMPLATE_SECTION, m_strIniPath);
	for (int i = 0; i < static_cast<int>(vecEntry.size()); ++i)
	{
		vecTemplateNames.push_back(vecEntry[i].strKey);
	}
}

BOOL CParameter::SaveTemplate()
{
	std::vector<CString> vecOldTemplateNames;
	LoadTemplateNames(vecOldTemplateNames);

	if (!WritePrivateProfileString(TEMPLATE_SECTION, nullptr, nullptr, m_strIniPath))
	{
		return FALSE;
	}

	for (int i = 0; i < static_cast<int>(m_vecTemplate.size()); ++i)
	{
		if (!WriteString(TEMPLATE_SECTION, m_vecTemplate[i].strName, _T("1")))
		{
			return FALSE;
		}

		const CString strTemplateSection = GetTemplateSection(m_vecTemplate[i].strName);

		if (!WritePrivateProfileString(strTemplateSection, nullptr, nullptr, m_strIniPath))
		{
			return FALSE;
		}

		const PARAM_TEMPLATE& paramTemplate = m_vecTemplate[i];
		if (!WriteString(strTemplateSection, TemplateKey::NAME, paramTemplate.strName)
			|| !WriteString(strTemplateSection, TemplateKey::ORIGIN_PATH, paramTemplate.strOriginPath)
			|| !WriteString(strTemplateSection, TemplateKey::DEST_PATH, paramTemplate.strDestPath)
			|| !WriteString(strTemplateSection, TemplateKey::ENABLE_MOVE, paramTemplate.bEnableMove ? _T("1") : _T("0"))
			|| !WriteString(strTemplateSection, TemplateKey::BOOT_START, paramTemplate.bBootStart ? _T("1") : _T("0"))
			|| !WriteString(strTemplateSection, TemplateKey::DRIVE_NAME, paramTemplate.strDriveName)
			|| !WriteString(strTemplateSection, TemplateKey::LIMIT_MODE, paramTemplate.strLimitMode)
			|| !WriteString(strTemplateSection, TemplateKey::LIMIT_VALUE, paramTemplate.strLimitValue)
			|| !WriteString(strTemplateSection, TemplateKey::END_VALUE, paramTemplate.strEndValue)
			|| !WriteString(strTemplateSection, TemplateKey::SCHEDULE_DAYS, paramTemplate.strScheduleDays)
			|| !WriteString(strTemplateSection, TemplateKey::SCHEDULE_TIME, paramTemplate.strScheduleTime))
		{
			return FALSE;
		}
	}

	for (int i = 0; i < static_cast<int>(vecOldTemplateNames.size()); ++i)
	{
		if (FindTemplate(vecOldTemplateNames[i]) == nullptr)
		{
			if (!DeleteTemplateSection(vecOldTemplateNames[i]))
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

BOOL CParameter::DeleteTemplateSection(LPCTSTR lpszName) const
{
	return WritePrivateProfileString(GetTemplateSection(lpszName), nullptr, nullptr, m_strIniPath);
}

BOOL CParameter::FormatIniFile() const
{
	CFile file;
	if (!file.Open(m_strIniPath, CFile::modeRead | CFile::typeBinary))
	{
		return FALSE;
	}

	const ULONGLONG ullLength = file.GetLength();
	if (ullLength == 0)
	{
		file.Close();
		return TRUE;
	}

	std::vector<BYTE> vecInput(static_cast<size_t>(ullLength));
	file.Read(vecInput.data(), static_cast<UINT>(vecInput.size()));
	file.Close();

	std::vector<BYTE> vecOutput;
	vecOutput.reserve(vecInput.size() + 256);

	bool bHasContentBefore = false;

	size_t nPos = 0;
	while (nPos < vecInput.size())
	{
		size_t nLineEnd = nPos;
		while (nLineEnd < vecInput.size()
			&& vecInput[nLineEnd] != '\r'
			&& vecInput[nLineEnd] != '\n')
		{
			++nLineEnd;
		}

		size_t nFirstText = nPos;
		while (nFirstText < nLineEnd
			&& (vecInput[nFirstText] == ' ' || vecInput[nFirstText] == '\t'))
		{
			++nFirstText;
		}

		const bool bBlankLine = nFirstText == nLineEnd;
		const bool bSectionLine = !bBlankLine && vecInput[nFirstText] == '[';

		if (bBlankLine)
		{
			if (nLineEnd < vecInput.size() && vecInput[nLineEnd] == '\r')
			{
				++nLineEnd;
			}

			if (nLineEnd < vecInput.size() && vecInput[nLineEnd] == '\n')
			{
				++nLineEnd;
			}

			nPos = nLineEnd;
			continue;
		}

		if (bSectionLine && bHasContentBefore)
		{
			vecOutput.push_back('\r');
			vecOutput.push_back('\n');
		}

		for (size_t i = nPos; i < nLineEnd; ++i)
		{
			vecOutput.push_back(vecInput[i]);
		}

		vecOutput.push_back('\r');
		vecOutput.push_back('\n');

		bHasContentBefore = true;

		if (nLineEnd < vecInput.size() && vecInput[nLineEnd] == '\r')
		{
			++nLineEnd;
		}

		if (nLineEnd < vecInput.size() && vecInput[nLineEnd] == '\n')
		{
			++nLineEnd;
		}

		nPos = nLineEnd;
	}

	if (!file.Open(m_strIniPath, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary))
	{
		return FALSE;
	}

	file.Write(vecOutput.data(), static_cast<UINT>(vecOutput.size()));
	file.Close();

	return TRUE;
}

CParameter::PARAM_TEMPLATE* CParameter::FindTemplate(LPCTSTR lpszName)
{
	for (int i = 0; i < static_cast<int>(m_vecTemplate.size()); ++i)
	{
		if (m_vecTemplate[i].strName == lpszName)
		{
			return &m_vecTemplate[i];
		}
	}

	return nullptr;
}

CString CParameter::GetTemplateSection(LPCTSTR lpszName) const
{
	CString strSection;
	strSection.Format(_T("%s.%s"), TEMPLATE_SECTION, lpszName);
	return strSection;
}

BOOL CParameter::WriteString(LPCTSTR lpszSection, LPCTSTR lpszKey, LPCTSTR lpszValue) const
{
	return WritePrivateProfileString(lpszSection, lpszKey, lpszValue, m_strIniPath);
}

#include "pch.h"
#include "CParameter.h"

CParameter::CParameter()
	: m_strIniPath(DEFAULT_INI_PATH)
	, m_eMode(PARAM_LOAD)
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

	m_eMode = PARAM_LOAD;

	BOOL bResult = TRUE;
	bResult &= LoadTemplate();

	return bResult;
}

BOOL CParameter::Save()
{
	m_eMode = PARAM_SAVE;

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
	for (int i = 0; i < static_cast<int>(paramTemplate.vecValue.size()); ++i)
	{
		if (paramTemplate.vecValue[i].strKey == lpszKey)
		{
			CString strValue = paramTemplate.vecValue[i].strValue;
			strValue.Trim();
			return strValue;
		}
	}

	return lpszDefault;
}

void CParameter::SetTemplateValue(PARAM_TEMPLATE& paramTemplate,
	LPCTSTR lpszKey, LPCTSTR lpszValue)
{
	for (int i = 0; i < static_cast<int>(paramTemplate.vecValue.size()); ++i)
	{
		if (paramTemplate.vecValue[i].strKey == lpszKey)
		{
			paramTemplate.vecValue[i].strValue = lpszValue;
			return;
		}
	}

	AddTemplateValue(paramTemplate, lpszKey, lpszValue);
}

void CParameter::AddTemplateValue(PARAM_TEMPLATE& paramTemplate,
	LPCTSTR lpszKey, const CString& strValue)
{
	PARAM_TEMPLATE_VALUE value;
	value.strKey = lpszKey;
	value.strValue = strValue;
	paramTemplate.vecValue.push_back(value);
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
		const CString strTemplateSection = GetTemplateSection(paramTemplate.strName);

		const DWORD dwBufferSize = 32767;
		CString strTemplateData;
		LPTSTR lpszTemplateBuffer = strTemplateData.GetBuffer(dwBufferSize);

		GetPrivateProfileSection(strTemplateSection, lpszTemplateBuffer, dwBufferSize, m_strIniPath);
		strTemplateData.ReleaseBuffer();

		LPCTSTR lpszTemplateItem = strTemplateData;
		while (*lpszTemplateItem != _T('\0'))
		{
			CString strTemplateItem(lpszTemplateItem);
			const int nTemplateEqual = strTemplateItem.Find(_T('='));

			if (nTemplateEqual > 0)
			{
				PARAM_TEMPLATE_VALUE value;
				value.strKey = strTemplateItem.Left(nTemplateEqual);
				value.strValue = strTemplateItem.Mid(nTemplateEqual + 1);
				paramTemplate.vecValue.push_back(value);
			}

			lpszTemplateItem += _tcslen(lpszTemplateItem) + 1;
		}

		m_vecTemplate.push_back(paramTemplate);
	}

	return TRUE;
}

void CParameter::LoadTemplateNames(std::vector<CString>& vecTemplateNames) const
{
	vecTemplateNames.clear();

	const DWORD dwBufferSize = 32767;
	CString strSection;
	LPTSTR lpszBuffer = strSection.GetBuffer(dwBufferSize);

	GetPrivateProfileSection(TEMPLATE_SECTION, lpszBuffer, dwBufferSize, m_strIniPath);
	strSection.ReleaseBuffer();

	LPCTSTR lpszItem = strSection;
	while (*lpszItem != _T('\0'))
	{
		CString strItem(lpszItem);
		const int nEqual = strItem.Find(_T('='));

		if (nEqual > 0)
		{
			vecTemplateNames.push_back(strItem.Left(nEqual));
		}

		lpszItem += _tcslen(lpszItem) + 1;
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

		for (int j = 0; j < static_cast<int>(m_vecTemplate[i].vecValue.size()); ++j)
		{
			if (!WriteString(strTemplateSection, m_vecTemplate[i].vecValue[j].strKey, m_vecTemplate[i].vecValue[j].strValue))
			{
				return FALSE;
			}
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

#include "pch.h"
#include "CParameter.h"

CParameter::CParameter()
	: m_strIniPath(DEFAULT_INI_PATH)
	, m_eMode(PARAM_LOAD)
{
	InitDefault();
	Load();
}

void CParameter::InitDefault()
{
	// Set initial parameter values here.
	// m_nMoveDelay = 100;
	// m_strSourcePath = _T("");
	// m_bUseAutoMove = TRUE;
	m_bUseBootAutoStart = false;

	m_vecTemplate.clear();

	Save();
}

BOOL CParameter::Load()
{
	InitDefault();
	m_eMode = PARAM_LOAD;

	BOOL bResult = Add();
	bResult &= LoadTemplate();

	return bResult;
}

BOOL CParameter::Save()
{
	m_eMode = PARAM_SAVE;

	BOOL bResult = Add();
	bResult &= SaveTemplate();

	return bResult;
}

BOOL CParameter::Add()
{
	BOOL bResult = TRUE;

	// Add parameters here.
	bResult &= AddParam(_T("System"), _T("m_bUseBootAutoStart"), m_bUseBootAutoStart, false);
	// bResult &= AddParam(_T("System"), _T("MoveDelay"), m_nMoveDelay, 100);
	// bResult &= AddParam(_T("Path"), _T("Source"), m_strSourcePath, _T(""));
	// bResult &= AddParam(_T("System"), _T("UseAutoMove"), m_bUseAutoMove, TRUE);

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

	for (int i = 0; i < static_cast<int>(pTemplate->vecValue.size()); ++i)
	{
		if (pTemplate->vecValue[i].strKey == lpszKey)
		{
			pTemplate->vecValue[i].strValue = lpszValue;
			return;
		}
	}

	PARAM_TEMPLATE_VALUE value;
	value.strKey = lpszKey;
	value.strValue = lpszValue;
	pTemplate->vecValue.push_back(value);
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

BOOL CParameter::AddParam(LPCTSTR lpszSection, LPCTSTR lpszKey, int& nValue, int nDefault)
{
	if (m_eMode == PARAM_LOAD)
	{
		nValue = GetPrivateProfileInt(lpszSection, lpszKey, nDefault, m_strIniPath);
		return TRUE;
	}

	CString strValue;
	strValue.Format(_T("%d"), nValue);
	return WriteString(lpszSection, lpszKey, strValue);
}

BOOL CParameter::AddParam(LPCTSTR lpszSection, LPCTSTR lpszKey, UINT& nValue, UINT nDefault)
{
	if (m_eMode == PARAM_LOAD)
	{
		nValue = static_cast<UINT>(GetPrivateProfileInt(lpszSection, lpszKey, nDefault, m_strIniPath));
		return TRUE;
	}

	CString strValue;
	strValue.Format(_T("%u"), nValue);
	return WriteString(lpszSection, lpszKey, strValue);
}

BOOL CParameter::AddParam(LPCTSTR lpszSection, LPCTSTR lpszKey, double& dValue, double dDefault)
{
	CString strValue;

	if (m_eMode == PARAM_LOAD)
	{
		CString strDefault;
		strDefault.Format(_T("%.15g"), dDefault);

		GetPrivateProfileString(lpszSection, lpszKey, strDefault, strValue.GetBuffer(128), 128, m_strIniPath);
		strValue.ReleaseBuffer();

		dValue = _tcstod(strValue, nullptr);
		return TRUE;
	}

	strValue.Format(_T("%.15g"), dValue);
	return WriteString(lpszSection, lpszKey, strValue);
}

BOOL CParameter::AddParam(LPCTSTR lpszSection, LPCTSTR lpszKey, CString& strValue, LPCTSTR lpszDefault)
{
	if (m_eMode == PARAM_LOAD)
	{
		GetPrivateProfileString(lpszSection, lpszKey, lpszDefault, strValue.GetBuffer(4096), 4096, m_strIniPath);
		strValue.ReleaseBuffer();
		return TRUE;
	}

	return WriteString(lpszSection, lpszKey, strValue);
}

BOOL CParameter::LoadTemplate()
{
	m_vecTemplate.clear();

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
			PARAM_TEMPLATE paramTemplate;
			paramTemplate.strName = strItem.Left(nEqual);
			const CString strTemplateSection = GetTemplateSection(paramTemplate.strName);

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

		lpszItem += _tcslen(lpszItem) + 1;
	}

	return TRUE;
}

BOOL CParameter::SaveTemplate()
{
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

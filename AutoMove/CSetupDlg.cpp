#include "pch.h"
#include "CSetupDlg.h"
#include "CSetupItem.h"

namespace
{

	constexpr LPCTSTR ERROR_DUPLICATE_NAME = _T("'%s' 항목의 이름이 중복되었습니다.");
	constexpr LPCTSTR ERROR_EMPTY_ORIGIN_PATH = _T("'%s' 항목의 대상경로가 비어 있습니다.");
	constexpr LPCTSTR ERROR_EMPTY_DEST_PATH = _T("'%s' 항목의 이동경로가 비어 있습니다.");
	constexpr LPCTSTR ERROR_EMPTY_DRIVE_NAME = _T("'%s' 항목의 드라이브가 선택되지 않았습니다.");
	constexpr LPCTSTR ERROR_EMPTY_LIMIT_VALUE = _T("'%s' 항목의 용량 값이 비어 있습니다.");
	constexpr LPCTSTR ERROR_INVALID_LIMIT_VALUE = _T("'%s' 항목의 용량 값은 1~100 사이의 숫자여야 합니다.");
	constexpr LPCTSTR ERROR_EMPTY_SCHEDULE_DAY = _T("'%s' 항목의 스케줄 요일이 선택되지 않았습니다.");
	constexpr LPCTSTR ERROR_EMPTY_SCHEDULE_TIME = _T("'%s' 항목의 스케줄 시간이 비어 있습니다.");
	constexpr LPCTSTR ERROR_INVALID_SCHEDULE_TIME = _T("'%s' 항목의 스케줄 시간은 0800 형식의 올바른 4자리 시간이어야 합니다.");
	constexpr LPCTSTR ERROR_SAVE_NOT_RUN = _T("설정이 저장되지 않았습니다.");

	bool HasTemplateName(const std::vector<CParameter::PARAM_TEMPLATE>& vecTemplate, const CString& strName)
	{
		for (int i = 0; i < static_cast<int>(vecTemplate.size()); ++i)
		{
			if (vecTemplate[i].strName == strName)
			{
				return true;
			}
		}

		return false;
	}


	bool IsAllDigits(const CString& strValue)
	{
		for (int i = 0; i < strValue.GetLength(); ++i)
		{
			if (!_istdigit(strValue[i]))
			{
				return false;
			}
		}

		return !strValue.IsEmpty();
	}

	bool IsValidScheduleTime(const CString& strValue)
	{
		if (strValue.GetLength() != 4 || !IsAllDigits(strValue))
		{
			return false;
		}

		const int nHour = _ttoi(strValue.Left(2));
		const int nMinute = _ttoi(strValue.Mid(2, 2));
		return nHour >= 0 && nHour <= 23 && nMinute >= 0 && nMinute <= 59;
	}

	void AddError(std::vector<CString>& vecErrors, LPCTSTR lpszFormat, const CString& strName)
	{
		CString strError;
		strError.Format(lpszFormat, static_cast<LPCTSTR>(strName));
		vecErrors.push_back(strError);
	}

	CString JoinErrors(const std::vector<CString>& vecErrors)
	{
		CString strMessage;
		for (int i = 0; i < static_cast<int>(vecErrors.size()); ++i)
		{
			if (!strMessage.IsEmpty())
			{
				strMessage += _T("\r\n");
			}

			strMessage += vecErrors[i];
		}

		if (!strMessage.IsEmpty())
		{
			strMessage += _T("\r\n");
			strMessage += ERROR_SAVE_NOT_RUN;
		}

		return strMessage;
	}
}

CSetupDlg::CSetupDlg(CWnd* pParent, CParameter* pRuntimeParam,
	const std::vector<CString>& vecAvailableDriveNames)
	: CDialogEx(IDD_SETUP_DIALOG, pParent)
	, m_pScrollView(nullptr)
	, m_pRuntimeParam(pRuntimeParam)
	, m_vecAvailableDriveNames(vecAvailableDriveNames)
{
}

CSetupDlg::~CSetupDlg()
{
	if (m_pScrollView != nullptr)
	{
		if (m_pScrollView->GetSafeHwnd())
		{
			m_pScrollView->DestroyWindow();
		}

		delete m_pScrollView;
		m_pScrollView = nullptr;
	}
}

BOOL CSetupDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_param.Load();

	CWnd* pWndPos = GetDlgItem(IDC_STATIC_SETUP_LIST);
	if (pWndPos == nullptr)
	{
		return TRUE;
	}

	CRect rect;
	pWndPos->GetWindowRect(&rect);
	ScreenToClient(&rect);
	pWndPos->ShowWindow(SW_HIDE);

	m_pScrollView = new CListScrollView(ITEM_SETUP);
	m_pScrollView->SetAvailableDriveNames(m_vecAvailableDriveNames);
	if (m_pScrollView->Create(NULL, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER,
		rect, this, 50001))
	{
		m_pScrollView->OnInitialUpdate();
		LoadParameterToControls();
	}

	return TRUE;
}

void CSetupDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSetupDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_SYSTEM_SAVE, &CSetupDlg::OnBnClickedBtSystemSave)
	ON_BN_CLICKED(IDC_BTN_SYSTEM_EXIT, &CSetupDlg::OnBnClickedBtSystemExit)
	ON_BN_CLICKED(IDC_BTN_SYSTEM_ADDITEM, &CSetupDlg::OnBnClickedBtnSystemAdditem)
	ON_BN_CLICKED(IDC_CK_SETUP_AUTOSTART, &CSetupDlg::OnBnClickedCheckAutoStart)
END_MESSAGE_MAP()

void CSetupDlg::OnBnClickedBtSystemSave()
{
	CString strErrorMessage;
	if (!SaveControlsToParameter(strErrorMessage))
	{
		MessageBox(strErrorMessage, _T("Save Failed"), MB_OK | MB_ICONWARNING);
		return;
	}

	if (m_param.Save())
	{
		if (m_pRuntimeParam != nullptr)
		{
			m_pRuntimeParam->Load();
		}

		EndDialog(IDOK);
		return;
	}

	MessageBox(_T("Failed to save settings."), _T("Save Failed"), MB_OK | MB_ICONERROR);
}

void CSetupDlg::OnBnClickedBtSystemExit()
{
	EndDialog(IDCANCEL);
}



void CSetupDlg::OnBnClickedBtnSystemAdditem()
{
	AddTemplateItem();
}

void CSetupDlg::OnBnClickedCheckAutoStart()
{
	SetAllTemplateBootStart(IsDlgButtonChecked(IDC_CK_SETUP_AUTOSTART) == BST_CHECKED);
}

void CSetupDlg::LoadParameterToControls()
{

	if (m_pScrollView == nullptr || !m_pScrollView->GetSafeHwnd())
	{
		return;
	}

	m_pScrollView->ClearItems();

	for (int i = 0; i < static_cast<int>(m_param.m_vecTemplate.size()); ++i)
	{
		AddTemplateItem(&m_param.m_vecTemplate[i]);
	}
}

BOOL CSetupDlg::SaveControlsToParameter(CString& strErrorMessage)
{
	std::vector<CString> vecErrors;

	m_param.InitDefault();
	m_param.ClearTemplate();

	if (m_pScrollView == nullptr || !m_pScrollView->GetSafeHwnd())
	{
		return TRUE;
	}

	for (int i = 0; i < m_pScrollView->GetItemCount(); ++i)
	{
		CSetupItem* pItem = dynamic_cast<CSetupItem*>(m_pScrollView->GetItem(i));
		if (pItem == nullptr)
		{
			continue;
		}

		CParameter::PARAM_TEMPLATE paramTemplate;
		pItem->SaveToTemplate(paramTemplate);
		paramTemplate.strName.Trim();
		if (paramTemplate.strName.IsEmpty())
		{
			continue;
		}

		if (HasTemplateName(m_param.m_vecTemplate, paramTemplate.strName))
		{
			AddError(vecErrors, ERROR_DUPLICATE_NAME, paramTemplate.strName);
		}

		BuildTemplateValidationErrors(paramTemplate, vecErrors);

		CParameter::SetTemplateValue(paramTemplate, CParameter::TemplateKey::NAME, paramTemplate.strName);

		m_param.m_vecTemplate.push_back(paramTemplate);
	}

	if (!vecErrors.empty())
	{
		m_param.ClearTemplate();
		strErrorMessage = JoinErrors(vecErrors);
		return FALSE;
	}

	return TRUE;
}

void CSetupDlg::BuildTemplateValidationErrors(const CParameter::PARAM_TEMPLATE& paramTemplate, std::vector<CString>& vecErrors) const
{
	const CString strName = paramTemplate.strName;
	const CString strOriginPath = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::ORIGIN_PATH);
	const CString strDestPath = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::DEST_PATH);
	const CString strEnableMove = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::ENABLE_MOVE);
	const CString strDriveName = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::DRIVE_NAME);
	const CString strLimitMode = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::LIMIT_MODE);
	const CString strLimitValue = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::LIMIT_VALUE);
	const CString strScheduleDays = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::SCHEDULE_DAYS);
	const CString strScheduleTime = CParameter::GetTemplateValue(paramTemplate, CParameter::TemplateKey::SCHEDULE_TIME);

	if (strOriginPath.IsEmpty())
	{
		AddError(vecErrors, ERROR_EMPTY_ORIGIN_PATH, strName);
	}

	if (strEnableMove == _T("1") && strDestPath.IsEmpty())
	{
		AddError(vecErrors, ERROR_EMPTY_DEST_PATH, strName);
	}

	if (strLimitMode == CParameter::TemplateKey::LIMIT_MODE_SCHEDULE)
	{
		if (strScheduleDays.IsEmpty())
		{
			AddError(vecErrors, ERROR_EMPTY_SCHEDULE_DAY, strName);
		}

		if (strScheduleTime.IsEmpty())
		{
			AddError(vecErrors, ERROR_EMPTY_SCHEDULE_TIME, strName);
		}
		else if (!IsValidScheduleTime(strScheduleTime))
		{
			AddError(vecErrors, ERROR_INVALID_SCHEDULE_TIME, strName);
		}
	}
	else
	{
		if (strDriveName.IsEmpty())
		{
			AddError(vecErrors, ERROR_EMPTY_DRIVE_NAME, strName);
		}

		if (strLimitValue.IsEmpty())
		{
			AddError(vecErrors, ERROR_EMPTY_LIMIT_VALUE, strName);
		}
		else if (!IsAllDigits(strLimitValue) || _ttoi(strLimitValue) < 1 || _ttoi(strLimitValue) > 100)
		{
			AddError(vecErrors, ERROR_INVALID_LIMIT_VALUE, strName);
		}
	}
}

void CSetupDlg::AddTemplateItem(const CParameter::PARAM_TEMPLATE* pTemplate)
{
	if (m_pScrollView == nullptr || !m_pScrollView->GetSafeHwnd())
	{
		return;
	}

	CSetupItem* pItem = dynamic_cast<CSetupItem*>(m_pScrollView->AddItem());
	if (pItem == nullptr)
	{
		return;
	}

	if (pTemplate != nullptr)
	{
		pItem->LoadFromTemplate(*pTemplate);
	}
	else if (IsDlgButtonChecked(IDC_CK_SETUP_AUTOSTART) == BST_CHECKED)
	{
		pItem->SetBootStart(TRUE);
	}
}

void CSetupDlg::SetAllTemplateBootStart(BOOL bBootStart)
{
	if (m_pScrollView == nullptr || !m_pScrollView->GetSafeHwnd())
	{
		return;
	}

	for (int i = 0; i < m_pScrollView->GetItemCount(); ++i)
	{
		CSetupItem* pItem = dynamic_cast<CSetupItem*>(m_pScrollView->GetItem(i));
		if (pItem != nullptr)
		{
			pItem->SetBootStart(bBootStart);
		}
	}
}